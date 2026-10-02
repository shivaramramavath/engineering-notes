# Token Management

Sessions suit browsers talking to your own backend. **Tokens** suit APIs, mobile apps and multiple services. The modern pattern pairs a **short-lived JWT access token** (cheap to verify) with a **long-lived, rotating refresh token** (stored in Redis, so it can be revoked).

This lesson also covers **single-use tokens** (password reset, email verification) and **OTP codes**, which Redis handles well thanks to TTLs and atomic commands.

## The architecture

```
login ─► access token  (JWT, ~10 minutes, stateless)
      └► refresh token (opaque, ~30 days, state in Redis, ROTATES on every use)

API call:       Authorization: Bearer <access>      → verify signature (+ cheap revocation check)
access expired: POST /auth/refresh (refresh token)  → new access + NEW refresh token (the old one dies)
logout:         revoke the refresh "family" in Redis, denylist the access token until it expires
```

Why the split:

| Token | Lifetime | Where state lives | Revocable? |
|-------|----------|-------------------|------------|
| **Access** (JWT) | Minutes | In the token itself | Only via a denylist or version check |
| **Refresh** (opaque) | Days to weeks | **Redis** | **Yes, immediately** |

A stolen access token is only useful for minutes. A stolen refresh token is the dangerous one, which is why it rotates and why reuse is detected.

## Keys to add to your registry

```ts
// src/redis/keys.ts (additions)
refreshFamily:   (id: string)  => K.key("rt", "fam", id),
userFamilies:    (userId: Id)  => K.key("user", userId, "families"),
denyJti:         (jti: string) => K.key("deny", "jti", jti),
denyFamily:      (id: string)  => K.key("deny", "fam", id),
userTokenVersion:(userId: Id)  => K.key("user", userId, "tv"),
oneTimeToken:    (purpose: string, hash: string) => K.key("ott", purpose, hash),
otp:             (id: string)  => K.key("otp", id),
```

## Access tokens (JWT)

Use a maintained library such as [`jose`](https://github.com/panva/jose). Pin the algorithm and validate the issuer and audience:

```ts
import { SignJWT, jwtVerify } from "jose";
import { randomUUID } from "node:crypto";

const secret = new TextEncoder().encode(process.env.JWT_SECRET!);   // 32+ random bytes for HS256
const ISS = "https://api.example.com";
const AUD = "shop-api";

export async function signAccess(userId: string, familyId: string, tokenVersion: number, ttlSec = 600) {
  return new SignJWT({ sid: familyId, tv: tokenVersion })
    .setProtectedHeader({ alg: "HS256", kid: "k1" })
    .setSubject(userId)
    .setJti(randomUUID())
    .setIssuer(ISS)
    .setAudience(AUD)
    .setIssuedAt()
    .setExpirationTime(`${ttlSec}s`)
    .sign(secret);
}

export async function verifyAccessSignature(token: string) {
  const { payload } = await jwtVerify(token, secret, {
    issuer: ISS,
    audience: AUD,
    algorithms: ["HS256"],                                          // never accept "none" or an unexpected algorithm
  });
  return payload;
}
```

| Rule | Why |
|------|-----|
| **Short expiry** (5 to 15 minutes) | Limits what a stolen token can do |
| `algorithms: [...]` pinned | Prevents algorithm-confusion attacks |
| Validate `iss` and `aud` | Tokens meant for another service are rejected |
| `jti` (unique ID) | Lets you denylist one token |
| `sid` (family ID) and `tv` (token version) claims | Cheap hooks for revocation (below) |
| Asymmetric keys (RS256 or EdDSA) when other services verify tokens | They hold only the **public** key |
| A `kid` header and key rotation plan | You can rotate signing keys without breaking live tokens |

Never put sensitive data in a JWT. It is **signed, not encrypted**, so anyone can read the payload.

## Revoking access tokens

A JWT is valid until `exp`, unless you check something. Options:

| Approach | How | Revocation speed | Cost per request |
|----------|-----|------------------|------------------|
| **Do nothing** | Rely on short expiry | Up to the TTL (minutes) | None (fully stateless) |
| **Denylist by `jti`** | Redis key per revoked token, TTL = remaining life | Immediate | One `EXISTS` |
| **Denylist by family (`sid`)** | One key kills every access token from a login | Immediate | One `EXISTS` |
| **Token version per user (`tv`)** | `INCR` a counter, reject tokens with an older `tv` | Immediate, **all** user tokens | One `GET` |
| **Session-bound tokens** | Look up the session on every request | Immediate | A full lookup (at which point plain sessions are simpler) |

A pragmatic combination checks all three cheap signals in **one round trip**:

```ts
export async function verifyAccess(redis: Redis, token: string) {
  const p = await verifyAccessSignature(token);                      // throws if invalid or expired
  const userId = p.sub!;

  const [famDenied, jtiDenied, currentTv] = await Promise.all([     // auto pipelining batches these
    redis.exists(keys.denyFamily(p.sid as string)),
    redis.exists(keys.denyJti(p.jti!)),
    redis.get(keys.userTokenVersion(userId)),
  ]);

  if (famDenied || jtiDenied || Number(currentTv ?? 0) > Number(p.tv ?? 0)) {
    throw new Unauthorized("revoked");
  }
  return { userId, familyId: p.sid as string, jti: p.jti! };
}
```

Key facts:

- Denylist entries **expire with the token**: `SET deny:jti:<id> 1 EX <remaining seconds>`. The list never grows unboundedly, and holds only tokens that are both revoked and unexpired
- Incrementing `tv` (`INCR user:42:tv`) revokes **every access token the user holds**, ideal for password changes
- Decide the failure policy if Redis is down: for sensitive APIs **fail closed** (`503`), for low-risk reads you may accept the signature alone with an alert (a conscious trade, written down)

## Refresh tokens: opaque, hashed, rotating

A refresh token is **not a JWT**. It is a random secret whose hash lives in Redis, so it can be revoked, rotated and tracked.

### Format and storage

```
refresh token (sent to the client):   <familyId>.<secret>          e.g.  q8Zk3...f2A.Vb91...Xw
Redis (shop:rt:fam:<familyId>):       hash { userId, cur: sha256(secret), prev, prevAt, revoked, createdAt, ip, ua }
                                      TTL = absolute refresh lifetime
```

- The **family** represents one login on one device. All tokens descended from it share the `familyId`
- Only the **hash** of the secret is stored, so a Redis leak doesn't reveal usable refresh tokens
- `cur` is the one currently valid token. `prev` is the one it replaced

### Rotation with reuse detection

Every refresh **replaces** the token. Each token is therefore single-use. If an **already-used** token appears, either the legitimate client or a thief is replaying it, and you can't tell which, so you **revoke the whole family** and force a new login.

```
login:       cur = A
refresh(A):  cur = B, prev = A        → client now holds B
refresh(B):  cur = C, prev = B
refresh(A):  A is neither cur nor prev-within-grace → REUSE → family revoked (C is dead too)
```

Legitimate retries (a dropped response, two browser tabs refreshing at once) must not trigger a revocation, so the previous token gets a short **grace window**.

The check-and-rotate must be **atomic**, otherwise two concurrent refreshes with the same token both succeed. That calls for a Lua script:

```lua
-- refresh-rotate.lua
-- KEYS[1] = family hash
-- ARGV[1] = sha256 of the presented secret, ARGV[2] = sha256 of the NEW secret, ARGV[3] = grace ms
-- returns {1, "rotated"} on success, otherwise {0, reason}
local t = redis.call("TIME")
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

local cur = redis.call("HGET", KEYS[1], "cur")
if not cur then return {0, "unknown"} end                                -- expired or never existed

if redis.call("HGET", KEYS[1], "revoked") == "1" then return {0, "revoked"} end

if cur == ARGV[1] then                                                   -- the normal case
  redis.call("HSET", KEYS[1], "prev", cur, "prevAt", now, "cur", ARGV[2])
  return {1, "rotated"}
end

local prev = redis.call("HGET", KEYS[1], "prev")
if prev and prev == ARGV[1] then
  local prevAt = tonumber(redis.call("HGET", KEYS[1], "prevAt") or "0")
  if now - prevAt <= tonumber(ARGV[3]) then
    return {0, "grace"}                                                  -- a benign retry: deny, but don't revoke
  end
end

redis.call("HSET", KEYS[1], "revoked", "1")                              -- an old token came back: assume theft
return {0, "reuse"}
```

Notes:

- The family keeps its **absolute TTL**, set at login and **not extended** by rotation. That gives refresh tokens an absolute lifetime (for a sliding lifetime, add an `EXPIRE` to the success branch)
- A revoked family record **stays until its TTL**, so replays of stolen tokens are still recognized as reuse
- On `"grace"` the caller gets a `401` and should retry with the **new** token it already received, or re-login. The family is **not** revoked

### The token service

```ts
// src/auth/token-service.ts
import { randomBytes, createHash } from "node:crypto";
import type { Redis } from "ioredis";
import { keys } from "../redis/keys.js";
import { signAccess } from "./jwt.js";

declare module "ioredis" {
  interface RedisCommander<Context> {
    refreshRotate(familyKey: string, presentedHash: string, newHash: string, graceMs: number): Promise<[number, string]>;
  }
}

const sha256 = (s: string) => createHash("sha256").update(s).digest("hex");
const rand = (bytes: number) => randomBytes(bytes).toString("base64url");

export class Unauthorized extends Error {}

export interface TokenOptions {
  accessTtlSec: number;          // 600
  refreshAbsoluteSec: number;    // 30 * 86_400
  graceMs: number;               // 10_000
}

export class TokenService {
  constructor(private redis: Redis, private o: TokenOptions, private log = console) {}

  async login(userId: string, meta: { ip: string; ua: string }) {
    const familyId = rand(16);
    const secret = rand(32);
    const key = keys.refreshFamily(familyId);

    await this.redis.multi()
      .hset(key, { userId, cur: sha256(secret), prev: "", prevAt: 0, revoked: 0,
                   createdAt: Date.now(), ip: meta.ip, ua: meta.ua.slice(0, 200) })
      .expire(key, this.o.refreshAbsoluteSec)
      .sadd(keys.userFamilies(userId), familyId)
      .expire(keys.userFamilies(userId), this.o.refreshAbsoluteSec)
      .exec();

    return { access: await this.access(userId, familyId), refresh: `${familyId}.${secret}` };
  }

  async refresh(refreshToken: string) {
    const parts = refreshToken.split(".");
    if (parts.length !== 2 || !parts[0] || !parts[1]) throw new Unauthorized("malformed");
    const [familyId, secret] = parts as [string, string];

    const newSecret = rand(32);
    const [ok, reason] = await this.redis.refreshRotate(
      keys.refreshFamily(familyId), sha256(secret), sha256(newSecret), this.o.graceMs
    );

    if (ok !== 1) {
      if (reason === "reuse") {
        await this.denyFamily(familyId);                     // kill outstanding access tokens too
        this.log.warn({ familyId }, "refresh token reuse detected, family revoked");
      }
      throw new Unauthorized(reason);
    }

    const userId = (await this.redis.hget(keys.refreshFamily(familyId), "userId"))!;
    return { access: await this.access(userId, familyId), refresh: `${familyId}.${newSecret}` };
  }

  /** Log out one device. */
  async logout(familyId: string, accessJti?: string, accessExp?: number) {
    await this.revokeFamily(familyId);
    if (accessJti && accessExp) {
      const remaining = accessExp - Math.floor(Date.now() / 1000);
      if (remaining > 0) await this.redis.set(keys.denyJti(accessJti), "1", "EX", remaining);
    }
  }

  /** Log out everywhere: password change, account recovery, admin action. */
  async logoutEverywhere(userId: string) {
    await this.redis.incr(keys.userTokenVersion(userId));           // kills every access token at once
    const families = await this.redis.smembers(keys.userFamilies(userId));
    await Promise.all(families.map((f) => this.revokeFamily(f)));
  }

  // ---- internals ----
  private async access(userId: string, familyId: string) {
    const tv = Number((await this.redis.get(keys.userTokenVersion(userId))) ?? 0);
    return signAccess(userId, familyId, tv, this.o.accessTtlSec);
  }

  private async revokeFamily(familyId: string) {
    await this.redis.multi()
      .hset(keys.refreshFamily(familyId), "revoked", "1")           // keep the record so reuse stays detectable
      .set(keys.denyFamily(familyId), "1", "EX", this.o.accessTtlSec)
      .exec();
  }

  private denyFamily(familyId: string) {
    return this.redis.set(keys.denyFamily(familyId), "1", "EX", this.o.accessTtlSec);
  }
}
```

`revokeFamily` plus the `deny:fam` key (with TTL equal to the access-token lifetime) ends **both** token types for that login.

### HTTP endpoints

```ts
const REFRESH_COOKIE = "__Secure-rt";

const refreshCookie = {
  httpOnly: true,
  secure: true,
  sameSite: "strict" as const,
  path: "/auth",                                        // sent only to auth endpoints
  maxAge: 30 * 86_400 * 1000,
};

app.post("/auth/login", loginLimiters, async (req, res) => {
  const user = await verifyCredentials(req.body.email, req.body.password);   // your code
  const { access, refresh } = await tokens.login(user.id, { ip: req.ip ?? "", ua: req.get("user-agent") ?? "" });
  res.cookie(REFRESH_COOKIE, refresh, refreshCookie).json({ accessToken: access });
});

app.post("/auth/refresh", requireCsrfHeader, async (req, res) => {
  const presented = req.cookies?.[REFRESH_COOKIE];
  if (!presented) return res.status(401).json({ error: "no_refresh_token" });

  try {
    const { access, refresh } = await tokens.refresh(presented);
    res.cookie(REFRESH_COOKIE, refresh, refreshCookie).json({ accessToken: access });
  } catch (err) {
    if (err instanceof Unauthorized) {
      res.clearCookie(REFRESH_COOKIE, { path: "/auth" });
      return res.status(401).json({ error: "reauthenticate" });
    }
    throw err;
  }
});

app.post("/auth/logout", authenticateAccess, async (req, res) => {
  await tokens.logout(req.auth.familyId, req.auth.jti, req.auth.exp);
  res.clearCookie(REFRESH_COOKIE, { path: "/auth" }).status(204).end();
});
```

Where each token lives on the client:

| Token | Browser | Mobile |
|-------|---------|--------|
| **Access** | In **memory** (a JS variable), sent as `Authorization: Bearer` | In memory |
| **Refresh** | **HttpOnly, Secure, SameSite=Strict cookie** scoped to `/auth` | The OS secure store (Keychain, Keystore), sent in the request body |

Avoid `localStorage` for refresh tokens, since any XSS can read it. Because the refresh token is in a cookie, protect `/auth/refresh` against CSRF with `SameSite=Strict` plus a required custom header (`X-Requested-With`) or token.

### Client behavior that avoids false reuse alarms

Browser tabs refreshing at the same moment is the most common trigger of grace-window denials. The client should have **one in-flight refresh at a time**:

```ts
let inflight: Promise<string> | null = null;

export function getFreshAccessToken(): Promise<string> {
  return (inflight ??= fetch("/auth/refresh", { method: "POST", credentials: "include" })
    .then((r) => (r.ok ? r.json() : Promise.reject(new Error("reauth"))))
    .then((b) => b.accessToken)
    .finally(() => { inflight = null; }));
}
```

All concurrent callers share one refresh. Across **browser tabs**, a `BroadcastChannel` or the Web Locks API can coordinate the same way.

## Single-use tokens: password reset and email verification

Redis fits well: a TTL gives expiry, and `GETDEL` gives **atomic single use** (Redis 6.2+).

```ts
// src/auth/one-time-tokens.ts
export class OneTimeTokens {
  constructor(private redis: Redis) {}

  /** Returns the raw token to email to the user. Only its hash is stored. */
  async issue(purpose: "reset" | "verify-email", userId: string, ttlSec: number): Promise<string> {
    const token = rand(32);
    const hash = sha256(token);
    const latestKey = keys.oneTimeToken(`${purpose}:latest`, userId);

    // invalidate any earlier outstanding token of this kind for the user
    const previous = await this.redis.get(latestKey);
    const m = this.redis.multi();
    if (previous) m.unlink(keys.oneTimeToken(purpose, previous));
    m.set(keys.oneTimeToken(purpose, hash), userId, "EX", ttlSec);
    m.set(latestKey, hash, "EX", ttlSec);
    await m.exec();

    return token;
  }

  /** Atomic single use: the first caller gets the user, everyone else gets null. */
  async consume(purpose: "reset" | "verify-email", token: string): Promise<string | null> {
    return this.redis.getdel(keys.oneTimeToken(purpose, sha256(token)));
  }
}
```

```ts
// request a reset: always respond the same way, whether or not the account exists
app.post("/auth/forgot", forgotLimiter, async (req, res) => {
  const user = await users.findByEmail(req.body.email);
  if (user) {
    const token = await ott.issue("reset", user.id, 3600);
    await mail.send(user.email, `${APP_URL}/reset?token=${token}`);
  }
  res.status(202).json({ ok: true });
});

// complete the reset
app.post("/auth/reset", resetLimiter, async (req, res) => {
  const userId = await ott.consume("reset", req.body.token);
  if (!userId) return res.status(400).json({ error: "invalid_or_expired" });

  await users.setPassword(userId, req.body.password);
  await tokens.logoutEverywhere(userId);                          // end every session and token after a reset
  res.json({ ok: true });
});
```

Principles:

- The token is **high-entropy and random**, so a fast hash lookup is safe, and there's nothing to compare in constant time
- **Only the hash is stored**, so a Redis leak doesn't expose live reset links
- **Short TTL** (15 to 60 minutes) and **single use** (`GETDEL`)
- **Same response** for known and unknown emails, to avoid account enumeration
- **Rate limit** issuing and consuming ([Distributed Rate Limiter](../12_rate-limiting/03_distributed-rate-limiter.md))
- After a reset or email change, **revoke sessions and tokens**
- Put tokens in the URL **fragment or POST body** where possible, and make sure pages that receive them send no `Referer` to third parties

## OTP codes with attempt limits

Short numeric codes are brute-forceable, so the **attempt counter** is the real protection:

```lua
-- otp-verify.lua
-- KEYS[1] = otp hash {c: code, a: attempts}
-- ARGV[1] = submitted code, ARGV[2] = max attempts
-- returns 1 = success, 0 = wrong code, -1 = no code or too many attempts (code destroyed)
local code = redis.call("HGET", KEYS[1], "c")
if not code then return -1 end

local attempts = redis.call("HINCRBY", KEYS[1], "a", 1)
if attempts > tonumber(ARGV[2]) then
  redis.call("DEL", KEYS[1])
  return -1
end

if code == ARGV[1] then
  redis.call("DEL", KEYS[1])                         -- single use
  return 1
end
return 0
```

```ts
import { randomInt } from "node:crypto";

declare module "ioredis" {
  interface RedisCommander<Context> {
    otpVerify(key: string, code: string, maxAttempts: number): Promise<number>;
  }
}

export async function issueOtp(redis: Redis, subject: string, ttlSec = 300) {
  const code = randomInt(0, 1_000_000).toString().padStart(6, "0");           // CSPRNG, not Math.random
  const key = keys.otp(subject);
  await redis.multi().del(key).hset(key, { c: code, a: 0 }).expire(key, ttlSec).exec();   // replaces any earlier code
  return code;                                                                // send by SMS or email
}

export const verifyOtp = (redis: Redis, subject: string, code: string) =>
  redis.otpVerify(keys.otp(subject), code, 5);                                // 1 ok, 0 wrong, -1 dead
```

| Control | Why |
|---------|-----|
| **5 attempts**, then the code is destroyed | A 6-digit code has only a million values |
| **5-minute TTL** | A short window |
| New code **replaces** the old one | Only one valid code at a time |
| **Rate limit sending** per phone, email and IP | Prevents SMS-pumping cost abuse and harassment |
| Generate with `crypto.randomInt` | Unbiased and unpredictable |

Because the attempt counter is incremented **before** the comparison, in one atomic script, parallel guesses can't beat the limit.

## API keys (brief)

Long-lived credentials for programmatic access follow the same rules:

- Format `sk_live_<id>_<secret>`, shown **once** at creation
- Store `{ keyId → sha256(secret), userId, scopes, createdAt, lastUsedAt }`, never the secret
- Look up by `keyId`, compare the hash with `crypto.timingSafeEqual`
- Rate limit **per key** ([Rate Limiting](../12_rate-limiting/README.md)), and support revocation by deleting the record
- Cache lookups in memory for a few seconds if volume demands it, accepting delayed revocation

## When Redis fails

| Component | If Redis is unreachable or loses data |
|-----------|---------------------------------------|
| Access token verification | Signature still works. Revocation checks fail: **fail closed** for sensitive APIs, or accept the delay knowingly |
| Refresh | Fails. Clients can't renew, and users re-login when their access token expires |
| Data loss (no persistence, or a lost failover write) | Refresh families vanish → everyone is logged out. A **lost rotation** can make a legitimate client look like reuse. A **lost revocation** can resurrect a revoked family |
| One-time tokens and OTPs | Lost → users request new ones (harmless) |

Mitigate with AOF (`everysec`), replicas, and a persistence-friendly (non-evicting) Redis for auth state ([Persistence](../02_redis-fundamentals/04_persistence.md)). Make the **client** resilient: on a `401` from refresh, send the user to login instead of looping.

## Observability and audit

Record security events to a **Stream** (so they are durable, replayable and consumable by alerting) ([Streams](../10_streams/README.md)):

```ts
await redis.xadd("shop:stream:security", "MAXLEN", "~", 100_000, "*",
  "type", "refresh.reuse", "userId", userId, "familyId", familyId, "ip", ip, "ts", String(Date.now()));
```

Alert on: reuse detections, spikes in `401` from refresh, logins from new countries or devices, many failed OTPs, bursts of reset requests. Never log tokens, secrets or full codes.

## Testing

```ts
it("rotates and rejects the old token after the grace window", async () => {
  const tokens = new TokenService(redis, { accessTtlSec: 60, refreshAbsoluteSec: 3600, graceMs: 50 });
  const { refresh: a } = await tokens.login("u1", { ip: "", ua: "" });

  const { refresh: b } = await tokens.refresh(a);
  await sleep(100);                                              // past the grace window

  await expect(tokens.refresh(a)).rejects.toThrow("reuse");      // replay of the old token
  await expect(tokens.refresh(b)).rejects.toThrow("revoked");    // the whole family is now dead
});

it("two concurrent refreshes with the same token: one rotates, the other is denied without revoking", async () => {
  const tokens = new TokenService(redis, { accessTtlSec: 60, refreshAbsoluteSec: 3600, graceMs: 5_000 });
  const { refresh } = await tokens.login("u2", { ip: "", ua: "" });

  const results = await Promise.allSettled([tokens.refresh(refresh), tokens.refresh(refresh)]);
  expect(results.map((r) => r.status).sort()).toEqual(["fulfilled", "rejected"]);

  const winner = results.find((r) => r.status === "fulfilled") as PromiseFulfilledResult<{ refresh: string }>;
  await expect(tokens.refresh(winner.value.refresh)).resolves.toBeDefined();   // the family survived
});

it("logoutEverywhere invalidates existing access tokens immediately", async () => {
  const { access } = await tokens.login("u3", { ip: "", ua: "" });
  await expect(verifyAccess(redis, access)).resolves.toBeDefined();
  await tokens.logoutEverywhere("u3");
  await expect(verifyAccess(redis, access)).rejects.toThrow("revoked");
});

it("a reset token works exactly once", async () => {
  const t = await ott.issue("reset", "u4", 60);
  expect(await ott.consume("reset", t)).toBe("u4");
  expect(await ott.consume("reset", t)).toBeNull();
});

it("an OTP dies after too many wrong guesses, even a correct one", async () => {
  const code = await issueOtp(redis, "phone:1");
  for (let i = 0; i < 5; i++) await verifyOtp(redis, "phone:1", "000000");
  expect(await verifyOtp(redis, "phone:1", code)).toBe(-1);
});
```

Run these against a real Redis. Also test: expired access token, wrong `aud`, tampered signature, wrong algorithm header, and denylist TTL matching the token's remaining lifetime.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Long-lived access tokens | 5 to 15 minutes, with refresh |
| Refresh token as a JWT with no server state | Opaque, hashed, stored in Redis |
| Storing raw refresh tokens, reset tokens or API keys | Store only SHA-256 hashes |
| No rotation, or rotation without reuse detection | Single-use refresh tokens and family revocation |
| Non-atomic rotate (read, compare, write) | The Lua script |
| No grace window, so tab races log people out | A short grace window plus client single-flight refresh |
| Accepting any JWT algorithm | Pin `algorithms`, validate `iss` and `aud` |
| Refresh tokens in `localStorage` | HttpOnly cookie or the platform secure store |
| Denylist entries with no TTL | TTL equal to the token's remaining lifetime |
| Reset tokens that survive use or reissue | `GETDEL`, and invalidate earlier tokens |
| 6-digit OTP without an attempt limit | Atomic attempt counter and destruction |
| Revoking only the refresh token on password change | Also bump the token version and revoke all families |
| Login, refresh, reset and OTP endpoints unrated | Rate limit all of them |

## Key takeaways

- **Short-lived JWT access tokens** plus **rotating, hashed, opaque refresh tokens** in Redis give you cheap verification and real revocation
- **Reuse detection** (with a small grace window) turns refresh-token theft into a forced re-login, and the rotation must be **atomic in Lua**
- Revoke access tokens with a **TTL'd denylist** or **token versioning**, and decide your failure policy for Redis outages
- `GETDEL` makes single-use tokens trivial, and atomic attempt counters make OTPs safe
- Hash every secret at rest, rate limit every auth endpoint, and keep an audit trail

**Next module:** [14_queues-and-workers](../14_queues-and-workers/README.md)
