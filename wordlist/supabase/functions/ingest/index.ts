// POST /functions/v1/ingest   Authorization: Bearer <INGEST_TOKEN>   { deck: "Deutsch C1", input }
// Phase 4: for an iOS Shortcut or any script. Generates and saves in one step, no preview.
// New cards get needs_review = true so they stand out in the Deck tab. Duplicates are skipped.
// Deploy with --no-verify-jwt (the personal token replaces the Supabase session).
import { generateCards, GenerateError, MAX_INPUT, MODEL } from "../_shared/claude.ts";
import { cors, json, lemmaOf, sameSecret, serviceClient, RATE_LIMIT_PER_HOUR } from "../_shared/http.ts";

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors(req) });
  if (req.method !== "POST") return json(req, { error: "Method not allowed" }, 405);

  const token = (req.headers.get("Authorization") ?? "").replace(/^Bearer\s+/i, "");
  const expected = Deno.env.get("INGEST_TOKEN") ?? "";
  const userId = Deno.env.get("OWNER_USER_ID") ?? "";
  if (!expected || !userId || !sameSecret(token, expected)) return json(req, { error: "Unauthorized" }, 401);

  let body: { deck?: string; input?: string };
  try { body = await req.json(); } catch { return json(req, { error: "Invalid JSON" }, 400); }
  const input = String(body.input ?? "").trim();
  if (!input) return json(req, { error: "input is required" }, 400);
  if (input.length > MAX_INPUT) return json(req, { error: `Input is longer than ${MAX_INPUT} characters` }, 413);

  // Service role bypasses RLS, so every query below filters by the owner explicitly.
  const sb = serviceClient();
  const deckName = String(body.deck ?? "Deutsch C1");
  const { data: deck } = await sb.from("decks").select("id, source_lang, target_lang")
    .eq("user_id", userId).eq("name", deckName).eq("archived", false).limit(1).maybeSingle();
  if (!deck) return json(req, { error: `Deck "${deckName}" not found` }, 404);

  const since = new Date(Date.now() - 3600_000).toISOString();
  const { count } = await sb.from("ai_calls").select("id", { count: "exact", head: true })
    .eq("user_id", userId).gte("created_at", since);
  if ((count ?? 0) >= RATE_LIMIT_PER_HOUR) return json(req, { error: "Limit reached: 20 generations per hour" }, 429);

  const { data: rows } = await sb.from("cards").select("lemma, position").eq("deck_id", deck.id);
  const have = new Set((rows ?? []).map((r) => r.lemma as string));
  let position = Math.max(0, ...(rows ?? []).map((r) => r.position as number));

  try {
    const out = await generateCards({ input, sourceLang: deck.source_lang, targetLang: deck.target_lang, existingLemmas: [...have] });
    await sb.from("ai_calls").insert({ user_id: userId, route: "ingest", model: MODEL,
      input_tokens: out.inputTokens, output_tokens: out.outputTokens });
    const saved: string[] = [], skipped: string[] = [];
    const inserts = [];
    for (const c of out.cards) {
      const lemma = lemmaOf(c.term);
      if (!lemma || have.has(lemma)) { skipped.push(c.term); continue; }
      have.add(lemma);
      inserts.push({ user_id: userId, deck_id: deck.id, position: ++position, term: c.term, lemma,
        pos: c.pos || null, forms: c.forms || null, meaning: c.meaning, usage: c.usage || null,
        example: c.example || null, example_translation: c.example_translation || null,
        user_input: c.user_input || null, needs_review: true });
      saved.push(c.term);
    }
    if (inserts.length) {
      const { error } = await sb.from("cards").insert(inserts);
      if (error) return json(req, { error: error.message }, 500);
    }
    return json(req, { saved, skipped, notes: out.notes });
  } catch (e) {
    await sb.from("ai_calls").insert({ user_id: userId, route: "ingest", model: MODEL, ok: false });
    if (e instanceof GenerateError) return json(req, { error: e.message }, e.status);
    console.error(e);
    return json(req, { error: "Generation failed" }, 500);
  }
});
