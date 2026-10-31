# Credential Types

An authentication system is a set of different secrets, each proving something different. The most important design rule: **one credential, one purpose.** Never reuse a token for something it wasn't issued for.

**Prerequisite:** [00-authn-vs-authz.md](./00-authn-vs-authz.md)

---

## The five credentials

| Credential | Proves | Format | Stored by client | Stored by server | Lifetime |
|---|---|---|---|---|---|
| Password | You know the secret | Text | Nowhere (user's head / manager) | Argon2id hash in MongoDB | Until changed |
| Access token | This request is from an authenticated session | Signed JWT | React memory | Nothing (verified by signature) | ~10 min |
| Refresh token | This session may continue | Opaque `sessionId.secret` | HttpOnly cookie | SHA-256 hash in Redis | ~30 days, rotated |
| Password reset token | You control the email, may reset one password | Random 256-bit | Email link | SHA-256 hash in Redis | ~15 min, single-use |
| Email verification token | You control this email address | Random 256-bit | Email link | SHA-256 hash in Redis | ~30 min, single-use |

The lifetimes are design choices used in this repo, not standards.

API keys (a fifth, machine-oriented type) are covered at the end.

---

## Two families of tokens

```text
Self-contained (JWT)                 Opaque (random string)
────────────────────                 ──────────────────────
Data inside the token                Meaningless string
Verified by signature                Verified by lookup (Redis)
No server state                      Requires server state
Cannot be revoked early*             Revoke instantly (delete the key)
→ short life                         → can live long
```

\* unless you add a deny-list, which reintroduces state.

That trade-off explains the whole design:

- The **access token** is a JWT so normal API requests need no Redis call. It is short-lived because it cannot be revoked.
- The **refresh token** is opaque so the server can kill a session instantly, rotate the secret, and detect reuse. It is long-lived because the server controls it.

---

## Why separate reset and verification tokens

You might be tempted to reuse one "email token" for both flows. Don't.

```text
Verification token used as reset token  →  someone who only proved
their email address can now change the password
```

Each has its own Redis key prefix (`password-reset:token:`, `email-verification:`), TTL and consequence. A leak of one cannot be replayed against the other flow, and each is deleted after one use.

---

## What each token must never contain or do

```text
Access token     ✗ passwords, PII beyond what authz needs
                 (JWTs are only signed, not encrypted: anyone can read the payload)
Refresh token    ✗ user data at all; it is just a lookup handle + secret
Reset/verify     ✗ guessable values (counters, user id, timestamps)
All              ✗ appear in logs, URLs kept in history, or analytics
```

The JWT point is a frequent misconception: base64url is encoding, not encryption.

```bash
# Anyone can read a JWT payload
echo "<payload part>" | base64 -d
```

---

## API keys

For machine-to-machine access, an API key is a long random string with no expiry by default.

- Treat it like a password: generate with a CSPRNG, store only a hash, show the raw key once at creation.
- Add a non-secret prefix (e.g. `ak_live_...`) so users and secret scanners can identify it.
- Scope it (read-only vs full) and allow revocation and rotation.

For service-to-service flows with expiring tokens, see [OAuth client credentials](../06-oauth/06-machine-to-machine.md).

---

## Quick summary

- Five credentials, five purposes: never cross-use.
- JWT = stateless and fast but not revocable, so keep it short-lived.
- Opaque token = stateful but revocable, so it can be long-lived.
- JWT payloads are readable by anyone; never put secrets in them.
- Server stores hashes of random tokens, never the raw value.

**Next:** [02-threat-model.md](./02-threat-model.md)
