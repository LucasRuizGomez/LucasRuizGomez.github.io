# Hollow Crypt — Project Context

Single-file, dependency-free top-down dungeon game in `rpg/index.html` (HTML5 Canvas, vanilla JS). Tests in `rpg/tests.html`.

## Goal of the game
Start at the Greenhollow camp with nothing. Gather wood, stone, herbs and ore, find weapon chests or craft your own gear, follow the road to the Hollow Crypt in the north-east mountains, find the key in the east wing, open the sealed door and defeat the Bone King.

---

## Rendering (why it looks sharp)
- The canvas backing store is `innerWidth × devicePixelRatio` (capped at 3), CSS size 100vw × 100vh.
- World is drawn with an **integer zoom** (`ZOOM`, ≥ 2) so 1 art pixel = N×N device pixels. View ≈ 22×13 tiles.
- `imageSmoothingEnabled = false`, all world draw positions rounded to whole art pixels, camera offset rounded to device pixels.
- All art is pixel art: characters/objects/icons are string sprites (`spr(rows, paletteOverrides)`), tiles are generated procedurally into 16×16 canvases (`buildTiles`). No `arc()` in sprites.
- Each map is **baked once** into an offscreen canvas (`bakeMap`), only the visible region is blitted each frame. Changed tiles are re-baked with `rebakeTiles` (e.g. the opened door).
- UI is drawn in a 1280×720 virtual space scaled by `UI`, so text is rendered at device resolution. Icons go through `drawSpr`, which scales by an integer factor.
- The crypt uses a light layer (`drawLighting`): darkness with radial cut-outs for the hero (bigger with a torch), wall torches, bone projectiles and the boss.

## Loop
Fixed 60 Hz `tick()` with an accumulator (same speed on 60/120/144 Hz screens), `render()` every animation frame.

## Modes
`title` · `play` · `pause` · `inv` · `craft` · `sign` · `dead` · `victory` (+ `G.fade` transitions).

---

## Maps
| Map | Size | Notes |
|---|---|---|
| `over` (Greenhollow) | 96×72 | Seeded generation (seed 1337, same every game): value-noise forest and lakes, river with bridges, rocky mountains (NE), sand shores, flowers. A* road from camp to the crypt. `ensureReach` carves access to every chest and ore rock. |
| `dungeon` (Hollow Crypt) | 64×48 | Hand-placed rooms in `ROOMS`: entry → hall → west / east wings → gate room → boss room. 3-tile corridors, pillars, wall torches. Door (row 11, cols 31–33) needs the Crypt Key. |

Tiles: `GRASS, WATER, TREE, PATH, SAND, ROCK, FLOWERS, BRIDGE, FLOOR, WALL, DOOR, STAIRS_DOWN, STAIRS_UP, PLANKS`. Walls and cliffs autotile (front face when the tile below is open).

Objects: `chest` (id, loot, opened), `node` (stone / iron / coal / herb, depletes and regrows), `anvil`, `campfire` (rest = heal + save), `sign`, `walltorch`. Trees are tiles; chopping uses a per-tree cooldown.

## Chests
| id | Where | Loot |
|---|---|---|
| c_camp | Camp | 2 bandages, 2 wood |
| c_forest | NW forest | Short Bow, 12 arrows |
| c_lake | South | Wooden Shield, 3 herbs |
| c_mountain | Mountains | Iron Sword, 2 iron ore |
| c_east | East | 2 potions, 5 gold |
| c_north | North | Pickaxe, 2 coal, torch |
| d_west | Crypt west wing | Knight's Blade, potion |
| d_east | Crypt east wing | Crypt Key, potion, 10 arrows |
| d_gate | Gate room | 3 bandages, torch |

Chests auto-loot everything on open. Weapons and shields auto-equip if they're a better tier.

## Gear
| Item | Stats | Source |
|---|---|---|
| Fists | 1 dmg | — |
| Wooden Sword | 2 dmg | Craft: 4 wood |
| Iron Sword | 4 dmg | Chest or craft at anvil: 2 iron ingots + 1 wood |
| Knight's Blade | 6 dmg | Crypt west wing chest |
| Wooden Shield | −1 on hits ≥ 2 | Chest or craft: 5 wood + rope |
| Iron Shield | −2 on hits ≥ 2 | Craft at anvil: 3 iron ingots |
| Short Bow | 3 dmg arrows (K) | Chest or craft: 3 wood + rope |

Other recipes: bandage (2 herbs), potion (2 herbs + gel), rope (2 herbs), torch (wood + coal), iron ingot (2 iron + coal, anvil), pickaxe (2 wood + 3 stone), arrows ×5 (wood + stone), bone arrows ×8 (2 bones + wood). Unique gear can't be crafted twice.

## Enemies
| Type | HP | Damage | Behaviour | Drops |
|---|---|---|---|---|
| Slime | 3 | ½ heart | Hops toward you | Gel, herb |
| Bat | 2 | ½ heart | Erratic flier, ignores water | Gold |
| Skeleton | 6 | 1 heart | Chases, slides along walls | Bone, gold |
| Bone King | 70 | 1½ hearts contact | Wakes when you enter the room. Cycles chase → slam (red circle telegraph, 2 hearts) or bone ring volley (1 heart each; double volley below half HP). Summons 3 bats at 50% and 25%. | Bone Crown, 25 gold, Heart Crystal (+1 heart) |

Health is in half-hearts (`P.hp`, max 10 → 12 with the Heart Crystal). Hits give 1 s of invulnerability and knockback. Melee can also destroy bone projectiles.

Overworld enemies respawn when you enter the overworld; crypt enemies respawn each time you enter the crypt. The boss stays dead.

## Controls
WASD / arrows move · J or Space attack · K bow · E interact (open, read, chop, mine, pick, rest, unlock) · Q quick heal · I bag · C crafting · M minimap · Esc pause (X sound, Backspace save & quit).

## Saving
`localStorage['hollowcrypt_save_v1']`: map, position, hp, max hp, inventory, equipment, chest states, door, boss, heart crystal, deaths, play time. Saved on map change, chest open, resting, door, boss defeat, death/respawn, and every 30 s.
Death: wake at camp with full health and all gear, minus 25% gold.

## Testing
- `rpg/tests.html` — unit tests of the pure logic (RNG, collision, shields, inventory, crafting chain).
- The game can be driven from an extra script tag: all game functions (`startGame`, `enterMap`, `tick`, `addItem`, …) and state (`G`, `P`) are globals, which makes headless smoke tests easy.
