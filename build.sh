#!/usr/bin/env bash
# Ubuntu / Debian / WSL.  Produces build/picodoom.uf2 (and build_sound/ if sound compiles).
set -e
sudo apt-get update && sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential git
cd "$(dirname "$0")"
[ -d ~/pico-sdk ] || git clone --depth 1 --recurse-submodules --shallow-submodules https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
[ -d ~/picosystem ] || git clone --depth 1 https://github.com/pimoroni/picosystem.git ~/picosystem
cp ~/pico-sdk/external/pico_sdk_import.cmake .
mkdir -p build && (cd build && cmake .. -DPICO_SDK_PATH=$HOME/pico-sdk -DPICOSYSTEM_DIR=$HOME/picosystem && make -j4)
echo "Safe build:  $(pwd)/build/picodoom.uf2"
mkdir -p build_sound && (cd build_sound && cmake .. -DPICO_SDK_PATH=$HOME/pico-sdk -DPICOSYSTEM_DIR=$HOME/picosystem -DPD_SOUND=ON && make -j4) \
  && echo "Sound build: $(pwd)/build_sound/picodoom.uf2" || echo "Sound build failed (API mismatch) - use the safe build."
echo "Flash: hold X while plugging in USB, drag the .uf2 onto RPI-RP2."
