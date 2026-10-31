# Crypto Primitives

The handful of cryptographic building blocks used in auth, and, just as important, which one to reach for in which situation. Most real-world auth bugs are a correct primitive used for the wrong job.

**Prerequisite:** [02-threat-model.md](./02-threat-model.md)

---

## Pick the right tool

```text
Need unpredictable secret?          → CSPRNG        (randomBytes)
Store a PASSWORD?                   → Argon2id      (slow, salted)
Store a random TOKEN?               → SHA-256       (fast is fine)
Prove a message wasn't tampered?    → HMAC / signature (JWT)
Compare two secrets?                → timingSafeEqual
Keep data confidential?             → encryption (AES-GCM), not hashing
```

All from Node's `crypto` module, except Argon2id (`argon2` package).

---

## 1. CSPRNG: random values

A cryptographically secure pseudo-random number generator pulls entropy from the OS.

```js
import crypto from "crypto";

const token = crypto.randomBytes(32).toString("base64url"); // 256 bits
const id = crypto.randomUUID();                              // for jti etc.
```

Never use `Math.random()` for anything security-related: it is predictable.

| Bytes | Bits | Typical use |
|---|---|---|
| 32 | 256 | Reset/verify tokens, session ids |
| 64 | 512 | Refresh secret |

Beyond ~128 bits, guessing is not a realistic threat; larger sizes are headroom, not necessity.

`base64url` is URL- and cookie-safe (no `+`, `/`, `=`), unlike plain base64.

---

## 2. Hashing: one-way fingerprints

A hash function maps any input to a fixed-size output and cannot be reversed. Two very different use cases need two very different hash types:

| | Passwords | Random tokens |
|---|---|---|
| Input entropy | Low (human-chosen) | Very high (256+ bits) |
| Attack | Offline guessing of likely passwords | Guessing is infeasible |
| Want hash to be | **Slow**, memory-hard, salted | Fast |
| Use | Argon2id | SHA-256 |

```js
// Random token → fast hash is enough
const tokenHash = crypto.createHash("sha256").update(token).digest("hex");
```

Why SHA-256 is safe for tokens but not passwords: a stolen hash of a 256-bit random token can't be brute-forced, because there is no small space of "likely tokens" to try. A human password has such a space, hence the deliberately slow, salted Argon2id ([hashing-argon2id](../02-passwords/00-hashing-argon2id.md)).

A **salt** is random data mixed into each password hash so identical passwords produce different hashes and rainbow tables fail. Argon2 generates and embeds it for you.

---

## 3. HMAC and signatures: integrity

A hash alone doesn't prove who made it. **HMAC** mixes in a secret key:

```js
const sig = crypto.createHmac("sha256", secretKey).update(data).digest("hex");
```

Only holders of the key can produce or verify it. A JWT with `HS256` is exactly this: the signature is an HMAC over `header.payload`. Asymmetric algorithms (`RS256`, `EdDSA`) use a private key to sign and a public key to verify; see [jwt-algorithms-and-keys](../03-tokens/07-jwt-algorithms-and-keys.md).

Signing provides integrity and authenticity, **not secrecy**: the data is still readable.

---

## 4. Constant-time comparison

`===` on strings can return as soon as it finds a differing character, so response time can leak how much matched. Use a constant-time comparison:

```js
export const safeCompare = (aHex, bHex) => {
  const a = Buffer.from(aHex, "hex");
  const b = Buffer.from(bHex, "hex");
  if (a.length !== b.length) return false; // timingSafeEqual throws on length mismatch
  return crypto.timingSafeEqual(a, b);
};
```

Comparing the **hashes** of tokens (as in this architecture) already makes timing leaks far less useful, since the attacker can't control hash prefixes. Constant-time is still the correct habit for MAC, signature and token comparisons.

---

## 5. Encryption vs hashing vs encoding

Three things people confuse:

| | Reversible? | Needs key? | Purpose |
|---|---|---|---|
| Encoding (base64) | Yes | No | Representation, not security |
| Hashing | No | No (HMAC: yes) | Fingerprint / verify |
| Encryption (AES-GCM) | Yes | Yes | Confidentiality |

Encryption is needed when you must read the value back later, such as a TOTP secret (see [totp](../05-mfa/01-totp.md)). Passwords and tokens never need to be read back, so they are hashed, not encrypted.

---

## Common mistakes

- Hashing passwords with SHA-256 / MD5, even "with a salt".
- Slow-hashing random tokens with Argon2: pointless cost on every request.
- Using `Math.random()` or timestamps for tokens.
- Calling base64 "encryption".
- Rolling your own crypto construction instead of using these vetted primitives.
- Comparing secrets with `===`.

---

## Quick summary

- Random → `crypto.randomBytes`. Never `Math.random`.
- Passwords → Argon2id. Random tokens → SHA-256.
- Integrity → HMAC/signatures (not secrecy).
- Compare secrets with `timingSafeEqual`.
- Need to read it back → encrypt; otherwise hash.

**Next:** [04-data-ownership.md](./04-data-ownership.md)
