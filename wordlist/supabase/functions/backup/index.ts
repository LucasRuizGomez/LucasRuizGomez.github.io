// Weekly backup: writes every deck as a wortschatz-export/v1 JSON file to the private
// `backups` bucket: backups/<YYYY-MM-DD>/<deck name>.json. Called by pg_cron (see ../../cron.sql)
// with `Authorization: Bearer <BACKUP_TOKEN>`. Deploy with --no-verify-jwt.
import { json, sameSecret, serviceClient } from "../_shared/http.ts";

const ms = (v: string | null) => (v ? Date.parse(v) : 0);

Deno.serve(async (req) => {
  const token = (req.headers.get("Authorization") ?? "").replace(/^Bearer\s+/i, "");
  if (!sameSecret(token, Deno.env.get("BACKUP_TOKEN") ?? "")) return json(req, { error: "Unauthorized" }, 401);

  const sb = serviceClient();
  const day = new Date().toISOString().slice(0, 10);
  const { data: decks, error } = await sb.from("decks").select("*");
  if (error) return json(req, { error: error.message }, 500);

  const written: string[] = [];
  for (const deck of decks ?? []) {
    const { data: cards } = await sb.from("cards").select("*").eq("deck_id", deck.id).order("position");
    const ids = (cards ?? []).map((c) => c.id);
    const { data: states } = ids.length
      ? await sb.from("card_states").select("*").in("card_id", ids)
      : { data: [] };
    const lemmaById = new Map((cards ?? []).map((c) => [c.id, c.lemma]));
    // Mirrors srs.js buildExport / rowToState.
    const out = {
      format: "wortschatz-export/v1",
      exported_at: new Date().toISOString().replace(/\.\d{3}Z$/, "Z"),
      deck: { name: deck.name, source_lang: deck.source_lang, target_lang: deck.target_lang, new_per_day: deck.new_per_day },
      cards: (cards ?? []).map((c) => JSON.parse(JSON.stringify({
        id: c.lemma, n: c.position, de: c.term, pos: c.pos || undefined, forms: c.forms || undefined, en: c.meaning,
        usage: c.usage || undefined, example: c.example || undefined, exampleEn: c.example_translation || undefined,
        added: (c.created_at || "").slice(0, 10) || undefined,
      }))),
      progress: Object.fromEntries((states ?? []).map((s) => [`${lemmaById.get(s.card_id)}__${s.direction}`, {
        state: s.state, step: s.step, interval: Number(s.interval_days), ease: Number(s.ease), due: ms(s.due),
        reps: s.reps, lapses: s.lapses, correct: s.correct, wrong: s.wrong, last: ms(s.last_review),
        introduced: ms(s.introduced_at),
      }])),
    };
    const safe = deck.name.replace(/[^\p{L}\p{N} _-]/gu, "_");
    const path = `${deck.user_id}/${day}/${safe}-${deck.id.slice(0, 8)}.json`;
    const { error: upErr } = await sb.storage.from("backups").upload(path,
      new Blob([JSON.stringify(out, null, 1)], { type: "application/json" }), { upsert: true });
    if (upErr) return json(req, { error: upErr.message, written }, 500);
    written.push(path);
  }
  return json(req, { written });
});
