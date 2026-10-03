# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-10-03

First working release. Verified on real hardware: an ESP32-P4 (silicon rev v1.3) with an
ESP32-C6 Wi-Fi coprocessor, flashed and confirmed serving its task website over its own
access point, with tasks surviving a power cycle.

### Added
- Offline task website served by the ESP32 itself: add, edit, tick off, delete, priority and
  All / Active / Done filters. Single page, mobile friendly, no CDN or internet needed.
- REST API: `GET/POST /api/tasks`, `PUT/DELETE /api/tasks/<id>`,
  `POST /api/tasks/<id>/toggle`, `GET/POST /api/wifi`, `POST /api/reset`.
- Persistent task store in NVS (up to 100 tasks) with input validation, a 500 ms coalesced
  write, and writes skipped entirely when nothing changed.
- Always-on Wi-Fi access point (`ESP-TASKMGR`, open by default) plus an optional join to a
  home network for LAN access and SNTP clock sync. Wall-clock timestamps fall back to a
  monotonic order when offline.
- On-device interface for use without a phone: SSD1306 OLED task list with a menu
  (quick add, delete, reset all) and three debounced buttons with short and long press.
- Boot-time factory reset (hold SELECT at power-on) and `POST /api/reset`.
- Graceful degradation: the device keeps running its on-device UI when no display is wired,
  and logs a clear warning instead of crashing when no Wi-Fi radio is available.
- ESP32-P4 support, verified on hardware: Wi-Fi from the ESP32-C6 coprocessor over SDIO via
  `esp_wifi_remote` + `esp_hosted`, and early-silicon (rev <3.0) clock configuration at 360 MHz.
- Plain ESP32 remains a supported target and builds clean as a fallback.
- Documentation: README (wiring, build, flash, use, API, limits), TESTING.md with per-check
  results, RUNNING.md, PLAN.md with decisions and risks, and `scripts/` helpers for building
  and capturing serial logs.

### Fixed
- The custom partition table was silently ignored by the build, so the task store received
  only 24 KB of NVS and the advertised 100-task limit was unreachable. The partition is now
  64 KB, verified in the built table.
- `POST /api/reset` was silently not registered: 9 routes were registered against the default
  limit of 8 handlers. The limit is now raised in code, and a failed registration is logged
  instead of aborting the device.
- A failing `esp_wifi_init` aborted the device at boot. Wi-Fi startup is now non-fatal.
- Early P4 silicon could not boot at the IDF default of 400 MHz and rebooted in a loop
  (`assert failed: esp_clk_init`); the correct 360 MHz path is selected for rev <3.0.
- Removed a backup-restore path whose NVS key was never written.
- Titles are JSON-escaped and HTML-safe; request bodies are length-capped; invalid input
  returns 400/404/413 instead of crashing.

### Known limitations
- No OLED or buttons are wired to the tested board, so the on-device UI is untested on
  hardware; the firmware runs headless in the meantime.
- Task text entry requires the website; three buttons cannot type.
- ASCII only on the OLED display (other characters are stored and shown correctly on the web).
- Plain HTTP on a local access point, with no TLS.
- USB power is expected while in use.
