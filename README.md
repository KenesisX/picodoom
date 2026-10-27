<div align="center">

# PICO DOOM

**A Doom-style first-person shooter for the Pimoroni PicoSystem (RP2040)**

![Platform](https://img.shields.io/badge/platform-PicoSystem%20%7C%20RP2040-red)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Levels](https://img.shields.io/badge/levels-5-orange)
![License](https://img.shields.io/badge/license-MIT-green)

<img src="https://github.com/KenesisX/picodoom/blob/main/banner.png" alt="PicoDoom screenshots" width="720">

*Textured raycasting engine, keycard doors, three weapons, five levels and a boss, running on a 133 MHz handheld with 264 KB of RAM.*

</div>

---

## Features

- **Software raycaster** written from scratch: textured walls, floors and ceilings, distance fog, muzzle-flash lighting, sprite scaling with depth sorting
- **5 levels:** Hangar 1, Reactor, Cryo Lab, Demon Warrens, Hell Keep
- **Keycards and locked doors:** red, blue and yellow, with sliding-door animation
- **3 weapons:** pistol, shotgun (7-pellet spread), rotary chaingun
- **4 enemy types + boss:** imps (fireballs), demons (fast melee), floaters (heavy fireballs, hovering), and the Baron (triple volleys)
- **Pickups:** health, ammo, armor, weapons, keycards
- **Pixel-art HUD** with a face that bleeds as you take damage, plus a minimap
- **Animated Doom-fire title screen**
- **Scoring and persistence:** par times, kill bonuses, best time per level and a high score saved to flash
- **Optional sound effects**
- **Procedural levels with a solvability checker** (`gen_levels.py` verifies every key and exit is reachable)

## Screenshots

| Title | Chaingun vs floater | Armor and imp |
|:--:|:--:|:--:|
| <img src="docs/title.png" width="240"> | <img src="docs/chaingun-floater.png" width="240"> | <img src="docs/armor-imp.png" width="240"> |
| **Floaters** | **Level clear** | **Victory** |
| <img src="docs/floaters.png" width="240"> | <img src="docs/level-clear.png" width="240"> | <img src="docs/victory.png" width="240"> |

## Controls

| Button | Action |
|---|---|
| D-pad Up / Down | Walk forward / back (hold Up to build speed) |
| D-pad Left / Right | Turn |
| **B** + Left / Right | Strafe |
| **A** | Fire |
| **X** | Cycle weapons |
| **Y** | Toggle minimap |

## Install (just want to play)

1. Download `picodoom.uf2` from the [Releases](../../releases) page, or from the latest **Actions** run (see below).
2. Hold **X** on the PicoSystem while plugging it into USB. An `RPI-RP2` drive appears.
3. Drag the `.uf2` file onto that drive. The game starts automatically.

### Which file?

| File | Contents |
|---|---|
| `picodoom.uf2` | Recommended: full game with saved scores, no sound |
| `picodoom-sound.uf2` | Same plus sound effects (built only if it compiles against the SDK) |
| `picodoom-minimal.uf2` | No sound, no flash saving (fallback) |

## Build it yourself

### Option 1: GitHub Actions (nothing to install)

Fork or push this repo, open the **Actions** tab, run **Build PicoDoom UF2**, and download the `picodoom-uf2` artifact.

### Option 2: Local build (Ubuntu / Debian / WSL)

```bash
git clone https://github.com/<your-username>/picodoom.git
cd picodoom
./build.sh
```

The script installs the ARM toolchain, fetches the [Pico SDK](https://github.com/raspberrypi/pico-sdk) and [PicoSystem SDK](https://github.com/pimoroni/picosystem), and produces `build/picodoom.uf2`.

### Option 3: Manual CMake

```bash
cp $PICO_SDK_PATH/external/pico_sdk_import.cmake .
mkdir build && cd build
cmake .. -DPICOSYSTEM_DIR=/path/to/picosystem          # add -DPD_SOUND=ON for sound
make -j4
```

| CMake option | Default | Effect |
|---|---|---|
| `PD_SOUND` | OFF | Enable sound effects |
| `PD_SAVE` | ON | Save best times and high score to the last flash sector |

## How it works

- **Rendering:** 120x120 internal framebuffer in 12-bit colour (4 bits per channel). Each frame draws floor and ceiling by row casting with fixed-point texture stepping, then walls by DDA raycasting, then depth-sorted billboard sprites, then the weapon and HUD. The finished frame is blitted through the PicoSystem API using horizontal run-length batching.
- **Textures:** all 32x32 wall, floor and ceiling textures and all sprites are generated procedurally or defined as tiny character-art in the source, so there are no asset files.
- **Enemies:** line-of-sight checks, wake-on-sight alerting, per-type AI (ranged, melee, volley), projectile entities.
- **Levels:** `gen_levels.py` carves rooms and corridors, places doors, keys, enemies and items, then flood-fills with key-gated passage to prove each level is completable. It writes `levels.h`.
- **Saves:** a small checksummed record in the final flash sector, written only when you set a new best.

## Making your own levels

Edit the room tables at the top of `gen_levels.py`, then run:

```bash
python3 gen_levels.py
```

Link types: `0` open corridor, `1` normal door, `'R'` / `'U'` / `'Y'` red / blue / yellow locked door.
Entities: `i` imp, `d` demon, `f` floater, `B` baron. Items: `h` health, `a` ammo, `s` shotgun, `g` chaingun, `v` armor, `r` / `u` / `y` keys.

## Project layout

```
main.cpp                    game engine
levels.h                    generated level data
gen_levels.py               level generator and solvability checker
CMakeLists.txt              PicoSystem build
build.sh                    one-shot local build (Linux / WSL)
.github/workflows/build.yml cloud build that outputs the .uf2 files
docs/                       screenshots
```

## Roadmap

- [ ] Difficulty settings
- [ ] Pause menu with brightness and sound options
- [ ] Horde / survival mode
- [ ] Rocket launcher with splash damage
- [ ] Secret doors and secret counters
- [ ] Textured skies and animated textures
- [ ] Link-cable deathmatch

## Credits

Created by **Kenesis_X**.
Built on the [Pico SDK](https://github.com/raspberrypi/pico-sdk) and [PicoSystem SDK](https://github.com/pimoroni/picosystem).

PicoDoom is an original work in the style of classic 90s corridor shooters. It contains no code or assets from id Software, and is not affiliated with or endorsed by id Software, Bethesda, Pimoroni or Raspberry Pi. "Doom" is a trademark of its respective owner.

## License

MIT. See [LICENSE](LICENSE).
