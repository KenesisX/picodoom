# PicoDoom ports
One engine (`../main.cpp`), one tiny shim per device. Controls everywhere: move/turn, A fire, B+turn strafe, X weapon, Y map.

| Device | Folder | Build |
|---|---|---|
| Pimoroni PicoSystem (RP2040) | `../` | GitHub Actions or `../build.sh` -> `picodoom.uf2` |
| M5Stack Cardputer / Cardputer ADV (ESP32-S3) | `cardputer/` | `pio run -d ports/cardputer -t upload` |
| ESP32 / ESP32-S3 + TFT_eSPI display + 8 buttons | `esp32/` | edit pins/driver in `platformio.ini` + `esp32_main.cpp`, then `pio run -d ports/esp32 -e esp32 -t upload` (or `-e esp32s3`) |
| Raspberry Pi 5 (native) | `pi/` | `sudo apt install build-essential libsdl2-dev && cd ports/pi && make && ./picodoom` |
| Raspberry Pi 5 (full web game) | `pi/` | `./kiosk.sh` (fullscreen Chromium, all features, 50+ soundtracks) |

Cardputer keys: `; . , /` = arrows (or W S A D), Space/F fire, B/Z strafe, X/Q weapon, M/Y map.
Pi keys: arrows/WASD, Space/Z fire, Shift/B strafe, X weapon, M/Y map, Esc quit.

Status: all ports compile and link in the host checks against stand-in headers; none has been flashed to real hardware.
The microcontroller builds run the C++ engine (5 levels, keys, 4 weapons, power-ups, crawlers, taunts). The full web game
(gore, sniper scope, 50+ soundtracks, new monsters, hazards) is browser-only; use `pi/kiosk.sh` on the Pi.
