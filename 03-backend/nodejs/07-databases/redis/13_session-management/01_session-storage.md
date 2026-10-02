# Session Storage

A **session** is server-side state about a logged-in user, found through an **opaque ID** the browser sends in a cookie. Redis stores it shared across every app instance, expires it automatically, and lets you delete it instantly.

## The flow

```
POST /login ──► verify credentials ──► create session in Redis ──► Set-Cookie: sid=<random>
GET  /api/me (cookie: sid) ──► hash(sid) ──► Redis lookup ──► session found? ──► handle request
POST /logout ──► delete the Redis key ──► clear cookie
```

The cookie carries **only a random ID**. All real data stays on the server.

## Session IDs

```ts
import { randomBytes, createHash } from "node:crypto";

export const newSessionId = () => randomBytes(32).toString("base64url");     // 256 bits of entropy
export const sha256 = (s: string) => createHash("sha256").update(s).digest("hex");
```

| Rule | Why |
|------|-----|
| **At least 128 bits** from a CSPRNG (`crypto.randomBytes`) | Unguessable. Never use `Math.random()`, timestamps, user IDs or UUID v1 |
| **Opaque** (no embedded data) | Nothing to forge or decode |
| **Store the hash** (`sha256(sid)`) as the Redis key | If someone reads Redis (a backup, a replica, a debug tool), they get hashes, not usable cookies |
| Regenerate on login and privilege change | Prevents **session fixation** |

Because the ID is already high-entropy, a plain fast hash (SHA-256) is enough. You don't need a slow password hash here.

## Data model

```
shop:session:{sha256(sid)}      hash   userId, role, createdAt, lastSeenAt, ip, ua, csrf, data (JSON)   TTL = idle timeout (capped by absolute)
shop:user:{id}:sessions         zset   session hash → lastSeenAt        for "my devices" and "log out everywhere"
```

A **hash** (not one JSON blob) lets you read and update single fields atomically, and see them in Redis tools ([Hashes](../04_data-structures/02_hashes.md)). Put arbitrary extras in a JSON `data` field, but keep it **small**.

Add the keys to your [registry](../08_nodejs-integration/05_redis-key-builder.md):

```ts
// src/redis/keys.ts (additions)
session:       (hash: string) => K.key("session", hash),
userSessions:  (userId: Id)   => K.key("user", userId, "sessions"),
```

## Expiration: idle and absolute

Two clocks protect an account:

| | **Idle timeout** | **Absolute timeout** |
|---|------------------|----------------------|
| Meaning | Logged out after N minutes of **inactivity** | Logged out N hours or days after login, **regardless of activity** |
| Implemented by | `EXPIRE` refreshed on activity (sliding) | Compare against `createdAt` |
| Protects against | Abandoned devices | A stolen session being kept alive forever |

Guidance (adjust to your risk): sensitive apps use short idle timeouts of minutes. Everyday apps use tens of minutes to a few hours, with an absolute limit of days or weeks. "Remember me" should be a **separate long-lived token** ([Token Management](./02_token-management.md)), not an endlessly extended session.

### Don't write on every request

Refreshing `lastSeenAt` and the TTL on **every** request adds a Redis write per request. **Throttle** it: only touch if the last touch is older than, say, 60 seconds. The idle timeout stays accurate to within that interval.

## The touch script

One atomic script checks the absolute limit and extends the idle TTL, but never beyond the absolute deadline:

```lua
-- session-touch.lua
-- KEYS[1] = session hash
-- ARGV[1] = now (ms), ARGV[2] = idle seconds, ARGV[3] = absolute seconds, ARGV[4] = touch interval seconds
local created = tonumber(redis.call("HGET", KEYS[1], "createdAt"))
if not created then return 0 end                                    -- no such session

local now = tonumber(ARGV[1])
local remainingAbs = math.floor((created + tonumber(ARGV[3]) * 1000 - now) / 1000)
if remainingAbs <= 0 then
  redis.call("DEL", KEYS[1])                                        -- absolute lifetime exceeded
  return 0
end

local last = tonumber(redis.call("HGET", KEYS[1], "lastSeenAt") or "0")
if now - last >= tonumber(ARGV[4]) * 1000 then                      -- throttled refresh
  redis.call("HSET", KEYS[1], "lastSeenAt", now)
  redis.call("EXPIRE", KEYS[1], math.min(tonumber(ARGV[2]), remainingAbs))
end
return 1
```

All timestamps use the app servers' clocks. Skew of a few seconds between instances is irrelevant for lifetimes of minutes and hours.

## The session store

```ts
// src/auth/session-store.ts
import type { Redis } from "ioredis";
import { keys } from "../redis/keys.js";
import { newSessionId, sha256 } from "./ids.js";

declare module "ioredis" {
  interface RedisCommander<Context> {
    sessionTouch(key: string, nowMs: number, idleSec: number, absSec: number, touchSec: number): Promise<number>;
  }
}

export interface Session {
  userId: string;
  role: string;
  createdAt: number;
  lastSeenAt: number;
  ip: string;
  ua: string;
  csrf: string;
  data: Record<string, unknown>;
}

export interface SessionOptions {
  idleSec: number;            // e.g. 30 * 60
  absoluteSec: number;        // e.g. 7 * 86_400
  touchEverySec?: number;     // default 60
  maxPerUser?: number;        // concurrent sessions allowed, oldest evicted
}

const toSession = (h: Record<string, string>): Session | null =>
  h.userId
    ? {
        userId: h.userId, role: h.role ?? "user",
        createdAt: Number(h.createdAt), lastSeenAt: Number(h.lastSeenAt),
        ip: h.ip ?? "", ua: h.ua ?? "", csrf: h.csrf ?? "",
        data: h.data ? JSON.parse(h.data) : {},
      }
    : null;

export class SessionStore {
  constructor(private redis: Redis, private o: SessionOptions) {}

  async create(userId: string, meta: { role: string; ip: string; ua: string }) {
    const sid = newSessionId();
    const hash = sha256(sid);
    const now = Date.now();
    const key = keys.session(hash);
    const idx = keys.userSessions(userId);

    await this.redis.multi()
      .hset(key, {
        userId, role: meta.role, createdAt: now, lastSeenAt: now,
        ip: meta.ip, ua: meta.ua.slice(0, 200), csrf: newSessionId(), data: "{}",
      })
      .expire(key, Math.min(this.o.idleSec, this.o.absoluteSec))
      .zadd(idx, now, hash)
      .expire(idx, this.o.absoluteSec)
      .exec();

    if (this.o.maxPerUser) await this.enforceLimit(userId, this.o.maxPerUser);
    return sid;                                   // give THIS to the browser, never the hash
  }

  /** Looks the session up and (throttled) extends it. Returns null if missing, idle-expired or past its absolute limit. */
  async get(sid: string): Promise<Session | null> {
    const key = keys.session(sha256(sid));
    const res = await this.redis.pipeline()
      .sessionTouch(key, Date.now(), this.o.idleSec, this.o.absoluteSec, this.o.touchEverySec ?? 60)
      .hgetall(key)
      .exec();

    const [[, ok], [, hash]] = res as [[Error | null, number], [Error | null, Record<string, string>]];
    return ok === 1 ? toSession(hash) : null;
  }

  async destroy(sid: string) {
    const hash = sha256(sid);
    const key = keys.session(hash);
    const userId = await this.redis.hget(key, "userId");
    const m = this.redis.multi().unlink(key);
    if (userId) m.zrem(keys.userSessions(userId), hash);
    await m.exec();
  }

  /** "Log out everywhere": password change, account compromise. */
  async destroyAllForUser(userId: string, exceptSid?: string) {
    const idx = keys.userSessions(userId);
    const keep = exceptSid ? sha256(exceptSid) : null;
    const hashes = (await this.redis.zrange(idx, 0, -1)).filter((h) => h !== keep);

    await Promise.all(hashes.map((h) => this.redis.unlink(keys.session(h))));   // per key: Cluster-safe
    if (hashes.length) await this.redis.zrem(idx, ...hashes);
  }

  /** New ID, same data. Call after login and after privilege changes (prevents fixation). */
  async rotate(oldSid: string): Promise<string | null> {
    const old = await this.get(oldSid);
    if (!old) return null;
    const sid = await this.create(old.userId, { role: old.role, ip: old.ip, ua: old.ua });
    await this.redis.hset(keys.session(sha256(sid)), "data", JSON.stringify(old.data));
    await this.destroy(oldSid);
    return sid;
  }

  /** For a "your devices" page. Cleans index entries whose session has expired. */
  async list(userId: string) {
    const idx = keys.userSessions(userId);
    const hashes = await this.redis.zrevrange(idx, 0, -1);
    if (!hashes.length) return [];

    const p = this.redis.pipeline();
    hashes.forEach((h) => p.hgetall(keys.session(h)));
    const res = (await p.exec()) ?? [];

    const live: (Session & { id: string })[] = [];
    const dead: string[] = [];
    res.forEach(([, h], i) => {
      const s = toSession(h as Record<string, string>);
      if (s) live.push({ ...s, id: hashes[i]!.slice(0, 12) });     // a short, non-secret handle
      else dead.push(hashes[i]!);
    });
    if (dead.length) await this.redis.zrem(idx, ...dead);
    return live;
  }

  /** Keep at most `max` sessions: evict the least recently active. */
  private async enforceLimit(userId: string, max: number) {
    const idx = keys.userSessions(userId);
    const extra = (await this.redis.zcard(idx)) - max;
    if (extra <= 0) return;
    const oldest = await this.redis.zrange(idx, 0, extra - 1);
    await Promise.all(oldest.map((h) => this.redis.unlink(keys.session(h))));
    await this.redis.zrem(idx, ...oldest);
  }
}
```

Register the script once (see [Lua Scripts](../06_advanced-commands/03_lua-scripts.md)): `redis.defineCommand("sessionTouch", { numberOfKeys: 1, lua: load("session-touch") })`.

Design notes:

| Choice | Reason |
|--------|--------|
| Key is `sha256(sid)` | A leaked Redis dump isn't a set of valid cookies |
| `touch` and `hgetall` in one pipeline | One round trip per request |
| Per-user sorted set (score = last activity) | Device list, global logout, and "evict the oldest" for session limits |
| Index entries may dangle after expiry | `list` cleans them, and the index itself has the absolute TTL |
| `unlink` per key | Works in Cluster, where session keys live in different slots |
| `csrf` token stored in the session | Server-side CSRF token without an extra store |

## Express middleware

```ts
// src/http/middleware/session.ts
import cookieParser from "cookie-parser";
import type { Request, Response, NextFunction } from "express";

const COOKIE = process.env.NODE_ENV === "production" ? "__Host-sid" : "sid";

const cookieOptions = (maxAgeSec: number) => ({
  httpOnly: true,                                    // not readable from JavaScript (blocks XSS theft)
  secure: process.env.NODE_ENV === "production",     // HTTPS only (required by the __Host- prefix)
  sameSite: "lax" as const,                          // not sent on cross-site POST requests
  path: "/",
  maxAge: maxAgeSec * 1000,                          // the cookie lives as long as the absolute limit
});

export function sessions(store: SessionStore, absoluteSec: number) {
  return [
    cookieParser(),
    async (req: Request, res: Response, next: NextFunction) => {
      const sid = req.cookies?.[COOKIE];
      req.sid = sid;
      req.session = sid ? await store.get(sid) : null;
      if (sid && !req.session) res.clearCookie(COOKIE, { path: "/" });   // stale cookie
      next();
    },
  ];
}

export async function login(store: SessionStore, req: Request, res: Response, user: { id: string; role: string }, absoluteSec: number) {
  if (req.sid) await store.destroy(req.sid);                             // never reuse a pre-login session ID
  const sid = await store.create(user.id, { role: user.role, ip: req.ip ?? "", ua: req.get("user-agent") ?? "" });
  res.cookie(COOKIE, sid, cookieOptions(absoluteSec));
}

export async function logout(store: SessionStore, req: Request, res: Response) {
  if (req.sid) await store.destroy(req.sid);
  res.clearCookie(COOKIE, { path: "/" });
}

export const requireSession = (req: Request, res: Response, next: NextFunction) =>
  req.session ? next() : res.status(401).json({ error: "unauthenticated" });
```

Cookie flags:

| Flag | Purpose |
|------|---------|
| `HttpOnly` | JavaScript can't read it, so XSS can't steal it |
| `Secure` | Only sent over HTTPS |
| `SameSite=Lax` (or `Strict`) | Blocks cross-site request forgery for most cases |
| `__Host-` name prefix | Browser enforces `Secure`, `Path=/` and no `Domain`, so a subdomain can't overwrite it |
| `Max-Age` equal to the absolute limit | The server enforces the idle timeout, so the cookie needn't be re-sent each request |

Behind a proxy, set `app.set("trust proxy", ...)` so `secure` cookies and `req.ip` behave correctly.

## Security essentials

### Session fixation

An attacker plants a known session ID in a victim's browser, the victim logs in, and the attacker now holds a logged-in session. The cure is already above: **destroy any existing session and issue a new ID at login** (and after privilege changes such as role elevation, MFA completion or password change).

### CSRF

Cookies are sent automatically, so a malicious site can trigger requests as the user. Layers:

- `SameSite=Lax` or `Strict` cookies (blocks most cross-site writes)
- A **CSRF token** for state-changing requests, stored in the session and sent in a header:

```ts
export function csrf(req: Request, res: Response, next: NextFunction) {
  if (["GET", "HEAD", "OPTIONS"].includes(req.method)) return next();
  if (!req.session || req.get("x-csrf-token") !== req.session.csrf) {
    return res.status(403).json({ error: "csrf" });
  }
  next();
}
```

Return `session.csrf` to your front end from `/api/me`, and have it send the value in `X-CSRF-Token`. Also validate `Origin` on unsafe requests.

### Other basics

- **Never put secrets or large objects in the session.** Keep it to IDs, role and small flags
- **Re-authenticate for sensitive actions** (changing the password, email, payment details)
- **Bind lightly, don't over-bind.** Logging the IP and user agent helps detect theft. Forcing logout on every IP change breaks mobile users, so prefer a **step-up challenge** on big changes
- **Limit concurrent sessions** (`maxPerUser`) if your product needs it
- **Notify users** about new device logins
- **Rate limit** login attempts ([Distributed Rate Limiter](../12_rate-limiting/03_distributed-rate-limiter.md#protecting-login-and-other-abuse-targets))
- **Redis hardening**: TLS, ACL users restricted to the session key pattern, no public exposure ([17_security](../17_security/README.md))

## The quick path: `express-session` with `connect-redis`

If you don't need per-user indexes, the standard packages work with ioredis:

```ts
import session from "express-session";
import { RedisStore } from "connect-redis";            // v7 used a default export instead. Check your version

app.set("trust proxy", 1);

app.use(session({
  store: new RedisStore({ client: redis, prefix: "shop:sess:", ttl: 30 * 60 }),
  name: process.env.NODE_ENV === "production" ? "__Host-sid" : "sid",
  secret: process.env.SESSION_SECRETS!.split(","),     // an array allows secret rotation (first signs, all verify)
  resave: false,                                       // don't re-save unchanged sessions
  saveUninitialized: false,                            // don't create sessions for anonymous visitors
  rolling: true,                                       // refresh the cookie's expiry on each response
  cookie: { httpOnly: true, secure: true, sameSite: "lax", maxAge: 30 * 60 * 1000 },
}));
```

| Strength | Limitation |
|----------|------------|
| Mature, minimal code | Stores the session as one JSON blob under a key containing the **raw session ID** |
| Plugs into the ecosystem (Passport, etc.) | **No per-user index**, so no "log out everywhere" or device list without extra work |
| | Idle expiry only, and an absolute limit needs you to track `createdAt` yourself |
| | `resave`/`saveUninitialized` defaults trip people up |

Use it for straightforward apps. Use the custom store above when you need **device management, global logout, hashed IDs or absolute timeouts**. Check the packages' current documentation, because option names and exports change between major versions.

## Running across many instances

With shared Redis, **instances are stateless**. Any instance can serve any request, so you don't need sticky sessions (only Socket.IO's polling transport needs them, see [Socket.IO with Redis](../09_pub-sub/03_socketio-with-redis.md)).

| Concern | Guidance |
|---------|----------|
| **Concurrent updates** | Two requests editing the session at once collide if you read, modify and rewrite a JSON blob (last write wins). Update **single hash fields** (`HSET`) or keep volatile data (carts, wizards) in its own key |
| **Read-after-write** | Read sessions from the **primary**. A replica can lag, so a user can log in and appear logged out on the next request |
| **Local caching** | Caching sessions in-process for a few seconds saves Redis reads, but delays logout. If you do, broadcast logout over Pub/Sub ([Event-Driven Node.js](../09_pub-sub/02_event-driven-nodejs.md)) and keep the cache TTL tiny |
| **Multi-region** | Pick one: a global session store (cross-region latency), regional stores with users pinned to a region, or accept re-login on region change. Active-active replication of sessions is hard, so avoid it unless you must |
| **Per-request cost** | One pipelined round trip, so keep Redis close and use a short `commandTimeout` |

## When Redis is unavailable

Decide per route:

| Policy | Behavior | Use for |
|--------|----------|---------|
| **Fail closed** | Treat as unauthenticated or return `503` | Almost everything that needs identity |
| **Degrade** | Serve public pages and static content, block account pages | Mixed sites |
| **Short local grace** | Honor a recently verified session from memory for seconds | High-traffic read paths where a brief outage shouldn't log everyone out (accept slightly delayed revocation) |

Never **fail open** into "everyone is logged in". Return `503` with `Retry-After` for authenticated APIs, and let the user stay on the page.

### Durability

Sessions are **recreatable but annoying to lose**: users must log in again.

- AOF with `appendfsync everysec` plus replicas keeps most sessions through a restart or failover
- A failover with asynchronous replication can drop the newest sessions. Accept it, or document it
- Use a **separate Redis (or logical role) for sessions** if cache eviction policies could otherwise evict them. Keep sessions on `noeviction` or `volatile-*` with explicit TTLs ([Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md))

## Testing

```ts
it("rejects a session past its absolute lifetime even if active", async () => {
  const store = new SessionStore(redis, { idleSec: 10, absoluteSec: 1, touchEverySec: 0 });
  const sid = await store.create("u1", { role: "user", ip: "1.1.1.1", ua: "t" });
  expect(await store.get(sid)).not.toBeNull();

  await sleep(1_200);
  expect(await store.get(sid)).toBeNull();
});

it("rotation invalidates the old id", async () => {
  const old = await store.create("u1", { role: "user", ip: "", ua: "" });
  const next = await store.rotate(old);
  expect(await store.get(old)).toBeNull();
  expect(await store.get(next!)).not.toBeNull();
});

it("logs out everywhere except the current session", async () => {
  const a = await store.create("u2", { role: "user", ip: "", ua: "" });
  const b = await store.create("u2", { role: "user", ip: "", ua: "" });
  await store.destroyAllForUser("u2", a);
  expect(await store.get(a)).not.toBeNull();
  expect(await store.get(b)).toBeNull();
});

it("evicts the oldest session beyond maxPerUser", async () => {
  const s = new SessionStore(redis, { idleSec: 60, absoluteSec: 60, maxPerUser: 2 });
  const first = await s.create("u3", { role: "user", ip: "", ua: "" });
  await sleep(5);
  await s.create("u3", { role: "user", ip: "", ua: "" });
  await sleep(5);
  await s.create("u3", { role: "user", ip: "", ua: "" });
  expect(await s.get(first)).toBeNull();
});
```

Use a real Redis, because TTL and script behavior are what you are testing. Also test the middleware with a fake store (stale cookie is cleared, `login` destroys the pre-login session, CSRF rejects a missing header).

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Predictable or short session IDs | `randomBytes(32)` |
| Raw session IDs as Redis keys | Key by `sha256(sid)` |
| Keeping the same ID across login | Destroy and recreate at login and privilege change |
| Cookie without `HttpOnly`, `Secure`, `SameSite` | Set all three, and use the `__Host-` prefix |
| Only a sliding timeout | Add an absolute limit |
| A Redis write on every request | Throttle touches |
| Read-modify-write of a JSON blob | Update single hash fields |
| No per-user index | Add the sorted set, so global logout and device lists work |
| Reading sessions from a lagging replica | Read from the primary |
| Treating a Redis outage as "logged in" | Fail closed with `503` |
| Large objects in the session | Store IDs, keep data elsewhere |
| Sessions sharing an evicting Redis with caches | Separate instance, or a no-eviction policy |
| No CSRF protection on cookie-authenticated writes | `SameSite` plus a CSRF token and Origin checks |

## Key takeaways

- A session is a **random opaque ID** in a cookie, pointing to a **Redis hash**, keyed by the **hash** of the ID
- Use **idle and absolute** expiry, throttle the touch, and **rotate** the ID on login
- A **per-user sorted set** unlocks device lists, global logout and session limits
- Stateless instances plus shared Redis means no sticky sessions, but plan for read-after-write, failover and Redis outages
- `connect-redis` is fine for simple needs, and a custom store gives you control

**Next:** [Token Management](./02_token-management.md)
