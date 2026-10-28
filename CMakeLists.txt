cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)          # copy from pico-sdk/external/
project(picodoom C CXX ASM)
pico_sdk_init()
find_package(PICOSYSTEM REQUIRED)       # cmake .. -DPICOSYSTEM_DIR=/path/to/picosystem

option(PD_SOUND "Enable sound effects" OFF)   # -DPD_SOUND=ON
option(PD_SAVE  "Save high scores to flash" ON) # -DPD_SAVE=OFF to disable

picosystem_executable(picodoom main.cpp)
pixel_double(picodoom)                  # 120x120 mode (what the game renders at)

if(PD_SOUND)
  target_compile_definitions(picodoom PRIVATE PD_SOUND=1)
endif()
if(PD_SAVE)
  target_compile_definitions(picodoom PRIVATE PD_SAVE=1)
  target_link_libraries(picodoom hardware_flash)
else()
  target_compile_definitions(picodoom PRIVATE PD_SAVE=0)
endif()
