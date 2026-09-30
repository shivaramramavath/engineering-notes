# Environment Variables

Environment variables are how configuration reaches a running Node process from the outside — without hardcoding values into your source code. This file covers the fundamentals; the dedicated `env/` folder covers `dotenv` and `envalid` in depth for actually managing them well.

## `process.env`

```js
console.log(process.env.NODE_ENV); // "development", "production", etc.
console.log(process.env.PORT); // "3000" — a string, always
```

Every value in `process.env` is a **string**, regardless of what it conceptually represents:

```js
process.env.PORT === 3000; // false — it's the string "3000", not the number 3000
process.env.PORT === "3000"; // true
Number(process.env.PORT) === 3000; // true — convert explicitly when you need a number
```

```js
process.env.DEBUG === "true"; // there's no boolean here either — compare against the string
```

This single fact — everything is a string — is the root cause of a lot of subtle config bugs, and exactly why a validation layer like `envalid` (`env/envalid.md`) exists: to convert and check these values once, in one place, rather than scattering `Number(...)`/`=== "true"` checks throughout the codebase.

---

## Setting environment variables

### Inline, for a single command

```bash
PORT=4000 node server.js
```

```powershell
# Windows PowerShell
$env:PORT=4000; node server.js
```

### Exported for the whole shell session

```bash
export PORT=4000
node server.js
```

### From a `.env` file (the common approach for local development)

```env
# .env
PORT=3000
NODE_ENV=development
DATABASE_URL=postgresql://localhost/mydb
```

```js
import "dotenv/config";
console.log(process.env.PORT); // "3000"
```

Node has no built-in way to load a `.env` file — this is exactly what the `dotenv` package (`env/dotenv.md`) does: read the file and copy its values into `process.env` at startup.

---

## Why environment variables, rather than a config file?

```js
// ❌ hardcoded — same value for every environment, and secrets end up in source control
const JWT_SECRET = "my-secret-key";
```

```js
// ✅ different value per environment, never committed
const JWT_SECRET = process.env.JWT_SECRET;
```

- The exact same code runs in development, staging, and production — only the environment variables differ between them
- Secrets (API keys, database passwords) never need to appear in source code or be committed to Git — see `08-authentication-security/` and the Docker guide's `06_production/01_environments-secrets.md` for how this changes (but doesn't disappear) in production, where secrets come from a platform's secret manager rather than a local `.env` file

---

## Common environment variables you'll see constantly

```env
NODE_ENV=production        # "development" | "production" | "test" — changes library behavior
PORT=3000                    # what port the server listens on
DATABASE_URL=...               # connection string for a database
LOG_LEVEL=info                   # how verbose logging should be (see 14-logging-observability/)
```

`NODE_ENV` in particular is checked by many popular libraries (Express included) to decide things like whether to show detailed error messages or minify output — worth setting explicitly and correctly in every environment, since some libraries default to development-style (slower, more verbose) behavior when it's unset.

---

## Checking for a required variable at startup

```js
if (!process.env.DATABASE_URL) {
  console.error("Missing required environment variable: DATABASE_URL");
  process.exit(1);
}
```

Failing loudly and immediately at startup, rather than letting a missing variable cause a confusing failure later (e.g. a database connection attempt with `undefined` as the URL), is a good default habit — this is exactly the problem `envalid`'s `cleanEnv` (`env/envalid.md`) solves more thoroughly, validating an entire schema of expected variables at once rather than checking them one by one.

## Common mistakes

- **Assuming `process.env.SOME_FLAG` is a boolean** — it's always a string; `"false"` is truthy in a plain `if` check, since it's a non-empty string.
- **Committing a `.env` file with real secrets** — always add `.env` to `.gitignore`; commit a `.env.example` with empty/placeholder values instead.
- **Not validating required variables at startup** — a missing `DATABASE_URL` should fail immediately and clearly, not three layers deep in a cryptic connection error.
- **Forgetting `NODE_ENV=production` in production** — many libraries change behavior based on it; leaving it unset can silently leave development-mode behavior (verbose errors, unoptimized output) running in production.

## Quick summary

- `process.env` holds environment variables — always as strings, converted explicitly when a number/boolean is actually needed
- `.env` files + the `dotenv` package are the standard way to manage these locally; Node has no built-in loader for `.env` files itself
- Environment variables let the same code run differently per environment, and keep secrets out of source control
- Validate required variables at startup rather than letting a missing one fail mysteriously later — `envalid` does this well at scale

## Section complete

That covers Node's fundamentals — the runtime, globals, module system, and environment variables. **`02-core-modules`** puts all of this to work through Node's actual built-in modules.
