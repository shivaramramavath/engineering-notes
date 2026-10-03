# Testing with Redis

Redis behavior depends on **server-side semantics**: expiry, atomic commands, Lua, ordering, blocking reads. Code that looks right can still fail against a real server, so a good test suite uses a mix of fast tests for your logic and real-Redis tests for everything Redis-specific.

```
          ▲  few       End-to-end (app + Redis + HTTP)
         ╱ ╲
        ╱   ╲          Integration (your code + real Redis)   ← most Redis value is here
       ╱     ╲
      ╱       ╲        Unit (logic + fake store)              ← fast, many
     ╱─────────╲  many
```

## What to test where

| Concern | Unit (fake) | Integration (real Redis) |
|---------|:-----------:|:------------------------:|
| Business logic around the cache | Yes | Optional |
| Cache hit and miss flow | Yes | Yes |
| TTL and expiry | No | Yes |
| Atomicity and race conditions | No | Yes |
| Lua scripts | No | Yes |
| Pub/Sub, streams, consumer groups | No | Yes |
| Serialization round-trips | Yes | Yes |
| Behavior when Redis is down | Yes (a failing fake) | Yes |
| Cluster or Sentinel behavior | No | Yes, against the same topology as production |

Do not test Redis itself. Test **your use** of it.

## Three ways to get a Redis in tests

| Approach | Pros | Cons |
|----------|------|------|
| **Real Redis** via Docker, a CI service or Testcontainers | Real semantics, same version as production | Needs Docker, a little slower |
| **ioredis-mock** | No server, quick start | Partial command coverage, scripting and Streams differ, no real timing or atomicity |
| **Your own fake** behind an interface | Fast, fully controllable, can inject failures | Only tests logic, not Redis behavior |

A sound default: **a fake for unit tests, a real Redis for integration tests, and ioredis-mock only where a real server is truly unavailable.**

## Design for testability

Depend on a small interface instead of the raw client:

```ts
export interface CacheStore {
  get(key: string): Promise<string | null>;
  set(key: string, value: string, ttlSeconds: number): Promise<void>;
  del(key: string): Promise<void>;
}

export class RedisCacheStore implements CacheStore {
  constructor(private redis: Redis) {}
  get(key: string) { return this.redis.get(key); }
  async set(key: string, value: string, ttl: number) { await this.redis.set(key, value, "EX", ttl); }
  async del(key: string) { await this.redis.unlink(key); }
}
```

A fake with controllable time lets unit tests check expiry logic without waiting:

```ts
export class MemoryCacheStore implements CacheStore {
  private data = new Map<string, { value: string; expiresAt: number }>();
  constructor(private now: () => number = Date.now) {}

  async get(key: string) {
    const hit = this.data.get(key);
    if (!hit) return null;
    if (hit.expiresAt <= this.now()) { this.data.delete(key); return null; }
    return hit.value;
  }
  async set(key: string, value: string, ttl: number) {
    this.data.set(key, { value, expiresAt: this.now() + ttl * 1000 });
  }
  async del(key: string) { this.data.delete(key); }
}
```

Unit-test the cache-aside flow with it (examples use Vitest. In Jest, replace `vi` with `jest` and drop the import):

```ts
import { describe, it, expect, vi } from "vitest";

describe("getUser (cache-aside)", () => {
  it("loads once, then serves from cache", async () => {
    const store = new MemoryCacheStore();
    const loader = vi.fn(async (id: number) => ({ id, name: "Ada" }));
    const service = new UserService(store, loader);

    await service.getUser(1);
    await service.getUser(1);

    expect(loader).toHaveBeenCalledTimes(1);
  });

  it("falls back to the loader when the store fails", async () => {
    const broken: CacheStore = {
      get: async () => { throw new Error("redis down"); },
      set: async () => { throw new Error("redis down"); },
      del: async () => { throw new Error("redis down"); },
    };
    const loader = vi.fn(async (id: number) => ({ id, name: "Ada" }));

    const user = await new UserService(broken, loader).getUser(1);

    expect(user.name).toBe("Ada");        // degraded, not failed
  });
});
```

## Integration tests against a real Redis

### Start Redis

**Docker (local):**

```bash
docker run --rm -p 6379:6379 redis:7
```

**Docker Compose:**

```yaml
# docker-compose.test.yml
services:
  redis:
    image: redis:7
    ports: ["6379:6379"]
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 2s
      timeout: 2s
      retries: 10
```

**Testcontainers** (starts a throwaway container per test run, no manual setup):

```ts
// test/global-setup.ts
import { RedisContainer, type StartedRedisContainer } from "@testcontainers/redis";

let container: StartedRedisContainer;

export async function setup() {
  container = await new RedisContainer("redis:7").start();
  process.env.REDIS_URL = container.getConnectionUrl();
}

export async function teardown() {
  await container.stop();
}
```

How environment variables reach test workers depends on your runner (Jest, Vitest). Check its global-setup docs, or use its provide/inject mechanism.

Use the **same Redis version and mode** as production. Behavior differs across versions, and a standalone server will not reveal `CROSSSLOT` errors that a cluster would.

### Isolation: one prefix per test file

Tests that share keys interfere with each other, especially when run in parallel. Give each test (or file) its own namespace, and clean up only that namespace.

```ts
// test/redis.ts
import Redis from "ioredis";
import { randomUUID } from "node:crypto";

export function createTestRedis() {
  const url = process.env.REDIS_URL ?? "redis://localhost:6379";

  // Safety guard: never run destructive tests against a shared or remote server
  if (process.env.NODE_ENV !== "test" || !/localhost|127\.0\.0\.1|redis-test/.test(url)) {
    throw new Error(`Refusing to run Redis tests against ${url}`);
  }

  const worker = process.env.VITEST_POOL_ID ?? process.env.JEST_WORKER_ID ?? "0";
  const prefix = `test:${worker}:${randomUUID().slice(0, 8)}:`;

  const raw = new Redis(url);                          // no prefix: for cleanup and inspection
  const redis = new Redis(url, { keyPrefix: prefix }); // for the code under test

  async function cleanup() {
    const keys: string[] = [];
    for await (const batch of raw.scanStream({ match: `${prefix}*`, count: 200 })) {
      keys.push(...(batch as string[]));
    }
    if (keys.length) await raw.unlink(...keys);
  }

  async function close() {
    await cleanup();
    await Promise.all([raw.quit(), redis.quit()]);
  }

  return { redis, raw, prefix, cleanup, close };
}
```

Use it:

```ts
import { beforeAll, afterAll, beforeEach, describe, it, expect } from "vitest";

const t = createTestRedis();

beforeEach(() => t.cleanup());
afterAll(() => t.close());                    // otherwise the test process hangs on open handles

describe("session store", () => {
  it("stores and expires sessions", async () => {
    const store = new SessionStore(t.redis);
    await store.create("abc", { userId: 1 }, 30);

    expect(await store.get("abc")).toEqual({ userId: 1 });
    expect(await t.redis.ttl("session:abc")).toBeGreaterThan(0);
  });
});
```

Notes on `keyPrefix`: ioredis adds it to key arguments of commands, which makes isolation almost free. It does not rewrite keys your Lua scripts build from `ARGV`, and keys returned by `SCAN` or `KEYS` come back **with** the prefix. That is why the cleanup uses a separate, unprefixed client. See [Common Pitfalls](./03_common-pitfalls.md).

Avoid `FLUSHALL` and `FLUSHDB` as your cleanup strategy. They are fine on a throwaway container and dangerous on anything shared, and they make parallel test files fight each other.

### Catch missing TTLs automatically

Unbounded keys are a top cause of memory problems. A tiny helper turns that into a test failure:

```ts
export async function expectAllKeysHaveTtl(raw: Redis, prefix: string) {
  for await (const batch of raw.scanStream({ match: `${prefix}*` })) {
    for (const key of batch as string[]) {
      const ttl = await raw.pttl(key);      // -1 means "no expiry", -2 means "missing"
      expect(ttl, `key ${key} has no TTL`).toBeGreaterThan(0);
    }
  }
}
```

Call it at the end of tests for caches and sessions.

## Testing expiry without flakiness

Fake timers (`vi.useFakeTimers()`, `jest.useFakeTimers()`) change **Node's** clock, not the **server's**. Redis expires keys using its own clock, so:

- Use a very short TTL in milliseconds (`PX`) and wait a bit longer
- Prefer **polling** over a fixed sleep

```ts
async function waitFor<T>(fn: () => Promise<T>, ok: (v: T) => boolean, timeoutMs = 2000, stepMs = 20) {
  const start = Date.now();
  for (;;) {
    const v = await fn();
    if (ok(v)) return v;
    if (Date.now() - start > timeoutMs) throw new Error("waitFor timed out");
    await new Promise((r) => setTimeout(r, stepMs));
  }
}

it("expires the key", async () => {
  await t.redis.set("temp", "x", "PX", 100);
  await waitFor(() => t.redis.exists("temp"), (n) => n === 0);
});
```

For logic that depends on time **in your code**, inject a clock (`now: () => number`) so unit tests control it.

## Testing atomicity and races

These bugs are invisible with one caller. Fire many at once:

```ts
it("counts correctly under concurrency", async () => {
  await Promise.all(Array.from({ length: 200 }, () => t.redis.incr("hits")));
  expect(await t.redis.get("hits")).toBe("200");
});
```

Prove that a read-modify-write version is broken, and that your fix works:

```ts
// buggy: read, modify, write
async function badIncrement(key: string) {
  const n = Number((await t.redis.get(key)) ?? 0);
  await t.redis.set(key, n + 1);
}

it("loses updates with read-modify-write", async () => {
  await Promise.all(Array.from({ length: 50 }, () => badIncrement("c")));
  expect(Number(await t.redis.get("c"))).toBeLessThan(50);   // demonstrates the race (probabilistic)
});
```

Run a concurrency test several times in CI to build confidence. Races are probabilistic, so a single pass proves little.

Test locks and idempotency the same way: start N workers, assert that exactly one wins.

```ts
it("grants the lock to exactly one caller", async () => {
  const results = await Promise.all(
    Array.from({ length: 20 }, () => t.redis.set("lock:job", "1", "EX", 10, "NX")),
  );
  expect(results.filter((r) => r === "OK")).toHaveLength(1);
});
```

## Testing Lua scripts

Always test scripts against a real server. Mocks rarely match Redis' Lua environment exactly.

```ts
t.redis.defineCommand("consume", {
  numberOfKeys: 1,
  lua: `
    local current = tonumber(redis.call("GET", KEYS[1]) or "0")
    if current >= tonumber(ARGV[1]) then return 0 end
    redis.call("INCR", KEYS[1])
    return 1
  `,
});

it("allows at most N calls, even concurrently", async () => {
  const results = await Promise.all(
    Array.from({ length: 30 }, () => (t.redis as any).consume("rate:u1", 10)),
  );
  expect(results.filter((r) => r === 1)).toHaveLength(10);
});
```

Things worth asserting for every script:

- Return types (numbers vs strings vs `nil`), since Lua conversion has surprises
- Behavior on missing keys
- Behavior at the boundary (limit, limit minus one, limit plus one)
- That it never exceeds your latency budget (a slow script blocks the whole server)

See [Lua Scripts](../06_advanced-commands/03_lua-scripts.md).

## Testing Pub/Sub

A subscriber needs its **own connection**, and it must be subscribed **before** the publish, or the message is lost.

```ts
it("delivers a published message", async () => {
  const sub = t.redis.duplicate();
  const received = new Promise<string>((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("no message")), 2000);
    sub.once("message", (_channel, message) => { clearTimeout(timer); resolve(message); });
  });

  await sub.subscribe("events");                // resolves once the subscription is confirmed
  await t.redis.publish("events", "hello");

  expect(await received).toBe("hello");
  await sub.quit();
});
```

Note that `duplicate()` copies the options, including `keyPrefix`. Channel names are **not** prefixed by `keyPrefix`, so use unique channel names per test (for example include `t.prefix`).

## Testing streams and consumer groups

```ts
it("processes and acknowledges a message", async () => {
  const stream = "orders";
  await t.redis.xgroup("CREATE", stream, "workers", "$", "MKSTREAM");
  await t.redis.xadd(stream, "*", "orderId", "42");

  const res = await t.redis.xreadgroup("GROUP", "workers", "c1", "COUNT", 1, "STREAMS", stream, ">");
  const [[, entries]] = res as [string, [string, string[]][]][];
  const [id] = entries[0];

  expect(((await t.redis.xpending(stream, "workers")) as any[])[0]).toBe(1);   // pending
  await t.redis.xack(stream, "workers", id);
  expect(((await t.redis.xpending(stream, "workers")) as any[])[0]).toBe(0);   // acknowledged
});
```

Also test the unhappy paths: a consumer that crashes before `XACK` (message stays pending and gets reclaimed), duplicate delivery (handlers must be idempotent) and poison messages that end up in a dead-letter stream. See [Consumer Groups and Acks](../10_streams/03_consumer-groups-and-acks.md).

## Testing failure paths

Production Redis will be slow, unreachable or restarting at some point. Your tests should know what happens then.

### Redis unreachable

```ts
import Redis from "ioredis";

it("serves from the database when Redis is down", async () => {
  const dead = new Redis({
    port: 6390,                  // nothing listens here
    lazyConnect: true,
    enableOfflineQueue: false,   // fail immediately instead of queuing
    retryStrategy: () => null,   // do not reconnect
  });
  dead.on("error", () => {});    // keep the test output quiet

  const service = new UserService(new RedisCacheStore(dead), loadFromDb);
  const user = await service.getUser(1);

  expect(user).toBeDefined();    // the cache failed open
});
```

### Timeouts and slow Redis

```ts
const slow = new Redis({ commandTimeout: 50 });
await expect(slow.call("DEBUG", "SLEEP", "0.2")).rejects.toThrow(/timed out/i);   // needs DEBUG enabled on the test server
```

For realistic network faults (latency, dropped connections, resets), put **Toxiproxy** between your app and Redis and add toxics from the test.

### Reconnect behavior

```ts
it("recovers after the connection is killed", async () => {
  const id = await t.redis.call("CLIENT", "ID");
  await t.raw.call("CLIENT", "KILL", "ID", String(id));

  await waitFor(async () => {
    try { return await t.redis.ping(); } catch { return null; }
  }, (v) => v === "PONG");
});
```

Test all four, because each fails differently: **down**, **slow**, **dropped mid-flight** and **restarted** (data loss if persistence is off).

## Testing HTTP endpoints that use Redis

```ts
import request from "supertest";

it("returns a cached response on the second call", async () => {
  const app = createApp({ redis: t.redis });

  const first = await request(app).get("/products/1");
  const second = await request(app).get("/products/1");

  expect(first.headers["x-cache"]).toBe("MISS");
  expect(second.headers["x-cache"]).toBe("HIT");
});
```

Inject the Redis client into `createApp` rather than importing a global singleton. It makes isolation and failure injection straightforward. See [Express Integration](../08_nodejs-integration/06_express-integration.md).

## Testing queues and workers

- Test the **job handler** as a plain function with no Redis at all
- Test enqueue, retry and dead-letter behavior with real Redis and short delays
- For BullMQ, create a queue with a unique name per test, close the `Queue`, `Worker` and `QueueEvents` objects in `afterAll`, and give workers a connection with `maxRetriesPerRequest: null`

See [BullMQ](../14_queues-and-workers/04_bullmq.md).

## Using ioredis-mock

```ts
import RedisMock from "ioredis-mock";

const redis = new RedisMock();
await redis.set("a", "1");
```

It is useful for quick unit tests of simple get, set and hash logic when starting a container is impractical. Know the limits:

- Command coverage is partial, and edge-case behavior can differ from the real server
- Scripting, Streams, Pub/Sub details and blocking commands are the weakest areas
- No real expiry timing, no real concurrency semantics, no persistence, no cluster or TLS
- A passing mock test does **not** prove the code works on Redis

If a test would be meaningless on a fake (TTL, atomicity, scripts), run it on a real server.

## Running in CI

GitHub Actions with a service container:

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    services:
      redis:
        image: redis:7
        ports: ["6379:6379"]
        options: >-
          --health-cmd "redis-cli ping"
          --health-interval 5s
          --health-timeout 3s
          --health-retries 5
    env:
      NODE_ENV: test
      REDIS_URL: redis://localhost:6379
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 22 }
      - run: npm ci
      - run: npm test
```

Tips:

- Pin the Redis image to the production **major version** (and rerun on upgrade candidates)
- Add a matrix entry for each topology you run in production (standalone, Sentinel, Cluster) if your code depends on it
- Run concurrency tests repeatedly (for example, three times) to flush out races
- Keep test timeouts generous in CI, and per-test timeouts tight enough to catch hangs

## Debugging flaky Redis tests

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Passes alone, fails in the full run | Shared keys between tests | Per-test prefix and cleanup |
| Fails only in parallel | Shared prefix or shared `FLUSHDB` | Prefix per worker, no global flush |
| Jest says "did not exit" | Open Redis connection | `await redis.quit()` in `afterAll`, run with `--detectOpenHandles` |
| Expiry test fails randomly | Fixed sleep too close to the TTL | Poll with `waitFor`, use margins |
| Pub/Sub test never receives | Published before subscribed, or same connection | `await sub.subscribe(...)` first, use `duplicate()` |
| Works on standalone, fails on Cluster | Multi-key command across slots | Hash tags, same-slot keys |
| Fake timers do nothing to TTL | Server clock is separate | Short real TTL with polling, or inject a clock in your code |
| Keys from earlier runs interfere | Cleanup skipped on failure | Clean in `beforeEach` as well as `afterAll` |

## Checklist

```
[ ] Code depends on an interface or an injected client, not a global
[ ] Unit tests use a fake store, including a "failing store"
[ ] Integration tests run on a real Redis of the production major version
[ ] Each test file has its own key prefix; no FLUSHALL on shared servers
[ ] Safety guard refuses to run against non-test servers
[ ] Connections are closed in afterAll
[ ] TTL tests poll instead of sleeping
[ ] Concurrency tests cover counters, locks and scripts
[ ] Failure tests: down, slow, dropped, restarted
[ ] CI runs Redis as a service with a health check
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Only testing against a mock | Add real-Redis integration tests |
| `FLUSHALL` between tests | Prefix per test and scoped cleanup |
| Running tests against a dev or shared instance | Safety guard plus a dedicated test instance |
| Sleeping for a fixed time | Poll with a timeout |
| Forgetting to close connections | `quit()` in `afterAll` |
| Testing only the happy path | Inject failures: down, slow, dropped |
| One-shot concurrency tests | Repeat them; races are probabilistic |
| Fake timers for server expiry | The server clock is separate; use short real TTLs |
| Different Redis version in tests than in production | Pin the image to the production version |
| `keyPrefix` assumed to cover Lua-built keys or `SCAN` results | Use a raw client for cleanup, pass full keys in `ARGV` deliberately |

## Key takeaways

- Put Redis behind a small interface so logic is easy to unit test
- Use a real Redis for anything involving TTL, atomicity, scripts, Pub/Sub or streams
- Isolate with per-test prefixes, clean up narrowly and guard against running on shared servers
- Poll instead of sleeping, and run concurrency tests more than once
- Test what happens when Redis is down, slow or restarted
- Let CI start Redis as a service of the same version as production

**Previous:** [Testing and Debugging README](./README.md) | **Next:** [Debugging](./02_debugging.md)
