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
  server.js
  download.html
  success.html
  resend-key.html
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

### Theme
Dark teal terminal / retro OS interface. Looks like a CRT monitor or old-school software UI. Heavy use of monospace fonts for labels, uppercase text, and `++` prefixes for section headers.

### Color Palette
| Role | Value |
|---|---|
| Body background | `#005060` → gradient ending `#001c3a` |
| Background gradient | Multi-layer teal wave with sparkle dots, light rays, ribbon waves |
| Primary text | `#9ad8d0` |
| Muted text | `#3a9888`, `#2a7868`, `#1a7868` |
| Dim/label text | `#0a6858`, `#0a5848`, `#0a5a50` |
| Accent / bright | `#44ddc8`, `#33ccbb`, `#22bb99`, `#55ccbb` |
| Brand name | `#c0f0ea` |
| Project card titles | `#88d8cc` |
| Panel borders | `rgba(0,180,165,0.20–0.22)` |
| Panel backgrounds | `rgba(0,18,28,0.88)` |
| Card backgrounds | `rgba(0,20,32,0.82)` |
| LIVE badge | `#33ccaa` on `rgba(0,120,90,0.18)` |
| SOON badge | `#1a7868` on `rgba(0,40,50,0.4)` |

### Typography
- **Body / UI**: Verdana, Tahoma, Arial, sans-serif — 11px base size
- **Monospace labels / code**: `'Courier New', monospace` — used for nav prefix, panel heads, section labels, ticker, footer
- **Brand / headings**: `'Trebuchet MS', Impact, Arial, sans-serif` — used for `.brand` and `.pc-name`
- Font sizes: 8–9px for labels, 10px for descriptions, 11px body, 13px card titles, `clamp(28px, 6vw, 44px)` for brand

### Layout Structure (top → bottom, all inside `.page` max-width 760px)
1. **`.sysbar`** — top status bar: site name left, domain + LED online badge right
2. **`.header-panel`** — brand name, version line (`// projects by lucas ruiz`), tagline
3. **`.navbar`** — nav links with `++ nav ::` prefix label
4. **`.panel#about`** — about text + quote + free-badge
5. **`.panel#projects`** — project grid
6. **`.ticker-bar`** — scrolling marquee text
7. **`.footbar`** — LED + site name left, copyright right

### Key CSS Patterns
- All panels share: `border: 1px solid rgba(0,180,165,0.20)`, `border-top: none`, `margin-top: -1px` (seamless stacking)
- `backdrop-filter: blur(6–8px)` on panels
- `.panel-head` uses `::before { content: '++ '; }` prefix
- Top edge shimmer: `::before` with `linear-gradient(90deg, transparent, rgba(0,220,195,0.5), transparent)` 1px line
- Card top shimmer: same pattern on `.project-card::before`
- LED pulse animation: `ledPulse` keyframes on 5×5 circle with teal glow
- Ticker: `tickerScroll` from `translateX(100vw)` to `translateX(-100%)` over 40s

### Project Card Structure
```html
<a href="..." class="project-card">
  <div class="pc-row">
    <span class="pc-name">PROJECT NAME</span>
    <span class="pc-tag tag-live">[ Live ]</span>   <!-- or tag-soon -->
  </div>
  <div class="pc-cat">// category · subcategory</div>
  <div class="pc-desc">Description text.</div>
  <div class="pc-link">&gt;&gt; path/to/project</div>
</a>
```
- `tag-live`: green badge — `[ Live ]`
- `tag-soon`: dim badge — `[ TBA ]`
- `.soon` class on card: `opacity: 0.3; pointer-events: none`

### Nav Links
- Pattern: `<a href="#section" class="nav-lnk active">Label</a>`
- Active state: bottom 2px gradient line + slightly brighter color

### Responsive
- Breakpoint: `max-width: 560px` — reduced padding, smaller brand text, smaller nav

---

## Projects

### AudioSend
- **Location**: `audiosend/index.html`
- **Status**: Live
- **Category**: audio · DAW utility
- **Description**: Sends DAW mix to phone over local WiFi. VST3 + AU for Windows & macOS. No cables, no bounce files.

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
- **Aesthetic** (internal): Amber phosphor terminal. Same structural DNA as main site (sysbar, header-panel, navbar, panels, footer) but with a warm amber/gold color palette — dark brown-black background (`#0a0702`), amber accents (`#c89030`, `#d4a038`, `#e8c870`), scanline overlay. Feels like a vintage Hercules amber-monitor terminal. Distinct from the teal of the main site.
- **Library**: `pdf-lib@1.17.1` from unpkg CDN for client-side PDF manipulation.

---

### StockView
- **Location**: `stockview/index.html`
- **Status**: Live
- **Category**: finance · market data
- **Description**: Enter any NYSE/NASDAQ ticker symbol to see an interactive price chart (5D/1M/3M/6M/1Y/5Y) and a key statistics grid — market cap, P/E, volume, 52-week range, beta, dividend yield, and more. No account needed.
- **Aesthetic** (internal): Pure black terminal. Maximum contrast — near-black background (`#050505`), pure white text (`#ffffff`), white chart line, scanline overlay. Green (`#4ade80`) for positive price change, red (`#f87171`) for negative. Same structural DNA as main site (sysbar, header-panel, navbar, panels, ticker, footer) but with a monochrome white/gray palette. Feels like a Bloomberg Terminal.
- **Libraries**: `chart.js@4.4.0` from jsDelivr CDN for the price chart canvas. Data from Yahoo Finance v8 chart API and v10 quoteSummary API (client-side, CORS-enabled, ~15 min delayed).
- **Features**: Timeframe buttons (5D → 5Y), localStorage remembers last searched ticker, graceful error handling for bad symbols or API failures.

---

### NoteRead
- **Location**: `noteread/index.html`
- **Status**: Live
- **Category**: music · sight-reading trainer
- **Description**: Music note recognition speed trainer. A random note appears on a treble clef staff; the user clicks (or presses 1–7) the correct Spanish note name (Do, Re, Mi, Fa, Sol, La, Si) as fast as possible. Tracks correct answers, errors, current streak, and average response time. Press R to reset stats.
- **Note range**: C4 (Middle C, ledger line below staff) through G5 (space above staff) — 12 positions, 7px vertical step.
- **Aesthetic** (internal): Dark indigo/purple theme. Background `#0f0e17`, accent `#818cf8` (indigo), secondary `#c4b5fd` (violet). Same structural DNA as main site but with a purple palette. Completely distinct from all other apps.
- **Keyboard shortcuts**: 1=Do, 2=Re, 3=Mi, 4=Fa, 5=Sol, 6=La, 7=Si, R=Reset
- **Note rendering**: SVG whole note — outer white ellipse rotated -20°, inner black ellipse rotated -15° (open note head appearance). Ledger line drawn for Middle C.

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
1. Add a new `<a>` (if live) or `<div>` (if TBA) in the `.project-grid` inside `#projects`
2. Use `.pc-tag.tag-live` or `.pc-tag.tag-soon`
3. Nav bar: add `<a href="path" class="nav-lnk">Label</a>` if it warrants top-level navigation
4. Ticker: add a description blurb to `.ticker-inner`
5. If replacing the "Next Project" placeholder card, remove the `.soon` div
6. **Create `appname/tests.html`** — MANDATORY (see Testing Standard below)
7. Update this `context.md` with the new app's section

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
