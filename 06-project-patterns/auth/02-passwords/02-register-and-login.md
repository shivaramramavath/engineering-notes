# Register and Login

The two endpoints that start every account's life. This note builds them on the architecture so far: Argon2id hashes in MongoDB, a Redis session per login, a refresh cookie, and an access JWT returned to React.

**Prerequisites:** [00-hashing-argon2id.md](./00-hashing-argon2id.md), [01-password-policy.md](./01-password-policy.md), [data ownership](../01-fundamentals/04-data-ownership.md)

---

## Register

```text
POST /api/auth/register  { email, password }
        │
        ▼
Validate (zod, policy, breach check)
        │
        ▼
Argon2id hash → create User (isEmailVerified: false)
        │
        ▼
Create verification token → email it
        │
        ▼
Generic 201 response (no session yet)
```

```js
// controllers/auth.controller.js
export async function register(req, res) {
  const { email, password } = registerSchema.parse(req.body);

  if (await isBreached(password)) {
    return res.status(400).json({ message: "That password has appeared in a data breach. Choose another." });
  }

  const passwordHash = await argon2.hash(password, hashOptions);

  try {
    const user = await User.create({ email, passwordHash });
    await sendVerificationEmail(user); // see 03-tokens/08-email-verification-tokens.md
  } catch (err) {
    if (err.code !== 11000) throw err; // 11000 = duplicate key (email exists)
    // swallow: do not reveal that the account already exists
  }

  res.status(201).json({
    message: "If this email can be registered, a verification link has been sent.",
  });
}
```

Key points:

- **The `unique` index is the real guard.** Checking `findOne` then `create` has a race condition; two simultaneous requests can both pass the check. Let MongoDB reject the duplicate (`E11000`) instead.
- **Enumeration:** responding "email already registered" reveals who has an account. The response above is identical either way. (If you do show it, accept that registration is enumerable and rate-limit accordingly. Many products consciously make that UX trade-off.)
- Normalize email (`lowercase`, `trim`) via the schema/model so `A@x.com` and `a@x.com` collide.
- Never accept `role` or `isEmailVerified` from the request body (mass assignment). Take only `email` and `password`.

---

## Login

```text
POST /api/auth/login  { email, password }
        │
        ▼
Find user by email
        │
        ▼
Argon2id verify (always run, even if user not found)
        │
        ▼
Check isActive / verification policy
        │
        ▼
Create Redis session + refresh token → HttpOnly cookie
        │
        ▼
Return access JWT in JSON
```

```js
const DUMMY_HASH = await argon2.hash("dummy-password", hashOptions); // computed once at startup

export async function login(req, res) {
  const { email, password } = loginSchema.parse(req.body);

  const user = await User.findOne({ email });

  // Verify against a dummy hash when the user doesn't exist, so response
  // time doesn't reveal whether the email is registered.
  const valid = await argon2.verify(user?.passwordHash ?? DUMMY_HASH, password);

  if (!user || !valid || !user.isActive) {
    return res.status(401).json({ message: "Invalid email or password" });
  }

  if (!user.isEmailVerified) {
    return res.status(403).json({ message: "Please verify your email first" });
  }

  const { refreshToken, sessionId } = await createSession(user._id.toString());
  const accessToken = signAccessToken(user, sessionId);

  setRefreshCookie(res, refreshToken);
  res.json({ accessToken });
}
```

`createSession` is the service from [redis-sessions](../04-sessions/01-redis-sessions.md): it generates `sessionId.secret`, stores `SHA-256(secret)` in `session:{id}`, adds the id to `user:sessions:{userId}`, and sets the TTL. `setRefreshCookie` applies the cookie flags from [cookie-sessions](../04-sessions/02-cookie-sessions.md).

---

## Security details that matter

| Detail | Why |
|---|---|
| Same `401` + same message for unknown email and wrong password | Prevents account enumeration |
| Verify against a dummy hash | Equalizes timing; Argon2 time otherwise leaks existence |
| New session on every login | Prevents session fixation; each device is independent |
| Rate limit by IP **and** by account | Stops brute force and credential stuffing ([rate-limiting](../08-hardening/00-rate-limiting.md)) |
| Check `isActive` after the password | Disabled accounts shouldn't reveal their state to guessers |
| Don't log the request body | It contains the password |

The `403` for unverified email is a deliberate disclosure: it is only shown after the correct password was supplied, so it doesn't help an attacker who doesn't know the password. Whether to block unverified users at all is a product decision.

If MFA is enabled for the user, login returns a short-lived "MFA required" state **instead of** creating the session; the session is created only after the second factor succeeds ([mfa-overview](../05-mfa/00-mfa-overview.md)).

---

## Responses and status codes

| Case | Status | Body |
|---|---|---|
| Register accepted | 201 | Generic message |
| Validation failed | 400 | Field errors (safe to be specific) |
| Wrong credentials / unknown email / disabled | 401 | "Invalid email or password" |
| Email not verified (after correct password) | 403 | Verification prompt |
| Too many attempts | 429 | `Retry-After` header |

---

## Frontend side

Store the access token in memory only, and let the browser keep the refresh cookie. The request must send credentials:

```js
const res = await fetch("/api/auth/login", {
  method: "POST",
  credentials: "include", // required for the Set-Cookie to be stored
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ email, password }),
});
const { accessToken } = await res.json();
setAccessToken(accessToken); // module variable / React state, not localStorage
```

---

## Common mistakes and debugging

| Symptom | Cause |
|---|---|
| Cookie not set in browser | Missing `credentials: "include"`, CORS `credentials: true`, or an origin mismatch |
| Duplicate users with different-case emails | Email not lowercased before unique index |
| Login time reveals unknown emails | Skipped the dummy-hash verify |
| Two accounts created simultaneously | Relied on `findOne` check instead of unique index |
| Password visible in server logs | Request body logging enabled on auth routes |
| User can set `role: "admin"` at signup | Spread `req.body` into `User.create` |

---

## Quick summary

- Register: validate, hash, create (unique index guards duplicates), send verification, return a generic response.
- Login: always run a verify (dummy hash if no user), one generic `401`, then create a session and return the access token.
- Each login is its own session; refresh token goes in the cookie, access token in memory.
- Rate-limit both endpoints and never log credentials.

**Next:** [03-password-change-and-reset.md](./03-password-change-and-reset.md)
