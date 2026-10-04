# Environment Variables

Configuration that changes between environments (database URLs, API keys, ports, feature flags) should live **outside the code**, in environment variables. This keeps secrets out of Git, lets the same build run in development, staging, and production, and follows the [Twelve-Factor App](https://12factor.net/config) approach to config.

This file covers loading variables (**Node's built-in `--env-file`**, **dotenv**, **dotenv-expand**, **dotenvx**) and, just as important, **validating** them (**envalid**, **zod**) so a missing or malformed value fails at startup instead of at 3 a.m.

See also: [Process and Env](../16_nodejs/03_process-and-env.md), [Project Structure](./01_project-structure.md), [Security](../22_security/00_README.md).

## The basics

`process.env` is an object of **strings** (or `undefined` when missing):

```js
process.env.PORT; // '3000' (a string, never a number)
process.env.DEBUG === "true"; // booleans are strings too: compare explicitly
process.env.MISSING; // undefined
```

You set them in the shell, in your hosting platform, or through a file:

```bash
PORT=4000 node app.js                  # one command (macOS, Linux, Git Bash)
export DATABASE_URL=postgres://...     # for the current shell session
```

A `.env` file is a convenience for **local development**: a plain text file of `KEY=value` lines that a loader copies into `process.env`.

```ini
# .env  (never commit this file)
NODE_ENV=development
PORT=3000
DATABASE_URL="postgres://app:secret@localhost:5432/app"
JWT_SECRET=change-me-in-real-environments
LOG_LEVEL=debug
```

## Option 1: Node's built-in support (no dependency)

Recent Node versions load `.env` files themselves:

```bash
node --env-file=.env app.js
node --env-file=.env --env-file=.env.local app.js    # later files override earlier ones
node --env-file=.env --watch src/index.js
```

```js
// or programmatically (Node 20.12+ / 21.7+)
process.loadEnvFile(".env"); // throws if the file is missing
process.loadEnvFile(); // defaults to ./.env

import { parseEnv } from "node:util";
const parsed = parseEnv("A=1\nB=2"); // { A: '1', B: '2' }  (no side effects)
```

Rules to remember:

- Variables **already set in the real environment win** over values in the file
- Newer Node versions add `--env-file-if-exists` for optional files; check `node --help` for your version
- There is no variable expansion or encryption; it is deliberately minimal

For many projects this is all you need: **no dotenv required**.

## Option 2: dotenv

The classic loader, still useful for older Node versions, programmatic control, and wide ecosystem support.

```bash
npm install dotenv
```

```js
// ESM: load before anything that reads process.env
import "dotenv/config";
import { env } from "./config/env.js";
```

```js
// CommonJS
require("dotenv").config();
```

### Import order matters

ES module imports run in the order written, **before** the rest of your file. If another module reads `process.env` while it is being imported, `dotenv` must already have run:

```js
// bad: config/env.js is evaluated BEFORE dotenv loads
import { env } from "./config/env.js";
import "dotenv/config";

// good: dotenv first
import "dotenv/config";
import { env } from "./config/env.js";

// best: load it from the command line, so import order never matters
// node --env-file=.env src/index.js
```

### Options

```js
import dotenv from "dotenv";

dotenv.config({
  path: [".env.local", ".env"], // several files: the FIRST file defining a key wins
  override: false, // default: do not replace variables already set
  quiet: true, // silence the load message printed by recent versions
});

const parsed = dotenv.parse(Buffer.from("A=1")); // parse a string or buffer without touching process.env
```

| Option       | Meaning                                                                                   |
| ------------ | ----------------------------------------------------------------------------------------- |
| `path`       | File or array of files to load                                                            |
| `override`   | `true` to overwrite existing variables                                                    |
| `quiet`      | Suppress the informational log (recent versions print one by default; check your version) |
| `encoding`   | File encoding (default `utf8`)                                                            |
| `processEnv` | Load into a custom object instead of `process.env`                                        |

### `.env` file syntax

```ini
# comments start with #
PORT=3000
NAME="Ada Lovelace"                 # quote values that contain spaces
SINGLE='no ${interpolation} here'   # single quotes: literal
MULTILINE="line one\nline two"      # \n expands inside double quotes
EMPTY=                              # empty string, not undefined
URL=postgres://user:p%40ss@host/db  # URL-encode special characters in credentials
```

## Variable expansion: dotenv-expand

Reference one variable inside another:

```bash
npm install dotenv-expand
```

```ini
DB_HOST=localhost
DB_PORT=5432
DB_NAME=app
DATABASE_URL=postgres://${DB_HOST}:${DB_PORT}/${DB_NAME}
```

```js
import dotenv from "dotenv";
import { expand } from "dotenv-expand";

expand(dotenv.config());
console.log(process.env.DATABASE_URL); // postgres://localhost:5432/app
```

Use sparingly: expansion makes it harder to see the final value. A plain full `DATABASE_URL` is usually clearer.

## Encrypted env files: dotenvx

**dotenvx** (from the dotenv author) runs any program with variables loaded and can **encrypt** `.env` files so they are safe to commit, with the decryption key kept separately.

```bash
npx dotenvx run -- node app.js        # load .env, then run
npx dotenvx encrypt                   # encrypts values in .env and writes a key to .env.keys
```

- `.env.keys` (the private key) must **never** be committed; give it to the runtime as `DOTENV_PRIVATE_KEY`
- Useful when you want config versioned with the code but secrets protected
- Evaluate it against a secrets manager before adopting; read the current docs for the workflow

## Validate your configuration

Loading is only half the job. Without validation, a missing `DATABASE_URL` shows up as a cryptic error deep inside a request, and `PORT="abc"` silently becomes `NaN`.

**Fail fast:** check everything once at startup, print a clear message, and exit.

### envalid

```bash
npm install envalid
```

```js
// src/config/env.js
import {
  cleanEnv,
  str,
  num,
  bool,
  port,
  url,
  email,
  host,
  json,
} from "envalid";

export const env = cleanEnv(process.env, {
  NODE_ENV: str({
    choices: ["development", "test", "production"],
    default: "development",
  }),
  PORT: port({ default: 3000 }),
  HOST: host({ default: "0.0.0.0" }),

  DATABASE_URL: url({
    desc: "PostgreSQL connection string",
    example: "postgres://user:pass@localhost:5432/app",
  }),
  REDIS_URL: url({
    default: "redis://localhost:6379",
    devDefault: "redis://localhost:6379",
  }),

  JWT_SECRET: str({ desc: "At least 32 random characters" }),
  JWT_TTL_MINUTES: num({ default: 15 }),

  ENABLE_SIGNUPS: bool({ default: true }),
  ADMIN_EMAIL: email({ default: "admin@example.com" }),
  FEATURE_FLAGS: json({ default: {} }),
});
```

```js
// anywhere else
import { env } from "#config/env.js";

env.PORT; // number 3000 (already converted)
env.ENABLE_SIGNUPS; // boolean
env.isProduction; // true when NODE_ENV === 'production'
env.isDev; // true when NODE_ENV === 'development'
env.isTest; // true when NODE_ENV === 'test'
```

If something is wrong, envalid prints every problem at once and exits:

```
================================
 Missing environment variables:
    DATABASE_URL: PostgreSQL connection string (eg. "postgres://user:pass@localhost:5432/app")
    JWT_SECRET: At least 32 random characters
 Invalid environment variables:
    PORT: Invalid port input: "abc"
================================
```

| Validator | Produces | Notes                                        |
| --------- | -------- | -------------------------------------------- |
| `str()`   | string   | `choices` restricts to a list                |
| `num()`   | number   | Rejects non-numeric input                    |
| `bool()`  | boolean  | Accepts `true/false/1/0/t/f` (parses safely) |
| `port()`  | number   | Integer from 1 to 65535                      |
| `url()`   | string   | Must be a valid URL                          |
| `email()` | string   | Basic email format                           |
| `host()`  | string   | Hostname or IP address                       |
| `json()`  | any      | Parses JSON text                             |

Useful options on any validator:

| Option                    | Meaning                                                                              |
| ------------------------- | ------------------------------------------------------------------------------------ |
| `default`                 | Used when the variable is missing, in **every** environment                          |
| `devDefault`              | Used only when `NODE_ENV` is not `production` (so production must set it explicitly) |
| `choices`                 | Allowed values                                                                       |
| `desc`, `example`, `docs` | Shown in error messages                                                              |

Custom validators:

```js
import { makeValidator, cleanEnv } from "envalid";

const secret = makeValidator((input) => {
  if (typeof input !== "string" || input.length < 32)
    throw new Error("must be at least 32 characters");
  return input;
});

export const env = cleanEnv(process.env, { JWT_SECRET: secret() });
```

Envalid returns a **read-only proxy**: reading an undeclared variable (`env.TYPO`) throws, which catches typos early. Customize behavior with the `reporter` option (for example to throw in tests instead of exiting the process).

### zod

If you already use [zod](./05_validation-libraries.md) for request validation, reuse it for configuration:

```js
// src/config/env.js
import { z } from "zod";

const schema = z.object({
  NODE_ENV: z
    .enum(["development", "test", "production"])
    .default("development"),
  PORT: z.coerce.number().int().min(1).max(65535).default(3000),
  DATABASE_URL: z.string().url(),
  JWT_SECRET: z.string().min(32, "must be at least 32 characters"),
  LOG_LEVEL: z.enum(["debug", "info", "warn", "error"]).default("info"),
  // booleans: see the warning below
  ENABLE_SIGNUPS: z
    .enum(["true", "false"])
    .default("true")
    .transform((v) => v === "true"),
});

export function loadEnv(source = process.env) {
  const result = schema.safeParse(source);

  if (!result.success) {
    const lines = result.error.issues.map(
      (i) => `  ${i.path.join(".")}: ${i.message}`,
    );
    console.error(`Invalid environment configuration:\n${lines.join("\n")}`);
    process.exit(1);
  }

  return Object.freeze(result.data);
}

export const env = loadEnv();
```

**Warning: `z.coerce.boolean()` is a trap.** It uses JavaScript truthiness, so the string `"false"` becomes `true`. Parse boolean variables explicitly as shown above.

### Choosing a validator

| Library              | Strengths                                                                          | Trade-offs                                               |
| -------------------- | ---------------------------------------------------------------------------------- | -------------------------------------------------------- |
| **envalid**          | Purpose-built: great error messages, `devDefault`, `isProduction`, read-only proxy | Env only; another schema vocabulary                      |
| **zod**              | One schema library for env and request validation; TypeScript inference            | You write the boolean and error-reporting logic yourself |
| **@t3-oss/env-core** | zod-based, with server/client separation (including for frameworks like Next.js)   | Extra layer; best in TypeScript projects                 |
| **env-var**          | Fluent accessors (`env.get('PORT').required().asPortNumber()`)                     | Validates per read rather than as one schema             |
| **convict**          | Rich config schema (files, env, args, docs)                                        | Heavier, older style                                     |

For a plain Node service, **envalid** is the simplest. If the project already uses zod, zod is the pragmatic choice.

## The pattern: one config module

```
src/config/env.js    ← the only file that touches process.env
```

```js
// every other file
import { env } from "#config/env.js";
const server = app.listen(env.PORT);
```

Benefits:

- Validation runs **once**, at startup
- The rest of the code gets **typed, converted, frozen** values
- A single place documents every variable
- Easy to enforce with a lint rule (`n/no-process-env` from `eslint-plugin-n`, allowed only in `config/`)

### Make it testable

Because validation reads `process.env` at import time, tests need control over the input. Expose a function that takes a source object:

```js
// config/env.js
export function loadEnv(source = process.env) {
  /* cleanEnv(source, spec) or schema.parse(source) */
}
export const env = loadEnv();
```

```js
// env.test.js
import { loadEnv } from "./env.js";

it("rejects an invalid port", () => {
  expect(() =>
    loadEnv({
      PORT: "abc",
      DATABASE_URL: "postgres://x",
      JWT_SECRET: "x".repeat(32),
    }),
  ).toThrow();
});
```

(With envalid's default reporter calling `process.exit`, pass a custom `reporter` that throws when testing.) Set test-time variables with your runner (`test.env` in Vitest) or a `.env.test` file loaded through `--env-file`.

## File conventions

| File              | Purpose                                                   | Committed                                    |
| ----------------- | --------------------------------------------------------- | -------------------------------------------- |
| `.env`            | Local values, possibly secrets                            | No                                           |
| `.env.local`      | Personal overrides                                        | No                                           |
| `.env.test`       | Values for the test run                                   | Usually yes (no real secrets)                |
| `.env.example`    | **Template**: every variable name with a safe placeholder | **Yes**                                      |
| `.env.production` | Real production secrets                                   | **Never** (use your platform's secret store) |

```ini
# .env.example: copy to .env and fill in
NODE_ENV=development
PORT=3000
DATABASE_URL=postgres://user:password@localhost:5432/app
JWT_SECRET=replace-with-at-least-32-random-characters
LOG_LEVEL=info
```

Generate strong secrets with Node itself:

```bash
node -e "console.log(require('crypto').randomBytes(32).toString('hex'))"
```

Keep `.env.example` in sync with the validation schema: a missing entry is a documentation bug. Some teams add a test or script that compares the two.

## Environments: `NODE_ENV` is not a stage name

`NODE_ENV` has three conventional values and libraries branch on them (`production` turns on optimizations in Express and many frameworks):

| Value         | Meaning                                                |
| ------------- | ------------------------------------------------------ |
| `development` | Local work: verbose errors, hot reload                 |
| `test`        | Automated tests                                        |
| `production`  | Anything running for real users, **including staging** |

Do not invent `NODE_ENV=staging`: libraries will treat it as non-production. Use a separate variable (`APP_ENV=staging`) for deployment stage behavior, and keep `NODE_ENV=production` on staging so it behaves like production.

## Secrets: where they should live

| Environment          | Where secrets live                                                                                  |
| -------------------- | --------------------------------------------------------------------------------------------------- |
| Local development    | Ignored `.env` file                                                                                 |
| CI                   | Encrypted CI secrets (GitHub Actions secrets)                                                       |
| Containers and PaaS  | Platform environment configuration (Heroku, Render, Fly, Vercel, Railway)                           |
| Cloud infrastructure | A secrets manager: AWS Secrets Manager or SSM, GCP Secret Manager, Azure Key Vault, HashiCorp Vault |
| Team workflows       | Doppler, 1Password CLI, Infisical, or similar                                                       |

Rules:

- **Never commit** secrets. If one is committed, treat it as **leaked**: rotate it immediately (deleting the commit is not enough, because it remains in history and clones)
- Do not bake `.env` into Docker images: list it in `.dockerignore` and pass variables at run time (`docker run --env-file .env` or orchestrator settings)
- Do not log environment variables or the whole `env` object (see [Logging](./06_logging.md) for redaction)
- Give each environment **different** secrets
- Environment variables are visible to the whole process and its children, and often in process listings or crash dumps; for the most sensitive material, prefer files mounted by the platform or a secrets manager API
- Scan for leaks in CI and hooks with tools such as **gitleaks** or **trufflehog**

## Frontend environment variables

Browser code cannot keep secrets. Bundlers **inline** variables into the public JavaScript at build time.

| Tool                      | Exposed prefix | Access                            |
| ------------------------- | -------------- | --------------------------------- |
| Vite                      | `VITE_`        | `import.meta.env.VITE_API_URL`    |
| Next.js                   | `NEXT_PUBLIC_` | `process.env.NEXT_PUBLIC_API_URL` |
| Create React App (legacy) | `REACT_APP_`   | `process.env.REACT_APP_API_URL`   |

```js
// src/config/env.js (frontend)
const required = ["VITE_API_URL"];
for (const key of required) {
  if (!import.meta.env[key]) throw new Error(`Missing ${key}`);
}
export const env = Object.freeze({ apiUrl: import.meta.env.VITE_API_URL });
```

Anything with that prefix is **public**. API keys that must stay secret belong on a server, called through your backend.

## Dev workflow

```json
{
  "scripts": {
    "dev": "node --env-file=.env --watch src/index.js",
    "start": "node src/index.js",
    "test": "node --env-file=.env.test --test"
  }
}
```

- In production, the platform provides the variables: `start` should **not** load a `.env` file
- Cross-platform inline assignment (`NODE_ENV=production node ...`) fails on Windows `cmd`; use `--env-file`, or [cross-env](./08_dev-tooling.md)

## Pitfalls

| Pitfall                                                | Why it hurts                                              | Better                                               |
| ------------------------------------------------------ | --------------------------------------------------------- | ---------------------------------------------------- | ----------------------------- | --------------------------------- |
| Reading `process.env.X` all over the code              | Typos, missing values, and wrong types surface at runtime | One validated `config/env.js`                        |
| Treating values as numbers or booleans                 | They are always strings: `"false"` is truthy              | Parse explicitly (envalid, zod)                      |
| `z.coerce.boolean()`                                   | `"false"` becomes `true`                                  | `z.enum(['true','false']).transform(...)`            |
| Importing config before `dotenv` runs                  | Variables are `undefined` at import time                  | `import 'dotenv/config'` first, or `node --env-file` |
| Committing `.env`                                      | Leaked secrets in Git history                             | `.gitignore`, `.env.example`, rotate leaked keys     |
| No `.env.example`                                      | Newcomers guess the required variables                    | Commit a complete template                           |
| Silent defaults for secrets (`JWT_SECRET               |                                                           | 'secret'`)                                           | Insecure production fallbacks | No default for secrets; fail fast |
| `NODE_ENV=staging`                                     | Libraries treat it as non-production                      | `NODE_ENV=production` plus `APP_ENV=staging`         |
| Secrets in frontend variables                          | Bundled into public JS                                    | Keep them on the server                              |
| Loading `.env` in production by habit                  | Stale or leaked files override platform config            | Let the platform inject variables                    |
| Logging the whole config object                        | Secrets end up in logs                                    | Log selected non-sensitive fields; use redaction     |
| Same secrets in every environment                      | One leak compromises everything                           | Separate secrets per environment                     |
| Validation scattered or lazy                           | Failures appear mid-request                               | Validate everything once at startup                  |
| Expecting `.env` changes to apply to a running process | Variables load at startup                                 | Restart (or use `node --watch`)                      |

## Key takeaways

- `process.env` holds **strings**; convert and validate them explicitly
- Modern Node can load files itself with `--env-file`; add **dotenv** when you need older versions or programmatic control, and **dotenv-expand** only if you need interpolation
- **Validate at startup** with **envalid** (purpose-built) or **zod** (if you already use it) and crash with a clear message
- Expose one frozen config object from one module; keep `process.env` reads out of the rest of the code
- Commit `.env.example`, never `.env`; rotate anything that leaks
- Use the platform's secret store in production and keep browser-exposed variables free of secrets
- Use `NODE_ENV` for `development` / `test` / `production` only; use another variable for stages like staging

**Next:** [Git Hooks and Commits](./03_git-hooks-and-commits.md)
