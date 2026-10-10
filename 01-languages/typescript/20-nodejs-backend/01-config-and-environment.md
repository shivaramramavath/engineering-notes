# Config and Environment

A backend's behavior depends on values that change between environments: database URLs, secrets, ports, feature flags, timeouts. They arrive as **environment variables**, which are untyped strings that may be missing or malformed. Good configuration handling turns that mess into one validated, typed, immutable object at startup, so the rest of the code never touches `process.env` and a bad deployment fails immediately with a clear message instead of misbehaving at 3 a.m.

**Prerequisites:**
- [Node.js types](./00-node-types.md)
- [Trust boundaries](../15-runtime-validation/00-trust-boundaries.md)
- [Zod](../15-runtime-validation/02-zod.md) (the examples use it)

---

## The problem

```ts
const port = process.env.PORT;                       // string | undefined
const pool = new Pool({ max: process.env.DB_POOL_MAX });   // error: string | undefined is not number
const debug = process.env.DEBUG === "true";          // works, until someone sets "1" or "TRUE"
```

Scattered `process.env` reads cause the same problems repeatedly: every one is `string | undefined`, conversions are repeated and inconsistent, defaults live in many places, a missing variable surfaces as a confusing runtime error far from its cause, and tests have to mutate global state.

## The pattern: parse once, export a typed object

```ts
// src/config.ts
import { z } from "zod";

const EnvSchema = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]).default("development"),
  PORT: z.coerce.number().int().min(1).max(65535).default(3000),
  LOG_LEVEL: z.enum(["debug", "info", "warn", "error"]).default("info"),

  DATABASE_URL: z.string().url(),
  DB_POOL_MAX: z.coerce.number().int().positive().default(10),

  JWT_SECRET: z.string().min(32, "JWT_SECRET must be at least 32 characters"),
  ACCESS_TTL_SECONDS: z.coerce.number().int().positive().default(900),

  CORS_ORIGINS: z
    .string()
    .default("")
    .transform((s) => s.split(",").map((o) => o.trim()).filter(Boolean)),   // "a.com,b.com" -> ["a.com", "b.com"]
});

function loadConfig(source: NodeJS.ProcessEnv) {
  const parsed = EnvSchema.safeParse(source);

  if (!parsed.success) {
    const lines = parsed.error.issues.map((i) => `  ${i.path.join(".")}: ${i.message}`);
    throw new Error(`Invalid environment configuration:\n${lines.join("\n")}`);
  }

  const env = parsed.data;
  return {
    env: env.NODE_ENV,
    isProduction: env.NODE_ENV === "production",
    logLevel: env.LOG_LEVEL,
    http: { port: env.PORT, corsOrigins: env.CORS_ORIGINS },
    db: { url: env.DATABASE_URL, poolMax: env.DB_POOL_MAX },
    auth: { jwtSecret: env.JWT_SECRET, accessTtlSeconds: env.ACCESS_TTL_SECONDS },
  } as const;
}

export type Config = ReturnType<typeof loadConfig>;
export const config: Config = loadConfig(process.env);
```

What this gives you:

- **Typed values.** `config.http.port` is a `number`, `config.http.corsOrigins` a `string[]`, `config.env` a literal union.
- **Fail fast.** The program refuses to start if anything is wrong, and the message lists **every** problem at once, with variable names but not their values (so secrets are not logged).
- **Defaults in one place,** next to the schema that documents what each variable means.
- **Conversions in one place:** numbers, lists, enums.
- **A `loadConfig(source)` function** that takes the environment as a parameter, so tests can call it with a fake object instead of mutating `process.env`.
- **Grouped by concern** (`http`, `db`, `auth`), so consumers depend on the part they need.

## Rules for using it

1. **Only `config.ts` reads `process.env`.** Everywhere else imports `config` (or receives it, see below). A lint rule such as `n/no-process-env` from `eslint-plugin-n` enforces this.
2. **Validate at startup,** at the top of your entry point, before opening ports or connections.
3. **Treat config as immutable.** `as const` makes the properties readonly in the type. Add `Object.freeze` (deeply, if you care) to enforce it at runtime.
4. **Do not log secrets.** If you log the config for diagnostics, redact secret fields.

## Injecting config instead of importing it

A module that imports the global `config` is tied to it. For code you want to test or reuse, pass the pieces it needs ([dependency injection](../17-design-patterns/05-dependency-injection.md)):

```ts
interface DbConfig { url: string; poolMax: number }

export function createPool({ url, poolMax }: DbConfig) {
  return new Pool({ connectionString: url, max: poolMax });
}

// composition root
const pool = createPool(config.db);
```

`createPool` depends on a small interface, not the whole config, and tests pass a literal object. Keep the global `config` import in the composition root.

## Parsing details

Environment variables are always strings, so conversion needs care:

| Type | Careful approach |
|---|---|
| **Number** | `z.coerce.number()` plus `.int()` and range checks. Beware: coercing `""` gives `0`. |
| **Boolean** | do **not** use `z.coerce.boolean()` (any non-empty string, including `"false"`, becomes `true`). Parse explicitly: `z.enum(["true", "false"]).transform((v) => v === "true")`. |
| **List** | split on a delimiter, trim, and drop empty entries. |
| **URL** | `z.string().url()` validates the format. |
| **Duration** | accept plain numbers with the unit in the name (`_SECONDS`, `_MS`), so there is no ambiguity. |
| **JSON** | parse with `JSON.parse` in a transform and validate the result with a nested schema. |
| **Enum** | `z.enum([...])` so typos fail at startup. |

Prefer **required with no default** for anything where a wrong default would be dangerous (database URLs, secrets). A default for `DATABASE_URL` that points at localhost is how a production service quietly connects to the wrong place.

## Where the values come from

| Source | Notes |
|---|---|
| **The process environment** (set by the host, container, or orchestrator) | the production norm: Docker `-e`, Kubernetes env and secrets, platform dashboards |
| **`.env` files** for local development | convenient, **never committed** (add to `.gitignore`), and provide a committed `.env.example` listing every variable with safe placeholders |
| **Secret managers** (cloud key vaults and similar) | for production secrets, injected as environment variables or fetched at startup |

Loading a `.env` file: the `dotenv` package is the long-standing choice. Recent Node versions can also load one natively, via the `--env-file` command-line flag and a `process.loadEnvFile` API (check your Node version's documentation):

```bash
node --env-file=.env dist/server.js
```

Precedence is typically: variables already in the environment win over the file, so deployments override local defaults. Check the loader's rules. Do not rely on `.env` files in production containers: inject real environment variables.

### Per-environment configuration

Prefer **one set of variables, different values per environment** (the "twelve-factor" approach) over per-environment config files in the repository. Branching on `NODE_ENV` inside business code (`if (env === "production")`) makes behavior hard to test and reason about. Isolate environment differences in the config layer, and inject behavior (a real mailer or a fake one) rather than checking the environment at call sites.

## Secrets

- Never commit secrets, and never log them.
- Use **different secrets per environment.**
- Support **rotation**: design so a secret can change without code changes (and, for signing keys, accept old and new during a transition).
- Validate minimum strength at startup (`min(32)` for signing secrets).
- Give each service the **least privilege** it needs (a read-only database user for a read-only service).
- Do not expose server secrets to browser code. In frameworks with client bundles, only variables with an explicit public prefix reach the client ([Next.js](../19-react-and-frontend/09-nextjs.md)).

## Augmenting `ProcessEnv` (and why it is weaker)

You can describe variables by augmenting the global type:

```ts
declare global {
  namespace NodeJS {
    interface ProcessEnv {
      DATABASE_URL: string;
      PORT?: string;
    }
  }
}
export {};
```

This makes `process.env.DATABASE_URL` a `string` everywhere, but it is only a **claim**: nothing checks that the variable exists, and values are still strings that need conversion ([global and module augmentation](../09-declaration-files/02-global-and-module-augmentation.md)). A validated config object is strictly better. If you use augmentation, treat it as documentation alongside the validation, not a replacement.

## Feature flags and runtime configuration

Environment variables are read at **startup**. Values that must change without a restart (feature flags, rate limits, kill switches) belong in a runtime source: a database, a configuration service, or a flag provider. Type them with a schema as well, and have safe defaults if the source is unreachable. Keep the distinction clear: **startup config** is validated once, **runtime config** is validated whenever it is read.

## Testing

```ts
import { loadConfig } from "./config";

it("rejects a short JWT secret", () => {
  expect(() => loadConfig({ DATABASE_URL: "postgres://x", JWT_SECRET: "short" })).toThrow(/JWT_SECRET/);
});

it("applies defaults", () => {
  const cfg = loadConfig({ DATABASE_URL: "postgres://x", JWT_SECRET: "x".repeat(32) });
  expect(cfg.http.port).toBe(3000);
});
```

Pass explicit objects, so no test depends on, or leaks into, the real environment. Export `loadConfig` (and not only the loaded `config`) for this reason. See [unit testing](../18-testing-and-debugging/00-unit-testing.md).

## Common mistakes

- Reading `process.env` directly throughout the codebase.
- Casting `process.env.X as string` instead of validating.
- Using `z.coerce.boolean()` or `Boolean(process.env.X)` and getting `true` for `"false"`.
- A default for a value that should be required (database URL, secrets).
- Committing `.env` files or secrets, or leaving no `.env.example`.
- Logging the whole config, including secrets.
- Validating lazily, so the first request to touch a variable is the one that fails.
- Branching on `NODE_ENV` throughout business logic.
- Coercing an empty string to `0` and accepting it as valid.
- Using the same secrets in every environment.

## Debugging

- The startup error should name the variable and the problem. If it does not, improve the schema's messages.
- Print the **names** of loaded variables (never values) to confirm what the process received.
- In containers, check the real environment with `env` inside the running container, and the order of precedence if both `.env` and real variables exist.
- If a number behaves strangely, log `typeof` the value: it may still be a string from a skipped coercion.
- If a variable is `undefined` in production only, check how the platform injects it (build time vs runtime) and its exact name.

## Quick summary

- Environment variables are untyped strings. Parse and validate them **once at startup** with a schema, and export a typed, immutable, grouped config object.
- Only the config module reads `process.env`. Inject the pieces other modules need.
- Fail fast with a message that lists every problem and names variables without printing values.
- Parse booleans, numbers, and lists explicitly, and do not give dangerous defaults to required values.
- Use real environment variables in production, `.env` files only locally (never committed, with a `.env.example`), and a secret manager for secrets.
- Expose `loadConfig(source)` so tests can supply their own environment.

**Next:** [Express](./02-express.md)
