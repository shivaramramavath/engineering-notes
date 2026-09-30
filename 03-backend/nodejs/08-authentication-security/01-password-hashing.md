# Password Hashing

How to store passwords so that a leaked database does not become a leaked set of user passwords.

## The rule

**Never store a password. Store a slow, salted hash of it.**

```
❌ password: "hunter2"                      (plaintext — one leak exposes everyone)
❌ password: md5("hunter2")                 (fast hash — cracked in seconds)
❌ password: sha256("hunter2")              (fast hash — billions of guesses per second on a GPU)
✅ password: "$2b$12$Kix...(60 chars)"      (bcrypt — slow, salted, tunable)
```

---

## Hashing vs encryption

| | Encryption | Hashing |
|---|---|---|
| Reversible? | Yes, with the key | **No** — one-way |
| Purpose | Protect data you need to read again | Verify a value without storing it |
| For passwords? | No | **Yes** |

You never need to recover a user's password — you only need to check whether what they typed *matches*. That is exactly what a one-way hash allows.

---

## Why "fast" hashes are the wrong tool

SHA-256 and MD5 are designed to be fast. That is great for checksums and terrible for passwords: an attacker with a stolen database and a GPU can test billions of guesses per second.

Password hashing algorithms are deliberately **slow and memory-hungry**, and the slowness is adjustable as hardware improves.

## Salt

A **salt** is random data mixed into each password before hashing.

- Two users with the password `123456` get **different** hashes.
- Precomputed "rainbow tables" become useless.
- Attackers must attack each hash individually.

bcrypt and argon2 generate a salt automatically and embed it inside the output string — you don't manage it yourself.

---

## Choosing an algorithm

| Algorithm | Notes |
|---|---|
| **argon2id** | Current OWASP first choice. Memory-hard, resists GPU/ASIC attacks. |
| **scrypt** | Memory-hard, built into Node (`crypto.scrypt`), no install needed. |
| **bcrypt** | Older, battle-tested, widely supported. Perfectly acceptable. Limit: only uses the first **72 bytes** of input. |
| PBKDF2 | Acceptable where FIPS compliance is required; weaker against GPUs. |

For a new project use **argon2id**; use **bcrypt** if you need maximum ecosystem familiarity. Both are shown below.

---

## bcrypt

```bash
npm install bcrypt
```

```js
import bcrypt from "bcrypt";

const SALT_ROUNDS = 12;   // cost factor: each +1 doubles the work

// Hashing (on registration / password change)
const hash = await bcrypt.hash("hunter2", SALT_ROUNDS);
// "$2b$12$Kix8...": algorithm $ cost $ salt+hash

// Verifying (on login)
const ok = await bcrypt.compare("hunter2", hash);   // true
const bad = await bcrypt.compare("wrong", hash);     // false
```

Always use the **async** versions (`hash`, `compare`), not `hashSync` / `compareSync`. Hashing is CPU-intensive on purpose; the sync versions block the event loop (see `03-javascript-for-node/02-event-loop.md` and `15-performance/01-event-loop-performance.md`).

### Choosing the cost factor

Pick the highest value that keeps a single hash around **250–500 ms** on your production hardware. Today that is usually 12 or higher.

```js
// quick benchmark
for (const rounds of [10, 11, 12, 13, 14]) {
  const start = performance.now();
  await bcrypt.hash("benchmark", rounds);
  console.log(rounds, Math.round(performance.now() - start), "ms");
}
```

### The 72-byte limit

bcrypt silently ignores everything past 72 bytes. Cap password length at the API level (e.g. 72 bytes, or 128 characters if you pre-hash), and reject longer input rather than truncating silently.

---

## argon2

```bash
npm install argon2
```

```js
import argon2 from "argon2";

const hash = await argon2.hash("hunter2", {
  type: argon2.argon2id,
  memoryCost: 19 * 1024,   // KiB (19 MiB) — OWASP minimum profile
  timeCost: 2,
  parallelism: 1,
});

const ok = await argon2.verify(hash, "hunter2");   // true / false
```

`argon2.verify` reads the parameters from the hash string itself, so you can change settings later and old hashes still verify.

---

## Registration and login flow

```js
// routes/auth.js
import { Router } from "express";
import bcrypt from "bcrypt";
import { User } from "../models/user.js";

const router = Router();

router.post("/register", async (req, res, next) => {
  try {
    const { email, password } = req.body;

    if (typeof password !== "string" || password.length < 12 || password.length > 72) {
      return res.status(400).json({ error: "Password must be 12–72 characters" });
    }

    const existing = await User.findOne({ email });
    if (existing) {
      return res.status(409).json({ error: "Email already registered" });
    }

    const passwordHash = await bcrypt.hash(password, 12);
    const user = await User.create({ email, passwordHash });

    res.status(201).json({ id: user.id, email: user.email });   // never return the hash
  } catch (err) {
    next(err);
  }
});

// A valid hash of a random string, computed once, used to equalize timing (see below)
const DUMMY_HASH = await bcrypt.hash("dummy-password-for-timing", 12);

router.post("/login", async (req, res, next) => {
  try {
    const { email, password } = req.body;
    const user = await User.findOne({ email });

    // Always run a compare, even if the user doesn't exist
    const hash = user ? user.passwordHash : DUMMY_HASH;
    const ok = await bcrypt.compare(String(password), hash);

    if (!user || !ok) {
      return res.status(401).json({ error: "Invalid email or password" });
    }

    // issue a session (03-sessions.md) or tokens (02-jwt-and-tokens.md)
    res.json({ message: "Logged in" });
  } catch (err) {
    next(err);
  }
});

export default router;
```

Top-level `await` works here because the project uses ES modules (see `01-fundamentals/03-module-system.md`).

---

## Don't leak which part was wrong

```js
// ❌ tells an attacker which emails have accounts
if (!user) return res.status(404).json({ error: "No such user" });
if (!ok)   return res.status(401).json({ error: "Wrong password" });

// ✅ same response either way
return res.status(401).json({ error: "Invalid email or password" });
```

This is **user enumeration**. Registration and password-reset endpoints can leak the same information; for reset, always answer "If that email exists, we sent a link."

### Timing attacks

If you skip the hash comparison when the user doesn't exist, the response is noticeably faster — an attacker can measure that difference. Running `compare` against a dummy hash (as above) keeps timing consistent.

---

## Upgrading hashes on login

Cost factors and algorithms age. When a user logs in successfully you have their plaintext for a moment — use it to upgrade:

```js
const needsUpgrade = bcrypt.getRounds(user.passwordHash) < 12;
if (ok && needsUpgrade) {
  user.passwordHash = await bcrypt.hash(password, 12);
  await user.save();
}
```

---

## Pepper (optional extra layer)

A **pepper** is a secret value kept *outside* the database (env var or secrets manager) and mixed into every password, typically with HMAC before hashing:

```js
import crypto from "node:crypto";

const prehash = crypto
  .createHmac("sha256", process.env.PASSWORD_PEPPER)
  .update(password)
  .digest("base64");                    // 44 chars — also avoids the 72-byte bcrypt limit

const hash = await bcrypt.hash(prehash, 12);
```

If only the database leaks, the attacker also lacks the pepper. The cost: rotating a pepper is hard. Treat it as an extra, not a replacement for salting.

---

## Password policy (what actually helps)

Following current NIST guidance:

- ✅ **Minimum length** (12+ is a good baseline); allow long passphrases
- ✅ Allow all characters, including spaces and Unicode
- ✅ Check against **breached-password lists** (e.g. the Have I Been Pwned range API)
- ✅ Support password managers (don't block paste)
- ❌ Don't force periodic rotation with no evidence of compromise
- ❌ Don't require arbitrary "one symbol, one capital" rules — they push people toward `Password1!`

---

## Password reset flow

1. User submits their email → respond identically whether or not it exists.
2. Generate a **random token** with `crypto.randomBytes(32).toString("hex")`.
3. Store only a **hash** of the token (SHA-256 is fine here — the token is high-entropy) plus an expiry (15–60 minutes).
4. Email a link containing the raw token.
5. On submit: hash the incoming token, look it up, check expiry, set the new password, **delete the token**, and invalidate existing sessions.

```js
import crypto from "node:crypto";

const rawToken = crypto.randomBytes(32).toString("hex");
const tokenHash = crypto.createHash("sha256").update(rawToken).digest("hex");

await ResetToken.create({
  userId: user.id,
  tokenHash,
  expiresAt: new Date(Date.now() + 30 * 60 * 1000),
});
// email: https://example.com/reset?token=<rawToken>
```

Use `crypto.randomBytes`, never `Math.random()`, for anything security-related.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Fast hash (MD5/SHA-*) for passwords | bcrypt / argon2id |
| Logging request bodies | Redact `password` fields in logs (see `14-logging-observability/`) |
| Returning `passwordHash` in API responses | Exclude it in the model's `toJSON` or use explicit DTOs |
| Sync hashing calls | Use async versions |
| Different error for "no such user" | One generic message |
| Comparing hashes with `===` | Use `bcrypt.compare` / `argon2.verify` |

## Next

**`02-jwt-and-tokens.md`** covers what to hand the user after they've logged in successfully.
