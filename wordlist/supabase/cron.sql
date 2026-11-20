-- Weekly backup schedule (Sundays 03:00 UTC). Run once in the SQL editor AFTER deploying
-- the `backup` function. Replace the two placeholders first; never commit the real token.
create extension if not exists pg_cron;
create extension if not exists pg_net;

select cron.unschedule('wortschatz-weekly-backup')
where exists (select 1 from cron.job where jobname = 'wortschatz-weekly-backup');

select cron.schedule('wortschatz-weekly-backup', '0 3 * * 0', $$
  select net.http_post(
    url := 'https://<PROJECT_REF>.supabase.co/functions/v1/backup',
    headers := jsonb_build_object('Authorization', 'Bearer <BACKUP_TOKEN>', 'Content-Type', 'application/json'),
    body := '{}'::jsonb
  );
$$);
