# Password Policy

What rules to put on passwords. Modern guidance (NIST SP 800-63B) is different from the old "8 characters, one uppercase, one symbol, change every 90 days" habit, because that approach produces predictable passwords like `Password1!` and then `Password2!`.

**Prerequisite:** [00-hashing-argon2id.md](./00-hashing-argon2id.md)

---

## What to enforce

| Rule | Recommendation |
|---|---|
| Minimum length | At least 8 characters is the floor; prefer 12+ (NIST's current draft revision moves the floor to 15 for password-only accounts, so check the latest) |
| Maximum length | Allow long passphrases (64+), but cap (e.g. 128) to bound hashing cost |
| Characters | Accept everything: spaces, Unicode, emoji |
| Composition rules | Don't require mixed case/digits/symbols |
| Breached passwords | Reject known-leaked passwords |
| Common passwords | Reject `password`, `qwerty123`, the user's email, etc. |
| Periodic expiry | Don't force rotation; require change only on evidence of compromise |
| Paste | Allow pasting so password managers work |
| Strength meter | Optional; use a real estimator, not character-class counting |

Length beats complexity: `correct horse battery staple` is stronger and easier to remember than `P@ssw0rd!`.

---

## Validating with zod

```js
import { z } from "zod";

export const passwordSchema = z
  .string()
  .min(12, "Use at least 12 characters")
  .max(128, "Password is too long");

export const registerSchema = z.object({
  email: z.string().email().max(254).toLowerCase(),
  password: passwordSchema,
});
```

Do **not** `trim()` or otherwise normalize the password. Leading/trailing spaces are part of it. (Some systems apply Unicode NFKC normalization consistently at set and verify time; if you do, do it in both places.)

The maximum matters because Argon2 is expensive on purpose; without a cap, an attacker can post megabyte-sized "passwords" to burn CPU. Also set a request body size limit (`express.json({ limit: "10kb" })`).

---

## Checking breached passwords

The Have I Been Pwned **Pwned Passwords** API uses k-anonymity: you send only the first 5 hex characters of the SHA-1 hash, receive all matching suffixes, and compare locally. The password never leaves your server.

```js
import crypto from "crypto";

export async function isBreached(password) {
  const sha1 = crypto.createHash("sha1").update(password).digest("hex").toUpperCase();
  const prefix = sha1.slice(0, 5);
  const suffix = sha1.slice(5);

  const res = await fetch(`https://api.pwnedpasswords.com/range/${prefix}`);
  if (!res.ok) return false; // fail open on lookup errors: don't block signup

  const body = await res.text();
  return body.split("\n").some((line) => line.split(":")[0].trim() === suffix);
}
```

SHA-1 here is only the lookup protocol of that API, not a storage choice. Passwords themselves are still Argon2id-hashed.

Decide your failure mode: failing open (as above) keeps signup working if the service is down; failing closed is stricter. Alternatively download the dataset and check offline.

---

## Context-specific checks

Reject passwords that contain obvious personal or service context:

```js
function containsContext(password, { email, name }) {
  const p = password.toLowerCase();
  const local = email.split("@")[0].toLowerCase();
  return p.includes(local) || (name && p.includes(name.toLowerCase())) || p.includes("yourapp");
}
```

Return helpful, non-leaking messages: "That password is too common" rather than only "invalid".

---

## Where policy is applied

```text
Register  ─┐
Reset     ─┼─► same passwordSchema + breach check
Change    ─┘
Login     ──► NO policy check (never reject a login for policy reasons)
```

Apply the policy at every place a password is *set*, using one shared function. Do not apply it at login: users with older passwords must still be able to sign in.

---

## Beyond policy

A password policy is only one layer. Pair it with MFA ([mfa-overview](../05-mfa/00-mfa-overview.md)), rate limiting ([rate-limiting](../08-hardening/00-rate-limiting.md)), and passkeys ([webauthn-passkeys](../05-mfa/04-webauthn-passkeys.md)), which remove the password problem entirely.

---

## Common mistakes

- Enforcing composition rules and mandatory rotation, which push users to weak patterns.
- A low maximum like 16 characters, which blocks passphrases.
- Trimming/modifying the password before hashing.
- Applying policy checks during login.
- Sending passwords to a third-party service in full (use k-anonymity).
- Disabling paste on password fields.

---

## Quick summary

- Prefer length and breach checks over composition rules and expiry.
- Allow long passphrases and all characters; cap at ~128.
- Never trim or alter the password.
- Check against breached lists using k-anonymity.
- Apply policy when setting a password (register/reset/change), not at login.

**Next:** [02-register-and-login.md](./02-register-and-login.md)
