# Opaque Tokens

An opaque token is a random string that means nothing by itself. Its validity is decided by looking it up on the server. Refresh tokens, reset tokens and verification tokens in this architecture are all opaque.

**Prerequisites:** [00-secure-random-tokens.md](./00-secure-random-tokens.md), [02-jwt-access-tokens.md](./02-jwt-access-tokens.md)

---

## Opaque vs JWT

```text
JWT                                  Opaque
───                                  ──────
Contains claims                      Contains nothing
Verified by signature (no lookup)    Verified by lookup (Redis)
Revocation: wait for exp             Revocation: delete the key (instant)
Server state: none                   Server state: required
Size: ~300-800 bytes                 Size: ~43-130 chars
```

Neither is "better". They trade statefulness for revocability.

| Choose opaque when | Choose JWT when |
|---|---|
| You need instant revocation | Many services must verify without calling a central store |
| Token is long-lived | Token is very short-lived |
| Token should reveal nothing | Claims are needed at verification time |
| One server-side authority exists | Stateless scaling matters most |

This repo uses **both on purpose**: JWT for the hot path (every API call), opaque for the cold, high-privilege path (renewing a session).

---

## A weak design vs a strong design

### Weak: random token alone, no structure

```text
refreshToken = "x9Qa...43chars"
Redis: refresh:{hash(token)} → { userId }
```

Works, but when a token is invalid you cannot tell whose session it was. Reuse of an old (rotated-away) token just looks like "unknown token", so you can't detect theft or kill the right session.

### Strong: `sessionId.secret`

```text
refreshToken = "<sessionId>.<secret>"

Redis: session:{sessionId} → { userId, refreshTokenHash: sha256(secret), status, ... }
```

```text
S1.k8Xj...
│   └─ secret: proves possession (only its hash is stored)
└───── session id: tells the server WHICH session to look at
```

What the structure buys:

1. **O(1) lookup** by `sessionId`, with no scan.
2. **Reuse detection.** Presenting an old secret still finds session `S1`, where the hash mismatches. The server knows a rotated-away token is being replayed and can revoke exactly that session.
3. **Per-device control.** Logout one device = delete one key.
4. **No user data in the token.** It leaks nothing if intercepted.

The session id is a handle, not a secret, but it's still random (256-bit) so it isn't enumerable.

---

## Parsing safely

```js
export function splitRefreshToken(token) {
  if (typeof token !== "string") return null;
  const i = token.indexOf(".");
  if (i < 1 || i === token.length - 1) return null;
  return { sessionId: token.slice(0, i), secret: token.slice(i + 1) };
}
```

Validate format and length before touching Redis, so malformed input is rejected cheaply and can't inject odd key names. `base64url` contains no `.`, so the first dot is an unambiguous separator.

Also reject absurdly long input; an attacker shouldn't be able to make you hash or look up megabytes.

---

## Lifecycle of an opaque token

```text
Issue   → random bytes, store hash + metadata, set TTL
Use     → lookup, compare hash, check status/expiry
Rotate  → replace hash atomically (refresh tokens)
Revoke  → delete key (or mark status)
Expire  → TTL removes it automatically
```

Every one of these keys must have a TTL; see [data-ownership](../01-fundamentals/04-data-ownership.md).

---

## Token introspection (aside)

In OAuth ecosystems, a resource server holding an opaque token calls the authorization server's *introspection endpoint* to ask "is this active, and for whom?". That is exactly what your API would do if access tokens were opaque: one Redis call per request. It is why this repo uses JWTs for access tokens and opaque only where a lookup is acceptable.

---

## Common mistakes

- Putting identifying data in the token ("opaque" tokens that are secretly base64 JSON).
- Using the session id alone as the credential (no secret, so anyone who learns the id owns the session).
- Storing the secret instead of its hash.
- Skipping TTL so sessions live forever.
- Parsing with `split(".")` and accepting more than two parts silently.
- Not distinguishing "unknown session" from "wrong secret" internally (you lose reuse detection).

---

## Quick summary

- Opaque = meaningless random string + server lookup; instantly revocable.
- JWT for frequent verification, opaque for long-lived renewal.
- `sessionId.secret` gives fast lookup, reuse detection and per-device revocation.
- Validate format before lookup; always set TTLs.

**Next:** [04-refresh-token-rotation.md](./04-refresh-token-rotation.md)
