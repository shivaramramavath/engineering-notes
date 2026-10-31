# Multi-Device Sessions

Every login creates its own independent session, so a user can be signed in on a laptop, phone and tablet at once, see them in a list, and sign any of them out individually. This note covers the model, the "active devices" endpoints, and the limits you should set.

**Prerequisite:** [01-redis-sessions.md](./01-redis-sessions.md)

---

## The model

```text
User U1
 ├── session:S1  Laptop   Chrome / macOS     refresh secret A1
 ├── session:S2  Phone    Safari / iOS       refresh secret B1
 └── session:S3  Tablet   Chrome / Android   refresh secret C1
```

Each session has its **own** refresh secret and rotation chain. That isolation is what makes things safe and manageable:

- Theft of the phone's token and the resulting reuse detection revokes only `S2`.
- "Log out of the tablet" deletes `S3` and leaves the rest alone.
- Rotation on one device never invalidates another.

A shared refresh token across devices would break all three properties.

---

## Capture device metadata at login

```js
const { refreshToken, sessionId } = await createSession(user._id.toString(), {
  ip: req.ip,
  userAgent: req.get("user-agent"),
});
```

| Field | Use | Notes |
|---|---|---|
| `createdAt` | "Signed in on 30 Oct" | |
| `lastUsedAt` | "Last active 5 min ago" | Updated on refresh, so it's approximate |
| `userAgent` | Derive "Chrome on macOS" | Parse with a UA library; truncate the raw string |
| `ip` | "Near Hyderabad, IN" via GeoIP | IPs are personal data under GDPR-style laws; keep only as long as useful |

`req.ip` is correct only if `trust proxy` is configured; otherwise it's your proxy's address.

User-Agent and IP are **informational**, not trusted: both can be spoofed. Don't use them as the sole basis for security decisions (they are useful for anomaly alerts, e.g. "new device sign-in").

---

## Endpoints

```text
GET    /api/auth/sessions             list my active sessions
DELETE /api/auth/sessions/:id         sign out one device
DELETE /api/auth/sessions             sign out all OTHER devices
```

### List

```js
router.get("/sessions", authenticate, async (req, res) => {
  const sessions = await listSessions(req.user.sub);        // from 01-redis-sessions.md

  res.json(
    sessions
      .sort((a, b) => Number(b.lastUsedAt) - Number(a.lastUsedAt))
      .map((s) => ({
        id: s.id,
        device: describeUserAgent(s.userAgent),
        ip: maskIp(s.ip),
        createdAt: Number(s.createdAt),
        lastUsedAt: Number(s.lastUsedAt),
        current: s.id === req.user.sid,                    // sid claim from the access JWT
      }))
  );
});
```

The `sid` claim in the access token is what lets the server mark the **current** session.

Never return `refreshTokenHash`, and consider whether to expose raw session ids to the client. They're random handles, not credentials (the secret half is separate), but exposing only an opaque public id is cleaner if you want to be strict.

### Revoke one: check ownership

```js
router.delete("/sessions/:id", authenticate, async (req, res) => {
  const session = await getSession(req.params.id);

  // The session must exist AND belong to the caller (prevents IDOR)
  if (!session || session.userId !== req.user.sub) {
    return res.status(404).json({ message: "Not found" });
  }

  await revokeSession(req.user.sub, req.params.id);
  res.status(204).end();
});
```

Skipping the `userId` comparison would let any logged-in user delete anyone's session by guessing ids, a textbook IDOR ([00-authn-vs-authz.md](../01-fundamentals/00-authn-vs-authz.md)).

### Revoke all others

```js
router.delete("/sessions", authenticate, async (req, res) => {
  await revokeOtherSessions(req.user.sub, req.user.sid);   // from 03-password-change-and-reset.md
  res.status(204).end();
});
```

If the user revokes the **current** session via `DELETE /sessions/:id`, treat it like logout: also clear the refresh cookie and the client's access token.

---

## Limits

Unbounded sessions let one account (or an attacker credential-stuffing) fill your Redis.

```js
const MAX_SESSIONS = 10;

async function enforceSessionLimit(userId) {
  const sessions = await listSessions(userId);
  if (sessions.length < MAX_SESSIONS) return;

  const oldest = sessions.sort((a, b) => Number(a.lastUsedAt) - Number(b.lastUsedAt))[0];
  await revokeSession(userId, oldest.id);     // evict least-recently-used
}
```

Call it before `createSession`. Alternatively, track the index as a sorted set scored by `createdAt` (`ZADD` / `ZRANGE`), so finding the oldest is O(log n) without loading every hash. The `Set` used so far is fine for a handful of devices.

---

## Product behavior to decide

| Question | Common choice |
|---|---|
| Same device logging in twice | Replace the old session if you can identify the device; otherwise allow both |
| Single-session policy (e.g. one active stream) | Revoke all other sessions on login |
| "Remember me" | Longer absolute TTL for that session; shorter otherwise |
| New-device alert email | Send when a login has an unseen device fingerprint/IP range |

Device "identification" is fuzzy; a long-lived random device id in a cookie is more reliable than a User-Agent comparison, but it is also just a hint.

---

## Common mistakes and debugging

| Mistake / symptom | Cause |
|---|---|
| User can see or revoke others' sessions | Missing ownership check on `:id` |
| Revoking a device doesn't log it out immediately | Its access token stays valid until `exp`; see [04-logout-and-revocation.md](./04-logout-and-revocation.md) |
| Ghost devices listed | Stale ids in the index set (clean lazily) |
| "Current device" flag never true | `sid` missing from the JWT, or comparing different types |
| Logging in on a second device logs out the first | Shared refresh secret, or code replaces a single `refreshToken` field on the user |
| Session list slow | Loading every hash with no cap; add the limit |

---

## Quick summary

- One independent session (and rotation chain) per login, so devices are isolated.
- Store light metadata for a friendly device list; treat it as informational.
- Always verify the session belongs to the caller before revoking.
- Cap sessions per user and evict the oldest.

**Next:** [04-logout-and-revocation.md](./04-logout-and-revocation.md)
