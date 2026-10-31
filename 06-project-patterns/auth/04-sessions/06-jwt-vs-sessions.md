# JWT vs Sessions

A decision guide. The honest answer to "JWT or sessions?" is usually "it depends on who verifies the credential and how fast you need revocation", and the common best answer for SPAs with APIs is a **hybrid**.

**Prerequisites:** all earlier notes in this chapter, [02-jwt-access-tokens.md](../03-tokens/02-jwt-access-tokens.md)

---

## Side by side

| | Server-side session (cookie) | Stateless JWT (Bearer) | Hybrid (this repo) |
|---|---|---|---|
| State lives | Server (Redis/DB) | In the token | Both: Redis session + short JWT |
| Per-request cost | Store lookup | Signature check (CPU) | Signature check |
| Revocation | Instant | At expiry | Within one access TTL |
| Change user data mid-session | Immediate | Stale until reissue | Stale up to TTL |
| Client storage | HttpOnly cookie | Memory/storage (you choose) | JWT in memory + HttpOnly refresh cookie |
| CSRF exposure | Yes (cookie) | No (header) | Only the refresh cookie |
| XSS exposure | Cookie unreadable | Depends on storage | Token in memory + cookie unreadable |
| Cross-service verification | Needs shared store | Easy (shared key / JWKS) | Easy for access tokens |
| Mobile / third-party clients | Awkward | Natural | Natural |
| Implementation effort | Lowest | Medium | Highest |
| Common bugs | Fixation, CSRF, store outages | Revocation gaps, alg confusion, token theft | Rotation races, cookie path/CORS |

---

## Decision flow

```text
Is it a server-rendered / same-site web app, one backend?
   └── Yes ─► Cookie sessions. Stop. (Simplest, instant revocation.)

Do multiple services, mobile apps or third parties need to verify identity?
   └── Yes ─► Tokens (JWT + refresh). Asymmetric keys if others verify.

Do you need to revoke access within seconds (finance, admin tools)?
   └── Yes ─► Sessions, or tokens with a per-request session check.

SPA + separate API, want good UX and cheap request verification?
   └── Hybrid: short JWT + rotating refresh session.

Machine-to-machine / API keys?
   └── Long-lived hashed API keys or OAuth client credentials.
```

---

## Scenarios

| Scenario | Pick | Why |
|---|---|---|
| Blog/CMS/admin panel, Express + templates | Cookie session | No SPA complexity; instant logout |
| React SPA + your own Node API, same site | Hybrid or cookie session | Hybrid for scale/mobile later; cookie session if you want simplicity |
| React Native + web + API | Hybrid | Native apps handle bearer tokens natively; refresh in secure storage |
| Microservices behind a gateway | JWT (asymmetric) | Services verify locally with the public key |
| Banking dashboard | Sessions + step-up, short idle timeout | Immediate revocation, tight controls |
| Third-party "Login with X" API | OAuth2/OIDC tokens | Standard, interoperable ([oauth2-fundamentals](../06-oauth/00-oauth2-fundamentals.md)) |
| Public API for developers | API keys or OAuth client credentials | Long-lived, scoped, revocable |

---

## Myths

| Myth | Reality |
|---|---|
| "JWTs are more secure" | Different risks, not fewer. A leaked JWT is valid until expiry and can't be revoked by itself |
| "Sessions don't scale" | A Redis lookup scales to a very large number of requests; it's a dependency, not a wall |
| "JWTs are stateless so no server storage needed" | True only if you give up revocation; real systems add refresh sessions or deny-lists |
| "JWT = authentication standard" | JWT is a token *format*; it doesn't define login, sessions or refresh |
| "localStorage is fine for JWTs" | Any XSS reads it ([06-token-storage.md](../03-tokens/06-token-storage.md)) |
| "Cookies are insecure" | HttpOnly+Secure+SameSite cookies are among the safest browser storage options for credentials |
| "Use JWT for sessions because it's modern" | For one same-site app, a cookie session is simpler and safer |

---

## Cost of the hybrid

Choosing hybrid means owning more moving parts:

```text
Access JWT issuing/verifying + key management
Refresh rotation with atomic Lua and reuse detection
Cookie path/CORS/SameSite configuration
Client single-flight refresh + multi-tab behavior
Session list, revocation, TTL and Redis operations
```

That is why [the notes](../03-tokens/04-refresh-token-rotation.md) exist, and why it's reasonable to start with cookie sessions and move to hybrid when a real requirement (mobile, other services, scale) appears.

## Migration path

```text
Cookie sessions ──(add mobile / services)──► Hybrid
        │
        └─ keep Redis session store; add JWT access tokens in front
```

The Redis session design in [01-redis-sessions.md](./01-redis-sessions.md) is deliberately the same foundation for both.

---

## Quick summary

- Where does state live, who must verify, and how quickly must you revoke?
- Same-site web app → cookie sessions. Multi-client or multi-service → tokens. SPA + API with scale/mobile → hybrid.
- JWT is a format, not a security upgrade; sessions aren't legacy.
- Whatever you choose, apply the same fundamentals: HTTPS, hashed server-side secrets, TTLs, revocation on password change.

**Next:** [MFA overview](../05-mfa/00-mfa-overview.md)
