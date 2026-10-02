# 13 · Session Management

Authentication state has to live somewhere. Redis is the standard answer when you run more than one app instance: it is fast, shared, has built-in expiry, and makes **revocation** (logging someone out *now*) straightforward.

This module covers the two halves of the problem: **sessions** (server-side state referenced by a cookie) and **tokens** (JWT access tokens, refresh tokens and one-time codes).

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Session Storage](./01_session-storage.md) | Opaque session IDs, a Redis-backed store, idle and absolute expiry, "log out everywhere", running across many instances |
| 02 | [Token Management](./02_token-management.md) | Short-lived JWTs, rotating refresh tokens with reuse detection, denylists, password-reset and OTP tokens |

## Learning outcomes

After this module you can:

- Build a session store with safe IDs, sliding and absolute expiry, device lists and global logout
- Configure secure cookies and prevent session fixation and CSRF
- Run sessions across many stateless instances without sticky sessions
- Implement refresh-token rotation with **reuse detection** that revokes a stolen token family
- Revoke JWT access tokens with a cheap denylist or token versioning
- Issue single-use tokens (password reset, email verification) and attempt-limited OTPs
- Reason about what happens to logins when Redis fails

## Sessions or tokens?

| | **Server-side sessions** | **JWT access + refresh tokens** |
|---|--------------------------|----------------------------------|
| State | In Redis, looked up per request | Access token is self-contained, refresh state is in Redis |
| Revocation | **Immediate** (delete the key) | Access tokens live until expiry unless you add a denylist |
| Per-request cost | One Redis read | Signature check (plus an optional Redis check) |
| Best for | **Browser apps** with your own backend | **APIs, mobile apps**, third-party clients, multiple services |
| Size on the wire | A small opaque ID | A larger signed token |
| Complexity | Lower | Higher (rotation, expiry, key management) |

A good default for a **web app with its own backend** is plain **server-side sessions**. Reach for tokens when clients aren't browsers on your domain, or when separate services must verify identity without calling a central store. Many systems use **both**: a session cookie for the website and tokens for the mobile app and public API.

## The mental model

```
SESSION                                         TOKENS
browser ── cookie: sid=opaque ──► app           client ── Authorization: Bearer <JWT, 10 min> ──► API
                                   │                                │ expired?
                                   ▼                                ▼
                          Redis: session:{hash}            POST /auth/refresh (refresh token, rotates)
                          { userId, role, ... }                     │
                          TTL = idle timeout                        ▼
                                                           Redis: refresh family {current, previous, revoked}
```

## Security principles used throughout

1. **Opaque, random identifiers** (256 bits from a CSPRNG). Never guessable, never derived from user data
2. **Hash secrets at rest.** Store `sha256(sessionId)` or `sha256(refreshToken)` in Redis, so a Redis dump can't be replayed as live credentials
3. **Short lifetimes plus rotation.** Limit the damage window of any stolen credential
4. **Both idle and absolute expiry.** Activity can't extend a login forever
5. **Revocation is a feature.** Users, admins and incident response must be able to end sessions immediately
6. **Fail safely.** Decide up front what an unreachable Redis means for each endpoint

## Prerequisites

- Completed [12_rate-limiting](./../12_rate-limiting/README.md), because login, refresh and OTP endpoints must be rate limited
- Comfortable with [Hashes](../04_data-structures/02_hashes.md), [expiration](../02_redis-fundamentals/03_expiration-and-ttl.md), [Lua scripts](../06_advanced-commands/03_lua-scripts.md) and the [Express wiring](../08_nodejs-integration/06_express-integration.md)

## Next

Continue to [14_queues-and-workers](../14_queues-and-workers/README.md).
