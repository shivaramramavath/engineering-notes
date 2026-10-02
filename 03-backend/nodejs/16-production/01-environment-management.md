# Environment Management

How configuration and secrets get into your app safely — and how to make a misconfigured deploy fail at startup instead of in front of users.

## Config is what changes between deploys

The same code runs in many places: your laptop, CI, staging, production. What differs between them is **configuration**:

| Kind | Examples | Sensitive? |
|---|---|---|
| **Connection info** | `DATABASE_URL`, `REDIS_URL`, queue endpoints | Often (embedded passwords) |
| **Credentials / secrets** | JWT signing keys, API keys, OAuth client secrets, session secret | **Yes** |
| **Behavior toggles** | `LOG_LEVEL`, feature flags, rate-limit values, cache TTLs | No |
| **Environment identity** | `NODE_ENV`, `APP_VERSION`, `AWS_REGION` | No |
| **Network settings** | `PORT`, `CORS_ORIGINS`, `TRUST_PROXY` | No |

The twelve-factor rule (`00-README.md`): **store config in the environment, not in the code.**

```js
// ❌ config baked into code: a different build per environment, and secrets in Git
const db = new Pool({ host: "prod-db.internal", user: "admin", password: "hunter2" });

// ✅ config from the environment: the same build everywhere
const db = new Pool({ connectionString: process.env.DATABASE_URL });
```

A good test for whether you've separated config from code: **could you open-source the repository right now without leaking a single credential?**

---

## `NODE_ENV`

`NODE_ENV` is the one environment variable with built-in meaning across the Node ecosystem.

```bash
NODE_ENV=production node src/server.js
```

| Value | Meaning |
|---|---|
| `development` | Local work: verbose errors, pretty logs, hot reload |
| `test` | Automated tests (`13-testing/`): quiet logs, test database |
| `production` | Real deployments |
| (unset) | Libraries usually treat it as development, **which is slower and less safe** |

What `production` actually changes:

- **Express:** enables view template caching, and sends terser error responses (no stack traces from the *default* error handler)
- **Many libraries** (React SSR, Mongoose, `debug` integrations, etc.) skip development-only checks and warnings, and run noticeably faster
- **`npm install`/`npm ci`** skip `devDependencies` when `NODE_ENV=production` (or with `--omit=dev`)

Important points:

- **Always set `NODE_ENV=production` in production** (typically in the Dockerfile, `03`). Forgetting it is one of the most common causes of "why is production slow?"
- **Use only these three values** (`development`, `test`, `production`). `staging` is *not* a `NODE_ENV`: libraries don't know it and treat it as development. Staging should run with `NODE_ENV=production` and identify itself with a separate variable such as `APP_ENV=staging`.
- **Don't use `NODE_ENV` as a general-purpose switch** for app behavior (`if (NODE_ENV === "staging")`). Use explicit config values (`EMAIL_PROVIDER=console|ses`, `LOG_LEVEL`), so behavior is controlled by what you actually mean.

```js
// ✅ explicit config instead of environment sniffing
const mailer = config.emailProvider === "ses" ? makeSesMailer(config.ses) : makeConsoleMailer();
```

---

## How Node.js reads environment variables

```js
process.env.PORT;                  // "3000": ALWAYS a string (or undefined)
process.env.ENABLE_CACHE;          // "false": a non-empty string, which is TRUTHY
```

The classic bugs:

```js
// ❌ strings, not numbers
const port = process.env.PORT || 3000;
app.listen(port + 1);                       // "30001"? No: "3000" + 1 = "30001" (string concatenation)

// ❌ the boolean trap
if (process.env.ENABLE_CACHE) { /* runs even when ENABLE_CACHE="false" */ }

// ❌ silent fallback for something that must be set
const secret = process.env.JWT_SECRET || "dev-secret";    // production quietly runs with a guessable secret
```

You need **parsing and validation in one place** (below).

---

## `.env` files for local development

Typing ten variables before every `npm start` isn't practical. A `.env` file holds them for **local development only.**

```bash
# .env: local development (NEVER committed)
NODE_ENV=development
PORT=3000
DATABASE_URL=postgres://app:app@localhost:5432/app_dev
REDIS_URL=redis://localhost:6379
JWT_ACCESS_SECRET=dev-only-secret-change-me-32chars-min
LOG_LEVEL=debug
```

### Loading it

**Node.js built-in** (v20.6+): no dependency needed:

```bash
node --env-file=.env src/server.js
node --env-file=.env --watch src/server.js        # restart on change
```

```json
{
  "scripts": {
    "dev": "node --env-file=.env --watch src/server.js",
    "start": "node src/server.js"
  }
}
```

Also available as `process.loadEnvFile(".env")` in recent Node versions. Note that **`--env-file` doesn't override variables that are already set** in the real environment, which is the behavior you want: real env always wins over the file.

**`dotenv`** (the long-standing package, works on all versions):

```js
import "dotenv/config";           // must run before anything reads process.env
```

Rules for `.env` files:

| Rule | Why |
|---|---|
| **Add `.env` to `.gitignore`** (and `.env.*` except the example) | Committing secrets is the most common way they leak |
| **Commit a `.env.example`** with every variable name and safe placeholder values | Documents what the app needs; new teammates copy it |
| **Use `.env` for local dev only.** In production, the platform injects real environment variables | Production secrets shouldn't live in files on disk or in the image |
| **Never bake `.env` into a Docker image** (add it to `.dockerignore`) | Anyone who can pull the image can read it (`03-docker-and-compose.md`) |
| **One file per purpose**, e.g. `.env` for dev and `.env.test` for tests | Avoid accidentally running tests against your dev database |

```bash
# .env.example: committed; values are placeholders
NODE_ENV=development
PORT=3000
DATABASE_URL=postgres://user:password@localhost:5432/dbname
REDIS_URL=redis://localhost:6379
JWT_ACCESS_SECRET=            # generate: openssl rand -base64 48
CORS_ORIGINS=http://localhost:5173
LOG_LEVEL=info
```

```gitignore
# .gitignore
.env
.env.*
!.env.example
```

---

## Validate configuration at startup (fail fast)

The most valuable habit in this file: **parse and validate every environment variable once, at startup, in one module, and crash immediately if anything is missing or malformed.**

Without it, a missing variable shows up later as `TypeError: Cannot read properties of undefined` in the middle of a customer's request, hours after the deploy that caused it. With it, the **deploy itself fails** (the container exits, the health check never passes, the rollout is aborted), and no user ever sees the bad version (`02-graceful-shutdown-and-health-checks.md`, `05-ci-cd.md`).

```js
// src/config/env.js
import { z } from "zod";

// z.coerce.* handles the "everything is a string" problem; booleans need an explicit parser (see below)
const booleanString = z.enum(["true", "false", "1", "0"]).transform((v) => v === "true" || v === "1");

const schema = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]).default("development"),
  APP_ENV: z.enum(["local", "ci", "staging", "production"]).default("local"),
  APP_VERSION: z.string().default("dev"),

  PORT: z.coerce.number().int().min(1).max(65535).default(3000),
  HOST: z.string().default("0.0.0.0"),

  DATABASE_URL: z.string().url(),                                   // no default: required everywhere
  DATABASE_POOL_MAX: z.coerce.number().int().positive().default(10),
  REDIS_URL: z.string().url(),

  JWT_ACCESS_SECRET: z.string().min(32, "must be at least 32 characters"),   // no default for secrets, ever
  JWT_ACCESS_TTL_SECONDS: z.coerce.number().int().positive().default(900),

  CORS_ORIGINS: z.string().default("").transform((s) => s.split(",").map((o) => o.trim()).filter(Boolean)),
  TRUST_PROXY: z.coerce.number().int().min(0).default(0),           // number of proxy hops (06-express, 08-authentication-security)
  LOG_LEVEL: z.enum(["fatal", "error", "warn", "info", "debug", "trace", "silent"]).default("info"),

  ENABLE_SIGNUPS: booleanString.default("true"),
  SENDGRID_API_KEY: z.string().optional(),
});

// Extra rules that depend on several values together
const refined = schema.superRefine((env, ctx) => {
  if (env.NODE_ENV === "production") {
    if (env.JWT_ACCESS_SECRET.includes("change-me") || env.JWT_ACCESS_SECRET.startsWith("dev")) {
      ctx.addIssue({ code: "custom", path: ["JWT_ACCESS_SECRET"], message: "looks like a development secret" });
    }
    if (env.CORS_ORIGINS.length === 0) {
      ctx.addIssue({ code: "custom", path: ["CORS_ORIGINS"], message: "must be set in production" });
    }
    if (!env.SENDGRID_API_KEY) {
      ctx.addIssue({ code: "custom", path: ["SENDGRID_API_KEY"], message: "required in production" });
    }
  }
});

const result = refined.safeParse(process.env);

if (!result.success) {
  // Print WHICH variables are wrong, never their VALUES (they may be secrets)
  console.error("❌ Invalid environment configuration:");
  for (const issue of result.error.issues) {
    console.error(`  - ${issue.path.join(".")}: ${issue.message}`);
  }
  process.exit(1);
}

export const config = Object.freeze(result.data);       // immutable: nothing can quietly mutate config at runtime
```

Using it:

```js
// everywhere else: import the typed, validated config, not process.env
import { config } from "./config/env.js";

app.set("trust proxy", config.TRUST_PROXY);
app.listen(config.PORT, config.HOST);
const pool = new Pool({ connectionString: config.DATABASE_URL, max: config.DATABASE_POOL_MAX });
```

### Rules for the config module

- **`process.env` is read in exactly one file.** Everywhere else imports `config`. A lint rule (`no-process-env`, from `eslint-plugin-n`) enforces it.
- **No defaults for secrets or connection strings.** A default `JWT_SECRET` means a forgotten variable silently ships a known secret. Defaults are for harmless tuning knobs (pool size, log level).
- **Required in every environment** is better than "required only in production", because the less production differs from everything else, the fewer surprises. Keep production-only checks to the few that truly must differ.
- **Parse types once:** numbers, booleans, comma-separated lists, durations, URLs.
- **Never print values** in validation errors or logs: only variable names and the problem.
- **Freeze** the object.
- **Fail with a non-zero exit code** so orchestrators treat the start as failed.
- **Inject config into code** (`makeMailer({ apiKey })`) rather than letting every module import the global, which keeps units testable (`10-architecture/04-dependency-injection.md`, `13-testing/`).

### Alternatives to hand-rolling

`envalid`, `env-var`, `convict`, and `@t3-oss/env-core` do similar jobs. The zod approach above has the advantage of one validation library across the whole project (`09-api-development/04-validation.md`).

### Testing it

```js
test("rejects a short JWT secret", () => {
  const result = schema.safeParse({ ...validEnv, JWT_ACCESS_SECRET: "short" });
  expect(result.success).toBe(false);
});

test("parses boolean strings correctly", () => {
  expect(schema.parse({ ...validEnv, ENABLE_SIGNUPS: "false" }).ENABLE_SIGNUPS).toBe(false);   // not truthy!
});
```

Export the **schema** separately from the code that exits the process so you can test it without killing the test runner.

---

## Secrets

**Secrets** are config that grants access or proves identity. They need extra care because leaking one is a security incident.

### Where secrets must never be

| ❌ Never | Why |
|---|---|
| In source code or committed files (including "private" repos) | Repos get shared, forked, cloned onto laptops, and leaked; history is forever |
| In Docker images (`ENV`, `COPY .env`, `ARG` used at runtime) | Layers are readable by anyone who can pull the image |
| In logs, error messages, URLs, or telemetry attributes | Logs are widely readable and long-lived (`14-logging-observability/01-pino-and-structured-logging.md`) |
| In front-end bundles | Everything shipped to a browser is public |
| In chat, tickets, or email | Searchable and forwarded forever |
| Shared across environments | A staging leak shouldn't compromise production |
| In CI logs | Mask them; don't `echo` them |

### Where they should live

```
Secret manager (source of truth)  ──fetched/injected at deploy or start──▶  environment variable in the running container
```

| Option | Notes |
|---|---|
| **AWS Secrets Manager** | Managed, encrypted (KMS), IAM-controlled, **automatic rotation** for RDS and others; small per-secret cost |
| **AWS SSM Parameter Store** (`SecureString`) | Cheaper (standard parameters are free), simpler; fine for most config + secrets |
| **HashiCorp Vault** | Powerful: dynamic short-lived database credentials, fine-grained policies; more to operate |
| **GCP Secret Manager / Azure Key Vault** | The equivalents on those clouds |
| **Doppler, Infisical, 1Password Secrets Automation** | SaaS/dev-friendly secret management with environment syncing |
| **Kubernetes Secrets** | Convenient, but only **base64-encoded**, not encrypted by default: enable encryption at rest and RBAC, or sync from a real secret manager (External Secrets Operator, Secrets Store CSI driver) |
| **CI/CD secret stores** (GitHub Actions secrets) | For build and deploy credentials, not application runtime secrets |

### Injecting them (no code changes needed)

Because your app reads environment variables, the **platform** does the fetching and injection. Your code stays the same.

```jsonc
// AWS ECS task definition: secrets are pulled from Secrets Manager/SSM at container start (06-aws.md)
{
  "containerDefinitions": [{
    "name": "api",
    "environment": [
      { "name": "NODE_ENV", "value": "production" },
      { "name": "LOG_LEVEL", "value": "info" }
    ],
    "secrets": [
      { "name": "DATABASE_URL",       "valueFrom": "arn:aws:secretsmanager:...:secret:prod/orders/database-url" },
      { "name": "JWT_ACCESS_SECRET",  "valueFrom": "arn:aws:ssm:...:parameter/prod/orders/jwt-access-secret" }
    ]
  }]
}
```

```yaml
# Kubernetes: environment from a Secret
env:
  - name: JWT_ACCESS_SECRET
    valueFrom:
      secretKeyRef: { name: orders-secrets, key: jwt-access-secret }
```

The alternative is to fetch secrets **in the app** at startup using the SDK (`@aws-sdk/client-secrets-manager`). It works, but adds code, startup latency, and failure modes, so prefer platform injection unless you need runtime refresh (rotation without restart).

### Limit blast radius

- **Least privilege:** each service can read only *its own* secrets (IAM policy per task role: `06-aws.md`).
- **Separate secrets per environment** and per service.
- **Short-lived credentials where possible:** IAM roles for AWS access instead of long-lived access keys; OIDC federation for CI (`05-ci-cd.md`); Vault's dynamic database users.
- **Audit access:** CloudTrail / Vault audit logs show who read what.

### Rotation

Secrets will eventually leak or expire; the question is whether you can change them **without downtime**. Design for it from the start:

| Secret | Rotation approach |
|---|---|
| **Database password** | Use Secrets Manager's managed rotation, or create a second user, switch the app, drop the old one. The app must re-read credentials on reconnect (or restart) |
| **JWT signing key** | Support **two keys at once**: sign with the new one, accept both during a grace period (use a key ID `kid` header: `08-authentication-security/02-jwt-and-tokens.md`) |
| **Session secret** | `express-session` accepts an **array** of secrets: the first signs, all verify, so sessions survive a rotation |
| **API keys for third parties** | Create the new key, deploy, then revoke the old one |
| **Webhook signing secrets** | Accept both old and new signatures during a window (`09-api-development/07-webhooks.md`) |

```js
// session secret rotation: newest first
app.use(session({ secret: [config.SESSION_SECRET_CURRENT, config.SESSION_SECRET_PREVIOUS].filter(Boolean), /* ... */ }));
```

### If a secret leaks

Assume it's compromised the moment it's pushed publicly: bots scan GitHub within seconds.

1. **Rotate/revoke it immediately.** This is the only step that actually fixes the problem.
2. Then (not instead) remove it from the repository history (`git filter-repo` or BFG) and force-push, knowing copies may already exist elsewhere.
3. **Check access logs** for use of the leaked credential.
4. Find out *how* it leaked and prevent a repeat.

Prevention tools (`05-ci-cd.md`):

- **Pre-commit hooks** with `gitleaks`, `detect-secrets`, or `trufflehog` to block commits containing secrets
- **GitHub secret scanning and push protection** (blocks pushes with recognized secret formats)
- **CI scanning** on every pull request

---

## Config for different environments

### One build, config injected per environment

```
                 same image: orders-api:3f9c2ab
                  ┌─────────────┼─────────────┐
                  ▼             ▼             ▼
               staging       production    preview-pr-482
            DATABASE_URL=…   DATABASE_URL=…  DATABASE_URL=…
            LOG_LEVEL=debug  LOG_LEVEL=info  ENABLE_SIGNUPS=false
```

**Don't build a separate image per environment** (`npm run build:staging`), because you're then not deploying what you tested. If values must differ, they come from the environment at runtime.

### Per-environment config files: usually avoid

Some projects use `config/default.json`, `config/production.json` (the `config` package). It's workable for non-secret, structural settings, but it multiplies the places configuration can hide, and invites "environment sniffing." Prefer **a flat set of environment variables, validated by one schema**, with a documented default for harmless values.

### Feature flags are not environment variables

Environment variables change on **deploy**. If you want to toggle behavior at **runtime**, without a deploy, per user, or as a gradual rollout, use a feature flag system (LaunchDarkly, Unleash, GrowthBook, Flagsmith, or a simple database-backed table). Keep long-lived flags and runtime kill switches in the flag system; use env vars for deploy-time settings.

### Build-time vs runtime config (front-end)

Single-page apps (Vite, Next.js client code) **bake environment variables into the bundle at build time** (`VITE_API_URL`), and the value is **public**. Never put a secret in such a variable. If you need runtime configuration for a front-end, serve a `/config.json` or inject values into the HTML at request time.

---

## Common environment variables

A typical Node.js API reads roughly these:

| Variable | Purpose | Notes |
|---|---|---|
| `NODE_ENV` | `production` / `development` / `test` | Set in the Dockerfile for production |
| `PORT`, `HOST` | Where to listen | Platforms often inject `PORT`; bind to `0.0.0.0` in containers, not `localhost` |
| `DATABASE_URL`, `REDIS_URL` | Backing services | URL form includes the credentials, so treat as secret |
| `JWT_ACCESS_SECRET`, `SESSION_SECRET` | Signing keys | ≥ 32 random bytes; generate with `openssl rand -base64 48` |
| `CORS_ORIGINS` | Allowed browser origins | Comma-separated list, no `*` with credentials |
| `TRUST_PROXY` | Number of proxy hops | Required for correct client IP, rate limits, secure cookies (`04-nginx.md`) |
| `LOG_LEVEL` | Logging verbosity | Adjustable without a code change |
| `APP_VERSION` / `GIT_SHA` | Which build is running | Include in logs, metrics, traces, and error reports |
| `OTEL_*` | OpenTelemetry settings | `14-logging-observability/04-tracing-and-opentelemetry.md` |
| `NODE_OPTIONS` | Node flags such as `--max-old-space-size=384` | `03-docker-and-compose.md` |
| `AWS_REGION` | Region | The SDK reads credentials from the task/instance role, not env keys |
| `TZ` | Time zone | Set `UTC` for servers |

Security-related footgun: binding to `localhost` inside a container makes the app unreachable from outside the container.

```js
app.listen(config.PORT, "0.0.0.0");       // ✅ reachable through the container network
app.listen(config.PORT, "localhost");     // ❌ only reachable from inside the container itself
```

---

## Local developer experience

Make it easy to do the right thing:

```bash
# one-time setup for a new teammate
cp .env.example .env
# edit .env (the example documents every variable)
docker compose up -d          # local Postgres + Redis (03-docker-and-compose.md)
npm run dev
```

- Provide **safe local defaults** in `.env.example` (local database URLs, dev-only secrets) so it works immediately.
- Make the startup validation error **actionable**: "DATABASE_URL: Required: copy `.env.example` to `.env`."
- Share *non-local* secrets (staging API keys) through a secret manager or password manager, not Slack.
- A script that checks `.env` keys against `.env.example` catches drift:

```bash
diff <(grep -oE '^[A-Z_]+' .env.example | sort) <(grep -oE '^[A-Z_]+' .env | sort)
```

---

## Common mistakes

```js
// ❌ secrets or connection strings committed to Git (even once)
// ❌ `process.env.X || "default"` for secrets → a forgotten variable silently uses a guessable value
// ❌ reading process.env all over the codebase: no validation, no types, hard to test
// ❌ treating env values as numbers/booleans without parsing ("false" is truthy; "3000" + 1 = "30001")
// ❌ NODE_ENV=staging (libraries treat unknown values as development); use APP_ENV for that
// ❌ forgetting NODE_ENV=production in production → slower, noisier, less safe
// ❌ baking .env or secrets into the Docker image (ENV, COPY, ARG)
// ❌ one set of credentials shared by dev, staging, and production
// ❌ logging config objects or validation errors that include secret values
// ❌ building a separate image per environment
// ❌ secrets in front-end env vars (VITE_*, NEXT_PUBLIC_*): they're public
// ❌ no rotation plan: when a secret leaks you're forced into downtime
// ❌ app binds to localhost inside a container
// ❌ `.env` committed "just for the example": use .env.example with placeholders
// ❌ config validation that only runs on first use, instead of at startup
```

## Checklist

- [ ] `NODE_ENV=production` set in production; only `development | test | production` used; `APP_ENV` identifies staging
- [ ] All config read through **one validated module** (`process.env` nowhere else); typed, parsed, frozen
- [ ] The app **exits non-zero at startup** with a clear message (names only, never values) when config is invalid
- [ ] No defaults for secrets or connection strings
- [ ] `.env` gitignored; `.env.example` committed and kept in sync; `.env` excluded from Docker builds
- [ ] Secrets stored in a secret manager and injected by the platform; none in code, images, logs, or front-end bundles
- [ ] Separate credentials per environment and per service; least-privilege access
- [ ] Rotation designed in (two JWT keys, array of session secrets, managed DB rotation)
- [ ] Secret scanning in pre-commit and CI; a written "secret leaked" response
- [ ] One image promoted through environments, with config injected at runtime
- [ ] App binds to `0.0.0.0` in containers; `TRUST_PROXY` set to the real number of hops

## Next

**`02-graceful-shutdown-and-health-checks.md`** covers the lifecycle of the running process: how your app should behave when the platform says "stop" (every deploy does), and how the platform knows whether your app is alive and ready for traffic.