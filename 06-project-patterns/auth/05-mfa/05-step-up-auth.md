# Step-Up Authentication

Being logged in is a weak statement: it may have been three weeks ago, on a laptop someone else is now using. **Step-up authentication** asks for a fresh proof, such as a password, TOTP or passkey, right before a sensitive action, even though the user already has a valid session.

**Prerequisites:** [00-mfa-overview.md](./00-mfa-overview.md), [04-logout-and-revocation.md](../04-sessions/04-logout-and-revocation.md)

---

## Why

```text
Stolen laptop / hijacked session / XSS-driven request
              │
              ▼
Valid access token for ~10 min, valid refresh session for 30 days
              │
              ▼
Without step-up: change email → reset password → permanent takeover
With step-up:    attacker is asked for the password/second factor they don't have
```

It limits the damage of any session compromise to non-sensitive actions.

---

## Which actions need it

| Action | Why it matters |
|---|---|
| Change password | Direct takeover |
| Change email / phone | Alters the recovery channel |
| Enable, disable or change MFA; regenerate recovery codes | Alters the security posture |
| View/export sensitive data, view API keys | Data exposure |
| Create/revoke API keys, add a personal access token | Persistent access |
| Delete account | Irreversible |
| Payments, payouts, adding a bank account | Money |
| Admin or privileged operations | Blast radius |

Routine reads and normal actions don't, otherwise users stop caring.

---

## The model: "recently authenticated"

Track **when and how** the user last proved themselves, on the **session**:

```text
session:{sid}
   lastAuthAt   1754650000000     ← login time, or last step-up
   amr          "pwd,otp"         ← methods used
```

A sensitive route demands `now - lastAuthAt <= maxAge`:

```js
// middleware/require-recent-auth.js
export const requireRecentAuth = (maxAgeSeconds = 300, { mfa = false } = {}) =>
  async (req, res, next) => {
    const session = await getSession(req.user.sid);                 // from 01-redis-sessions.md
    if (!session) return res.status(401).json({ message: "Session ended" });

    const age = (Date.now() - Number(session.lastAuthAt || 0)) / 1000;
    const hasMfa = (session.amr || "").split(",").some((m) => ["otp", "hwk", "swk", "mfa"].includes(m));

    if (age > maxAgeSeconds || (mfa && !hasMfa)) {
      return res.status(403).json({ code: "STEP_UP_REQUIRED", maxAge: maxAgeSeconds, mfa });
    }
    next();
  };

router.post("/account/email",   authenticate, requireRecentAuth(300),            changeEmail);
router.post("/account/delete",   authenticate, requireRecentAuth(120, { mfa: true }), deleteAccount);
```

Reading the freshness from **Redis**, not from the JWT, matters: the access token is minted at refresh time and doesn't know about a step-up that just happened (or that the session has been revoked). The session is the source of truth.

`amr` values follow RFC 8176 naming (for example `pwd`, `otp`, `hwk` for hardware key, `swk` for software key, `mfa`); use your own consistent set if you prefer.

---

## The step-up endpoint

```text
POST /api/auth/step-up
{ method: "password" | "totp" | "webauthn", ...proof }
```

```js
export async function stepUp(req, res) {
  const user = await User.findById(req.user.sub).select("+passwordHash +mfa.totpSecretEnc +mfa.lastTotpStep");
  const { method } = req.body;

  let ok = false;
  let amr;

  if (method === "password") {
    ok = await argon2.verify(user.passwordHash, req.body.password);
    amr = "pwd";
  } else if (method === "totp" && user.mfa.enabled) {
    ok = await verifyTotpForLogin(user, req.body.code);            // from 01-totp.md
    amr = "pwd,otp";                                               // password was proven at login; this adds a fresh second factor
  }
  // webauthn: same flow as 04-webauthn-passkeys.md, ending with ok = true, amr = "hwk"

  if (!ok) return res.status(401).json({ message: "Verification failed" });

  await redis.hSet(`session:${req.user.sid}`, { lastAuthAt: Date.now(), amr });
  res.status(204).end();
}
```

Rules for this endpoint:

- **Rate-limit it** like login. It is a password/code oracle for anyone holding a session ([00-rate-limiting.md](../08-hardening/00-rate-limiting.md)).
- Require the factor level the action needs: a user with MFA enabled should step up with the second factor, not just the password, for the most sensitive actions.
- Bind the result to **this session only**. Other devices are unaffected.
- Don't issue a new long-lived credential; just update `lastAuthAt`.

---

## Client flow

```js
// axios interceptor
api.interceptors.response.use(undefined, async (error) => {
  const data = error.response?.data;
  if (error.response?.status === 403 && data?.code === "STEP_UP_REQUIRED") {
    await openReauthModal({ mfa: data.mfa });       // prompts password / TOTP / passkey, calls /auth/step-up
    return api(error.config);                       // retry the original request once
  }
  return Promise.reject(error);
});
```

Status codes: use a distinct `code` (`STEP_UP_REQUIRED`) so the client can tell it apart from a normal `403 Forbidden` (the user lacks permission) and from `401` (needs refresh, [06-token-storage.md](../03-tokens/06-token-storage.md)).

```text
401 → try refresh, retry
403 + STEP_UP_REQUIRED → re-authenticate, retry
403 otherwise → show "not allowed"
```

---

## Choosing the freshness window

| Sensitivity | Window |
|---|---|
| Moderate (change profile email) | 10-15 minutes |
| High (password, MFA changes) | 5 minutes |
| Critical (money movement, account deletion) | Every time, or 1-2 minutes, plus a specific confirmation |

For one-shot actions, use a **single-use step-up grant**: after verification, store `stepup:{sid}:{action}` in Redis with a 2-minute TTL and `GETDEL` it when the action executes. This prevents "I verified once, then ran five sensitive actions".

```js
await redis.set(`stepup:${sid}:delete-account`, "1", { EX: 120 });
// in the action:
if (!(await redis.getDel(`stepup:${req.user.sid}:delete-account`))) return res.status(403).json({ code: "STEP_UP_REQUIRED" });
```

---

## Related concepts

- **Transaction signing:** for payments, bind the confirmation to *what* is being approved (amount, recipient), so an attacker can't swap details after the user approves.
- **OIDC equivalents:** the OIDC `max_age` parameter and `auth_time`/`acr`/`amr` claims express the same idea when an external identity provider is involved ([02-oidc.md](../06-oauth/02-oidc.md)).
- **Risk-based/adaptive MFA:** demand step-up when context changes (new country, new device, impossible travel) in addition to action sensitivity.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Enforcing step-up only in the frontend | The API is still callable directly |
| Keeping freshness only in the JWT | Stale: doesn't reflect later step-ups or revocation |
| One global "verified" flag that never expires | Defeats the point |
| Step-up endpoint without rate limits | Password/code guessing from a hijacked session |
| Accepting the password alone when MFA is enabled, for the highest-risk actions | Weakens the factor model |
| Making step-up apply across sessions | A verification on one device unlocks others |
| Reusing the same `403` as permission errors | The UI can't tell what to do |
| Not re-checking after the action starts (long flows) | Window expires mid-operation; verify at the final commit step |

## Debugging

| Symptom | Cause |
|---|---|
| Endless step-up prompts | `lastAuthAt` not updated, or comparing seconds with milliseconds |
| Prompt appears right after login | `lastAuthAt` not set at session creation |
| Step-up works, action still blocked | Middleware reads a different session (wrong `sid`) or requires `mfa` but `amr` doesn't contain it |
| Clock-based flakiness | Mixed `Date.now()` (ms) and Redis TTL (seconds) units |

---

## Quick summary

- Step-up = demand fresh proof before sensitive actions, even in a valid session.
- Store `lastAuthAt` and `amr` on the Redis session; enforce in middleware on the server.
- A dedicated, rate-limited step-up endpoint updates the session; the client retries after a `STEP_UP_REQUIRED` response.
- Use short windows, or single-use grants, for the most critical actions.

**Next:** [OAuth 2.0 fundamentals](../06-oauth/00-oauth2-fundamentals.md)
