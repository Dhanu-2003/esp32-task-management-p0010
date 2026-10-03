# Manual test steps — TaskDeck on ESP32

Test mode: manual. Run these once a board is flashed (see README for flashing).
Reset counts: after "reboot", press RESET on the ESP32.

## T1 — Build
```powershell
cd firmware
powershell ..\scripts\idf_cmd.ps1 build
```
Pass: ends with `Project build complete`.

## T2 — Flash and boot
Find the port (never guess), then flash and read the log:
```powershell
powershell ..\scripts\idf_cmd.ps1 -p COMx flash
python ..\scripts\read_log.py --port COMx --seconds 20 --out boot.log
```
Pass: log contains `TASKDECK up ap=192.168.4.1 tasks=N`.
Note: if no OLED is wired you also see `OLED init failed` — that is fine, the web UI still works.

## T3 — Web UI and persistence (the main test)
1. Join Wi-Fi `ESP-TASKMGR` on a phone or laptop (no internet needed).
2. Open `http://192.168.4.1/`.
   Pass: the TaskDeck page loads in under ~2 s.
3. Add task `Buy milk`. Pass: it appears in the list, count says `1 open / 1`.
4. Refresh the page. Pass: the task is still there.
5. Tick its checkbox. Pass: it shows as done and the count drops to `0 open`.
6. Add `Pay bills` with priority High. Pass: it shows the `High` badge.
7. Edit `Buy milk` to `Buy oat milk`. Pass: the new text shows.
8. Delete `Pay bills`. Pass: it disappears.
9. Reboot the ESP32, reload the page. Pass: `Buy oat milk` is still there and still done.
10. Open `http://192.168.4.1/api/tasks`.
    Pass: JSON lists the same task(s) with `done:true` and the ids from the page.
11. Filters All / Active / Done. Pass: each shows the right subset.

## T4 — Standalone use (the point of the project: no phone)
1. Disconnect the phone/computer from the AP (or turn its Wi-Fi off).
2. Look at the OLED.
   Pass: header shows `Tasks N open`, rows show `[ ]`/`[x]` and the task text.
3. Press DOWN a few times.
   Pass: the `>` cursor moves down and the list scrolls when you pass the bottom.
4. Press SELECT on a task.
   Pass: it flips between `[ ]` and `[x]`.
5. Hold SELECT (~1 s).
   Pass: a menu appears (Add quick task / Delete selected / Reset all / Back).
6. Choose `Add quick task`, press SELECT.
   Pass: it reports it was added.
7. Reconnect the phone to `ESP-TASKMGR`, reload `http://192.168.4.1/`.
   Pass: the changes made with the buttons are there — the device and the web UI share one list.

## T5 — Robustness
1. With the page open, run these in a browser console or terminal:
   - Add a task with an empty title → expect `400` and `title required`.
   - Add a task whose title is 200 characters → expect `400`.
   - Send malformed JSON (e.g. `{"title":`) → expect `400 invalid JSON`.
   - Toggle/delete a task id that does not exist (e.g. `/api/tasks/9999/toggle`) → expect `404`.
   - Fill the store past 100 tasks → the 101st returns `413 task limit reached`.
   Pass: the device never crashes or restarts; the web UI stays reachable afterwards.
2. Power-cycle during normal use (unplug and replug USB).
   Pass: after reboot the list is intact and no task is corrupted.
3. In the Wi-Fi section, enter a home Wi-Fi name with a wrong password.
   Pass: the AP keeps working; status shows it is still trying to join.

## T6 — Factory reset
Hold SELECT while powering on, for about 3 seconds.
Pass: the OLED shows `Factory reset!` and the task list comes back empty.

## T7 — User-visible check (only a person can do this)
With the board wired and running, look at the OLED and confirm in one look:
- the text is legible and not garbled,
- pressing UP/DOWN/SELECT does what the labels say.
