# Error Handling

Redis can fail in several distinct ways. Knowing which one you are looking at tells you whether to retry, fall back, or alert.

## Types of errors

### 1. Connection errors (emitted as `error` events)

Network-level failures that ioredis handles by reconnecting:

| Error code | Meaning |
|-----------|---------|
| `ECONNREFUSED` | Nothing listening on that host/port |
| `ETIMEDOUT` | Connection attempt or socket timed out |
| `ECONNRESET` | Server closed the connection |
| `ENOTFOUND` | DNS lookup failed |

```ts
redis.on("error", (err: NodeJS.ErrnoException) => {
  logger.error({ code: err.code, msg: err.message }, "redis connection error");
});
```

These do not reject your command promises immediately. Commands are queued and retried per your configuration.

### 2. Command errors (server replies with an error)

The promise **rejects** with a `ReplyError`. The message begins with an error code:

| Prefix | Meaning | Typical action |
|--------|---------|----------------|
| `WRONGTYPE` | Wrong data type for the command | Fix the bug |
| `ERR ...` | Syntax or argument problem | Fix the bug |
| `NOAUTH` / `WRONGPASS` | Missing or bad credentials | Fix config, alert |
| `NOPERM` | ACL forbids the command or key | Fix permissions |
| `OOM` | Out of memory under `noeviction` | Shed load, alert |
| `READONLY` | Wrote to a replica (often after failover) | Reconnect, see `reconnectOnError` |
| `LOADING` | Server still loading the dataset | Retry later |
| `BUSY` | A long script is running | Retry later |
| `NOSCRIPT` | Script SHA isn't cached | Reload the script |
| `MOVED` / `ASK` / `CLUSTERDOWN` | Cluster routing problems | Handled by `Cluster`, alert on `CLUSTERDOWN` |

```ts
try {
  await redis.lpush("string-key", "x");
} catch (err: any) {
  if (err.message.startsWith("WRONGTYPE")) {
    // programmer error, log loudly
  }
  throw err;
}
```

### 3. Client-side errors

| Error | Cause |
|-------|-------|
| `MaxRetriesPerRequestError` | Redis unreachable longer than `maxRetriesPerRequest` allows |
| `Command timed out` | `commandTimeout` exceeded |
| `Connection is closed.` | Command sent after `quit()` or `end` |
| `Stream isn't writeable and enableOfflineQueue options is false` | Offline queue disabled while disconnected |

```ts
try {
  await redis.get("k");
} catch (err: any) {
  if (err.name === "MaxRetriesPerRequestError") {
    // Redis is down: fall back or return 503
  }
}
```

## The golden rules

1. **Always attach an `error` listener** to every client (including duplicates and subscribers)
2. **Always `await` or `.catch()`** every command
3. **Decide per call site** whether Redis failure is fatal, degradable, or ignorable
4. **Don't retry non-idempotent commands blindly** (`INCR`, `LPUSH`, `XADD`). A timeout doesn't mean the command didn't run

## Pattern: fail open for caches

If Redis is only a cache, a Redis outage should slow the app down, not break it:

```ts
async function getUser(id: string) {
  const key = `cache:user:${id}`;

  try {
    const hit = await redis.get(key);
    if (hit) return JSON.parse(hit);
  } catch (err) {
    logger.warn({ err }, "cache read failed, using database");
  }

  const user = await db.users.findById(id);

  redis.set(key, JSON.stringify(user), "EX", 300)
    .catch((err) => logger.warn({ err }, "cache write failed"));

  return user;
}
```

Pair this with a low `commandTimeout` so a sick Redis doesn't add seconds of latency per request.

## Pattern: fail closed for correctness

For locks, rate limiting on abuse-prone endpoints, or idempotency, decide deliberately:

```ts
async function allowRequest(ip: string): Promise<boolean> {
  try {
    const n = await redis.incr(`rate:${ip}`);
    return n <= 100;
  } catch (err) {
    logger.error({ err }, "rate limiter unavailable");
    return true;   // fail open (favor availability) OR return false (favor protection)
  }
}
```

There is no universal answer. Write the decision down.

## Pattern: retry transient errors

Retry only **idempotent** operations and only for transient conditions:

```ts
async function withRetry<T>(fn: () => Promise<T>, attempts = 3, baseMs = 50): Promise<T> {
  let lastErr: unknown;
  for (let i = 0; i < attempts; i++) {
    try {
      return await fn();
    } catch (err: any) {
      lastErr = err;
      const transient =
        err.message?.startsWith("LOADING") ||
        err.message?.startsWith("BUSY") ||
        err.message?.startsWith("READONLY") ||
        err.name === "MaxRetriesPerRequestError";
      if (!transient) throw err;
      await new Promise((r) => setTimeout(r, baseMs * 2 ** i + Math.random() * baseMs));
    }
  }
  throw lastErr;
}

const value = await withRetry(() => redis.get("k"));
```

## Pipeline and transaction errors

`exec()` **resolves** even when individual commands fail. Each result is an `[error, result]` pair:

```ts
const results = await redis
  .pipeline()
  .set("a", "1")
  .lpush("a", "x")       // WRONGTYPE
  .get("a")
  .exec();

results?.forEach(([err, res], i) => {
  if (err) console.error(`command ${i} failed:`, err.message);
  else console.log(`command ${i}:`, res);
});
```

Rules:

- A pipeline's `exec()` rejects only for connection-level problems
- A `multi().exec()` returns `null` when a `WATCH`ed key changed. Treat that as "retry the transaction"
- Always inspect the per-command errors

## Errors in event handlers

Errors thrown inside `message` handlers on a subscriber are your code's errors, not Redis's. Wrap them:

```ts
subscriber.on("message", async (channel, message) => {
  try {
    await handle(JSON.parse(message));
  } catch (err) {
    logger.error({ err, channel }, "handler failed");
  }
});
```

## Process-level safety net

```ts
process.on("unhandledRejection", (reason) => {
  logger.error({ reason }, "unhandled rejection");
});
```

Use this to **log**, not to hide bugs. Fix the root cause.

## Health, alerting, and observability

Alert on:

- Repeated `reconnecting` or `end` events
- Spikes in `MaxRetriesPerRequestError` or `Command timed out`
- `OOM`, `READONLY`, `NOAUTH` errors
- Latency growth on a simple `PING` probe

## Common mistakes

| Mistake | Consequence | Fix |
|---------|-------------|-----|
| No `error` listener | Silent failures | Attach on every client |
| Empty `catch {}` | Hidden data loss | Log or rethrow |
| Blind retry on `INCR` | Double counting | Retry only idempotent commands |
| Ignoring pipeline per-command errors | Partial failures unnoticed | Check each `[err, res]` |
| Infinite retries in the request path | Hung requests | Low `maxRetriesPerRequest` and `commandTimeout` |
| Same policy for cache and locks | Wrong trade-off | Decide per use case |

## Key takeaways

- Distinguish connection errors, server replies and client-side errors
- Fail open for caches, decide explicitly for correctness-critical uses
- `exec()` resolves with per-command errors. Always inspect them
- Combine low timeouts, bounded retries and good logging

**Next:** [Graceful Shutdown](./06_graceful-shutdown.md)
