// Wortschatz: pure logic shared by index.html and tests.html.
// No DOM, no network. The scheduler is FSRS-6; the study queue is a straight port of reference-index.html.

export const MIN = 60_000, DAY = 86_400_000;
export const STEPS = [1, 10];            // learning steps, minutes
export const LEARN_AHEAD = 20 * MIN;
export const DAY_START_HOUR = 4;         // the study day starts at 04:00 local time
export const EXPORT_FORMAT = 'wortschatz-export/v1';

/* ---------- scheduler: FSRS-6 ----------
   Free Spaced Repetition Scheduler, https://github.com/open-spaced-repetition/free-spaced-repetition-scheduler
   Each card direction has a memory state:
     stability  S  days until recall probability falls to 90%
     difficulty D  1 (easy) … 10 (hard)
   Learning steps (1 min, 10 min) and the 10-min relearning step stay as before; FSRS decides
   every interval in days. `ease` is kept only so cards scheduled by the old SM-2 code can be
   converted to an FSRS memory state the first time they are answered.                    */

// FSRS-6 default parameters (same as ts-fsrs / py-fsrs / Anki before optimisation).
export const W = Object.freeze([
  0.212, 1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194, 0.001, 1.8722, 0.1666, 0.796,
  1.4835, 0.0614, 0.2629, 1.6483, 0.6014, 1.8729, 0.5425, 0.0912, 0.0658, 0.1542,
]);
export const RETENTION = 0.9;            // desired probability of recall when a card comes due
export const MAX_INTERVAL = 36500;       // days
const S_MIN = 0.001, S_MAX = 36500;
const DECAY = -W[20], FACTOR = Math.pow(0.9, 1 / DECAY) - 1;
const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));

// Probability of recall after `days` with stability S (forgetting curve).
export const forgettingCurve = (days, S) => Math.pow(1 + FACTOR * days / S, DECAY);

// Days until recall probability drops to `r`.
export function nextInterval(S, r = RETENTION) {
  return clamp(Math.round(S / FACTOR * (Math.pow(r, 1 / DECAY) - 1)), 1, MAX_INTERVAL);
}

const initStability = g => clamp(W[g - 1], S_MIN, S_MAX);
const initDifficulty = g => W[4] - Math.exp(W[5] * (g - 1)) + 1;   // unclamped (mean-reversion target uses it raw)

function nextDifficulty(D, g) {
  const damped = D + (-W[6] * (g - 3)) * (10 - D) / 9;               // linear damping
  return clamp(W[7] * initDifficulty(4) + (1 - W[7]) * damped, 1, 10); // mean reversion
}

function recallStability(D, S, R, g) {
  const hard = g === 2 ? W[15] : 1, easy = g === 4 ? W[16] : 1;
  return clamp(S * (1 + Math.exp(W[8]) * (11 - D) * Math.pow(S, -W[9]) * (Math.exp(W[10] * (1 - R)) - 1) * hard * easy), S_MIN, S_MAX);
}

function forgetStability(D, S, R) {
  const s = W[11] * Math.pow(D, -W[12]) * (Math.pow(S + 1, W[13]) - 1) * Math.exp(W[14] * (1 - R));
  return clamp(Math.min(s, S / Math.exp(W[17] * W[18])), S_MIN, S_MAX);
}

// Reviews on the same day (learning / relearning steps).
function shortTermStability(S, g) {
  let inc = Math.exp(W[17] * (g - 3 + W[18])) * Math.pow(S, -W[19]);
  if (g >= 3) inc = Math.max(inc, 1);
  return clamp(S * inc, S_MIN, S_MAX);
}

// Memory state for a card last scheduled by SM-2 (port of fsrs-rs memory_state_from_sm2, 90% retention).
export function fromSm2(ease, interval) {
  const S = clamp(interval, S_MIN, S_MAX);
  const D = 11 - (ease - 1) / (Math.exp(W[8]) * Math.pow(S, -W[9]) * (Math.exp((1 - 0.9) * W[10]) - 1));
  return { stability: S, difficulty: clamp(D, 1, 10) };
}

// Whole study days between two timestamps (the day rolls over at DAY_START_HOUR, like Anki).
export const daysBetween = (a, b) => Math.max(0, Math.round((startOfDay(b) - startOfDay(a)) / DAY));

// The memory state after answering `g` at time t.
export function memoryAfter(p, g, t) {
  let S = p.stability, D = p.difficulty;
  if (!(S > 0 && D > 0)) {
    if (p.state === 'new' || !(p.interval > 0)) return { stability: initStability(g), difficulty: clamp(initDifficulty(g), 1, 10) };
    ({ stability: S, difficulty: D } = fromSm2(p.ease || 2.5, p.interval));
  }
  const elapsed = p.last ? daysBetween(p.last, t) : 0;
  if (elapsed === 0) return { stability: shortTermStability(S, g), difficulty: nextDifficulty(D, g) };
  const R = forgettingCurve(elapsed, S);
  return {
    stability: g === 1 ? forgetStability(D, S, R) : recallStability(D, S, R, g),
    difficulty: nextDifficulty(D, g),
  };
}

// Current probability of recall (0…1), or null for cards never answered.
export function retrievability(p, t = Date.now()) {
  if (p.state === 'new') return null;
  const S = p.stability > 0 ? p.stability : p.interval > 0 ? p.interval : 0;
  if (!S || !p.last) return null;
  return forgettingCurve(Math.max(0, (t - p.last) / DAY), S);
}

export function fresh() {
  return { state: 'new', step: 0, interval: 0, ease: 2.5, stability: 0, difficulty: 0, due: 0, reps: 0, lapses: 0,
    correct: 0, wrong: 0, last: 0, introduced: 0 };
}

export function schedule(p, g, t = Date.now()) {
  const mem = [1, 2, 3, 4].map(x => memoryAfter(p, x, t));
  // Day intervals per grade. On review cards they are kept in order: Hard ≤ Good < Easy.
  let hard = nextInterval(mem[1].stability), good = nextInterval(mem[2].stability), easy = nextInterval(mem[3].stability);
  if (p.state === 'review') { hard = Math.min(hard, good); good = Math.max(good, hard + 1); easy = Math.max(easy, good + 1); }

  const s = { ...p }; s.reps++;
  if (!s.introduced) s.introduced = t;
  if (g === 1) s.wrong++; else s.correct++;
  s.stability = mem[g - 1].stability; s.difficulty = mem[g - 1].difficulty;
  const graduate = days => { s.state = 'review'; s.step = 0; s.interval = days; s.due = t + days * DAY; };

  if (s.state !== 'review') {
    const relearn = s.state === 'relearning';
    if (s.state === 'new') s.state = 'learning';
    if (g === 1) { s.step = 0; s.due = t + (relearn ? 10 : STEPS[0]) * MIN; }
    else if (g === 2) { s.due = t + (relearn ? 10 : (s.step === 0 ? 6 : STEPS[s.step])) * MIN; }
    else if (g === 3) {
      if (!relearn && s.step + 1 < STEPS.length) { s.step++; s.due = t + STEPS[s.step] * MIN; }
      else graduate(good);
    } else graduate(easy);
  } else {
    if (g === 1) { s.lapses++; s.state = 'relearning'; s.step = 0; s.interval = nextInterval(s.stability); s.due = t + 10 * MIN; }
    else graduate(g === 2 ? hard : g === 3 ? good : easy);
  }
  s.last = t;
  return s;
}

export function fmtSpan(ms) {
  if (ms < 60 * MIN) return Math.max(1, Math.round(ms / MIN)) + ' min';
  if (ms < DAY) return Math.round(ms / (60 * MIN)) + ' h';
  const d = ms / DAY;
  if (d < 30) return Math.round(d) + ' d';
  if (d < 365) return (d / 30).toFixed(1).replace('.0', '') + ' mo';
  return (d / 365).toFixed(1).replace('.0', '') + ' y';
}

export function startOfDay(t = Date.now()) {
  const d = new Date(t); d.setHours(DAY_START_HOUR, 0, 0, 0);
  if (t < +d) d.setDate(d.getDate() - 1);
  return +d;
}

/* ---------- study queue ----------
   cards:    [{ id, deck_id, position, ... }]
   progress: { "<cardId>__f": State, "<cardId>__r": State }
   decks:    { [deckId]: { new_per_day } }                                   */

export const keyOf = (cardId, dir) => cardId + '__' + dir;

export function items(cards, progress) {
  const list = [];
  const sorted = cards.slice().sort((a, b) => (a.position || 0) - (b.position || 0));
  for (const c of sorted) {
    const fk = keyOf(c.id, 'f'), rk = keyOf(c.id, 'r');
    const fp = progress[fk];
    list.push({ key: fk, card: c, dir: 'f', p: fp || fresh() });
    if (progress[rk] || (fp && fp.state === 'review')) list.push({ key: rk, card: c, dir: 'r', p: progress[rk] || fresh() });
  }
  return list;
}

const isLearning = x => x.p.state === 'learning' || x.p.state === 'relearning';

export function buckets(cards, progress, decks, t = Date.now()) {
  const all = items(cards, progress), sod = startOfDay(t);
  // New-card limit applies per deck ("All decks" sums the per-deck allowances).
  const byDeck = new Map();
  for (const x of all) {
    const d = x.card.deck_id;
    if (!byDeck.has(d)) byDeck.set(d, []);
    byDeck.get(d).push(x);
  }
  let news = [];
  for (const [deckId, list] of byDeck) {
    const limit = decks?.[deckId]?.new_per_day ?? 20;
    const introducedToday = list.filter(x => x.p.introduced >= sod).length;
    const left = Math.max(0, limit - introducedToday);
    const fresh = list.filter(x => x.p.state === 'new')
      .sort((a, b) => (a.dir === b.dir ? 0 : a.dir === 'f' ? -1 : 1));
    news = news.concat(fresh.slice(0, left));
  }
  // Across decks: keep f before r, then position, so a single deck matches the reference exactly.
  if (byDeck.size > 1) news.sort((a, b) => (a.dir === b.dir ? 0 : a.dir === 'f' ? -1 : 1) || (a.card.position - b.card.position));
  const learning = all.filter(isLearning);
  return {
    all,
    learnDue: learning.filter(x => x.p.due <= t).sort((a, b) => a.p.due - b.p.due),
    learnSoon: learning.filter(x => x.p.due <= t + LEARN_AHEAD).sort((a, b) => a.p.due - b.p.due),
    learningAll: learning,
    reviewDue: all.filter(x => x.p.state === 'review' && x.p.due <= t).sort((a, b) => a.p.due - b.p.due),
    news,
  };
}

export function counts(b, t = Date.now()) {
  return {
    new: b.news.length,
    learning: b.learningAll.filter(x => x.p.due <= t + LEARN_AHEAD).length,
    due: b.reviewDue.length,
  };
}

export function pickNext(b, lastKey) {
  const avoid = arr => arr.find(x => x.key !== lastKey) || arr[0];
  if (b.learnDue.length) return avoid(b.learnDue);
  if (b.reviewDue.length) return avoid(b.reviewDue);
  if (b.news.length) return avoid(b.news);
  if (b.learnSoon.length) return avoid(b.learnSoon);
  return null;
}

export function nextDue(b) {
  return b.all.filter(x => x.p.state !== 'new').map(x => x.p.due).sort((a, c) => a - c)[0] || null;
}

/* ---------- extra practice ---------- */

export function shuffle(a, rnd = Math.random) {
  a = a.slice();
  for (let i = a.length - 1; i > 0; i--) { const j = Math.floor(rnd() * (i + 1)); [a[i], a[j]] = [a[j], a[i]]; }
  return a;
}

// A missed card goes back three places later (or to the end of a short queue).
export function cramRequeue(queue, item, g) {
  const q = queue.slice();
  if (g === 1) q.splice(Math.min(3, q.length), 0, item);
  return q;
}

/* ---------- lemmas ---------- */

// "der Vorreiter" -> "vorreiter", "sich widersetzen" -> "widersetzen", umlauts kept.
export function lemmaOf(term) {
  let s = String(term || '').trim().toLowerCase().replace(/\s+/g, ' ');
  s = s.split(/ \/ |; |, /)[0];  // "ergeben / sich ergeben" -> "ergeben"
  s = s.replace(/^(der|die|das)(\/(der|die|das))* /, '');
  s = s.replace(/^sich /, '').replace(/ sich$/, '');
  return s.trim();
}

/* ---------- DB <-> client state ---------- */

const ms = v => (v ? Date.parse(v) : 0);
const iso = v => (v ? new Date(v).toISOString() : null);

export function rowToState(r) {
  return {
    state: r.state, step: r.step, interval: Number(r.interval_days), ease: Number(r.ease),
    stability: Number(r.stability) || 0, difficulty: Number(r.difficulty) || 0, due: ms(r.due), reps: r.reps, lapses: r.lapses, correct: r.correct, wrong: r.wrong,
    last: ms(r.last_review), introduced: ms(r.introduced_at),
  };
}

export function stateToRow(cardId, direction, s) {
  return {
    card_id: cardId, direction, state: s.state, step: s.step, interval_days: s.interval, ease: s.ease,
    stability: s.stability > 0 ? s.stability : null, difficulty: s.difficulty > 0 ? s.difficulty : null,
    due: iso(s.due) || new Date(0).toISOString(), reps: s.reps, lapses: s.lapses, correct: s.correct,
    wrong: s.wrong, last_review: iso(s.last), introduced_at: iso(s.introduced),
  };
}

/* ---------- import / export (seed format) ---------- */

// Turns a wortschatz-export/v1 file into rows for the import_deck RPC.
export function parseExport(json) {
  const d = typeof json === 'string' ? JSON.parse(json) : json;
  if (!d || d.format !== EXPORT_FORMAT) throw new Error('Not a ' + EXPORT_FORMAT + ' file');
  if (!Array.isArray(d.cards)) throw new Error('The file has no cards');
  const progress = d.progress || {};
  const seen = new Set();
  const cards = [];
  for (const c of d.cards) {
    if (!c || !c.de || !c.en) continue;
    const lemma = lemmaOf(c.de);
    if (!lemma || seen.has(lemma)) continue;
    seen.add(lemma);
    const states = [];
    for (const dir of ['f', 'r']) {
      const p = progress[keyOf(c.id, dir)];
      if (!p) continue;
      const s = { ...fresh(), ...p };
      const row = stateToRow(null, dir, s);
      delete row.card_id;
      states.push(row);
    }
    cards.push({
      lemma, position: c.n || 0, term: c.de, meaning: c.en, pos: c.pos || null, forms: c.forms || null,
      usage: c.usage || null, example: c.example || null, example_translation: c.exampleEn || null,
      user_input: c.userInput || null, created_at: c.added ? new Date(c.added).toISOString() : null,
      states,
    });
  }
  const deck = d.deck || {};
  return {
    deck: { name: deck.name || 'Imported', source_lang: deck.source_lang || 'de',
      target_lang: deck.target_lang || 'en', new_per_day: deck.new_per_day ?? 20 },
    cards,
  };
}

// Builds a wortschatz-export/v1 file from loaded data (same shape as the seed).
export function buildExport(deck, cards, progress, now = Date.now()) {
  const sorted = cards.slice().sort((a, b) => a.position - b.position);
  const out = {
    format: EXPORT_FORMAT,
    exported_at: new Date(now).toISOString().replace(/\.\d{3}Z$/, 'Z'),
    deck: { name: deck.name, source_lang: deck.source_lang, target_lang: deck.target_lang, new_per_day: deck.new_per_day },
    cards: sorted.map(c => {
      const o = { id: c.lemma, n: c.position, de: c.term, pos: c.pos || undefined, forms: c.forms || undefined,
        en: c.meaning, usage: c.usage || undefined, example: c.example || undefined,
        exampleEn: c.example_translation || undefined, added: (c.created_at || '').slice(0, 10) || undefined };
      return JSON.parse(JSON.stringify(o));
    }),
    progress: {},
  };
  for (const c of sorted) for (const dir of ['f', 'r']) {
    const p = progress[keyOf(c.id, dir)];
    if (p) out.progress[keyOf(c.lemma, dir)] = { ...p };
  }
  return out;
}

/* ---------- drafts (Add words) ---------- */

export const DRAFT_FIELDS = ['term', 'pos', 'forms', 'meaning', 'usage', 'example', 'example_translation'];

// Normalises a draft from the generate function and marks duplicates the model missed.
export function prepareDrafts(drafts, existingLemmas) {
  const have = new Set(existingLemmas);
  const batch = new Set();
  return drafts.map(d => {
    const lemma = lemmaOf(d.term) || String(d.lemma || '').toLowerCase();
    const flags = new Set(d.flags || []);
    if (have.has(lemma)) flags.add('duplicate'); else flags.delete('duplicate');
    if (batch.has(lemma)) flags.add('duplicate');
    batch.add(lemma);
    return { ...d, lemma, flags: [...flags], include: !flags.has('duplicate') };
  });
}

export function draftToRow(d, deckId, position) {
  const clean = v => (v == null || String(v).trim() === '' ? null : String(v).trim());
  return {
    deck_id: deckId, position, term: String(d.term).trim(), lemma: lemmaOf(d.term), pos: clean(d.pos),
    forms: clean(d.forms), meaning: String(d.meaning).trim(), usage: clean(d.usage), example: clean(d.example),
    example_translation: clean(d.example_translation), user_input: clean(d.user_input),
  };
}

export const draftValid = d => !!(String(d.term || '').trim() && String(d.meaning || '').trim() && lemmaOf(d.term));

/* ---------- deck table ---------- */

export function deckRows(cards, progress, t = Date.now()) {
  return cards.map(c => {
    const f = progress[keyOf(c.id, 'f')] || fresh(), r = progress[keyOf(c.id, 'r')];
    const correct = f.correct + (r ? r.correct : 0), wrong = f.wrong + (r ? r.wrong : 0);
    let status, label;
    if (f.state === 'new') { status = 'new'; label = 'New'; }
    else if (f.state !== 'review') { status = 'learn'; label = 'Learning'; }
    else { status = 'rev'; label = f.due <= t ? 'Due now' : 'in ' + fmtSpan(f.due - t); }
    return { c, correct, wrong, status, label, due: f.state === 'new' ? Infinity : f.due,
      acc: correct + wrong ? correct / (correct + wrong) : 2 };
  });
}

const bare = s => s.replace(/^(der|die|das|sich) /, '');
export const SORTERS = {
  n: (a, b) => (a.c.position || 0) - (b.c.position || 0),
  de: (a, b) => bare(a.c.term).localeCompare(bare(b.c.term), 'de'),
  wrong: (a, b) => b.wrong - a.wrong || a.acc - b.acc,
  due: (a, b) => a.due - b.due,
};

export function matches(c, q) {
  if (!q) return true;
  q = q.toLowerCase();
  return [c.term, c.meaning, c.usage, c.example].some(v => v && v.toLowerCase().includes(q));
}
