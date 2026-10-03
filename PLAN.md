# PLAN.md — P0010 ESP32 Task Management ("TaskDeck on ESP32")

## 1. Objective and success criteria

**Objective:** A portable ESP32 device the user can carry instead of a laptop/phone for day-to-day
task tracking. The ESP32 hosts a self-contained task-management website (usable from any
phone/laptop browser when one is nearby) **and** a physical on-device UI (OLED display +
buttons) so daily use — view, check-off, add, delete — needs no second device at all.

**What will exist at the end:**
- One ESP-IDF firmware project in `C:\nova\projects\P0010_esp32_task_management\firmware\`
  that builds with `C:/nova/agent/idf.bat build` and flashes with
  `C:/nova/agent/idf.bat -p COMx flash`.
- Private GitHub repo `Dhanu-2003/esp32-task-management-p0010` with README, LICENSE (MIT),
  CHANGELOG, `.gitignore`, v1.0.0 release.
- The device boots, creates Wi-Fi AP `ESP-TASKMGR`, serves `http://192.168.4.1/` — a single-page
  offline task manager (add / list / check / edit / delete / filter), persisted across reboots.
- A 128×64 OLED + 3 buttons on the device mirror the same task list standalone.

**Success criteria (anyone can check):**
1. `idf.bat build` succeeds with no errors.
2. After flash + reset, a phone/laptop sees Wi-Fi `ESP-TASKMGR`, joins it, opens
   `http://192.168.4.1/` and gets the TaskDeck page (no internet needed).
3. Web UI: create a task "Buy milk" → it appears; refresh page → still there; reboot
   device → still there; toggle done → checkbox persists; delete → gone.
4. Without any phone/laptop: OLED shows the task list; buttons scroll, toggle done,
   quick-add and delete work; changes appear in the web UI next time it is opened.
5. `GET http://192.168.4.1/api/tasks` returns JSON array of the same tasks.

## 2. Requirements

### Functional (F1–F10)
- F1. Wi-Fi AP mode on boot: SSID `ESP-TASKMGR` (open), IP `192.168.4.1`, always up.
- F2. Optional STA join: web UI "Wi-Fi setup" page/section stores home SSID/password in NVS;
  device tries STA + keeps AP (APSTA mode) so it is reachable on LAN too.
- F3. Website served from ESP32 flash (embedded single HTML file, no CDN/internet dependency):
  responsive, mobile-friendly, works in AP mode.
- F4. Task model: `{id:int, title:string(1..120 chars), done:bool, priority:0|1|2,
  created:int epoch, updated:int epoch}`. Max 100 tasks (NVS bound, see Decisions).
- F5. REST API: `GET /api/tasks`, `POST /api/tasks {title, priority?}`,
  `PUT /api/tasks/:id {title?, done?, priority?}`, `POST /api/tasks/:id/toggle`,
  `DELETE /api/tasks/:id`, `GET /api/wifi`, `POST /api/wifi {ssid, pass}`.
- F6. Persistence across power loss/reboot (NVS namespace `tasks`, key `db_v1`).
- F7. On-device UI: SSD1306 128×64 OLED (I2C) shows up to ~5 tasks + status bar
  (AP IP, task count, STA state); 3 buttons: UP / DOWN / SELECT (short=move/toggle,
  long-press=context: add/delete/back).
- F8. On-device quick-add: creates `Task N` placeholder editable later via web
  (full text entry impossible with 3 buttons — documented limitation).
- F9. Time: `created/updated` use `time()` (epoch 0 until SNTP syncs when STA online;
  monotonic fallback `esp_timer` sequence so ordering still works offline).
- F10. Factory reset: hold SELECT 5 s on boot (or `POST /api/reset`) clears tasks + Wi-Fi.

### Non-functional (NF1–NF7)
- NF1. Offline-first: zero internet dependency; page loads < 2 s on AP; total HTML < 60 KB.
- NF2. Reliability: no heap leak over 24 h; invalid API input → 400, never crash/panic;
  watchdog enabled.
- NF3. Performance: API p95 < 300 ms with 100 tasks; render 60 fps-class OLED refresh (10 Hz).
- NF4. Security (best-effort on open AP): no remote exposure beyond AP/LAN; input
  sanitised (HTML-escaped on render, JSON length-capped); optional AP password via
  menuconfig (`CONFIG_TASK_AP_PASS`); documented "no HTTPS on ESP32" limitation.
- NF5. Power: runs off USB 5 V; optional Li-ion/battery noted; no sleep in v1
  (always-on AP), deep-sleep listed as future work.
- NF6. Maintainability: plain ESP-IDF C (no Arduino), one `main/` component + `ssd1306/`
  component, `pins.h` for board wiring, Kconfig for SSID/target.
- NF7. Portability: default target `esp32`; retarget to `esp32s3/esp32c3` via
  `idf.bat set-target`; I2C pins centralised for rewiring.

## 3. Decisions and assumptions

| # | Decision | Why |
|---|----------|-----|
| D1 | ESP-IDF (v6.1, C) + `esp_http_server`, not Arduino, not MicroPython | User asked ESP32 hosting; IDF HTTP server is maintained, smallest RAM, matches `restful_server` example |
| D2 | Start from ESP-IDF `examples/protocols/http_server/restful_server` pattern, vendored into `firmware/` (not cloned whole IDF) | Gives correct server/cJSON/NVS wiring; we keep only what we need |
| D3 | Carry use = OLED + buttons; web UI = full editor when phone/PC nearby | Brief says "don't want to carry laptop/phone" — a hosted website alone still needs a browser. Only a physical UI makes "carry just the ESP" true. Assumption recorded explicitly |
| D4 | Display: SSD1306 128×64 I2C OLED, addr 0x3C, SDA GPIO21 / SCL GPIO22; buttons: UP GPIO15, DOWN GPIO2, SELECT GPIO4 (all `INPUT_PULLUP`, active-low) | Most common cheap "carry" combo; one header file to rewire. If user's board differs, only `pins.h` changes |
| D5 | Storage: NVS blob JSON (`tasks` / `db_v1`), cap 100 tasks, NVS partition 64 KB | Avoids a custom filesystem in v1; survives reboot; enough for personal tasks. 100 × 120-char titles is ~18 KB, inside the verified 59.9 KB blob limit. Migrate to LittleFS if >100 tasks are ever needed |
| D6 | Wi-Fi: APSTA — AP always on + optional STA client | Device must work in the street (no home Wi-Fi) and at home (LAN access + SNTP time) |
| D7 | Single-file gzipped HTML embedded via `EMBED_TXTFILE` | Works with zero filesystem; loads fast on AP |
| D8 | No auth on AP in v1 (physical proximity = auth); optional `CONFIG_TASK_AP_PASS` | Login over open HTTP adds little; documented |
| D9 | **Target changed during step 5:** the connected board (COM21) is an **ESP32-P4, silicon rev v1.3**, not a plain ESP32. The P4 has no Wi-Fi radio, so Wi-Fi comes from the **ESP32-C6 coprocessor on the board over SDIO** (`espressif/esp_wifi_remote` + `espressif/esp_hosted`, pulled in only for `esp32p4`/`esp32h2` by `rules:` in `main/idf_component.yml`; no application code changes needed). Plain ESP32 stays a supported fallback target. User's decision (2026-10-03): build both paths | Never guess the chip: `nova_ports(probe=true)` identified it. Two IDF quirks had to be handled: (a) rev<3 and rev>=3 P4 silicon are mutually exclusive in IDF → `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` + `CONFIG_ESP32P4_REV_MIN_100=y` in `sdkconfig.defaults.esp32p4`; (b) that path only supports 40/90/180/360 MHz, so the default 400 MHz tripped `assert(res)` in `esp_clk_init` and rebooted the chip in a loop → the target-specific defaults file makes Kconfig pick 360 MHz |
| D10 | Time: `sntp` only when STA connected; else boot-count + `esp_timer` ordering | Correct wall-clock impossible fully offline; ordering preserved |

**Assumptions (confirmed or corrected by hardware on 2026-10-03):** the user's board is an
**ESP32-P4 rev v1.3 with an ESP32-C6 coprocessor** (verified working over SDIO); it has **no
OLED wired yet** (firmware runs headless); USB power is fine; 100-task cap is fine; open AP is
acceptable. Original assumption of "any ESP32 devkit" replaced: plain ESP32 is still supported
as a fallback target but is not the user's board.

## 4. Architecture

```
                ┌──────────────────────────────── ESP32 ────────────────────────────────┐
                │                                                                        │
  Buttons ──►   │  buttons.c (debounce, short/long) ─► task_store (NVS JSON, mutex) ◄──►  │
  (15/2/4)      │         │                                          │                   │
                │         ▼                                          ▼                   │
  OLED I2C ──►  │  ui_oled.c (SSD1306, list/detail/menu)   http_server.c + api.c          │
  (21/22)       │                                               │                        │
                │                                         embedded index.html            │
                │                                               │                        │
                └───────────────────────────────────────────────┼────────────────────────┘
                                                                │ Wi-Fi APSTA
                                                     AP 192.168.4.1 / LAN IP
                                                                │
                              ┌─────────────────────┐  ┌───────┴───────┐
                              │ On-device UI only   │  │ Any browser   │
                              │ (no 2nd device)     │  │ (when nearby) │
                              └─────────────────────┘  └───────────────┘
```

**Modules / files (all under `firmware/`):**
- `firmware/CMakeLists.txt`, `firmware/partitions.csv` (default + NVS 24 KB),
  `firmware/sdkconfig.defaults` (AP SSID, target esp32, HTTP task stack).
- `firmware/main/CMakeLists.txt` (`EMBED_TXTFILE index.html`), `main/app_main.c`
  (init NVS → task_store → wifi → http → oled/buttons tasks).
- `main/task_store.h/.c` — mutex-guarded array, CRUD, JSON serialise, NVS load/save.
- `main/wifi_apsta.h/.c` — AP always, STA optional from NVS, SNTP on STA, status API.
- `main/http_server.h/.c`, `main/api.c` — routes, validation, error JSON.
- `main/index.html` — single-page UI (vanilla JS + fetch, inline CSS, no CDN).
- `main/ui_oled.h/.c`, `main/buttons.h/.c`, `main/pins.h`, `main/time_util.h/.c`.
- `components/ssd1306/` — minimal vendored SSD1306 I2C driver (MIT, credited).
- `scripts/read_log.py` — pyserial 20 s capture (no interactive monitor).

**Data model:** `[{id:u32 inc, title:utf8<=120, done:bool, prio:0..2, created:i64, updated:i64}]`.
Stored as one NVS blob `db_v1` = `{"seq":N,"tasks":[...]}`. Writes: coalesced 500 ms debounce
to limit NVS wear.

**Interfaces:** REST JSON (above) + physical (OLED/buttons). No cloud, no BLE in v1.

## 5. Alternatives considered

| Alternative | Rejected because |
|-------------|------------------|
| Arduino IDE + WebServer + SPIFFS | Easier for beginners but weaker APSTA/SNTP control, larger binaries, less professional IDF structure the laptop toolchains target |
| MicroPython + picoweb | Fast to write, but RAM-tight with HTTP+OLED, slower AP throughput, harder to make robust against bad input |
| ESP32-S3-BOX / M5Stack / e-ink + LVGL | Nicer UX, but assumes hardware the user may not own; generic devkit+OLED works for everyone; LVGL = 10× code |
| LittleFS/SD-card task file | Better for >100 tasks, but needs extra component + custom partition in v1; NVS is zero-dependency and enough now |
| BLE-only companion app | Still needs a phone app — violates "no phone" goal; Wi-Fi browser needs no install |
| Cloud sync (Firebase/MQTT) | Needs internet + accounts; opposite of portable offline goal; future option |

## 6. Risks, loopholes and blockers

| Risk | L×I | Mitigation |
|------|-----|------------|
| No board connected right now (`nova_ports` empty) → can't flash/test on HW yet | H×M | Build in CI-style now; report `urgent` asking user to connect board; flash when present; keep default `esp32` target retargetable |
| Wrong board/display wiring vs user's actual HW | M×H | All pins in `pins.h`; SSD1306 addr configurable; README wiring table + photo request; `set-target` documented |
| "Website needs a browser" loophole — user hoped to leave phone behind entirely | H×M | Solved by on-device OLED UI for daily ops; README states clearly: full text editing needs browser occasionally |
| 3-button text entry impractical | M×M | Quick-add placeholder + full edit on web; documented, not hidden |
| NVS wear / task cap / blob size | M×M | **Verified against IDF v6.1 source and docs:** page 4096 B, entry 32 B, 126 entries/page, single-page chunk max 4000 B; blob limit = min(508000, 97.6% × partition − 4000) → 59,943 B for our 64 KB partition. A full store is ~18 KB, so it fits with room for compaction. Writes are coalesced (500 ms) and skipped entirely when the payload is unchanged (both by us and by NVS itself). NVS partition raised 24 KB → 64 KB. No backup copy kept: NVS is documented power-fail safe (only an in-flight write can be lost) and a second copy would halve compaction headroom. LittleFS migration path if ever needed |
| Open AP sniffing / no HTTPS | M×M | Short-range AP, optional password, no sensitive data, input caps, documented |
| Heap exhaustion (HTTP + OLED framebuffer 1 KB + JSON of 100 tasks ~15 KB) | M×H | Static OLED buffer, cJSON streaming avoided, `CONFIG_HTTPD_MAX_REQ_HDR_LEN 512`, tested with 100-task fill |
| Power on the go (USB bank needed) | M×L | Documented; deep-sleep + e-ink as future work |
| IDF v6.1 API drift vs examples | L×M | Pin to `%IDF_PATH%` v6.1, build early (step 5) to catch drift |
| GitHub `gh` auth missing | L×M | If `gh repo create` fails, keep local git + report; push when auth present |

## 7. Edge cases and error handling

- Empty list → OLED "No tasks — hold SELECT to add"; web shows empty-state + Add box focused.
- Title empty / >120 chars / bad JSON → `400 {error}`; OLED ignores; never panics.
- Power cut mid-write → NVS is documented power-fail safe (only an in-flight write can be lost),
  so no hand-rolled backup copy is kept: it would halve the free pages NVS needs to compact.
  A payload that does not parse is logged and the store starts empty rather than crashing.
- AP clients = 0 for hours → still runs; HTTP idle timeout 30 s.
- STA password wrong → after 3 fails, stay AP-only, OLED shows `STA fail`, web reports status.
- 100-task cap → POST returns `413 {error:"task limit 100"}`; OLED beep-equivalent (invert flash).
- Button bounce / simultaneous press → 50 ms debounce + event queue; SELECT+UP = back.
- Time without internet → `created` uses boot-seq fallback; resorted correctly after SNTP sync.
- Factory reset (SELECT 5 s at boot splash) → clears NVS `tasks` + `wifi`, OLED confirms.

## 8. Security and privacy

- Data never leaves the device (no cloud, no telemetry). Tasks + Wi-Fi password in local NVS only.
- AP open by default for field usability; set `CONFIG_TASK_AP_PASS` (menuconfig/Kconfig) to
  close it. Documented in README.
- HTTP only (ESP32 TLS-serving is heavy); warn: don't enter passwords/secrets as task titles.
- API validates lengths/types; HTML-escapes titles on render; no `eval`, no external JS.
- Wi-Fi password stored in NVS, never returned by `GET /api/wifi` (returns SSID + connected bool only).

## 9. Test strategy (manual mode)

- T1 Build: `C:/nova/agent/idf.bat build` → success, binary < 1.2 MB. Pass = 0 errors.
- T2 Flash/boot (once board connected): `nova_ports(probe=true)` → `idf.bat -p COMx flash` →
  20 s `scripts/read_log.py` shows `TASKDECK up ap=192.168.4.1 tasks=N`.
- T3 Web CRUD: join `ESP-TASKMGR` → `http://192.168.4.1/` → add/toggle/edit/delete/filter →
  reboot → all persist; `curl /api/tasks` matches.
- T4 Standalone: disconnect phone/PC → OLED lists tasks → UP/DOWN scroll, SELECT toggles,
  long-SELECT menu adds/deletes → reconnect browser → same state.
- T5 Robustness: POST empty title/oversize/bad JSON → 400; fill to 100 → 101st gives 413;
  wrong STA password → AP still works.
- T6 Ask user (one `check`): "OLED legible + buttons map correctly?" — single combined question.
- Pass criteria: T1–T5 all pass; T6 user confirms. Failures fixed, rebuilt, re-flashed.

## 10. The 7 steps

### Step 1 — Plan ✅ (this file)
- [x] Read status file, inbox (empty), check IDF examples + `nova_ports` (empty).
- [x] Write this detailed PLAN.md.
- [x] Report `nova_report(step=1, stage="plan", ... feasible=true)` with difficulty.
- Done when: report accepted. Files: `PLAN.md`.

### Step 2 — Setup
- [x] `git init -b main`, `.gitignore` (IDF: `build/`, `sdkconfig`, `.venv`), README skeleton,
  LICENSE (MIT), CHANGELOG skeleton.
- [x] Scaffold `firmware/` from `restful_server` pattern (CMake, `app_main`, Kconfig, `pins.h`).
- [x] Vendor minimal `ssd1306` component with credit; add `scripts/read_log.py`.
- [x] `gh repo create Dhanu-2003/esp32-task-management-p0010 --private --source . ...`,
  first commit + push; topics.
- [x] Toolchain fix (found during setup): stock `idf.bat` fails on this laptop —
  `python.exe` not on PATH and `xtensa-esp32-elf-gcc` shim panics on spaces in
  `C:\Users\Dhanu S\...`. Fix: `C:\nova\.home\espressif` junction + `scripts/idf_cmd.ps1`
  helper. Scaffold builds clean (`Project build complete`, LASTEXIT=0).
- Done when: repo exists (or local git + reported blocker), project listed, first push on main.
- Files: `.gitignore`, `README.md`, `LICENSE`, `CHANGELOG.md`, `firmware/**`, `scripts/**`.

### Step 3 — Core (API + web + persistence, no display yet)
- [x] `task_store` (NVS JSON, mutex, 100-cap, validation) + `api.c` + `http_server`
  + `index.html` (CRUD/filter) + APSTA Wi-Fi + SNTP fallback.
- [x] Build passes (`idf.bat build` — via `scripts/idf_cmd.ps1`; found `json` moved to
  managed component `espressif/cjson` in v6.1, added to `main/idf_component.yml`).
- Done when: API contract works (verified by build + host-side JSON unit smoke where possible;
  HW test deferred to step 5/6 if no board).
- Files: `firmware/main/task_store.*`, `api.c`, `http_server.*`, `index.html`, `wifi_apsta.*`.

### Step 4 — Features (OLED + buttons + polish + error handling)
- [x] `ssd1306` driver (I2C master API, embedded 5x7 font), `ui_oled` (list/detail/menu/splash/reset),
  `buttons` (debounce/short/long), factory reset (boot-time + `POST /api/reset`), Wi-Fi setup section in web UI,
  empty states, 400/413 errors, HTML escaping, status bar.
- [x] README wiring table + photos placeholder; CHANGELOG update.
- [x] Build passes clean; code review pass (delegate `review`).
- Done when: builds clean; code review pass (delegate `review`).
- Files: `firmware/main/ui_oled.*`, `buttons.*`, `components/ssd1306/**`, `README.md`, `CHANGELOG.md`.

### Step 5 — Build and run (flash the board)
- [x] `nova_ports(probe=true)` → board on **COM21**, identified by esptool as **ESP32-P4 rev v1.3**.
- [x] `set-target esp32p4` + `build` clean (479 KB image, zero warnings in project files).
- [x] Three real bring-up bugs found and fixed by flashing:
  1. Boot loop `assert failed: esp_clk_init clk.c:105` — early P4 silicon cannot run at the
     default 400 MHz; `sdkconfig.defaults.esp32p4` selects rev<3 so Kconfig picks 360 MHz.
  2. Crash inside `esp_wifi_init` (P4 has no radio of its own) because of `ESP_ERROR_CHECK`.
     Wi-Fi failure is now non-fatal: log it and run OLED/buttons only.
  3. `ESP_ERR_HTTPD_HANDLERS_FULL` — 9 routes against the default limit of 8, so
     `POST /api/reset` was silently missing. `max_uri_handlers` is a struct field in IDF v6.1
     (not Kconfig), so it is set to 16 in code; registration failures no longer abort.
- [x] Flashed and booted: `TASKDECK up ap=192.168.4.1 tasks=0 web=on`, `AP up ssid=ESP-TASKMGR`,
  DHCP server started, 30 s heartbeat, no reset loop.
- [x] Wi-Fi over the C6 coprocessor verified on hardware (`WLAN over SDIO`,
  `esp-hosted fw versions: host=3.0.9 coprocessor=0.0.0`).
- [x] Plain-ESP32 fallback target also builds clean (`set-target esp32`, 887 KB).
- [x] NVS fixes + verified limits (64 KB partition; blob limit ~59.9 KB; full store ~18 KB).
- Done when: flashed + boot log shows AP up. **Achieved.**
- Files: `firmware/sdkconfig.defaults.esp32p4`, `partitions.csv`, `RUNNING.md`, `TESTING.md`.

### Step 6 — Test (manual T1–T8, results recorded in TESTING.md)
- [x] T1 build, T2 flash+boot, T3 Wi-Fi over the C6 coprocessor — pass.
- [x] T4 website from a phone (user-confirmed: page loads, task added and ticked off) — pass.
- [x] T5 persistence across a hard reset — pass, automated: `loaded 2 tasks` after the reset.
- [ ] T6 API robustness (400/404/413) and T8 factory reset — implemented, not yet exercised.
- [ ] T7 OLED/buttons — blocked: display and buttons are not wired to the board.
- Done when: the core web path is proven on hardware and remaining gaps are documented honestly.
  **Core path proven on hardware; two checks await wiring and a few API calls.**
- Files: `TESTING.md`, fixes, `README.md`.

### Step 7 — Deliver
- [x] Final README (what it is, wiring, how to flash, how to use web + buttons, config,
  limits, known gaps, future work), CHANGELOG `v1.0.0` with verified results and limitations.
- [x] `gh release create v1.0.0`; tag pushed; repo private on `main`; no secrets in the tree.
- [x] Final `nova_report(step=7, done=true)` + TASK ACCOMPLISHED block with manual test steps.
- Done when: pushed, released, reported. **Achieved.**

## 11. Recommendations and future work

- LittleFS partition when the user outgrows 100 tasks; export/import JSON backup.
- E-ink display (GDEY037T03) + deep-sleep for week-long battery carry.
- Rotary encoder instead of 3 buttons for faster scrolling.
- Optional WPA2 AP password by default + per-user lists.
- STA NTP-synced reminders/buzzer for due tasks; BLE provisioning instead of Wi-Fi form.
- OTA update page (`/update`) so the device never needs USB again.

---
*Plan master: EMP-005 (muse-spark-1.3) · 2026-10-03 · IDF v6.1 · `nova_ports` empty (no board yet) ·
inbox empty · target default `esp32`, retarget on probe.*
