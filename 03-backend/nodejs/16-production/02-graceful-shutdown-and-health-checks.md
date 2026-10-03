# Graceful Shutdown & Health Checks

How your app should behave when the platform says "stop" (every deploy does), and how the platform knows whether your app is alive and ready for traffic.

## Why the lifecycle matters

In production, processes are **started and stopped constantly**: every deploy, every autoscaling event, every node replacement, every crash. If your app handles these moments badly, each one drops requests:

```
Deploy starts → platform sends SIGTERM → app dies instantly
                                           │
                       in-flight requests: connection reset  → users see errors
                       half-finished jobs:  abandoned         → duplicate or lost work
                       open DB transactions: aborted          → possible inconsistency
```

Two halves make deploys invisible to users:

1. **Graceful shutdown:** when told to stop, finish what you're doing, refuse new work, then exit.
2. **Health checks:** tell the platform *whether you're alive* (restart me if not) and *whether you're ready* (send me traffic or not).

---

## Signals: how the platform talks to your process

| Signal | Sent by | Meaning | Can you catch it? |
|---|---|---|---|
| **`SIGTERM`** | Docker (`docker stop`), Kubernetes, ECS, systemd, PM2 | "Please shut down" | ✅ Yes, and you should |
| **`SIGINT`** | You pressing Ctrl+C | Same, interactively | ✅ Yes |
| **`SIGKILL`** | The platform, after a **grace period** expires | "Die now" | ❌ **No**, the OS kills you |
| `SIGHUP` | Terminal closed; sometimes "reload config" | Varies | ✅ |

The sequence on every deploy:

```
t=0s    Platform sends SIGTERM  ──▶ your handler runs: stop accepting, drain, close
t=0..N  Grace period (Docker default 10 s; Kubernetes 30 s; ECS 30 s, configurable)
t=N     Anything still running gets SIGKILL: instant death, no cleanup
```

**Node.js does *not* exit on `SIGTERM` by default if you've registered a handler**, and *does* exit immediately (with no cleanup) if you haven't. Either way, you must handle it deliberately.

---

## The shutdown sequence

A well-behaved shutdown does these steps **in order**:

```
1. Mark as "not ready"        → readiness probe starts failing, so the load balancer stops sending new requests
2. Wait a moment              → let the LB/endpoint list actually update (a few seconds)
3. Stop accepting connections → server.close()
4. Drain in-flight requests   → let them finish (with a deadline)
5. Stop background work       → queue workers, schedulers, consumers (11-async-processing/)
6. Close resources            → DB pool, Redis, Kafka, OpenTelemetry flush, log flush
7. Exit with code 0           → (or 1 if forced / error)
```

Doing step 6 before step 4 is the classic bug: you close the database while requests that need it are still running.

---

## Implementation

### The HTTP server

```js
// src/server.js
import http from "node:http";
import { app } from "./app.js";
import { config } from "./config/env.js";
import { logger } from "./config/logger.js";
import { pool } from "./db/pool.js";
import { redis } from "./lib/redis.js";

const server = http.createServer(app);

// ---- state shared with the health endpoints ----
export const state = { shuttingDown: false };

server.listen(config.PORT, config.HOST, () => {
  logger.info({ port: config.PORT, version: config.APP_VERSION }, "Server listening");
});

// ---- graceful shutdown ----
let shutdownPromise;

function shutdown(signal) {
  // idempotent: a second signal (or Ctrl+C twice) must not start a second shutdown
  shutdownPromise ??= (async () => {
    logger.info({ signal }, "Shutdown started");
    state.shuttingDown = true;                                    // 1. readiness now returns 503

    // Safety net: never hang forever; leave margin before the platform's SIGKILL
    const forceTimer = setTimeout(() => {
      logger.error("Graceful shutdown timed out; forcing exit");
      process.exit(1);
    }, config.SHUTDOWN_TIMEOUT_MS ?? 25_000);
    forceTimer.unref();

    try {
      // 2. give the load balancer time to notice we're unready
      await sleep(config.SHUTDOWN_DELAY_MS ?? 5_000);

      // 3 + 4. stop accepting new connections; resolves when in-flight requests have finished
      await new Promise((resolve, reject) => {
        server.close((err) => (err ? reject(err) : resolve()));
        server.closeIdleConnections();                            // Node 18.2+: drop idle keep-alive sockets so close() can finish
      });

      // 5. stop background work (workers, schedulers, consumers) if this process runs any
      // await Promise.all(workers.map((w) => w.close()));

      // 6. close resources AFTER requests are done
      await Promise.allSettled([pool.end(), redis.quit()]);
      await flushTelemetry();                                      // logs, OpenTelemetry spans (14-logging-observability/)

      logger.info("Shutdown complete");
      process.exit(0);
    } catch (err) {
      logger.error({ err }, "Error during shutdown");
      process.exit(1);
    }
  })();
  return shutdownPromise;
}

process.on("SIGTERM", () => shutdown("SIGTERM"));
process.on("SIGINT", () => shutdown("SIGINT"));

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
```

### Why `server.close()` alone isn't enough

`server.close()` stops accepting **new** connections and its callback fires when **all existing connections have ended**. But with HTTP **keep-alive**, a connection can sit idle for a long time after its last request, so `close()` may wait (up to the keep-alive timeout) for nothing.

| Method (Node 18.2+/19+) | Effect |
|---|---|
| `server.close()` | Stop listening; wait for connections to end |
| `server.closeIdleConnections()` | Immediately close connections that aren't handling a request |
| `server.closeAllConnections()` | Forcibly destroy **all** connections, including in-flight (last resort, after your deadline) |

Recent Node versions' `server.close()` closes idle connections itself; calling `closeIdleConnections()` explicitly is harmless and makes the behavior obvious. Also tell clients to stop reusing the connection once you're shutting down:

```js
app.use((req, res, next) => {
  if (state.shuttingDown) res.set("Connection", "close");        // "don't reuse this connection for the next request"
  next();
});
```

Libraries such as **`http-terminator`** and **`stoppable`** package this logic (including a hard deadline) if you'd rather not hand-roll it.

### The deadline ladder

Your timeouts must **nest correctly**, from the platform's outermost kill down to your innermost step:

```
Platform grace period     30 s   (Kubernetes terminationGracePeriodSeconds / ECS stopTimeout / docker stop -t)
  └─ your force-exit timer 25 s   (leaves margin so YOU exit rather than get SIGKILLed mid-cleanup)
       └─ LB drain delay    5 s   (step 2)
       └─ in-flight drain  ≤ 15 s (slow requests get this long)
       └─ resource closing ≤ 5 s
```

If your slowest legitimate request takes 20 seconds, the grace period must be longer than that. If it can't be, those requests **will** be cut off on deploy, so make long operations asynchronous jobs instead (`11-async-processing/`).

### Workers, consumers, and schedulers

Anything that pulls work must **stop pulling first**, then finish what it has (`11-async-processing/02-workers-retry-dlq.md` has the full pattern):

```js
await worker.close();                    // BullMQ: stops taking jobs, waits for active ones
await consumer.disconnect();             // Kafka: leave the group cleanly (fast rebalance)
scheduledTask.stop();                    // cron
io.close();                              // Socket.IO: disconnect clients (jittered; see 12-realtime/03)
```

### Crashes are different from shutdowns

```js
process.on("unhandledRejection", (reason) => {
  logger.fatal({ err: reason }, "Unhandled promise rejection");
  shutdown("unhandledRejection").finally(() => process.exit(1));
});

process.on("uncaughtException", (err) => {
  logger.fatal({ err }, "Uncaught exception");
  process.exit(1);                       // state is unknown: do NOT keep serving; let the orchestrator restart us
});
```

After an `uncaughtException`, the process may be in an inconsistent state. Log, flush logs, and **exit**, relying on the platform to start a fresh instance (`09-api-development/05-error-responses.md`). The `logger.flush()` (or a synchronous log write) matters here: the crash line is the one you most want to see.

---

## The PID 1 problem (Docker)

In a container, your app is **process ID 1**, which has special signal-handling rules in Linux, and the way you start it decides whether `SIGTERM` ever reaches Node.

```dockerfile
# ❌ npm becomes PID 1, receives SIGTERM, and does NOT forward it to node → 10 s later: SIGKILL
CMD ["npm", "start"]

# ❌ shell form: /bin/sh -c "node server.js": the shell is PID 1 and may not forward signals
CMD node src/server.js

# ✅ exec form, running node directly as the entrypoint
CMD ["node", "src/server.js"]
```

Even then, PID 1 has two quirks: the kernel doesn't apply default signal actions to it, and **zombie processes** (children whose parents never reap them) accumulate. An init process fixes both:

```bash
docker run --init my-app                  # Docker's built-in tini
```

```dockerfile
# or bake tini into the image
RUN apk add --no-cache tini               # (or apt-get install tini on Debian-based images)
ENTRYPOINT ["/sbin/tini", "--"]
CMD ["node", "src/server.js"]
```

In Compose: `init: true`. Kubernetes' `shareProcessNamespace` or the pause container handles some of this, but the exec-form `CMD` rule still applies. See `03-docker-and-compose.md`.

**Test it:** `docker stop <container>` should return in about a second (your handler ran and exited), not after exactly 10 seconds (it got SIGKILLed).

---

## Zero-downtime deploys in practice

A **rolling deploy** replaces instances gradually:

```
Before:   [v1] [v1] [v1]
Step 1:   [v1] [v1] [v1] [v2]          start a v2, wait until it's READY
Step 2:   [v1] [v1] (draining) [v2]    stop sending traffic to a v1, SIGTERM it, wait for drain
Step 3:   [v1] [v2] [v2]  ... and so on until all are v2
```

For this to be invisible to users, three things must be true:

1. **New instances only receive traffic once ready** → readiness probes (below)
2. **Old instances stop receiving traffic *before* they stop serving** → fail readiness first, delay, then drain
3. **v1 and v2 can run side by side** → backward-compatible database changes and API contracts (`05-ci-cd.md`)

### The endpoint-removal race (Kubernetes)

When a pod terminates, two things happen **concurrently**: the kubelet sends `SIGTERM`, and the control plane removes the pod from the Service's endpoints. Removal takes a moment to propagate, so for a few seconds after `SIGTERM`, traffic can still arrive. If you shut down instantly, those requests fail.

Fixes: the **drain delay** in step 2 above, and/or a `preStop` hook:

```yaml
lifecycle:
  preStop:
    exec: { command: ["sleep", "5"] }         # keep serving while endpoints update; then SIGTERM arrives
terminationGracePeriodSeconds: 40              # must exceed preStop + your shutdown time
```

(On ECS behind an ALB, the equivalent is the target group's **deregistration delay**, with a matching `stopTimeout`: `06-aws.md`.)

---

## Health checks

A health check is an endpoint the platform polls to decide what to do with your instance. There are **three different questions**, and conflating them causes outages.

| Probe | Question | If it fails | Typical endpoint |
|---|---|---|---|
| **Liveness** | "Is this process **alive and not stuck**?" | **Restart** the container | `/healthz` |
| **Readiness** | "Can this instance **serve traffic right now**?" | **Stop sending traffic** (don't restart) | `/readyz` |
| **Startup** | "Has the app **finished starting**?" | Keep waiting; liveness/readiness don't run until it passes | `/healthz` or `/readyz` |

### Liveness: keep it minimal

Liveness answers only: *is the process responsive?* It should **not** depend on external services.

```js
app.get("/healthz", (req, res) => res.status(200).json({ status: "ok" }));
```

**Why not check the database in liveness?** Imagine the database has a 30-second blip. Every instance's liveness check fails, the platform **restarts every instance at once**, the restarts hammer the recovering database with reconnections, and a short blip becomes a long outage. Restarting your app can't fix a database problem. The liveness check exists to catch a *wedged process* (deadlock, event loop permanently blocked), and "can I answer an HTTP request at all" is exactly that test.

### Readiness: can I do useful work?

Readiness checks whether this instance *should receive traffic now*. It's appropriate to check things the instance needs in order to serve requests, and to report unready when shutting down.

```js
// src/routes/health.js
import { Router } from "express";
import { pool } from "../db/pool.js";
import { redis } from "../lib/redis.js";
import { state } from "../server.js";

const router = Router();

const withTimeout = (promise, ms, name) =>
  Promise.race([
    promise,
    new Promise((_, reject) => setTimeout(() => reject(new Error(`${name} check timed out`)), ms)),
  ]);

async function checkDependencies() {
  const checks = {
    database: withTimeout(pool.query("SELECT 1"), 1000, "database"),
    redis: withTimeout(redis.ping(), 1000, "redis"),
  };
  const results = await Promise.allSettled(Object.values(checks));
  return Object.fromEntries(
    Object.keys(checks).map((name, i) => [name, results[i].status === "fulfilled" ? "ok" : "fail"])
  );
}

router.get("/healthz", (req, res) => res.status(200).json({ status: "ok" }));          // liveness

router.get("/readyz", async (req, res) => {                                             // readiness
  if (state.shuttingDown) {
    return res.status(503).json({ status: "shutting_down" });                            // fail FIRST on shutdown
  }
  const deps = await checkDependencies();
  const ready = Object.values(deps).every((v) => v === "ok");
  res.status(ready ? 200 : 503).json({ status: ready ? "ready" : "not_ready", checks: deps, version: process.env.APP_VERSION });
});

export default router;
```

Rules for readiness checks:

- **Always use timeouts** on dependency checks (1–2 s). A hung health check is worse than a failing one.
- **Check only what this instance needs** to serve requests. If Redis is only used for an optional cache you gracefully degrade without, don't make it block readiness.
- **Be careful with shared dependencies.** If the database goes down and *all* instances fail readiness, the load balancer has no healthy targets and returns errors to everyone, which is correct if you truly can't serve, but not if most endpoints work without the database. Decide deliberately which dependencies are *required*. Some teams keep readiness "local" (process-level) and surface dependency problems via metrics and alerts (`14-logging-observability/`) instead.
- **Keep it cheap.** Probes run every few seconds from several sources: a heavy health query is a self-inflicted load problem. Cache results for a second or two if needed.
- **Don't expose internals publicly.** The detailed JSON (`checks`, versions) is useful to operators but shouldn't leak to the internet: serve the detail only on an internal port or behind auth, and return a bare `200`/`503` publicly (`04-nginx.md`).
- **Exclude health endpoints** from access logs, metrics, tracing, and rate limiting (`14-logging-observability/`, `08-authentication-security/06-rate-limiting.md`). Otherwise probes swamp your data and an overzealous limiter can mark you unhealthy.
- **No authentication** on the probe endpoints: the platform's probe can't log in.

### Startup: slow boots

If your app takes a while to start (large migrations, cache warming, loading models), a liveness probe that starts too early will restart it in a loop. Use a **startup probe** (Kubernetes) or a **start period** (Docker/ECS) to give it time before health checks count:

```yaml
startupProbe:
  httpGet: { path: /healthz, port: 3000 }
  failureThreshold: 30          # 30 × 2 s = up to 60 s to start
  periodSeconds: 2
livenessProbe:
  httpGet: { path: /healthz, port: 3000 }
  periodSeconds: 10
  timeoutSeconds: 2
  failureThreshold: 3           # 3 consecutive failures → restart
readinessProbe:
  httpGet: { path: /readyz, port: 3000 }
  periodSeconds: 5
  timeoutSeconds: 2
  failureThreshold: 2
```

Better still: **listen only once you're ready** (or report not-ready until initialization is done), and keep startup fast, since fast starts make scaling and recovery quicker.

```js
await runStartupChecks();           // connect to DB, verify migrations are applied, warm caches
state.ready = true;                 // /readyz returns 200 only after this
server.listen(config.PORT);
```

### Docker `HEALTHCHECK`

For plain Docker and Compose (`03-docker-and-compose.md`), define the check in the Dockerfile or Compose file. Slim images often lack `curl`, so use Node itself:

```dockerfile
HEALTHCHECK --interval=15s --timeout=3s --start-period=20s --retries=3 \
  CMD node -e "fetch('http://127.0.0.1:3000/healthz').then(r => process.exit(r.ok ? 0 : 1)).catch(() => process.exit(1))"
```

```yaml
# docker-compose.yml
healthcheck:
  test: ["CMD", "node", "-e", "fetch('http://127.0.0.1:3000/healthz').then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"]
  interval: 15s
  timeout: 3s
  start_period: 20s
  retries: 3
```

Note: Docker alone only *labels* a container `unhealthy`; **plain Docker doesn't restart it** (Swarm and orchestrators do). On ECS and Kubernetes, use the platform's own health mechanisms.

### Load balancer health checks

Cloud load balancers (AWS ALB/NLB target groups, GCP, Azure) have their own health checks that decide whether to route traffic to a target. Point them at **`/readyz`** (or a cheap `/healthz` if you keep readiness local), with sensible thresholds:

| Setting | Typical value | Why |
|---|---|---|
| Path | `/readyz` | Takes unready instances out of rotation |
| Interval | 10–15 s | Detection speed vs probe load |
| Healthy threshold | 2–3 | Avoid adding a flapping instance |
| Unhealthy threshold | 2–3 | Avoid ejecting on one blip |
| Timeout | 2–5 s (< interval) | Must exceed your check's timeouts |
| Success codes | `200` | |

The detection time for a bad instance is roughly `interval × unhealthy threshold`: 15 s × 3 = 45 s of failing traffic before ejection, unless the instance also fails fast and loudly.

---

## The keep-alive timeout mismatch (a classic source of random `502`s)

Behind a load balancer, you may see **occasional `502 Bad Gateway`** errors with nothing in your app logs. The usual cause: **your Node server closes idle keep-alive connections *sooner* than the load balancer expects.**

```
Load balancer idle timeout:  60 s   (AWS ALB default)
Node http.Server keepAliveTimeout:  5 s   (default!)

t=0     LB reuses a connection it thinks is still open
t=5.001 Node has just closed that idle connection ...
        LB sends a request on it → connection reset → 502
```

The rule: **the backend's keep-alive timeout must be *longer* than the load balancer's idle timeout**, so the LB is always the one to close idle connections.

```js
// src/server.js
server.keepAliveTimeout = 65_000;     // > ALB's 60 s idle timeout
server.headersTimeout = 66_000;       // must be > keepAliveTimeout
```

The same applies to Nginx upstream keep-alive (`04-nginx.md`) and any proxy in front of Node. This setting is easy to forget and painful to diagnose, since the 502s are rare and unreproducible in development.

---

## Startup and shutdown for the whole process model

| Component | Start | Stop |
|---|---|---|
| **HTTP server** | After config validated, DB connected, caches warm → `listen` | Fail readiness → delay → `close()` + drain |
| **Queue workers** (`11-async-processing/`) | After the DB/Redis connections are up | `worker.close()` (finish active jobs) *before* closing connections |
| **Scheduled jobs** | Via a queue scheduler (once per tick, not per instance) | Stop timers |
| **Kafka consumers** | After handlers are ready | `consumer.disconnect()` (clean group leave) |
| **Socket.IO** | With the HTTP server | Jittered client drain, then `io.close()` (`12-realtime/03-scaling-with-redis-adapter.md`) |
| **OpenTelemetry / logs** | **Before** anything else (instrumentation must load first) | **Last**: flush spans and logs after everything else has stopped |

Close in the **reverse order of creation**: last started, first stopped.

---

## Testing the lifecycle

```js
test("readiness fails once shutdown begins", async () => {
  expect((await request(app).get("/readyz")).status).toBe(200);
  state.shuttingDown = true;
  expect((await request(app).get("/readyz")).status).toBe(503);
  state.shuttingDown = false;
});

test("readiness fails when the database is down", async () => {
  const app = buildApp({ db: { query: async () => { throw new Error("down"); } } });
  expect((await request(app).get("/readyz")).status).toBe(503);
  expect((await request(app).get("/healthz")).status).toBe(200);          // liveness is independent of dependencies
});
```

An end-to-end shutdown test (spawn the real server as a child process):

```js
import { spawn } from "node:child_process";

test("finishes an in-flight request after SIGTERM, then exits 0", async () => {
  const child = spawn("node", ["src/server.js"], { env: { ...process.env, PORT: "4010", SHUTDOWN_DELAY_MS: "100" } });
  await waitForOutput(child, "Server listening");

  const slow = fetch("http://127.0.0.1:4010/slow?ms=1500");               // a test-only endpoint that takes 1.5 s
  await new Promise((r) => setTimeout(r, 200));
  child.kill("SIGTERM");                                                   // arrives while the request is in flight

  expect((await slow).status).toBe(200);                                   // the request still completed
  const code = await new Promise((r) => child.on("exit", r));
  expect(code).toBe(0);
});
```

Manual check: `docker stop` should take ~1–2 seconds, and your logs should show "Shutdown started" then "Shutdown complete". During a load test (`15-performance/04-load-balancing-and-testing.md`), run a rolling deploy and confirm **zero failed requests**.

---

## Common mistakes

```js
// ❌ no SIGTERM handler: every deploy kills in-flight requests instantly
// ❌ closing the DB pool before the HTTP server has drained → "pool is closed" errors for in-flight requests
// ❌ `CMD ["npm", "start"]` or shell-form CMD: SIGTERM never reaches Node, so you get a 10 s delay and SIGKILL
// ❌ liveness probe that checks the database → a DB blip restarts every instance at once
// ❌ one /health endpoint used for liveness AND readiness AND load balancer checks
// ❌ health checks without timeouts (a slow dependency hangs the probe)
// ❌ heavy health checks (full queries) hit every few seconds from several sources
// ❌ health endpoints included in logs/metrics/rate limits, or exposing internal details publicly
// ❌ no readiness failure on shutdown, so the load balancer keeps sending traffic to a draining instance
// ❌ shutting down instantly on SIGTERM, ignoring the endpoint-removal race
// ❌ grace period shorter than your slowest request, or shutdown logic with no force-exit deadline
// ❌ keepAliveTimeout shorter than the load balancer's idle timeout → mysterious intermittent 502s
// ❌ not exiting after an uncaughtException (continuing in an unknown state)
// ❌ a second SIGTERM starting a second, conflicting shutdown (make it idempotent)
// ❌ liveness probe starting before a slow app has booted → restart loop (use a startup probe / start period)
```

## Checklist

- [ ] `SIGTERM` and `SIGINT` handled; shutdown is idempotent and has a force-exit deadline below the platform's grace period
- [ ] Order: fail readiness → short delay → stop accepting → drain → stop workers → close DB/Redis → flush telemetry → exit
- [ ] `Connection: close` while shutting down; idle keep-alive connections closed
- [ ] `uncaughtException`/`unhandledRejection`: log, flush, exit non-zero
- [ ] Container starts Node directly (exec-form `CMD`), with an init (`--init`/tini); `docker stop` returns in about a second
- [ ] Separate **liveness** (process only, no dependencies), **readiness** (can serve traffic, fails during shutdown), and **startup** probes
- [ ] Dependency checks have short timeouts and are cheap; detailed output isn't public
- [ ] Health endpoints excluded from logs, metrics, tracing, and rate limiting; unauthenticated
- [ ] Load balancer health check points at readiness, with sensible intervals/thresholds
- [ ] `server.keepAliveTimeout` > the load balancer's idle timeout; `headersTimeout` > `keepAliveTimeout`
- [ ] Platform grace period > longest legitimate request + drain delay; `preStop`/deregistration delay set
- [ ] Rolling deploy tested under load with zero failed requests

## Next

**`03-docker-and-compose.md`** packages your app into the container this lifecycle runs inside: a small, secure, cache-friendly Dockerfile for Node.js, plus Compose for running the app and its dependencies together.
