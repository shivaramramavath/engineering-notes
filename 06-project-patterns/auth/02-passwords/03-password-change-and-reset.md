# Password Change and Reset

Two different flows that both end in a new password hash, but start from different proofs of identity:

| Flow | User is | Proves identity with |
|---|---|---|
| **Change** | Logged in | Current password |
| **Reset** | Locked out | A token sent to their email |

Both must end by **revoking sessions**, because a changed password is often a response to compromise.

**Prerequisites:** [02-register-and-login.md](./02-register-and-login.md), [credential types](../01-fundamentals/01-credential-types.md)

---

## Change password (authenticated)

```text
POST /api/auth/change-password   Authorization: Bearer <access>
{ currentPassword, newPassword }
        │
        ▼
Load user → verify currentPassword
        │
        ▼
Validate newPassword (policy, breach, not same as old)
        │
        ▼
Hash → update MongoDB
        │
        ▼
Revoke all OTHER sessions → issue fresh session for this device
```

```js
export async function changePassword(req, res) {
  const { currentPassword, newPassword } = changePasswordSchema.parse(req.body);

  const user = await User.findById(req.user.sub);
  if (!user || !(await argon2.verify(user.passwordHash, currentPassword))) {
    return res.status(401).json({ message: "Current password is incorrect" });
  }

  if (currentPassword === newPassword || (await isBreached(newPassword))) {
    return res.status(400).json({ message: "Choose a different, non-breached password" });
  }

  user.passwordHash = await argon2.hash(newPassword, hashOptions);
  await user.save();

  await revokeOtherSessions(user._id.toString(), req.user.sid);
  res.json({ message: "Password updated" });
}
```

Why re-ask for the current password even though the user is logged in: a stolen access token or an unattended laptop must not be enough to take over the account. This is a case of step-up authentication ([step-up-auth](../05-mfa/05-step-up-auth.md)).

Keeping the current device signed in (`revokeOtherSessions`) is friendlier; the strict alternative revokes everything and forces a fresh login.

---

## Reset password (forgot)

Two endpoints:

### 1. Request: `POST /api/auth/forgot-password`

```text
{ email }
   │
   ▼
Always respond with the same generic message
   │
   ▼ (only if the user exists)
Generate 256-bit token → store SHA-256 in Redis (TTL 15m) → email raw token
```

```js
export async function forgotPassword(req, res) {
  const { email } = forgotSchema.parse(req.body);

  const user = await User.findOne({ email });
  if (user && user.isActive) {
    const token = crypto.randomBytes(32).toString("base64url");
    const tokenHash = hashToken(token);

    await redis.set(
      `password-reset:token:${tokenHash}`,
      JSON.stringify({ userId: user._id.toString() }),
      { EX: 15 * 60 }
    );

    // don't await: keeps response time identical whether or not user exists
    sendResetEmail(user.email, token).catch(logEmailFailure);
  }

  res.json({ message: "If an account exists for this email, a reset link has been sent." });
}
```

The email link points to the **frontend**: `https://yourapp.com/reset-password?token=...`. The page should remove the token from the address bar immediately ([see logging and URL hygiene](../08-hardening/05-logging-and-audit.md)).

Sending the email without awaiting avoids a timing difference: an existing account would otherwise respond slower than a non-existent one.

### 2. Confirm: `POST /api/auth/reset-password`

```js
export async function resetPassword(req, res) {
  const { token, password } = resetSchema.parse(req.body);
  const tokenHash = hashToken(token);

  // GETDEL (Redis 6.2+) reads and deletes in one atomic step → strictly single-use,
  // even if two requests race with the same token.
  const data = await redis.getDel(`password-reset:token:${tokenHash}`);
  if (!data) {
    return res.status(400).json({ message: "Invalid or expired reset link" });
  }

  const { userId } = JSON.parse(data);

  if (await isBreached(password)) {
    return res.status(400).json({ message: "Choose a non-breached password" });
  }

  const passwordHash = await argon2.hash(password, hashOptions);
  await User.updateOne({ _id: userId }, { passwordHash });

  await revokeAllSessions(userId);

  res.json({ message: "Password updated. Please sign in." });
}
```

Notes on this design:

- Use `getDel` rather than `get` + `del` so a token cannot be used twice by racing requests.
- Consuming the token before validating the new password means a rejected password burns the link. The alternative (validate first, then consume) avoids frustrating users; either is defensible. Validate cheap checks (schema, policy) **before** consuming, and consume right before the write.
- After a successful reset do **not** auto-login; require a normal login (and MFA). Resetting via email alone must not bypass a second factor.
- Optionally, invalidate other outstanding reset tokens for the user and send a "your password was changed" notification email.

---

## Revoking sessions

```js
export async function revokeAllSessions(userId) {
  const ids = await redis.sMembers(`user:sessions:${userId}`);
  const tx = redis.multi();
  for (const id of ids) tx.del(`session:${id}`);
  tx.del(`user:sessions:${userId}`);
  await tx.exec();
}

export async function revokeOtherSessions(userId, keepSessionId) {
  const ids = (await redis.sMembers(`user:sessions:${userId}`)).filter((id) => id !== keepSessionId);
  const tx = redis.multi();
  for (const id of ids) {
    tx.del(`session:${id}`);
    tx.sRem(`user:sessions:${userId}`, id);
  }
  await tx.exec();
}
```

What revocation does and doesn't do: refresh tokens die immediately, but already-issued **access tokens stay valid until they expire** (~10 minutes). That window is the accepted cost of stateless JWTs; see [logout-and-revocation](../04-sessions/04-logout-and-revocation.md).

---

## Security checklist

| Item | Reason |
|---|---|
| Reset token: 256-bit random, hash in Redis, 15 min TTL, single-use | Limits theft and replay |
| Generic response on forgot-password | No account enumeration |
| Rate limit by IP **and** email | Stops email bombing and token guessing |
| Revoke all sessions after reset | Evicts an attacker who already had access |
| `Referrer-Policy: no-referrer` on reset page | Token in the URL must not leak to third parties |
| Don't auto-login after reset | Keeps MFA meaningful |
| Notify the user by email of changes | Lets them detect unauthorized changes |
| Reset links go to your fixed frontend URL, never a host from the request | Prevents Host-header poisoning |

---

## Common mistakes and debugging

| Symptom / mistake | Cause / fix |
|---|---|
| Reset link always "invalid" | Frontend sent the raw token but backend hashed a different encoding; hash exactly the string emailed |
| Link works twice | Used `get` then `del` instead of atomic `getDel` |
| Old devices still logged in after reset | Forgot to revoke sessions (or revoked only the Redis hash, not the user set) |
| Reset email link points to attacker domain | Built from `req.headers.host`; use a configured `FRONTEND_URL` |
| Existing-account timing leak | Awaited email sending only when the user exists |
| User locked out after password change | Revoked the current session without issuing a new one |

---

## Quick summary

- Change = prove with current password; reset = prove with an emailed single-use token.
- Reset tokens: random, hashed in Redis, short TTL, consumed atomically.
- Same generic response whether or not the account exists.
- Always revoke sessions after a password change; don't auto-login after reset.
- Remember that access tokens outlive session revocation until they expire.

**Next:** [Tokens: secure random tokens](../03-tokens/00-secure-random-tokens.md)
