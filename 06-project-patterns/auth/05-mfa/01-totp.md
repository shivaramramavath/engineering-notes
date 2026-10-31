# TOTP (Authenticator Apps)

Time-based One-Time Passwords (RFC 6238) are the six-digit codes in Google Authenticator, Authy, 1Password, and similar apps. Server and app share a secret; both compute a code from that secret and the current time, so no network call or SMS is involved.

**Prerequisite:** [00-mfa-overview.md](./00-mfa-overview.md)

---

## How it works

```text
shared secret (random, base32)         current time
        │                                   │
        └────────────┬──────────────────────┘
                     ▼
        step = floor(unixTime / 30)
                     ▼
        HMAC-SHA1(secret, step)  → truncate → 6 digits
                     ▼
                 "492 039"
```

- The **secret** is created by the server once at enrollment and given to the app via QR code. After that, nothing secret crosses the wire again.
- Defaults that every mainstream authenticator supports: **SHA-1, 6 digits, 30-second period**. SHA-1 here is HMAC-SHA1, which remains sound; but other settings (SHA-256, 8 digits) are not reliably supported by all apps, so stick to defaults.
- The server must keep the secret readable (it needs to compute codes), so it is **encrypted**, not hashed.

---

## Setup

```bash
npm i otplib qrcode
```

`otplib`'s `authenticator` API shown here is from v12. Newer major versions reworked the API, so check the docs of the version you install and adapt the calls. The concepts and the surrounding code don't change.

```js
import { authenticator } from "otplib";

authenticator.options = { digits: 6, step: 30, window: 1 };
```

`window: 1` accepts the previous, current and next 30-second step, tolerating small clock differences between the phone and server.

---

## Encrypting the secret at rest

If the database leaks, plaintext TOTP secrets let attackers generate every user's codes forever. Encrypt with AES-256-GCM using a key held outside the database:

```js
// utils/encryption.js
import crypto from "crypto";

const KEY = Buffer.from(process.env.MFA_ENC_KEY, "base64");   // 32 random bytes

export function encrypt(plain) {
  const iv = crypto.randomBytes(12);
  const cipher = crypto.createCipheriv("aes-256-gcm", KEY, iv);
  const ct = Buffer.concat([cipher.update(plain, "utf8"), cipher.final()]);
  return [iv, cipher.getAuthTag(), ct].map((b) => b.toString("base64url")).join(".");
}

export function decrypt(blob) {
  const [iv, tag, ct] = blob.split(".").map((s) => Buffer.from(s, "base64url"));
  const decipher = crypto.createDecipheriv("aes-256-gcm", KEY, iv);
  decipher.setAuthTag(tag);
  return Buffer.concat([decipher.update(ct), decipher.final()]).toString("utf8");
}
```

Generate the key once (`openssl rand -base64 32`) and keep it in your secret manager ([01-env-and-secrets.md](../00-setup/01-env-and-secrets.md)). A fresh random IV per encryption is mandatory with GCM. Key rotation means re-encrypting every secret (store a key id with the blob if you plan to rotate).

---

## Enrollment

### 1. Start: generate a pending secret and QR

```js
import QRCode from "qrcode";

export async function startTotp(req, res) {
  const user = await User.findById(req.user.sub);
  const secret = authenticator.generateSecret();            // base32

  // Pending: not on the user yet, expires on its own
  await redis.set(`mfa:totp:pending:${user.id}`, encrypt(secret), { EX: 10 * 60 });

  const otpauth = authenticator.keyuri(user.email, "YourApp", secret);
  const qrDataUrl = await QRCode.toDataURL(otpauth);

  res.json({ qrDataUrl, manualKey: secret });               // manual entry as a fallback
}
```

The `otpauth://` URI encodes issuer, account and secret:

```text
otpauth://totp/YourApp:alice%40example.com?secret=JBSWY3DPEHPK3PXP&issuer=YourApp
```

Always offer the **manual key** too, since users on the phone itself can't scan their own screen. This response contains the secret: send `Cache-Control: no-store`, never log it, and only return it to a recently re-authenticated session.

### 2. Confirm: the user proves the app works

```js
export async function confirmTotp(req, res) {
  const { code } = req.body;
  const pending = await redis.get(`mfa:totp:pending:${req.user.sub}`);
  if (!pending) return res.status(400).json({ message: "Setup expired, start again" });

  const secret = decrypt(pending);
  if (!authenticator.check(code, secret)) {
    return res.status(400).json({ message: "Invalid code" });
  }

  const recoveryCodes = await generateRecoveryCodes();      // see 02-recovery-codes.md
  await User.updateOne(
    { _id: req.user.sub },
    {
      "mfa.enabled": true,
      "mfa.totpSecretEnc": pending,
      "mfa.lastTotpStep": Math.floor(Date.now() / 30000),
      $addToSet: { "mfa.methods": "totp" },
      "mfa.recoveryCodes": recoveryCodes.hashes,
    }
  );
  await redis.del(`mfa:totp:pending:${req.user.sub}`);
  await revokeOtherSessions(req.user.sub, req.user.sid);

  res.json({ recoveryCodes: recoveryCodes.plain });         // shown once
}
```

---

## Verification at login

```js
export async function verifyTotpForLogin(user, code) {
  if (!/^\d{6}$/.test(code)) return false;                  // cheap format check first

  const secret = decrypt(user.mfa.totpSecretEnc);
  const delta = authenticator.checkDelta(code, secret);     // null if invalid, else steps off (-1, 0, +1)
  if (delta === null) return false;

  const step = Math.floor(Date.now() / 30000) + delta;

  // Replay protection: accept each time step at most once.
  // The filter makes the check-and-update atomic.
  const res = await User.updateOne(
    { _id: user._id, "mfa.lastTotpStep": { $lt: step } },
    { "mfa.lastTotpStep": step }
  );
  return res.modifiedCount === 1;
}
```

Why replay protection: a valid code stays valid for the whole window (up to ~90 seconds with `window: 1`). An attacker who shoulder-surfs or phishes a code could reuse it within that time. RFC 6238 says a code should be accepted once; tracking the last accepted step does that. Compare per user, not globally.

`checkDelta` availability and return shape depend on the library version; if yours lacks it, compute the step yourself by checking each of the three candidate steps.

---

## Clock drift

TOTP depends on both sides agreeing on the time.

| Problem | Fix |
|---|---|
| Server clock wrong | Run NTP (chrony/systemd-timesyncd); check `date -u` on every instance |
| Phone clock wrong | Users must enable automatic time; Google Authenticator has a "Time correction" option in settings |
| Occasional mismatch | `window: 1` (±30 s). Larger windows weaken security and increase replay exposure |

Don't widen the window to hide a broken server clock: fix the clock.

---

## Disabling TOTP

Requires step-up (password + current factor), then:

```js
await User.updateOne(
  { _id },
  { "mfa.enabled": false, $unset: { "mfa.totpSecretEnc": 1 }, "mfa.recoveryCodes": [], $pull: { "mfa.methods": "totp" } }
);
await revokeOtherSessions(userId, currentSid);
// + notification email
```

If another method (passkey) remains, keep `mfa.enabled` true.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Plaintext secrets in the DB | A DB leak compromises every second factor |
| Enabling before the user confirms a code | Lockouts |
| No replay protection | A captured code works repeatedly in its window |
| No attempt limit | A million possible codes can be brute-forced |
| Non-default parameters (8 digits, SHA-512) | Many apps silently compute the wrong codes |
| Returning the secret in a cacheable response or logging it | Secret leak |
| Large `window` | More valid codes at any moment |

## Debugging

| Symptom | Cause |
|---|---|
| Valid-looking code always rejected | Server or phone clock skew; secret decrypted with the wrong key; stored a different secret than the QR showed |
| Works on one phone, not another | Different time settings on that phone |
| QR scans but wrong account name | `keyuri` built with a wrong label/issuer |
| `decrypt` throws "Unsupported state or unable to authenticate data" | Wrong `MFA_ENC_KEY` or a corrupted/trimmed blob |
| Code accepted twice | Replay check missing or not atomic |

Test with a known secret by computing the expected code in a script (`authenticator.generate(secret)`) and comparing with your app.

---

## Quick summary

- TOTP = HMAC of a shared secret and the current 30-second step; use the defaults (SHA-1, 6 digits, 30 s).
- Encrypt the secret (AES-GCM), confirm a code before enabling, always offer a manual key.
- Verify with a ±1-step window, reject replays via `lastTotpStep`, limit attempts.
- Keep clocks accurate; fix drift rather than widening the window.
- TOTP is phishable; passkeys are the stronger upgrade.

**Next:** [02-recovery-codes.md](./02-recovery-codes.md)
