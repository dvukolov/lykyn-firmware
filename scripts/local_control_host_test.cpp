// Host test for include/local_control.h (lease, chamber-timed jobs, status JSON).
// Build and run: g++ -std=c++17 -Wall -Wextra -I include scripts/local_control_host_test.cpp -o /tmp/lc && /tmp/lc
#include "local_control.h"

#include <cmath>
#include <cstdio>
#include <string>

using esphome::lykyn::LcInputs;
using esphome::lykyn::LcOutputs;
using esphome::lykyn::LocalControl;

static int failed = 0;
static void expect(bool ok, const char *msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    failed++;
  }
}
static bool has(const std::string &hay, const char *needle) { return hay.find(needle) != std::string::npos; }

struct Rig {
  LocalControl lc;
  LcInputs in;
  LcOutputs out;
  Rig() {
    in.now_ms = 1000;
    in.external = false;
    in.off = false;
    in.fresh = true;
    in.rh = 90.0f;
    in.hard_stop = 94.0f;
    in.fallback = true;  // most tests start from the chamber's own SMART fallback
    in.boot_id = 7;
  }
  // Mirrors the firmware: a request to enter a mode is applied at once.
  void apply_mode() {
    if (out.enter_external && !in.external) { in.external = true; lc.mode_changed(true, in.now_ms); }
    if (out.enter_fallback && in.external) { in.external = false; lc.mode_changed(false, in.now_ms); }
    out.enter_external = out.enter_fallback = false;
  }
  void cmd(const char *line) { lc.command(line, in, out); apply_mode(); }
  void tick(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 100) {
      in.now_ms += 100;
      lc.tick(in, out);
      apply_mode();
    }
  }
  std::string status() {
    return lc.status(0xDEADBEEF, 7, 300, in.rh, 24.3f, out.fan_in * 100, out.fan_out * 100, out.mister,
                     in.external ? "EXTERNAL" : "SMART", in.fallback, in.now_ms);
  }
};

int main() {
  {  // jobs need the lease
    Rig r;
    r.cmd("mist 5 id=1");
    expect(r.lc.last_reason() == std::string("nolease"), "mist without lease is refused nolease");
    expect(!r.out.mister, "mister stays off without lease");
    expect(has(r.status(), "\"c\":{\"id\":1,\"r\":\"nolease\"}"), "status acknowledges the refusal in c");
    expect(has(r.status(), "\"j\":{\"id\":0,\"k\":\"none\""), "a refused command never appears in j");
  }
  {  // lease enters EXTERNAL; a mist runs for its duration, timed here
    Rig r;
    r.cmd("lease 15 id=1");
    expect(r.in.external, "lease switches the mode to EXTERNAL");
    expect(r.lc.lease_left(r.in.now_ms) == 15000, "lease is 15 s");
    r.cmd("mist 2.5 id=2");
    expect(r.lc.last_reason().empty(), "mist accepted");
    r.lc.tick(r.in, r.out);
    expect(r.out.mister, "mister on during the job");
    expect(has(r.status(), "\"j\":{\"id\":2,\"k\":\"mist\",\"st\":\"run\""), "status shows the running mist");
    r.tick(2400);
    expect(r.out.mister, "still on at 2.4 s");
    r.tick(200);
    expect(!r.out.mister, "off after 2.5 s");
    expect(has(r.status(), "\"st\":\"done\""), "job done");
  }
  {  // a lease heartbeat during a job does not hide the job
    Rig r;
    r.cmd("lease 15 id=1");
    r.cmd("fans 10 in=33 out=33 id=2");
    r.lc.tick(r.in, r.out);
    expect(std::fabs(r.out.fan_in - 0.33f) < 0.001f && std::fabs(r.out.fan_out - 0.33f) < 0.001f, "fans at 33%");
    r.tick(3000);
    r.cmd("lease 15 id=3");
    std::string s = r.status();
    expect(has(s, "\"c\":{\"id\":3,\"r\":\"\"}") && has(s, "\"j\":{\"id\":2,\"k\":\"fans\",\"st\":\"run\""),
           "heartbeat acknowledged in c while j still shows the fans job");
    r.tick(7100);
    expect(r.out.fan_in == 0.0f && r.out.fan_out == 0.0f, "fans off after 10 s");
  }
  {  // duty below 33 lifts to 33; zero stays zero; range checks
    Rig r;
    r.cmd("lease 15 id=1");
    r.cmd("fans 5 in=10 out=0 id=2");
    r.lc.tick(r.in, r.out);
    expect(std::fabs(r.out.fan_in - 0.33f) < 0.001f && r.out.fan_out == 0.0f, "10% lifts to 33%, 0 stays off");
    r.tick(5100);
    r.cmd("mist 31 id=3");
    expect(r.lc.last_reason() == std::string("range"), "mist over 30 s is out of range");
    r.cmd("fans 0.2 in=33 out=33 id=4");
    expect(r.lc.last_reason() == std::string("range"), "fans under 0.5 s is out of range");
    r.cmd("fans 5 in=150 out=33 id=5");
    expect(r.lc.last_reason() == std::string("range"), "duty over 100 is out of range");
    r.cmd("lease 90 id=6");
    expect(r.lc.last_reason() == std::string("range"), "lease over 60 s is out of range");
    r.cmd("dance 5 id=7");
    expect(r.lc.last_reason() == std::string("bad"), "unknown verb is bad");
    r.cmd("mist five id=8");
    expect(r.lc.last_reason() == std::string("bad"), "non-numeric duration is bad");
    r.cmd("mist 5");
    expect(r.lc.last_reason() == std::string("bad"), "missing id is bad");
  }
  {  // one job at a time; the interlock; duplicates
    Rig r;
    r.cmd("lease 15 id=1");
    r.cmd("fans 10 in=50 out=50 id=2");
    r.cmd("mist 5 id=3");
    expect(r.lc.last_reason() == std::string("busy"), "mist while fans run is busy");
    r.cmd("fans 10 in=50 out=50 id=2");
    expect(has(r.status(), "\"c\":{\"id\":3,"), "a duplicate id is not acknowledged again as new");
    expect(r.lc.job_running(), "duplicate did not disturb the running job");
    r.cmd("stop id=4");
    r.lc.tick(r.in, r.out);
    expect(!r.lc.job_running() && r.out.fan_in == 0.0f, "stop ends the job and turns fans off");
    expect(has(r.status(), "\"j\":{\"id\":2,\"k\":\"fans\",\"st\":\"cut\",\"r\":\"stop\""), "stop cuts the job");
  }
  {  // vetoes: stale reading and ceiling at start; ceiling during the mist
    Rig r;
    r.cmd("lease 15 id=1");
    r.in.fresh = false;
    r.cmd("mist 5 id=2");
    expect(r.lc.last_reason() == std::string("stale"), "stale reading vetoes mist");
    r.in.fresh = true;
    r.in.rh = 94.0f;
    r.cmd("mist 5 id=3");
    expect(r.lc.last_reason() == std::string("ceiling"), "ceiling vetoes mist at the hard stop");
    r.in.rh = 92.0f;
    r.cmd("mist 5 id=4");
    r.tick(1000);
    expect(r.out.mister, "mist running");
    r.in.rh = 94.2f;
    r.tick(100);
    expect(!r.out.mister, "mist cut when the reading reaches the hard stop");
    expect(has(r.status(), "\"st\":\"cut\",\"r\":\"ceiling\""), "cut reason ceiling");
  }
  {  // lease expiry: everything off, fallback requested
    Rig r;
    r.cmd("lease 5 id=1");
    r.cmd("fans 60 in=50 out=50 id=2");
    r.tick(4900);
    expect(r.in.external && r.lc.job_running(), "still running before expiry");
    r.tick(200);
    expect(!r.in.external, "lease expiry falls back out of EXTERNAL");
    expect(!r.lc.lease_held() && r.out.fan_in == 0.0f, "lease gone, fans off");
    expect(has(r.status(), "\"r\":\"expired\""), "job cut with expired");
    r.cmd("mist 5 id=3");
    expect(r.lc.last_reason() == std::string("nolease"), "no jobs after expiry until the lease is taken again");
  }
  {  // commands renew an active lease
    Rig r;
    r.cmd("lease 5 id=1");
    r.tick(4000);
    r.cmd("stop id=2");
    expect(r.lc.lease_left(r.in.now_ms) == 5000, "an accepted command renews the lease by its length");
  }
  {  // the operator's mode change ends EXTERNAL; OFF blocks the lease
    Rig r;
    r.cmd("lease 15 id=1");
    r.cmd("mist 10 id=2");
    r.in.external = false;
    r.lc.mode_changed(false, r.in.now_ms);
    r.lc.tick(r.in, r.out);
    expect(!r.lc.lease_held() && !r.out.mister, "leaving EXTERNAL drops the lease and stops the mist");
    expect(has(r.status(), "\"r\":\"mode\""), "job cut with mode");
    r.in.off = true;
    r.cmd("lease 15 id=3");
    expect(r.lc.last_reason() == std::string("mode") && !r.in.external, "OFF is not taken over by a lease");
  }
  {  // release
    Rig r;
    r.cmd("release id=1");
    expect(r.lc.last_reason().empty() && !r.in.external, "release without a lease is a no-op");
    r.cmd("lease 15 id=2");
    r.cmd("fans 30 in=33 out=33 id=3");
    r.cmd("release id=4");
    r.lc.tick(r.in, r.out);
    expect(!r.in.external && !r.lc.lease_held() && r.out.fan_in == 0.0f, "release stops, drops the lease, falls back");
  }
  {  // a lease takes over only EXTERNAL or the chamber's own fallback SMART, never an operator's choice
    Rig r;
    r.in.fallback = false;  // the operator chose SMART or MANUAL on the page
    r.cmd("lease 15 id=1");
    expect(r.lc.last_reason() == std::string("mode") && !r.in.external, "an operator's mode is not taken over");
    r.in.fallback = true;  // SMART entered by the chamber itself (boot from EXTERNAL, expiry, release)
    r.cmd("lease 15 id=2");
    expect(r.lc.last_reason().empty() && r.in.external, "the fallback SMART is taken over");
  }
  {  // a lease that names a boot is refused after a reboot, so a late heartbeat cannot take a new boot
    Rig r;
    r.in.boot_id = 111;
    r.cmd("lease 15 b=111 id=1");
    expect(r.lc.last_reason().empty(), "lease with the current boot accepted");
    r.in.boot_id = 222;
    r.cmd("lease 15 b=111 id=2");
    expect(r.lc.last_reason() == std::string("boot"), "lease naming an old boot is refused");
    r.cmd("lease 15 b=x id=3");
    expect(r.lc.last_reason() == std::string("bad"), "a malformed boot is bad");
  }
  {  // renew extends a held lease only; it never takes one
    Rig r;
    r.cmd("renew 15 b=7 id=1");
    expect(r.lc.last_reason() == std::string("nolease") && !r.in.external, "renew without a lease is refused");
    r.cmd("lease 5 b=7 id=2");
    r.tick(3000);
    r.cmd("renew 15 b=7 id=3");
    expect(r.lc.last_reason().empty() && r.lc.lease_left(r.in.now_ms) == 15000, "renew extends a held lease");
    r.cmd("renew 15 b=8 id=4");
    expect(r.lc.last_reason() == std::string("boot"), "renew for another boot is refused");
    r.cmd("renew 15 id=5");
    expect(r.lc.last_reason() == std::string("bad"), "renew without b= is bad");
  }
  {  // a lapsed lease expires before any command is handled, even between ticks
    Rig r;
    r.cmd("lease 5 b=7 id=1");
    r.cmd("fans 60 in=33 out=33 id=2");
    r.in.now_ms += 5050;  // past the deadline, no tick yet
    r.cmd("renew 15 b=7 id=3");
    expect(r.lc.last_reason() == std::string("nolease"), "a late renew cannot revive the lease");
    expect(!r.in.external && !r.lc.lease_held(), "the expiry fell back before the command");
    expect(has(r.status(), "\"r\":\"expired\""), "the job was cut as expired");
  }
  {  // a fresh lease arriving just after expiry takes the fallback it creates, instead of being dropped
    Rig r;
    r.cmd("lease 5 b=7 id=1");
    r.in.now_ms += 5050;  // expired, no tick yet; the chamber is still EXTERNAL
    r.cmd("lease 15 b=7 id=2");
    expect(r.lc.last_reason().empty() && r.lc.lease_held() && r.in.external, "the new lease holds");
    expect(r.lc.lease_left(r.in.now_ms) == 15000, "with its own length");
  }
  {  // renew checks the boot before the lease, so a reboot reads as a reboot
    Rig r;
    r.in.boot_id = 9;
    r.cmd("renew 15 b=7 id=1");
    expect(r.lc.last_reason() == std::string("boot"), "renew naming an old boot is refused with boot");
  }
  {  // a mist is cut when its reading goes stale
    Rig r;
    r.cmd("lease 15 b=7 id=1");
    r.cmd("mist 20 id=2");
    r.tick(1000);
    r.in.fresh = false;
    r.tick(100);
    expect(!r.out.mister && has(r.status(), "\"st\":\"cut\",\"r\":\"stale\""), "stale reading cuts the mist");
  }
  {  // an OTA upload stops everything before the main loop blocks
    Rig r;
    r.cmd("lease 15 b=7 id=1");
    r.cmd("mist 10 id=2");
    r.lc.ota_begin(r.in.now_ms);
    r.lc.tick(r.in, r.out);
    expect(!r.out.mister && !r.lc.lease_held(), "OTA cuts the job and drops the lease");
    expect(has(r.status(), "\"r\":\"ota\""), "cut reason ota");
  }
  {  // status JSON shape and length
    Rig r;
    r.cmd("lease 60 id=4294967295");
    r.cmd("fans 600 in=100 out=100 id=4294967294");
    r.lc.tick(r.in, r.out);
    r.in.rh = NAN;
    std::string s = r.status();
    expect(has(s, "\"b\":3735928559,\"s\":7,\"a\":300,\"rh\":null,\"t\":24.30"), "header fields, null rh");
    expect(has(s, "\"md\":\"EXTERNAL\",\"fb\":"), "mode and fallback flag");
    expect(s.size() <= 255, "status fits in 255 characters");
    std::printf("%s\n(%zu chars)\n", s.c_str(), s.size());
  }
  if (failed) {
    std::fprintf(stderr, "%d failure(s)\n", failed);
    return 1;
  }
  std::printf("local_control host test: all passed\n");
  return 0;
}
