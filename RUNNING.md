# RUNNING.md

## What runs on the device

| What | Where | Started by | How to stop |
|------|-------|-----------|-------------|
| TaskDeck firmware | On the ESP32-P4 board on **COM21**, flashed 2026-10-03 | `powershell ..\scripts\idf_cmd.ps1 -p COM21 flash` | Power-cycle (unplug USB) or press RESET |

Firmware state on the board right now: flashed for **esp32p4**, app image ~479 KB,
custom partition table active (`nvs` 64 KB at 0x9000, `factory` 1 MB at 0x20000).
Last verified boot log: `TASKDECK up ap=192.168.4.1 tasks=0 web=on`.

## Device-side network

| What | Address | Notes |
|------|---------|-------|
| TaskDeck access point | `ESP-TASKMGR` (open, no password) | Always on. Wi-Fi comes from the ESP32-C6 coprocessor on the board over SDIO |
| Web UI + REST API | `http://192.168.4.1/` | Join the AP first; no internet needed |
| Home Wi-Fi (LAN) | only if joined via the web UI's Wi-Fi section | Shows at the top of `/api/wifi` |

**Nothing runs on this laptop.** There is no local server, process, port or PID to manage, and
no endless monitor is ever started here.

## Why the laptop does not join the device's access point

Joining `ESP-TASKMGR` from this laptop would take the laptop off the internet, which would cut
the agent's own connection mid-task. So the website is verified from a **phone** instead
(steps in `TESTING.md`), not from this machine.

## Logs

Capture the device's serial output for a fixed time (never run `idf.py monitor`):

```powershell
python scripts\read_log.py --port COM21 --seconds 20 --out boot.log
```

Build log of the last successful build: not kept in git (see `.gitignore`); rebuild with
`powershell -ExecutionPolicy Bypass ..\scripts\idf_cmd.ps1 build` from `firmware/`.

## Known hardware gaps

- **No OLED or buttons are wired to the board yet.** The boot log says
  `ui: OLED init failed (SDA=21 SCL=22 addr=0x3c); continuing headless`, which is expected and
  harmless: the device runs the web UI only. Wiring steps are in `README.md`; the pins are all
  in `firmware/main/pins.h`.
- The ESP32-C6 coprocessor reports `coprocessor=0.0.0`, so its ESP-Hosted slave firmware
  version could not be read back. Wi-Fi works regardless; update the coprocessor firmware only
  if a problem appears.
