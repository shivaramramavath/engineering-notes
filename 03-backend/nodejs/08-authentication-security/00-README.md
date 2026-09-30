# 08 — Authentication & Security

How to prove who a user is, decide what they may do, and keep your Node.js API from becoming an easy target.

## Why this section exists

Most real-world breaches are not clever zero-days. They are stored plaintext passwords, missing authorization checks, leaked secrets, and unlimited login attempts. Everything in this folder targets those boring, common failures.

---

## Two words people mix up

| Term | Question it answers | Example |
|---|---|---|
| **Authentication (authN)** | *Who are you?* | Checking an email + password, verifying a JWT |
| **Authorization (authZ)** | *What are you allowed to do?* | "Only admins can delete users", "you can only edit your own post" |

Authentication happens first and produces an identity (`req.user`). Authorization then uses that identity to allow or deny each action. The middleware patterns for both live in `06-express/06-auth-and-authorization.md`; this folder explains the underlying mechanisms.

---

## What's in this folder

| File | What you'll learn |
|---|---|
| `01-password-hashing.md` | Storing passwords safely with bcrypt/argon2, login flow, reset flow |
| `02-jwt-and-tokens.md` | Access and refresh tokens, signing, rotation, revocation |
| `03-sessions.md` | Server-side sessions with cookies and Redis, when to prefer them over JWT |
| `04-oauth-and-oidc.md` | "Sign in with Google/GitHub", authorization code flow, PKCE |
| `05-common-vulnerabilities.md` | Injection, XSS, CSRF, SSRF, IDOR, mass assignment, and their fixes |
| `06-rate-limiting.md` | Throttling abuse, brute-force protection, Redis-backed limits |
| `07-helmet.md` | Security HTTP headers, CSP, HSTS |
| `08-security-checklist.md` | A pre-launch checklist and a hardened baseline app |

---

## Suggested reading order

1. **Passwords** (`01`) — the foundation of almost every login system.
2. **Sessions** (`03`) *and* **JWT** (`02`) — read both, then compare. Choosing between them is a real design decision.
3. **Vulnerabilities** (`05`) — learn how attackers think.
4. **Rate limiting** (`06`) and **Helmet** (`07`) — cheap, high-value defenses.
5. **OAuth/OIDC** (`04`) — when you want third-party login.
6. **Checklist** (`08`) — run through it before every deployment.

---

## The security mindset in five rules

1. **Never trust input.** Body, query, params, headers, cookies, uploaded files — all attacker-controlled. Validate (see `09-api-development/04-validation.md`).
2. **Defense in depth.** No single control is enough. Hashing + rate limiting + HTTPS + monitoring together make attacks expensive.
3. **Least privilege.** Users, services, and database accounts get only the access they need.
4. **Fail closed.** If something errors during an auth check, deny access.
5. **Don't invent cryptography.** Use vetted libraries (`bcrypt`, `argon2`, `jose`, `node:crypto`) — never your own scheme.

---

## A quick threat model for a typical API

```
Attacker goal                    Typical attack                     Defense (file)
------------------------------   --------------------------------   -----------------------
Take over accounts               Credential stuffing, brute force   06 rate limiting, 01 hashing
Steal a database's passwords     SQL injection, backup leak         05 injection, 01 hashing
Impersonate a user               Stolen/forged token                02 JWT, 03 sessions
Run script in victims' browsers  XSS                                05 XSS, 07 CSP
Act as a logged-in victim        CSRF                               03 sessions, 05 CSRF
Read other users' data           Missing ownership check (IDOR)     05 IDOR, 06-express/06
Make the server call internals   SSRF                               05 SSRF
```

---

## Prerequisites

- `02-core-modules/03-http.md` and `05-http-web/` — especially `03-cookies.md` and `04-cors.md`
- `06-express/02-middleware.md` — nearly every defense here is middleware
- `01-fundamentals/04-environment-variables.md` — secrets never belong in source code

## Next

**`01-password-hashing.md`** covers the first thing every login system must get right.
