# Configuration

ioredis defaults suit development. Production needs deliberate choices about retries, timeouts, offline behavior and security.

## Essential options

| Option | Default | Purpose |
|--------|---------|---------|
| `host` / `port` | `127.0.0.1` / `6379` | Server address |
| `path` | none | Unix socket path |
| `username` / `password` | none | Authentication (ACL user or `requirepass`) |
| `db` | `0` | Logical database |
| `connectionName` | none | Name shown in `CLIENT LIST` |
| `connectTimeout` | `10000` | ms to establish the socket |
| `commandTimeout` | none | ms before a command rejects |
| `keepAlive` | `0` | TCP keepalive initial delay (ms) |
| `noDelay` | `true` | Disable Nagle's algorithm |
| `family` | `4` | IP version (`4` or `6`) |
| `keyPrefix` | none | Prefix added to command keys |
| `lazyConnect` | `false` | Don't connect until told |
| `tls` | none | TLS options (`{}` for defaults) |
| `retryStrategy` | backoff to 2s | Delay before reconnect attempts |
| `reconnectOnError` | none | Reconnect on specific server errors |
| `maxRetriesPerRequest` | `20` | Retries before a command fails (`null` = forever) |
| `enableOfflineQueue` | `true` | Queue commands while disconnected |
| `enableReadyCheck` | `true` | Wait for the server to finish loading |
| `autoResubscribe` | `true` | Restore subscriptions after reconnect |
| `autoResendUnfulfilledCommands` | `true` | Resend commands that had no reply |
| `enableAutoPipelining` | `false` | Batch commands issued in the same tick |
| `disconnectTimeout` | `2000` | Wait time on `disconnect` |

Check exact defaults for your installed version in the ioredis README, since they can change between releases.

## Retry strategy

Called each time a reconnect is needed, with the attempt count:

```ts
const redis = new Redis({
  retryStrategy(times) {
    if (times > 20) return null;                        // give up, emits "end"
    const delay = Math.min(times * 200, 5000);
    const jitter = Math.floor(Math.random() * 200);     // avoid thundering herd
    return delay + jitter;
  },
});
```

- Return a **number**: retry after that many ms
- Return a **non-number** (`null`, `false`): stop reconnecting
- Add **jitter** so hundreds of app instances don't reconnect at the same instant

## `maxRetriesPerRequest`

How many times a **single command** is retried across reconnects before failing with `MaxRetriesPerRequestError`.

```ts
new Redis({ maxRetriesPerRequest: 3 });     // fail fast in web requests
new Redis({ maxRetriesPerRequest: null });  // retry forever (required by BullMQ workers)
```

Lower it for request/response paths so users aren't left waiting while Redis is down.

## Offline queue

```ts
new Redis({ enableOfflineQueue: false });
```

| Setting | Behavior while disconnected | Good for |
|---------|-----------------------------|----------|
| `true` (default) | Commands wait and run after reconnect | Background jobs, startup sequences |
| `false` | Commands **fail immediately** | APIs that should fail fast or fall back to the database |

Caveat: with `false`, commands sent **before the first connection** are also rejected ("Stream isn't writeable and enableOfflineQueue options is false"). Wait for `ready` first.

## Timeouts

```ts
new Redis({
  connectTimeout: 5000,     // give up on a hanging TCP connect
  commandTimeout: 2000,     // reject slow commands ("Command timed out")
  keepAlive: 10000,         // detect dead peers
});
```

`commandTimeout` rejects the promise on the client side. The server may still execute the command, so be careful with non-idempotent writes.

## `reconnectOnError`

Reconnect when the server returns a specific error, typically after a failover when a replica was promoted or demoted:

```ts
new Redis({
  reconnectOnError(err) {
    if (err.message.includes("READONLY")) {
      return 2;  // reconnect AND resend the failed command
    }
    return false;
  },
});
```

| Return | Effect |
|--------|--------|
| `false` / `0` | Do nothing |
| `true` / `1` | Reconnect |
| `2` | Reconnect and resend the failed command |

## Authentication

```ts
new Redis({
  username: "app",                        // ACL user (Redis 6+)
  password: process.env.REDIS_PASSWORD,
});
```

Legacy `requirepass` setups use only `password`.

## TLS

```ts
import fs from "node:fs";

new Redis({
  host: "redis.example.com",
  port: 6380,
  tls: {
    ca: fs.readFileSync("/certs/ca.crt"),
    servername: "redis.example.com",       // for SNI and hostname verification
  },
});

new Redis("rediss://user:pass@redis.example.com:6380"); // note the double "s"
```

Use `tls: {}` for servers with publicly trusted certificates. Never disable certificate verification in production. See `17_security/02_tls.md`.

## Key prefix

```ts
const redis = new Redis({ keyPrefix: "shop:" });
await redis.set("user:1", "Ada"); // stores "shop:user:1"
```

Remember the caveats from `02_redis-fundamentals/01_keys-and-values.md`: results, `SCAN` patterns and Pub/Sub channels aren't prefixed automatically.

## Environment-based configuration

```ts
// src/redis/options.ts
import type { RedisOptions } from "ioredis";

const base: RedisOptions = {
  host: process.env.REDIS_HOST ?? "127.0.0.1",
  port: Number(process.env.REDIS_PORT ?? 6379),
  username: process.env.REDIS_USERNAME || undefined,
  password: process.env.REDIS_PASSWORD || undefined,
  db: Number(process.env.REDIS_DB ?? 0),
  connectionName: process.env.SERVICE_NAME ?? "app",
};

export const devOptions: RedisOptions = { ...base };

export const prodOptions: RedisOptions = {
  ...base,
  connectTimeout: 5000,
  commandTimeout: 3000,
  keepAlive: 10000,
  maxRetriesPerRequest: 3,
  retryStrategy: (n) => Math.min(n * 200, 5000) + Math.floor(Math.random() * 200),
  reconnectOnError: (err) => (err.message.includes("READONLY") ? 2 : false),
  tls: process.env.REDIS_TLS === "true" ? {} : undefined,
};

export const redisOptions: RedisOptions =
  process.env.NODE_ENV === "production" ? prodOptions : devOptions;
```

```ts
import { Redis } from "ioredis";
import { redisOptions } from "./options.js";

export const redis = new Redis(redisOptions);
```

## Presets by workload

| Workload | Suggested tweaks |
|----------|------------------|
| **Web API cache** | `maxRetriesPerRequest: 1-3`, `commandTimeout: 100-500`, consider `enableOfflineQueue: false`, fall back to the database |
| **Background worker / BullMQ** | `maxRetriesPerRequest: null`, generous retry strategy |
| **Pub/Sub subscriber** | Default `autoResubscribe: true`, an `error` listener, dedicated connection |
| **Serverless** | `lazyConnect: true`, reuse the client across invocations, low timeouts |

## Inspecting effective options

```ts
console.log(redis.options.host, redis.options.retryStrategy?.name);
```

Never log the whole `options` object because it contains the password.

## Key takeaways

- Tune `retryStrategy`, `maxRetriesPerRequest` and `commandTimeout` to your failure policy
- Fail fast for user-facing paths, retry patiently for background work
- Use `reconnectOnError` to recover from failovers
- Keep secrets in environment variables and use TLS outside localhost

**Next:** [Commands and Options](./03_commands-and-options.md)
