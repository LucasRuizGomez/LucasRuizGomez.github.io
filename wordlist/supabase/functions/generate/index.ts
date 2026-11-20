// POST /functions/v1/generate  { deckId, input }  ->  { cards: DraftCard[], notes: string[] }
// Builds draft cards with Claude. Saves nothing: the app shows a preview and inserts what you keep.
import { generateCards, GenerateError, MAX_INPUT, MODEL } from "../_shared/claude.ts";
import { cors, json, overRateLimit, userClient, lemmaOf } from "../_shared/http.ts";

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors(req) });
  if (req.method !== "POST") return json(req, { error: "Method not allowed" }, 405);

  const who = await userClient(req);
  if (!who) return json(req, { error: "Not signed in" }, 401);
  const { sb, userId } = who;

  let body: { deckId?: string; input?: string };
  try { body = await req.json(); } catch { return json(req, { error: "Invalid JSON" }, 400); }
  const input = String(body.input ?? "").trim();
  if (!body.deckId || !input) return json(req, { error: "deckId and input are required" }, 400);
  if (input.length > MAX_INPUT) return json(req, { error: `Input is longer than ${MAX_INPUT} characters` }, 413);

  const { data: deck } = await sb.from("decks").select("id, source_lang, target_lang").eq("id", body.deckId).maybeSingle();
  if (!deck) return json(req, { error: "Deck not found" }, 404);
  if (await overRateLimit(sb, userId)) return json(req, { error: "Limit reached: 20 generations per hour" }, 429);

  const { data: rows } = await sb.from("cards").select("lemma").eq("deck_id", deck.id);
  const existing = (rows ?? []).map((r) => r.lemma as string);

  try {
    const out = await generateCards({ input, sourceLang: deck.source_lang, targetLang: deck.target_lang, existingLemmas: existing });
    await sb.from("ai_calls").insert({ user_id: userId, route: "generate", model: MODEL,
      input_tokens: out.inputTokens, output_tokens: out.outputTokens });
    // Duplicate check does not rely on the model alone.
    const have = new Set(existing);
    const cards = out.cards.map((c) => {
      const lemma = lemmaOf(c.term) || c.lemma;
      const flags = new Set(c.flags ?? []);
      if (have.has(lemma)) flags.add("duplicate");
      return { ...c, lemma, flags: [...flags] };
    });
    return json(req, { cards, notes: out.notes });
  } catch (e) {
    await sb.from("ai_calls").insert({ user_id: userId, route: "generate", model: MODEL, ok: false });
    if (e instanceof GenerateError) return json(req, { error: e.message }, e.status);
    console.error(e);
    return json(req, { error: "Generation failed" }, 500);
  }
});
