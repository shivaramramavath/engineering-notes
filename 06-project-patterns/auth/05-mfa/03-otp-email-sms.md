# Email and SMS OTP

A one-time code sent over a channel the user already has: an email inbox or a phone number. It requires no app and is familiar to everyone, which makes it popular as a first MFA option and as a recovery/verification step. It is also the weakest common method, so understand exactly what it does and doesn't protect.

**Prerequisites:** [00-mfa-overview.md](./00-mfa-overview.md), [00-secure-random-tokens.md](../03-tokens/00-secure-random-tokens.md)

---

## How it differs from TOTP

```text
TOTP:   code derived from a shared secret + time; works offline; no message sent
OTP:    server generates a random code, stores it, sends it; user echoes it back
```

Because the server sends something, it must store state: the code, an expiry, an attempt counter.

---

## Strength of each channel

| Channel | Threats | Verdict |
|---|---|---|
| **Email** | Anyone who owns the mailbox gets the code. If email is also the reset channel, it adds little as a *second* factor | Fine for low-risk verification and passwordless "magic link"; weak as MFA |
| **SMS** | SIM swapping, SS7/interception, phone-number recycling, real-time phishing; cost abuse ("SMS pumping" toll fraud) | NIST classifies PSTN as a *restricted* authenticator. Better than nothing; avoid as the only option for high-risk accounts |
| **TOTP / passkeys** | Much harder to intercept or redirect | Prefer |

Everything here can be phished in real time, like TOTP.

---

## Generating the code

6 digits is only about 20 bits of entropy. That's acceptable **only** because of strict limits on lifetime and attempts:

```js
import crypto from "crypto";

const code = String(crypto.randomInt(0, 1_000_000)).padStart(6, "0");   // uniform, no modulo bias
```

| Control | Value | Why |
|---|---|---|
| TTL | 5 minutes (≤ 10) | Shrinks the guessing window |
| Max attempts | 5 per code | 5 / 1,000,000 = 0.0005% chance per code |
| Single-use | Delete on success | No replay |
| Resend cooldown | 30-60 s, plus an hourly cap | Stops spamming and cost abuse |
| Bound to | user **and** purpose (`login`, `verify-phone`, ...) | A code for one flow can't be used for another |
| New code | Invalidates the previous one | Only one valid code at a time |

---

## Storage

The code is low-entropy, so a plain SHA-256 hash in Redis can be reversed instantly by anyone who sees the hash (a million candidates). Use an HMAC with a server-side secret, so a Redis dump alone is not enough:

```js
const otpHmac = (userId, purpose, code) =>
  crypto.createHmac("sha256", process.env.OTP_KEY).update(`${userId}:${purpose}:${code}`).digest("hex");
```

Including `userId` and `purpose` in the HMAC input binds the code to its context.

```text
otp:{purpose}:{userId}  (Hash, TTL 300 s)
   codeHmac   "ab12..."
   attempts   0
   sentTo     "a***@example.com"   (optional, for audit/UX)
```

---

## Send

```js
export async function sendOtp(user, purpose, channel) {
  const key = `otp:${purpose}:${user.id}`;

  // Cooldown: refuse to send again within 60 seconds
  const cool = await redis.set(`otp:cooldown:${purpose}:${user.id}`, "1", { NX: true, EX: 60 });
  if (!cool) throw new AuthError("Please wait before requesting another code", 429);

  const code = String(crypto.randomInt(0, 1_000_000)).padStart(6, "0");

  await redis
    .multi()
    .del(key)                                                     // invalidate any previous code
    .hSet(key, { codeHmac: otpHmac(user.id, purpose, code), attempts: 0 })
    .expire(key, 300)
    .exec();

  if (channel === "email") await sendMail(user.email, "Your sign-in code", `Your code is ${code}. It expires in 5 minutes.`);
  else await sendSms(user.phone, `Your YourApp code is ${code}. Don't share it.`);
}
```

`SET ... NX EX` is an atomic "create if not exists" so two simultaneous sends can't both pass the cooldown.

Message hygiene: say what the code is for, how long it lasts, and "never share it". For web-based SMS autofill the Web OTP API format ends with `@yourapp.com #123456`; check current docs for the exact format.

---

## Verify

```js
export async function verifyOtp(userId, purpose, input) {
  const key = `otp:${purpose}:${userId}`;

  const stored = await redis.hGet(key, "codeHmac");
  if (!stored) return false;                                      // expired / never sent (check first: HINCRBY would create a TTL-less key)
  const attempts = await redis.hIncrBy(key, "attempts", 1);       // atomic: counts every try, before comparing
  if (attempts > 5) { await redis.del(key); return false; }       // exhausted

  const ok = safeCompare(otpHmac(userId, purpose, input), stored);
  if (!ok) return false;

  // Single-use: only the request whose DEL actually removed the key wins
  return (await redis.del(key)) === 1;
}
```

Notes:

- `hIncrBy` runs **before** the comparison, so parallel guessing can't dodge the counter.
- The final `del === 1` check handles the case where two requests both entered the right code at once.
- Validate input shape (`/^\d{6}$/`) before touching Redis.
- Respond with one generic "Invalid or expired code", no matter the reason.

---

## SMS-specific concerns

| Concern | Mitigation |
|---|---|
| **SMS pumping / toll fraud:** bots trigger sends to premium-rate numbers you pay for | Rate-limit per IP/account/number prefix, require a verified account or CAPTCHA before sending, geo-restrict, monitor cost spikes |
| **SIM swap** | Treat SMS as lower assurance; alert on phone-number changes; enforce a delay on SMS-enabled recovery |
| **Number recycling** | Re-verify phone numbers periodically; don't trust a number forever |
| **Enumeration** via different responses | Same response whether or not the number/account exists |
| **Phone number is personal data** | Store minimally, mask in UI (`+91 ••••• ••123`) |
| **Delivery failures** | Offer a fallback method; don't lock users out |

Use a reputable provider (Twilio Verify, AWS SNS, etc.); many offer built-in code generation, rate limiting and fraud controls, which can be safer than rolling your own. Review their current docs and costs.

---

## Email OTP vs magic link

| | Email OTP | Magic link |
|---|---|---|
| User action | Copy 6 digits back | Click a link |
| Phishing/relay | Phishable | Phishable; link prefetch can burn it |
| Same-device requirement | No | Often opens on a different device than the browser that asked |
| Entropy | Low (needs attempt limits) | High (256-bit token, no guessing issue) |

For passwordless sign-in, a long random link token (see [09-reset-tokens.md](../03-tokens/09-reset-tokens.md) for the same design) has more entropy than a 6-digit OTP, but handle email scanner prefetching via a confirmation page.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| `Math.random()` for codes | Predictable |
| No attempt limit | 1,000,000 guesses is trivial to automate |
| Plain SHA-256 of a 6-digit code | Offline reversal in milliseconds |
| Code not bound to user/purpose | Cross-flow reuse |
| Old codes remain valid after resend | Multiple valid codes at once |
| Not single-use | Replay |
| Distinct errors for "wrong" vs "expired" vs "no such user" | Enumeration and oracle |
| Unlimited SMS sends | Bill shock |
| Offering SMS as the only recovery for admin accounts | SIM-swap takeover |

## Debugging

| Symptom | Cause |
|---|---|
| Correct code rejected | Expired (check TTL), HMAC inputs differ (leading zero lost: use `padStart`), code was invalidated by a resend |
| Always "too many attempts" | Counter incremented by retries/double-submits; disable the button after click |
| Email arrives late | Provider queue; in dev, use Mailpit ([02-dev-tooling.md](../00-setup/02-dev-tooling.md)) |
| Works for some SMS carriers only | Provider/route issues; check delivery receipts |

---

## Quick summary

- Email/SMS OTP = server-generated, server-stored random code; the weakest common MFA but widely usable.
- 6 digits only works with a 5-minute TTL, ≤5 attempts, single-use, and sending limits.
- Store an HMAC (not a plain hash), bound to user and purpose.
- Guard against SMS pumping and SIM swap; prefer TOTP or passkeys when you can.

**Next:** [04-webauthn-passkeys.md](./04-webauthn-passkeys.md)
