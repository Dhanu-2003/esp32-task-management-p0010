# ESP32 Task Management (TaskDeck on ESP32) — P0010

A portable task manager hosted on an ESP32. Carry just the ESP32 device instead of a
laptop/phone: the daily list lives on an OLED display with 3 buttons, and the same list is
served as an offline website (`http://192.168.4.1/`) whenever a phone/laptop browser is nearby.

> Status: scaffolding (step 2). Core API + web UI lands in step 3, OLED/buttons in step 4.
> See `PLAN.md` for the full plan.

## Hardware (default wiring, all changeable in `firmware/main/pins.h`)

| Part | Connection |
|------|------------|
| ESP32 devkit (default target `esp32`) | USB 5 V power |
| SSD1306 128×64 OLED, I2C addr 0x3C | SDA GPIO21, SCL GPIO22, VCC 3V3, GND GND |
| Button UP / DOWN / SELECT (active-low, `INPUT_PULLUP`) | GPIO15 / GPIO2 / GPIO4 to GND |

Wire each button between its GPIO and GND (no external resistors — internal pull-ups).
The firmware runs headless if no display is wired (web UI keeps working).

## Using it

- **Carried device (no phone):** OLED lists tasks. UP/DOWN scroll, SELECT ticks off,
  hold SELECT for the menu (quick-add / delete / reset). Hold SELECT at boot for
  factory reset.
- **Browser (when nearby):** join Wi-Fi `ESP-TASKMGR`, open `http://192.168.4.1/`
  — full editor, filters, priorities, home Wi-Fi setup.

## Build (this laptop)

The stock wrapper needs the working environment first — always build via the helper
(python.exe on PATH + space-free tools junction, see `scripts/idf_cmd.ps1`):

```powershell
cd firmware
powershell ..\scripts\idf_cmd.ps1 set-target esp32
powershell ..\scripts\idf_cmd.ps1 build
powershell ..\scripts\idf_cmd.ps1 -p COM7 flash
```

Then join Wi-Fi `ESP-TASKMGR` and open `http://192.168.4.1/`.

## Project structure

```
firmware/            ESP-IDF project (builds to the ESP32)
  main/              app_main, task_store, HTTP API, web UI, OLED, buttons
  components/        vendored ssd1306 driver (MIT, credited)
scripts/             serial log capture (no interactive monitor)
PLAN.md              detailed plan, requirements, architecture
TESTING.md           manual test steps (from step 6)
RUNNING.md           what runs where (from step 5)
CHANGELOG.md         Keep a Changelog
```

## License

MIT — see `LICENSE`.
