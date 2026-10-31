# Secure Random Tokens

Every opaque credential in this system (refresh secrets, reset links, verification links, API keys, session ids) is just a long unpredictable string. Its entire security comes from one property: **an attacker cannot guess it.** That property depends on where the randomness comes from.

**Prerequisite:** [03-crypto-primitives.md](../01-fundamentals/03-crypto-primitives.md)

---

## The rule

```js
import crypto from "crypto";

const token = crypto.randomBytes(32).toString("base64url");
```

`crypto.randomBytes` reads from the operating system's CSPRNG. `Math.random()` is a fast, statistically-random generator whose internal state can be recovered from a few outputs, so its future values become predictable. Never use it, or `Date.now()`, counters, or user ids, for anything an attacker must not guess.

---

## Size and entropy

Entropy is measured in bits: a token with *n* bits takes about 2^n guesses to brute-force.

| Bytes | Bits | Base64url chars | Use |
|---|---|---|---|
| 16 | 128 | 22 | Practical minimum for a secret |
| 32 | 256 | 43 | Default for reset, verification, session ids |
| 64 | 512 | 86 | Refresh secret (headroom, not necessity) |

128 bits is already beyond brute force. Larger sizes cost nothing meaningful, so this repo uses 256+ for simplicity and margin.

---

## Encoding

Random bytes must be turned into text to travel in URLs, cookies and JSON.

| Encoding | Example chars | Safe in URL/cookie? | Length for 32 bytes |
|---|---|---|---|
| `hex` | `0-9a-f` | Yes | 64 |
| `base64` | `A-Za-z0-9+/=` | **No** (`+ / =`) | 44 |
| `base64url` | `A-Za-z0-9-_` | Yes | 43 |

Use `base64url` for tokens. Encoding doesn't change entropy; it only changes how the same bytes are written.

---

## Other generators

```js
crypto.randomUUID();           // v4 UUID, 122 random bits, from the CSPRNG
crypto.randomInt(0, 1_000_000) // uniform integer, e.g. 6-digit OTP
```

- `randomUUID()` is fine for identifiers such as the JWT `jti`. Prefer `randomBytes` for secrets: the UUID format wastes bits on fixed characters and was designed as an identifier.
- `randomInt` avoids **modulo bias** (what you get from `randomBytes()[0] % 10`, which favors small digits). Use it for numeric codes:

```js
const otp = String(crypto.randomInt(0, 1_000_000)).padStart(6, "0");
```

A 6-digit code has only about 20 bits, so it is only safe with a short TTL, attempt limits, and rate limiting ([otp-email-sms](../05-mfa/03-otp-email-sms.md)).

Libraries such as `nanoid` are also CSPRNG-based and fine for ids; confirm any library uses `crypto` and not `Math.random`.

---

## Sync vs async

`randomBytes(32)` without a callback is synchronous. For 32-64 bytes it's effectively instant and non-blocking in practice, so sync is fine for auth tokens. A callback or `util.promisify` version exists but adds nothing here.

---

## Helper

```js
// utils/crypto.js
export const randomToken = (bytes = 32) => crypto.randomBytes(bytes).toString("base64url");
```

Use one helper everywhere so a code review can verify in one place that all tokens are generated correctly.

---

## Common mistakes

- `Math.random().toString(36)` for tokens or ids used as secrets.
- Deriving tokens from user data (`hash(email + timestamp)`): guessable inputs, guessable output.
- Making tokens short for "nicer URLs".
- Using `base64` and getting `+`, `/` or `=` broken by URL parsing.
- Generating tokens on the client for server-trusted purposes.
- Reusing the same token for two purposes.

---

## Quick summary

- Secrets come from `crypto.randomBytes`, never `Math.random`.
- 32 bytes (256 bits) default; 16 bytes is the floor.
- Encode as `base64url`.
- Numeric codes: `crypto.randomInt`, plus attempt limits.
- Centralize generation in one helper.

**Next:** [01-token-hashing.md](./01-token-hashing.md)
