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




---------------------------------------------------------------------------------------------------------------------
SETUP INSTRUCTIONS FOR THE PIMORONI PICO 2040 GAME SYSTEM


Copilot Search Branding

Like

Dislike
How to Program the Pimoroni PicoSystem (RP2040 Game Console)
You can program the PicoSystem using the official PicoSystem SDK in C++ or MicroPython, by setting up the Raspberry Pi Pico SDK, building your project, and flashing it via DFU mode 
wiki.picosystem.com
wiki.picosystem.com
+1
.

1. Understand the Hardware
The PicoSystem is a small handheld game console built around the RP2040 chip, with:

1.54" 240×240 colour IPS LCD

D‑pad + 4 action buttons

Piezo speaker

525 mAh LiPo battery

USB‑C for programming/recharging 
wiki.picosystem.com
wiki.picosystem.com

2. Install the Development Environment
Option A – C++ SDK 
wiki.picosystem.com
wiki.picosystem.com

On Linux (Raspberry Pi OS recommended), install build tools:

Bash

Copy
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
Clone the Raspberry Pi Pico SDK:

Bash

Copy
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
export PICO_SDK_PATH="$HOME/pico-sdk"
Clone the PicoSystem SDK:

Bash

Copy
git clone https://github.com/pimoroni/picosystem.git ~/picosystem
Create a build folder and compile examples:

Bash

Copy
mkdir ~/picosystem/build
cd ~/picosystem/build
cmake ..
make
This produces .uf2 files in ~/picosystem/build/examples.

Option B – MicroPython 
Github
Github

Download the PicoSystem MicroPython image from the PicoSystem GitHub and flash it via DFU mode.

Use the provided API for graphics, audio, and input.

3. Boot PicoSystem into DFU Mode 
wiki.picosystem.com
wiki.picosystem.com
Connect it to your PC via USB‑C.

Hold the X button, then toggle power.

The device will enter DFU mode and appear as a USB drive.

4. Build and Flash Your Game
In C++, create a project using the PicoSystem API (≈40 functions for hardware, graphics, audio, input) 
wiki.picosystem.com
wiki.picosystem.com
+1
.

The SDK’s game loop calls init(), update(), and draw() every ~10 ms 
smittytone messes with micros
smittytone messes with micros
.

After building, copy the .uf2 file from your build folder into the DFU drive.

The PicoSystem will boot your game.

5. Example Workflow
Write code in C++ or MicroPython using the SDK.

Build with cmake/make (C++) or micropython (MicroPython).

Flash via DFU mode.

Test and iterate.

6. Resources
PicoSystem Wiki: wiki.picosystem.com — hardware, API, tools 
wiki.picosystem.com
wiki.picosystem.com

GitHub SDK: pimoroni/picosystem — code, examples, MicroPython image 
Github
Github

Tutorial: smittytone blog — game loop example 
smittytone messes with micros
smittytone messes with micros

Tip: Start with the SDK’s example projects to learn the API, then adapt them to your own game.
