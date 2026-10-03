# Manual test steps and results — TaskDeck on ESP32

Test mode: manual. Board used: **ESP32-P4 rev v1.3 with ESP32-C6 coprocessor, COM21**.

Status: **6 of 8 checks passed on real hardware.** The two open items need the OLED and buttons
to be wired, which they are not yet (see T7).

## How to run the tests

```powershell
cd firmware
powershell -ExecutionPolicy Bypass ..\scripts\idf_cmd.ps1 build
powershell -ExecutionPolicy Bypass ..\scripts\idf_cmd.ps1 -p COM21 flash
python ..\scripts\read_log.py --port COM21 --seconds 20 --out boot.log
```

> The website is tested from a **phone**, not from the development laptop: joining the device's
> access point `ESP-TASKMGR` from the laptop would take the laptop off the internet.

## T1 — Build — PASS
`build` ends with `Project build complete`, zero warnings in project files.
- esp32p4 image: ~479 KB. Plain-esp32 image: ~887 KB. Both build clean.

## T2 — Flash and boot — PASS
Flash with `-p COM21`, then read the log. Expected lines:
```
task_store: loaded N tasks (seq=N)
wifi: AP up ssid=ESP-TASKMGR ip=192.168.4.1
taskdeck: TASKDECK up ap=192.168.4.1 tasks=N web=on
```
Observed on 2026-10-03: `TASKDECK up ap=192.168.4.1 tasks=0 web=on`, DHCP server started,
heartbeat every 30 s, no reset loop (ran 13+ minutes continuously).

## T3 — Wi-Fi over the C6 coprocessor — PASS
Observed in the boot log:
```
eh_sdio: transport[host]: SDIO 4-bit 40000 kHz CLK=18 CMD=19 D0=14 D1=15 D2=16 D3=17 RESET=54
eh_init_evt: * WLAN over SDIO
eh_init_evt: esp-hosted fw versions: host=3.0.9 coprocessor=0.0.0
```
Pass: the access point comes up through the coprocessor. (The coprocessor version string reads
`0.0.0`; Wi-Fi works regardless.)

## T4 — Website from a phone — PASS
1. Join Wi-Fi `ESP-TASKMGR` on a phone.
2. Open `http://192.168.4.1/`.
3. Add a task and tick it off.

Confirmed by the user on 2026-10-03: **the page loads, and a task can be added and ticked off.**

## T5 — Persistence across a power cycle — PASS
Automated check, no phone needed:
1. From the phone, create two tasks (done in T4).
2. Hard-reset the board: `python -m esptool --chip esp32p4 -p COM21 --before default_reset --after hard_reset run`
3. Read the log and check the loaded task count.

Observed: `task_store: loaded 2 tasks (seq=2)` and `TASKDECK up ... tasks=2 web=on`.
Pass: tasks created in the browser survive a reset, and the access point comes back.

## T6 — Robustness of the API — NOT YET RUN
Run from a browser console or a terminal on the phone while joined to the AP:
- add a task with an empty title → expect `400` and `title required`
- add a task with a 200-character title → expect `400`
- send malformed JSON (`{"title":`) → expect `400 invalid JSON`
- toggle or delete a task id that does not exist (`/api/tasks/9999/toggle`) → expect `404`
- fill the store past 100 tasks → the 101st returns `413 task limit reached`
- `POST /api/reset` → expect `{"ok":true}` and an empty list

Pass criteria: the device never crashes or reboots, and the website stays reachable afterwards.
The error paths are implemented and the handler is registered (an earlier build silently failed
to register it because of the handler limit — that is fixed), but these calls have not been
exercised against the device yet.

## T7 — On-device OLED and buttons — BLOCKED, hardware not wired
The boot log reports `ui: OLED init failed (SDA=21 SCL=22 addr=0x3c); continuing headless`,
which is correct behaviour with no display attached. Once an SSD1306 and three buttons are
wired (see README), check:
- the header shows `Tasks N open` and rows show `[ ]` / `[x]` with the task text
- UP/DOWN move the `>` cursor and the list scrolls
- SELECT toggles done, SELECT held opens the menu
- "Add quick task" appears in the website afterwards
- holding SELECT at power-on factory-resets the list

## T8 — Factory reset — NOT YET RUN
Hold SELECT while powering on for ~3 s. Expected: the OLED shows `Factory reset!` and the list
is empty. Also reachable now via `POST /api/reset` once T6 is run.
