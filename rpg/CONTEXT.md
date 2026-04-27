# RPG — Project Context

## What was built

A self-contained, browser-based top-down 2D RPG using vanilla HTML5 Canvas. No libraries or external dependencies. Single file: `index.html`.

---

## Architecture

Everything lives inside a single `<script>` block in `index.html`. No bundler, no modules, no external assets.

### Constants
- `TILE = 32` — tile size in pixels
- `COLS = 25`, `ROWS = 20` — map dimensions in tiles
- `SPEED = 2.5` — player movement speed in pixels per frame

---

## Tile System

Tiles are integers stored in a 2D array `MAP[row][col]`.

| Type  | Value | Walkable | Notes                        |
|-------|-------|----------|------------------------------|
| GRASS | 0     | yes      | Default open terrain         |
| WATER | 1     | no       | Animated wave effect         |
| TREE  | 2     | no       | Drawn as layered canopy      |
| PATH  | 3     | yes      | Dirt road                    |
| SAND  | 4     | yes      | Sandy area                   |
| WALL  | 5     | no       | Stone wall with mortar lines |

### Map Layout
25×20 tile map. Tree border surrounds the whole map. Features:
- Top-left: walled stone room with PATH doorway (row 6, col 5). **Chest inside (col 5, row 4).**
- Center: winding PATH road network connecting the map
- Top-right open grass. **Chest at col 21, row 2.**
- Right side: water lake (cols 19–20, rows 4–7)
- Bottom-left: sand patch (cols 4–7, rows 13–15)
- Bottom-right path trail. **Chest at col 20, row 17.**
- Mid-map open field. **Chest at col 12, row 8.**

---

## Player

Object: `{ x, y, w, h, dir, hp, maxHp, inventory }`. Position is world-space pixel center.
Starts at col 3, row 8 (grass tile).

Drawn programmatically: blue cloak, skin head, brown hair, directional eyes, side sword.
`dir` (`'up'|'down'|'left'|'right'`) drives which eye variant is drawn.

### Health System
- `player.hp = 10`, `player.maxHp = 10`
- Red HP bar HUD, always visible top-left
- Bar turns brighter red when HP ≤ 4
- Restored by using `potion` (+3 HP) or `bandage` (+2 HP) from inventory
- No damage sources yet — system is in place for enemies/traps

---

## Items

All items defined in `ITEMS` dict: `{ name, desc }`.
Icons all drawn programmatically in `drawItemIcon(id, x, y, size)`.

| ID         | Name           | Category    | Usable | Notes                              |
|------------|----------------|-------------|--------|------------------------------------|
| wood       | Wood           | Raw         | no     | Crafting material                  |
| stone      | Stone          | Raw         | no     | Crafting material                  |
| herb       | Herb           | Raw         | no     | Healing ingredient                 |
| coal       | Coal           | Raw         | no     | Smelting ingredient                |
| iron       | Iron Ore       | Raw         | no     | Smelting ingredient                |
| gold       | Gold           | Raw         | no     | Collectible, no shop yet           |
| iron_ingot | Iron Ingot     | Processed   | no     | Made from iron+coal, used in sword |
| rope       | Rope           | Processed   | no     | Made from herbs, used in equipment |
| torch      | Torch          | Processed   | no     | Made from wood+coal (×2 output)    |
| potion     | Health Potion  | Consumable  | yes    | Restores 3 HP                      |
| bandage    | Bandage        | Consumable  | yes    | Restores 2 HP                      |
| sword      | Iron Sword     | Equipment   | no     | Multi-step craft (needs iron_ingot)|
| shield     | Wooden Shield  | Equipment   | no     | Crafted                            |
| arrows     | Arrows         | Equipment   | no     | ×5 per craft                       |
| bow        | Short Bow      | Equipment   | no     | Crafted, needs rope                |

---

## Inventory System

- `player.inventory` — array of `{ id, qty }` (sparse, items stack by id)
- 20 display slots (5×4 grid)
- **Open/close**: `I`
- **Navigate**: Arrow keys (no row-wrap on left/right)
- **Use item**: `Enter` — potions and bandages restore HP
- Movement paused when open

---

## Chest System

4 chests on the map. Each chest: `{ col, row, opened, loot: [{id, qty}] }`.

`opened` becomes `true` once the player interacts. `loot` shrinks as items are taken.

### Chest UI (split-panel, opened with E)
Opens a full-screen overlay split into two sections:
- **Left panel (CHEST CONTENTS)**: 3×3 grid of chest loot items
- **Right panel (INVENTORY)**: 5×3 grid showing the player's first 15 inventory slots (read-only view)
- Both panels have a selection cursor; `Tab` switches active pane
- In chest pane: `Enter` takes selected item, `T` takes all
- `E` or `Escape` closes the UI
- Movement paused when open

### Chest visuals
- Not opened → closed chest with lock and metal bands
- Opened, has loot → open chest with gold glow
- Opened, empty → open chest, dark interior

### Chest locations and loot
| Col | Row | Loot                                    |
|-----|-----|-----------------------------------------|
| 5   | 4   | Wood ×2, Herb ×1, Coal ×1 (inside room)|
| 12  | 8   | Stone ×2, Gold ×1, Iron ×2             |
| 21  | 2   | Herb ×3, Gold ×2, Coal ×2              |
| 20  | 17  | Wood ×3, Stone ×2, Iron ×3             |

Chests always block player movement (checked in `walkable()`).

---

## Crafting System

`RECIPES` array: `{ name, category, inputs: [{id, qty}], output: {id, qty} }`.

### Crafting UI (opened with C)
Split two-panel overlay:
- **Left panel**: Category filter tabs + recipe list
  - 4 tabs: All / Healing / Materials / Combat — `Tab` key cycles
  - Recipe list filtered by active tab
  - Up/Down to navigate; selected recipe highlighted
- **Right panel**: Recipe detail for selected recipe
  - Large output item icon (60px)
  - Recipe name, output quantity, category badge
  - Item description
  - Ingredients list with live counts (have/need) in green/red + ✓/✗
  - Craft status line ("Ready to craft" / "Missing materials")
  - `Enter` — craft ×1; `Shift+Enter` — craft up to ×5
- Movement paused when open

### Recipes (9 total)

**Healing:**
| Name          | Ingredients  | Output      |
|---------------|--------------|-------------|
| Health Potion | Herb ×2      | Potion ×1   |
| Bandage       | Herb ×1      | Bandage ×1  |

**Materials:**
| Name       | Ingredients            | Output        |
|------------|------------------------|---------------|
| Rope       | Herb ×3                | Rope ×1       |
| Iron Ingot | Iron ×2 + Coal ×1      | Iron Ingot ×1 |
| Torch      | Wood ×1 + Coal ×1      | Torch ×2      |

**Combat:**
| Name          | Ingredients                 | Output     |
|---------------|-----------------------------|------------|
| Arrows        | Wood ×1 + Stone ×1          | Arrows ×5  |
| Short Bow     | Wood ×2 + Rope ×1           | Bow ×1     |
| Wooden Shield | Wood ×3 + Rope ×1           | Shield ×1  |
| Iron Sword    | Iron Ingot ×2 + Wood ×1     | Sword ×1   |

Iron Sword is multi-step: requires Iron Ingots which must be smelted first from Iron Ore + Coal.

---

## Input Map

| Key                | Context           | Action                             |
|--------------------|-------------------|------------------------------------|
| WASD / Arrow keys  | World             | Move player                        |
| E                  | World             | Open nearby chest                  |
| I                  | World             | Toggle inventory                   |
| C                  | World             | Toggle crafting                    |
| Arrow keys         | Inventory open    | Navigate slots                     |
| Enter              | Inventory open    | Use selected item                  |
| Arrow keys         | Chest open        | Navigate active pane               |
| Tab                | Chest open        | Switch between chest / inventory pane |
| Enter              | Chest pane        | Take selected item                 |
| T                  | Chest pane        | Take all items                     |
| E / Escape         | Chest open        | Close chest UI                     |
| Arrow keys ↑↓      | Crafting open     | Navigate recipe list               |
| Tab                | Crafting open     | Cycle category filter              |
| Enter              | Crafting open     | Craft selected recipe ×1           |
| Shift+Enter        | Crafting open     | Craft selected recipe ×5 (max possible) |

---

## Rendering Pipeline

`drawMap()` → `drawChests()` → `drawPlayer()` → `drawHUD()` → overlays (inventory / chest / crafting)

- Water tiles animate via `waterAnim = timestamp * 0.002`
- `drawItemIcon(id, x, y, size)` draws all item icons programmatically (ctx.save/restore)
- HUD always on top of world; overlays rendered last

### Canvas Scaling
CSS dimensions scaled to fit viewport on load (aspect-ratio preserved, max 2×). Internal resolution: 800×640.

---

## Known Limitations / Not Yet Built

- No camera — map must fit on screen (800×640)
- No spritesheet — all visuals are `fillRect`/`arc`
- No walk-cycle animation
- No enemies or damage sources — health system is passive
- Sword, shield, bow, arrows have no combat use yet
- Gold has no shop system
- No save state — resets on page reload
- Inventory capped at 20 displayed slots
- No NPC or dialogue system
- Map is hardcoded

---

## Intended Next Steps

- Camera that follows the player (offset all draw calls)
- Enemy entities with patrol/chase AI and melee attacks (triggers HP loss)
- Equipment slots: equip sword/shield/bow for stat bonuses
- Combat: sword swings, arrow firing, enemy HP bars
- Shop NPC: spend gold to buy items
- Save/load via `localStorage`
- Spritesheet rendering (PNG atlas, `drawImage`)
- Walk-cycle animation
- Larger map or multi-room transitions
