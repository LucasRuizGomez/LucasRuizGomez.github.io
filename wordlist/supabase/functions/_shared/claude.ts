// Card generation with Claude (CONTEXT.md §6). Used by `generate` and `ingest`.
import Anthropic from "npm:@anthropic-ai/sdk@1.8.0";

export const MODEL = Deno.env.get("WORTSCHATZ_MODEL") ?? "claude-sonnet-5-5";
export const MAX_INPUT = 8000;

const LANG: Record<string, string> = { de: "German", en: "English", es: "Spanish", fr: "French", it: "Italian" };
const langName = (code: string) => LANG[code] ?? code;

const FLAGS = ["normalized", "corrected", "duplicate", "uncertain"];

// The contract from §6.2. strict + additionalProperties:false make the arguments schema-valid.
export const saveCardsTool: Anthropic.Tool = {
  name: "save_cards",
  description: "Return the flashcards built from the user's input.",
  strict: true,
  input_schema: {
    type: "object",
    additionalProperties: false,
    properties: {
      cards: {
        type: "array",
        items: {
          type: "object",
          additionalProperties: false,
          properties: {
            term: { type: "string", description: "Dictionary form with article for nouns, 'sich' for reflexive verbs" },
            lemma: { type: "string", description: "Lowercase, no article, no 'sich', umlauts kept" },
            pos: { type: "string" },
            forms: { type: "string" },
            meaning: { type: "string" },
            usage: { type: "string" },
            example: { type: "string" },
            example_translation: { type: "string" },
            user_input: { type: "string", description: "The exact item the user wrote" },
            flags: { type: "array", items: { type: "string", enum: FLAGS } },
            flag_note: { type: "string" },
          },
          required: ["term", "lemma", "pos", "meaning", "example", "example_translation", "user_input", "flags"],
        },
      },
      notes: { type: "array", items: { type: "string" } },
    },
    required: ["cards"],
  },
} as Anthropic.Tool;

function systemPrompt(source: string, target: string, existing: string[]) {
  return `You build vocabulary flashcards for a learner preparing for a C1 exam in ${langName(source)}.
Meanings and translations are written in ${langName(target)}.

For every item in the user's input:
- Reduce it to its dictionary form (lemma). Inflected forms (genitive, plural, conjugated verbs,
  zu-infinitives) become the base form; add flag "normalized" and say what you changed in flag_note.
- If the word does not exist or is misspelled, pick the most likely intended word, add flag
  "corrected" and explain in flag_note. If you can't tell, add flag "uncertain".
- If the lemma is in this list of existing cards, add flag "duplicate": ${existing.length ? existing.join(", ") : "(none)"}
- If the user gave a definition, use it to choose the right sense, but write the meaning yourself.

Card content (German):
- Nouns: term with article; forms = "der X, -es, -e" (genitive and plural); feminine form if common.
- Verbs: forms = "3rd sg present – Präteritum – Perfekt (hat/ist …)"; note separable/inseparable,
  reflexive, and the case or preposition the verb governs (e.g. "sich widersetzen + Dat").
- Adjectives/adverbs: comparison or derived forms if useful.
- meaning: the 1–4 most useful senses, most common first, separated by semicolons.
- usage: fixed collocations and idioms a C1 candidate should know, separated by "; " or " · ".
  Include false friends or easily confused words (e.g. fordern ≠ fördern).
- example: one natural sentence at C1 level, preferably formal/written register (news, essay,
  work), 10–20 words, showing the most useful collocation.
- example_translation: faithful translation of the example.
- Mark register when relevant (colloquial, formal, dated).

Put general remarks in notes. Never invent words.

Answer by calling the save_cards tool once, with one card per item in the input.`;
}

export type DraftCard = {
  term: string; lemma: string; pos: string; forms?: string; meaning: string; usage?: string;
  example: string; example_translation: string; user_input: string; flags: string[]; flag_note?: string;
};

export class GenerateError extends Error {
  constructor(message: string, public status = 502) { super(message); }
}

const client = new Anthropic(); // reads ANTHROPIC_API_KEY from the function's secrets

export async function generateCards(opts: { input: string; sourceLang: string; targetLang: string; existingLemmas: string[] }) {
  let inputTokens = 0, outputTokens = 0;
  // Claude Sonnet 5.5 rejects forced tool_choice, so ask for the tool with `auto`
  // and retry once if the model answered without calling it.
  for (let attempt = 0; attempt < 2; attempt++) {
    // deno-lint-ignore no-explicit-any
    const params: any = {
      model: MODEL,
      max_tokens: 16000,
      betas: ["server-side-fallback-2026-07-01"],
      fallbacks: "default",           // retry server-side if a safety classifier declines
      output_config: { effort: "medium" },
      system: systemPrompt(opts.sourceLang, opts.targetLang, opts.existingLemmas),
      tools: [saveCardsTool],
      tool_choice: { type: "auto" },
      messages: [{ role: "user", content: opts.input }],
    };
    let res;
    try {
      res = await client.beta.messages.create(params);
    } catch (e) {
      if (e instanceof Anthropic.RateLimitError) throw new GenerateError("Claude is rate-limited right now, try again in a minute.", 429);
      if (e instanceof Anthropic.APIStatusError) throw new GenerateError(`Claude API error ${e.status}`, 502);
      if (e instanceof Anthropic.APIConnectionError) throw new GenerateError("Could not reach the Claude API.", 502);
      throw e;
    }
    inputTokens += res.usage?.input_tokens ?? 0;
    outputTokens += res.usage?.output_tokens ?? 0;
    if (res.stop_reason === "refusal") throw new GenerateError("Claude declined this input.", 422);
    if (res.stop_reason === "max_tokens") throw new GenerateError("The input is too long to process in one go. Split it in smaller batches.", 413);
    // deno-lint-ignore no-explicit-any
    const call = res.content.find((b: any) => b.type === "tool_use" && b.name === "save_cards") as any;
    if (call) {
      const out = typeof call.input === "string" ? JSON.parse(call.input) : call.input;
      const cards: DraftCard[] = (out.cards ?? []).filter((c: DraftCard) => c && c.term && c.meaning);
      return { cards, notes: (out.notes ?? []) as string[], inputTokens, outputTokens };
    }
  }
  throw new GenerateError("Claude did not return any cards. Try again.", 502);
}
