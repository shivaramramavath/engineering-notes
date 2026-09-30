# Security Checklist

A practical, copy-paste checklist that ties together everything in this section. Run through it before every release, and use it as a review template for pull requests that touch auth, data access, or infrastructure.

## How to use this file

- Treat every unchecked box as a **question to answer**, not necessarily a bug. Sometimes the answer is "not applicable — here's why."
- Each group links back to the file that explains the reasoning.
- Security is not a one-time task. Re-run this list when you add a feature, a dependency, or an integration.

---

## 1. Passwords and credentials

> Details: `01-password-hashing.md`

- [ ] Passwords are hashed with **argon2id**, **bcrypt**, or **scrypt** — never MD5/SHA-1/SHA-256 alone, never plain text, never reversible encryption.
- [ ] Hashing uses the **async** API (the sync versions block the event loop).
- [ ] Cost parameters are tuned so one hash takes roughly 100–500 ms on production hardware.
- [ ] Login returns the **same generic error and status** for unknown email and wrong password.
- [ ] Login does comparable work whether or not the user exists (dummy hash) to avoid timing leaks.
- [ ] Hashes are upgraded transparently when parameters change (rehash on successful login).
- [ ] Password policy favors **length** (8+ minimum, allow long passphrases) over composition rules; no silly maximum lengths.
- [ ] Bcrypt's 72-byte input limit is handled (or argon2 is used).
- [ ] Password reset tokens are random, single-use, short-lived, and **stored hashed**.
- [ ] Password changes invalidate existing sessions and refresh tokens.
- [ ] Password hashes are never returned in API responses or written to logs.

---

## 2. Tokens (JWT) and sessions

> Details: `02-jwt-and-tokens.md`, `03-sessions.md`

**JWTs**

- [ ] The signing secret is long and random (32+ bytes) or an asymmetric key pair, loaded from environment/secret manager.
- [ ] `verify()` **pins the algorithm** (`algorithms: ["HS256"]` or your chosen one) — `alg: none` and algorithm-confusion attacks are impossible.
- [ ] Access tokens are **short-lived** (5–15 minutes).
- [ ] Standard claims are set and checked: `sub`, `exp`, and where relevant `iss` and `aud`.
- [ ] No sensitive data (passwords, PII beyond what's needed) inside the payload — it's readable by anyone.
- [ ] Refresh tokens are opaque random values, **stored hashed**, **rotated on every use**, with reuse detection.
- [ ] A revocation path exists (logout, password change, "log out everywhere").
- [ ] Tokens are never put in URLs or logged.

**Sessions**

- [ ] Session IDs come from a CSPRNG (the default in `express-session`), and the secret is strong and rotatable.
- [ ] Sessions are stored in **Redis/DB**, not the default in-memory store, in production.
- [ ] `req.session.regenerate()` is called on login (prevents session fixation).
- [ ] Logout calls `req.session.destroy()` **and** clears the cookie.
- [ ] Idle and absolute session timeouts are set.

**Cookies (both)**

- [ ] `HttpOnly: true` — JavaScript can't read them.
- [ ] `Secure: true` in production (and `trust proxy` configured correctly behind a proxy).
- [ ] `SameSite` is `lax` or `strict` (or `none` **only** with a deliberate CSRF strategy).
- [ ] Cookie names don't leak the framework (change the default `connect.sid` if desired).

---

## 3. OAuth / OpenID Connect

> Details: `04-oauth-and-oidc.md`

- [ ] **Authorization Code flow with PKCE** is used — not the implicit flow.
- [ ] A random `state` parameter is generated per request, stored server-side, and verified on callback.
- [ ] A `nonce` is used and checked against the ID token for OIDC.
- [ ] ID token signature, `iss`, `aud`, and `exp` are verified (using the provider's JWKS).
- [ ] Redirect URIs are registered **exactly** with the provider — no wildcards.
- [ ] Client secrets live in environment variables, never in front-end code or Git.
- [ ] Minimum scopes requested.
- [ ] Accounts are linked only on **verified** emails (`email_verified: true`).
- [ ] Users are identified by the provider's stable `sub`, not by email alone.

---

## 4. Input validation and injection

> Details: `05-common-vulnerabilities.md`, `09-api-development/04-validation.md`

- [ ] **Every** request body, query string, route param, and header you use is validated with a schema (zod/joi) on the server.
- [ ] Validation runs **before** any business logic or database call.
- [ ] Unknown fields are stripped or rejected (no mass assignment).
- [ ] SQL uses **parameterized queries** only.
- [ ] Mongo queries never receive unvalidated objects (`$ne`, `$gt`, etc. can't sneak in).
- [ ] No user input reaches `exec`, `eval`, `new Function`, or `vm` — use `execFile` with argument arrays if you must spawn processes.
- [ ] File paths built from input are resolved and checked to stay inside an allowed directory.
- [ ] User-supplied URLs fetched by the server go through an allow-list (SSRF).
- [ ] Deep-merge / `Object.assign` on untrusted input is guarded against prototype pollution.
- [ ] Regexes on untrusted input are simple, and input length is capped first (ReDoS).
- [ ] Request body size is limited (`express.json({ limit: "100kb" })`).

---

## 5. Authorization and data access

> Details: `06-express/06-auth-and-authorization.md`

- [ ] **Every** non-public route requires authentication (deny by default, allow by exception).
- [ ] Every resource access checks **ownership or role** in the same query that fetches it (no IDOR).
- [ ] Admin/privileged routes have their own explicit role check.
- [ ] Responses return explicit field lists — no `res.json(userDocument)`.
- [ ] Horizontal checks (user A vs user B) are tested, not just vertical (user vs admin).
- [ ] Authorization logic lives in one place (middleware/policy layer), not scattered across handlers.
- [ ] Unauthorized access to another user's resource returns 404 (or 403) without confirming it exists.

---

## 6. Abuse protection

> Details: `06-rate-limiting.md`

- [ ] A global rate limiter covers the whole API.
- [ ] Strict limiters protect **login**, **registration**, **password reset**, and **OTP/SMS/email-sending** endpoints.
- [ ] Login is limited per **IP and per account**.
- [ ] Limiters use a **shared store (Redis)** when more than one instance runs.
- [ ] `trust proxy` is set to the correct number of hops.
- [ ] Health-check endpoints are excluded from limiting.
- [ ] Clients get `429` with `Retry-After` and a helpful body.
- [ ] Failed logins and lockouts are logged for monitoring.

---

## 7. HTTP hardening

> Details: `07-helmet.md`, `05-http-web/04-cors.md`

- [ ] `helmet()` is registered before all routes.
- [ ] A **Content Security Policy** exists (at least report-only) for any HTML you serve; no `'unsafe-inline'` scripts.
- [ ] HSTS is enabled over HTTPS, with a sensible `maxAge`.
- [ ] `X-Powered-By` is removed.
- [ ] CORS allows a **specific list of origins** — never `*` together with credentials, never reflecting any `Origin` header blindly.
- [ ] Sensitive responses carry `Cache-Control: no-store`.
- [ ] HTTPS is enforced everywhere; HTTP redirects to HTTPS.
- [ ] Your domain scores well on securityheaders.com / Mozilla Observatory.

---

## 8. Secrets and configuration

> Details: `16-production/01-environment-management.md`

- [ ] No secrets in source code, Dockerfiles, front-end bundles, or Git history.
- [ ] `.env` is in `.gitignore`; a `.env.example` documents required variables (without values).
- [ ] Secrets differ between development, staging, and production.
- [ ] Production secrets live in a secret manager (AWS Secrets Manager, SSM, Vault) or the platform's encrypted env settings.
- [ ] Any secret that was ever committed or shared in chat has been **rotated**.
- [ ] Required environment variables are validated at startup — the app refuses to boot with missing or weak secrets.
- [ ] `NODE_ENV=production` is set in production.
- [ ] Secrets can be **rotated without downtime** (e.g. JWT key IDs, multiple session secrets).

A simple startup check:

```js
import { z } from "zod";

const env = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]),
  JWT_ACCESS_SECRET: z.string().min(32),
  SESSION_SECRET: z.string().min(32),
  DATABASE_URL: z.string().url(),
  REDIS_URL: z.string().url(),
}).parse(process.env);          // throws → the process crashes on boot, which is what you want
```

---

## 9. Dependencies and supply chain

> Details: `05-common-vulnerabilities.md` (section 15), `04-npm-ecosystem/`

- [ ] `package-lock.json` is committed, and CI/Docker use `npm ci`.
- [ ] `npm audit` runs in CI and **fails the build** on high/critical issues.
- [ ] Dependabot/Renovate (or equivalent) is on, and updates get reviewed regularly.
- [ ] New dependencies are vetted (maintenance, popularity, typosquatting check).
- [ ] Unused dependencies are removed.
- [ ] Node.js itself runs a **supported LTS** version that still receives security patches.
- [ ] Your own npm/GitHub accounts have 2FA enabled.
- [ ] Docker base images are pinned and regularly rebuilt.

---

## 10. Logging, monitoring, and incident response

> Details: `14-logging-observability/`

- [ ] Security events are logged: logins (success/failure), password resets, permission denials, token reuse, rate-limit hits.
- [ ] Logs **never** contain passwords, tokens, full `Authorization` headers, card numbers, or other secrets (redaction configured).
- [ ] Every log line carries a correlation/request ID.
- [ ] Alerts exist for spikes in 401/403/429 and for repeated failed logins.
- [ ] Error responses to clients are generic; details go to logs only.
- [ ] Someone knows **how to rotate secrets and revoke all sessions** in an emergency, and it's been practiced or written down.
- [ ] Users can be notified of suspicious activity (new device login, password changed).

Redaction example with Pino:

```js
import pino from "pino";

const logger = pino({
  redact: {
    paths: ["req.headers.authorization", "req.headers.cookie", "*.password", "*.token"],
    censor: "[REDACTED]",
  },
});
```

---

## 11. Infrastructure and deployment

> Details: `16-production/`

- [ ] App runs as a **non-root** user in the container.
- [ ] Database, Redis, and internal services are **not exposed to the public internet** (private network / security groups).
- [ ] Databases require authentication and use least-privilege accounts (the app can't `DROP TABLE`).
- [ ] TLS terminates at the proxy/load balancer with valid, auto-renewing certificates.
- [ ] Only the ports you need are open.
- [ ] Backups exist, are encrypted, and restoring from them has been tested.
- [ ] Debug endpoints, stack traces, and verbose errors are disabled in production.
- [ ] Graceful shutdown and health checks are configured (`16-production/02-graceful-shutdown-and-health-checks.md`).
- [ ] Uploaded files are validated (type, size), stored outside the web root or in object storage, and never executed.

---

## 12. Testing security

> Details: `13-testing/`

- [ ] Tests cover **negative** cases: no token, expired token, tampered token, wrong user, wrong role.
- [ ] A test proves user A **cannot** read or modify user B's data.
- [ ] Tests confirm rate limiters return `429` after the threshold.
- [ ] Tests assert password hashes and secrets are **not** present in any response.
- [ ] Validation tests send wrong types, oversized input, and unexpected fields.

Example negative test:

```js
import request from "supertest";
import app from "../src/app.js";

test("user cannot read another user's invoice", async () => {
  const res = await request(app)
    .get(`/invoices/${userB.invoiceId}`)
    .set("Authorization", `Bearer ${userA.token}`);

  expect(res.status).toBe(404);
});

test("rejects a token signed with a different secret", async () => {
  const res = await request(app)
    .get("/me")
    .set("Authorization", `Bearer ${forgedToken}`);

  expect(res.status).toBe(401);
});
```

---

## A reference middleware order

Order matters. A sensible starting stack for an API:

```js
import express from "express";
import helmet from "helmet";
import cors from "cors";
import cookieParser from "cookie-parser";

const app = express();

app.disable("x-powered-by");
app.set("trust proxy", 1);

app.use(helmet());                                   // 1. security headers
app.use(cors({ origin: allowedOrigins, credentials: true }));   // 2. CORS
app.use(globalLimiter);                              // 3. rate limiting
app.use(express.json({ limit: "100kb" }));           // 4. body parsing with a size cap
app.use(cookieParser());
// app.use(session(...));                            // 5. sessions, if you use them

app.use("/auth", authLimiter, authRoutes);           // 6. public auth routes with strict limits
app.use("/api", requireAuth, apiRoutes);             // 7. everything else requires authentication

app.use(notFoundHandler);                            // 8. 404
app.use(errorHandler);                               // 9. central error handler (last)
```

---

## Top 10 mistakes (the short version)

1. Storing passwords with a fast hash or in plain text.
2. Forgetting ownership checks (IDOR) — "logged in" is not "allowed."
3. Trusting `req.body` without validation (injection, mass assignment).
4. Accepting JWTs without pinning the algorithm, or making them long-lived.
5. Putting tokens in `localStorage` for apps where `HttpOnly` cookies are an option.
6. No rate limiting on login and password reset.
7. Committing secrets to Git and not rotating them.
8. `cors({ origin: true, credentials: true })` — reflecting every origin with credentials.
9. Returning raw database objects or stack traces to clients.
10. Ignoring `npm audit` and running unsupported Node versions.

---

## Before you ship: the 10-minute version

If you only have a few minutes, check these:

- [ ] HTTPS everywhere + Helmet enabled
- [ ] Passwords hashed with argon2id/bcrypt
- [ ] Rate limit on login/reset/OTP
- [ ] Input validation on all routes
- [ ] Ownership checks on all resource routes
- [ ] No secrets in Git; env validated on boot
- [ ] `npm audit` clean (or risks accepted knowingly)
- [ ] Generic error messages; no stack traces to clients
- [ ] Logs free of secrets; security events logged
- [ ] Database and Redis not publicly reachable

## Further reading

- OWASP Top 10 and the OWASP Cheat Sheet Series (Authentication, Session Management, Password Storage, JWT, Node.js Security)
- Node.js official *Security Best Practices* guide
- `19-system-design/01-scalable-api-and-rate-limiter.md` for rate limiting at scale
- `21-interview/03-security.md` for how these topics appear in interviews

## Next

Section **`09-api-development/`** — with authentication and hardening in place, move on to designing clean, consistent, versioned APIs.
