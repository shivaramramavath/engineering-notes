# Refresh Token Rotation

The refresh token is the most valuable credential on the client: it can mint new access tokens for up to 30 days. Rotation limits the damage of theft by making every refresh token **single-use**: each refresh returns a new one and kills the old.

**Prerequisites:** [03-opaque-tokens.md](./03-opaque-tokens.md), [01-token-hashing.md](./01-token-hashing.md)

---

## The problem rotation solves

Without rotation, a stolen refresh token works for its entire lifetime and nobody notices.

```text
No rotation:   R1 ───────────────────────────────► valid for 30 days
                 stolen copy of R1 also works, silently
```

With rotation:

```text
R1 ──refresh──► R2 ──refresh──► R3
                     ▲
          stolen R2 replayed → mismatch → detected
```

Rotation turns theft from a silent, long-lived compromise into something that is detected the next time either party refreshes.

---

## Session data model

```text
session:{sessionId}   (Hash, TTL 30d)
   userId
   refreshTokenHash    ← SHA-256 of the CURRENT secret only
   status              active
   createdAt, lastUsedAt
```

Only the **current** hash is stored. A previous secret has no hash on record; that is why replaying it fails.

---

## Create (login)

```js
export async function createSession(userId) {
  const sessionId = randomToken(32);
  const secret = randomToken(64);

  const key = `session:${sessionId}`;
  await redis
    .multi()
    .hSet(key, {
      userId,
      refreshTokenHash: hashToken(secret),
      status: "active",
      createdAt: Date.now(),
      lastUsedAt: Date.now(),
    })
    .expire(key, 30 * 24 * 60 * 60)
    .sAdd(`user:sessions:${userId}`, sessionId)
    .exec();

  return { sessionId, refreshToken: `${sessionId}.${secret}` };
}
```

`multi()` queues the commands in one transaction, so a crash can't leave a session without a TTL.

---

## Refresh (rotate)

```text
Cookie: S1.SECRET_A
   │
   ▼
Parse → sessionId=S1, secret=SECRET_A
   │
   ▼
Load session:S1 → exists? active?
   │
   ▼
hash(SECRET_A) == stored hash?
   │
   ├─ No  → reject; REVOKE session (possible theft)
   │
   └─ Yes → generate SECRET_B
             store hash(SECRET_B)       ┐ must be ONE atomic step
             issue S1.SECRET_B (cookie) ┘ (see 05-atomic-rotation-lua.md)
             issue new access JWT
```

A readable, non-atomic version to understand the logic first:

```js
export async function rotate(refreshToken) {
  const parts = splitRefreshToken(refreshToken);
  if (!parts) throw new AuthError("Invalid refresh token");

  const key = `session:${parts.sessionId}`;
  const session = await redis.hGetAll(key);
  if (!session.userId || session.status !== "active") throw new AuthError("Invalid refresh token");

  if (!safeCompare(hashToken(parts.secret), session.refreshTokenHash)) {
    await redis.del(key);                       // reuse detected → kill session
    logSecurityEvent("REFRESH_REUSE_DETECTED", { sessionId: parts.sessionId });
    throw new AuthError("Invalid refresh token");
  }

  const newSecret = randomToken(64);
  await redis.hSet(key, { refreshTokenHash: hashToken(newSecret), lastUsedAt: Date.now() });

  return { sessionId: parts.sessionId, userId: session.userId, refreshToken: `${parts.sessionId}.${newSecret}` };
}
```

**This version has a race condition** (two requests can both pass the compare before either writes). The production version in [05-atomic-rotation-lua.md](./05-atomic-rotation-lua.md) fixes it. Learn the logic here, ship the atomic one.

After rotating, the controller sets the new cookie and returns a fresh access token, re-checking that the user still exists and `isActive` in MongoDB.

---

## Reuse detection

Mismatch means one of two things:

1. **An attacker** replayed a stolen, already-rotated token, or the real user is now using a token the attacker already rotated.
2. **A legitimate race** (two tabs refreshing at the same moment; a network retry after a lost response).

Revoking the session on any mismatch is the conservative choice: the affected device logs in again. The cost is occasional false positives from case 2. Mitigations:

- **Client single-flight:** only one refresh request in flight per browser; other tabs/requests wait for its result ([06-token-storage.md](./06-token-storage.md)).
- **Grace window (optional):** keep the previous hash for a few seconds and accept it once, returning the same new token. This adds state and complexity; use only if races are a real problem.

```text
Which side did the attacker land on?
  Attacker refreshes first  → attacker holds R3, user's R2 is rejected → session revoked → both logged out
  User refreshes first      → user holds R3, attacker's R2 is rejected → session revoked
Either way the stolen token stops working and the user must re-authenticate.
```

---

## Lifetimes: idle vs absolute

The Redis TTL above gives an **absolute** 30-day cap, but note that rotation as written doesn't extend it. Two common models:

| Model | Behavior |
|---|---|
| Absolute expiry | Session dies 30 days after login, regardless of activity |
| Sliding expiry | Each rotation resets the TTL (`EXPIRE` again), with an absolute maximum stored as `createdAt` + limit |

Pick explicitly. Sliding with an absolute ceiling is common: active users stay signed in, but nobody stays forever.

---

## Common mistakes

- Rotating the token but not invalidating the old one (e.g. storing a list of valid hashes and never removing).
- Non-atomic check-then-set (race).
- Not revoking on reuse, so the attacker keeps going.
- Forgetting to set the new cookie, so the browser keeps sending the dead token and gets logged out on the next refresh.
- Returning a new access token before checking `isActive`, so disabled users keep renewing.
- Logging the refresh token.

## Debugging

| Symptom | Likely cause |
|---|---|
| Users randomly logged out, mostly multi-tab | Concurrent refresh race; add client single-flight |
| Logged out after exactly one refresh | New cookie not set (path/domain mismatch with the original) |
| `req.cookies.refreshToken` undefined on refresh | Cookie `Path` doesn't match the route; request missing `credentials: "include"` |
| Refresh works in Postman but not browser | `SameSite`/`Secure` or CORS settings |

---

## Quick summary

- Every refresh returns a new token and invalidates the old: single-use.
- Store only the current secret's hash; a stale secret fails and signals theft.
- On mismatch, revoke the session; reduce false positives with client single-flight.
- Rotation must be atomic: next note.

**Next:** [05-atomic-rotation-lua.md](./05-atomic-rotation-lua.md)
