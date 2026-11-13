# lykyn-firmware

Native Shroom Labs firmware for the Lykyn mushroom chamber (ESP32-C3, 4 MB). Built with ESPHome.
Status: compiled, **not yet flashed on a real unit**. Read `docs/FLASH.md` and `docs/PROBE_PLAN.md` first.

What it does:

- Reads the SHT3x (I2C 0x44) for temperature and humidity.
- Runs a local climate loop with no cloud: humidity hysteresis, fresh air exchange cycles, over-humidity and
  over-temperature fan boost, sensor-lost failsafe.
- Drives both PWM fans, the ultrasonic mister (on/off with a time-proportional Mist Level), the 16 pixel
  WS2812B grow ring (RGB and white mix, photoperiod) and the status pixel.
- Default profile is Black Oyster, SMART, 15 to 21 C, 85 to 92 percent RH. Stages: Fruiting, Pinning, Colonization, Custom.
- Local HTTP/JSON API (ESPHome web server REST), same entity names the OpenShrooly hub already polls.
- OTA over Wi-Fi, Wi-Fi setup by captive portal (`Lykyn-Setup`) or Improv over USB.
- Same 4 MB partition layout as the stock firmware, so the stock image and this one can swap.

Pins and how they were found are in `docs/PINMAP.md`.

## Builds

| File | Use |
| --- | --- |
| `lykyn-public.yaml` | No secrets inside. Safe to host for the web installer. No web or OTA password. |
| `lykyn-provision.yaml` | No Wi-Fi inside, web UI and OTA passwords from `secrets.yaml`. |
| `lykyn-home.yaml` | Same plus your Wi-Fi from `secrets.yaml`. |

```
python3 -m venv .venv && .venv/bin/pip install esphome
cp secrets.example.yaml secrets.yaml   # edit it, never commit it
.venv/bin/esphome compile lykyn-provision.yaml
```

`secrets.yaml` and every compiled image from the private builds are git ignored. Compiled images embed the
passwords from `secrets.yaml`.

## LAN API

Basic auth on the private builds. Examples:

```
curl -u shroom:PASSWORD http://lykyn.local/text_sensor/status_json
curl -u shroom:PASSWORD http://lykyn.local/sensor/current_humidity
curl -u shroom:PASSWORD -X POST "http://lykyn.local/number/target_humidity/set?value=90"
curl -u shroom:PASSWORD -X POST "http://lykyn.local/select/stage/set?option=Pinning"
curl -u shroom:PASSWORD -X POST "http://lykyn.local/select/control_mode/set?option=MANUAL"
curl -u shroom:PASSWORD -X POST http://lykyn.local/switch/humidifier/turn_on
```

Entity ids the hub already knows: `current_temperature`, `current_humidity`, `target_humidity`,
`temperature_target`, `humidifier_status`, `air_exchange_status`, `light_status`, `active_alerts`, `ip_address`, `wifi_ssid`.

## Shroom Labs link

Today this is the same path the OpenShrooly chambers use: the hub polls the LAN REST API and syncs to
shroomlabs.io. The outbound cloud client (`shroom_labs_cloud` with claim code and signed telemetry) is a stub in the
openshrooly repo, so it is not built here yet. Coach profiles reach the unit by writing the numbers and
selects above.

## Safety clamps

- Mister never runs more than 120 s of demand in a row, then rests 30 s.
- Mister off at or above the high end of the humidity band, and always off at 95 percent RH.
- No valid sensor reading for 60 s: mister off, fans at 33 percent.
- Fans never run below 33 percent duty.
- Temperature min and max are alerts only. The Lykyn has no heater or cooler.

## Not done

- Colour order of the ring is assumed GRB until the LED Test button says otherwise.
- GPIO0 and GPIO6 purpose unknown. Held HIGH like stock.
- No on-device test yet.
