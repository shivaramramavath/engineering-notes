# Password Hashing

You must never store passwords, or anything from which they can be recovered. You store a **password hash** produced by a deliberately slow, salted algorithm, so that a stolen database doesn't immediately become a list of working passwords. This note covers which algorithm to use, how to wire it into Nest, how to evolve parameters over time, and the related problem of storing reset tokens.

Prerequisites: [Authentication architecture](./01-authentication-architecture.md), [Passport local strategy](./02-passport-and-local-strategy.md).

> Algorithm recommendations and cost parameters change as hardware improves. The values below reflect common current guidance (for example OWASP's password storage recommendations), but check the current guidance and benchmark on your own hardware.

## Hashing vs encryption vs "fast hashes"

| Approach | Reversible? | Suitable for passwords? |
|----------|-------------|-------------------------|
| Plain text | n/a | **Never** |
| Encryption (AES, ...) | Yes, with the key | **No**: key theft exposes every password |
| Fast hash (MD5, SHA-1, SHA-256) | No, but trivially brute-forced | **No**: billions of guesses per second on a GPU |
| **Slow, salted password hash** (Argon2id, bcrypt, scrypt) | No, and costly to brute-force | **Yes** |

Properties you want:

- **One-way:** you verify by hashing the attempt and comparing, never by decrypting.
- **Salted:** a unique random salt per password defeats rainbow tables and makes identical passwords hash differently. Modern libraries generate and embed the salt for you.
- **Slow and tunable:** a cost factor (time/memory) you can raise as hardware improves.

## Choosing an algorithm

| Algorithm | Notes |
|-----------|-------|
| **Argon2id** | Current first choice: memory-hard, resists GPU/ASIC attacks. npm package `argon2` |
| **bcrypt** | Mature, widely supported. Cost factor 10-12+ (benchmark). **Truncates input at 72 bytes**. npm packages `bcrypt` (native) or `bcryptjs` (pure JS, slower) |
| **scrypt** | Memory-hard; built into Node's `crypto` (`crypto.scrypt`), but you handle encoding/params yourself |
| PBKDF2 | Acceptable where FIPS compliance requires it; needs very high iteration counts |

Default to **Argon2id**. Use bcrypt if you need maximum compatibility or can't build native Argon2 in your environment. Don't use plain SHA-family hashes, even "salted and iterated" by hand.

## Argon2 in Nest

```bash
npm i argon2
```

Wrap the library in a provider so the algorithm is swappable and testable ([tokens and abstractions](../../03-core-concepts/04-modules-and-di/06-injection-tokens-and-optional-dependencies.md)):

```ts
// auth/password-hasher.ts
import { Injectable } from '@nestjs/common';
import * as argon2 from 'argon2';

@Injectable()
export class PasswordHasher {
  private readonly options: argon2.Options = {
    type: argon2.argon2id,
    memoryCost: 19 * 1024,    // KiB (≈19 MiB): a commonly cited minimum for Argon2id
    timeCost: 2,              // iterations
    parallelism: 1,
  };

  hash(password: string) {
    return argon2.hash(password, this.options);       // returns a self-describing string
  }

  async verify(hash: string, password: string) {
    try {
      return await argon2.verify(hash, password);     // constant-time comparison inside
    } catch {
      return false;                                   // malformed hash etc.: treat as failure
    }
  }

  needsRehash(hash: string) {
    return argon2.needsRehash(hash, this.options);
  }
}
```

The stored string looks like `$argon2id$v=19$m=19456,t=2,p=1$<salt>$<hash>`: it **embeds the algorithm, parameters, and salt**, so one `varchar`/`text` column is enough and verification needs nothing else. Pick parameters by benchmarking: aim for roughly 50-500 ms per hash on your production hardware, within your memory and concurrency budget.

### bcrypt alternative

```ts
import * as bcrypt from 'bcrypt';

await bcrypt.hash(password, 12);              // cost factor (benchmark; higher = slower)
await bcrypt.compare(password, storedHash);   // constant-time compare
```

bcrypt silently ignores bytes after 72. Two different long passwords sharing the first 72 bytes verify the same, and passwords with NUL bytes can truncate early in some implementations. Enforce a maximum length (for example 72 bytes) in validation, or pre-hash carefully (a base64-encoded SHA-256 of the password, then bcrypt) if you need longer passphrases. Prefer Argon2 to avoid the problem.

## Using it: registration and login

```ts
// register
async register(dto: RegisterDto) {
  const passwordHash = await this.hasher.hash(dto.password);
  return this.users.create({ email: dto.email.toLowerCase(), passwordHash });
}
```

```ts
// login verification (see the local strategy note for the dummy-hash detail)
const ok = await this.hasher.verify(user?.passwordHash ?? DUMMY_HASH, password);
if (ok && this.hasher.needsRehash(user.passwordHash)) {
  await this.users.updatePasswordHash(user.id, await this.hasher.hash(password));  // upgrade silently
}
```

`DUMMY_HASH` is a hash of any random string, created **once** with the real parameters, so unknown-user logins cost the same as real ones (prevents timing-based [account enumeration](./02-passport-and-local-strategy.md)).

## Evolving over time: rehash on login

Hardware improves; your parameters should too. Since you only see the plaintext password at login, **upgrade hashes then**: after a successful verification, if `needsRehash` says the stored hash used weaker parameters (or an old algorithm), hash again and store the new value. Users who never log in keep old hashes, so consider forcing a reset for very old ones.

Migrating **between algorithms** (bcrypt → Argon2id) uses the same trick, relying on the self-describing prefix to choose a verifier:

```ts
async verify(hash: string, password: string) {
  if (hash.startsWith('$argon2')) return argon2.verify(hash, password);
  if (hash.startsWith('$2')) return bcrypt.compare(password, hash);   // legacy bcrypt
  return false;
}
// after a successful legacy verify: rehash with Argon2id and store
```

## Password policy

Current guidance (for example NIST SP 800-63B) favors **length and breach checking** over composition rules:

- Minimum length around 8 (many teams choose 10-12+); allow long passphrases (at least 64 characters, subject to your hashing limits).
- **Don't require** arbitrary composition rules (one uppercase, one symbol), which push users toward predictable patterns, and **don't force periodic rotation** without evidence of compromise.
- **Check against known-breached passwords** (for example via the Have I Been Pwned range API with k-anonymity, or a local list) and reject common ones.
- Allow paste and password managers; support all printable characters and Unicode (normalize consistently).
- Validate in the DTO (`@MinLength`, `@MaxLength`) so abuse and DoS inputs are rejected before hashing ([class-validator](../../03-core-concepts/02-validation-and-serialization/03-class-validator.md)).

Add 2FA for meaningful protection beyond passwords ([two-factor authentication](./10-two-factor-authentication.md)).

## Hashing is expensive: protect your CPU

Each Argon2/bcrypt call deliberately burns CPU/memory. Native implementations run on the libuv **thread pool** (default size 4), so a flood of login requests can queue behind hashing and also starve other thread-pool work (file system, DNS, crypto). Mitigate:

- **Rate-limit** login/registration/reset endpoints ([rate limiting](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)).
- Cap password length (so nobody submits 10 MB "passwords").
- Consider raising `UV_THREADPOOL_SIZE` and budgeting memory (Argon2 uses `memoryCost` per concurrent hash).
- Never hash on the request path in a hot loop (bulk imports): batch with a concurrency limit.

## Peppers (optional)

A **pepper** is a secret value (kept outside the database, in a secret manager/HSM) mixed into hashing, for example by HMAC-ing the password with it before hashing. It helps if only the database leaks, but complicates rotation and recovery. If you use one, store it in [secrets management](../../07-production/01-security/06-secrets-management.md), version it, and keep the pepper version alongside the hash. Skip it unless you have operational maturity to manage it.

## Reset and verification tokens (related storage problem)

Password-reset and email-verification links carry a secret token. Treat it like a password-equivalent:

```ts
import { randomBytes, createHash } from 'node:crypto';

const token = randomBytes(32).toString('base64url');                 // sent to the user (in the link)
const tokenHash = createHash('sha256').update(token).digest('hex');  // stored in the database

await this.resets.create({ userId, tokenHash, expiresAt: new Date(Date.now() + 30 * 60_000) });
```

On use: hash the presented token, look it up, check expiry and "unused", then **mark it used** (atomically, such as `UPDATE ... WHERE used_at IS NULL`) and change the password. Properties:

- Generated with a **CSPRNG** (`crypto.randomBytes`), at least 128 bits.
- **Hashed at rest** (SHA-256 is fine here because the token is high-entropy, unlike a password).
- **Short expiry** (15-60 minutes), **single use**, and invalidated when the password changes.
- Response to "forgot password" is identical whether or not the email exists.
- After a reset, **revoke existing sessions/refresh tokens** ([refresh tokens](./05-refresh-tokens-and-logout.md)).

## Where hashing logic lives

- In a dedicated provider (`PasswordHasher`) used by `AuthService`/`UsersService`, not scattered in controllers.
- **Explicitly in the service**, rather than in entity hooks like `@BeforeInsert`: ORM hooks don't run for bulk/update paths, so a plaintext password can slip through ([entity hooks](../03-typeorm/02-entities.md), [Mongoose hooks](../05-mongoose/05-middleware-hooks.md)).
- Never log request bodies on auth routes; exclude `password` from logs and error reports.

## Testing

- Unit tests: `hash` produces different outputs for the same input; `verify` accepts the right password and rejects wrong ones; `needsRehash` flags old parameters.
- Use **cheap parameters in tests** (override `memoryCost`/`timeCost` or the bcrypt cost) so the suite stays fast; keep production parameters in config.
- Never assert on a specific hash value (salts vary).

## Common mistakes

- **Fast hashes** (SHA-256/MD5) or hand-rolled "salt + iterate" schemes.
- **Encrypting passwords** instead of hashing.
- **Reusing one global salt**, or generating your own instead of using library defaults.
- **bcrypt with passwords over 72 bytes** and no length limit.
- **Hashing in entity hooks** that don't run for updates.
- **No rehash path**, leaving old weak hashes forever.
- **Unbounded password length**, enabling CPU DoS.
- **Composition-rule policies without breach checking.**
- **Storing reset tokens in plaintext**, or making them reusable/long-lived.
- **Different timing/response for unknown accounts.**
- **Logging passwords or full request bodies.**

## Debugging

- `argon2.verify` throws or returns false for valid passwords: the stored value is truncated (column too short; use `text` or at least 255 chars), or came from a different algorithm.
- Hashing is far slower in production than locally: container CPU limits or memory pressure; re-benchmark and tune.
- bcrypt/argon2 native build failures in Docker: missing build tools or a platform mismatch; use a matching base image, install prebuilt binaries, or switch libraries.
- Logins queue up under load: thread-pool saturation from hashing; rate-limit and consider more threads/instances.
- Old users can't log in after switching algorithms: your verify path doesn't handle the legacy prefix.

## Quick Summary

- Store a **slow, salted hash** (Argon2id first choice; bcrypt acceptable), never plaintext, encryption, or fast hashes.
- The hash string embeds algorithm, parameters, and salt; use a `PasswordHasher` provider and verify with the library's constant-time compare.
- Rehash on login when parameters are outdated; migrate algorithms via the hash prefix.
- Policy: length and breach checks, not composition rules; bound length to prevent CPU DoS; rate-limit.
- Reset/verification tokens: random, hashed at rest, short-lived, single-use; revoke sessions after reset.
- Hash explicitly in services, not in ORM hooks.

## Next

[JWT →](./04-jwt.md)
