# Redis Sessions

The session store used by this architecture: one Redis hash per login, plus an index set per user. This note is the complete **session service**: create, read, renew, list, revoke and clean up.

**Prerequisites:** [00-session-basics.md](./00-session-basics.md), [04-refresh-token-rotation.md](../03-tokens/04-refresh-token-rotation.md)

---

## Why Redis

| Need | Redis feature |
|---|---|
| Automatic expiry | `EXPIRE` / TTL |
| Instant revoke | `DEL` |
| Fast lookup | In-memory, O(1) hash access |
| Group per user | Sets |
| Atomic multi-step logic | `MULTI`, Lua |

Sessions are *temporary state*; MongoDB holds permanent accounts ([04-data-ownership.md](../01-fundamentals/04-data-ownership.md)).

---

## Key design

```text
session:{sessionId}           Hash   TTL 30d
   userId              "U1"
   refreshTokenHash    sha256(current secret)
   status              "active"
   createdAt           1754650000000
   lastUsedAt          1754651000000
   ip                  "203.0.113.7"          (optional metadata)
   userAgent           "Mozilla/5.0 ..."      (optional, truncated)

user:sessions:{userId}        Set    members = sessionIds
```

```text
          user:sessions:U1                 session:S1  ─ Laptop
          ┌────────────┐                   session:S2  ─ Phone
          │ S1  S2  S3 │ ───────────────►  session:S3  ─ Tablet
          └────────────┘
          (index only; the hashes are the truth)
```

Always **namespace** keys with a prefix so you can reason about and monitor them, and never build keys from unvalidated input (validate `sessionId` format before using it in a key name).

---

## The service

```js
// services/session.service.js
import { redis } from "../config/redis.js";
import { randomToken, hashToken } from "../utils/crypto.js";

const SESSION_TTL = 30 * 24 * 60 * 60; // seconds
const sKey = (id) => `session:${id}`;
const uKey = (userId) => `user:sessions:${userId}`;

export async function createSession(userId, meta = {}) {
  const sessionId = randomToken(32);
  const secret = randomToken(64);
  const now = Date.now();

  await redis
    .multi()
    .hSet(sKey(sessionId), {
      userId,
      refreshTokenHash: hashToken(secret),
      status: "active",
      createdAt: now,
      lastUsedAt: now,
      ip: meta.ip ?? "",
      userAgent: (meta.userAgent ?? "").slice(0, 200),
    })
    .expire(sKey(sessionId), SESSION_TTL)
    .sAdd(uKey(userId), sessionId)
    .expire(uKey(userId), SESSION_TTL)
    .exec();

  return { sessionId, refreshToken: `${sessionId}.${secret}` };
}

export async function getSession(sessionId) {
  const s = await redis.hGetAll(sKey(sessionId));
  return s.userId ? s : null;     // hGetAll returns {} for a missing key
}

export async function revokeSession(userId, sessionId) {
  await redis.multi().del(sKey(sessionId)).sRem(uKey(userId), sessionId).exec();
}
```

`multi()` sends the commands as one `MULTI/EXEC` transaction: they execute together, so there is no window where the session exists without a TTL. (It does not make a *read-then-write* decision atomic; that needs Lua, as in [05-atomic-rotation-lua.md](../03-tokens/05-atomic-rotation-lua.md).)

The refresh/rotation step is the Lua script from the tokens chapter; it also updates `lastUsedAt`.

### TTL on the index set

The `user:sessions:{userId}` set gets a TTL too, refreshed whenever a session is created, so abandoned users' sets disappear. Its TTL is therefore at least as long as the latest session's.

---

## Expiry policy

Create sets an **absolute** 30-day TTL. To make activity extend the session (sliding expiry), re-issue `EXPIRE` on rotation, but cap it with an absolute limit:

```js
const MAX_AGE_MS = 90 * 24 * 60 * 60 * 1000;
if (Date.now() - Number(session.createdAt) > MAX_AGE_MS) {
  await revokeSession(session.userId, sessionId);   // hard stop
  throw new AuthError("Session expired");
}
await redis.expire(sKey(sessionId), SESSION_TTL);   // sliding
```

---

## Dangling ids in the index set

When a session hash expires by TTL, its id stays in `user:sessions:{userId}`. The set is an **index, not the source of truth**, so code that reads it must tolerate stale members and clean them:

```js
export async function listSessions(userId) {
  const ids = await redis.sMembers(uKey(userId));
  if (ids.length === 0) return [];

  const pipeline = redis.multi();
  ids.forEach((id) => pipeline.hGetAll(sKey(id)));
  const rows = await pipeline.exec();

  const live = [];
  const dead = [];
  rows.forEach((row, i) => (row.userId ? live.push({ id: ids[i], ...row }) : dead.push(ids[i])));

  if (dead.length) await redis.sRem(uKey(userId), dead);   // lazy cleanup
  return live;
}
```

(Reply shape from `exec()` differs slightly across `node-redis` versions; inspect what yours returns.)

---

## Operational notes

- **Persistence:** Redis is in-memory. With no persistence (or `allkeys-lru` eviction) a restart or memory pressure silently logs users out. For sessions use persistence (AOF/RDB) and an eviction policy that doesn't evict them (`volatile-ttl` or `noeviction` on a dedicated instance).
- **Memory estimate:** ~400-500 bytes per session; 1 million sessions is roughly half a gigabyte. Measure with `MEMORY USAGE session:<id>`.
- **Never use `KEYS`** in production (blocks Redis); use `SCAN`, or better, use the per-user set.
- **High availability:** use a replica/sentinel/managed service; decide whether auth fails closed (deny) when Redis is down. It should.
- **Separate Redis databases/instances** for sessions vs cache, so a cache flush can't log everyone out.

---

## Common mistakes

| Mistake | Effect |
|---|---|
| No TTL on the session or set | Memory grows forever |
| Treating the index set as authoritative | Phantom sessions in the UI, wrong counts |
| Storing the raw refresh secret | Redis leak = session hijack |
| Using `KEYS session:*` in code | Blocks Redis at scale |
| Same Redis for cache and sessions with eviction on | Random logouts under memory pressure |
| Updating `lastUsedAt` on every API request | Redis write per request; update per refresh instead |

## Debugging

```bash
docker compose exec redis redis-cli -a devpass
```

```text
HGETALL session:<id>            # contents
TTL session:<id>                # -2 missing, -1 no expiry (bug), >0 seconds left
SMEMBERS user:sessions:<userId> # index
EXISTS session:<id>             # 1/0
```

| Symptom | Check |
|---|---|
| Everyone logged out after deploy | Redis restarted without persistence, or `REDIS_URL` points to a fresh instance |
| Session list shows ghost devices | Dangling ids; use the cleanup in `listSessions` |
| `TTL = -1` | A write path forgot `expire` |

---

## Quick summary

- One hash per login (`session:{id}`) with TTL, plus a per-user index set.
- Store only the hash of the refresh secret.
- The set is an index; tolerate and clean stale ids.
- Plan for persistence, eviction and Redis outages.

**Next:** [02-cookie-sessions.md](./02-cookie-sessions.md)
