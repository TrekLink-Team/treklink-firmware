---
id: test-run-2026-11-04-v2-usb-cdc-console
type: test-run
status: closed
owner: KhoaDD
summary: The v2 release image with ARDUINO_USB_CDC_ON_BOOT=1 flashed on all four v2 units; every unit answers the client API over its USB port and keeps its settings.
verified: {commit: 9a4b112, date: 2026-11-04, scope: "the four units below, read back with tk.py"}
---

# 2026-11-04: v2 console and client API over USB CDC

- **Tester:** KhoaDD held each unit's power button at the bench; the agent flashed and read back.
- **Firmware:** branch `fix/v2-usb-cdc-console` at `9a4b112`, env `treklink-v2`, release build (no
  test hooks), reporting `2.7.26.9a4b112`. The only change is `-D ARDUINO_USB_CDC_ON_BOOT=1` in the
  env, which moves `Serial` (console and client API) from UART0 on GPIO 43/44 to the USB port.
- **Before:** a v2 on the stock image returned 0 bytes in 5 s on `/dev/ttyACM0`, and
  `tk.py read` got no client API reply (finding F1 of the 2026-10-29 run).
- **Flash:** esptool from `tool-esptoolpy` (v4.11.0), bootloader `0x0`, partitions `0x8000`,
  `boot_app0` `0xe000`, app `0x10000`, so NVS and LittleFS were kept. Power button held through every
  reset (the GPIO 9 latch).

## Results

| Unit | Node | Battery | Reboots | Read over USB after the flash |
|---|---|---|---|---|
| 1 | `!0aa3a278` "TrekLink a278" | 95 % | 57 | ✅ env `treklink-v2`, firmware `2.7.26.9a4b112` |
| 2 | `!0aa3a294` "TrekLink a294" | 31 % | 72 | ✅ same |
| 3 | `!0aa3a268` "TrekLink a268" | 101 (charging) | 147 | ✅ same |
| 4 | `!0aa3a274` "TrekLink a274" | 101 (charging) | 84 | ✅ same |

Names and reboot counters survived, so the node settings in NVS were kept.

## Flash procedure finding

`--before usb_reset --baud 115200` (the route of the 2026-10-29 notes) failed three times on unit 1:
the bootloader, partitions and `boot_app0` wrote and verified, then the app write lost the
connection at its first block and ended with `Could not configure port: (5, 'Input/output error')`;
`--no-stub` failed the same way after the erase. The default reset at `--baud 921600` (the route of
`tk.py flash`) worked on all four: esptool reconnected once by itself during the app write on units
1 and 2 and then verified the hash; units 3 and 4 needed no reconnect. App write time 12.2 to
12.9 s. Use the default reset at 921600 for v2.
