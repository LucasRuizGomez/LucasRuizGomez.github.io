# Wortschatz

Anki-style flashcards for C1 German vocabulary, served at **lukeda.me/wordlist/**. It's a single-user PWA that syncs between phone and PC through Supabase. The full spec is in [`CONTEXT.md`](CONTEXT.md), and the previous version of the app is [`reference-index.html`](reference-index.html).

## How it is built (and why it differs from CONTEXT.md §3)

lukeda.me is a static GitHub Pages site with no build step, so the app follows the same pattern instead of Next.js on Vercel. CONTEXT.md allows this ("si lukeda.me ya usa otro stack, adáptalo").

| Spec | Here |
|---|---|
| Next.js pages | `index.html` (vanilla JS modules), published by the existing Pages workflow |
| Shared TS algorithm | `srs.js`, an ES module imported by both the app and `tests.html` |
| Route handlers | Supabase directly from the browser under RLS, plus Postgres functions (`record_review`, `import_deck`) for the steps that must be atomic |
| `/api/cards/generate` | Edge function `generate` |
| `/api/ingest` | Edge function `ingest` (personal bearer token) |
| Weekly backup | Edge function `backup` + `pg_cron` (`supabase/cron.sql`) → private `backups` bucket |
| Export | Built in the browser from loaded data (`srs.js buildExport`), in the same format as the seed |

The data model and the generation contract are unchanged, with these differences:

- **Scheduler: FSRS-6 instead of SM-2.** `srs.js` implements [FSRS](https://github.com/open-spaced-repetition/free-spaced-repetition-scheduler) with the default FSRS-6 parameters and 90% desired retention (`RETENTION`). Every card direction stores `stability` and `difficulty` (migration `20261120130000_fsrs.sql`). The 1 min / 10 min learning steps and the 10 min relearning step are kept; FSRS decides every day interval, and answers on the same study day use its short-term stability formula. Cards scheduled by the old SM-2 code are converted from `ease` + `interval_days` the first time they're answered (the fsrs-rs `memory_state_from_sm2` formula), so existing progress carries over. The memory model matches `ts-fsrs` to within 1e-6.

- **Claude call.** `claude-sonnet-5-5` rejects forced `tool_choice` and non-default `temperature` with a 400 error. So `save_cards` is called with `tool_choice: auto` and `strict: true`, and the function retries once if Claude answers without calling it. `fallbacks: "default"` is enabled: if a safety classifier declines, the request is retried server-side on another model. Set the `WORTSCHATZ_MODEL` secret to `claude-haiku-5-5` to cut costs.
- **Extra columns.** `cards.needs_review` (for ingest), `review_log.client_id` (makes offline retries idempotent) and the `ai_calls` table (token usage).
- **Answer conflicts.** An answer that syncs late never overwrites a newer one from another device.

## Files

```
index.html     the app (Study · Deck · Add words · Decks · Settings)
srs.js         scheduler, study queue, lemma, import/export mapping — pure, tested
store.js       Supabase backend, local demo backend, offline outbox (IndexedDB)
config.js      Supabase URL + anon key (public)
sw.js          app-shell service worker
manifest.json  PWA manifest; icons/ (make_icons.py regenerates the PNGs)
tests.html     unit tests + parity check against the reference app + manual checklist
supabase/      migrations, edge functions, cron.sql
```

If `config.js` is empty, the app runs in **demo mode**. Data stays in that browser and "Generate with Claude" is disabled. It's handy for trying the UI locally (`python -m http.server 8000` → http://localhost:8000/wordlist/).

## Setup (one time)

1. **Create a Supabase project.**
   - In *Authentication → Providers → Email*, keep Email enabled and turn **off** "Allow new users to sign up".
   - In *Authentication → Users → Add user*, create your account with your email.
   - In *Authentication → URL Configuration*, set the Site URL to `https://lukeda.me/wordlist/` and add `http://localhost:8000/wordlist/` to the redirect URLs.
   - In *Authentication → Email Templates → Magic Link*, add `{{ .Token }}` to the body. The iPhone home-screen app can't receive the link's session, so you sign in there with the 6-digit code.
2. **Create the schema.** Paste `supabase/migrations/20261120000000_init.sql` and then `20261120130000_fsrs.sql` into the SQL editor and run them in that order. Alternatively, run `supabase link` and then `supabase db push`.
3. **Deploy the functions.** Install the Supabase CLI, then from `wordlist/` run:
   ```bash
   supabase secrets set ANTHROPIC_API_KEY=sk-ant-... ALLOWED_EMAIL=<your email> \
     ALLOWED_ORIGINS=https://lukeda.me,http://localhost:8000 \
     INGEST_TOKEN=$(openssl rand -hex 32) BACKUP_TOKEN=$(openssl rand -hex 32) \
     OWNER_USER_ID=<your user id from Authentication → Users>
   supabase functions deploy generate
   supabase functions deploy ingest --no-verify-jwt
   supabase functions deploy backup --no-verify-jwt
   ```
4. **Schedule backups.** Fill in `<PROJECT_REF>` and `<BACKUP_TOKEN>` in `supabase/cron.sql` and run it in the SQL editor. Don't commit the filled-in copy.
5. **Connect the app.** Put the project URL and the anon key (*Project Settings → API*) in `config.js`, commit and push. The Pages workflow publishes it.
6. **First run.** Open the app and sign in. "Import Deutsch C1" loads `seed-deutsch-c1.json` with all its progress.

When you change any shell file, bump `VERSION` in `sw.js` so installed copies update.

## iOS Shortcut (ingest)

Add a *Get Contents of URL* action with these settings:

- URL: `https://<PROJECT_REF>.supabase.co/functions/v1/ingest`
- Method: POST
- Header: `Authorization: Bearer <INGEST_TOKEN>`
- JSON body: `{ "deck": "Deutsch C1", "input": <Shortcut Input> }`

Cards arrive marked **Check** in the Deck tab. Opening and saving a card clears the mark.

## Spending

Each generation is logged in `ai_calls` (tokens in and out). Generations are capped at 20 per hour and inputs at 8,000 characters.

```sql
select date_trunc('day', created_at) d, count(*), sum(input_tokens) i, sum(output_tokens) o
from ai_calls group by 1 order by 1 desc;
```

## Tests

Open `tests.html` over http (for example `http://localhost:8000/wordlist/tests.html`). It covers the scheduler, the queue, lemmas, DB mapping, seed import, export round trips, drafts and the deck table. It also replays a 120-answer session on the seed through both this app's queue and the reference app's, and checks that the counts and the next card match at every step.
