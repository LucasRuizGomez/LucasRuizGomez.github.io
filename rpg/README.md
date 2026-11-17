# Hollow Crypt

A top-down open-world action RPG that runs in the browser: six regions, six towns, seven dungeons, 21 quests, crafting, random gear, mounts and a day and night cycle. Written in C++20 on SDL2 and compiled to WebAssembly with Emscripten. The same code also builds as a desktop program.

Play it at [lukeda.me/rpg](https://lukeda.me/rpg/).

## Layout

| Folder | What is in it |
|---|---|
| `src/core/` | Small shared pieces: types, seeded random numbers and noise, file and JSON helpers |
| `src/game/` | The game itself: data definitions, world generation, simulation, inventory, quests, saves. No SDL and no drawing, so it runs headless in the tests |
| `src/engine/` | The platform layer on SDL2: drawing, fonts, audio, input, storage |
| `src/view/` | Sprite cutting and world rendering |
| `src/app/` | Main loop, HUD, in-game screens, title menu and pause menu |
| `config/` | All game data and tuning as JSON (see below) |
| `assets/` | Art, sound and fonts. Authors and licences are in [`assets/CREDITS.md`](assets/CREDITS.md) |
| `tests/` | Unit tests of the game library (doctest) |
| `web/` | The HTML page the game is embedded in |
| `cmake/`, `scripts/` | Build modules and helper scripts |
| `docs/` | [`ARCHITECTURE.md`](docs/ARCHITECTURE.md) explains how the code fits together |

`index.html` and `tests.html` in this folder are the previous JavaScript version of the game. They stay only until the site is deployed by GitHub Actions, and can be deleted after that.

## Config files

The game reads everything below at start-up. Changing a value and rebuilding is enough; no C++ needs to change. The files allow `//` comments.

| File | Defines |
|---|---|
| `game.json` | Global tuning: player speed and timings, levelling, day length, spawning, save slots |
| `world.json` | Regions, towns and their people, dungeons, shops, points of interest, world size and seed |
| `items.json` | Items, rarities and the random bonuses gear can roll |
| `enemies.json` | Monsters, bosses and elite variants |
| `quests.json` | Quests, their objectives, rewards and the order they unlock in |
| `recipes.json` | Crafting recipes |
| `controls.json` | Default key and gamepad bindings, default volumes |
| `audio.json` | Which sound files play for each event, and the music for each place |
| `sprites.json` | Where each sprite sits in the art sheets |
| `assets.json` | Which asset files are packed into the web build |

## Building

You need [CMake](https://cmake.org) 3.24 or newer, [Ninja](https://ninja-build.org), Python 3 and the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) 7.0.0. `scripts/env.sh` looks for the SDK in `~/emsdk`; set `EMSDK` if yours is somewhere else.

```bash
scripts/build.sh web-debug      # configure, build and run the tests
scripts/serve.sh web-debug      # then open http://127.0.0.1:8377/
scripts/build.sh web-release    # optimised build, the one that is deployed
```

On Windows run these from Git Bash. The build output lands in `build/<preset>/dist/`.

The presets in `CMakePresets.json` can also be used directly, which is what editors such as VS Code and Visual Studio do:

```bash
source scripts/env.sh
cmake --preset web-debug
cmake --build --preset web-debug
ctest --preset web-debug
```

| Preset | Result |
|---|---|
| `web-debug` | WebAssembly with assertions, fast to rebuild |
| `web-release` | WebAssembly, optimised (about 2 MB of code plus a 10 MB asset pack; music is fetched as it is needed) |
| `native-debug`, `native-release` | Desktop program. SDL2 and SDL2_mixer are downloaded and compiled on the first configure. This path has not been tested yet |

The header-only libraries (nlohmann/json, stb, doctest) are downloaded once into `build/_deps/`, pinned by version and checksum in `cmake/Dependencies.cmake`.

## Tests

`ctest --preset web-debug` runs the unit tests under Node. They cover the random number generator, config loading, collision, pathfinding, world and dungeon generation (everything must be reachable), inventory, crafting, gear rolls, combat, levelling, quests, the day cycle, save and load, and a bot that mashes the controls in every map.

## Debug switches

Add these to the URL of a web build (`?autostart&kit&tp=128,40`), or pass them as `--name=value` to the desktop build. A game started this way never touches the real save slots.

| Switch | Effect |
|---|---|
| `autostart` | Skip the title menu and start a new game |
| `kit` | Start with gear, gold, potions and a few levels |
| `tp=column,row` | Start at a tile of the current map |
| `map=id` | Start inside a dungeon (ids are in `config/world.json`) |
| `hour=22` | Set the time of day |
| `reveal` | Uncover the whole world map and unlock every waypoint |
| `screen=name` | Open a screen straight away: `inv`, `craft`, `quests`, `map`, `pause`, `controls`, or on the title menu `load`, `new`, `settings` |
| `mute` | No sound |

## Deployment

`.github/workflows/deploy.yml` (in the repository root) builds the release preset on every push to `main`, runs the tests, puts the result in `rpg/` of the site and publishes the site to GitHub Pages. Pull requests are built and tested but not published.

For this to work the repository's **Settings → Pages → Source** must be set to **GitHub Actions**.
