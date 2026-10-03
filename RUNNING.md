# RUNNING.md

Nothing is running right now: this is firmware on an ESP32 board, not a program on this
laptop, so there is no local process, port, PID or log directory to manage.

## What runs on the device

| What | Where | Started by | How to stop |
|------|-------|-----------|-------------|
| TaskDeck firmware | On the ESP32 board, after flashing | `powershell ..\scripts\idf_cmd.ps1 -p COMx flash` | Power-cycle (unplug USB) or press RESET |

## Device-side network

| What | Address | Notes |
|------|---------|-------|
| TaskDeck access point | `ESP-TASKMGR` (open by default) | Always on, even with no home Wi-Fi |
| Web UI + API | `http://192.168.4.1/` | Join the AP first; no internet needed |
| Home Wi-Fi (LAN) | only if joined via the web UI's Wi-Fi section | Shown at the top of `/api/wifi` |

## Logs

There is no endless monitor on this laptop (never run `idf.py monitor`). To capture the
device's serial output for a fixed time:

```powershell
python scripts\read_log.py --port COMx --seconds 20 --out boot.log
```

## Build state

- Last successful build: step 5, target `esp32`, image ~886 KB of a 1 MB app partition.
- Build artefacts live in `firmware/build/` (git-ignored).
- **Not yet flashed or tested on hardware: no ESP32 board has been connected to this laptop
  (`nova_ports(probe=true)` returned no ports on 2026-10-03).** Step 6 hardware tests are
  pending a connected board; the manual steps are written out in `TESTING.md`.
