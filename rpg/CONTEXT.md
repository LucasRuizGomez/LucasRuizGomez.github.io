# Hollow Crypt — Project Context

Open-world top-down action RPG. C++20 on SDL2, compiled to WebAssembly with Emscripten (the same code also targets desktop).

- **How to build, test and run:** [`README.md`](README.md)
- **How the code is organised:** [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- **Game data and tuning:** `config/*.json` (each file starts with a comment describing its fields)
- **Asset authors and licences:** [`assets/CREDITS.md`](assets/CREDITS.md)

## State of the project

- Version 2.0.0 is a full rewrite of the earlier single-file JavaScript game. `index.html` and `tests.html` in this folder are that old version; they are only kept until the site is deployed through GitHub Actions and can then be deleted.
- The web build is the tested one. The desktop presets are set up but have not been built yet.
- Deployment: `.github/workflows/deploy.yml` builds, tests and publishes on every push to `main`. It needs Settings → Pages → Source set to "GitHub Actions".

## Things worth knowing before changing code

- `src/game` must stay free of SDL and drawing code: the tests link against it alone and run headless in Node.
- World generation must stay deterministic. Use `Rng` and the noise functions in `core/rng.h`, never `rand()` or floating-point tricks that differ between platforms.
- A new image has to be listed in `config/assets.json` or it will be missing from the web build.
- Saves live in `localStorage` under `hollowcrypt3_slot0..3`. Bump `SAVE_VERSION` in `src/game/save.cpp` when the format changes; old saves are then refused instead of half-loaded.
- Headless screenshots for checking the look of a change: serve a build and open it with the debug switches listed in the README (`?autostart&kit&mute&screen=inv`).
