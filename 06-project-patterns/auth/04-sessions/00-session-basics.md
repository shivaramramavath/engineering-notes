# Session Basics

HTTP is stateless: every request arrives with no memory of the last one. A **session** is how the server remembers "this series of requests belongs to the same logged-in user". Everything in the rest of this chapter is a variation on that idea.

**Prerequisites:** [01-credential-types.md](../01-fundamentals/01-credential-types.md), [03-opaque-tokens.md](../03-tokens/03-opaque-tokens.md)

---

## What a session is

```text
Login succeeds
     │
     ▼
Server creates a session record      ← "who, since when, still valid?"
     │
     ▼
Client receives a handle to it       ← cookie or token
     │
     ▼
Every later request presents the handle → server finds the record → knows the user
```

Two parts, always:

| Part | Where | Example |
|---|---|---|
| **Session state** | Server (Redis/DB) or inside a signed token | `{ userId, createdAt, status }` |
| **Session handle** | Client | `sid` cookie, refresh token |

The whole stateful-vs-stateless debate is about where the state lives.

---

## Stateful vs stateless

```text
Stateful (server-side session)          Stateless (self-contained token)
──────────────────────────────          ────────────────────────────────
Cookie: sid=abc123                      Authorization: Bearer <JWT>
        │                                       │
        ▼                                       ▼
Server looks up abc123 in Redis         Server verifies signature, reads claims
→ { userId, role, ... }                 → { sub, role, exp }
```

| | Stateful | Stateless |
|---|---|---|
| Lookup per request | Yes (Redis/DB) | No (CPU only) |
| Instant revocation | Yes (delete the record) | No (wait for `exp`) |
| Change data mid-session | Yes (update the record) | No (token is frozen) |
| Scales across services | Needs a shared store | Only needs the key |
| Token contents visible to client | Nothing | Everything in the payload |
| Size on the wire | Tiny id | Hundreds of bytes |

"Stateless" is a slight misnomer: a stateless access token still depends on state at the issuer (the refresh session that keeps minting it). It is stateless only on the hot path.

---

## The model used in this repo: hybrid

```text
                     Stateless hot path                 Stateful cold path
                     ──────────────────                 ──────────────────
Every API request →  access JWT (verify locally)
Every ~10 minutes →                                     refresh token → Redis session
```

- **Access token (JWT):** verified without a store; valid up to ~10 minutes.
- **Session (Redis):** one record per login/device; controls whether renewal is allowed.

You get cheap request verification **and** revocation within one access-token lifetime. The next notes build each side: [01-redis-sessions.md](./01-redis-sessions.md) (the stateful store) and [02-cookie-sessions.md](./02-cookie-sessions.md) (the classic pure-session variant, for comparison).

---

## Session lifecycle

```text
Create  → at login (after password + MFA)
Use     → on each request / each refresh
Renew   → rotate the secret, optionally extend expiry
Revoke  → logout, password change, admin action, theft detected
Expire  → idle timeout or absolute timeout (TTL)
```

### Two kinds of timeout

| Timeout | Meaning | Example |
|---|---|---|
| **Idle** | Expires after N minutes without activity | 30 min for a banking app |
| **Absolute** | Expires N time after login no matter what | 30 days here |

Use both where risk justifies it. Idle timeouts need a "last seen" update, which is a write per request in a pure session design but only per refresh in the hybrid design.

---

## Properties of a good session

- **Unguessable handle:** 128+ bits from a CSPRNG.
- **Created fresh at login:** never reuse a pre-login id (prevents **session fixation**, where an attacker plants a known session id before the victim logs in).
- **Server-controlled lifetime:** expiry and revocation decided by the server, not by the client's cookie alone.
- **One per device/login:** so devices can be listed and revoked independently ([03-multi-device.md](./03-multi-device.md)).
- **Minimal data:** just enough to authorize; look up the rest in MongoDB.
- **Transported safely:** HTTPS, HttpOnly, SameSite.

---

## Session attacks to keep in mind

| Attack | Idea | Defense |
|---|---|---|
| Fixation | Attacker sets victim's session id | New id on login; never accept client-chosen ids |
| Hijacking | Attacker steals the handle | HTTPS, HttpOnly, short-lived access, rotation |
| Prediction | Guess ids | CSPRNG, 128+ bits |
| CSRF | Browser sends the session cookie for an attacker | `SameSite`, CSRF tokens ([05-csrf.md](./05-csrf.md)) |
| Persistence after logout | Handle still works | Server-side deletion, not just clearing the cookie |

"Clearing the cookie" is not logout. If the server keeps the session alive, anyone who copied the handle still has access.

---

## Common mistakes

- Treating logout as a client-only action.
- Putting large or sensitive data in the session.
- No expiry on sessions ("remember me" forever).
- Keeping the same session id across the login boundary.
- Believing a stateless token is "more secure" or a stateful one is "more secure"; they trade different risks ([06-jwt-vs-sessions.md](./06-jwt-vs-sessions.md)).

---

## Quick summary

- A session = server-side state + a client-held handle.
- Stateful: instant revocation, one lookup per request. Stateless: no lookup, revocation waits for expiry.
- This repo is hybrid: JWT for requests, Redis session for renewal.
- Fresh id at login, server-controlled expiry, one session per device.

**Next:** [01-redis-sessions.md](./01-redis-sessions.md)
