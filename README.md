# ESP32 Task Management — TaskDeck on ESP32

A task manager that lives on an ESP32, so you carry one small device instead of a laptop or
phone. The device shows your task list on a small OLED and three buttons, and it also serves
a working task website (`http://192.168.4.1/`) over its own Wi-Fi whenever you want a full
editor. Everything works offline; tasks are stored on the device and survive a power cycle.

Because the brief was "I don't want to carry my laptop or phone", the on-device OLED and
buttons are the main interface — the website is there for when you want to type longer text,
and it does not require any internet connection or app installation.

## What you get

- **Offline task website on the ESP32** — add, edit, tick off, delete, set priority, filter
  (All / Active / Done). Mobile-friendly, single page, no external scripts or CDN.
- **Standalone device UI** — OLED task list with a menu (quick add, delete, reset all). Works
  with no phone or laptop nearby. Runs headless if no display is wired.
- **Persistent storage** — up to 100 tasks in NVS, survives reboots and power cuts.
- **Always reachable** — the ESP32 runs its own Wi-Fi access point (`ESP-TASKMGR`), so it works
  in the street as well as at home. Optionally it can also join your home Wi-Fi (for LAN
  access and real clock sync).

## Hardware

Any ESP32 dev board (default target `esp32`) plus, for the standalone experience:

| Part | Connection | Default pin |
|------|------------|-------------|
| SSD1306 128×64 I2C OLED (addr `0x3C`) | SDA / SCL / VCC 3V3 / GND | GPIO21 / GPIO22 |
| Button UP | between GPIO and GND | GPIO15 |
| Button DOWN | between GPIO and GND | GPIO2 |
| Button SELECT (OK) | between GPIO and GND | GPIO4 |

Buttons are active-low using the ESP32's internal pull-ups: no external resistors needed.
All pins, the display address and the timings are in one file, `firmware/main/pins.h`, so
you can rewire everything for your board without touching any other code.

## Build and flash

The build runs through `scripts/idf_cmd.ps1`, which sets up this laptop's ESP-IDF
environment (Python on PATH, plus a space-free tools path — the cross-compiler shim fails on
paths containing spaces).

```powershell
cd firmware
powershell ..\scripts\idf_cmd.ps1 set-target esp32     # first time, or when changing board
powershell ..\scripts\idf_cmd.ps1 build
powershell ..\scripts\idf_cmd.ps1 -p COMx flash       # always give the port explicitly
```

To read the device's output without an endless monitor:

```powershell
python ..\scripts\read_log.py --port COMx --seconds 20 --out boot.log
```

Note: the firmware uses a custom partition table (64 KB of NVS for the task store). The
first time you flash after switching partition tables, run `fullclean` before `build`, and
note that flashing a new table erases the old NVS area.

## Use it

**On the device (no phone needed):**
- UP / DOWN move the cursor through the list.
- SELECT (short) ticks the task done / not done.
- SELECT (hold ~1 s) opens the menu: Add quick task, Delete selected, Reset all, Back.
- SELECT held while powering on (3 s) factory-resets everything.
- A quick-added task gets the name `Task N`; rename it from the website when convenient.

**In the website:** join the Wi-Fi `ESP-TASKMGR` and open `http://192.168.4.1/`. The same page
lets you set your home Wi-Fi so the device is also reachable on your LAN.

## Configuration

Edit with `menuconfig` (`idf.py menuconfig`) under *TaskDeck Configuration*, or in
`sdkconfig.defaults`:

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
| `GET /api/wifi` | AP/lan status (never returns the stored password) |
| `POST /api/wifi` | save home Wi-Fi: `{"ssid":"...","pass":"..."}` |
| `POST /api/reset` | delete all tasks |

## Limits worth knowing

- Up to 100 tasks.
- The OLED shows ASCII; other characters are skipped on the display but stored correctly
  and shown properly in the website.
- The web server is plain HTTP on a local access point — fine for personal tasks, but do not
  type passwords or secrets as task titles.
- Task text entry needs the website (three buttons cannot type); on the device you quick-add
  and toggle.
- USB power is expected while in use; battery operation with an e-ink screen is a possible
  next step (see below).

## Project layout

```
firmware/            ESP-IDF project (the firmware that runs on the ESP32)
  main/              app wiring, task store, HTTP API, web UI, OLED UI, buttons, pins
  components/ssd1306/  small SSD1306 I2C driver (embedded 5x7 font)
scripts/             build helper, serial log capture
TESTING.md           full manual test steps
PLAN.md              the plan this was built from
RUNNING.md           what runs where
CHANGELOG.md         what changed
```

## Ideas for later

- A tiny filesystem (LittleFS) instead of NVS when you want more than 100 tasks or an
  export/import backup.
- An e-ink screen plus deep sleep for week-long battery life.
- A rotary encoder for quicker scrolling, and a buzzer for due-task reminders once real clock
  sync is in place.
- Over-the-air updates so the device never needs a USB cable again.

## License

MIT — see `LICENSE`.
