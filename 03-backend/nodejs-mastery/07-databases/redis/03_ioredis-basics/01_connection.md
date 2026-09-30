# Connection

An ioredis client is a long-lived object that owns a TCP connection to Redis, reconnects automatically, and queues commands while it does.

## Creating a client

```ts
import { Redis } from "ioredis";

const redis = new Redis();                              // 127.0.0.1:6379
const r2 = new Redis(6380, "redis.internal");           // port, host
const r3 = new Redis("redis://:secret@host:6379/0");    // URL
const r4 = new Redis({ host: "host", port: 6379, password: "secret", db: 0 });
```

By default the client **connects immediately** and you can send commands right away. They are queued until the connection is ready.

## Lifecycle and status

```
wait ─► connecting ─► connect ─► ready ─► close ─► reconnecting ─► connecting ...
                                              └────────► end (no more retries / quit)
```

`redis.status` is one of:

| Status | Meaning |
|--------|---------|
| `wait` | `lazyConnect` is on and `connect()` hasn't been called |
| `connecting` | TCP connection in progress |
| `connect` | TCP connected, running handshake (AUTH, SELECT, ready check) |
| `ready` | Ready to serve commands |
| `reconnecting` | Waiting to retry after a drop |
| `close` | Connection closed (may reconnect) |
| `end` | Finished. No further reconnects |

```ts
console.log(redis.status); // "ready"
```

## Events

```ts
redis.on("connect", () => console.log("TCP connected"));
redis.on("ready", () => console.log("ready for commands"));
redis.on("error", (err) => console.error("error:", err.message));
redis.on("close", () => console.log("connection closed"));
redis.on("reconnecting", (delayMs: number) => console.log(`retry in ${delayMs}ms`));
redis.on("end", () => console.log("no more reconnects"));
```

| Event | When | Use it for |
|-------|------|-----------|
| `connect` | Socket connected | Logging |
| `ready` | After handshake (and `INFO` ready check) | Marking the app healthy |
| `error` | Any connection-level error | **Always attach a listener** |
| `close` | Socket closed | Marking unhealthy |
| `reconnecting` | Before each retry | Metrics on flapping |
| `end` | Reconnection stopped | Alerting, crash and restart |
| `wait` | With `lazyConnect` | Rare |

**Always add an `error` listener.** Without one, ioredis prints "Unhandled error event" to the console and you get no chance to react.

## Lazy connect

Delay the connection until you need it:

```ts
const redis = new Redis({ lazyConnect: true });
console.log(redis.status); // "wait"

await redis.connect();     // connect explicitly
```

The first command also triggers the connection if you never call `connect()`. Lazy connect is useful for tests, serverless cold starts, and failing fast at boot:

```ts
try {
  await redis.connect();
  await redis.ping();
} catch (err) {
  console.error("Redis unavailable at startup", err);
  process.exit(1);
}
```

## Waiting until ready

```ts
import { Redis } from "ioredis";

export function waitUntilReady(redis: Redis, timeoutMs = 10_000): Promise<void> {
  if (redis.status === "ready") return Promise.resolve();
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("Redis ready timeout")), timeoutMs);
    redis.once("ready", () => {
      clearTimeout(timer);
      resolve();
    });
  });
}
```

## Automatic reconnection

When the connection drops, ioredis calls your `retryStrategy(times)`. The number you return is the delay in milliseconds before the next attempt. Returning a non-number stops reconnecting.

```ts
const redis = new Redis({
  retryStrategy: (times) => Math.min(times * 100, 3000), // 100ms, 200ms ... capped at 3s
});
```

During reconnection:

- New commands go to the **offline queue** (`enableOfflineQueue`, on by default)
- Commands that were already sent but unanswered are **resent** after reconnect (`autoResendUnfulfilledCommands`)
- Subscriptions are **restored** (`autoResubscribe`)
- The selected `db` is re-selected automatically

Details and trade-offs are in [Configuration](./02_configuration.md).

## One connection is usually enough

Redis processes commands in order on one thread, and ioredis **multiplexes** many in-flight commands over a single connection (concurrent `await`s don't block each other). Most apps need **one shared client**, not a pool.

```ts
// Fires all 1000 concurrently over one connection
await Promise.all(Array.from({ length: 1000 }, (_, i) => redis.get(`k:${i}`)));
```

## When you need more than one connection

| Situation | Why |
|-----------|-----|
| **Subscriber** (`SUBSCRIBE`, `PSUBSCRIBE`) | A subscribing connection can only run subscription commands |
| **Blocking commands** (`BLPOP`, `BRPOP`, `XREAD BLOCK`, `XREADGROUP BLOCK`) | They occupy the connection until they return |
| **Different `db`** or different credentials | Connection-level state |
| **BullMQ** | Uses separate connections for workers and queues |

Use `duplicate()` to clone options:

```ts
const redis = new Redis(process.env.REDIS_URL);
const subscriber = redis.duplicate();           // same options, new connection
const blocker = redis.duplicate();              // for blocking reads

await subscriber.subscribe("news");
subscriber.on("message", (channel, message) => console.log(channel, message));
```

A connection **in subscriber mode** cannot run `GET`, `SET`, etc. Use another client for those.

### About connection pooling

ioredis has no built-in pool, and you rarely need one. A pool helps only when single-connection throughput becomes a bottleneck (very large payloads, heavy blocking use), or when you want isolation between workloads. If you must, create a small fixed set of clients and round-robin:

```ts
const pool = Array.from({ length: 4 }, () => new Redis(process.env.REDIS_URL));
let i = 0;
export const getClient = () => pool[i++ % pool.length];
```

Measure before adding this complexity. Pipelining and auto pipelining usually give bigger wins (covered in `06_advanced-commands`).

## Naming connections

```ts
const redis = new Redis({ connectionName: "api-cache" });
```

Then in `redis-cli`:

```
CLIENT LIST
# name=api-cache addr=... 
```

This makes debugging clients on a busy server far easier.

## Checking health

```ts
async function isRedisHealthy(redis: Redis): Promise<boolean> {
  if (redis.status !== "ready") return false;
  try {
    return (await redis.ping()) === "PONG";
  } catch {
    return false;
  }
}
```

Use this in `/health` or readiness endpoints.

## Debug logging

```bash
DEBUG=ioredis:* node dist/index.js
```

Prints connection events and commands. Use it for troubleshooting only.

## Common mistakes

| Mistake | Fix |
|---------|-----|
| Creating a new `Redis()` per request | One shared client per process |
| No `error` listener | Attach one at creation |
| Sharing one client for subscribe and normal commands | Use `duplicate()` |
| Sharing one client for blocking reads and normal commands | Dedicated client for blocking |
| Forgetting to close scripts | `await redis.quit()` at the end |

## Key takeaways

- One client = one multiplexed connection, and reuse it
- Track `status` and events, and always handle `error`
- Use `duplicate()` for subscribers and blocking commands
- Reconnection, offline queueing and resubscription are automatic

**Next:** [Configuration](./02_configuration.md)
