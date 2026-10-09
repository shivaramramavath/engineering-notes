# Authentication Architecture

Before writing any login code, decide **what proof of identity your API accepts after login** and **where the client keeps it**. These two decisions shape everything: revocation, scaling, XSS/CSRF exposure, mobile support. The rest of this section implements the options.

## Authentication vs authorization

| | Authentication (authN) | Authorization (authZ) |
|-|------------------------|-----------------------|
| Question | Who are you? | What are you allowed to do? |
| Failure | `401 Unauthorized` | `403 Forbidden` |
| Nest home | Guard that sets `req.user` | Guard that reads `req.user` + metadata |
| Covered in | This section | [Authorization](../07-authorization/README.md) |

Keep them separate in code: one guard establishes identity, a second decides permissions ([guards](../../03-core-concepts/01-request-pipeline/04-guards.md)).

## The generic flow

```text
1. Login:    client ──credentials──► server verifies ──► server issues proof
2. Requests: client ──proof──► guard validates proof ──► req.user set ──► handler
3. End:      logout / expiry / revocation invalidates the proof
```

"Proof" is either a **session id** (opaque, looked up server-side) or a **token** (self-contained, verified cryptographically).

## Stateful sessions vs stateless tokens

| | **Server-side session** | **JWT access token** |
|-|-------------------------|----------------------|
| What the client holds | Random session id (cookie) | Signed token containing claims |
| Server stores | Session data (Redis/DB) | Nothing (for access tokens) |
| Validation | Lookup per request | Verify signature + expiry (no lookup) |
| **Revocation** | Instant (delete the session) | Hard: valid until expiry unless you add state |
| Scaling | Needs a shared store | Any instance can verify |
| Payload staleness | Always current | Claims frozen until reissue |
| Best fit | Browser apps, same-site web | APIs, mobile, service-to-service, multiple services |

Neither is universally better. Common honest summary:

- **Browser app talking to your own backend:** server-side sessions (or short-lived tokens in `httpOnly` cookies) are simpler and safer to revoke.
- **Mobile apps / third-party API clients / microservices:** tokens (short-lived access + refresh).
- **"JWT everywhere because it's stateless"** is a trap: the moment you need logout, "kick this user", or role changes to take effect immediately, you rebuild state (denylists, token versions, refresh storage) and lose the simplicity.

## Where the client keeps the proof

| Storage | XSS risk | CSRF risk | Notes |
|---------|----------|-----------|-------|
| `httpOnly` cookie | JavaScript **can't read it** | **Yes**: browser sends it automatically | Needs CSRF defenses ([cookie auth](./06-cookie-authentication.md)) |
| `localStorage` / `sessionStorage` | **Any XSS can steal it** | No (not sent automatically) | Convenient, widely used, higher blast radius |
| In-memory (JS variable) | Harder to steal, still exposed to XSS during the session | No | Lost on reload, so pair with a refresh cookie |
| Mobile secure storage (Keychain/Keystore) | n/a | n/a | Standard for native apps |

You trade XSS exposure for CSRF exposure; there's no option with neither, which is why you mitigate both (CSP and output encoding for XSS; `SameSite`/CSRF tokens for cookies).

## Typical architectures

**A. Cookie session (classic web)**

```text
POST /auth/login ──► verify password ──► create session in Redis ──► Set-Cookie: sid=...
GET /me (cookie) ──► load session ──► req.user
```

[Session authentication](./07-session-authentication.md).

**B. Access + refresh tokens (APIs, mobile, SPAs)**

```text
POST /auth/login   ──► { accessToken (15m JWT), refreshToken (opaque, 7–30d) }
GET /me            ──► Authorization: Bearer <access>
POST /auth/refresh ──► rotate refresh token, return new pair
POST /auth/logout  ──► revoke refresh token
```

[JWT](./04-jwt.md) and [refresh tokens](./05-refresh-tokens-and-logout.md).

**C. Delegated login (OAuth/OIDC)**

```text
"Sign in with Google" ──► provider authenticates ──► your app maps the identity to a user ──► issue A or B
```

[OAuth 2.0](./08-oauth2.md) and [social login](./09-social-login.md). Delegating to an identity provider means you may never store passwords at all.

Whichever you pick, **step-up** (a second factor) layers on top: [two-factor authentication](./10-two-factor-authentication.md).

## How it maps to Nest

```text
src/
├── auth/
│   ├── auth.module.ts
│   ├── auth.controller.ts        // /auth/login, /auth/refresh, /auth/logout
│   ├── auth.service.ts           // verify credentials, issue/rotate tokens
│   ├── strategies/               // Passport strategies (local, jwt, google)
│   ├── guards/                   // JwtAuthGuard, LocalAuthGuard
│   └── decorators/               // @Public(), @CurrentUser()
└── users/                        // users + password hash storage (no auth logic)
```

Building blocks:

| Piece | Role |
|-------|------|
| **Guard** (global `APP_GUARD`) | Authenticates every request; `@Public()` opts out ([guards](../../03-core-concepts/01-request-pipeline/04-guards.md)) |
| **Strategy** (Passport) or custom guard logic | Extracts and validates proof, returns the user |
| **`AuthService`** | Credential checks, token issuing, rotation |
| **`@CurrentUser()`** | Param decorator reading `req.user` ([custom decorators](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md)) |
| **`UsersService`** | Looks up users; stores password hashes ([password hashing](./03-password-hashing.md)) |

Secure by default: register the authentication guard **globally** and mark exceptions (`login`, `register`, health checks) with `@Public()`. Forgetting to annotate a route then fails closed.

## Threat model: what you're defending against

| Threat | Mitigation |
|--------|-----------|
| Stolen database | Slow password hashing ([hashing](./03-password-hashing.md)); hash refresh/reset tokens too |
| Credential stuffing / brute force | Rate limiting, lockout/backoff, 2FA ([rate limiting](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)) |
| Token theft (XSS, logs, proxies) | Short access-token life, `httpOnly` cookies, CSP, never log tokens, refresh rotation |
| CSRF | `SameSite`, CSRF tokens, require custom headers ([CSRF](../../07-production/01-security/03-csrf.md)) |
| Session fixation | Regenerate the session id on login |
| Account enumeration | Same response and similar timing for "no such user" and "wrong password" |
| Weak/forged tokens | Strong secrets/keys, pinned algorithms, verify signature/expiry/issuer ([JWT](./04-jwt.md)) |
| Replay of reset/verification links | Single-use, hashed, expiring tokens |
| Man-in-the-middle | TLS everywhere, `Secure` cookies, HSTS ([security headers](../../07-production/01-security/02-http-security-headers-and-cors.md)) |
| Account takeover via social login | Only link on **verified** emails ([social login](./09-social-login.md)) |

## Principles to keep

- **Don't invent cryptography.** Use maintained libraries for hashing, signing, TOTP.
- **Fail the same way.** Generic error messages for failed logins; log the detail internally.
- **Short-lived credentials, revocable long-lived ones.**
- **Secrets from config**, validated at startup, never in git ([secrets management](../../07-production/01-security/06-secrets-management.md)).
- **Rate-limit and monitor** login, refresh, reset, and 2FA endpoints; alert on anomalies.
- **Prefer a managed identity provider** (or a mature library) when you can; auth is high-risk to build and operate yourself.

## Common mistakes

- **Choosing JWT for everything** and later bolting on revocation.
- **Tokens in `localStorage`** without a plan for XSS.
- **Cookie auth without CSRF defense.**
- **Authenticating in some routes and forgetting others** (no global guard).
- **Returning different errors/timing** for unknown user vs wrong password.
- **Long-lived access tokens** (days) with no revocation path.
- **Mixing authentication and authorization** in one guard.
- **Rolling your own password hashing or token format.**
- **Logging `Authorization` headers, cookies, or credentials.**

## Quick Summary

- AuthN = who; AuthZ = what they may do; separate the guards.
- Proof is a session id (stateful, easy revocation) or a token (stateless, hard revocation); pick for your clients, not by fashion.
- Cookies trade XSS exposure for CSRF exposure; localStorage the reverse. Mitigate whichever you choose.
- In Nest: global guard + `@Public()`, strategies/guards for verification, `AuthService` for issuing, `@CurrentUser()` for access.
- Defend against stolen DBs, brute force, token theft, CSRF, fixation, and enumeration with layered controls.

## Next

[Passport and local strategy →](./02-passport-and-local-strategy.md)
