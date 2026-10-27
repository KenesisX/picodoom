#!/usr/bin/env bash
# One-shot build for Ubuntu / Debian / WSL. Produces build/picodoom.uf2
set -e
sudo apt-get update && sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential git
cd "$(dirname "$0")"
[ -d ~/pico-sdk ] || git clone --depth 1 --recurse-submodules --shallow-submodules https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
[ -d ~/picosystem ] || git clone --depth 1 https://github.com/pimoroni/picosystem.git ~/picosystem
cp ~/pico-sdk/external/pico_sdk_import.cmake .
mkdir -p build && cd build
cmake .. -DPICO_SDK_PATH=$HOME/pico-sdk -DPICOSYSTEM_DIR=$HOME/picosystem
make -j4
echo "Done: $(pwd)/picodoom.uf2  (hold X while plugging in USB, drag it onto RPI-RP2)"
