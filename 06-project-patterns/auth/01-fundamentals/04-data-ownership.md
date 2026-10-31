# Data Ownership

Where does each piece of authentication data live: MongoDB, Redis, or the browser? Getting this wrong produces either insecure storage or a slow, stateful mess. The rule is simple: **permanent identity in MongoDB, temporary credentials in Redis, short-lived client state in the browser.**

**Prerequisite:** [03-crypto-primitives.md](./03-crypto-primitives.md)

---

## The three stores

```text
MongoDB                  Redis                       Browser
(permanent)              (temporary, TTL)            (client-held)
─────────────            ─────────────────           ─────────────────
users                    session:{sessionId}         accessToken  → JS memory
 ├ email                 user:sessions:{userId}      refreshToken → HttpOnly cookie
 ├ passwordHash          password-reset:token:{h}
 ├ role                  email-verification:{h}
 ├ isEmailVerified       rate-limit:...
 ├ isActive
 └ timestamps
```

### Why this split

| Question | Answer |
|---|---|
| Who is the user? Does the account exist? | MongoDB: durable, queryable, must survive restarts |
| Which logins are active right now? | Redis: fast lookups, automatic expiry |
| Can this session continue? | Redis: delete the key and it is revoked instantly |
| Can this request call the API? | The JWT itself: no store lookup needed |

Redis is chosen for credentials because of **TTLs**: expiry is automatic, so abandoned sessions and tokens clean themselves up. MongoDB is chosen for accounts because losing them is unacceptable.

---

## Redis data shapes

```text
session:{sessionId}                  (Hash, TTL 30d)
   userId, refreshTokenHash, status, createdAt, lastUsedAt

user:sessions:{userId}               (Set of sessionIds)
   → powers "logout all devices" and a device list

password-reset:token:{sha256}        (String JSON, TTL 15m)
   { userId }

email-verification:{sha256}          (String JSON, TTL 30m)
   { userId }
```

Observations:

- Token keys use the **hash** of the token, so a Redis dump contains nothing usable.
- Every key has a TTL. A temporary credential without one is a bug.
- `user:sessions:{userId}` is an index only; expired sessions may linger in it until cleaned, so code must tolerate dangling ids.

---

## What belongs in the browser

| Item | Location | Why |
|---|---|---|
| Access JWT | JS memory only | Lost on reload, which is safe: refresh restores it |
| Refresh token | `HttpOnly; Secure; SameSite=Strict; Path=/api/auth/refresh` cookie | Unreadable by scripts, sent only where needed |

Never persist the access token in `localStorage`, `sessionStorage` or IndexedDB. See [token-storage](../03-tokens/06-token-storage.md) for the full trade-off discussion.

---

## What must never be stored (anywhere durable)

```text
❌ Plaintext passwords
❌ Raw refresh tokens, reset tokens, or verification tokens
❌ Access tokens
```

Only hashes of server-validated tokens are stored server-side; only the user holds the raw values.

---

## Authoritative source for each fact

Duplicated data drifts out of sync. Know which store wins:

| Fact | Source of truth | Copy / cache |
|---|---|---|
| User role | MongoDB `users.role` | `role` claim in JWT (up to ~10 min stale) |
| Account disabled | MongoDB `isActive` | Must be checked at login and refresh |
| Session valid | Redis `session:{id}` | `sid` claim in JWT (not re-checked per request) |

Consequence: access tokens remain valid until they expire even after a session is deleted. That window (the access TTL) is the accepted cost of stateless verification. If an endpoint is sensitive enough to need instant revocation, check Redis (`sid`) or the database for it.

---

## Failure modes

| Failure | Effect | Mitigation |
|---|---|---|
| Redis restarts without persistence | All sessions/tokens lost: users log in again | Enable persistence (AOF/RDB) and HA in production |
| Redis unavailable | Refresh, login, reset fail; existing access tokens keep working until expiry | Monitoring, replicas, fail closed for auth |
| MongoDB unavailable | Login and account lookups fail | Replica set |

Failing **closed** (deny when the store is unreachable) is the right default for authentication.

---

## Quick summary

- MongoDB = permanent identity; Redis = temporary credentials with TTL; browser = access token in memory, refresh in HttpOnly cookie.
- Store only hashes of server-validated tokens.
- Every temporary key gets a TTL.
- Know the source of truth for each fact; JWT claims are cached copies.
- Fail closed when a store is down.

**Next:** [Passwords: hashing with Argon2id](../02-passwords/00-hashing-argon2id.md)
