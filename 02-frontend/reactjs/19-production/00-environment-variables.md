# Environment Variables

An app behaves differently in different places: it talks to `localhost:3000` in development, `api.staging.example.com` in staging, and `api.example.com` in production. **Environment variables** are how you keep that configuration **out of your code** so the same code runs everywhere.

In a frontend app there's a catch that surprises almost everyone:

> **Frontend environment variables are not secret.** They're substituted into your JavaScript bundle at build time, and anyone can read your bundle.

## How Vite handles env vars

Vite loads variables from `.env` files and exposes the ones starting with **`VITE_`** to your code:

```bash
# .env
VITE_API_URL=http://localhost:3000/api
VITE_APP_NAME=Acme
DB_PASSWORD=hunter2          # NOT exposed: no VITE_ prefix
```

```ts
console.log(import.meta.env.VITE_API_URL)    // "http://localhost:3000/api"
console.log(import.meta.env.DB_PASSWORD)     // undefined: the prefix protects accidental leaks
```

The `VITE_` prefix is a **safety rail**: variables without it stay out of the client bundle. It's not encryption. **Anything with the prefix is public.**

### Built-in variables

```ts
import.meta.env.MODE        // "development" | "production" | custom mode
import.meta.env.DEV         // true in dev
import.meta.env.PROD        // true in production builds
import.meta.env.BASE_URL    // the `base` config
import.meta.env.SSR         // true when server-rendered
```

Use `DEV`/`PROD` to guard dev-only code; the bundler removes dead branches in production ([bundle optimization](../14-performance/05-bundle-optimization.md#build-configuration)).

## `.env` files and precedence

```text
.env                  loaded in every mode
.env.local            every mode, ignored by git (your machine's overrides)
.env.development      only for `vite` (dev server)
.env.production       only for `vite build`
.env.[mode]           only for `--mode <mode>` (e.g. staging)
.env.[mode].local     that mode, ignored by git
```

More specific files win: `.env.production.local` > `.env.production` > `.env.local` > `.env`. Variables already set in the **real shell environment** (such as in CI) take priority over files.

### What goes in git

- **Commit** `.env` (non-secret defaults) and a **`.env.example`** documenting every variable with placeholder values, so new developers know what to set.
- **Never commit** `*.local` files or anything containing real credentials. Add to `.gitignore`:

```gitignore
.env.local
.env.*.local
```

### Modes

A **mode** selects which `.env.[mode]` file loads. `vite` runs in `development`, `vite build` in `production`. Add your own for staging:

```bash
vite build --mode staging       # loads .env.staging
```

Mode (which env files load) is separate from `NODE_ENV` (which optimizations apply). A `staging` build still sets `NODE_ENV=production` unless you override it.

## Types

Without help, `import.meta.env.VITE_X` is typed `any`. Declare your variables:

```ts
// src/vite-env.d.ts
/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_API_URL: string
  readonly VITE_SENTRY_DSN?: string
  readonly VITE_FEATURE_NEW_CHECKOUT?: "true" | "false"
}

interface ImportMeta {
  readonly env: ImportMetaEnv
}
```

Remember **env values are always strings**. `"false"` is truthy:

```ts
if (import.meta.env.VITE_FEATURE_NEW_CHECKOUT) { … }             // ✗ "false" is truthy
if (import.meta.env.VITE_FEATURE_NEW_CHECKOUT === "true") { … }  // ✓
const timeout = Number(import.meta.env.VITE_TIMEOUT_MS ?? 5000)  // convert numbers
```

## Validate at startup

A missing variable should fail **loudly and immediately**, not as `undefined/api/projects` buried in a network error:

```ts
// src/lib/env.ts
import { z } from "zod"

const schema = z.object({
  VITE_API_URL: z.string().min(1),
  VITE_SENTRY_DSN: z.string().optional(),
  VITE_ENABLE_DEVTOOLS: z.enum(["true", "false"]).default("false"),
})

const parsed = schema.safeParse(import.meta.env)
if (!parsed.success) {
  throw new Error(`Invalid environment configuration:\n${parsed.error.message}`)
}

export const env = {
  apiUrl: parsed.data.VITE_API_URL,
  sentryDsn: parsed.data.VITE_SENTRY_DSN,
  enableDevtools: parsed.data.VITE_ENABLE_DEVTOOLS === "true",
}
```

Import `env` everywhere instead of touching `import.meta.env` directly. One place to validate, convert, and rename. (Zod's error API varies slightly by version.)

## Secret vs public configuration

This distinction decides what's safe to put in `VITE_*`:

| Public (fine in the bundle) | Secret (**never** in the bundle) |
|---|---|
| API base URL | Database passwords |
| App name, feature flags | Private API keys (Stripe **secret** key, AWS secret) |
| Sentry **DSN** (designed to be public) | JWT signing secrets |
| Stripe **publishable** key | OAuth client **secrets** |
| Analytics IDs, Google Maps key restricted by domain | Admin/service tokens |

Rule: **if leaking it would let someone do damage, it can't be in the frontend.** Ever. Not obfuscated, not base64-encoded, not "hidden" in a variable name. They're all readable in DevTools or a `curl` of your JS.

### How to use a secret anyway

Keep it on a **server you control** and let the frontend call *that*:

```text
Browser ──► your backend / serverless function (holds the secret) ──► third-party API
```

The browser sends a request; your server adds the API key and forwards it. This is also how you hide third-party keys from rate-limit abuse. Even "public" keys (maps, analytics) should be **restricted** at the provider (allowed domains, quotas, scoped permissions).

## Build-time vs runtime configuration

Vite **replaces** `import.meta.env.VITE_X` with the literal value **during the build**. The bundle contains `"https://api.staging.example.com"` as a string. Consequences:

- **Changing a value requires a rebuild.** Editing an env var on the host after deployment does nothing.
- **One build per environment.** The staging build and the production build are *different artifacts*, so what you tested isn't byte-for-byte what you ship.

For many teams that's fine: CI builds per environment. But it conflicts with the principle **"build once, deploy many"**: build a single artifact, test it, then *promote* the same artifact from staging to production with different config.

### Runtime configuration

To get there, load configuration **when the page loads**, not when it's built.

**Option A: a config file served next to the app**

```html
<!-- index.html -->
<script src="/config.js"></script>      <!-- must load BEFORE the app bundle -->
<script type="module" src="/src/main.tsx"></script>
```

```js
// public/config.js (replaced per environment at deploy/container start)
window.__CONFIG__ = { API_URL: "https://api.example.com", ENV: "production" }
```

```ts
// src/lib/config.ts
declare global { interface Window { __CONFIG__?: { API_URL: string; ENV: string } } }
export const config = window.__CONFIG__ ?? { API_URL: import.meta.env.VITE_API_URL, ENV: import.meta.env.MODE }
```

The deploy step (or a container entrypoint, see [Docker](./03-docker.md#runtime-configuration)) writes the right `config.js`. Serve it with **`Cache-Control: no-cache`** so changes take effect immediately.

**Option B: fetch config at startup** from `/config.json` or your API before rendering. This costs an extra request on every load (and you need to handle failure), but it avoids a script tag and allows per-user or per-tenant config.

Either way, it's **still public**. Runtime config solves *flexibility*, not *secrecy*.

Use runtime config when you promote one artifact through several environments or run many tenants from one build. For most projects, per-environment builds are simpler and fine.

## Dev proxy: avoiding env juggling and CORS

For local development, a dev-server proxy means the frontend can use the same relative URL everywhere:

```ts
// vite.config.ts
server: { proxy: { "/api": { target: "http://localhost:3000", changeOrigin: true } } }
```

```bash
VITE_API_URL=/api        # same in dev and (if you proxy in production) production
```

In production, route `/api` to your backend at the host or CDN level ([deployment](./02-deployment.md#routing-the-api)). Same-origin requests also remove CORS entirely and simplify cookie auth ([authentication](../11-api-integration/03-authentication.md#cross-origin-cookies-checklist)).

## Using env vars in `vite.config.ts`

`import.meta.env` isn't available in the config file itself. Use `loadEnv`:

```ts
import { defineConfig, loadEnv } from "vite"

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), "")      // "" = load ALL vars (including non-VITE_), for config use only
  return {
    server: { proxy: { "/api": env.DEV_API_TARGET ?? "http://localhost:3000" } },
    define: { __APP_VERSION__: JSON.stringify(env.APP_VERSION ?? "dev") },
  }
})
```

Variables read this way live in the **config process**. They don't reach the client unless you `define` them, so be deliberate.

## Env vars in CI/CD

The pipeline supplies per-environment values, as real environment variables:

```yaml
- run: npm run build
  env:
    VITE_API_URL: ${{ vars.API_URL }}               # non-secret config: repository/environment "variables"
    VITE_SENTRY_DSN: ${{ vars.SENTRY_DSN }}
    SENTRY_AUTH_TOKEN: ${{ secrets.SENTRY_AUTH_TOKEN }}   # secret used by the BUILD (source map upload): not VITE_-prefixed
```

Notice the split: public config as variables; **build-time secrets** (a token to upload source maps) as secrets **without** the `VITE_` prefix, so they never enter the bundle. See [CI/CD](./04-ci-cd.md#secrets-and-environments).

## Feature flags

Env vars are a crude flag mechanism: a rebuild per toggle. For toggling features without redeploying, use a flag service (or runtime config fetched from your backend), keyed by user, percentage, or environment. Keep flags **short-lived**: remove them once a feature is fully launched.

## Common mistakes

- **Putting secrets in `VITE_*` variables**, which ships them to every visitor.
- **Thinking `.env` files are private** because they're gitignored. They're excluded from git, not from the bundle.
- **Committing `.env.local` or real credentials**, and then rotating keys too late.
- **Expecting a host's env var edit to change a deployed app**, when Vite inlined it at build time.
- **Treating env values as booleans or numbers** (`"false"` is truthy).
- **No validation**, so a missing variable becomes a confusing runtime error far from the cause.
- **Forgetting the `VITE_` prefix** (variable is `undefined`) or restarting the dev server after editing `.env` (it only loads at startup).
- **Using `import.meta.env` in `vite.config.ts`** (use `loadEnv`).
- **Building one artifact and expecting it to behave per environment** without runtime config.
- **Relying on obscurity** (base64, splitting a key across variables) to "hide" a secret in the client.
- **No `.env.example`**, so setup knowledge lives in someone's head.
- **Leaving provider keys unrestricted** (no domain allow-list or quota).

## Quick summary

- Vite exposes only **`VITE_`-prefixed** variables on `import.meta.env`; they're **inlined at build time** and **public**.
- **Never put secrets in the frontend.** Keep them server-side and have the browser call your backend.
- Files: `.env`, `.env.local`, `.env.[mode]`, `.env.[mode].local` (more specific wins; the real environment wins over files). Commit `.env.example`; gitignore `*.local`.
- Values are strings: convert and **validate at startup** (e.g. with Zod) and import one typed `env` object.
- Changing a value needs a **rebuild**, unless you use **runtime config** (`config.js`/`config.json`) to "build once, deploy many".
- Use `loadEnv` in `vite.config.ts`; use a dev **proxy** to keep URLs and CORS simple; supply per-environment values from CI (public as variables, build-time secrets without the `VITE_` prefix).

## Next

[01 — Build](./01-build.md)
