# Reset Tokens

A password-reset token is a temporary credential that lets someone with access to an email inbox set a new password. It is one of the most attacked credentials in any system: if it can be guessed, stolen or reused, an attacker owns the account without ever knowing the password.

This note covers the **token design**. The endpoint code (forgot-password / reset-password controllers) is in [03-password-change-and-reset.md](../02-passwords/03-password-change-and-reset.md).

**Prerequisites:** [08-email-verification-tokens.md](./08-email-verification-tokens.md), [01-token-hashing.md](./01-token-hashing.md)

---

## Requirements

| Property | Value | Defends against |
|---|---|---|
| Unpredictable | 256-bit CSPRNG | Guessing |
| Stored hashed | SHA-256 in Redis | Redis/backup leak |
| Short-lived | ~15 min TTL | Stale emails, later inbox compromise |
| Single-use | Atomic `GETDEL` | Replay, double submit |
| Purpose-bound | Own key prefix, never usable for verification/login | Cross-flow abuse |
| User-bound | Payload `{ userId }` | Applying a token to a different account |
| Rate-limited | Per IP and email | Token guessing, email bombing |

---

## Using the shared helper

```js
const RESET_PREFIX = "password-reset:token:";
const RESET_TTL = 15 * 60;

// issue
const token = await createOneTimeToken(RESET_PREFIX, { userId }, RESET_TTL);

// consume (inside the reset endpoint)
const data = await consumeOneTimeToken(RESET_PREFIX, token);
if (!data) throw new AuthError("Invalid or expired reset link");
```

`createOneTimeToken` / `consumeOneTimeToken` are defined in [08-email-verification-tokens.md](./08-email-verification-tokens.md). Only the prefix, TTL and payload differ between flows.

---

## Why not a JWT for reset?

It's tempting: a signed `{ userId, exp }` needs no storage. The problem is **single-use**. A JWT can't be "used up" without server state, so the link would work repeatedly until expiry, including for anyone who later sees the email or browser history. A random token with a Redis entry is revocable and consumable. (Signed reset links can bind to the current password hash to self-invalidate, but that adds subtle coupling; an opaque token is simpler and safer.)

---

## Multiple outstanding tokens

User clicks "forgot password" three times. Policy options:

| Policy | Behavior |
|---|---|
| Allow all until they expire | Simple; more valid tokens in the wild |
| **Latest wins** | Issuing a token deletes the previous ones (track the latest hash per user) |

"Latest wins" is tidier: once the user has the newest email, older links die. Combine with a per-user cooldown (e.g. one email per minute) so the endpoint can't be used to spam an inbox.

---

## Lifecycle events

```text
Issue    → hash stored, 15m TTL, raw sent by email
Consume  → GETDEL (atomic) → token gone forever
Success  → password changed, ALL sessions revoked,
           other outstanding reset tokens for the user deleted,
           "your password was changed" email sent
Expire   → TTL removes it
```

After a **normal password change** (not a reset), also delete pending reset tokens: a user who just changed their password shouldn't leave a working reset link behind.

---

## Delivering the token safely

The link opens a frontend page: `https://app.example.com/reset-password?token=RAW`.

- **Build the link from configuration** (`FRONTEND_URL`), never from the request's `Host` header. Otherwise an attacker triggers a reset email for the victim with a poisoned `Host`, and the victim's link points at the attacker's domain.
- **Strip the token from the URL** as soon as the page loads (`history.replaceState`), and keep it in memory for the form submit.
- **`Referrer-Policy: no-referrer`** on that page so the token isn't sent to any third-party resource.
- **No third-party scripts/analytics** on the reset page.
- **Don't log URLs with tokens**: reverse proxy access logs often record query strings. Scrub them.
- Alternative: place the token in the URL **fragment** (`#token=...`). Fragments are never sent to servers or in `Referer` headers, which removes the log/referrer leaks (the frontend reads it with JS). Query-string is more common; fragment is more private.

---

## Attack surface and defenses

| Attack | Defense |
|---|---|
| Guess tokens | 256 bits; rate limit confirm endpoint |
| Enumerate accounts via forgot-password | Identical response and timing |
| Steal from logs/history/referrer | Strip from URL, scrub logs, no-referrer |
| Replay after use | `GETDEL` |
| Race two submissions | `GETDEL` returns data to only one |
| Host-header poisoning | Link from config |
| Attacker with a stolen session keeps access | Revoke all sessions on reset |
| Reset bypasses MFA | Don't auto-login after reset; require normal login with MFA ([mfa-overview](../05-mfa/00-mfa-overview.md)) |
| Account takeover via compromised inbox | MFA, notification emails, optionally require MFA to complete reset |

---

## Common mistakes and debugging

| Symptom / mistake | Cause / fix |
|---|---|
| "Invalid link" immediately | Token trimmed/URL-encoded differently from what was hashed; hash the exact emailed string |
| Link works twice | `get` + `del` instead of `getDel` |
| Reset link goes to attacker's domain | Host header used to build link |
| Many valid links outstanding | No latest-wins policy; add per-user tracking |
| Token appears in server logs | Query string logged by proxy/app; scrub or use fragment |
| Users still logged in on old devices after reset | Sessions not revoked |
| Reset emails flood a victim | No per-email rate limit/cooldown |

---

## Quick summary

- Reset token = random, hashed, short TTL, single-use (`GETDEL`), purpose-bound, rate-limited.
- Prefer an opaque token over a JWT, which can't be made single-use without state.
- Build links from config, keep tokens out of logs and referrers.
- On success: revoke all sessions, delete other reset tokens, notify the user, don't auto-login.

**Next:** [Sessions: session basics](../04-sessions/00-session-basics.md)
