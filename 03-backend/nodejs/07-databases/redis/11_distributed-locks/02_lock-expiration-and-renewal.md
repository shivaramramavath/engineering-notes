# Lock Expiration and Renewal

The TTL that makes a lock safe against crashes also makes it **unsafe against slowness**. This lesson covers what happens when a holder outlives its lock, how to extend a lease automatically, how to stop work when the lock is lost, and how **fencing tokens** make protected resources safe even when everything else goes wrong.

## The core problem

```
time ──────────────────────────────────────────────────────────────►
A  acquire(TTL 10s) ───── long GC pause / slow I/O / blocked event loop ───── write ✗
Redis                      lock expires at t=10
B                                       acquire ── write ✓ ──
                                                                   A wakes up, still believes it holds the lock
```

A **checked the lock, then paused**. By the time it acts, B holds the lock, and both write. Nothing in Redis can prevent that, because Redis doesn't know A is alive but slow.

Causes in Node.js:

| Cause | Example |
|-------|---------|
| Slow dependency | A database or API call that hangs |
| Blocked event loop | CPU-heavy synchronous code, huge `JSON.parse` |
| Process suspended | Container throttling, VM pause, `SIGSTOP`, live migration |
| Network stall | Packets delayed between your service and the protected resource |
| GC pauses | Large heaps |

## Defenses, in order of strength

| # | Defense | What it does | Guarantee |
|---|---------|--------------|-----------|
| 1 | Short critical sections | Less time to expire | Reduces odds |
| 2 | Generous TTL | More slack | Reduces odds |
| 3 | **Renewal (watchdog)** | Extends the lease while you work | Handles slow work, **not** pauses |
| 4 | **Abort on loss** | Stops work when the lock is lost | Limits damage after detection |
| 5 | **Fencing tokens** | The protected resource rejects stale holders | **Actual safety** |
| 6 | Idempotent operations | A duplicate does no harm | Safety by design |

Defenses 1 to 4 make the problem **rare**. Only 5 and 6 make it **harmless**.

## Renewal (the watchdog)

Instead of one long TTL, use a **short lease** and extend it periodically while you still hold the lock. If the process dies, the lease lapses quickly.

Extension must be **owner-checked and atomic**:

```lua
-- lock-extend.lua
-- KEYS[1] = lock key, ARGV[1] = token, ARGV[2] = new ttl in ms
if redis.call("GET", KEYS[1]) == ARGV[1] then
  return redis.call("PEXPIRE", KEYS[1], ARGV[2])
end
return 0
```

It returns `1` if the lock was yours and was extended, `0` if you **no longer own it**. Renew at roughly **TTL / 3**, so you have two chances before expiry.

## A complete lock manager

This replaces the simple helpers from [lesson 01](./01_locking-fundamentals.md) with renewal, abort signals and fencing.

### Commands

```lua
-- lock-acquire.lua
-- KEYS[1] = lock key, KEYS[2] = fence counter key (same slot as KEYS[1])
-- ARGV[1] = token, ARGV[2] = ttl ms
if redis.call("SET", KEYS[1], ARGV[1], "NX", "PX", ARGV[2]) then
  return redis.call("INCR", KEYS[2])        -- fencing token: strictly increasing per resource
end
return 0
```

```lua
-- lock-release.lua
if redis.call("GET", KEYS[1]) == ARGV[1] then
  return redis.call("DEL", KEYS[1])
end
return 0
```

```ts
// src/locks/commands.ts
import type { Redis } from "ioredis";
import { readFileSync } from "node:fs";
import { join } from "node:path";

declare module "ioredis" {
  interface RedisCommander<Context> {
    lockAcquire(lockKey: string, fenceKey: string, token: string, ttlMs: number): Promise<number>;
    lockExtend(lockKey: string, token: string, ttlMs: number): Promise<number>;
    lockRelease(lockKey: string, token: string): Promise<number>;
  }
}

const lua = (name: string) => readFileSync(join(import.meta.dirname, "scripts", `${name}.lua`), "utf8");

export function registerLockCommands(redis: Redis) {
  redis.defineCommand("lockAcquire", { numberOfKeys: 2, lua: lua("lock-acquire") });
  redis.defineCommand("lockExtend",  { numberOfKeys: 1, lua: lua("lock-extend") });
  redis.defineCommand("lockRelease", { numberOfKeys: 1, lua: lua("lock-release") });
}
```

(See [Typed Redis Client](../08_nodejs-integration/04_typed-redis-client.md) for the typing notes, and [Lua Scripts](../06_advanced-commands/03_lua-scripts.md) for `defineCommand`.)

### The lock handle

```ts
// src/locks/lock.ts
import type { Redis } from "ioredis";

type Logger = { warn: (o: unknown, m?: string) => void };

export class LockLostError extends Error {
  constructor(public lockKey: string, cause?: unknown) {
    super(`lock lost: ${lockKey}`, { cause });
  }
}

export class Lock {
  /** Aborted when the lock is lost. Pass it to fetch(), DB calls, or check it between steps. */
  readonly signal: AbortSignal;

  private ac = new AbortController();
  private timer?: NodeJS.Timeout;
  private lastRenewed = Date.now();
  private released = false;

  constructor(
    private redis: Redis,
    readonly lockKey: string,
    readonly token: string,
    readonly fence: number,            // fencing token: send it to the protected resource
    private ttlMs: number,
    renew: boolean,
    private log: Logger = console
  ) {
    this.signal = this.ac.signal;
    if (renew) {
      this.timer = setInterval(() => void this.renewOnce(), Math.max(100, Math.floor(ttlMs / 3)));
      this.timer.unref();              // never keep the process alive just for renewals
    }
  }

  get lost() { return this.signal.aborted; }

  private async renewOnce() {
    if (this.released || this.lost) return;
    try {
      const ok = await this.redis.lockExtend(this.lockKey, this.token, this.ttlMs);
      if (ok === 1) { this.lastRenewed = Date.now(); return; }
      this.markLost(new LockLostError(this.lockKey));                 // someone else owns it now (or it expired)
    } catch (err) {
      // Redis unreachable: keep trying until the lease would have expired anyway
      if (Date.now() - this.lastRenewed >= this.ttlMs) this.markLost(new LockLostError(this.lockKey, err));
    }
  }

  private markLost(reason: LockLostError) {
    clearInterval(this.timer);
    this.log.warn({ lock: this.lockKey, fence: this.fence }, "lock lost, aborting work");
    this.ac.abort(reason);
  }

  async release(): Promise<boolean> {
    this.released = true;
    clearInterval(this.timer);
    try {
      return (await this.redis.lockRelease(this.lockKey, this.token)) === 1;
    } catch {
      return false;                     // the TTL will free it
    }
  }
}
```

### The manager

```ts
// src/locks/manager.ts
import { randomUUID } from "node:crypto";
import type { Redis } from "ioredis";
import { Lock, LockLostError } from "./lock.js";
import { KeyBuilder } from "../redis/key-builder.js";

const K = KeyBuilder.create("shop");
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export interface AcquireOptions {
  ttlMs: number;                // the lease, renewed at ttl / 3
  retries?: number;             // default 0 (try once)
  retryDelayMs?: number;        // default 100
  jitterMs?: number;            // default 100
  renew?: boolean;              // default true
}

export class LockManager {
  constructor(private redis: Redis) {}

  /** Hash-tagged so the lock and its fence counter share a slot (Cluster-safe). */
  private keysFor(name: string) {
    return {
      lock: K.tagged(["lock", name], "mutex"),
      fence: K.tagged(["lock", name], "fence"),
    };
  }

  async acquire(name: string, o: AcquireOptions): Promise<Lock | null> {
    const { lock: lockKey, fence: fenceKey } = this.keysFor(name);
    const token = randomUUID();
    const { retries = 0, retryDelayMs = 100, jitterMs = 100 } = o;

    for (let attempt = 0; attempt <= retries; attempt++) {
      const fence = await this.redis.lockAcquire(lockKey, fenceKey, token, o.ttlMs);
      if (fence > 0) return new Lock(this.redis, lockKey, token, fence, o.ttlMs, o.renew ?? true);
      if (attempt < retries) await sleep(retryDelayMs + Math.random() * jitterMs);
    }
    return null;
  }

  /** Run `fn` while holding the lock. Throws LockLostError if the lock was lost during the work. */
  async using<T>(name: string, o: AcquireOptions, fn: (lock: Lock) => Promise<T>): Promise<T> {
    const lock = await this.acquire(name, o);
    if (!lock) throw new LockNotAcquiredError(name);

    try {
      const result = await fn(lock);
      if (lock.lost) throw lock.signal.reason;       // the work finished, but exclusivity was not guaranteed
      return result;
    } finally {
      await lock.release();
    }
  }
}

export class LockNotAcquiredError extends Error {
  constructor(public lockName: string) { super(`could not acquire lock: ${lockName}`); }
}
```

### Using it

```ts
await locks.using("order:1042", { ttlMs: 10_000, retries: 20 }, async (lock) => {
  const order = await loadOrder(1042);

  lock.signal.throwIfAborted();                              // stop early if the lock is gone
  await chargeCard(order, { idempotencyKey: `order-${order.id}`, signal: lock.signal });

  lock.signal.throwIfAborted();
  const { rowCount } = await db.query(
    "UPDATE orders SET status = 'paid', fence = $2 WHERE id = $1 AND fence < $2",
    [order.id, lock.fence]                                   // fencing check in the database
  );
  if (rowCount === 0) throw new Error("stale lock holder: write rejected");
});
```

What each line protects:

| Line | Purpose |
|------|---------|
| `ttlMs: 10_000` with renewal | Short lease, extended every ~3.3 s while working |
| `lock.signal` | Cancels in-flight calls and tells you to stop when the lock is lost |
| `idempotencyKey` | A repeated charge is a no-op at the provider |
| `AND fence < $2` | The database **rejects a stale holder** even if every other defense failed |

`AbortSignal.throwIfAborted()` needs Node 17.3+. On older versions check `signal.aborted` yourself.

## Node.js caveats for renewal

- **A blocked event loop blocks renewal.** `setInterval` can't fire while synchronous code runs. A 15 s CPU-bound loop means **no renewals** for 15 s. Move heavy CPU work to **worker threads**, or chunk it with `await setImmediate`
- **Timers drift** and may run late under load, which is why renewal happens at **TTL / 3** (slack for two misses)
- `timer.unref()` stops renewals from keeping the process alive
- Renewal calls share your commands connection, so a saturated or slow connection delays them. Keep Redis latency low and avoid giant blocking commands

If the process is **frozen** (container throttled, VM paused), the watchdog can't run and **no client-side code can save you**. That is what fencing is for.

## Fencing tokens

A **fencing token** is a number that increases every time the lock is granted. The holder sends it with every write, and the **protected resource** remembers the highest token it has seen and **rejects anything lower**:

```
A acquires → token 33        A pauses ...
B acquires → token 34 ── writes (34) ──► resource: max=34 ✓
A wakes up ── writes (33) ─────────────► resource: 33 < 34 → REJECTED
```

Safety now **doesn't depend on timing**. It depends on the resource doing a simple comparison. The `INCR` in the acquire script produces the token **atomically with the lock grant**, so tokens are strictly increasing per resource.

### Enforcing it

| Resource | How |
|----------|-----|
| **SQL** | `UPDATE ... SET ..., fence = $t WHERE id = $id AND fence < $t`, and treat 0 rows as "stale" |
| **Another Redis key** | A Lua script that compares the stored fence before writing |
| **An external API** | Use its idempotency or version feature (ETag, `If-Match`, sequence numbers) |
| **File or object storage** | Conditional writes (`If-Match`, generation numbers), or write to a path including the token |

A Redis-side example:

```lua
-- KEYS[1] = value key, KEYS[2] = its fence key; ARGV[1] = fence, ARGV[2] = new value
local current = tonumber(redis.call("GET", KEYS[2]) or "0")
if tonumber(ARGV[1]) < current then
  return 0                                   -- stale writer
end
redis.call("SET", KEYS[1], ARGV[2])
redis.call("SET", KEYS[2], ARGV[1])
return 1
```

### Limits of fencing with Redis

- The protected system must **support** the check. A system you can't modify can't enforce it
- The fence counter lives on one Redis primary, and a failover with **async replication can roll the counter back**, so for the strictest needs use a consensus store (etcd, ZooKeeper) to issue tokens
- It protects **writes**. A stale holder can still do harmless reads or send emails, so combine with idempotency keys for external side effects

## What to do when the lock is lost

Decide **before** it happens:

| Strategy | When it fits |
|----------|--------------|
| **Abort and let the next holder redo it** | Work is idempotent (the best case) |
| **Abort and roll back** | You own a transaction you can cancel |
| **Finish, but fence the write** | A short critical section ending in a conditional write |
| **Compensate** | Undo what you can (refund, delete the partial result) |
| **Alert a human** | Money or data integrity is at stake and the case is rare |

`LockManager.using` throws `LockLostError` if the lock was lost during the work, so the caller **cannot ignore** that exclusivity was in doubt.

## Leader election with leases

A leader is simply a **lock held for a long time with renewal**. Everyone else retries; when the leader dies, the lease expires and another instance takes over.

```ts
export class LeaderElector {
  private running = false;
  private current?: Lock;
  private stopAc = new AbortController();

  constructor(
    private locks: LockManager,
    private name: string,
    private ttlMs: number,
    private onElected: (signal: AbortSignal) => Promise<void>,   // returns when leadership should end
    private onLost?: () => void
  ) {}

  async start() {
    this.running = true;
    while (this.running) {
      const lock = await this.locks.acquire(this.name, { ttlMs: this.ttlMs, retries: 0 });

      if (!lock) {
        await sleep(this.ttlMs / 2 + Math.random() * 500);        // follower: check again soon
        continue;
      }

      this.current = lock;
      try {
        await this.onElected(AbortSignal.any([lock.signal, this.stopAc.signal]));   // Node 20.3+
      } finally {
        await lock.release();
        this.current = undefined;
        this.onLost?.();                                           // step down: stop leader-only work
      }
    }
  }

  async stop() {
    this.running = false;
    this.stopAc.abort();                                           // makes onElected return
  }
}
```

```ts
const elector = new LeaderElector(locks, "scheduler", 15_000, async (signal) => {
  while (!signal.aborted) {
    await runScheduledTasks();
    await sleep(1_000);
  }
});
void elector.start();
```

Properties and cautions:

- **At most one leader, best effort.** During pauses or failover two instances can briefly both act as leader, so leader work should be **idempotent** or fenced
- Followers **poll** at about half the lease, so failover takes up to `ttl + poll interval`
- On `onLost`, leader-only work **must stop immediately**
- For anything beyond scheduling and housekeeping, prefer Kubernetes Leases or etcd/Consul/ZooKeeper

## Choosing TTLs and renewal intervals

| Work | Lease (TTL) | Renew every |
|------|-------------|-------------|
| Sub-second critical section | 5 to 10 s, **no renewal** needed | n/a |
| A few seconds of I/O | 10 to 15 s | ~4 to 5 s |
| Minutes (batch job) | 30 to 60 s | 10 to 20 s |
| Leader election | 10 to 30 s | ~3 to 10 s |

A shorter lease means faster recovery after a crash, but a higher chance of expiring during a hiccup. Pick the shortest lease you can **comfortably renew at TTL / 3** in your environment.

## Don't rely on expiry notifications

Keyspace notifications for expired keys are **best-effort Pub/Sub** ([Pub/Sub Fundamentals](../09_pub-sub/01_pub-sub-fundamentals.md#keyspace-notifications-optional)) and may arrive late or never. Don't use them to hand off a lock. Use polling with jitter, or a queue.

## Testing expiry and fencing

Disable renewal and force the pause:

```ts
it("a stale holder is rejected by the fenced resource", async () => {
  const resource = new FencedStore();                    // remembers the highest fence, rejects lower ones

  const a = await locks.acquire("res:1", { ttlMs: 100, renew: false });   // A acquires, fence 1
  await sleep(150);                                                       // A "pauses" past its lease
  const b = await locks.acquire("res:1", { ttlMs: 5_000, renew: false }); // B acquires, fence 2

  resource.write("from B", b!.fence);                                     // accepted
  expect(() => resource.write("from A", a!.fence)).toThrow(/stale/);      // rejected
  expect(resource.value).toBe("from B");
});

it("aborts work when renewal fails", async () => {
  const lock = await locks.acquire("res:2", { ttlMs: 300 });
  await redis.del(lock!.lockKey);                         // simulate losing the lock
  await waitFor(() => lock!.lost, 1_000);                 // the watchdog notices on its next renewal
  expect(lock!.signal.reason).toBeInstanceOf(LockLostError);
});
```

Also test with a **blocked event loop** (a synchronous busy loop longer than the TTL) to see exactly what the watchdog can't do, and confirm the fence still protects the data. Chaos checks worth running: restart Redis while a lock is held, and trigger a Sentinel failover.

## Observability

- **Renewal failures** and **lock-lost events** (any non-zero count deserves investigation)
- **Hold time versus lease** (holds approaching the lease length suggest the work or the lease needs changing)
- **Fence rejections** at the protected resource (each one is a stale holder that fencing just saved you from)
- Acquire contention and wait time ([lesson 01](./01_locking-fundamentals.md#observability))

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| One long TTL "to be safe" | A short lease with renewal |
| Renewal without an owner check | Lua compare-then-`PEXPIRE` |
| Ignoring the result of `extend` | `0` means the lock is lost, so abort |
| CPU-heavy work on the main thread | Worker threads, or chunk and yield |
| Continuing work after the lock is lost | Pass `signal` to I/O, check `throwIfAborted()` |
| Assuming the watchdog protects against pauses | It can't. Use fencing or idempotency |
| Fencing token generated separately from the lock grant | Issue it in the **same Lua script** |
| Protected system can't check the token | Idempotency keys, conditional writes, or a different design |
| Using lock expiry events as a signal | Poll with jitter, or use a queue |
| Leader work that isn't idempotent | Assume two leaders can overlap briefly |

## Key takeaways

- A lock **can expire while its holder is still working**, and renewal reduces that but can't eliminate it
- Use a short lease, renew at TTL/3, and **abort work through an `AbortSignal`** when renewal fails
- **Fencing tokens** (issued atomically with the grant, enforced by the protected resource) turn a probabilistic lock into a safe one
- Design for loss: idempotent operations, conditional writes, and clear handling when the lock is lost

**Next:** [Redlock](./03_redlock.md)
