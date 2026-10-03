# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Added
- Project scaffold: ESP-IDF `firmware/`, `pins.h`, Kconfig, default partitions,
  `sdkconfig.defaults`, stub `ssd1306` component, `scripts/read_log.py`.
- Plan (PLAN.md), README skeleton, MIT LICENSE.
- Core: NVS-backed task store (100 tasks, coalesced writes, backup restore),
  REST API (`/api/tasks`, `/api/wifi`), offline single-page web UI, APSTA Wi-Fi
  with optional home-router join and SNTP time sync.
- Managed dependency `espressif/cjson`; `scripts/idf_cmd.ps1` build helper.
- Features: SSD1306 OLED task list with menu (quick-add, delete, reset),
  debounced UP/DOWN/SELECT buttons with long-press, boot-time factory reset,
  `POST /api/reset`, headless fallback without display.
- Fixes: the custom partition table was ignored by the build, so the task store only got
  24 KB of NVS and the 100-task limit could not have been met (now 64 KB, verified in the
  built table); removed a backup-restore path whose key was never written; skip flash writes
  when the stored payload has not changed.

## [1.0.0] — to be released at delivery (step 7)
