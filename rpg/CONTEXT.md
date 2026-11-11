# Hollow Crypt — Project Context

Single-file, dependency-free top-down dungeon game in `rpg/index.html` (HTML5 Canvas, vanilla JS). Tests in `rpg/tests.html`.

## Goal of the game
Start at the Greenhollow camp with nothing. Gather wood, stone, herbs and ore, find weapon chests or craft your own gear, follow the road to the Hollow Crypt in the north-east mountains, find the key in the east wing, open the sealed gate and defeat the Hollow King.

---

## Rendering (why it looks sharp)
- The canvas backing store is `innerWidth × devicePixelRatio` (capped at 3), CSS size 100vw × 100vh.
- World is drawn with an **integer zoom** (`ZOOM`, ≥ 2) so 1 art pixel = N×N device pixels. View ≈ 22×13 tiles.
- `imageSmoothingEnabled = false`, all world draw positions rounded to whole art pixels, camera offset rounded to device pixels.
- All art comes from the craftpix packs in `rpg/assets/` (see **Assets** below). `loadAssets()` loads every PNG before the game starts; `buildSprites()` cuts frames/tiles/icons into canvases. Only water is still drawn in code (no pack has water).
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

The `Tiled/*.tmx` files in the fortress kit point at a different 25-column image, so tile ids there don't match `Tileset/Tiled_Map.png`; crops in `buildSprites` use pixel coordinates in `Tiled_Map.png` (18 × 16 px columns). Grass uses an 8×8-quarter autotile (`drawGrassTile`) from the grass patch (x160 y224) and the grass-with-hole block (x240 y240).

## Loop
Fixed 60 Hz `tick()` with an accumulator (same speed on 60/120/144 Hz screens), `render()` every animation frame.

## Modes
`title` · `play` · `pause` · `inv` · `craft` · `sign` · `dead` · `victory` (+ `G.fade` transitions).

---

## Maps
| Map | Size | Notes |
|---|---|---|
| `over` (Greenhollow) | 96×72 | Seeded generation (seed 1337, same every game): value-noise forest and lakes, river with bridges, rocky mountains (NE), sand shores, flowers. A* road from camp to the crypt. `ensureReach` carves access to every chest and ore rock. |
| `dungeon` (Hollow Crypt) | 64×48 | Hand-placed rooms in `ROOMS`: entry → hall → west / east wings → gate room → boss room. 3-tile corridors, pillars, wall torches. Gate (rows 11–13, cols 31–33) needs the Gate Key. |

Tiles: `GRASS, WATER, TREE, PATH, SAND, ROCK, FLOWERS, BRIDGE, FLOOR, WALL, DOOR, STAIRS_DOWN, STAIRS_UP, PLANKS`. Walls and cliffs autotile (front face when the tile below is open).

Objects: `chest` (id, loot, opened), `node` (stone / iron / coal / herb, depletes and regrows), `campfire` (rest = heal + save; also the cooking/forging station), `npc` (talk / search, shows text), `deco` (crate, barrels, log), `walltorch`, `gate` (crypt gate, 3×5 tiles over the DOOR cells), `arch` (crypt entrance). Trees are tiles; chopping uses a per-tree cooldown.

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

Chests auto-loot everything on open. Weapons and shields auto-equip if they're a better tier.

## Gear
| Item | Stats | Source |
|---|---|---|
| Fists | 1 dmg | — |
| Wooden Sword | 2 dmg | Craft: 4 wood |
| Iron Sword | 4 dmg | Chest or craft at the camp fire: 3 iron ore + 1 coal + 1 wood |
| Knight's Blade | 6 dmg | Crypt west wing chest |
| Wooden Shield | −1 on hits ≥ 2 | Chest or craft: 5 wood + 1 herb |
| Iron Shield | −2 on hits ≥ 2 | Craft at the camp fire: 4 iron ore + 1 coal |
| Crossbow | 3 dmg bolts (K) | Chest or craft: 3 wood + 2 herbs |

Other recipes: bandage (2 herbs, 1 heart), roast meat (1 raw meat + 1 herb at the fire, 3 hearts), torch (wood + coal), pickaxe (2 wood + 3 stone), bolts ×5 (wood + stone). Raw meat can be eaten for ½ heart. Unique gear can't be crafted twice.

## Enemies
| Type | Where | HP | Damage | Behaviour | Drops |
|---|---|---|---|---|---|
| Boar | Overworld | 4 | ½ heart | Charges in bursts | Meat |
| Fox | Overworld | 3 | ½ heart | Fast chaser | Meat |
| Hare / Deer | Overworld | 1 / 3 | — | Passive, flee | Meat |
| Black grouse | Mountains | 2 | ½ heart | Erratic flier, ignores water | Meat |
| Guard | Crypt | 6 | 1 heart | Chases, slides along walls | Gold, rare roast |
| Spearman | Crypt | 5 | 1 heart | Dashes at you | Gold |
| Crossbowman | Crypt | 4 | ½ heart bolts | Keeps distance and shoots | Bolts, gold |
| Hollow King | Crypt boss | 70 | 1½ hearts contact | Wakes when you enter the room. Chase → slam (red circle, 2 hearts) or spinning knife ring (1 heart each; double below half HP). Summons 2 spearmen at 50% and 25%. | Hollow Crown, 25 gold, Heart Crystal |

Killed enemies play their death animation (`m.corpses`) before disappearing.

Health is in half-hearts (`P.hp`, max 10 → 12 with the Heart Crystal). Hits give 1 s of invulnerability and knockback. Melee can also destroy knife projectiles.

Overworld enemies respawn when you enter the overworld; crypt enemies respawn each time you enter the crypt. The boss stays dead.

## Controls
WASD / arrows move · J or Space attack · K bow · E interact (open, read, chop, mine, pick, rest, unlock, talk) · Q quick heal · I bag · C crafting · M minimap · Esc pause (X sound, Backspace save & quit).

## Saving
`localStorage['hollowcrypt_save_v2']` (v2: new item ids; v1 saves are ignored): map, position, hp, max hp, inventory, equipment, chest states, door, boss, heart crystal, deaths, play time. Saved on map change, chest open, resting, door, boss defeat, death/respawn, and every 30 s.
Death: wake at camp with full health and all gear, minus 25% gold.

## Testing
- `rpg/tests.html` — unit tests of the pure logic (RNG, collision, shields, inventory, crafting chain).
- The game can be driven from an extra script tag: all game functions (`startGame`, `enterMap`, `tick`, `addItem`, …) and state (`G`, `P`) are globals, which makes headless smoke tests easy.
