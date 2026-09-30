# Graceful Shutdown

When your process receives a stop signal (deploy, scale-down, `Ctrl+C`), you want in-flight work to finish and connections to close cleanly, not to drop commands or hang forever.

## `quit()` vs `disconnect()`

| Method | Behavior | Use when |
|--------|----------|----------|
| `await redis.quit()` | Sends `QUIT`, waits for pending replies, then closes | **Normal shutdown** |
| `redis.disconnect()` | Closes the socket immediately, no reconnect | Forced shutdown, after a timeout |
| `redis.disconnect(true)` | Closes and reconnects | Rare (force a reconnect) |

`quit()` is graceful because commands already sent get their replies first.

```ts
await redis.quit();     // resolves with "OK" when closed cleanly
```

Calling `quit()` on an already closed client can reject with "Connection is closed." Guard for it:

```ts
async function closeRedis(client: Redis) {
  if (client.status === "end") return;
  try {
    await client.quit();
  } catch {
    client.disconnect();
  }
}
```

## Scripts

Scripts must close their connection, or Node stays alive because of the open socket:

```ts
async function main() {
  await redis.set("k", "v");
  console.log(await redis.get("k"));
}

main()
  .catch(console.error)
  .finally(() => redis.quit());
```

## Servers: the right order

For an Express (or any HTTP) server:

1. Stop accepting **new** requests
2. Let **in-flight** requests finish (they may still use Redis)
3. Close **workers and subscribers**
4. Close **Redis**
5. Exit

```ts
import http from "node:http";
import express from "express";
import { redis, subscriber } from "./redis/clients.js";

const app = express();
const server = http.createServer(app).listen(3000);

let shuttingDown = false;

async function shutdown(signal: string) {
  if (shuttingDown) return;           // ignore repeated signals
  shuttingDown = true;
  console.log(`${signal} received, shutting down`);

  // Hard deadline so we never hang
  const force = setTimeout(() => {
    console.error("forced exit");
    redis.disconnect();
    subscriber.disconnect();
    process.exit(1);
  }, 10_000);
  force.unref();

  try {
    // 1 + 2: stop new connections, wait for active ones
    await new Promise<void>((resolve, reject) =>
      server.close((err) => (err ? reject(err) : resolve()))
    );

    // 3: subscribers
    await subscriber.unsubscribe();
    await subscriber.quit();

    // 4: main client
    await redis.quit();

    console.log("clean shutdown");
    process.exit(0);
  } catch (err) {
    console.error("shutdown error", err);
    process.exit(1);
  }
}

process.on("SIGTERM", () => shutdown("SIGTERM"));  // Docker, Kubernetes, PM2
process.on("SIGINT", () => shutdown("SIGINT"));    // Ctrl+C
```

Notes:

- `server.close()` stops new connections but waits for open ones. Keep-alive connections can delay it. Node 18.2+ offers `server.closeIdleConnections()`, and Node 19+ closes idle connections in `close()`
- Mark the app **not ready** in your readiness probe as soon as shutdown starts, so load balancers stop sending traffic
- Keep the hard deadline below your platform's grace period (Kubernetes defaults to 30 seconds)

## Health endpoint that respects shutdown

```ts
app.get("/ready", async (_req, res) => {
  if (shuttingDown) return res.status(503).send("shutting down");
  const ok = redis.status === "ready" && (await redis.ping()) === "PONG";
  res.status(ok ? 200 : 503).send(ok ? "ok" : "redis unavailable");
});
```

## Subscribers

Unsubscribe before quitting so the server cleans up subscriptions right away:

```ts
await subscriber.unsubscribe();
await subscriber.punsubscribe();
await subscriber.quit();
```

## Blocking readers and workers

A loop using `BLPOP` or `XREADGROUP BLOCK` needs a way to stop:

```ts
let running = true;

async function worker() {
  while (running) {
    const res = await blocker.blpop("jobs", 2);   // short timeout so we can check `running`
    if (!res) continue;
    await processJob(res[1]);
  }
}

async function stopWorker() {
  running = false;          // loop exits after at most ~2 seconds
  await workerDone;         // the promise returned by worker()
  await blocker.quit();
}
```

Use short block timeouts (1 to 5 seconds) so shutdown isn't delayed by a long-blocking call.

If you must interrupt a blocked connection immediately, call `blocker.disconnect()`.

## Pending pipelines and auto pipelining

`quit()` waits for commands that are already sent. Await your own pending work first (`await Promise.all(pending)`) so nothing is queued but unsent when you close.

## Kubernetes and Docker specifics

- Kubernetes sends `SIGTERM`, waits `terminationGracePeriodSeconds`, then sends `SIGKILL`
- Docker `stop` sends `SIGTERM`, then `SIGKILL` after 10 seconds by default
- Make sure PID 1 is your Node process (use `node dist/index.js` directly or a proper init like `tini`). Wrapping in `npm start` can swallow signals

```dockerfile
CMD ["node", "dist/index.js"]
```

## Releasing locks and state

If your process holds distributed locks or claims work, release them during shutdown (step 3), not after Redis is closed:

```ts
await releaseLock(lockKey, token);
await redis.quit();
```

Locks with a TTL will expire on their own if you crash, which is exactly why locks always need one.

## Testing your shutdown

```bash
node dist/index.js &
kill -TERM $!
```

Confirm that the process exits with code 0, logs a clean shutdown, and that in-flight requests complete.

## Common mistakes

| Mistake | Consequence | Fix |
|---------|-------------|-----|
| No signal handlers | Abrupt kill, dropped requests | Handle `SIGTERM` and `SIGINT` |
| Closing Redis before the HTTP server | In-flight requests fail | Close the server first |
| `disconnect()` for normal shutdown | Pending replies lost | Use `quit()` |
| No hard timeout | Process hangs, then gets `SIGKILL`ed | Add a force-exit timer |
| Long `BLPOP` timeouts | Slow shutdown | Short timeouts plus a `running` flag |
| Running via `npm start` in Docker | Signals not forwarded | `CMD ["node", ...]` |
| Handling the signal twice | Double close errors | `shuttingDown` guard |

## Key takeaways

- `quit()` for graceful, `disconnect()` as the fallback
- Order matters: stop traffic, drain work, release locks, close subscribers, then close Redis
- Always add a hard deadline and a shutdown guard
- Make readiness fail during shutdown so traffic drains first

**Next module:** [04_data-structures](../04_data-structures/README.md)
