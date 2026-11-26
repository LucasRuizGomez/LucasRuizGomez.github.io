# Website Context — LucasRuizGomez.github.io (lukeda.me)

## Overview
Professional portfolio of Lucas Ruiz Gómez, aimed at software recruiters, plus the free browser apps he built. Domain: lukeda.me. The portfolio pages are an Astro site in `site/`; every app is still a self-contained static folder.

---

## File Structure
```
site/                   ← Astro portfolio (/, /resume, 404) on the Halftone theme
  public/Lucas_Ruiz_Gomez_CV.pdf ← the CV; every CV button downloads it
  src/data/profile.ts   ← ALL portfolio copy: projects, competitions, interests (source of truth: the CV)
  src/components/Vessel.astro ← the ship LUKEDA in the hero; projects are its containers; each opens a <dialog>
  src/components/Sea.astro   ← animated swells along the foot of the cover plate
  src/pages/            ← index.astro, resume.astro (the CV as a page + PDF download), 404.astro
  src/styles/           ← tokens.css (theme defaults), theme.css (overrides), global.css
audiosend/
  index.html            ← AudioSend product page
  style.css
  main.js
  hero-illustration.html
positwebsite/
  index.html            ← "My Little Board" interactive sticky-note app
  tests.html            ← Test suite
filekit/
  index.html            ← Filekit PDF tools
  tests.html            ← Test suite
stockview/
  index.html            ← Portfolio tracker
  tests.html            ← Test suite
noteread/
  index.html            ← NoteRead music sight-reading trainer
  tests.html            ← Test suite
  context.md            ← NoteRead-specific context and spec
wordlist/
  index.html            ← Wortschatz flashcards (PWA, Supabase sync) — see wordlist/README.md
  tests.html            ← Test suite
context.md              ← This file
prompts/base_prompt.md
```

---

## Portfolio site (site/)

Astro 7 + Tailwind v4, built on the Halftone theme (ondelva, MIT; licence in `site/LICENSE`). The deploy workflow builds it and lays `site/dist` over the repo root, so `/`, `/resume/`, `/404.html` and the CV PDF come from Astro and the app folders are served as they are. The sub-apps' `../index.html` links still land on the home page.

### Commands (inside `site/`)
`npm install` · `npm run dev` (localhost:4321) · `npm run check && npm run check:contrast && npm run build`. CI runs all three.

### Design (from the theme; keep it)
- Cover plate: one photograph with a scrim, the name and the "now" cover lines set on it. Type on a plate stays inside 32rem.
- One spot colour (`--primary`, red), printed as a field: the name, kicker boxes, links on hover. No second accent.
- `--radius: 0`. Rules, not cards, shadows or gradients. Section bars are an uppercase head on a 2px rule with a mono folio.
- Fonts: Schibsted Grotesk (headings, UI), Spectral (body), IBM Plex Mono (kickers, folios, stack lines). Self-hosted at build time.
- Light and dark through `light-dark()`; the toggle in the header cycles System/Light/Dark.
- Hero: name and pitch on the left; on the right (below the text under 80rem) the feeder ship LUKEDA seen from starboard, moored in front of the cranes in the cover photo. Hull, bridge, funnel and mast are one SVG in a 1000x560 box; the containers are buttons laid over the deck in the same coordinates (3 bays x 3 tiers of 40ft, one decorative filler). Ship colours are fixed plate colours: the plate is dark in both modes.
- Containers carry no images: corrugation, rails, corner castings, ISO 6346 code `LRGU 00000N` with a computed check digit, owner logo. Click one: the side panel swings open (CSS 3D), the hold lights up, then a `<dialog>` "manifest card" opens with the screenshot, detail, stack and links. The Manifest section below repeats the same content as a plain list for scanning and no-JS.
- Motion: swells drift, the ship rolls, radar sweeps, mast light blinks, funnel smokes. All CSS; `prefers-reduced-motion` stops it (global rule in global.css).
- Project screenshots are imported from `/docs/*.png` (shared with the README) and served as AVIF. Portfolio's screenshot uses sample positions and mocked quotes.

### Copy
Written for recruiters: direct, brief, specific, first person. No buzzwords. The Capgemini client is never named. No email or phone on the pages: contact details live only in the CV PDF. Keep facts in step with the CV. Edit words in `src/data/profile.ts`, not in the pages.

---

## Projects

### AudioSend
- **Location**: `audiosend/index.html`. Every AudioSend link opens the GitHub repo: https://github.com/LucasRuizGomez/Audiosend
- **Status**: Live
- **Category**: audio · DAW plugin
- **Description**: Sends audio exports to your phone over local WiFi (QR code, plays in the phone's browser) with loudness analysis. Free and open source (AGPLv3). VST3 for Windows only.

### My Little Board (positwebsite)
- **Location**: `positwebsite/index.html`
- **Status**: Live
- **Category**: productivity · browser tool
- **Description**: Interactive sticky-note board. Click anywhere to place a note. Drag to move, scroll to rotate, add/check/edit tasks per note.
- **Aesthetic** (internal): Warm paper tones (`#faf5ef`), dot-grid background, `Caveat` handwriting font from Google Fonts. Pastel sticky-note colors. Completely different from main site — this is the app itself.

---

### Filekit
- **Location**: `filekit/index.html`
- **Status**: Live
- **Category**: file tools · browser utility
- **Description**: Browser-based file utilities. All processing runs client-side (no uploads). First tool: PDF merge — drop two PDFs, receive one merged file. More tools to come.
- **Aesthetic** (internal): Same design rules as the main site (tokens on `:root`, system sans, 4px spacing, inner radius = outer − padding). Warm-neutral surfaces (`#0c0a07` / `#16130e`); amber `--accent #f0b13c` reserved for the run button, active states and focus. Tools are a segmented tab control; each tool shows a status line, a collapsible Details log, spinner on the run button, shimmer skeletons for page thumbnails.
- **Library**: `pdf-lib@1.17.1` from unpkg CDN for client-side PDF manipulation.

---

### StockView
- **Location**: `stockview/index.html`
- **Status**: Live
- **Category**: finance · market data
- **Description**: Enter any NYSE/NASDAQ ticker symbol to see an interactive price chart (5D/1M/3M/6M/1Y/5Y) and a key statistics grid — market cap, P/E, volume, 52-week range, beta, dividend yield, and more. No account needed.
- **Aesthetic** (internal): Same design rules as the main site. Near-black neutrals (`#050505` / `#101010`), green `#4ade80` / red `#f87171` only for gains/losses, blue `--accent #7cc4ff` for the Add button and focus. Portfolio value is the enlarged stat. Skeleton cells while prices load, price-change flashes, row add/remove animation. Data comes from Finnhub (user's own free key, stored as `sv_key`).
- **Libraries**: `chart.js@4.4.0` from jsDelivr CDN for the price chart canvas. Data from Yahoo Finance v8 chart API and v10 quoteSummary API (client-side, CORS-enabled, ~15 min delayed).
- **Features**: Timeframe buttons (5D → 5Y), localStorage remembers last searched ticker, graceful error handling for bad symbols or API failures.

---

### NoteRead
- **Location**: `noteread/index.html`
- **Status**: Live
- **Category**: music · sight-reading trainer
- **Description**: Music note recognition speed trainer. A random note appears on a treble clef staff; the user clicks (or presses 1–7) the correct Spanish note name (Do, Re, Mi, Fa, Sol, La, Si) as fast as possible. Tracks correct answers, errors, current streak, and average response time. Press R to reset stats.
- **Note range**: C4 (Middle C, ledger line below staff) through G5 (space above staff) — 12 positions, 7px vertical step.
- **Aesthetic** (internal): Same design rules as the main site. Indigo-tinted neutrals (`#0b0a12` / `#14131e`), `--accent #8b93ff` for selected settings and the restart button. The staff and note buttons are the enlarged main feature; settings are collapsed behind "Ajustes". Feedback: staff outline flashes green/red (shake on wrong), streak number bumps, note slides in. UI copy is in Spanish.
- **Keyboard shortcuts**: 1=Do, 2=Re, 3=Mi, 4=Fa, 5=Sol, 6=La, 7=Si, R=Reset
- **Note rendering**: SVG whole note — outer white ellipse rotated -20°, inner black ellipse rotated -15° (open note head appearance). Ledger line drawn for Middle C.

---

### Wortschatz (wordlist)
- **Location**: `wordlist/index.html`; logic in `wordlist/srs.js` (shared with `wordlist/tests.html`), data layer in `wordlist/store.js`, backend in `wordlist/supabase/`. Spec in `wordlist/CONTEXT.md`, setup in `wordlist/README.md`.
- **Status**: Live; personal (single user, sign-in required). Card on the landing page; screenshot `docs/wordlist.png`
- **Category**: language learning · flashcards
- **Description**: Anki-style spaced repetition for C1 German vocabulary. Several decks, progress synced through Supabase, words added with Claude (edge function), installable PWA with offline answers.
- **Aesthetic** (internal): Its own light/dark palette (`#f6f5f2` / `#161618`), Inter for UI and Source Serif 4 for words, as in the original app.
- **Exception**: unlike the rest of the site it has an account (magic link) because the data syncs between devices.

---

### Hollow Crypt (rpg)
- **Location**: `rpg/index.html` (details in `rpg/CONTEXT.md`), tests in `rpg/tests.html`
- **Status**: Live
- **Category**: game · keyboard
- **Description**: Top-down pixel-art dungeon game. 96×72 overworld (Greenhollow) with resources, weapon chests and slimes; the Hollow Crypt dungeon with five rooms of enemies, a key, a sealed door and the Bone King boss. Weapons come from chests or are crafted (iron gear at the camp anvil).

---

## Adding a New Project — Checklist
1. Add the project to `projects` in `site/src/data/profile.ts` (it becomes a container)
2. Add a 1280×800 screenshot to `docs/` and import it there
3. **Create `appname/tests.html`** — MANDATORY (see Testing Standard below)
4. Update this `context.md` with the new app's section

---

## Testing Standard — MANDATORY FOR EVERY APP

Every application in this repo MUST have a `tests.html` file in its folder.

### Format
- Self-contained HTML file — no build tools, no external test framework
- Matches the aesthetic of the app it tests (same color palette, monospace font, dark background)
- Has a `← AppName` back link to `index.html`

### Structure
```
tests.html
├── Automated unit tests  (vanilla JS, run on page load, show PASS / FAIL badges)
└── Manual / visual checklist  (things that require human eyes or browser interaction)
```

### Test Runner Pattern (copy this boilerplate)
```javascript
const results = [];
let currentSuite = '';
function suite(n) { currentSuite = n; }
function test(name, fn) {
  try { fn(); results.push({ suite: currentSuite, name, pass: true }); }
  catch(e) { results.push({ suite: currentSuite, name, pass: false, detail: e.message }); }
}
function assert(c, m) { if (!c) throw new Error(m || 'assertion failed'); }
function assertEqual(a, b) { if (a !== b) throw new Error(`expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`); }
function assertClose(a, b, tol=0.001) { if (Math.abs(a-b)>tol) throw new Error(`expected ~${b}, got ${a}`); }
```

### What to test (unit tests)
- **Pure logic functions**: calculations, formatters, validators, data transforms
- **localStorage**: round-trip persistence, corrupt-JSON graceful fallback, key names
- **State machines**: any enable/disable/toggle logic (e.g. canMerge, canSubmit)
- **Data models**: shape validation, required fields
- **Keyboard maps / constants**: every defined mapping

### What to put in the manual checklist
- Visual/aesthetic correctness (colors, fonts, layout)
- Browser API interactions (drag, drop, audio, canvas, file download)
- CDN-dependent features (pdf-lib, chart.js, etc.)
- Responsive breakpoints
- Error states that require real API calls or file input

### Naming convention for logic mirrors
Since all app logic is inline in `index.html`, re-declare the pure functions inside `tests.html` so they can be tested without loading the full app. Label them with `// mirrors index.html`.

### Existing test files
| App | File | Suites |
|---|---|---|
| NoteRead | `noteread/tests.html` | Note Pool, Accidentals, Stats, Settings, Keyboard, Weakness Tracking, Answer Logic |
| Portfolio | `stockview/tests.html` | P&L Calculations, Portfolio Totals, Formatting, localStorage, Position Model |
| Filekit | `filekit/tests.html` | File Validation, Merge State Machine, Output Filename, UI Helpers |
| My Little Board | `positwebsite/tests.html` | Note Creation, Color, Rotation, Position Clamping, Tasks CRUD, Persistence |
| Wortschatz | `wordlist/tests.html` | Scheduler, Study queue, Extra practice, Lemmas, DB rows, Add words, Deck table, Seed import, Export round trip, Parity with reference |

---

## Notes
- CNAME file sets custom domain: `lukeda.me`
