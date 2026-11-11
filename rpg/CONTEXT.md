# Hollow Crypt — Project Context

Single-file, dependency-free top-down dungeon game in `rpg/index.html` (HTML5 Canvas, vanilla JS). Tests in `rpg/tests.html`.

## Goal of the game
Start at the Greenhollow camp with nothing. Gather wood, stone, herbs and ore, find weapon chests or craft your own gear, follow the road to the Hollow Crypt in the north-east mountains, find the key in the east wing, open the sealed gate and defeat the Hollow King.

---

## Rendering (why it looks sharp)
- The canvas backing store is `innerWidth × devicePixelRatio` (capped at 3), CSS size 100vw × 100vh.
- World is drawn with an **integer zoom** (`ZOOM`, ≥ 2) so 1 art pixel = N×N device pixels. View ≈ 22×13 tiles.
- `imageSmoothingEnabled = false`, all world draw positions rounded to whole art pixels, camera offset rounded to device pixels.
- All art comes from the craftpix packs in `rpg/assets/` (see **Assets** below). Water tiles themselves are still drawn in code (no pack has water tiles); ripples, splashes and the whirlpool on top come from the water pack. `loadAssets()` loads every PNG before the game starts; `buildSprites()` cuts frames/tiles/icons into canvases. Only water is still drawn in code (no pack has water).
- Trees and mountain boulders are y-sorted props (`m.props`, built by `buildProps`) so you can walk behind them.
- Each map is **baked once** into an offscreen canvas (`bakeMap`), only the visible region is blitted each frame. Changed tiles are re-baked with `rebakeTiles` (e.g. the opened door).
- UI is drawn in a 1280×720 virtual space scaled by `UI`, so text is rendered at device resolution. Icons go through `drawSpr`, which scales by an integer factor.
- The crypt uses a light layer (`drawLighting`): darkness with radial cut-outs for the hero (bigger with a torch), wall torches, bone projectiles and the boss.

## Assets
| Pack | Used for |
|---|---|
| `craftpix-net-749505…medieval-fortress…` | Tileset (grass autotile + dirt, stone floor, brick walls, trees, flowers, crate/barrel/log, ladder bridge, crypt arch), Chest (opening anim), Meat (camp fire), Torch, Gate (opening anim), characters 1–9 (hero = 8, guard = 9, crossbowman = 3, spearman = 5, Hollow King = 4 at 2×, NPCs = 6/7, fallen knight = 1), Knife (boss volley), Charge_1 (bolts), UI.png (hearts, item glyphs, 9-slice panel frame) |
| `craftpix-net-789196…hunt-animals…` | Boar, fox, hare, deer, black grouse (4 directions: rows down/up/left/right) |
| `craftpix-net-974061…rocks-and-stones…` | Mountain boulders (Rock1/3/5), stone (Rock4), iron (Rock2), coal (Rock8), rocks in water (Rock7) |
| `craftpix-net-255216…basic-pixel-art-ui-for-rpg` | The whole interface: parchment panel (`Main_tiles.png` 208,196 48×40) as a 9-slice, bag slots (`Inventory.png` 177/193,113 14×14), green buttons (`Buttons.png` 51/147,97 42×15), the ✕ close button (`Inventory.png` 206,3). Item icons from `Icons.png`: key, pickaxe, crossbow, herb, gold coin, wood, iron sword, Knight's Blade, wooden sword (tinted), shields, Champion's Shield, Heart Crystal (tinted gem). |
| `craftpix-net-523429…water-effects…` | Source only — not loaded by the game. Its effects are smooth, high-resolution animations (up to 1800×1200 frames), so `rpg/tools/pixelize_water.py` turns three of them into tiny pixel-art strips in `rpg/assets/generated/`: `water_ripple` (Water3, 23×30 px frames), `water_splash` (Water4, 16×26 px) and `water_whirlpool` (Water1, 16×40 px). Re-run the script if you change sizes or pick other effects. |

The `Tiled/*.tmx` files in the fortress kit point at a different 25-column image, so tile ids there don't match `Tileset/Tiled_Map.png`; crops in `buildSprites` use pixel coordinates in `Tiled_Map.png` (18 × 16 px columns). Grass uses an 8×8-quarter autotile (`drawGrassTile`) from the grass patch (x160 y224) and the grass-with-hole block (x240 y240).

## Loop
Fixed 60 Hz `tick()` with an accumulator (same speed on 60/120/144 Hz screens), `render()` every animation frame.

## Modes
`title` · `play` · `pause` · `options` · `inv` · `craft` · `shop` · `sign` · `dead` · `victory` (+ `G.fade` transitions). `G.hitstop` freezes `tick()` for a few frames when a blow lands.

---

## Maps
| Map | Size | Notes |
|---|---|---|
| `over` (Greenhollow) | 96×72 | Seeded generation (seed 1337, same every game): value-noise forest and lakes, river with bridges, rocky mountains (NE), sand shores, flowers. A* road from camp to the crypt. `ensureReach` carves access to every chest and ore rock. |
| `dungeon` (Hollow Crypt) | 64×48 | Hand-placed rooms in `ROOMS`: entry → hall → west / east wings → gate room → boss room. 3-tile corridors, pillars, wall torches, 4 arrow traps, a mimic in the east wing. Gate (rows 11–13, cols 31–33) needs the Gate Key. Beating the boss opens `STAIRS_DOWN` at (32, 3). |
| `depths` (Deep Vaults) | 60×46 | `genDepths()`, seed 4242: 9 rooms chained from the lowest one by 3-wide L corridors, pillars in big rooms, torches, up to 5 traps over corridors. Stairs up in the first room (`m.start`), the Champion's Shield chest in the last one, plus `v_gold`, `v_supplies` and the `v_mimic` chest. Enemies are rolled per room on each visit; the last room has 5 guardians. |

Tiles: `GRASS, WATER, TREE, PATH, SAND, ROCK, FLOWERS, BRIDGE, FLOOR, WALL, DOOR, STAIRS_DOWN, STAIRS_UP, PLANKS`. Walls and cliffs autotile (front face when the tile below is open).

Objects: `chest` (id, loot, opened), `node` (stone / iron / coal / herb, depletes and regrows), `campfire` (rest = heal + save; also the cooking/forging station), `npc` (talk / search, shows text), `deco` (crate, barrels, log), `walltorch`, `gate` (crypt gate, 3×5 tiles over the DOOR cells), `arch` (crypt entrance), `trap` (arrow slit on a front wall: when you stand in its lane below it glows for 26 ticks, then fires a bolt straight down; 90-tick reload), chests with `mimic: true` (opening one spawns a `mimic` enemy instead). NPCs: Camp Warden (quest), Scout (quest), Travelling merchant (`shop: true`), fallen knight (`dead: true`). Trees are tiles drawn as props.

**Breaking things:** a tree takes 3 chops (it shakes on each hit, `m.treeHit`), then falls over (`m.falling`), leaving a `stump` object (walkable). After ~50 s the stump becomes a solid sapling, and ~40 s later the tree tile and prop come back. Rock/herb `node`s shrink one sprite stage per hit (`SPR.node[res]`: full → half → last bit) and disappear completely when empty, becoming walkable; they regrow later, but only once nobody is standing on the tile (`tileOccupied`). Map-edge trees can't be chopped. Stumps, saplings and mined nodes are saved with the time left until they regrow (`worldState` / `applyWorld`).

**Prop placement:** pines in the mountains and in cool noise patches, broadleaf elsewhere; deep forest uses only full-size trees, edges mix in smaller ones. Inside thick woods and rock fields only every other tile gets a sprite (staggered, slight jitter); the row below still covers the gaps. Boulders follow coherent veins: snow up north, sandstone patches in the grey rock. Nodes are never placed where a canopy or boulder would hide them.

## Chests
| id | Where | Loot |
|---|---|---|
| c_camp | Camp | 2 bandages, 2 wood |
| c_forest | NW forest | Crossbow, 12 bolts |
| c_lake | South | Wooden Shield, 3 herbs |
| c_mountain | Mountains | Iron Sword, 2 iron ore |
| c_east | East | 2 roast meat, 5 gold |
| c_north | North | Pickaxe, 2 coal, torch |
| d_west | Crypt west wing | Knight's Blade, roast meat |
| d_east | Crypt east wing | Gate Key, roast meat, 10 bolts |
| d_gate | Gate room | 3 bandages, torch |
| d_mimic | Crypt east wing | Mimic! (8 gold, maybe roast) |
| v_gold / v_supplies | Deep Vaults | 12 gold + 2 roast / 15 bolts + 3 bandages |
| v_mimic | Deep Vaults | Mimic! |
| v_champion | Deep Vaults, last room | Champion's Shield, 20 gold, 2 roast |

Chests auto-loot everything on open. Weapons and shields auto-equip if they're a better tier.

## Gear
| Item | Stats | Source |
|---|---|---|
| Fists | 1 dmg | — |
| Wooden Sword | 2 dmg | Craft: 4 wood |
| Iron Sword | 4 dmg | Chest or craft at the camp fire: 3 iron ore + 1 coal + 1 wood |
| Steel Spear | 5 dmg, long reach (21) | Craft at the camp fire: 4 iron ore + 2 coal + 2 wood |
| Knight's Blade | 6 dmg | Crypt west wing chest |
| Wooden Shield | −1 on hits ≥ 2 | Chest or craft: 5 wood + 1 herb |
| Iron Shield | −2 on hits ≥ 2 | Craft at the camp fire: 4 iron ore + 1 coal |
| Champion's Shield | −3 on hits ≥ 2 | Deep Vaults, last room |
| Crossbow | 3 dmg bolts (K) | Chest or craft: 3 wood + 2 herbs |

Other recipes: bandage (2 herbs, 1 heart), roast meat (1 raw meat + 1 herb at the fire, 3 hearts), torch (wood + coal), pickaxe (2 wood + 3 stone), bolts ×5 (wood + stone). Raw meat can be eaten for ½ heart. Unique gear can't be crafted twice.

## Enemies
| Type | Where | HP | Damage | Behaviour | Drops |
|---|---|---|---|---|---|
| Boar | Overworld | 4 | ½ heart | Paws the ground with a "!" for 30 ticks, then charges | Meat |
| Fox | Overworld | 3 | ½ heart | Fast chaser; faster and bolder at night, a pack of 4 appears at nightfall | Meat |
| Hare / Deer | Overworld | 1 / 3 | — | Passive, flee | Meat |
| Black grouse | Mountains | 2 | ½ heart | Erratic flier, ignores water | Meat |
| Guard | Crypt | 6 | 1 heart | Chases, slides along walls | Gold, rare roast |
| Spearman | Crypt | 5 | 1 heart | Crouches with a "!" for 24 ticks, then dashes | Gold |
| Crossbowman | Crypt | 4 | ½ heart bolts | Keeps distance; aims with a dotted red line for 40 ticks (it tracks you, then locks for the last 12, so you can sidestep), then fires | Bolts, gold |
| Mimic | Fake chests | 12 | 1 heart | Chomping charges (`CHARGE` table, like boar/spearman) | 8 gold, roast |
| Hollow King | Crypt boss | 70 | 1½ hearts contact | Wakes when you enter the room. Chase → slam (red circle, 2 hearts) or spinning knife ring (1 heart each; double below half HP). Summons 2 spearmen at 50% and 25%. | Hollow Crown, 25 gold, Heart Crystal |

Killed enemies play their death animation (`m.corpses`) before disappearing.

Health is in half-hearts (`P.hp`, max 10 → 12 with the Heart Crystal). Hits give 1 s of invulnerability and knockback. Melee can also destroy knife projectiles.

**Combat feel:** dodge roll (`P.roll`, 16 ticks, 40-tick cooldown) dashes the way you move or face and makes you untouchable for most of it (`invuln()`), leaving a white ghost trail. Melee hits freeze the world for 3 frames (5 on a kill, 4 when you get hit). Damage numbers pop up, fall back with a bounce and fade.

Overworld enemies respawn when you enter the overworld; crypt enemies respawn each time you enter the crypt. The boss stays dead.

## Controls
Three schemes, chosen automatically by what you last used (`G.inputMode`: moving the mouse in play or clicking → `mouse`, the attack/crossbow keys → `keys`, any gamepad input → `pad`). HUD keycaps (`btnLabel`), the hint bar, the pause screen and the cursor follow the active scheme.

| | Keyboard | Mouse + keyboard | Gamepad (standard mapping) |
|---|---|---|---|
| Move | WASD / arrows (fixed) | WASD / arrows | Left stick (analog) / d-pad |
| Attack | Space / J: turns toward an adjacent monster (`meleeTarget`) | Left click toward the cursor | X or RB (auto-turn like keyboard) |
| Crossbow | K / L: auto-aim at the monster marked with yellow corners (`bowTarget`) | Right click at the cursor, any angle | RT / LT; right stick aims freely, else auto-aim |
| Dodge roll | Shift | Shift | B |
| Use / talk | E / F | E / F | A |
| Heal | Q (smart), 1 bandage, 2 roast, 3 raw meat | same | Y |
| Bag / crafting / map | I or Tab / C / M | same | Back / LB / R3 |
| Pause | Esc / P | Esc / P | Start |

All keyboard actions except movement, Enter and Esc can be rebound in **Options** (pause → Options): `ACTIONS` lists them, `SET.binds` holds up to 2 keys each, `rebindTo` refuses movement/menu keys and won't steal the only key of another action. Options also has sound-effect and music volume and interface size (75–150%). Settings live in `localStorage['hollowcrypt_settings_v1']`.

Hold to keep attacking or firing; presses up to 10 ticks before the cooldown ends are buffered (`BUFFER`). E/F interact with whatever is right next to you, preferring what you face. Menus are fully clickable (`hit()` regions rebuilt every frame, `HITS`): bag slots (click selects, click again uses), crafting tabs and recipes, shop rows, pause buttons, option rows (◀ ▶). Right click = back, mouse wheel scrolls lists. On a gamepad, menus use the d-pad / stick, A confirm, B back, LB / RB tabs, X = craft ×5. Melee hitboxes are 4-way, so aimed swings snap to the nearest of the 4 directions; bolts use 8 sprite directions (`SPR.bolt8`).

## World systems
- **Day and night:** an 8-minute day (`DAY_LEN`) driven by play time; new games start at 8:00. Dusk 18–20, night 20–5, dawn 5–7 (`darkness()`), with a warm tint at dusk/dawn. At night the overworld goes dark except around you (bigger with a torch) and the camp fire. Resting at the fire at night sleeps until 7:00. The clock and day number are under the minimap.
- **Quests** (`G.quests`, shown under the minimap): the Camp Warden wants 5 raw meat (pays 2 roast meat + 10 gold); the Scout wants 4 black grouse shot down (pays 15 bolts + 8 gold).
- **Merchant** (next to the camp): buys bolts ×10 (6), bandage (3), roast meat (6), torch (5), iron ore (4), coal (3), pickaxe (10); buys meat, herbs, wood, stone (1) and iron, coal (2). Shift = ×5.
- **Minimap markers:** unopened chests (gold), people (blue), merchant (green), camp fire (orange); in dark maps only once explored.
- **Zoom:** whole-number zoom close to 22×13 tiles, never fewer than 15×9 tiles visible on small screens.

## Interface theme
Menus are parchment in a wooden frame (`panel()` = `nine(SPR.ui.panel, …)`, 2 UI px per art pixel) with dark ink text (`C` palette); things drawn straight over the world — toasts, the hint bar, the boss name, the area title, the title screen — use dark wood with light text (`LT` palette). Buttons and tabs are the pack's green buttons (`button()`, light green = selected); list rows use `row()`; bag and HUD slots use the pack's slot tiles (`slotBox()`).

## Water effects
`m.wfx` holds short ripple / splash animations on the overworld: every 9 ticks a ripple (15 % of the time a "fish" splash) may appear on open water in view, a bolt that runs out over water splashes where it lands, and a grouse shot down over water splashes and sinks (no corpse). One whirlpool loops in the widest lake (`m.whirl`, the lake by the crypt plaza); water rocks are not placed next to it.

## Audio
All sound is synthesized with Web Audio (`audio()`), through separate effect and music gains (`applyVolume`). Effects mix oscillators with filtered noise (`osc`, `noise`, `SFX` table) — hits, chops, mining clinks, whooshes, surface-aware footsteps (grass vs stone). Music is generative (`musicTick`, scheduled ahead on the audio clock): a chord pad, a bass line and a sparse melody from a scale, one mood per situation — day, night, crypt, boss (with drums). X mutes everything.

## Mob movement
Enemies' facing is derived from the distance they actually moved each tick (`faceMotion`), never from where they wanted to go, so a mob can't walk backwards or moonwalk against a wall. Animals keep their current 4-way facing while it is within ~50° of the motion (no flicker on diagonals). While knocked back a mob is stunned (no walking, no turning). Soldiers standing still turn on the spot to face you. Note: in the hunt pack, `Fox_Run.png` stores its right-facing row before the left-facing one (all other sheets are left, right); `buildSprites` swaps them back.

## Saving
`localStorage['hollowcrypt_save_v2']` (v2: new item ids; v1 saves are ignored): map, position, hp, max hp, inventory, equipment, chest states (incl. revealed mimics), door, boss, heart crystal, deaths, play time (also the time of day), quests, and `world` (stumps/saplings and mined nodes per map, with time left to regrow). Saved on map change, chest open, resting, door, boss defeat, death/respawn, and every 30 s.
Death: wake at camp with full health and all gear, minus 25% gold.

## Testing
- Sprites still wanted for later: real water tiles, a 4-direction hero attack, proper item icons (key, crown, pickaxe, crossbow…). The code is ready to swap them in.
- `rpg/tests.html` — unit tests of the pure logic (RNG, collision, shields, inventory, crafting chain).
- The game can be driven from an extra script tag: all game functions (`startGame`, `enterMap`, `tick`, `addItem`, …) and state (`G`, `P`) are globals, which makes headless smoke tests easy.
