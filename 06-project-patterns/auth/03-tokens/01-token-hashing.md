# Token Hashing

Server-validated tokens (refresh, reset, verification) are stored as **hashes**, not raw values. If Redis or a backup leaks, the attacker gets fingerprints that cannot be turned back into usable tokens.

**Prerequisite:** [00-secure-random-tokens.md](./00-secure-random-tokens.md)

---

## The idea

```text
                Random token (256-bit)
                       │
        ┌──────────────┴──────────────┐
        ▼                             ▼
  Sent to user/browser           SHA-256
  (the only raw copy)                 │
                                      ▼
                                 Stored in Redis
```

Verification works in reverse: hash what the client sends, compare with the stored hash. The server never needs the raw value after issuing it, so it never keeps it.

---

## Why SHA-256 (and not Argon2)

A hash must be slow when the input is guessable (passwords). A random 256-bit token is not guessable: there is no small set of "likely tokens" to try, so an attacker with the hash gains nothing from speed. A fast hash is correct here, and it runs on every refresh request, so Argon2's 100+ ms would be a real cost with no benefit.

| | Password | Random token |
|---|---|---|
| Input entropy | Low | 256+ bits |
| Hash | Argon2id | SHA-256 |
| Salt needed | Yes | No (input is already unique and unpredictable) |

This only holds if the token really is high-entropy. A 6-digit OTP hashed with SHA-256 can be brute-forced from the hash in milliseconds, so OTP storage needs different handling (short TTL, attempt counters).

---

## Implementation

```js
// utils/crypto.js
import crypto from "crypto";

export const hashToken = (token) =>
  crypto.createHash("sha256").update(token).digest("hex");

export const safeCompare = (aHex, bHex) => {
  if (typeof aHex !== "string" || typeof bHex !== "string") return false;
  const a = Buffer.from(aHex, "hex");
  const b = Buffer.from(bHex, "hex");
  if (a.length !== b.length) return false; // timingSafeEqual throws on unequal length
  return crypto.timingSafeEqual(a, b);
};
```

Two usage patterns:

### Pattern A: lookup by hash (reset, verification)

The hash **is** the Redis key. A wrong token simply doesn't exist.

```js
const data = await redis.getDel(`password-reset:token:${hashToken(token)}`);
```

There is no separate comparison step: the keyed lookup is the verification.

### Pattern B: lookup by id, compare hash (refresh)

The token carries a session id; the hash is a field in that session.

```js
const [sessionId, secret] = splitToken(refreshToken);
const stored = await redis.hGet(`session:${sessionId}`, "refreshTokenHash");
const ok = stored && safeCompare(hashToken(secret), stored);
```

Here a comparison exists, so use `safeCompare`. (In the atomic version, [the comparison happens inside a Lua script](./05-atomic-rotation-lua.md).)

---

## Is constant-time comparison really needed?

Honest answer: it's cheap insurance, with a limited effect here. Timing leaks help an attacker only if they can iteratively learn bytes of the *secret being compared*. Since the server compares **hashes** of attacker-supplied input, the attacker cannot steer the hash prefix, so a timing channel gives them no useful gradient. Constant-time comparison is still the correct habit for any MAC, signature or token comparison, because it removes the question entirely and protects you if the design changes later.

---

## What exactly to hash

For the refresh token `sessionId.secret`, hash **only the secret**:

```js
const refreshTokenHash = hashToken(secret); // not the whole string
```

The session id is a lookup handle, not a secret; the secret is what proves possession.

---

## HMAC variant (optional)

Some teams use `HMAC-SHA256(token, serverKey)` instead of plain SHA-256, so a leaked Redis dump can't even be checked against guessed tokens without the key. With 256-bit tokens this adds little; it adds a secret to manage and rotate. Plain SHA-256 is sufficient and simpler.

---

## Common mistakes

- Storing raw tokens "temporarily" in Redis or logs.
- Hashing with a fast hash when the token has low entropy (OTPs).
- `Buffer.from` with the wrong encoding on one side, so equal values never match.
- Comparing hex strings with `===` or `==`.
- Not handling length mismatch before `timingSafeEqual` (it throws).
- Hashing a different string than the one emailed, e.g. after URL-decoding or trimming.

## Debugging

| Symptom | Cause |
|---|---|
| Valid token never verifies | Hashed different strings at issue vs verify (whitespace, encoding) |
| `RangeError: Input buffers must have the same byte length` | Missing length check before `timingSafeEqual` |
| Token works from curl but not the link | URL mangled the token (use `base64url`, not `base64`) |

---

## Quick summary

- Store hashes of tokens; give the raw value only to the user.
- SHA-256 is right for high-entropy tokens; Argon2 is for passwords.
- Either use the hash as the lookup key, or compare with `timingSafeEqual`.
- Hash exactly the secret you issued, consistently encoded.

**Next:** [02-jwt-access-tokens.md](./02-jwt-access-tokens.md)
