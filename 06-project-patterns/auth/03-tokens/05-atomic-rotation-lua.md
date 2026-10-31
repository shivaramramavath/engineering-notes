# Atomic Rotation with Redis Lua

The rotation logic from the previous note reads, compares, then writes. Between the read and the write, another request can slip in. This note shows the race and fixes it with a Redis Lua script, which Redis executes as one indivisible operation.

**Prerequisite:** [04-refresh-token-rotation.md](./04-refresh-token-rotation.md)

---

## The race

```text
Time   Request A                    Request B
t1     HGET hash → H(R1)
t2                                  HGET hash → H(R1)
t3     compare OK
t4                                  compare OK        ← both think R1 is valid
t5     HSET hash = H(R2)
t6                                  HSET hash = H(R3) ← overwrites A's token
```

Both requests succeed and receive different "valid" tokens; A's token is already dead. Worse, an attacker holding a stolen R1 can win the race alongside the real user, and the rotation guarantee ("only one request can consume R1") is gone.

Needed behavior:

```text
Check R1 → if current → replace with R2 → exactly ONE request succeeds
```

---

## Why Lua

Redis runs commands one at a time on a single thread, and a Lua script runs to completion without any other command interleaving. So "read, compare, write" inside a script is atomic. (`WATCH`/`MULTI` optimistic transactions also work but require retry loops; a script is simpler.)

---

## The script

```lua
-- redis-scripts/rotate-refresh.lua
-- KEYS[1] = session key              e.g. session:S1
-- ARGV[1] = SHA-256 of presented secret
-- ARGV[2] = SHA-256 of the NEW secret
-- ARGV[3] = now (ms)
--
-- Returns: 1 = rotated, 0 = no such / inactive session, -1 = hash mismatch (reuse)

local stored = redis.call('HGET', KEYS[1], 'refreshTokenHash')
if not stored then
  return 0
end

local status = redis.call('HGET', KEYS[1], 'status')
if status ~= 'active' then
  return 0
end

if stored ~= ARGV[1] then
  redis.call('DEL', KEYS[1])   -- reuse detected: revoke the session
  return -1
end

redis.call('HSET', KEYS[1], 'refreshTokenHash', ARGV[2], 'lastUsedAt', ARGV[3])
return 1
```

Notes:

- In Lua, a missing hash field comes back as `false`, so `if not stored` handles "no session".
- The comparison `stored ~= ARGV[1]` is a plain string comparison inside Redis. It compares **hashes** of attacker-supplied input, so timing offers no useful signal (see [01-token-hashing.md](./01-token-hashing.md)). Lua has no `timingSafeEqual`.
- All keys the script touches must be passed in `KEYS` (required for Redis Cluster). Here there's only the session key. The `user:sessions:{userId}` set isn't touched; cleanup is handled separately.

---

## Calling it from Node

```js
import fs from "fs";

const ROTATE_SCRIPT = fs.readFileSync(new URL("../redis-scripts/rotate-refresh.lua", import.meta.url), "utf8");

export async function rotateRefreshToken(refreshToken) {
  const parts = splitRefreshToken(refreshToken);
  if (!parts) throw new AuthError("Invalid refresh token");

  const newSecret = randomToken(64);

  const result = await redis.eval(ROTATE_SCRIPT, {
    keys: [`session:${parts.sessionId}`],
    arguments: [hashToken(parts.secret), hashToken(newSecret), String(Date.now())],
  });

  if (result === -1) {
    logSecurityEvent("REFRESH_REUSE_DETECTED", { sessionId: parts.sessionId });
    throw new AuthError("Invalid refresh token");
  }
  if (result !== 1) throw new AuthError("Invalid refresh token");

  const userId = await redis.hGet(`session:${parts.sessionId}`, "userId");
  return { sessionId: parts.sessionId, userId, refreshToken: `${parts.sessionId}.${newSecret}` };
}
```

The generation of the new secret happens **outside** the script, because Lua shouldn't need randomness; only its hash enters Redis. If the script returns 1, this caller (and only this caller) holds the valid new secret.

(`node-redis` option names for `eval` have changed between major versions; check the version you installed. For frequent calls, `SCRIPT LOAD` + `EVALSHA` avoids re-sending the script text, and `node-redis` offers `defineScript` for that.)

---

## Proving it works

Fire parallel requests with the same token:

```js
// tests/refresh-rotation.test.js
const token = (await login()).refreshCookie;

const results = await Promise.all(
  Array.from({ length: 10 }, () => request(app).post("/api/auth/refresh").set("Cookie", token))
);

const ok = results.filter((r) => r.status === 200);
expect(ok).toHaveLength(1);   // exactly one winner
```

Note the consequence: the first request wins; others hit a hash that was already rotated and (per this design) **revoke the session**. That's intended for a malicious replay, but it also means *legitimate* concurrent refreshes log the user out. Prevent that on the client:

```js
let refreshing = null;
export function refreshOnce() {
  refreshing ??= doRefresh().finally(() => { refreshing = null; });
  return refreshing;
}
```

See [06-token-storage.md](./06-token-storage.md) for cross-tab handling.

---

## Atomicity beyond rotation

The same pattern protects any "check then change" security operation:

| Operation | Atomic primitive |
|---|---|
| Single-use reset / verify token | `GETDEL` (Redis 6.2+) |
| Rate-limit counter | `INCR` + `EXPIRE` (or Lua) |
| Refresh rotation | Lua script (this note) |
| Revoke all sessions | `MULTI/EXEC` or Lua |

Rule of thumb: if the second step depends on the result of the first, and two requests could interleave, make it atomic.

---

## Common mistakes

- Doing `HGET` in Node, comparing in Node, then `HSET`: the exact race above.
- Using `MULTI/EXEC` alone: it batches commands but doesn't let you branch on a read inside it (without `WATCH`).
- Putting keys inside the script body instead of `KEYS`, which breaks in Redis Cluster.
- Long-running Lua: scripts block all other Redis clients. Keep them tiny.
- Returning different errors to the client for "mismatch" vs "unknown session", which helps attackers; log precisely, respond generically.

## Debugging

| Symptom | Cause |
|---|---|
| `ERR Error running script ... attempt to compare nil` | A field was missing; handle `false` from `HGET` |
| Script returns 0 for a valid session | `status` field missing or not exactly `"active"` |
| Works locally, `NOSCRIPT` in prod | Using `EVALSHA` without loading the script on that node |
| All concurrent refreshes fail | Expected for reuse; add client single-flight |

---

## Quick summary

- Check-then-set in application code races; two requests can both "consume" one token.
- A Lua script is atomic in Redis: compare, rotate and revoke in one step.
- Generate the new secret in Node; pass only hashes to the script.
- Add client-side single-flight so legitimate concurrency doesn't trip reuse detection.

**Next:** [06-token-storage.md](./06-token-storage.md)
