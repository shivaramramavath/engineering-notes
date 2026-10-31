# Enumeration Protection

**Account enumeration** is learning which emails/usernames have accounts by watching how your system responds differently. It turns credential stuffing from a blind guess into a targeted attack, enables phishing and spam of confirmed users, and can leak sensitive facts ("this email has an account on a medical service").

**Prerequisites:** [02-register-and-login.md](../02-passwords/02-register-and-login.md), [00-rate-limiting.md](./00-rate-limiting.md)

---

## The principle

> For any request an unauthenticated visitor can make, **the observable result must not depend on whether the account exists.**

"Observable" is broader than the message text:

| Channel | Leak example |
|---|---|
| Message | "No account with that email" vs "Wrong password" |
| Status code | `404` vs `401`, `409` vs `201` |
| Response body shape/size | Extra fields, different lengths |
| **Timing** | Existing accounts take ~200 ms (hash verified), unknown accounts return in 2 ms |
| Side effects | Existing users receive an email, unknown ones don't, visible to the attacker if they own the mailbox |
| Headers/cookies | `Set-Cookie` only on one branch |
| Rate-limit behavior | Only real accounts get locked out |
| Redirects / UI flow | Different next page ("enter password" vs "create account") |

---

## Where it leaks, and the fix

### Login

```js
// ❌ Leaks twice: message and timing
const user = await User.findOne({ email });
if (!user) return res.status(404).json({ message: "No account with that email" });
if (!(await argon2.verify(user.passwordHash, password))) return res.status(401).json({ message: "Wrong password" });
```

```js
// ✅ One message, one status, same work either way
const DUMMY_HASH = await argon2.hash("not-a-real-password", hashOptions);   // computed once, same parameters as real hashes

const user = await User.findOne({ email }).select("+passwordHash");
const valid = await argon2.verify(user?.passwordHash ?? DUMMY_HASH, password);
if (!user || !valid) return res.status(401).json({ message: "Invalid email or password" });
```

The dummy hash must use **the same Argon2 parameters** as real ones, or the timing difference remains. Rehash the dummy when you change parameters.

### Forgot password

```js
export async function forgotPassword(req, res) {
  const { email } = forgotSchema.parse(req.body);
  const user = await User.findOne({ email });

  if (user) {
    const token = await createOneTimeToken("password-reset:token:", { userId: user.id }, 15 * 60);
    queueEmail("password-reset", user.email, { token });      // NOT awaited / handed to a queue
  }

  res.json({ message: "If an account exists for this email, a reset link has been sent." });
}
```

Sending the email inline would make existing accounts slower (an SMTP round trip) than unknown ones: a timing leak. Use a job queue (BullMQ, SQS) or a fire-and-forget call with error handling, so the request returns in the same time on both branches. The DB lookup and token creation difference is small but measurable; for strict equality, always perform a comparable amount of work (for example create and discard a token) or enforce a **minimum response time**.

### Registration

The hardest one, because the natural error ("email already registered") is exactly the leak.

| Approach | UX | Enumeration |
|---|---|---|
| Say "email already in use" | Best | **Leaks** |
| Always respond "check your email" and **email the existing owner** ("someone tried to register with your address; sign in or reset your password") | Slightly worse | Safe |
| Email-first flow: ask for email, send a verification link, collect the password only after the link is clicked | Neutral | Safe |

```js
const user = await User.findOne({ email });
if (user) queueEmail("already-registered", email);                 // tells the real owner
else { /* create user, queue verification email */ }
res.status(202).json({ message: "If this email can be registered, check your inbox to continue." });
```

If you decide the UX cost isn't worth it (many products make that call), do it knowingly and add strict rate limits and bot protection on the endpoint, since you've chosen to be enumerable.

### Verification, reset and resend endpoints

- `POST /resend-verification`: same generic response whether the account exists or is already verified.
- `POST /reset-password` with an invalid token: one generic error, same for "expired", "used" and "never existed".
- `GET /verify-email?token=`: don't distinguish "unknown user" from "bad token".

### MFA and recovery

- Don't reveal whether MFA is enabled **before** the password is verified.
- A wrong code and an expired challenge produce the same error.
- Recovery flows ("forgot MFA device") follow the same generic-response rule.

### OAuth

- "Sign in with Google" for an email without a local account: create the account or show a neutral message; don't say "no account found" to unauthenticated visitors. See [04-account-linking.md](../06-oauth/04-account-linking.md).

### Helper endpoints

| Endpoint | Fix |
|---|---|
| `GET /api/users/check-email?email=` ("is it taken?") for live form validation | Remove, or put behind CAPTCHA and tight limits; better, validate on submit |
| `GET /api/users/:id` returning `404` for missing and `403` for others | Return the same `404` for both ([01-permissions-and-policies.md](../07-authorization/01-permissions-and-policies.md)) |
| Sequential numeric ids | Use random ids (ObjectId/UUID), though ObjectIds contain a timestamp and aren't secrets |
| Public profile search by email/phone | Require authentication, exact-match limits |

---

## Rate limiting must not be a side channel

If the limiter only counts attempts against **existing** accounts, then "locked out" reveals the account exists. Count and lock identifiers regardless of whether they exist, and use the same response ([00-rate-limiting.md](./00-rate-limiting.md), [02-account-lockout.md](./02-account-lockout.md)).

---

## Equalizing timing

Techniques, from simple to strict:

1. **Always do the expensive operation** (dummy hash verify).
2. **Move variable-time side effects out of the request** (queue emails).
3. **Pad to a minimum duration**:

```js
const MIN_MS = 300;

// The handler RETURNS { status, body } instead of responding; the wrapper pads, then sends.
export const withMinDuration = (handler) => async (req, res, next) => {
  const start = performance.now();
  try {
    const { status, body } = await handler(req);
    const remaining = MIN_MS - (performance.now() - start);
    if (remaining > 0) await new Promise((r) => setTimeout(r, remaining));
    res.status(status).json(body);
  } catch (err) {
    next(err);
  }
};
```

Padding has to happen **before** the response is written, which is why the handler returns its result rather than calling `res.json` itself. Choose `MIN_MS` above your normal worst case, or slow requests still stand out. Treat padding as a last layer on top of equal work, not a replacement for it.

4. **Same code path**: share one function for both branches so a future change can't make one faster.

Network jitter hides small differences from casual attackers, but statistical attacks (many samples) can recover them, so measure.

### Measure it

```js
// scripts/timing-check.js: median latency for a known vs unknown account
import { performance } from "node:perf_hooks";

async function median(email, n = 200) {
  const times = [];
  for (let i = 0; i < n; i++) {
    const t = performance.now();
    await fetch("http://localhost:4000/api/auth/login", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ email, password: "wrong-password-123" }),
    });
    times.push(performance.now() - t);
  }
  return times.sort((a, b) => a - b)[n >> 1];
}

console.log("known  :", await median("alice@example.com"));
console.log("unknown:", await median("nobody-here@example.com"));
```

(Use a test environment with rate limits relaxed or disabled for this script.) If medians differ by more than a few milliseconds, there's a leak.

---

## What generic messages should say

| Endpoint | Message |
|---|---|
| Login | "Invalid email or password." |
| Forgot password | "If an account exists for this email, a reset link has been sent." |
| Register (safe mode) | "If this email can be registered, check your inbox to continue." |
| Resend verification | "If the account exists and needs verification, we've sent a new email." |
| Bad/expired token | "This link is invalid or has expired." |

Specific messages are fine for **validation** errors that don't depend on account state ("Password must be at least 12 characters") and for authenticated users.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Different messages for "no user" / "wrong password" | Direct enumeration |
| Returning early when the user isn't found | Timing leak |
| Dummy hash with different Argon2 parameters | Timing leak persists |
| Awaiting SMTP inside the handler only for real users | Timing leak |
| `409 Conflict` on registration | Direct enumeration |
| Real-time "email taken" checks | Free bulk enumeration API |
| Only locking existing accounts | Lock status reveals existence |
| Different `Set-Cookie` or response size per branch | Side-channel |
| Writing the specific reason to the **response** instead of the log | Leak (log specifics server-side only) |
| Leaking via 2FA prompts ("enter your code") before the password step | MFA status disclosure |

## Debugging

| Symptom | Cause |
|---|---|
| Timing test shows a gap | Early return, missing dummy hash, or awaited email |
| Users complain they "didn't get the reset email" with a generic message | Expected UX trade-off: add helper text ("check spam; make sure you used the email you signed up with") |
| Can't tell why a login failed | Log the real reason server-side with the request id; keep the response generic |

---

## Quick summary

- The visible result of any anonymous request must not depend on account existence: message, status, body, timing, side effects.
- Login: dummy-hash verify and one error. Reset/resend: generic response, email via queue. Registration: tell the *owner* by email, not the visitor.
- Limit and lock unknown identifiers the same as known ones.
- Measure timing; don't assume.

**Next:** [02-account-lockout.md](./02-account-lockout.md)
