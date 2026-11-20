import { createClient, type SupabaseClient } from "npm:@supabase/supabase-js@2.119.0";

const ORIGINS = (Deno.env.get("ALLOWED_ORIGINS") ?? "https://lukeda.me,http://localhost:8000")
  .split(",").map((s) => s.trim());

export function cors(req: Request): Record<string, string> {
  const origin = req.headers.get("origin") ?? "";
  return {
    "Access-Control-Allow-Origin": ORIGINS.includes(origin) ? origin : ORIGINS[0],
    "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type",
    "Access-Control-Allow-Methods": "POST, OPTIONS",
    "Vary": "Origin",
  };
}

export function json(req: Request, body: unknown, status = 200) {
  return new Response(JSON.stringify(body), { status, headers: { ...cors(req), "Content-Type": "application/json" } });
}

// Client that acts as the signed-in user, so RLS applies to everything it touches.
export async function userClient(req: Request): Promise<{ sb: SupabaseClient; userId: string } | null> {
  const auth = req.headers.get("Authorization");
  if (!auth) return null;
  const sb = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, {
    global: { headers: { Authorization: auth } },
    auth: { persistSession: false },
  });
  const { data, error } = await sb.auth.getUser();
  if (error || !data.user) return null;
  const allowed = Deno.env.get("ALLOWED_EMAIL");
  if (allowed && data.user.email?.toLowerCase() !== allowed.toLowerCase()) return null;
  return { sb, userId: data.user.id };
}

export function serviceClient(): SupabaseClient {
  return createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!, {
    auth: { persistSession: false },
  });
}

// Constant-time comparison for bearer tokens.
export function sameSecret(a: string, b: string) {
  const x = new TextEncoder().encode(a), y = new TextEncoder().encode(b);
  if (x.length !== y.length || !x.length) return false;
  let d = 0;
  for (let i = 0; i < x.length; i++) d |= x[i] ^ y[i];
  return d === 0;
}

export const RATE_LIMIT_PER_HOUR = 20;

export async function overRateLimit(sb: SupabaseClient, userId: string) {
  const since = new Date(Date.now() - 3600_000).toISOString();
  const { count } = await sb.from("ai_calls").select("id", { count: "exact", head: true })
    .eq("user_id", userId).gte("created_at", since);
  return (count ?? 0) >= RATE_LIMIT_PER_HOUR;
}

// Mirrors srs.js lemmaOf.
export function lemmaOf(term: string) {
  let s = String(term || "").trim().toLowerCase().replace(/\s+/g, " ");
  s = s.split(/ \/ |; |, /)[0];
  s = s.replace(/^(der|die|das)(\/(der|die|das))* /, "");
  s = s.replace(/^sich /, "").replace(/ sich$/, "");
  return s.trim();
}
