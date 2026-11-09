H2D ALARM - ESP32-S3 N16R8 firmware (PlatformIO / Arduino)
==========================================================

STATUS: written but NOT compiled or tested on hardware. Expect to fix a few
small compile errors on first build, and to tune things against your real H2D.

BUILD
  pio run -t upload        (firmware)
  pio run -t uploadfs      (uploads data/sounds/*.wav into LittleFS)
  pio device monitor

POWER (USB-C 5V only)
  - Use a USB-C breakout/receptacle board with 5.1k CC resistors (so a C-to-C
    charger gives 5V), feed the 5V rail. Use a 5V 2A+ charger.
  - 5V rail feeds: ESP32-S3 (5V pin), MAX98357A VIN, WS2812, buzzer.
  - Put a 470-1000uF capacitor across the 5V rail near the amp + buzzer.
  - Use a 5V buzzer. Passive piezo (loudest, PWM tone) via MOSFET, or an
    active 5V buzzer (set BUZZER_ACTIVE 1 in src/pins.h).

PINS (src/pins.h)
  GPIO4  buzzer MOSFET gate (1k series, 100k pulldown, flyback diode on buzzer)
  GPIO5  MAX98357A BCLK      GPIO6  LRC      GPIO7  DIN
  GPIO15 ACK button to GND   (short = stop, hold 1.5s = snooze)
  GPIO14 SETUP button to GND (hold 1s = enter/leave setup mode)
  GPIO16 WS2812 data (330R series), 12 LEDs
  MAX98357A GAIN: float or GND for louder.

SETUP MODE
  - Hold the SETUP button 1s. Blue breathing LEDs = setup mode.
  - Join Wi-Fi "H2D-Alarm-Setup" (password: h2d-alarm), open http://192.168.4.1
    (captive portal usually pops up). If already on your LAN, also reachable at
    http://h2d-alarm.local .
  - Tabs: status / Wi-Fi / printers (auto-discovered via SSDP) / alarm rules /
    sounds (upload mp3 etc., converted to WAV in the browser).
  - Setup mode auto-exits after the configured idle minutes (not when no Wi-Fi
    is configured yet). The web page is only served in setup mode.
  - Factory reset: hold SETUP while powering on for 3 s.

ALARM BEHAVIOUR
  stage 0 (gentle): speaker ramps up       stage 1: speaker full + buzzer pulses
  stage 2: buzzer continuous + LED strobe. Each event chooses its starting stage,
  sound, speaker/buzzer, auto-stop time, and whether it rings in do-not-disturb window.
  Events: print failed, finished (RUNNING->FINISH), paused (includes filament
  runout), error/HMS, printer offline (no report for N seconds).

THINGS TO VERIFY ON YOUR H2D
  1. Local MQTT (8883, user bblp, password = Access Code) may need LAN mode /
     developer mode depending on current firmware. Check in the printer settings.
  2. SSDP discovery (UDP 1990) is passive; if nothing shows, enter IP/serial by hand.
  3. PubSubClient buffer is capped at 60000 bytes. If H2D full-state snapshots are
     bigger they are skipped, but incremental updates (which carry the state
     changes) still work. If that proves flaky, swap to esp-mqtt client.
  4. HMS array can include low-severity notices -> disable the "error" event in the
     setup page if it is too chatty.
  5. Config (including Wi-Fi password and Access Code) is stored as plain JSON in LittleFS.

FIRMWARE UPDATE OVER Wi-Fi (web page > Firmware tab)
  - Settings, sounds and the error texts live in a separate flash partition and are NOT touched by an update.
    The new image goes to the idle one of the two 3 MB app slots; the device only switches to it after the whole
    image was received and checked, so a failed transfer leaves the running firmware as it was.
    It cannot start while an alarm is ringing. The first install over USB (Upload) is still needed once, with this version.
  - A) Upload a file: pick firmware.bin (.pio/build/esp32s3/firmware.bin, or from a Release) and press "Upload and update".
       The header is checked first (ESP image, built for ESP32-S3); anything else is rejected before writing.
  - B) From GitHub: enter "user/repo" as the update source and press "Check for updates". The device reads
       version.json and firmware.bin from the latest Release of that PUBLIC repo; if the version is newer, "Install" appears.
       A full http(s) folder URL ending in / also works (host the two files yourself, e.g. when GitHub is slow).
       version.json = {"version":"1.0.1","sha256":"<hex of firmware.bin>","size":<bytes>}; the sha256 is checked before
       the new image is accepted. The TLS connection does not verify GitHub's certificate (the device has no
       certificate store), so the sha256 protects against a corrupted download, not against a hostile network.
  - Publishing a release: commit, then  git tag v1.0.1  and  git push --tags . The workflow in
    .github/workflows/release.yml builds the firmware with that version, and creates the Release with firmware.bin
    and version.json. Use plain numbers (v1.2.3); the device compares them to the version shown on the Firmware tab.
    A local build without FW_VERSION reports 0.0.0-dev (set FW_VERSION=1.2.3 before pio run to stamp it).
  - Automatic rollback: a freshly installed firmware starts "on trial". The device confirms it after the main loop has
    run for 60 s; if it crashes, hangs (watchdog), boot-loops or is power-cycled before that, the bootloader goes back to
    the previous firmware by itself. So wait a minute after an update before touching the power. Limits: it only catches
    problems that show up within that minute (a bug that only appears when an alarm rings is not caught), and a USB
    flash always replaces the running image without a trial.

LANGUAGE (Chinese / English)
  - One setting for the whole device: cfg.lang (0 = Chinese, 1 = English), stored in config.json. The EN / 中文 button
    in the web page header changes it on the device (not just in the browser), so it is the same for every browser.
  - Push notifications (ntfy, Bark, Telegram, webhook, Feishu, WeCom, DingTalk) follow it and are sent in ONE language:
    title, event name, "Code" label, test message, the official error text. Webhook JSON also carries "lang":"zh"|"en".
  - The web page, its alarm log (official error texts) and the notifications all switch together; no reboot needed.
  - Not translated, because you typed them: printer names, custom error-code notes, alarm clock labels.
  - Serial log output is English only.

OFFICIAL ERROR TEXTS (built into the firmware: assets/hms_zh.bin + assets/hms_en.bin)
  - Notifications and the web alarm log show the official Bambu text for an error code, in the device language. A
    custom note you set for a code always wins. A 16-digit HMS code that is not in the official table still gets a
    meaning decoded from its own digits (module + level); other unknown codes show just the code.
  - The tables are part of the firmware, so the meanings work right after flashing, with nothing to upload, and every
    firmware update brings newer tables (the release workflow rebuilds them first).
  - Sources: Bambu Studio's open-source HMS data (H2D 094 + H2S 093), then Bambu's live table (e.bambulab.com) for the
    codes the first one lacks (~4800 HMS codes, ~800 print_error codes). Some codes exist in Bambu's data with an EMPTY
    text; those are shown as "not in the official list" plus the decoded module and level.
  - Rebuild the tables yourself (needs Python + internet):  python tools/build_hms.py   -> writes assets/hms_zh.bin and
    assets/hms_en.bin, then build the firmware. Options: --lang en or --lang zh-cn (only one file), --src <folder with
    hms_*.json> (use a local Bambu Studio clone, no online table).
  - Optional: put newer tables on ONE device without a firmware update (a file on the device overrides the built-in
    table for that language; one call per language):
      curl -u admin:PASSWORD -F "file=@assets/hms_zh.bin" "http://bambu-radar.local/api/hmsdb?lang=zh"
      curl -u admin:PASSWORD -F "file=@assets/hms_en.bin" "http://bambu-radar.local/api/hmsdb?lang=en"
    (drop -u when no web password is set or setup mode is on). The reply is {"ok":true,"lang":"en","count":N}; the
    status JSON (/api/state) has "hmsdbZh": N and "hmsdbEn": N = codes available per language (built in or uploaded).
  - An old single /hms.bin on the device keeps working as the Chinese file until you upload hms_zh.bin (which removes it).
  - Careful: "pio run -t uploadfs" replaces the WHOLE LittleFS partition (config.json with Wi-Fi / printers, sounds
    you uploaded in the browser, the event log). Only use it on a fresh device, or re-enter everything afterwards.
  - Re-run the script now and then: Bambu adds texts for new firmware.

MULTIPLE PRINTERS
  - Up to MAX_PRINTERS (src/config.h, default 4) printers, each with its own MQTT/TLS link.
  - Every extra printer costs ~40-50KB of internal heap. ESP32-S3 N16R8 should handle 4;
    going higher may need heap tuning. Watch free heap on the status page.
  - Alarm rules are shared by all printers. While an alarm is ringing, events from
    other printers are ignored until you press stop (they are logged on serial).

DO-NOT-DISTURB WINDOW
  - Setup page > Alarm: start/end time (minute precision), weekdays, per-event
    "still ring during do-not-disturb" switch. End earlier than start = overnight.
  - Defaults: failed / paused / error / offline still ring; only "finished" is muted.
    Untick that switch on an event to mute it during the window.
  - Needs the clock: time syncs over NTP after Wi-Fi connects (timezone is a POSIX
    string, default CST-8). Until synced, do-not-disturb is NOT applied (alarms ring).
