# Environment Variables and Secrets

Auth code depends on secrets: JWT signing keys, database URLs, SMTP credentials. This note covers how to load them, validate them, keep them out of Git, and rotate them.

**Prerequisite:** [project-setup.md](./project-setup.md)

---

## The `.env` workflow

```text
.env            # real values, never committed
.env.example    # variable names with fake values, committed
.gitignore      # contains .env
```

```bash
# .env.example
NODE_ENV=development
PORT=4000
FRONTEND_URL=http://localhost:5173

MONGODB_URI=mongodb://localhost:27017/auth-mastery
REDIS_URL=redis://:devpass@localhost:6379

ACCESS_TOKEN_SECRET=replace-with-generated-secret

SMTP_HOST=localhost
SMTP_PORT=1025
```

```bash
# .gitignore
.env
.env.*
!.env.example
```

Node 20.6+ loads the file natively:

```bash
node --env-file=.env src/server.js
```

If a variable is already set in the real environment, it takes precedence over the file. That is how production overrides work.

---

## Generate real secrets

Never invent a secret by typing something. Use the OS CSPRNG:

```bash
node -e "console.log(require('crypto').randomBytes(64).toString('base64url'))"
```

Use a **different secret per purpose**. Reusing one value across JWT signing, cookies and anything else means one leak compromises all of them.

---

## Validate at startup

A missing secret should crash the app at boot, not produce a vague `jwt must be provided` error in production.

```js
// src/config/env.js
import { z } from "zod";

const schema = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]).default("development"),
  PORT: z.coerce.number().default(4000),
  FRONTEND_URL: z.string().url(),
  MONGODB_URI: z.string().min(1),
  REDIS_URL: z.string().min(1),
  ACCESS_TOKEN_SECRET: z.string().min(32),
});

export const env = schema.parse(process.env);
```

Import `env` instead of reading `process.env` all over the codebase. Everything is validated, typed by coercion, and defined in one place.

`z.coerce.number()` matters: all environment variables are strings.

---

## Secrets never belong in

```text
❌ Git history
❌ Frontend bundles
❌ Logs
❌ Error responses
❌ Docker images (baked in at build time)
```

Frontend tooling such as Vite exposes variables with a public prefix (`VITE_...`) to the browser. Anything with that prefix is public by definition.

**If a secret was ever committed, treat it as leaked and rotate it.** Deleting the file or rewriting history does not undo exposure; clones, forks and CI caches may already have it.

`.env` is plain text, not encryption. In production, inject values through your platform or a secret manager (environment variables from the host, AWS Secrets Manager, Vault, etc.) instead of shipping a file.

---

## Key rotation

Rotation means changing a secret on a schedule, or immediately after a suspected leak, without logging everyone out unnecessarily.

How each credential in this architecture is affected:

| Credential | Signed with env secret? | Effect of rotating |
|---|---|---|
| Access JWT | Yes | Old tokens fail verification |
| Refresh token | No (opaque, hash in Redis) | Unaffected |
| Reset / verification tokens | No (hash in Redis) | Unaffected |

Because access tokens live ~10 minutes and refresh tokens are not signed, a JWT secret rotation is cheap: affected clients simply hit `/auth/refresh` and receive a token signed with the new key.

### Zero-downtime rotation

Accept both keys for one access-token lifetime, sign only with the new one:

```js
import jwt from "jsonwebtoken";

const keys = {
  k2: process.env.ACCESS_TOKEN_SECRET_NEW,
  k1: process.env.ACCESS_TOKEN_SECRET_OLD,
};

export const sign = (payload) =>
  jwt.sign(payload, keys.k2, { keyid: "k2", expiresIn: "10m" });

export const verify = (token) => {
  const { header } = jwt.decode(token, { complete: true }) ?? {};
  const key = keys[header?.kid];
  if (!key) throw new Error("Unknown key");
  return jwt.verify(token, key);
};
```

After more than 10 minutes, remove `k1`. For asymmetric keys and JWKS, see [jwt-algorithms-and-keys.md](../03-tokens/jwt-algorithms-and-keys.md).

---

## Debugging

| Symptom | Likely cause |
|---|---|
| Variable is `undefined` | Script not started with `--env-file`, or typo in the name |
| Value has quotes or spaces | Check the `.env` line; avoid trailing whitespace |
| Local `.env` change ignored | Same variable already exported in your shell, which wins over the file |
| `PORT` behaves like a string | Missing coercion; validate with `z.coerce.number()` |

---

## Quick summary

- Commit `.env.example`, never `.env`.
- Generate secrets with `crypto.randomBytes`, one per purpose.
- Validate all env vars at startup and crash early.
- A committed secret is a leaked secret: rotate it.
- Rotate JWT keys with a `kid` and a temporary overlap of one token lifetime.

**Next:** [dev-tooling.md](./dev-tooling.md)