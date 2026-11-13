# Flashing (do not run until Mike says go)

Rollback is `~/shroomlabs/lykyn-backup/lykyn_full_4MB_a.bin` on the Mini
(sha256 `10cef88c561e0c2d0040d8287b21a87ed471a7012a2d65fe8d3b383c9b156c62`).

## Option A: the whole app slot, keep NVS, factory_nvs and stock app1 (recommended)
Writes bootloader, partition table, a blank otadata and our app into app0. The stock firmware stays in app1,
and stock settings in NVS and factory_nvs are untouched. A blank otadata makes the bootloader pick app0.

```
esptool.py --chip esp32c3 --port /dev/cu.usbmodem101 --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0     dist/bootloader.bin \
  0x8000  dist/partition-table.bin \
  0xe000  dist/ota_data_initial.bin \
  0x10000 dist/lykyn-app.bin
```

## Option B: one merged file (what the web installer uses)
```
esptool.py --chip esp32c3 --port /dev/cu.usbmodem101 --baud 460800 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 dist/lykyn-provision.factory.bin
```
Do not add `erase_flash`. The 0x3e0000 factory_nvs sector holds the stock unit data.

## Rollback
```
esptool.py --chip esp32c3 --port /dev/cu.usbmodem101 --baud 460800 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 lykyn_full_4MB_a.bin
```
Quick rollback without rewriting everything: erase only otadata, then write the stock one back
(`esptool.py ... write_flash 0xe000 <otadata carved from the backup, offset 0xe000 size 0x2000>`).
Stock otadata pointed at app1 (seq 2).

## First boot
The provisioning build has no Wi-Fi inside. The Lykyn opens the network `Lykyn-Setup` (captive portal)
and also answers Improv over USB. Or build `lykyn-home.yaml` with your own `secrets.yaml`.
