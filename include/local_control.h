// Local control by an external controller: a lease, jobs timed on the chamber, and a status line.
//
// The contract lives in lykyn-control: docs/2026-11-23-local-control-contract.md. In short, the
// controller writes one-line commands ("lease 15 id=1", "mist 5 id=2", "fans 10 in=33 out=33 id=3",
// "stop id=4", "release id=5") to the "Controller Command" text entity and reads "Controller Status".
// Jobs run only while the controller holds the lease and Control Mode is EXTERNAL. When the lease
// lapses, everything stops and the firmware falls back to SMART, so a controller that dies or loses
// the network can never leave the mister or the fans running.
//
// Pure C++ with no ESPHome types, so the host test can drive it (scripts/local_control_host_test.cpp).
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome {
namespace lykyn {

struct LcInputs {
  uint32_t now_ms{0};
  bool external{false};  // Control Mode is EXTERNAL
  bool off{false};       // Control Mode is OFF: the operator's stop, never taken over by a lease
  bool fresh{false};     // a valid reading within the last 10 s
  float rh{NAN};         // latest raw humidity
  float hard_stop{94.0f};
  // SMART that the chamber entered by itself (boot from EXTERNAL, lease expiry, release), as opposed
  // to SMART chosen by the operator. Only this SMART, or EXTERNAL, may be taken over by a lease.
  bool fallback{false};
  uint32_t boot_id{0};   // this boot's id, so "lease ... b=<boot>" can refuse a lease meant for an old boot
};

struct LcOutputs {
  bool mister{false};
  float fan_in{0.0f};   // 0..1
  float fan_out{0.0f};  // 0..1
  bool enter_external{false};  // the firmware should set Control Mode to EXTERNAL
  bool enter_fallback{false};  // the firmware should set Control Mode to SMART
};

class LocalControl {
 public:
  static constexpr float MIST_MIN_S = 0.1f, MIST_MAX_S = 30.0f;
  static constexpr float FANS_MIN_S = 0.5f, FANS_MAX_S = 600.0f;
  static constexpr float LEASE_MIN_S = 5.0f, LEASE_MAX_S = 60.0f;
  static constexpr float FAN_FLOOR_PCT = 33.0f;  // stock's level 1; a fan below this may stall

  // Handles one command line and updates the outputs for this instant.
  void command(const char *line, const LcInputs &in, LcOutputs &out) {
    // A lease past its deadline expires first, even between ticks, so a late command cannot revive it.
    // The command is then judged on the mode that expiry leads to: the SMART fallback.
    LcInputs cur = in;
    expire_(in, out);
    if (out.enter_fallback) {
      cur.external = false;
      cur.fallback = true;
    }
    Parsed p = parse_(line);
    if (!p.ok_id) {  // without an id the controller cannot match the answer; still say why
      ack_(0, "bad");
      outputs_(out);
      return;
    }
    if (duplicate_(p)) {
      outputs_(out);
      return;
    }
    const char *why = p.bad ? "bad" : check_(p, cur);
    if (*why) {
      ack_(p.id, why);
      outputs_(out);
      return;
    }
    ack_(p.id, "");
    if (lease_ && p.kind != Kind::Lease && p.kind != Kind::Renew && p.kind != Kind::Release) renew_(in.now_ms);
    switch (p.kind) {
      case Kind::Lease:
        lease_ = true;
        lease_len_ms_ = (uint32_t) lroundf(p.seconds * 1000.0f);
        renew_(in.now_ms);
        // A lease taken right after an expiry supersedes the fallback that expiry asked for.
        out.enter_fallback = false;
        if (!in.external) out.enter_external = true;
        break;
      case Kind::Renew:
        lease_len_ms_ = (uint32_t) lroundf(p.seconds * 1000.0f);
        renew_(in.now_ms);
        break;
      case Kind::Mist:
      case Kind::Fans:
        start_(p, in.now_ms);
        break;
      case Kind::Stop:
        cut_("stop", in.now_ms);
        break;
      case Kind::Release:
        if (lease_) {
          cut_("stop", in.now_ms);
          lease_ = false;
          if (in.external) out.enter_fallback = true;
        }
        break;
      default:
        break;
    }
    outputs_(out);
  }

  // Called about every 100 ms: ends jobs on time, cuts a mist at the hard stop, expires the lease.
  void tick(const LcInputs &in, LcOutputs &out) {
    const uint32_t now = in.now_ms;
    expire_(in, out);
    if (jstate_ == St::Run) {
      if (!in.external) {
        cut_("mode", now);
      } else if (jkind_ == Kind::Mist && !in.fresh) {
        cut_("stale", now);  // no reading to check the ceiling against
      } else if (jkind_ == Kind::Mist && !(in.rh < in.hard_stop)) {
        cut_("ceiling", now);
      } else if (now - jstart_ms_ >= jdur_ms_) {
        jstate_ = St::Done;
        jelapsed_ms_ = jdur_ms_;
      }
    }
    outputs_(out);
  }

  // An OTA upload blocks the main loop, so no tick could end a job during it: stop now.
  void ota_begin(uint32_t now) {
    cut_("ota", now);
    lease_ = false;
  }

  // The firmware calls this whenever Control Mode changes. Leaving EXTERNAL (the operator, or the
  // fallback) drops the lease and cuts any job.
  void mode_changed(bool external, uint32_t now) {
    if (external) return;
    if (jstate_ == St::Run) cut_("mode", now);
    lease_ = false;
  }

  bool lease_held() const { return lease_; }
  bool job_running() const { return jstate_ == St::Run; }
  uint32_t lease_left(uint32_t now) const {
    if (!lease_) return 0;
    const int32_t left = (int32_t) (lease_until_ms_ - now);
    return left > 0 ? (uint32_t) left : 0;
  }
  const std::string &last_reason() const { return cmd_reason_; }

  // The status line, at most about 230 characters (the contract allows 255).
  std::string status(uint32_t boot_id, uint32_t seq, uint32_t age_ms, float rh, float t, float fi_pct, float fo_pct,
                     bool mister, const char *mode, bool fallback, uint32_t now) const {
    char rhs[12], ts[12];
    fmt_(rhs, sizeof rhs, rh);
    fmt_(ts, sizeof ts, t);
    const uint32_t e = jstate_ == St::Run ? now - jstart_ms_ : jelapsed_ms_;
    char buf[300];
    std::snprintf(buf, sizeof buf,
                  "{\"b\":%u,\"s\":%u,\"a\":%u,\"rh\":%s,\"t\":%s,\"fi\":%.0f,\"fo\":%.0f,\"m\":%d,\"md\":\"%s\","
                  "\"fb\":%d,\"l\":%u,\"c\":{\"id\":%u,\"r\":\"%s\"},\"j\":{\"id\":%u,\"k\":\"%s\",\"st\":\"%s\","
                  "\"r\":\"%s\",\"e\":%u}}",
                  (unsigned) boot_id, (unsigned) seq, (unsigned) age_ms, rhs, ts, fi_pct, fo_pct, mister ? 1 : 0, mode,
                  fallback ? 1 : 0, (unsigned) lease_left(now), (unsigned) cmd_id_, cmd_reason_.c_str(), (unsigned) jid_,
                  kind_name_(jkind_), state_name_(jstate_), jreason_, (unsigned) e);
    return std::string(buf);
  }

 private:
  enum class Kind : uint8_t { None, Lease, Renew, Mist, Fans, Stop, Release };

  void expire_(const LcInputs &in, LcOutputs &out) {
    if (!lease_ || (int32_t) (in.now_ms - lease_until_ms_) < 0) return;
    cut_("expired", in.now_ms);
    lease_ = false;
    if (in.external) out.enter_fallback = true;
  }
  enum class St : uint8_t { None, Run, Done, Cut };

  struct Parsed {
    Kind kind{Kind::None};
    float seconds{NAN};
    float in_pct{0.0f}, out_pct{0.0f};
    uint32_t id{0};
    bool ok_id{false};
    bool bad{false};
    bool has_boot{false};
    uint32_t boot{0};
  };

  static bool num_(const char *s, float *v) {
    char *end = nullptr;
    const double d = std::strtod(s, &end);
    if (end == s || *end != '\0' || !std::isfinite(d)) return false;
    *v = (float) d;
    return true;
  }

  static Parsed parse_(const char *line) {
    Parsed p;
    char buf[256];
    std::strncpy(buf, line ? line : "", sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    int i = 0;
    bool have_in = false, have_out = false;
    for (char *tok = std::strtok(buf, " "); tok; tok = std::strtok(nullptr, " "), i++) {
      if (i == 0) {
        if (!std::strcmp(tok, "lease")) p.kind = Kind::Lease;
        else if (!std::strcmp(tok, "renew")) p.kind = Kind::Renew;
        else if (!std::strcmp(tok, "mist")) p.kind = Kind::Mist;
        else if (!std::strcmp(tok, "fans")) p.kind = Kind::Fans;
        else if (!std::strcmp(tok, "stop")) p.kind = Kind::Stop;
        else if (!std::strcmp(tok, "release")) p.kind = Kind::Release;
        else p.bad = true;
      } else if (!std::strncmp(tok, "id=", 3)) {
        char *end = nullptr;
        const unsigned long v = std::strtoul(tok + 3, &end, 10);
        if (end == tok + 3 || *end != '\0' || v > 0xFFFFFFFFul) return p;  // no usable id
        p.id = (uint32_t) v;
        p.ok_id = true;
      } else if (!std::strncmp(tok, "b=", 2) && (p.kind == Kind::Lease || p.kind == Kind::Renew)) {
        char *end = nullptr;
        const unsigned long v = std::strtoul(tok + 2, &end, 10);
        if (end == tok + 2 || *end != '\0' || v > 0xFFFFFFFFul) {
          p.bad = true;
        } else {
          p.boot = (uint32_t) v;
          p.has_boot = true;
        }
      } else if (!std::strncmp(tok, "in=", 3)) {
        have_in = num_(tok + 3, &p.in_pct);
        if (!have_in) p.bad = true;
      } else if (!std::strncmp(tok, "out=", 4)) {
        have_out = num_(tok + 4, &p.out_pct);
        if (!have_out) p.bad = true;
      } else if (i == 1 && (p.kind == Kind::Lease || p.kind == Kind::Renew || p.kind == Kind::Mist ||
                            p.kind == Kind::Fans)) {
        if (!num_(tok, &p.seconds)) p.bad = true;
      } else {
        p.bad = true;
      }
    }
    const bool timed = p.kind == Kind::Lease || p.kind == Kind::Renew || p.kind == Kind::Mist || p.kind == Kind::Fans;
    if (timed && std::isnan(p.seconds)) p.bad = true;
    if (p.kind == Kind::Renew && !p.has_boot) p.bad = true;  // renew always names the boot it renews
    if (p.kind == Kind::Fans && !(have_in && have_out)) p.bad = true;
    return p;
  }

  bool duplicate_(const Parsed &p) const {
    if (have_cmd_ && p.id == cmd_id_) return true;
    return jstate_ == St::Run && p.id == jid_ && p.kind == jkind_;
  }

  // Returns "" when the command may run, otherwise the refusal reason.
  const char *check_(const Parsed &p, const LcInputs &in) const {
    switch (p.kind) {
      case Kind::Lease:
        if (p.seconds < LEASE_MIN_S || p.seconds > LEASE_MAX_S) return "range";
        if (p.has_boot && p.boot != in.boot_id) return "boot";
        // Only EXTERNAL or the chamber's own SMART fallback: OFF, MANUAL and an operator's SMART stay theirs.
        if (in.off || !(in.external || in.fallback)) return "mode";
        return "";
      case Kind::Renew:
        if (p.seconds < LEASE_MIN_S || p.seconds > LEASE_MAX_S) return "range";
        if (p.boot != in.boot_id) return "boot";  // a reboot reads as a reboot, not just a lost lease
        if (!lease_) return "nolease";
        return "";
      case Kind::Mist:
        if (p.seconds < MIST_MIN_S || p.seconds > MIST_MAX_S) return "range";
        if (!lease_) return "nolease";
        if (!in.external) return "mode";
        if (jstate_ == St::Run) return "busy";
        if (fan_in_ > 0.0f || fan_out_ > 0.0f) return "interlock";
        if (!in.fresh) return "stale";
        if (!(in.rh < in.hard_stop)) return "ceiling";
        return "";
      case Kind::Fans:
        if (p.seconds < FANS_MIN_S || p.seconds > FANS_MAX_S) return "range";
        if (p.in_pct < 0.0f || p.in_pct > 100.0f || p.out_pct < 0.0f || p.out_pct > 100.0f) return "range";
        if (!lease_) return "nolease";
        if (!in.external) return "mode";
        if (jstate_ == St::Run) return "busy";
        if (mister_ && (p.in_pct > 0.0f || p.out_pct > 0.0f)) return "interlock";
        return "";
      case Kind::Stop:
      case Kind::Release:
        return "";
      default:
        return "bad";
    }
  }

  static float lift_(float pct) { return pct > 0.0f && pct < FAN_FLOOR_PCT ? FAN_FLOOR_PCT : pct; }

  void start_(const Parsed &p, uint32_t now) {
    jid_ = p.id;
    jkind_ = p.kind;
    jstate_ = St::Run;
    jreason_ = "";
    jstart_ms_ = now;
    jdur_ms_ = (uint32_t) lroundf(p.seconds * 1000.0f);
    jelapsed_ms_ = 0;
    jin_ = p.kind == Kind::Fans ? lift_(p.in_pct) / 100.0f : 0.0f;
    jout_ = p.kind == Kind::Fans ? lift_(p.out_pct) / 100.0f : 0.0f;
  }

  void cut_(const char *why, uint32_t now) {
    if (jstate_ != St::Run) return;
    jstate_ = St::Cut;
    jreason_ = why;
    jelapsed_ms_ = now - jstart_ms_;
  }

  void renew_(uint32_t now) { lease_until_ms_ = now + lease_len_ms_; }

  void ack_(uint32_t id, const char *why) {
    cmd_id_ = id;
    have_cmd_ = id != 0;
    cmd_reason_ = why;
  }

  void outputs_(LcOutputs &out) {
    const bool run = jstate_ == St::Run;
    mister_ = run && jkind_ == Kind::Mist;
    fan_in_ = run && jkind_ == Kind::Fans ? jin_ : 0.0f;
    fan_out_ = run && jkind_ == Kind::Fans ? jout_ : 0.0f;
    out.mister = mister_;
    out.fan_in = fan_in_;
    out.fan_out = fan_out_;
  }

  static void fmt_(char *dst, size_t n, float v) {
    if (std::isnan(v)) std::snprintf(dst, n, "null");
    else std::snprintf(dst, n, "%.2f", v);
  }
  static const char *kind_name_(Kind k) {
    return k == Kind::Mist ? "mist" : k == Kind::Fans ? "fans" : "none";
  }
  static const char *state_name_(St s) {
    return s == St::Run ? "run" : s == St::Done ? "done" : s == St::Cut ? "cut" : "none";
  }

  bool lease_{false};
  uint32_t lease_len_ms_{0}, lease_until_ms_{0};

  uint32_t cmd_id_{0};
  bool have_cmd_{false};
  std::string cmd_reason_;

  uint32_t jid_{0};
  Kind jkind_{Kind::None};
  St jstate_{St::None};
  const char *jreason_{""};
  uint32_t jstart_ms_{0}, jdur_ms_{0}, jelapsed_ms_{0};
  float jin_{0.0f}, jout_{0.0f};

  bool mister_{false};
  float fan_in_{0.0f}, fan_out_{0.0f};
};

// One instance for the whole firmware, reached from the YAML lambdas.
inline LocalControl &local_control() {
  static LocalControl lc;
  return lc;
}

}  // namespace lykyn
}  // namespace esphome
