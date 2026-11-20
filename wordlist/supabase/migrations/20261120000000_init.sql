-- Wortschatz schema. Run once in the Supabase SQL editor (or `supabase db push`).
-- Data model from CONTEXT.md §4, plus:
--   cards.needs_review    marks cards added by /ingest without a preview
--   review_log.client_id  makes offline retries idempotent
--   ai_calls              token usage per Claude call (cost tracking, rate limit)

create extension if not exists pgcrypto;

create table if not exists decks (
  id            uuid primary key default gen_random_uuid(),
  user_id       uuid not null default auth.uid() references auth.users on delete cascade,
  name          text not null,
  source_lang   text not null default 'de',
  target_lang   text not null default 'en',
  new_per_day   int  not null default 20 check (new_per_day between 0 and 9999),
  archived      boolean not null default false,
  created_at    timestamptz not null default now()
);

create table if not exists cards (
  id                   uuid primary key default gen_random_uuid(),
  user_id              uuid not null default auth.uid() references auth.users on delete cascade,
  deck_id              uuid not null references decks on delete cascade,
  position             int  not null,
  term                 text not null,
  lemma                text not null,
  pos                  text,
  forms                text,
  meaning              text not null,
  usage                text,
  example              text,
  example_translation  text,
  user_input           text,
  needs_review         boolean not null default false,
  created_at           timestamptz not null default now(),
  unique (deck_id, lemma)
);

create table if not exists card_states (
  card_id        uuid not null references cards on delete cascade,
  direction      char(1) not null check (direction in ('f','r')),
  user_id        uuid not null default auth.uid() references auth.users on delete cascade,
  state          text not null default 'new'
                 check (state in ('new','learning','review','relearning')),
  step           int not null default 0,
  interval_days  numeric not null default 0,
  ease           numeric not null default 2.5,
  due            timestamptz not null default now(),
  reps           int not null default 0,
  lapses         int not null default 0,
  correct        int not null default 0,
  wrong          int not null default 0,
  last_review    timestamptz,
  introduced_at  timestamptz,
  primary key (card_id, direction)
);

create table if not exists review_log (
  id             bigserial primary key,
  user_id        uuid not null default auth.uid() references auth.users on delete cascade,
  card_id        uuid not null references cards on delete cascade,
  direction      char(1) not null,
  grade          smallint not null check (grade between 1 and 4),
  state_before   text not null,
  interval_after numeric not null,
  reviewed_at    timestamptz not null default now(),
  client_id      uuid unique
);

create table if not exists ai_calls (
  id             bigserial primary key,
  user_id        uuid not null default auth.uid() references auth.users on delete cascade,
  route          text not null,
  model          text not null,
  input_tokens   int not null default 0,
  output_tokens  int not null default 0,
  ok             boolean not null default true,
  created_at     timestamptz not null default now()
);

create index if not exists cards_deck_idx        on cards (deck_id);
create index if not exists card_states_due_idx   on card_states (user_id, due);
create index if not exists review_log_user_idx   on review_log (user_id, reviewed_at);
create index if not exists ai_calls_user_idx     on ai_calls (user_id, created_at);

-- Row Level Security: every row belongs to its user.
do $$
declare t text;
begin
  foreach t in array array['decks','cards','card_states','review_log','ai_calls'] loop
    execute format('alter table %I enable row level security', t);
    execute format('drop policy if exists own_rows on %I', t);
    execute format('create policy own_rows on %I for all to authenticated using (user_id = auth.uid()) with check (user_id = auth.uid())', t);
  end loop;
end $$;

-- A card can only point at a deck of the same user (RLS checks user_id, not the FK target).
create or replace function cards_same_owner() returns trigger language plpgsql as $$
begin
  if not exists (select 1 from decks d where d.id = new.deck_id and d.user_id = new.user_id) then
    raise exception 'deck not found';
  end if;
  return new;
end $$;
drop trigger if exists cards_same_owner on cards;
create trigger cards_same_owner before insert or update of deck_id, user_id on cards
  for each row execute function cards_same_owner();

-- One answer: upsert the state and append to the log, atomically.
-- An older answer (e.g. synced late from an offline phone) never overwrites a newer one.
create or replace function record_review(
  p_card uuid, p_direction char, p_grade smallint, p_state_before text,
  p_next jsonb, p_client_id uuid default null
) returns void language plpgsql security invoker as $$
declare uid uuid := auth.uid();
begin
  if uid is null then raise exception 'not signed in'; end if;
  if p_client_id is not null and exists (select 1 from review_log where client_id = p_client_id) then
    return;  -- already recorded (retry)
  end if;
  insert into card_states as cs (card_id, direction, user_id, state, step, interval_days, ease, due,
      reps, lapses, correct, wrong, last_review, introduced_at)
  values (p_card, p_direction, uid, p_next->>'state', (p_next->>'step')::int,
      (p_next->>'interval_days')::numeric, (p_next->>'ease')::numeric, (p_next->>'due')::timestamptz,
      (p_next->>'reps')::int, (p_next->>'lapses')::int, (p_next->>'correct')::int, (p_next->>'wrong')::int,
      (p_next->>'last_review')::timestamptz, (p_next->>'introduced_at')::timestamptz)
  on conflict (card_id, direction) do update set
      state = excluded.state, step = excluded.step, interval_days = excluded.interval_days,
      ease = excluded.ease, due = excluded.due, reps = excluded.reps, lapses = excluded.lapses,
      correct = excluded.correct, wrong = excluded.wrong, last_review = excluded.last_review,
      introduced_at = coalesce(cs.introduced_at, excluded.introduced_at)
    where cs.last_review is null or excluded.last_review >= cs.last_review;
  insert into review_log (user_id, card_id, direction, grade, state_before, interval_after, reviewed_at, client_id)
  values (uid, p_card, p_direction, p_grade, p_state_before, (p_next->>'interval_days')::numeric,
      coalesce((p_next->>'last_review')::timestamptz, now()), p_client_id)
  on conflict (client_id) do nothing;
end $$;

-- Import a wortschatz-export/v1 file (already mapped by srs.js parseExport) in one transaction.
-- p = { deck: {name, source_lang, target_lang, new_per_day}, cards: [{lemma, position, term, ..., states: [...]}] }
-- p_deck: import into this existing deck instead of creating a new one.
-- Returns { deck_id, inserted, skipped: [lemma, ...] }.
create or replace function import_deck(p jsonb, p_deck uuid default null)
returns jsonb language plpgsql security invoker as $$
declare
  uid uuid := auth.uid();
  did uuid := p_deck;
  base int := 0;
  c jsonb; s jsonb; cid uuid;
  inserted int := 0;
  skipped text[] := '{}';
begin
  if uid is null then raise exception 'not signed in'; end if;
  if did is null then
    insert into decks (user_id, name, source_lang, target_lang, new_per_day)
    values (uid, coalesce(p->'deck'->>'name', 'Imported'), coalesce(p->'deck'->>'source_lang', 'de'),
            coalesce(p->'deck'->>'target_lang', 'en'), coalesce((p->'deck'->>'new_per_day')::int, 20))
    returning id into did;
  else
    if not exists (select 1 from decks where id = did) then raise exception 'deck not found'; end if;
    select coalesce(max(position), 0) into base from cards where deck_id = did;
  end if;

  for c in select * from jsonb_array_elements(coalesce(p->'cards', '[]'::jsonb)) loop
    insert into cards (user_id, deck_id, position, term, lemma, pos, forms, meaning, usage, example,
        example_translation, user_input, created_at)
    values (uid, did, base + coalesce((c->>'position')::int, 0), c->>'term', c->>'lemma', c->>'pos', c->>'forms',
        c->>'meaning', c->>'usage', c->>'example', c->>'example_translation', c->>'user_input',
        coalesce((c->>'created_at')::timestamptz, now()))
    on conflict (deck_id, lemma) do nothing
    returning id into cid;
    if cid is null then
      skipped := skipped || (c->>'lemma');
      continue;
    end if;
    inserted := inserted + 1;
    for s in select * from jsonb_array_elements(coalesce(c->'states', '[]'::jsonb)) loop
      insert into card_states (card_id, direction, user_id, state, step, interval_days, ease, due, reps,
          lapses, correct, wrong, last_review, introduced_at)
      values (cid, s->>'direction', uid, s->>'state', (s->>'step')::int, (s->>'interval_days')::numeric,
          (s->>'ease')::numeric, (s->>'due')::timestamptz, (s->>'reps')::int, (s->>'lapses')::int,
          (s->>'correct')::int, (s->>'wrong')::int, (s->>'last_review')::timestamptz,
          (s->>'introduced_at')::timestamptz);
    end loop;
    cid := null;
  end loop;
  return jsonb_build_object('deck_id', did, 'inserted', inserted, 'skipped', to_jsonb(skipped));
end $$;

-- Weekly backups land here (private bucket, written by the backup function with the service key).
insert into storage.buckets (id, name, public) values ('backups', 'backups', false)
on conflict (id) do nothing;
