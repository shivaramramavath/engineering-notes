# Connection Management

Getting connections right means three things: **the right number**, **created once**, and **closed cleanly**. This lesson turns the basics from [03_ioredis-basics](../03_ioredis-basics/01_connection.md) into a reusable module.

## How many connections does a process need?

| Role | Connections | Why |
|------|-------------|-----|
| **Commands** (`GET`, `SET`, pipelines, scripts) | **1 per process** | ioredis multiplexes concurrent commands over one connection |
| **Subscriber** (`SUBSCRIBE`, `PSUBSCRIBE`) | 1 per process | A subscribing connection can't run normal commands |
| **Blocking reads** (`BLPOP`, `XREADGROUP BLOCK`) | 1 per consumer loop | A blocking call occupies its connection |
| **`WATCH` transactions** | 1 per attempt (or use Lua) | `WATCH` state is per connection |
| **BullMQ** | Managed by BullMQ | Queues, workers and events use their own connections |
| **Socket.IO Redis adapter** | 2 (pub and sub) | One publishes, one subscribes |

Budget check:

```
total = instances × connections per process
```

20 instances × 5 connections = 100 connections. That's fine for Redis itself (default `maxclients` is 10,000), but **managed services often enforce lower limits**, so check yours. Inspect what is connected with:

```bash
redis-cli CLIENT LIST | grep -c "name=app"
redis-cli CLIENT LIST         # look at name=, addr=, cmd=
```

Name every connection so this is readable (`connectionName`).

## A connection module

```ts
// src/redis/connection.ts
import { Redis, type RedisOptions } from "ioredis";
import { env } from "../config/env.js";

const baseOptions: RedisOptions = {
  host: env.redis.host,
  port: env.redis.port,
  username: env.redis.username,
  password: env.redis.password,
  db: env.redis.db,
  tls: env.redis.tls ? {} : undefined,
  connectTimeout: 5_000,
  keepAlive: 10_000,
  retryStrategy: (n) => Math.min(n * 200, 5_000) + Math.floor(Math.random() * 200),
  reconnectOnError: (err) => (err.message.includes("READONLY") ? 2 : false),
};

export type Role = "commands" | "subscriber" | "blocking" | "worker";

/** Options that differ by role. */
const roleOptions: Record<Role, RedisOptions> = {
  commands:   { maxRetriesPerRequest: 3, commandTimeout: 2_000 },
  subscriber: { maxRetriesPerRequest: null },      // wait patiently, autoResubscribe restores channels
  blocking:   { maxRetriesPerRequest: null },      // blocking commands must not time out client-side
  worker:     { maxRetriesPerRequest: null },      // required by BullMQ workers
};

export function createClient(name: string, role: Role = "commands", overrides: RedisOptions = {}): Redis {
  const client = new Redis({
    ...baseOptions,
    ...roleOptions[role],
    connectionName: `${env.serviceName}:${name}`,
    ...overrides,
  });

  client.on("error", (err) => console.error(`[redis:${name}]`, err.message));
  client.on("reconnecting", (ms: number) => console.warn(`[redis:${name}] reconnecting in ${ms}ms`));
  client.on("end", () => console.error(`[redis:${name}] connection ended`));

  return client;
}
```

Why per-role options matter:

- A `commandTimeout` on a **blocking** connection would reject `BLPOP` calls that legitimately wait
- A low `maxRetriesPerRequest` on a **worker** would make it give up during a brief outage
- The **commands** client should fail fast so HTTP requests don't hang

## The singleton (when you want one)

```ts
let shared: Redis | undefined;

export function getRedis(): Redis {
  return (shared ??= createClient("app"));
}
```

The `??=` makes the client **lazy** (created on first use) and shared afterwards. Node caches ES modules, so the module itself is a singleton. A plain `export const redis = createClient("app")` also works, but it connects at import time, which is awkward for tests and scripts.

Prefer to **create the client in the composition root and inject it** (lessons 02 and 06). Reserve `getRedis()` for code where injection is impractical.

### Hot reload in development

Watchers (`tsx watch`, `nodemon`, framework dev servers) re-evaluate modules, which can leak a connection per reload. Cache the client on `globalThis` in development:

```ts
const g = globalThis as unknown as { __redis?: Redis };

export function getRedis(): Redis {
  if (process.env.NODE_ENV === "production") return (shared ??= createClient("app"));
  return (g.__redis ??= createClient("app"));
}
```

A growing count in `CLIENT LIST` after saving files is the symptom.

## Duplicating for other roles

```ts
const commands = createClient("app", "commands");
const subscriber = commands.duplicate({ ...roleOptionsFor("subscriber"), connectionName: "svc:subscriber" });
```

`duplicate()` clones the options, so either pass overrides or use `createClient` with a role, which is clearer. Whichever you choose, **don't share** a connection between roles.

## Standalone, Sentinel or Cluster

Keep topology out of business code. One factory decides:

```ts
import { Redis, Cluster, type ClusterNode } from "ioredis";

export type RedisClient = Redis | Cluster;

export function createTopologyClient(name: string): RedisClient {
  switch (env.redis.mode) {
    case "cluster":
      return new Cluster(env.redis.nodes as ClusterNode[], {
        redisOptions: { username: env.redis.username, password: env.redis.password, tls: env.redis.tls ? {} : undefined },
        scaleReads: "master",                 // "slave" / "all" to read from replicas (accepts stale reads)
        enableAutoPipelining: true,
      });

    case "sentinel":
      return new Redis({
        sentinels: env.redis.sentinels,       // [{ host, port }, ...]
        name: env.redis.masterName,           // e.g. "mymaster"
        password: env.redis.password,
        sentinelPassword: env.redis.sentinelPassword,
      });

    default:
      return createClient(name);
  }
}
```

Things that change with topology (details in `15_redis-architecture`):

| Topology | What to watch |
|----------|---------------|
| **Cluster** | Multi-key commands, transactions and Lua need keys in one slot (hash tags). Scan every primary. Pub/Sub works, but `SUBSCRIBE` uses a single node |
| **Sentinel** | ioredis asks Sentinels for the current primary and follows failovers. Handle `READONLY` with `reconnectOnError` |
| **Standalone** | Simplest. One primary, optional replicas |

`Redis | Cluster` share the command API, but not every method or option (for example `select`, `keyPrefix` behavior and some multi-key helpers). If you support both, test against both.

## A lifecycle manager

Collect every connection in one place so shutdown can close them in the right order:

```ts
// src/redis/connections.ts
import type { Redis } from "ioredis";
import { createClient, type Role } from "./connection.js";

export class RedisConnections {
  private all: { name: string; role: Role; client: Redis }[] = [];

  create(name: string, role: Role = "commands"): Redis {
    const client = createClient(name, role);
    this.all.push({ name, role, client });
    return client;
  }

  /** Subscribers first (stop inbound events), then blocking/workers, then commands. */
  async close(timeoutMs = 5_000) {
    const order: Role[] = ["subscriber", "blocking", "worker", "commands"];
    for (const role of order) {
      await Promise.all(
        this.all.filter((c) => c.role === role).map((c) => closeOne(c.client, timeoutMs))
      );
    }
  }

  async health() {
    const out: Record<string, boolean> = {};
    await Promise.all(this.all.map(async ({ name, client }) => {
      out[name] = client.status === "ready" && (await client.ping().catch(() => "")) === "PONG";
    }));
    return out;
  }
}

async function closeOne(client: Redis, timeoutMs: number) {
  if (client.status === "end") return;
  const timer = setTimeout(() => client.disconnect(), timeoutMs);   // force after the deadline
  try {
    await client.quit();
  } catch {
    client.disconnect();
  } finally {
    clearTimeout(timer);
  }
}
```

Usage:

```ts
const connections = new RedisConnections();
const redis = connections.create("app");
const subscriber = connections.create("events", "subscriber");

// on shutdown
await connections.close();
```

Blocking loops need their own stop flag first (see [Graceful Shutdown](../03_ioredis-basics/06_graceful-shutdown.md)) so `quit()` isn't waiting on a long `BLPOP`.

## Startup: fail fast, or start degraded?

Decide deliberately:

```ts
// Redis is REQUIRED (queues, locks, sessions): refuse to start without it
await redis.ping();

// Redis is OPTIONAL (pure cache): start anyway and let it connect in the background
redis.ping().catch(() => console.warn("Redis unavailable, running without cache"));
```

For required Redis, use `lazyConnect: true`, then `await redis.connect()` with a timeout during boot, and exit with a clear error.

## Short-lived processes

| Environment | Guidance |
|-------------|----------|
| **CLI scripts, cron jobs** | Create, use, `await quit()` in `finally`, otherwise the process hangs on the open socket |
| **Serverless (Lambda, Cloud Functions)** | Create the client **outside** the handler so warm invocations reuse it. Use `lazyConnect`, low timeouts, and beware connection counts multiplying with concurrency. Consider a proxy or a managed serverless-friendly endpoint |
| **Next.js and similar dev servers** | Cache on `globalThis` (above) |
| **Tests** | Create per test file, `quit()` in `afterAll`, use a dedicated DB index or key prefix |

## Testing connections

```ts
// vitest / jest style
let redis: Redis;

beforeAll(() => { redis = createClient("test", "commands", { db: 15 }); });
afterEach(async () => { await redis.flushdb(); });          // only ever on the test DB
afterAll(async () => { await redis.quit(); });
```

Guard `flushdb` in test helpers: refuse to run unless the DB index or host is the test one. More in `18_testing-and-debugging`.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| A new client per request or per call | One shared client, created at startup |
| Sharing one client for commands, subscribe and blocking | One connection per role |
| Same options for all roles | Role-specific timeouts and retries |
| Connections created at import time in tests and scripts | `lazyConnect`, or create inside `main()` |
| Leaked connections on hot reload | `globalThis` cache in development |
| No connection names | `connectionName` on every client |
| Topology logic spread across the code | One factory |
| Shutdown closes the commands client before the subscriber and workers | Ordered close |
| Ignoring provider connection limits | Budget `instances × connections` |

## Key takeaways

- One commands connection per process, plus one per special role
- Centralize creation in a factory with role-specific options and names
- Manage lifecycle with a single manager that closes connections in order
- Hide topology (standalone, Sentinel, Cluster) behind the factory

**Next:** [Redis Service](./02_redis-service.md)
