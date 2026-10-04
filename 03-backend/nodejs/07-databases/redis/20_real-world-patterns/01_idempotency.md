# Idempotency

An operation is **idempotent** if doing it twice has the same effect as doing it once. Networks are unreliable, so clients retry, load balancers retry and queues redeliver. Without idempotency, a retried "charge the card" request charges twice.

```
Client ──POST /payments (Idempotency-Key: k1)──► API ──► charge card ──► 201
   │                                              ▲
   └── timeout, client retries ──────────────────┘  must NOT charge again, returns the first 201
```

## The problem

| Situation | What goes wrong without idempotency |
|-----------|-------------------------------------|
| Client times out and retries | Duplicate order, double charge |
| User double-clicks "Pay" | Two payments |
| Mobile app resends after reconnect | Duplicate record |
| Queue redelivers a message | Side effect runs twice |
| Load balancer retries on a 502 | The first request actually succeeded |

`GET`, `PUT` and `DELETE` are naturally idempotent by definition. `POST` and "increment" style operations are not, so they need help.

## The idea: idempotency keys

The client sends a unique key (usually a UUID) with each **logical** operation and reuses the same key on every retry of it. The server remembers the outcome per key.

```
POST /payments
Idempotency-Key: 7c9e6679-7425-40de-944b-e07fc1f90ae7
```

Server rules:

| First call | Same key, same request | Same key, different request | Same key, first still running |
|------------|------------------------|-----------------------------|-------------------------------|
| Run the operation, store the result | Return the **stored result**, do not run again | Reject with `422` (key reuse with a different body) | Reject with `409` and `Retry-After` (or wait) |

## Design

### State machine

```
            begin (SET if absent)                    complete
  (none) ─────────────────────────► in_progress ──────────────────► completed (stored response, TTL)
                                         │
                                         │ handler failed before any effect
                                         ▼
                                      released (key deleted, client may retry)
```

### Keys and data

```
idem:{scope}:{key}   HASH
   status    "in_progress" | "completed"
   fp        sha256 of method + path + canonical body   (detects key reuse)
   token     owner token of the request that holds the lock
   response  JSON {status, body}   (only when completed)
   TTL       short while in progress (lock), long once completed (replay window)
```

- **Scope** the key by user or tenant (`idem:{userId}:{key}`) so one customer can never read or collide with another's
- Keep the completed TTL longer than the longest realistic retry window. 24 hours is a common choice
- Keep the in-progress TTL longer than your slowest handler, but short enough that a crashed request does not block retries for long

## Implementation

Atomic transitions need Lua so that "check state, then write" cannot interleave. See [Lua Scripts](../06_advanced-commands/03_lua-scripts.md).

```ts
import Redis from "ioredis";
import { createHash, randomUUID } from "node:crypto";

// KEYS[1] = idem key | ARGV[1] = fingerprint, ARGV[2] = lock TTL (s), ARGV[3] = owner token
const BEGIN = `
local status = redis.call("HGET", KEYS[1], "status")
if not status then
  redis.call("HSET", KEYS[1], "status", "in_progress", "fp", ARGV[1], "token", ARGV[3])
  redis.call("EXPIRE", KEYS[1], ARGV[2])
  return {"started"}
end
if redis.call("HGET", KEYS[1], "fp") ~= ARGV[1] then return {"mismatch"} end
if status == "completed" then
  return {"completed", redis.call("HGET", KEYS[1], "response")}
end
return {"in_progress"}
`;

// ARGV[1] = owner token, ARGV[2] = response JSON, ARGV[3] = result TTL (s)
const COMPLETE = `
if redis.call("HGET", KEYS[1], "token") ~= ARGV[1] then return 0 end
redis.call("HSET", KEYS[1], "status", "completed", "response", ARGV[2])
redis.call("HDEL", KEYS[1], "token")
redis.call("EXPIRE", KEYS[1], ARGV[3])
return 1
`;

// ARGV[1] = owner token. Releases the key only if we still own it.
const ABORT = `
if redis.call("HGET", KEYS[1], "token") == ARGV[1] then return redis.call("DEL", KEYS[1]) end
return 0
`;

export class IdempotencyError extends Error {
  constructor(public status: 409 | 422, message: string) { super(message); }
}

export interface StoredResponse { status: number; body: unknown }

type Scripted = Redis & {
  idemBegin(key: string, fp: string, lockTtl: number, token: string): Promise<[string, string?]>;
  idemComplete(key: string, token: string, response: string, ttl: number): Promise<number>;
  idemAbort(key: string, token: string): Promise<number>;
};

export class IdempotencyStore {
  private r: Scripted;

  constructor(redis: Redis, private lockTtlSec = 60, private resultTtlSec = 86_400) {
    redis.defineCommand("idemBegin", { numberOfKeys: 1, lua: BEGIN });
    redis.defineCommand("idemComplete", { numberOfKeys: 1, lua: COMPLETE });
    redis.defineCommand("idemAbort", { numberOfKeys: 1, lua: ABORT });
    this.r = redis as Scripted;
  }

  fingerprint(...parts: string[]) {
    return createHash("sha256").update(parts.join("\n")).digest("hex");
  }

  async run(
    scope: string,
    idemKey: string,
    fingerprint: string,
    work: () => Promise<StoredResponse>,
  ): Promise<{ replayed: boolean; result: StoredResponse }> {
    const key = `idem:${scope}:${idemKey}`;
    const token = randomUUID();

    const [state, payload] = await this.r.idemBegin(key, fingerprint, this.lockTtlSec, token);

    if (state === "completed") return { replayed: true, result: JSON.parse(payload!) };
    if (state === "mismatch") throw new IdempotencyError(422, "Idempotency-Key was reused with a different request");
    if (state === "in_progress") throw new IdempotencyError(409, "A request with this key is still being processed");

    try {
      const result = await work();
      const ok = await this.r.idemComplete(key, token, JSON.stringify(result), this.resultTtlSec);
      if (!ok) console.warn("idempotency lock expired before completion", { key }); // handler outlived the lock TTL
      return { replayed: false, result };
    } catch (err) {
      await this.r.idemAbort(key, token);   // nothing was recorded: let the client retry
      throw err;
    }
  }
}
```

### Using it in Express

```ts
const store = new IdempotencyStore(redis);
const KEY_FORMAT = /^[A-Za-z0-9_-]{8,64}$/;

app.post("/payments", async (req, res, next) => {
  const idemKey = req.header("Idempotency-Key");
  if (!idemKey || !KEY_FORMAT.test(idemKey)) {
    return res.status(400).json({ error: "Idempotency-Key header (8 to 64 chars) is required" });
  }

  const fp = store.fingerprint(req.method, req.path, JSON.stringify(req.body));

  try {
    const { replayed, result } = await store.run(req.user.id, idemKey, fp, async () => {
      const payment = await chargeCard(req.body, { idempotencyKey: idemKey });  // pass the key downstream too
      return { status: 201, body: payment };
    });

    if (replayed) res.set("Idempotent-Replayed", "true");
    res.status(result.status).json(result.body);
  } catch (err) {
    if (err instanceof IdempotencyError) {
      if (err.status === 409) res.set("Retry-After", "1");
      return res.status(err.status).json({ error: err.message });
    }
    next(err);
  }
});
```

Points to notice:

- The **client** generates the key. A server-generated key cannot help with retries of the very first request
- `JSON.stringify(req.body)` depends on property order. For strict fingerprints, canonicalize (sort keys) before hashing
- Business errors that are deterministic (validation failed, card declined) should be returned as a `StoredResponse` with a `4xx` status, so a retry gets the same answer. Let only **unexpected** errors throw (they release the key)

## The part Redis cannot solve alone

Consider this crash:

```
begin ─► charge card (success) ─► ⚡ process dies ─► complete never recorded
```

The lock expires, the client retries, `BEGIN` succeeds again and the card is charged **twice**. Redis tracked the request but not the real-world effect. Close the gap with one or more of:

| Defense | How |
|---------|-----|
| **Pass the key downstream** | Payment providers accept their own idempotency key. Reuse the same value (as in the example) |
| **Database unique constraint** | `INSERT ... ON CONFLICT (idempotency_key) DO NOTHING`, so the source of truth refuses duplicates |
| **Record the effect and the key in one transaction** | The DB row and the idempotency record commit together |
| **Reconcile** | A periodic job compares in-progress records with downstream state |

Redis gives you a fast first line of defense and a place to cache the response. For money, inventory or anything irreversible, the durable store must also be safe against repeats.

## Variations

### Message consumers

A queue delivers at least once, so consumers need the same protection. Use the message ID as the key, and process only if you win the claim. The simplified form is in [Request Deduplication](./02_request-deduplication.md), and the full consumer wrapper in [Event-Driven System](./06_event-driven-system.md).

### Naturally idempotent designs

Prefer operations that are safe by construction:

| Instead of | Prefer |
|------------|--------|
| "Add 1 to balance" | "Set balance to X for version N" or "Apply transaction T" (ledger with unique transaction IDs) |
| "Create an order" | "Put order with client-generated ID `o-123`" |
| "Send email" | "Send email for notification `n-42`" (record sent state, check first) |

### Waiting instead of 409

For short operations, the second caller can poll briefly for completion instead of failing:

```ts
async function waitForResult(key: string, timeoutMs = 3000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const [status, response] = await redis.hmget(key, "status", "response");
    if (status === "completed") return JSON.parse(response!);
    await new Promise((r) => setTimeout(r, 100));
  }
  return null;   // still running: answer 409
}
```

## Edge cases

| Case | Behavior | Notes |
|------|----------|-------|
| Same key, same body, after completion | Stored response replayed | Add `Idempotent-Replayed: true` |
| Same key, different body | `422` | Prevents accidental key reuse |
| Same key while first is running | `409` and `Retry-After` (or wait) | Never run twice in parallel |
| First request crashes | Lock expires, retry can proceed | See "The part Redis cannot solve alone" |
| Handler slower than lock TTL | A second request may start | Size the TTL generously or renew it |
| Redis unavailable | Decide: fail closed (`503`) for payments, fail open for harmless operations | A silent bypass defeats the purpose |
| Failover loses recent keys | Replays possible right after failover | Use AOF, replicas and `WAIT` for important keys, and keep the DB constraint |
| Large responses | Memory use | Store a reference or the essential fields, and cap size |
| Sensitive data in responses | Stored in Redis for 24h | Store minimal fields, secure Redis (see [Security](../17_security/README.md)) |

## Choosing TTLs

| TTL | Value | Guidance |
|-----|-------|----------|
| In progress (lock) | 30 to 120 s | Longer than the 99.9th percentile handler time |
| Completed (replay window) | 24 h | Longer than the longest client retry policy |

## Testing

```ts
it("runs the work once for concurrent duplicates", async () => {
  let calls = 0;
  const work = async () => { calls++; await sleep(50); return { status: 201, body: { id: 1 } }; };
  const fp = store.fingerprint("POST", "/payments", "{}");

  const results = await Promise.allSettled(
    Array.from({ length: 20 }, () => store.run("u1", "key-12345678", fp, work)),
  );

  expect(calls).toBe(1);
  const ok = results.filter((r) => r.status === "fulfilled");
  const conflicts = results.filter((r) => r.status === "rejected");
  expect(ok.length + conflicts.length).toBe(20);
});

it("replays the stored response", async () => {
  const fp = store.fingerprint("POST", "/p", "{}");
  await store.run("u1", "key-aaaaaaaa", fp, async () => ({ status: 201, body: { n: 1 } }));
  const again = await store.run("u1", "key-aaaaaaaa", fp, async () => ({ status: 201, body: { n: 2 } }));
  expect(again.replayed).toBe(true);
  expect(again.result.body).toEqual({ n: 1 });
});

it("rejects key reuse with a different request", async () => {
  await store.run("u1", "key-bbbbbbbb", "fp-1", async () => ({ status: 200, body: {} }));
  await expect(store.run("u1", "key-bbbbbbbb", "fp-2", async () => ({ status: 200, body: {} })))
    .rejects.toMatchObject({ status: 422 });
});

it("releases the key when the handler throws", async () => {
  await expect(store.run("u1", "key-cccccccc", "fp", async () => { throw new Error("boom"); })).rejects.toThrow();
  const retry = await store.run("u1", "key-cccccccc", "fp", async () => ({ status: 200, body: {} }));
  expect(retry.replayed).toBe(false);
});
```

Also test expiry of the in-progress lock with a very short TTL, and a simulated crash (take the lock, never complete, wait for expiry, then run again). Helpers are in [Testing with Redis](../18_testing-and-debugging/01_testing-with-redis.md).

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `SET key NX` then a separate write for the result | Use one Lua script so transitions are atomic |
| No fingerprint check | Detect key reuse with a different body (`422`) |
| Key not scoped by user or tenant | Prefix with the caller's ID |
| Server generates the key | The client must generate it, once per logical operation |
| Lock TTL shorter than the handler | Size it generously, or renew |
| Storing failures from crashes as final | Release on unexpected errors, store only deterministic outcomes |
| Believing Redis alone prevents double charges | Add a downstream key and a DB unique constraint |
| Fail-open on Redis errors for payments | Fail closed with `503` |
| Releasing without checking the owner token | Compare-and-delete, as in `ABORT` |
| No TTL on completed records | Always expire, or memory grows forever |

## Key takeaways

- The client sends one key per logical operation and reuses it on retries
- Store `in_progress` and `completed` states atomically with Lua, scoped per caller and guarded by a request fingerprint
- Replay the stored response for repeats and reject key reuse with a different request
- A crash between the side effect and the record is the real danger. Defend with downstream idempotency keys and database constraints
- Choose TTLs deliberately and decide how to behave when Redis is unavailable
- Test with parallel duplicates, key reuse, handler failure and lock expiry

**Previous:** [Real-World Patterns README](./README.md) | **Next:** [Request Deduplication](./02_request-deduplication.md)
