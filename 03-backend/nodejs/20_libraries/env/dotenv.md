# dotenv — Reference Guide

## What is dotenv?

`dotenv` is a Node.js library that loads variables from a `.env` file into `process.env`, so you can keep configuration out of your source code.

```
.env → dotenv → process.env → Application
```

## Why use it?

```js
// ❌ Hardcoded
const PORT = 5000;
const JWT_SECRET = "secret";
```

```env
# ✅ In .env
PORT=5000
JWT_SECRET=my-secret
```

```js
console.log(process.env.PORT);
```

Common uses: database URLs, API keys, JWT secrets, ports, Redis URLs, and other app config that differs between environments.

## Install

```bash
npm install dotenv
```

## Usage (ES Modules)

```js
import "dotenv/config";

console.log(process.env.PORT);
```

If you need custom options (like a non-default path), import `dotenv` directly instead:

```js
import dotenv from "dotenv";

dotenv.config({ path: ".env.development" });
```

## `.env` file

```env
PORT=5000
NODE_ENV=development
DATABASE_URL=mongodb://localhost:27017/app
JWT_SECRET=my-secret
REDIS_URL=redis://localhost:6379
```

## Values are always strings

```js
process.env.PORT; // "5000"  (string, not number)
process.env.DEBUG; // "true"  (string, not boolean)
```

Convert manually when needed:

```js
const port = Number(process.env.PORT);
const debug = process.env.DEBUG === "true";
```

For proper schema validation and type coercion, pair `dotenv` with **envalid**.

## `.env.example`

Commit a template, never the real secrets:

```env
PORT=
NODE_ENV=
DATABASE_URL=
JWT_SECRET=
REDIS_URL=
```

`.gitignore`:

```gitignore
.env
.env.*
!.env.example
```

## Multiple environments

```
.env
.env.development
.env.test
.env.production
```

```js
dotenv.config({ path: ".env.development" });
```

## Overriding existing variables

By default, `dotenv` will **not** overwrite a variable that's already set in `process.env`. To force it to:

```js
dotenv.config({ override: true });
```

Use this deliberately — it affects environment-variable precedence (e.g. values set by your shell or CI system).

## Centralize config access

Avoid reading `process.env` scattered throughout your codebase. Instead:

```
src/
└── config/
    └── env.ts
```

```ts
import "dotenv/config";

export const env = {
  nodeEnv: process.env.NODE_ENV ?? "development",
  port: Number(process.env.PORT ?? 5000),
  databaseUrl: process.env.DATABASE_URL,
  jwtSecret: process.env.JWT_SECRET,
  redisUrl: process.env.REDIS_URL,
};
```

```ts
import { env } from "./config/env.js";

console.log(env.port);
```

## dotenv + envalid

`dotenv` loads values; `envalid` validates and types them.

```ts
import "dotenv/config";
import { cleanEnv, port, str, url } from "envalid";

export const env = cleanEnv(process.env, {
  PORT: port({ default: 5000 }),
  DATABASE_URL: url(),
  JWT_SECRET: str(),
});
```

| Library   | Purpose                                       |
| --------- | --------------------------------------------- |
| `dotenv`  | Load `.env` into `process.env`                |
| `envalid` | Validate and type-check environment variables |

> **dotenv = Load. envalid = Validate.**

## Security

`dotenv` is **not** a secrets manager. It does not encrypt, validate, or secure credentials.

- Never log `process.env` in full.
- Never commit `.env` to version control.
- For production, use a real secrets manager: AWS Secrets Manager, HashiCorp Vault, Kubernetes Secrets, Docker Secrets, or your cloud provider's equivalent.

## Docker

```yaml
services:
  server:
    env_file:
      - .env
```

Values are available the same way inside the container:

```js
process.env.DATABASE_URL;
```

## Native alternative (no dependency)

Modern Node.js can load `.env` files without installing `dotenv`:

```bash
node --env-file=.env src/server.js
```

Use `dotenv` when you need multiple files, overrides, or programmatic control; use `--env-file` for a zero-dependency, single-file setup.

## Common mistakes

- **Forgetting to load it** — nothing under `import "dotenv/config"` runs before it, so import it first.
- **Assuming types** — `process.env.PORT` is `"5000"`, not `5000`.
- **Committing `.env`** — always add it to `.gitignore`.
- **Reading `process.env` everywhere** — centralize access through a single config module.
- **Expecting validation** — `dotenv` only loads; pair it with `envalid` if you need validation.

## One-line definition

> `dotenv` loads environment variables from `.env` into `process.env`.
