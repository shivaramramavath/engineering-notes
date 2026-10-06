# Environment Variables

Environment variables keep configuration and secrets (database URLs, API keys) out of your source code. Next.js loads them from `.env*` files and exposes them to code. The one rule you must understand: **only variables prefixed with `NEXT_PUBLIC_` reach the browser**. Everything else stays on the server.

## Basics

Create `.env.local` in the project root:

```bash
# .env.local
DATABASE_URL="postgres://user:pass@localhost:5432/app"
STRIPE_SECRET_KEY="sk_test_123"
NEXT_PUBLIC_APP_URL="http://localhost:3000"
```

Read them through `process.env`:

```tsx
// Server Component, Route Handler, Server Action
const db = process.env.DATABASE_URL;

// anywhere, including Client Components
const url = process.env.NEXT_PUBLIC_APP_URL;
```

Next loads the files automatically. No `dotenv` package is needed.

## Server vs browser

| Variable | Available on server | Available in browser |
|---|---|---|
| `DATABASE_URL` | Yes | **No** (`undefined`) |
| `NEXT_PUBLIC_APP_URL` | Yes | Yes |

Referencing a non-public variable in a Client Component does not throw: it silently evaluates to `undefined`. That is the most common "my env var is missing" bug.

`NEXT_PUBLIC_` is a **publishing** instruction. Anything with that prefix ends up in the JavaScript sent to every visitor. Never put secrets behind it.

## Which file does what

| File | Loaded in | Commit to git? |
|---|---|---|
| `.env` | all environments | Yes, only for non-secret defaults |
| `.env.development` | `next dev` | Yes (non-secret) |
| `.env.production` | `next build` / `next start` | Yes (non-secret) |
| `.env.test` | test runs | Yes (non-secret) |
| `.env.local` | all except test | **No**: personal and secret values |
| `.env.*.local` | that environment, locally | **No** |

Lookup order, first match wins:

1. `process.env` (variables already set in the shell or hosting platform)
2. `.env.$(NODE_ENV).local`
3. `.env.local` (not loaded when `NODE_ENV` is `test`)
4. `.env.$(NODE_ENV)`
5. `.env`

So a real environment variable on your host always overrides the files. The `.local` files are for your machine and should be listed in `.gitignore` (the generated `.gitignore` already ignores `.env*.local`).

## Build time vs runtime

This is where most production surprises come from.

- **`NEXT_PUBLIC_*` values are inlined at build time.** The build replaces `process.env.NEXT_PUBLIC_X` with the literal string. Changing the variable afterwards has no effect until you rebuild.
- **Server-only variables are read when the server code runs.** In dynamically rendered code that is per request. In a statically rendered page, the code runs at build time, so the value is baked into the output.

Consequences:

```text
Build image once, deploy to staging and prod?   → NEXT_PUBLIC_ values will be the build's values for both
Page rendered statically using process.env.X?   → X was read at build time
Changed an env var on the host, nothing changed → needs a rebuild (public) or a fresh render (static page)
```

If you need to change a public value per environment without rebuilding, expose it via a server endpoint or read it on the server and pass it to the client as props.

Docker and deployment specifics are in [Production Build](../22-production/00-production-build.md).

## Variable expansion

Variables can reference each other:

```bash
HOST="example.com"
API_URL="https://api.$HOST"
```

Escape a literal `$` with `\$` when a value contains it (for example, passwords).

## Using variables outside Next (scripts, tests, tooling)

Scripts you run with `node` or `tsx` do not get Next's loader. Use `@next/env`:

```ts
// scripts/seed.ts
import { loadEnvConfig } from "@next/env";

loadEnvConfig(process.cwd());
console.log(process.env.DATABASE_URL);
```

## Validate at startup

`process.env.X` is typed as `string | undefined`, and a missing value usually fails far from where it was needed. Validate once and export a typed object:

```ts
// lib/env.ts
import { z } from "zod";

const schema = z.object({
  DATABASE_URL: z.string().url(),
  STRIPE_SECRET_KEY: z.string().min(1),
  NEXT_PUBLIC_APP_URL: z.string().url(),
});

export const env = schema.parse(process.env);
```

Keep server-only variables in a module that is never imported by Client Components; see [Secrets](../21-security/04-secrets.md).

For editor autocomplete without a validator, declare types:

```ts
// env.d.ts
declare namespace NodeJS {
  interface ProcessEnv {
    DATABASE_URL: string;
    NEXT_PUBLIC_APP_URL: string;
  }
}
```

## `.env.example`

Commit a template so new developers know what to set:

```bash
# .env.example
DATABASE_URL=
STRIPE_SECRET_KEY=
NEXT_PUBLIC_APP_URL=http://localhost:3000
```

## Common mistakes and debugging

| Symptom | Cause | Fix |
|---|---|---|
| Variable is `undefined` in a Client Component | No `NEXT_PUBLIC_` prefix | Prefix it, or read it on the server and pass it down |
| Change to `.env.local` has no effect | Dev server reads files at start | Restart `next dev` |
| Works locally, `undefined` in production | Not set on the host, or `.env.local` was never deployed | Set it in the hosting platform |
| Public variable shows old value in production | Inlined at build time | Rebuild |
| `process.env[name]` returns `undefined` in the browser | Dynamic access is not inlined | Use the literal `process.env.NEXT_PUBLIC_X` |
| Secret appeared in browser source | Used `NEXT_PUBLIC_` or imported a server module into the client | Rotate the secret, fix the prefix |
| Secret committed to git | `.env.local` not ignored | Rotate it; add to `.gitignore` |

Quick check of what the server actually sees:

```bash
node -e "console.log(Object.keys(process.env).filter(k => k.startsWith('NEXT_PUBLIC_')))"
```

(Run inside a script that calls `loadEnvConfig` if you want Next's file loading.)

## Quick Summary

- `.env.local` for secrets and machine-specific values; never commit it.
- Only `NEXT_PUBLIC_*` reaches the browser; everything else is `undefined` there.
- `NEXT_PUBLIC_*` is baked in at build time; changing it requires a rebuild.
- Real environment variables on the host override `.env` files.
- Validate with a schema at startup and commit a `.env.example`.

## Next

- [next.config](./04-next-config.md)
- [Secrets](../21-security/04-secrets.md)
- [Production Build](../22-production/00-production-build.md)
