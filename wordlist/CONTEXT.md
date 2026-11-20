# Wortschatz: contexto para reconstruir la app en lukeda.me

Este documento es el contexto completo para que un agente de código (Claude Code u otro) reconstruya la app de flashcards en mi web, **lukeda.me**. Úsalo como `CLAUDE.md` del repo o pégalo como primer mensaje.

Archivos que lo acompañan:
- `reference-index.html`: la versión actual de la app, en un único HTML. Es la fuente de verdad para el algoritmo de repaso, la UI y los textos.
- `seed-deutsch-c1.json`: export del mazo actual, con 28 tarjetas y todo el progreso a 20-11-2026. Hay que importarlo al arrancar.

---

## 1. Qué es

Una app personal tipo Anki para preparar vocabulario del examen C1 de alemán. Tiene **un solo usuario** (yo).

Ahora mismo vive como artifact en claude.ai y guarda los datos en una base de datos de ese artifact. El objetivo es alojarla en mi dominio, por ejemplo en `lukeda.me/wortschatz` o en `vocab.lukeda.me`. Tiene que ser instalable en el móvil como PWA y conservar todo el progreso.

## 2. Funcionalidades

### 2.1 Lo que ya existe (hay que mantenerlo igual)
- **Estudio tarjeta a tarjeta.** Se ve el anverso, se pulsa "Show answer" (o Espacio) y se puntúa con **Again / Hard / Good / Easy** (teclas 1-4). Cada botón muestra el próximo intervalo.
- **Dos direcciones por palabra.** Primero Alemán → Inglés (`f`). La inversa Inglés → Alemán (`r`) se desbloquea cuando la tarjeta `f` gradúa al estado `review`.
- **Contenido del reverso:** traducción, formas (artículo y plural, o los tiempos del verbo), uso (régimen, preposiciones, colocaciones), una frase de ejemplo de nivel C1 y su traducción.
- **Botón 🔊** que lee en voz alta la palabra y la frase de ejemplo en alemán. En la versión nueva se usa la Web Speech API (`speechSynthesis`, voz `de-DE`).
- **Contadores tipo Anki:** New (azul), Learning (rojo), Due (verde). Límite de 20 tarjetas nuevas al día. El día empieza a las 04:00 hora local.
- **Pantalla "Fertig für heute!"** cuando no queda nada. Indica cuándo vence la siguiente tarjeta y ofrece **práctica extra**, que no toca el calendario de repasos.
- **Pestaña Deck:** tabla con palabra, significado, estado y aciertos/fallos. Se puede ordenar por palabra, por estado o por número de fallos.
- Modo claro y oscuro automático, diseño responsive y uso cómodo en el móvil.

### 2.2 Funcionalidades nuevas
1. **Persistencia en servidor.** Las palabras, los mazos y el progreso se guardan en una base de datos y se sincronizan entre el móvil y el PC. Nada depende de `localStorage` salvo como caché.
2. **Varios mazos.**
   - Crear, renombrar, archivar y borrar mazos (borrar pide confirmación).
   - Cada mazo tiene su idioma de origen y de destino (por defecto `de` → `en`) y su límite de tarjetas nuevas al día.
   - Un selector de mazo en la cabecera, con los contadores de cada mazo. Debe poder estudiarse un mazo concreto o "Todos".
   - Mover una tarjeta de un mazo a otro.
3. **Añadir palabras con Claude.**
   - Una pantalla "Añadir palabras" con un textarea donde pego texto libre: palabras sueltas, `palabra: definición`, formas flexionadas (`Zorns`, `fordert`, `Vorlieben`), frases o listas con viñetas.
   - El servidor llama a la API de Anthropic y devuelve tarjetas estructuradas.
   - Las tarjetas aparecen en una **vista previa editable**, donde puedo editar o descartar cada una antes de guardar.
   - Claude debe:
     - normalizar cada palabra a su lema;
     - avisar de los duplicados que ya estén en el mazo;
     - avisar de las palabras que no existen y proponer la correcta (ejemplo real: `verrappeln` → `sich aufrappeln`).
   - Opcional (fase 4): un endpoint con token personal para mandar palabras desde un Atajo de iOS o desde otro sitio.
4. **Importar y exportar en JSON**, con el mismo formato que `seed-deutsch-c1.json`.
5. **PWA:** `manifest.json`, iconos, `display: standalone` y un service worker que cachee la app shell. Que se pueda "Añadir a pantalla de inicio" y se abra a pantalla completa.

## 3. Stack recomendado

Si lukeda.me ya usa otro stack, adáptalo. Lo que no se puede cambiar es el modelo de datos, el algoritmo y el contrato de la API de generación.

- **Frontend y backend:** Next.js (App Router, TypeScript), con despliegue en Vercel y conectado al dominio lukeda.me.
- **Base de datos y autenticación:** Supabase.
  - Postgres con Row Level Security.
  - Auth con magic link a mi email. Hay que desactivar el registro público o restringirlo a mi email.
- **IA:** la API de Anthropic, llamada **solo desde el servidor** con el SDK `@anthropic-ai/sdk` desde un route handler. La clave `ANTHROPIC_API_KEY` va únicamente en variables de entorno del servidor.
- **Estilos:** CSS propio o Tailwind, pero hay que reproducir el look actual (ver los tokens en la sección 8).

## 4. Modelo de datos (Postgres / Supabase)

```sql
create table decks (
  id            uuid primary key default gen_random_uuid(),
  user_id       uuid not null references auth.users on delete cascade,
  name          text not null,
  source_lang   text not null default 'de',
  target_lang   text not null default 'en',
  new_per_day   int  not null default 20,
  archived      boolean not null default false,
  created_at    timestamptz not null default now()
);

create table cards (
  id                   uuid primary key default gen_random_uuid(),
  user_id              uuid not null references auth.users on delete cascade,
  deck_id              uuid not null references decks on delete cascade,
  position             int  not null,             -- orden de alta (antes "n")
  term                 text not null,             -- "der Vorreiter", "sich widersetzen"
  lemma                text not null,             -- "vorreiter", normalizado: sin artículo/sich, minúsculas
  pos                  text,                      -- "noun", "verb (separable)", ...
  forms                text,
  meaning              text not null,
  usage                text,
  example              text,
  example_translation  text,
  user_input           text,                      -- lo que escribí yo, por trazabilidad
  created_at           timestamptz not null default now(),
  unique (deck_id, lemma)
);

create table card_states (
  card_id        uuid not null references cards on delete cascade,
  direction      char(1) not null check (direction in ('f','r')),
  user_id        uuid not null references auth.users on delete cascade,
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

create table review_log (
  id           bigserial primary key,
  user_id      uuid not null references auth.users on delete cascade,
  card_id      uuid not null references cards on delete cascade,
  direction    char(1) not null,
  grade        smallint not null check (grade between 1 and 4),
  state_before text not null,
  interval_after numeric not null,
  reviewed_at  timestamptz not null default now()
);
```

- Activa la RLS en todas las tablas, con la política `user_id = auth.uid()` para select, insert, update y delete.
- Índices: `cards(deck_id)`, `card_states(user_id, due)` y `review_log(user_id, reviewed_at)`.
- Si una fila de `card_states` no existe, la tarjeta se trata como `new`. Se crea al primer repaso.

## 5. Algoritmo de repaso (copiar tal cual)

Es el mismo que usa `reference-index.html`. Hay que portarlo a TypeScript compartido entre el cliente y el servidor, y **cubrirlo con tests unitarios**.

```ts
const MIN = 60_000, DAY = 86_400_000;
const STEPS = [1, 10];                 // pasos de aprendizaje, en minutos
const LEARN_AHEAD = 20 * MIN;

type State = { state: 'new'|'learning'|'review'|'relearning'; step: number; interval: number;
  ease: number; due: number; reps: number; lapses: number; correct: number; wrong: number;
  last: number; introduced: number };

function schedule(p: State, g: 1|2|3|4, t = Date.now()): State {
  const s = { ...p }; s.reps++; s.last = t;
  if (!s.introduced) s.introduced = t;
  if (g === 1) s.wrong++; else s.correct++;
  if (s.state !== 'review') {
    const relearn = s.state === 'relearning';
    if (s.state === 'new') s.state = 'learning';
    if (g === 1) { s.step = 0; s.due = t + (relearn ? 10 : STEPS[0]) * MIN; }
    else if (g === 2) { s.due = t + (relearn ? 10 : (s.step === 0 ? 6 : STEPS[s.step])) * MIN; }
    else if (g === 3) {
      if (!relearn && s.step + 1 < STEPS.length) { s.step++; s.due = t + STEPS[s.step] * MIN; }
      else { s.state = 'review'; s.step = 0; s.interval = relearn ? Math.max(1, s.interval) : 1; s.due = t + s.interval * DAY; }
    } else { s.state = 'review'; s.step = 0; s.interval = relearn ? Math.max(1, s.interval) + 1 : 4; s.due = t + s.interval * DAY; }
  } else {
    if (g === 1) { s.lapses++; s.ease = Math.max(1.3, s.ease - 0.2); s.interval = Math.max(1, Math.round(s.interval * 0.5)); s.state = 'relearning'; s.step = 0; s.due = t + 10 * MIN; }
    else if (g === 2) { s.ease = Math.max(1.3, s.ease - 0.15); s.interval = Math.max(s.interval + 1, Math.round(s.interval * 1.2)); s.due = t + s.interval * DAY; }
    else if (g === 3) { s.interval = Math.max(s.interval + 1, Math.round(s.interval * s.ease)); s.due = t + s.interval * DAY; }
    else { s.ease += 0.15; s.interval = Math.max(s.interval + 2, Math.round(s.interval * s.ease * 1.3)); s.due = t + s.interval * DAY; }
  }
  return s;
}
```

**Cola de estudio.** En cada paso se elige la siguiente tarjeta con estas reglas:
1. Primero, las tarjetas en learning o relearning con `due <= now`, empezando por la más antigua.
2. Después, las de review con `due <= now`.
3. Después, las nuevas: hasta `new_per_day` menos las introducidas hoy (desde las 04:00). Van primero las `f`, por `position`, y luego las `r`.
4. Después, las de learning que vencen en menos de 20 minutos (estudio adelantado).
5. Si no queda ninguna, se muestra la pantalla de "terminado".

Además:
- Siempre que haya otra opción, se evita repetir la tarjeta que se acaba de responder.
- En "Todos los mazos" el límite de nuevas se aplica por mazo.
- La práctica extra baraja todas las tarjetas, no guarda nada, y las falladas vuelven a la cola tres posiciones más atrás.

**Guardado.** Cada respuesta hace un upsert en `card_states` y un insert en `review_log`.
- La UI es optimista y la escritura se encola.
- Si falla, se muestra un aviso discreto y se reintenta.
- Sin conexión, las respuestas se guardan en IndexedDB y se sincronizan al volver la red.

## 6. Integración con Claude (añadir palabras)

### 6.1 Flujo
1. `POST /api/cards/generate` con el body `{ deckId, input }`. `input` es el texto libre, hasta unos 8.000 caracteres.
2. El servidor carga el mazo (idiomas y lemas existentes) y llama a la API de Anthropic con **tool use forzado**, para que la respuesta sea JSON válido según un esquema.
3. Devuelve `{ cards: DraftCard[], notes: string[] }` y no guarda nada todavía.
4. El cliente muestra la vista previa editable. Cada tarjeta tiene sus avisos (duplicado, corregida, forma flexionada normalizada) y una casilla para incluirla o no.
5. `POST /api/cards/bulk` con `{ deckId, cards }` las inserta con `position` consecutiva. Si choca con `unique(deck_id, lemma)`, la tarjeta se omite y se informa.

### 6.2 Llamada a la API

- **Modelo:** `claude-sonnet-5-5`, por la calidad del alemán. Se puede usar `claude-haiku-5-5` si quiero abaratar.
- **Parámetros:** `max_tokens: 8000` y `temperature: 0.3`.
- **Tool use:** `tools: [saveCardsTool]` y `tool_choice: { type: "tool", name: "save_cards" }`.

```ts
const saveCardsTool = {
  name: "save_cards",
  description: "Return the flashcards built from the user's input.",
  input_schema: {
    type: "object",
    properties: {
      cards: { type: "array", items: {
        type: "object",
        properties: {
          term:   { type: "string", description: "Dictionary form with article for nouns, 'sich' for reflexive verbs" },
          lemma:  { type: "string", description: "Lowercase, no article, no 'sich', umlauts kept" },
          pos:    { type: "string" },
          forms:  { type: "string" },
          meaning:{ type: "string" },
          usage:  { type: "string" },
          example:{ type: "string" },
          example_translation: { type: "string" },
          user_input: { type: "string", description: "The exact item the user wrote" },
          flags:  { type: "array", items: { type: "string",
                    enum: ["normalized", "corrected", "duplicate", "uncertain"] } },
          flag_note: { type: "string" }
        },
        required: ["term","lemma","pos","meaning","example","example_translation","user_input","flags"]
      }},
      notes: { type: "array", items: { type: "string" } }
    },
    required: ["cards"]
  }
};
```

**Prompt de sistema.** Se rellenan `{source_lang}`, `{target_lang}` y `{existing_lemmas}`:

```
You build vocabulary flashcards for a learner preparing for a C1 exam in {source_lang}.
Meanings and translations are written in {target_lang}.

For every item in the user's input:
- Reduce it to its dictionary form (lemma). Inflected forms (genitive, plural, conjugated verbs,
  zu-infinitives) become the base form; add flag "normalized" and say what you changed in flag_note.
- If the word does not exist or is misspelled, pick the most likely intended word, add flag
  "corrected" and explain in flag_note. If you can't tell, add flag "uncertain".
- If the lemma is in this list of existing cards, add flag "duplicate": {existing_lemmas}
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
```

### 6.3 Seguridad y costes
- La clave nunca llega al cliente. Las rutas comprueban que la sesión es mía y devuelven 401 en caso contrario.
- Límite de 20 llamadas por hora por usuario y tope de `input` de unos 8.000 caracteres.
- Cada llamada se registra en una tabla `ai_calls` (tokens de entrada y salida) para poder ver el gasto.
- **Fase 4 (opcional):** `POST /api/ingest` con un token personal en la cabecera `Authorization: Bearer <INGEST_TOKEN>` y el body `{ deck: "Deutsch C1", input }`. Ejecuta generate y bulk directamente, sin vista previa, y marca las tarjetas como `needs_review`. Sirve para un Atajo de iOS o para enviar palabras desde otro sitio.

## 7. API (route handlers)

| Método | Ruta | Qué hace |
|---|---|---|
| GET | `/api/decks` | Lista los mazos con sus contadores (new, learning, due, total) |
| POST | `/api/decks` | Crea un mazo `{ name, source_lang, target_lang, new_per_day }` |
| PATCH / DELETE | `/api/decks/:id` | Renombra o cambia ajustes / borra (en cascada) |
| GET | `/api/decks/:id/cards` | Devuelve tarjetas y estados (para estudiar y para la tabla) |
| POST | `/api/cards/generate` | Llama a Claude y devuelve borradores (sección 6) |
| POST | `/api/cards/bulk` | Inserta tarjetas |
| PATCH / DELETE | `/api/cards/:id` | Edita, mueve de mazo o borra |
| POST | `/api/reviews` | Guarda `{ cardId, direction, grade, nextState }` (upsert del estado + log) |
| GET / POST | `/api/export`, `/api/import` | JSON en el formato del seed |

Con Supabase, parte de esto puede ir directamente desde el cliente con RLS. Las rutas de IA, import y export se quedan en el servidor.

## 8. UI y diseño

- **Pantallas:**
  - **Study:** la que hay ahora.
  - **Deck:** la tabla, con búsqueda, edición inline, borrado y mover de mazo.
  - **Añadir palabras:** textarea, botón "Generar con Claude", vista previa y botón "Guardar N tarjetas".
  - **Mazos:** lista, crear y ajustes.
  - **Ajustes:** exportar/importar, tarjetas nuevas por día y cerrar sesión.
- **Cabecera:** "Deutsch C1 · Wortschatz", selector de mazo y pestañas.
- **Tipografía:** Inter para la interfaz y Source Serif 4 para las palabras y los ejemplos.
- **Colores (claro / oscuro):**
  - Fondo `#f6f5f2` / `#161618`
  - Panel `#fff` / `#1f1f22`
  - Texto `#1d1d1f` / `#ececee`
  - Atenuado `#6b6b70` / `#9a9aa1`
  - Línea `#e4e2dc` / `#2e2e33`
  - Azul `#2f6fde` / `#6b9cff`
  - Rojo `#d4443b` / `#ff6b61`
  - Verde `#2e9d5b` / `#4fc47f`
  - Ámbar `#c98a12` / `#e6a93a`
- **Tarjeta:** radio de 16 px, sombra suave y altura mínima de 340 px. La palabra del anverso va a 38 px en serif.
- **Botones de nota:** borde inferior de color (Again rojo, Hard ámbar, Good verde, Easy azul) con el intervalo debajo.
- **Móvil:**
  - Botones grandes y fijos en la parte inferior, respetando `env(safe-area-inset-bottom)`.
  - Tocar la tarjeta le da la vuelta.
  - Opcional: deslizar a la izquierda = Again y a la derecha = Good.
- **Teclado:** Espacio o Enter da la vuelta. Con la tarjeta girada, 1-4 puntúan y Espacio cuenta como Good.

## 9. Migración de los datos actuales

Se importa `seed-deutsch-c1.json` en un mazo nuevo llamado "Deutsch C1" (`de` → `en`).

Correspondencia de campos de las tarjetas:

| JSON | Base de datos |
|---|---|
| `id` | `lemma` (sin las umlauts, como `quaelen` o `schluepfer`; recalcula el lemma desde `term`) |
| `n` | `position` |
| `de` | `term` |
| `en` | `meaning` |
| `pos`, `forms`, `usage` | igual |
| `example` | `example` |
| `exampleEn` | `example_translation` |
| `added` | `created_at` |

Correspondencia del progreso:
- La clave `"<id>__f"` o `"<id>__r"` da la `direction`.
- `due`, `last` e `introduced` están en ms epoch y pasan a `timestamptz`.
- `interval` pasa a `interval_days`.
- El resto de campos se mantiene igual.

## 10. Plan por fases y criterios de aceptación

1. **Fase 1 (MVP):**
   - Auth, un mazo, estudio con el algoritmo portado e importación del seed.
   - *Hecho cuando* tenga los mismos contadores y la misma tarjeta siguiente que la versión actual con los mismos datos, y el progreso se vea igual en el móvil y en el PC.
2. **Fase 2:**
   - Varios mazos, CRUD de tarjetas, export/import y PWA instalable.
   - *Hecho cuando* pueda crear el mazo "Redemittel C1", añadir tarjetas a mano y estudiarlo por separado.
3. **Fase 3:**
   - Añadir palabras con Claude, con vista previa y detección de duplicados.
   - *Hecho cuando* al pegar la lista del 19-11, con `Zorns`, `fordert`, `verrappeln` y `Urteil` (que ya existe), salgan los lemas normalizados, `verrappeln` aparezca corregido a `sich aufrappeln` con su aviso y `Urteil` aparezca marcado como duplicado.
4. **Fase 4 (opcional):**
   - Endpoint `/api/ingest` con token, estadísticas (racha, repasos por día y palabras más falladas) y audio TTS mejor.

## 11. Notas
- Todo es para un solo usuario. Hay que priorizar que sea simple y fiable antes que escalable.
- No se pueden perder datos: confirmar antes de borrar y hacer backup semanal automático, exportando el JSON a Supabase Storage.
- Interfaz en inglés y alemán, como ahora. Los textos de ayuda pueden estar en español.
