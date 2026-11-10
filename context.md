# Website Context — LucasRuizGomez.github.io (lukeda.me)

## Overview
Personal portfolio / free projects site by Lucas Ruiz. Brand name: **Lukeda**. Domain: lukeda.me. All projects are released for free — no paywalls, no accounts.

---

## File Structure
```
index.html              ← Main landing / portfolio page
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
context.md              ← This file
prompts/base_prompt.md
```

---

## Visual Aesthetic — Main Site (index.html)

Clean dark-teal product page. Sub-apps keep their own retro themes; the landing page does not.

### Design tokens (`:root` in index.html — use these, don't hard-code values)
- Surfaces: `--bg #031a22`, `--bg-deep`, `--surface #072631`, `--surface-hi`, `--border`, `--border-hi`
- Text: `--text #e8f6f3`, `--text-muted #a9cec7`, `--text-faint #84aca5` (all ≥ 4.5:1)
- Brand teal `--teal #4fd8c4`; accent amber `--accent #f6b94a` — accent is reserved for the primary CTA, focus rings and hover feedback
- Type: system sans (`--font-sans`) everywhere; `--font-mono` only for literal code/domains. Scale `--fs-sm` … `--fs-3xl`
- Spacing: 4px scale `--s-1` … `--s-9`; sections are grouped by spacing, not borders
- Radius: `--r-media = --r-card − --pad-media` (inner radius = outer − padding)

### Rules
- No filler badges, `++`/`//`/`>>` prefixes, tickers, LEDs or decorative glows
- Headlines say what the thing does; use `text-wrap: balance` on headings
- One icon set: inline SVG `<symbol>`s in the page, 16px, 1.75 stroke, round caps
- Numbers use `.num` (tabular-nums)
- Screenshots (`docs/*.png`, 1280×800) show each product; `.media` shows a shimmer until the image loads
- Motion only for feedback (hover/press/load); respects `prefers-reduced-motion`

### Layout
1. Header — wordmark + Projects / GitHub
2. Hero — headline + one-paragraph summary
3. Featured card — AudioSend, large, with screenshot and the amber CTA
4. `#projects` grid — browser apps as `.card` (screenshot, name + arrow, description, category)
5. Footer

### Project Card Structure
```html
<a href="app/index.html" class="card">
  <div class="media"><div class="media-frame">
    <img src="docs/app.png" width="1280" height="800" alt="..." loading="lazy">
  </div></div>
  <div class="card-body">
    <h3>Name <svg class="icon" aria-hidden="true"><use href="#i-arrow-right"/></svg></h3>
    <p>What it does, in one or two sentences.</p>
    <span class="meta">Category</span>
  </div>
</a>
```

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

### Hollow Crypt (rpg)
- **Location**: `rpg/index.html` (details in `rpg/CONTEXT.md`), tests in `rpg/tests.html`
- **Status**: Live
- **Category**: game · keyboard
- **Description**: Top-down pixel-art dungeon game. 96×72 overworld (Greenhollow) with resources, weapon chests and slimes; the Hollow Crypt dungeon with five rooms of enemies, a key, a sealed door and the Bone King boss. Weapons come from chests or are crafted (iron gear at the camp anvil).

---

## Writing / Voice Style
- Lowercase descriptive lines under brand (`// projects by lucas ruiz`)
- Section labels prefixed with `++` in monospace
- Nav items uppercase
- Descriptions written in plain, direct first person: "I build and release here", "started as something I needed"
- `·` and `····` as separators in ticker
- `>>` as link prefix in cards

---

## Adding a New Project — Checklist
1. Add a `.card` to the `.grid` inside `#projects` (see structure above)
2. Add a 1280×800 screenshot to `docs/`
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

---

## Notes
- `background-attachment: fixed` on desktop, `scroll` on mobile (perf)
- All CSS is inline in `<style>` inside `index.html` — no external stylesheet for main page
- CNAME file sets custom domain: `lukeda.me`
