# Logout and Revocation

"Logout" is really **revocation**: making the server stop honoring a credential. Clearing a cookie in the browser only hides it; the session must be destroyed server-side. This note covers single-device logout, logout everywhere, the events that should trigger revocation, and the window left by stateless access tokens.

**Prerequisites:** [01-redis-sessions.md](./01-redis-sessions.md), [03-multi-device.md](./03-multi-device.md)

---

## What can be revoked, and how fast

| Credential | Revocation | Effective when |
|---|---|---|
| Refresh token / session | Delete `session:{id}` | Instantly, on its next use |
| Access JWT | Cannot be revoked by itself | When it expires (≤ ~10 min) |
| Reset / verification token | Delete the key (or `GETDEL`) | Instantly |

```text
Time ─────────────────────────────────────────────►
10:00  session deleted
        │  refresh now fails ✗
        │  access JWT (exp 10:05) still passes verify ✓  ← the revocation window
10:05  access JWT expires → client can't renew → fully logged out
```

---

## Logout (this device)

```js
export async function logout(req, res) {
  const parts = splitRefreshToken(req.cookies.refreshToken);

  if (parts) {
    const session = await getSession(parts.sessionId);
    // Only act if the secret actually matches; don't let strangers kill sessions by id
    if (session && safeCompare(hashToken(parts.secret), session.refreshTokenHash)) {
      await revokeSession(session.userId, parts.sessionId);
    }
  }

  res.clearCookie("refreshToken", cookieOptions);   // SAME options (path, domain, flags) as when set
  res.status(204).end();                            // idempotent: always succeed
}
```

Details:

- **Idempotent:** logging out twice, or without a valid cookie, is not an error. Always clear the cookie and return success.
- **Verify the secret** before deleting. Session ids aren't secret handles on their own; requiring the matching secret prevents someone who learned an id (for example from a device list) from logging a user out.
- **Client:** drop the in-memory access token and reset UI state, and clear any cached user data.

### ⚠️ Cookie `Path` must reach the logout route

If the refresh cookie is set with `Path=/api/auth/refresh`, the browser sends it **only** to that exact path prefix, so `POST /api/auth/logout` will not receive it and can't find the session. Pick one:

| Option | How | Trade-off |
|---|---|---|
| Widen the cookie path | `Path=/api/auth` | Cookie also goes to login/register/etc. Still same-origin auth routes only; the usual practical choice |
| Identify by access token | Use `req.user.sid` (needs a valid access token) | Logout fails if the access token already expired; combine with the cookie as a fallback |
| Put logout under the refresh path | e.g. `POST /api/auth/refresh/logout` | Works, but an unusual URL design |

`clearCookie` must also use the **same** `path` as when the cookie was set, or the browser keeps the cookie.

---

## Logout everywhere / other devices

```js
export async function revokeAllSessions(userId) {
  const ids = await redis.sMembers(`user:sessions:${userId}`);
  const tx = redis.multi();
  ids.forEach((id) => tx.del(`session:${id}`));
  tx.del(`user:sessions:${userId}`);
  await tx.exec();
}
```

"Logout other devices" is the same loop skipping the current `sid` ([03-multi-device.md](./03-multi-device.md)).

Large accounts: `MULTI` with thousands of `DEL`s is fine at this scale; cap sessions per user so it stays small.

---

## Revoke on these events

| Event | Revoke | Reason |
|---|---|---|
| User clicks logout | That session | Intent |
| "Log out everywhere" | All | Intent |
| Password reset | **All** | Attacker may have a session |
| Password change | All others | Same, keep the current device |
| Email change | All others (or step-up) | Account-recovery channel changed |
| MFA enabled/disabled or recovery codes regenerated | All others | Security posture changed |
| Refresh token reuse detected | That session | Probable theft |
| Admin disables account / user deleted | **All** | Access must end now |
| Suspicious activity flagged | All | Containment |

The same `revokeAllSessions(userId)` function serves all of them; call it from services, not controllers, so no code path forgets it.

---

## Closing the access-token window

Most apps accept up to 10 minutes. If you can't, choose a tool by how strict you need to be:

### 1. Shorter access TTL

Cheapest: 5 minutes cuts the window in half at the cost of more refresh calls.

### 2. Per-request session check

```js
export const requireActiveSession = async (req, res, next) => {
  const exists = await redis.exists(`session:${req.user.sid}`);
  if (!exists) return res.status(401).json({ message: "Session ended" });
  next();
};

router.post("/account/delete", authenticate, requireActiveSession, deleteAccount);
```

This reintroduces one Redis lookup, so apply it only to sensitive endpoints (account changes, payments, admin), keeping the cheap JWT-only path for the rest.

### 3. Deny-list by `jti`

Store revoked token ids until their `exp`. Needs a lookup per request, and is mostly redundant with checking `sid`; use it only for revoking a *specific* token without ending the session.

### 4. Disabled-user checks

`isActive` lives in MongoDB. Check it at **refresh** time (always) and at sensitive endpoints, so a disabled user can't renew.

---

## Single-use side effects

- Delete outstanding reset and verification tokens when a password changes.
- Clear device-specific state (push tokens, WebSocket connections). A WebSocket opened with an access token stays open after revocation unless you close it; re-validate the session periodically or on a revoke event.

---

## Common mistakes

- "Logout" that only clears the cookie or `localStorage`.
- Cookie path mismatch: logout route never sees the cookie, or `clearCookie` uses different options.
- Not revoking after password reset/change.
- Returning an error when logout is called without a session (breaks idempotency and UX).
- Assuming a deleted session blocks access immediately (JWT window).
- Letting anyone delete a session by id alone.
- Forgetting long-lived connections (WebSockets, SSE).

## Debugging

| Symptom | Cause |
|---|---|
| Logged out in UI, still logged in after reload | Server session still alive; refresh cookie not cleared (path mismatch) |
| Logout returns 401 | Route requires an access token that has already expired |
| Revoked device keeps working ~10 min | Expected: JWT window; add `requireActiveSession` where it matters |
| `clearCookie` has no effect | Options differ from `res.cookie` (path/domain/secure/sameSite) |

---

## Quick summary

- Logout = delete the server-side session; clearing the cookie alone is not logout.
- Make logout idempotent, require the matching secret, and make sure the cookie `Path` reaches the route.
- Revoke all sessions on password reset, account disable, security changes.
- Access JWTs stay valid until expiry; shorten the TTL or check the session on sensitive routes.

**Next:** [05-csrf.md](./05-csrf.md)
