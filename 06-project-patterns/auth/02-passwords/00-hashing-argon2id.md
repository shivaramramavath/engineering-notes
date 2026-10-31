# Password Hashing with Argon2id

Passwords are the one credential chosen by humans, so they are guessable. The server therefore never stores them; it stores a deliberately slow, salted hash that makes a database leak expensive to exploit. Argon2id is the current recommended algorithm.

**Prerequisite:** [03-crypto-primitives.md](../01-fundamentals/03-crypto-primitives.md)

---

## Why a normal hash is wrong

SHA-256 is designed to be fast. A GPU can try billions of SHA-256 guesses per second, so a leaked table of SHA-256 password hashes falls to a dictionary attack within minutes.

A password hash must be:

```text
Slow         → each guess costs real time
Memory-hard  → each guess needs lots of RAM (defeats GPUs/ASICs)
Salted       → unique random value per password (defeats rainbow tables
               and "crack one, crack all identical passwords")
```

Argon2id satisfies all three. It won the Password Hashing Competition and is a top OWASP recommendation. `bcrypt` and `scrypt` are acceptable alternatives if Argon2 isn't available; fast hashes are never acceptable.

**Argon2 variants:** `argon2d` (GPU-resistant, side-channel prone), `argon2i` (side-channel resistant), `argon2id` (hybrid). Use `argon2id`.

---

## Basic usage

```bash
npm i argon2
```

```js
import argon2 from "argon2";

const passwordHash = await argon2.hash(password, { type: argon2.argon2id });

const ok = await argon2.verify(passwordHash, password); // true | false
```

Store `passwordHash` in MongoDB. That is all you need: salt and parameters are embedded in the output string.

```text
$argon2id$v=19$m=65536,t=3,p=4$<salt>$<hash>
   │        │       │               │      └─ derived key
   │        │       │               └─ random per-password salt
   │        │       └─ cost parameters
   │        └─ version
   └─ algorithm
```

Because the parameters live in the hash, `verify` needs only the stored string and the candidate password. You can change parameters later without breaking existing hashes.

---

## Cost parameters

| Option | Meaning |
|---|---|
| `memoryCost` | Memory in KiB (`65536` = 64 MiB) |
| `timeCost` | Number of passes |
| `parallelism` | Threads/lanes |

```js
const options = {
  type: argon2.argon2id,
  memoryCost: 2 ** 16, // 64 MiB
  timeCost: 3,
  parallelism: 1,
};
```

How to choose: tune to your hardware so a single hash takes roughly **100-500 ms** (a common target for login), without exhausting memory under concurrent logins. OWASP lists minimum configurations such as 19 MiB / 2 iterations / 1 lane; stronger is better if the server can afford it. Check the current guidance rather than copying numbers from a tutorial.

Capacity math matters: 64 MiB per hash x 50 simultaneous logins = about 3.2 GB of RAM. Hashing also runs in the libuv thread pool, so a burst of logins can saturate it and slow unrelated requests. This is why login endpoints need rate limiting.

---

## Upgrading parameters over time

Hardware improves, so cost should rise. Rehash transparently at login, the only moment you hold the plaintext:

```js
if (await argon2.verify(user.passwordHash, password)) {
  if (argon2.needsRehash(user.passwordHash, options)) {
    user.passwordHash = await argon2.hash(password, options);
    await user.save();
  }
}
```

---

## Peppers (optional)

A **pepper** is a secret value, stored outside the database (env or secret manager), mixed into the hash so a database-only leak is useless. Argon2 supports it as the `secret` option:

```js
const hash = await argon2.hash(password, {
  type: argon2.argon2id,
  secret: Buffer.from(process.env.PASSWORD_PEPPER, "base64url"),
});
// verify must pass the same secret
```

Trade-off: rotating a pepper is hard (you can't rehash without plaintext). Treat it as extra defense in depth, not a replacement for good hashing. Skip it if you can't manage the operational cost.

---

## Common mistakes

- Using SHA-256/MD5, or `bcrypt` with a low cost, "because it's simpler".
- Hashing on the client instead of the server. A client-side hash simply becomes the password. Send the password over HTTPS and hash on the server.
- Truncating or silently altering passwords before hashing (bcrypt truncates at 72 bytes; Argon2 doesn't, but still enforce a sane maximum length to prevent DoS).
- Comparing hashes yourself instead of calling `argon2.verify`.
- Logging the password or hash.
- Setting cost so low that hashing takes 5 ms.

## Debugging

| Symptom | Cause |
|---|---|
| `verify` always false | Wrong stored value (hash was trimmed, re-hashed, or stored in a field with a size limit) |
| Install/build error for `argon2` | Native addon; ensure a supported Node version and build tools, or use a prebuilt-capable version |
| Login latency spikes | Cost too high for concurrency; lower it or move to rate limits and a larger pool |
| Throws on `verify` | Hash string is not a valid Argon2 hash (e.g. `undefined` passwordHash) |

---

## Quick summary

- Passwords get slow, memory-hard, salted hashes: Argon2id.
- Hash and salt live together in one string; store only that.
- Tune cost to ~100-500 ms and watch memory under concurrency.
- Rehash at login when parameters are outdated.
- Hash on the server, never on the client; never log either value.

**Next:** [01-password-policy.md](./01-password-policy.md)
