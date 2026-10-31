# Email Verification Tokens

A verification token proves one narrow thing: **the person registering controls this email address.** It is a single-purpose, single-use, short-lived opaque token delivered by email.

**Prerequisites:** [01-token-hashing.md](./01-token-hashing.md), [03-opaque-tokens.md](./03-opaque-tokens.md)

---

## Why verify at all

- Stops people registering with someone else's address.
- Makes password reset trustworthy: the reset email only goes to an address the owner has proven.
- Reduces fake accounts and bounced mail.
- Prevents account pre-hijacking, where an attacker registers a victim's email first, then waits for the victim to sign in with a social provider and gets linked into the same account ([account-linking](../06-oauth/04-account-linking.md)).

---

## Flow

```text
Register
   │
   ▼
Create user (isEmailVerified = false)
   │
   ▼
token = randomToken(32)
   ├── raw  ──► emailed link  https://app.com/verify-email?token=RAW
   └── hash ──► Redis  email-verification:{hash} = { userId, email }   TTL 30m

User opens link → frontend page → POST /api/auth/verify-email { token }
   │
   ▼
GETDEL email-verification:{hash}   (atomic, single-use)
   │
   ▼
User.updateOne({ isEmailVerified: true })
```

---

## A reusable one-time-token helper

Verification and reset tokens share the same mechanics, so write them once:

```js
// services/one-time-token.service.js
import { redis } from "../config/redis.js";
import { randomToken, hashToken } from "../utils/crypto.js";

export async function createOneTimeToken(prefix, payload, ttlSeconds) {
  const token = randomToken(32);
  await redis.set(`${prefix}${hashToken(token)}`, JSON.stringify(payload), { EX: ttlSeconds });
  return token; // raw value: send by email, never store
}

export async function consumeOneTimeToken(prefix, token) {
  if (typeof token !== "string" || token.length < 20 || token.length > 200) return null;
  const data = await redis.getDel(`${prefix}${hashToken(token)}`); // atomic read + delete
  return data ? JSON.parse(data) : null;
}
```

`GETDEL` (Redis 6.2+) makes "use once" true even when two requests race with the same token.

---

## Implementation

```js
const VERIFY_PREFIX = "email-verification:";
const VERIFY_TTL = 30 * 60;

export async function sendVerificationEmail(user) {
  const token = await createOneTimeToken(
    VERIFY_PREFIX,
    { userId: user._id.toString(), email: user.email },
    VERIFY_TTL
  );
  const link = `${env.FRONTEND_URL}/verify-email?token=${token}`;
  await sendMail(user.email, "Verify your email", link);
}

export async function verifyEmail(req, res) {
  const data = await consumeOneTimeToken(VERIFY_PREFIX, req.body.token);
  if (!data) return res.status(400).json({ message: "Invalid or expired link" });

  // Bind to the address: only verify if the user's email is still the one the link was sent to
  const result = await User.updateOne(
    { _id: data.userId, email: data.email },
    { isEmailVerified: true }
  );
  if (result.matchedCount === 0) return res.status(400).json({ message: "Invalid or expired link" });

  res.json({ message: "Email verified" });
}
```

Storing `email` in the payload and matching it on update matters when users can change their email: a link sent to the old address must not verify the new one.

---

## Design decisions

| Decision | Choice | Reason |
|---|---|---|
| Verification via `GET` link vs `POST` | Link opens a frontend page; the page sends `POST` | Email security scanners and link previewers often auto-`GET` URLs, which would burn a one-time token before the user clicks |
| Token in query string | Remove from the address bar after reading it | Avoids leaking via history, referrers, analytics |
| TTL | ~30 minutes (or longer, e.g. 24h, if your users are slow to check email) | Product trade-off; the shorter, the safer |
| Hash in Redis | Yes | A Redis leak can't be used to verify accounts |

---

## Resend

```text
POST /api/auth/resend-verification { email }
```

- Return the same generic response whether or not the account exists or is already verified.
- **Rate limit** per email and per IP; otherwise your endpoint is an email-bombing tool ([rate-limiting](../08-hardening/00-rate-limiting.md)).
- Issuing a new token should invalidate older ones. Track the latest hash per user:

```js
// after creating the new token, remember its hash and delete the previous one
// (SET ... GET returns the old value; Redis 6.2+)
const prevHash = await redis.set(`email-verification:user:${userId}`, newHash, {
  EX: VERIFY_TTL,
  GET: true,
});
if (prevHash) await redis.del(`${VERIFY_PREFIX}${prevHash}`);
```

This needs the new token's hash, so either return it from `createOneTimeToken` or compute `hashToken(token)` at the call site. Skipping this step means older links stay valid until they expire, which is low risk.

---

## What verification does *not* do

- It doesn't authenticate the user: don't create a session from the verification link alone, and don't auto-login.
- It doesn't prove ownership of other identifiers (phone numbers need their own flow).
- It's not a second factor.

---

## Common mistakes

- Verifying on `GET` so prefetchers consume the token.
- Reusing one token type for verification and password reset.
- No TTL, or storing the raw token.
- Not rate-limiting resend.
- Flipping `isEmailVerified` for a user whose email has since changed.
- Revealing "this email is already registered/verified" in the response.
- Keeping unverified accounts forever (clean them up after N days).

## Debugging

| Symptom | Cause |
|---|---|
| "Invalid link" when the user clicks first time | A scanner/prefetcher already consumed it (use POST confirmation page) |
| Link works but user stays unverified | Update filter didn't match (email changed or wrong id type) |
| Email never arrives | SMTP/provider config; in dev check Mailpit at `localhost:8025` |
| Link has a broken token | `base64` instead of `base64url`, or mail client wrapped the URL |

---

## Quick summary

- Proves control of an email address, nothing more.
- Random 256-bit token; raw by email, hash in Redis with a TTL; consumed atomically.
- Bind the token to the address; confirm via `POST` from a frontend page.
- Rate-limit resend; keep responses generic.

**Next:** [09-reset-tokens.md](./09-reset-tokens.md)
