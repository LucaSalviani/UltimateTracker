-- UltimateTracker: tabla donde se juntan los datos de la PC y el celu.
-- Pegalo entero en Supabase → SQL Editor → New query → Run (una sola vez).

create table if not exists public.records (
  user_id    uuid    not null default auth.uid() references auth.users on delete cascade,
  kind       text    not null,          -- folder, pomodoro, interval, goal, day
  uid        text    not null,
  data       jsonb,
  updated_at bigint  not null,          -- hora del último cambio (ms): gana el más nuevo
  deleted    boolean not null default false,
  seq        bigserial,                 -- orden de llegada: cada aparato baja lo nuevo desde su último seq
  primary key (user_id, kind, uid)
);
create index if not exists records_user_seq on public.records (user_id, seq);

-- Cada usuario ve y toca solo lo suyo.
alter table public.records enable row level security;
drop policy if exists "leer lo mio" on public.records;
drop policy if exists "agregar lo mio" on public.records;
drop policy if exists "cambiar lo mio" on public.records;
create policy "leer lo mio"    on public.records for select using (user_id = auth.uid());
create policy "agregar lo mio" on public.records for insert with check (user_id = auth.uid());
create policy "cambiar lo mio" on public.records for update using (user_id = auth.uid()) with check (user_id = auth.uid());

-- Subir cambios: solo pisa lo que tiene si lo que llega es más nuevo.
create or replace function public.push_records(items jsonb)
returns void
language plpgsql
security invoker
set search_path = public
as $$
declare it jsonb;
begin
  for it in select * from jsonb_array_elements(items) loop
    insert into public.records as r (user_id, kind, uid, data, updated_at, deleted)
    values (auth.uid(), it->>'kind', it->>'uid', it->'data', (it->>'updated_at')::bigint, coalesce((it->>'deleted')::boolean, false))
    on conflict (user_id, kind, uid) do update
      set data = excluded.data,
          updated_at = excluded.updated_at,
          deleted = excluded.deleted,
          seq = nextval(pg_get_serial_sequence('public.records', 'seq'))
      where r.updated_at < excluded.updated_at;
  end loop;
end;
$$;

grant select, insert, update on public.records to authenticated;
grant usage, select on all sequences in schema public to authenticated;
grant execute on function public.push_records(jsonb) to authenticated;
