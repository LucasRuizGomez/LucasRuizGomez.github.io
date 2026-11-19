# Hollow Crypt — asset credits

Everything in `assets/` was made by other people and is used under the licences below.
The game's own code is in `src/`.

## Music

| Files | Work | Author | Licence | Source |
|---|---|---|---|---|
| `audio/music/*.mp3` | *GT RPG Soundtrack* (20 of its 37 tracks, renamed by where they play) | Juhani Junkala | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) | https://opengameart.org/content/gt-rpg-soundtrack |

Track list (file → original title): `title` Anthem · `overworld` Hymn Overworld · `forest` Hymn Embarking · `coast` Hymn Waterfalls ·
`marsh` Marshlands · `mountains` Mist Covered Mountains · `night` Ambient Evil 1 · `town_a` Town Settlers Lament · `town_b` Town Frog Swim ·
`town_c` Town Cliffs of Dooneen · `town_harbor` Sumia Harbor · `dungeon_caves` Hymn Mysterious Caves · `dungeon_crypt` Sinister Theme ·
`dungeon_cellars` Flooded Cellars · `dungeon_deep` Ambient Evil 2 · `combat` Imminent Battle Loop · `combat_elite` Battle - Crystal World ·
`boss` Final Boss · `victory` Battle Victory · `death` Farewell Old Friend.

The music is unmodified apart from the file names. Attribution is required: it is shown in the game's Credits screen.

## Sound effects

| Files | Work | Author | Licence | Source |
|---|---|---|---|---|
| `audio/sfx/*.wav` | *RPG Sound Pack* | artisticdude | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://opengameart.org/content/rpg-sound-pack |
| `audio/sfx/*.ogg` | *80 CC0 RPG SFX* | rubberduck | CC0 1.0 | https://opengameart.org/content/80-cc0-rpg-sfx |

## Graphics

| Folder | Work | Author | Licence | Used for |
|---|---|---|---|---|
| `armm1998-zelda-like/` | *Zelda-like tilesets and sprites* | ArMM1998 | CC0 1.0 — https://opengameart.org/content/zelda-like-tilesets-and-sprites | Houses, stalls, fountains, towers, statues, signs, graves, water tiles, town paving, dungeon entrances, quest marks, pots, the stumpling monster, and (`Inner.png`) the floors, walls and furniture inside every building |
| `owlishmedia-rpg-icons/` | *RPG UI Icons* | OwlishMedia | CC0 1.0 — https://opengameart.org/content/rpg-ui-icons | Skill tree icons, building signs, conversation icons (packed into `generated/owl_icons.png` by `tools/pack_icons.py`) |
| `keiffer-skill-tree-icons/` | *Misc. RPG Icons* | Keiffer | CC0 1.0 — https://opengameart.org/node/151805 | The octagonal frame around skill tree icons |
| `craftpix-net-749505-…-medieval-fortress-…/` | *Free Medieval Fortress Top-Down Pixel Minigame Kit* | CraftPix.net | [CraftPix free licence](https://craftpix.net/file-licenses/) | Terrain tileset, trees, chests, torches, the gate, all human characters |
| `craftpix-net-789196-…-hunt-animals-…/` | *Free Top-Down Hunt Animals Pixel Sprite Pack* | CraftPix.net | CraftPix free licence | Boar, fox, hare, deer, black grouse (and the mounts) |
| `craftpix-net-974061-…-rocks-and-stones-…/` | *Free Rocks and Stones Top-Down Pixel Art* | CraftPix.net | CraftPix free licence | Boulders, ore and stone nodes |
| `craftpix-net-255216-…-basic-pixel-art-ui-for-rpg/` | *Free Basic Pixel Art UI for RPG* | CraftPix.net | CraftPix free licence | Panels, buttons, slots, item icons |
| `craftpix-net-523429-…-water-effects-…/` | *Free Water Effects Sprite Pack* | CraftPix.net | CraftPix free licence | Source for `generated/water_*.png` (see `tools/pixelize_water.py`) |
| `undead/` | *Free Undead Tileset Top Down Pixel Art* | CraftPix.net | CraftPix free licence | Big dead and dark trees, boulders, ruins, graves, bones, thorns (copied by `tools/import_dark_art.py`) |
| `generated/vampires/` | *Free Vampire 4-Direction Pixel Character Sprite Pack* | CraftPix.net | CraftPix free licence | The three vampires, cut to 32x32 frames by `tools/import_dark_art.py` |
| `dcss/` | *Dungeon Crawl Stone Soup* tiles | The Dungeon Crawl Stone Soup artists ([OpenGameArt](https://opengameart.org/content/dungeon-crawl-32x32-tiles)) | [CC0](https://creativecommons.org/publicdomain/zero/1.0/) | Rats, jackals, ravens, cultists, the risen dead, spiders, bats, wights, ghouls, wraiths and more |
| `lpc/` | *Universal LPC Spritesheet Character Generator* (Liberated Pixel Cup) | Many LPC artists: the full list of authors for every layer used is in [`lpc/CREDITS.csv`](lpc/CREDITS.csv) | CC-BY-SA 3.0 / GPL 3.0 / OGA-BY 3.0 (per file, see `lpc/CREDITS.csv`) | The knight: body, plate, mail and leather armour, helms, hood, cape, shields. Fetched and cut to 40x40 by `tools/import_lpc.py`; these files keep their share-alike licences |
| `generated/item_icons.png`, `generated/skill_icons.png` | *Dungeon Crawl Stone Soup* tiles | The Dungeon Crawl Stone Soup artists | CC0 | Item and skill icons, packed by `tools/add_gear.py` and `tools/pack_skill_icons.py` |

## Typeface

| Files | Work | Author | Licence |
|---|---|---|---|
| `fonts/Almendra-*.ttf` | *Almendra* (body text) | Ana Sanfelippo | [SIL Open Font License 1.1](fonts/OFL-Almendra.txt) |
| `fonts/PirataOne-Regular.ttf` | *Pirata One* (titles) | Rodrigo Fuenzalida, Nicolas Massi | [SIL Open Font License 1.1](fonts/OFL-PirataOne.txt) |
