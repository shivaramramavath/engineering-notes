# MFA Overview

Multi-factor authentication requires proof from **two or more different categories** of evidence, so stealing one thing (usually the password) is not enough. This note covers the factors, how strong each is, how MFA changes the login flow, and the data model the rest of the chapter builds on.

**Prerequisites:** [02-register-and-login.md](../02-passwords/02-register-and-login.md), [01-redis-sessions.md](../04-sessions/01-redis-sessions.md)

---

## Factors

| Category | Meaning | Examples |
|---|---|---|
| Knowledge | Something you know | Password, PIN |
| Possession | Something you have | Phone with an authenticator app, security key, passkey on a device |
| Inherence | Something you are | Fingerprint, face (usually unlocks a device-held key rather than being sent to the server) |

Two passwords are **not** MFA; both are knowledge. A password plus an emailed code is technically two channels but a weak pairing, because anyone who controls the inbox also controls password reset.

---

## Methods ranked by strength

```text
Strongest ─────────────────────────────────────────────► Weakest
Passkey / security key   >   TOTP app   >   Email OTP   >   SMS OTP
(WebAuthn)                   (RFC 6238)
phishing-resistant           phishable      inbox-bound     SIM-swap, SS7
```

| Method | Phishing-resistant? | Main weakness | Note |
|---|---|---|---|
| WebAuthn / passkeys | **Yes** (bound to the site's origin) | Device loss, recovery UX | [04-webauthn-passkeys.md](./04-webauthn-passkeys.md) |
| TOTP app | No: a fake site can relay the code in real time | Secret theft, phishing, replay | [01-totp.md](./01-totp.md) |
| Email OTP | No | Only as strong as the mailbox | [03-otp-email-sms.md](./03-otp-email-sms.md) |
| SMS OTP | No | SIM swap, interception, toll fraud costs | Last resort |
| Recovery codes | n/a | Must be stored safely | [02-recovery-codes.md](./02-recovery-codes.md) |

**Phishing-resistance** matters most. A user who types a TOTP code into a convincing fake page hands the attacker a code valid for ~30 seconds, which the attacker forwards to the real site immediately. WebAuthn can't be relayed that way because the browser signs a challenge together with the real origin.

Even weak MFA stops the dominant attack, credential stuffing with leaked passwords, so offering SMS beats offering nothing; but design towards phishing-resistant methods.

---

## Login flow with MFA

The key rule: **no session and no tokens until the second factor succeeds.**

```text
POST /auth/login (email, password)
        │
        ▼
Verify password ──fail──► 401 generic
        │ ok
        ▼
mfa.enabled? ── no ──► create session ──► tokens
        │ yes
        ▼
Create MFA challenge (opaque, Redis, TTL 5 min)
        │
        ▼
200 { mfaRequired: true, challengeToken, methods: ["totp","webauthn","recovery"] }

POST /auth/mfa/verify { challengeToken, method, code }
        │
        ▼
Look up challenge → check attempts → verify factor
        │ ok
        ▼
Delete challenge → create session (amr: pwd + otp) → tokens
```

### The challenge is an opaque token, not a JWT

```js
// services/mfa-challenge.service.js
const KEY = (h) => `mfa:challenge:${h}`;
const TTL = 5 * 60;
const MAX_ATTEMPTS = 5;

export async function createChallenge(userId) {
  const token = randomToken(32);
  await redis
    .multi()
    .hSet(KEY(hashToken(token)), { userId, attempts: 0 })
    .expire(KEY(hashToken(token)), TTL)
    .exec();
  return token;
}

// Returns userId if the caller may attempt; throws if exhausted/unknown.
export async function beginAttempt(token) {
  const key = KEY(hashToken(token));
  const userId = await redis.hGet(key, "userId");
  if (!userId) throw new AuthError("Challenge expired");      // check first: HINCRBY on a missing key would CREATE it with no TTL
  const attempts = await redis.hIncrBy(key, "attempts", 1);   // atomic counter
  if (attempts > MAX_ATTEMPTS) {
    await redis.del(key);
    throw new AuthError("Too many attempts");
  }
  return userId;
}

export const finishChallenge = (token) => redis.del(KEY(hashToken(token)));
```

Why opaque rather than a "pre-auth JWT": a JWT carrying `mfa: false` would be accepted by any middleware that only checks signature, so one forgotten check turns the half-authenticated state into a full login. An opaque token works only at the single verify endpoint.

Each wrong code consumes an attempt; after 5, the challenge dies and the user must enter their password again. Combine with IP/account rate limits ([00-rate-limiting.md](../08-hardening/00-rate-limiting.md)): a 6-digit code has only a million possibilities.

---

## Enrollment flow

```text
Authenticated (and recently re-authenticated: step-up)
        │
        ▼
Start: server generates factor material (TOTP secret / WebAuthn options)
        │  stored as PENDING, with TTL
        ▼
User proves they can use it (enter a code / touch a key)
        │
        ▼
Activate: persist factor, set mfa.enabled
        │
        ▼
Show recovery codes ONCE → user confirms they saved them
        │
        ▼
Revoke other sessions (security posture changed)
```

Never activate a factor on the first request. If the user scans the QR and then it fails (typo, wrong time), they'd be locked out of an account that now demands a factor they can't produce. **Confirm before enabling.**

---

## Data model

```js
// models/user.model.js (additions)
mfa: {
  enabled:        { type: Boolean, default: false },
  totpSecretEnc:  { type: String },            // AES-GCM ciphertext, never plaintext
  lastTotpStep:   { type: Number, default: 0 },// replay protection
  recoveryCodes:  [{ type: String }],          // HMACs of the codes
  methods:        [{ type: String, enum: ["totp", "webauthn"] }],
},
```

WebAuthn credentials get their own collection (one user, many keys); see [04-webauthn-passkeys.md](./04-webauthn-passkeys.md).

Mark `totpSecretEnc` and `recoveryCodes` as `select: false` so they don't appear in ordinary queries or API responses by accident.

---

## What the session records

Record **how** the user authenticated so later code can reason about it:

```js
await redis.hSet(`session:${sessionId}`, {
  amr: "pwd,otp",            // authentication methods used (RFC 8176 style values)
  lastAuthAt: Date.now(),
});
```

Include `amr` in the access JWT if downstream services need it. [05-step-up-auth.md](./05-step-up-auth.md) uses `lastAuthAt` to demand fresh authentication for sensitive actions.

---

## Policy decisions

| Question | Typical answer |
|---|---|
| Optional or mandatory? | Optional for users; **mandatory for admins** and privileged roles |
| MFA on every login? | Trusted devices can skip for N days, but the "trust" token must be random, hashed, revocable, and device-bound |
| Offer multiple methods? | Yes: at least one backup (a second method or recovery codes) |
| Disabling MFA | Requires step-up, sends a notification email, revokes other sessions |
| Password reset | Does not bypass MFA: no auto-login after reset ([03-password-change-and-reset.md](../02-passwords/03-password-change-and-reset.md)) |
| Lost every factor | Recovery codes, else a deliberate, slow, audited support process |

Account recovery is the weakest link of any MFA system: if "email me a link to disable MFA" exists, MFA is only as strong as email.

---

## Common mistakes

- Issuing tokens before the second factor completes.
- Enabling MFA before the user has verified a working code.
- No attempt limit on code verification.
- Storing TOTP secrets in plaintext.
- Allowing password reset or email change to silently remove MFA.
- No recovery path (support nightmare) or an insecure one (attack path).
- Revealing which factor failed ("password OK, code wrong" is acceptable only after the password step succeeded).

---

## Quick summary

- MFA = different factor categories; phishing-resistant methods (passkeys) beat codes.
- No session or tokens until the second factor passes; use an opaque, attempt-limited challenge.
- Confirm a factor before enabling it; show recovery codes once.
- Record `amr` and `lastAuthAt` on the session.
- Recovery and reset flows must not bypass MFA.

**Next:** [01-totp.md](./01-totp.md)
