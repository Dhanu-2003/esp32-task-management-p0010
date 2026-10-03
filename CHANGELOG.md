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

## [1.0.0] — to be released at delivery (step 7)
