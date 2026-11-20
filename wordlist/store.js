// Data access. Two backends with the same interface:
//   remoteStore: Supabase (Postgres + RLS + edge functions) — the real app
//   localStore:  this browser only — demo mode when config.js is empty
// Plus the offline outbox (IndexedDB) for answers given without a connection.

import { SUPABASE_URL, SUPABASE_ANON_KEY } from './config.js';

export const configured = !!(SUPABASE_URL && SUPABASE_ANON_KEY);

/* ---------- Supabase ---------- */

async function remoteStore() {
  const { createClient } = await import('https://cdn.jsdelivr.net/npm/@supabase/supabase-js@2.119.0/+esm');
  const sb = createClient(SUPABASE_URL, SUPABASE_ANON_KEY, {
    auth: { persistSession: true, autoRefreshToken: true, detectSessionInUrl: true },
  });
  const ok = ({ data, error }) => { if (error) throw error; return data; };
  const pageAll = async (table, cols) => {           // PostgREST caps responses at 1000 rows
    const out = [];
    for (let from = 0; ; from += 1000) {
      const rows = ok(await sb.from(table).select(cols).range(from, from + 999));
      out.push(...rows);
      if (rows.length < 1000) return out;
    }
  };
  return {
    mode: 'remote',
    async session() { return (await sb.auth.getSession()).data.session; },
    onAuth(fn) { sb.auth.onAuthStateChange((_e, s) => fn(s)); },
    async sendLink(email) {
      // Always the folder URL (…/wordlist/), never …/index.html, so it matches the allowed redirect URL.
      const redirect = new URL('./', location.href).href;
      ok(await sb.auth.signInWithOtp({ email, options: { shouldCreateUser: false, emailRedirectTo: redirect } }));
    },
    async verifyCode(email, token) { ok(await sb.auth.verifyOtp({ email, token, type: 'email' })); },
    async signInPassword(email, password) { ok(await sb.auth.signInWithPassword({ email, password })); },
    async setPassword(password) { ok(await sb.auth.updateUser({ password })); },
    async signOut() { await sb.auth.signOut(); },
    async loadAll() {
      const [decks, cards, states] = await Promise.all([
        pageAll('decks', '*'), pageAll('cards', '*'), pageAll('card_states', '*'),
      ]);
      return { decks, cards, states };
    },
    async recordReview(r) {
      ok(await sb.rpc('record_review', { p_card: r.cardId, p_direction: r.direction, p_grade: r.grade,
        p_state_before: r.stateBefore, p_next: r.next, p_client_id: r.clientId }));
    },
    async createDeck(d) { return ok(await sb.from('decks').insert(d).select().single()); },
    async updateDeck(id, patch) { return ok(await sb.from('decks').update(patch).eq('id', id).select().single()); },
    async deleteDeck(id) { ok(await sb.from('decks').delete().eq('id', id)); },
    async insertCards(rows) {
      const data = ok(await sb.from('cards').upsert(rows, { onConflict: 'deck_id,lemma', ignoreDuplicates: true }).select());
      const got = new Set(data.map(c => c.lemma));
      return { inserted: data, skipped: rows.filter(r => !got.has(r.lemma)).map(r => r.term) };
    },
    async updateCard(id, patch) { return ok(await sb.from('cards').update(patch).eq('id', id).select().single()); },
    async deleteCard(id) { ok(await sb.from('cards').delete().eq('id', id)); },
    async importDeck(payload, deckId) { return ok(await sb.rpc('import_deck', { p: payload, p_deck: deckId || null })); },
    async generate(deckId, input) {
      const { data, error } = await sb.functions.invoke('generate', { body: { deckId, input } });
      if (error) {
        let msg = error.message;
        try { msg = (await error.context.json()).error || msg; } catch {}
        throw new Error(msg);
      }
      return data;
    },
  };
}

/* ---------- this browser only (demo) ---------- */

const LS_KEY = 'wz_demo_db';
function uuid() { return crypto.randomUUID ? crypto.randomUUID() : 'id-' + Math.random().toString(36).slice(2) + Date.now(); }

function localStore() {
  let db;
  try { db = JSON.parse(localStorage.getItem(LS_KEY)) || null; } catch { db = null; }
  db = db || { decks: [], cards: [], states: [] };
  const save = () => { try { localStorage.setItem(LS_KEY, JSON.stringify(db)); } catch {} };
  const now = () => new Date().toISOString();
  const lemmaTaken = (deckId, lemma, exceptId) => db.cards.some(c => c.deck_id === deckId && c.lemma === lemma && c.id !== exceptId);
  return {
    mode: 'local',
    async session() { return { user: { email: 'demo (this browser only)' } }; },
    onAuth() {},
    async signOut() {},
    async loadAll() { return structuredClone(db); },
    async recordReview(r) {
      const i = db.states.findIndex(s => s.card_id === r.cardId && s.direction === r.direction);
      const row = { ...r.next, card_id: r.cardId, direction: r.direction };
      if (i < 0) db.states.push(row);
      else if (!db.states[i].last_review || row.last_review >= db.states[i].last_review) db.states[i] = row;
      save();
    },
    async createDeck(d) { const deck = { id: uuid(), archived: false, created_at: now(), new_per_day: 20, source_lang: 'de', target_lang: 'en', ...d }; db.decks.push(deck); save(); return deck; },
    async updateDeck(id, patch) { const d = db.decks.find(x => x.id === id); Object.assign(d, patch); save(); return d; },
    async deleteDeck(id) {
      const gone = new Set(db.cards.filter(c => c.deck_id === id).map(c => c.id));
      db.decks = db.decks.filter(d => d.id !== id);
      db.cards = db.cards.filter(c => !gone.has(c.id));
      db.states = db.states.filter(s => !gone.has(s.card_id));
      save();
    },
    async insertCards(rows) {
      const inserted = [], skipped = [];
      for (const r of rows) {
        if (lemmaTaken(r.deck_id, r.lemma)) { skipped.push(r.term); continue; }
        const c = { id: uuid(), created_at: now(), needs_review: false, ...r };
        db.cards.push(c); inserted.push(c);
      }
      save();
      return { inserted, skipped };
    },
    async updateCard(id, patch) {
      const c = db.cards.find(x => x.id === id);
      const next = { ...c, ...patch };
      if (lemmaTaken(next.deck_id, next.lemma, id)) throw new Error('That word is already in the target deck');
      Object.assign(c, patch); save(); return c;
    },
    async deleteCard(id) { db.cards = db.cards.filter(c => c.id !== id); db.states = db.states.filter(s => s.card_id !== id); save(); },
    async importDeck(p, deckId) {
      let deck = deckId && db.decks.find(d => d.id === deckId);
      let base = 0;
      if (!deck) deck = await this.createDeck(p.deck);
      else base = Math.max(0, ...db.cards.filter(c => c.deck_id === deck.id).map(c => c.position));
      let inserted = 0; const skipped = [];
      for (const c of p.cards) {
        if (lemmaTaken(deck.id, c.lemma)) { skipped.push(c.lemma); continue; }
        const { states, ...rest } = c;
        const card = { id: uuid(), needs_review: false, ...rest, deck_id: deck.id, position: base + (c.position || 0), created_at: c.created_at || now() };
        db.cards.push(card); inserted++;
        for (const s of states || []) db.states.push({ ...s, card_id: card.id });
      }
      save();
      return { deck_id: deck.id, inserted, skipped };
    },
    async generate() { throw new Error('Claude needs the server. Set up Supabase (see README) to use it.'); },
  };
}

export async function openStore() { return configured ? remoteStore() : localStore(); }

/* ---------- offline outbox (IndexedDB) ---------- */

const DB_NAME = 'wortschatz', OUTBOX = 'outbox';
let idb = null;
function openIdb() {
  if (idb) return idb;
  idb = new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, 1);
    req.onupgradeneeded = () => req.result.createObjectStore(OUTBOX, { keyPath: 'clientId' });
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
  return idb;
}
async function tx(mode, fn) {
  const d = await openIdb();
  return new Promise((resolve, reject) => {
    const t = d.transaction(OUTBOX, mode);
    const r = fn(t.objectStore(OUTBOX));
    t.oncomplete = () => resolve(r && r.result);
    t.onerror = () => reject(t.error);
  });
}

// Falls back to memory if IndexedDB is unavailable (private mode, etc.).
const memory = new Map();
export const outbox = {
  async add(item) { try { await tx('readwrite', s => s.put(item)); } catch { memory.set(item.clientId, item); } },
  async remove(id) { try { await tx('readwrite', s => s.delete(id)); } catch {} memory.delete(id); },
  async all() {
    let items = [];
    try { items = (await tx('readonly', s => s.getAll())) || []; } catch {}
    return items.concat([...memory.values()]).sort((a, b) => (a.next.last_review < b.next.last_review ? -1 : 1));
  },
};
