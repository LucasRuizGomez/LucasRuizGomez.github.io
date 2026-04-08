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

---

## Notes
- `background-attachment: fixed` on desktop, `scroll` on mobile (perf)
- All CSS is inline in `<style>` inside `index.html` — no external stylesheet for main page
- CNAME file sets custom domain: `lukeda.me`
