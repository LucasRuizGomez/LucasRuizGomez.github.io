-- FSRS-6 scheduler: each card direction stores its memory state.
--   stability   days until recall probability falls to 90%
--   difficulty  1 … 10
-- Both stay null for cards last scheduled by SM-2; srs.js converts them from
-- (ease, interval_days) the next time they are answered.

alter table card_states add column if not exists stability  numeric;
alter table card_states add column if not exists difficulty numeric;

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
  insert into card_states as cs (card_id, direction, user_id, state, step, interval_days, ease, stability, difficulty,
      due, reps, lapses, correct, wrong, last_review, introduced_at)
  values (p_card, p_direction, uid, p_next->>'state', (p_next->>'step')::int,
      (p_next->>'interval_days')::numeric, (p_next->>'ease')::numeric,
      (p_next->>'stability')::numeric, (p_next->>'difficulty')::numeric, (p_next->>'due')::timestamptz,
      (p_next->>'reps')::int, (p_next->>'lapses')::int, (p_next->>'correct')::int, (p_next->>'wrong')::int,
      (p_next->>'last_review')::timestamptz, (p_next->>'introduced_at')::timestamptz)
  on conflict (card_id, direction) do update set
      state = excluded.state, step = excluded.step, interval_days = excluded.interval_days,
      ease = excluded.ease, stability = excluded.stability, difficulty = excluded.difficulty,
      due = excluded.due, reps = excluded.reps, lapses = excluded.lapses,
      correct = excluded.correct, wrong = excluded.wrong, last_review = excluded.last_review,
      introduced_at = coalesce(cs.introduced_at, excluded.introduced_at)
    where cs.last_review is null or excluded.last_review >= cs.last_review;
  insert into review_log (user_id, card_id, direction, grade, state_before, interval_after, reviewed_at, client_id)
  values (uid, p_card, p_direction, p_grade, p_state_before, (p_next->>'interval_days')::numeric,
      coalesce((p_next->>'last_review')::timestamptz, now()), p_client_id)
  on conflict (client_id) do nothing;
end $$;

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
      insert into card_states (card_id, direction, user_id, state, step, interval_days, ease, stability, difficulty,
          due, reps, lapses, correct, wrong, last_review, introduced_at)
      values (cid, s->>'direction', uid, s->>'state', (s->>'step')::int, (s->>'interval_days')::numeric,
          (s->>'ease')::numeric, (s->>'stability')::numeric, (s->>'difficulty')::numeric,
          (s->>'due')::timestamptz, (s->>'reps')::int, (s->>'lapses')::int,
          (s->>'correct')::int, (s->>'wrong')::int, (s->>'last_review')::timestamptz,
          (s->>'introduced_at')::timestamptz);
    end loop;
    cid := null;
  end loop;
  return jsonb_build_object('deck_id', did, 'inserted', inserted, 'skipped', to_jsonb(skipped));
end $$;
