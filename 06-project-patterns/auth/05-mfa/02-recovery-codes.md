# Recovery Codes

Recovery codes are one-time backup credentials that let a user sign in when their second factor is lost: phone broken, authenticator app wiped, key left at home. They are generated at MFA enrollment, shown once, and each works exactly one time.

**Prerequisite:** [00-mfa-overview.md](./00-mfa-overview.md)

---

## Why they exist

Without a recovery path, "lost phone" means a permanently locked account or a support process that attackers will impersonate. Recovery codes give users a self-service path that doesn't rely on the lost device.

```text
Normal:   password + TOTP/passkey
Recovery: password + recovery code   (consumes the code)
```

A recovery code is **as powerful as the MFA factor itself**, so it is handled with the same care as a password: random, stored hashed, rate-limited, single-use.

---

## Design

| Property | Choice | Reason |
|---|---|---|
| Count | 8-10 codes | Enough for several incidents, few enough to store |
| Entropy | **≥ 64-80 bits each** | See below: they're stored with a fast hash |
| Format | Groups for readability, unambiguous alphabet | Users copy these by hand |
| Storage | HMAC of each code, never plaintext | A DB leak shouldn't reveal usable codes |
| Use | Single-use, removed atomically | Replay protection |
| Display | Once, at generation | Nobody should be able to re-read them later |
| Regeneration | Replaces **all** old codes | Old printouts die |

### Why more entropy than an OTP, and why HMAC

A 6-digit OTP is protected by a 5-minute lifetime and attempt limits. A recovery code lives for months and sits in the database. If it were short (say 40 bits) and stored with plain SHA-256, an attacker with a database copy could brute-force it **offline** at billions of guesses per second.

Two levers fix this: use at least 64-80 bits of randomness, and store `HMAC-SHA256(serverSecret, code)` so an offline attacker lacking the server secret can't even test guesses. (You could use Argon2 instead, but with high-entropy codes it adds latency for no real gain.)

---

## Generate

```js
import crypto from "crypto";

const ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   // no I, O, 0, 1

const rawCode = () => {
  const bytes = crypto.randomBytes(16);
  let s = "";
  for (let i = 0; i < 16; i++) s += ALPHABET[bytes[i] % ALPHABET.length];  // 32 chars: no modulo bias
  return s.match(/.{4}/g).join("-");               // XXXX-XXXX-XXXX-XXXX → 80 bits
};

const hmac = (code) =>
  crypto
    .createHmac("sha256", process.env.RECOVERY_CODE_KEY)
    .update(normalize(code))
    .digest("hex");

const normalize = (code) => code.toUpperCase().replace(/[^A-Z0-9]/g, "");

export async function generateRecoveryCodes(count = 10) {
  const plain = Array.from({ length: count }, rawCode);
  return { plain, hashes: plain.map(hmac) };
}
```

`bytes[i] % 32` is unbiased because 256 is divisible by 32; with an alphabet whose size doesn't divide 256, use `crypto.randomInt`. Normalizing on verify (uppercase, strip dashes and spaces) prevents "I typed it in lowercase" failures.

---

## Use at login

```js
export async function verifyRecoveryCode(userId, input) {
  const hash = hmac(input);

  // Atomic: remove the code only if it is still present. modifiedCount tells us who won.
  const res = await User.updateOne(
    { _id: userId, "mfa.recoveryCodes": hash },
    { $pull: { "mfa.recoveryCodes": hash } }
  );
  return res.modifiedCount === 1;
}
```

Why one `updateOne` instead of read-then-write: two simultaneous requests with the same code must not both succeed. The filter plus `$pull` makes consumption atomic in MongoDB.

Because a lookup by HMAC in the document replaces a comparison loop, there is no timing-based leak of which codes exist.

After a successful recovery login:

1. Count remaining codes. If few (say < 3), tell the user to regenerate.
2. **Send a notification email**: "A recovery code was used to sign in."
3. Strongly nudge the user to re-enroll a new authenticator, since they've lost their usual factor.
4. Log `MFA_RECOVERY_USED` as a security event.

Recovery attempts go through the same challenge and attempt limits as any other factor ([00-mfa-overview.md](./00-mfa-overview.md)).

---

## Regenerate

```js
export async function regenerateRecoveryCodes(req, res) {
  // requires step-up: see 05-step-up-auth.md
  const { plain, hashes } = await generateRecoveryCodes();
  await User.updateOne({ _id: req.user.sub }, { "mfa.recoveryCodes": hashes });   // replaces ALL old codes
  res.set("Cache-Control", "no-store").json({ recoveryCodes: plain });
}
```

Regenerating should also notify the user and may revoke other sessions, because it is a security-posture change.

---

## UX that prevents lockouts

- Show the codes with **Copy / Download / Print** buttons and require a confirmation ("I saved these") before leaving the screen.
- Explain where to keep them: a password manager or a printed copy, **not** in the same device as the authenticator.
- Show remaining count in account settings (count only; never the codes).
- Let users with a passkey or a second TOTP device treat recovery codes as optional but recommended.

---

## When recovery codes aren't enough

If a user has lost both the device **and** the codes, there is no self-service path left. A support process must verify identity through multiple independent signals (account history, payment details, waiting period with email notices to all contact points). Make it deliberately slow and high-friction, document it, and log it. This process is the real security level of your MFA: attackers will go straight to it.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Storing codes in plaintext | DB leak = MFA bypass |
| Short codes + fast unsalted hash | Offline brute force |
| Not single-use or not atomic | Replay; two requests both succeed |
| Letting users view codes again later | Defeats "shown once"; XSS or session theft reveals them |
| Regenerating without invalidating the old set | Old printouts still work |
| No rate limit on the recovery endpoint | Online guessing |
| No notification when a code is used | The real user never learns of a takeover |
| Treating recovery as a way to **disable** MFA without any checks | Bypass path |

---

## Quick summary

- Recovery codes are single-use backup factors, generated at enrollment and shown once.
- Use 64-80+ bits of randomness and store HMACs; consume atomically with one filtered update.
- Regeneration replaces the whole set; use and regeneration trigger notifications.
- Support-assisted recovery is the real weak point: make it slow and audited.

**Next:** [03-otp-email-sms.md](./03-otp-email-sms.md)
