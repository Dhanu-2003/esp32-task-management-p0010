# ESP32 Task Management — TaskDeck on ESP32

A task manager that lives on an ESP32 board, so you carry one small device instead of a laptop
or phone. The device shows your task list on a small OLED with three buttons, and it also
serves a real task website (`http://192.168.4.1/`) over its own Wi-Fi whenever you want a full
editor. Everything works offline; tasks are stored on the device and survive a power cycle.

Because the brief was "I don't want to carry my laptop or phone", the on-device OLED and
buttons are the main interface — the website is there for when you want to type longer text,
and it needs no internet connection and no app installation.

**Verified working on real hardware** (2026-10-03): flashed on an ESP32-P4 board, access point
`ESP-TASKMGR` up at `http://192.168.4.1/`, tasks added from a phone and still present after a
hard reset.

## What you get

- **Offline task website on the ESP32** — add, edit, tick off, delete, set priority, filter
  (All / Active / Done). Mobile-friendly, single page, no external scripts or CDN.
- **Standalone device UI** — OLED task list with a menu (quick add, delete, reset all). Works
  with no phone or laptop nearby. Runs headless if no display is wired.
- **Persistent storage** — up to 100 tasks in NVS, survives reboots and power cuts.
- **Always reachable** — the device runs its own Wi-Fi access point, so it works in the street
  as well as at home. It can optionally also join your home Wi-Fi (LAN access + clock sync).

## Which board?

| Board | Wi-Fi | Status |
|-------|-------|--------|
| **ESP32-P4** with ESP32-C6 coprocessor (yours, on COM21) | from the C6 over SDIO | **Tested and working** |
| Plain ESP32 / ESP32-S3 devkit | built in | Supported fallback, builds clean |

The ESP32-P4 has **no Wi-Fi radio of its own**. Your board carries an ESP32-C6 coprocessor,
and the firmware brings Wi-Fi up over SDIO. The C6 must have ESP-Hosted slave firmware
(pre-flashed on boards such as the ESP32-P4-Function-EV-Board).

## Hardware

For the standalone experience, wire:

| Part | Connection | Default pin |
|------|------------|-------------|
| SSD1306 128×64 I2C OLED (addr `0x3C`) | SDA / SCL / VCC 3V3 / GND | GPIO21 / GPIO22 |
| Button UP | between GPIO and GND | GPIO15 |
| Button DOWN | between GPIO and GND | GPIO2 |
| Button SELECT (OK) | between GPIO and GND | GPIO4 |

Buttons are active-low using the chip's internal pull-ups: no external resistors needed. All
pins, the display address and the timings live in one file, `firmware/main/pins.h`, so you can
rewire everything for your board without touching any other code.

**No OLED is wired yet.** The boot log says `ui: OLED init failed ...; continuing headless`.
That is deliberate and harmless: the website keeps working, and plugging in an SSD1306 brings
the on-device UI up with no code change.

## Build and flash

On this laptop, build through `scripts/idf_cmd.ps1`, which sets up the ESP-IDF environment
(Python on `PATH`, plus a space-free tools path — the cross-compiler shim fails on paths
containing spaces). Run it from the `firmware/` directory and always pass the port explicitly:

```powershell
cd firmware
powershell -ExecutionPolicy Bypass ..\scripts\idf_cmd.ps1 set-target esp32p4   # first time / changing board
powershell -ExecutionPolicy Bypass ..\scripts\idf_cmd.ps1 build
powershell -ExecutionPolicy Bypass ..\scripts\idf_cmd.ps1 -p COM21 flash
python ..\scripts\read_log.py --port COM21 --seconds 20 --out boot.log
```

A healthy boot ends with:

```
I (425) task_store: loaded 2 tasks (seq=2)
I (2495) wifi: AP up ssid=ESP-TASKMGR ip=192.168.4.1 (STA not configured)
I (2505) taskdeck: TASKDECK up ap=192.168.4.1 tasks=2 web=on
```

Notes that will save you time:

- **Early P4 silicon:** `firmware/sdkconfig.defaults.esp32p4` sets
  `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_100=y` for your rev v1.3
  board. Rev <3.0 and rev >=3.0 P4 silicon are mutually exclusive in IDF — comment those two
  lines out for a newer board. That path only supports 40/90/180/360 MHz, so the CPU runs at
  **360 MHz**; the IDF default of 400 MHz is unsupported here and puts the chip in a reboot
  loop (`assert failed: esp_clk_init`).
- **Partition table:** the 64 KB NVS area for the task store comes from
  `firmware/partitions.csv`, which is only used because `sdkconfig.defaults` sets
  `CONFIG_PARTITION_TABLE_CUSTOM=y`. Without that line the file is silently ignored and the
  task store silently gets only 24 KB. Run `fullclean` before `build` after changing the
  partition table, and note that flashing a new table erases the old NVS area.
- **Managed components:** `espressif/cjson` (JSON), plus `espressif/esp_wifi_remote` and
  `espressif/esp_hosted` which are pulled in only for `esp32p4`/`esp32h2` via `rules:` in
  `firmware/main/idf_component.yml`. They are needed on the first build and need network access.

## Use it

**On the device (no phone needed):**
- UP / DOWN move the cursor through the list.
- SELECT (short) ticks the task done / not done.
- SELECT (hold ~1 s) opens the menu: Add quick task, Delete selected, Reset all, Back.
- SELECT held while powering on (3 s) factory-resets everything.
- A quick-added task is named `Task N`; rename it from the website when convenient.

**In the website:** join the Wi-Fi `ESP-TASKMGR` and open `http://192.168.4.1/`. The same page
lets you set your home Wi-Fi so the device is also reachable on your LAN.

## Configuration

Edit with `idf.py menuconfig` under *TaskDeck Configuration*, or in `sdkconfig.defaults`:

| Option | Meaning | Default |
|--------|---------|---------|
| `CONFIG_TASK_AP_SSID` | access point name | `ESP-TASKMGR` |
| `CONFIG_TASK_AP_PASS` | access point password (empty = open) | empty |
| `CONFIG_TASK_AP_MAX_CONN` | max clients on the AP | 4 |

## API

| Method & path | Purpose |
|---------------|---------|
| `GET /api/tasks` | all tasks as JSON |
| `POST /api/tasks` | create: `{"title":"...","priority":0..2}` |
| `PUT /api/tasks/<id>` | update `title`, `done` and/or `priority` |
| `POST /api/tasks/<id>/toggle` | flip done |
| `DELETE /api/tasks/<id>` | delete |
| `GET /api/wifi` | AP/LAN status (never returns the stored password) |
| `POST /api/wifi` | save home Wi-Fi: `{"ssid":"...","pass":"..."}` |
| `POST /api/reset` | delete all tasks |

## Limits worth knowing

- Up to 100 tasks. The store is one NVS blob; a full store is ~18 KB against a verified limit
  of ~59.9 KB for the 64 KB partition.
- The OLED shows ASCII only; other characters are stored correctly and shown properly on the
  website.
- The web server is plain HTTP on a local access point — fine for personal tasks, but do not
  type passwords or secrets as task titles.
- Typing task text needs the website (three buttons cannot type); on the device you quick-add
  and toggle.
- USB power is expected while in use; battery operation with an e-ink screen is a possible
  next step.
- Time stamps only become real wall-clock time once the device has joined a Wi-Fi network
  (SNTP); offline it keeps a monotonic order instead.

## Project layout

```
firmware/              ESP-IDF project (the firmware that runs on the board)
  main/                app wiring, task store, HTTP API, web UI, OLED UI, buttons, pins
  components/ssd1306/  small SSD1306 I2C driver (embedded 5x7 font)
scripts/               build helper, serial log capture
TESTING.md             manual test steps and what passed
PLAN.md                the plan this was built from, with decisions and risks
RUNNING.md             what runs where, and the known hardware gaps
CHANGELOG.md           what changed
```

## Ideas for later

- A tiny filesystem (LittleFS) instead of NVS when you want more than 100 tasks or an
  export/import backup.
- An e-ink screen plus deep sleep for week-long battery life.
- A rotary encoder for quicker scrolling, and a buzzer for due-task reminders.
- Over-the-air updates so the device never needs a USB cable again.

## License

MIT — see `LICENSE`.
