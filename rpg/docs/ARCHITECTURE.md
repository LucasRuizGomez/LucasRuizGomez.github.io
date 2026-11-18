# Architecture

Hollow Crypt is split into two build targets with a hard line between them.

```
hc_game  (static library)          hollowcrypt  (executable)
  src/core   types, rng, files       src/engine  SDL2: drawing, fonts, audio, input, storage
  src/game   rules and world         src/view    sprites and world rendering
                                     src/app     loop, HUD, screens, menus
        ▲                                   │
        └────────── links against ──────────┘
```

`hc_game` knows nothing about SDL, the screen or the browser. It takes input as a plain struct, advances the world, and exposes its state for someone else to draw. That is why the tests can link against it alone and run in Node without a window.

## Data flow in one frame

`App::frame()` in `src/app/app.cpp` runs once per display refresh:

1. **Input.** `Input` collects SDL keyboard, mouse and gamepad events. The app turns them into actions (`attack`, `interact`, `bag`…) using the current key bindings.
2. **Simulate.** Time is banked in an accumulator and spent in fixed steps of 1/60 s, so the game runs at the same speed on any display. Each step calls `Sim::tick(SimInput)`. One-off actions go through `Sim::action(id)`.
3. **React.** The app plays the sound events the simulation queued (`Sim::sfx`), picks the music for the place and the level of danger, and writes a save when `Sim::wantSave` is set.
4. **Draw.** `WorldView` draws the map, then the app draws the HUD and whichever screen or menu is open.

The simulation never calls into the app. Everything it wants to tell the outside world is left in public fields that the app reads.

## The game library (`src/game`)

| File | Role |
|---|---|
| `defs.h/.cpp` | `Database`: the contents of `config/*.json` as structs. Loaded once, never changed |
| `world.h`, `map.cpp` | `Map` and what lives on it (objects, enemies, projectiles, drops), plus collision and pathfinding |
| `worldgen.cpp` | Builds the overworld and every dungeon from a seed |
| `sim.h` | `Sim`: the whole running game. The methods are implemented across the `sim_*.cpp` files below |
| `sim_core.cpp` | Lifecycle, the tick, camera, spawning (dungeons refill only after the hero has been outside), day and night, the skill tree |
| `sim_player.cpp` | Movement, attacks, rolling, Whirlwind, mounts (including flight) |
| `sim_enemies.cpp` | Enemy behaviour, bosses, projectiles |
| `sim_interact.cpp` | Chests, gathering, shrines, doors, traps |
| `inventory.cpp` | Bag, the six equipment slots, throwing things away, stats, crafting, shops |
| `loot.cpp` | Gear rarity and random bonuses |
| `quests.cpp` | Quest availability, progress and rewards |
| `save.cpp` | Saves as JSON, with a version number; older versions are refused rather than half-loaded |

**Determinism.** World generation only uses `Rng` (mulberry32) and the integer noise in `core/rng.cpp`, so a seed gives the same world on every platform. The seed is in `config/game.json`.

**Units.** Positions are in art pixels (one tile is 16), time is in ticks (60 per second), health and damage are in half hearts.

## Rendering (`src/view`, `src/engine`)

- `Gfx` wraps `SDL_Renderer` with a canvas-like transform (scale and offset), so the world is drawn in art pixels and the interface in a virtual 1280×720 space.
- The world uses a whole-number zoom so every art pixel covers the same number of screen pixels.
- Terrain is baked into 32×32-tile chunk textures and only re-baked where tiles change. Props and characters are sorted by their feet and drawn on top, so the hero can walk behind trees and houses.
- `Sprites` cuts every sprite from the art sheets at start-up using `config/sprites.json`. Tinted variants (a region's grass colour, recoloured monsters) are made the first time they are needed.
- Text is rasterised from the TrueType font with stb_truetype into one glyph atlas per size.
- Darkness at night and in dungeons is a separate layer with holes cut around light sources.

## Platform differences (`src/engine/platform.cpp`)

| | Browser | Desktop |
|---|---|---|
| Main loop | `requestAnimationFrame` through `emscripten_set_main_loop` | a plain loop |
| Saves and settings | `localStorage` | files in the user's data folder |
| Assets | packed into `index.data`; music streamed from `music/` on demand | read from `assets/` and `config/` in the source tree |
| Debug switches | URL query | command-line arguments |

## Adding content

- **An item, monster, recipe or quest:** add an entry to the matching JSON file. The test "config loads and cross-references resolve" catches ids that point at nothing.
- **A sprite:** add its rectangle to `config/sprites.json`. If it comes from a new image, list the image in `config/assets.json` too, or it will not be in the web build.
- **A town or dungeon:** add it to `config/world.json`. The reachability tests fail if the generator cannot connect it to the rest of the world.
- **A new rule:** put it in `src/game` and give it a test in `tests/test_game.cpp`.
