# Local control by an external controller

The **EXTERNAL** Control Mode lets a controller on the home network drive the fans and mister directly, with every job timed on the chamber itself.
- **Contract:** the command and status format is in lykyn-control, `docs/2026-11-23-local-control-contract.md`.
- **Logic:** `include/local_control.h`, pure C++.
- **Wiring:** in `lykyn-core.yaml`, the `Controller Command` text, the 100 ms job interval, and `publish_controller_status`.

**Commands and status.**
- **Commands:** the controller writes one line to `POST /text/Controller Command/set?value=...`, for example `lease 15 id=1`, `mist 5 id=2`, `fans 10 in=33 out=33 id=3`, `stop id=4` or `release id=5`.
- **Status:** it reads the `Controller Status` JSON from `GET /events`. The JSON is published every second and on every change.

**Safety.**
- **Jobs need a lease.** Jobs run only while the controller holds a lease and the mode is EXTERNAL.
- **When the lease lapses or the chamber reboots,** the mister and fans switch off and the chamber falls back to SMART.
- **The operator always wins:** choosing SMART, MANUAL or OFF on the page ends EXTERNAL.
- **Hard limits apply in every mode:** the mister never runs at or above **Hard Humidity Stop** (default 94), and a mist job is cut the moment the reading reaches it.

**Other changes in this branch:**
- the SHT3x is read every second;
- **Temperature Offset** (default −1.0 °C);
- the fan boost reacts to humidity only;
- in MANUAL, the 120 s mister cap switches the Humidifier off instead of resuming after the rest;
- no reboot on a Wi-Fi outage;
- in the home build, only native OTA (`esphome upload`), which switches everything off before an upload starts. Web OTA, and the captive portal that would load it, are left out: web OTA's begin hook runs too late to stop the outputs. The setup builds (provision, public) keep the captive portal;
- MANUAL mist requests also end at the hard stop and when the sensor goes stale;
- the timezone is Europe/Madrid;
- the Shroom Labs cloud link is off.

## Test

```
g++ -std=c++17 -Wall -Wextra -I include scripts/local_control_host_test.cpp -o /tmp/local_control_test && /tmp/local_control_test
```

## Build and first install

```
python3 -m venv .venv && .venv/bin/pip install 'esphome==2026.10.2'
cp secrets.example.yaml secrets.yaml      # real values before flashing a chamber
scripts/build-webapp.sh
.venv/bin/esphome compile lykyn-home.yaml
```

For the first install, flash the separate images that `.esphome/build/lykyn/build/flasher_args.json` lists: bootloader at 0x0, partition table at 0x8000, `ota_data_initial.bin` at 0xe000, and `lykyn.bin` at 0x10000.

Don't use `esphome upload` over USB. It writes the merged factory image at 0x0, and that blanks the settings area at 0x9000.
