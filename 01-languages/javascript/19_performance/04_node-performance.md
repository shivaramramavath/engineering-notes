# Node Performance

Node excels at I/O-heavy workloads because its single JavaScript thread never waits: it hands I/O to the OS or thread pool and handles other work meanwhile. Performance problems usually come from **blocking that thread**, **slow dependencies** (database, network), **too much work per request**, or **memory pressure**. This file covers how to keep a Node service fast and how to tune it.

Related: [Node Event Loop](../16_nodejs/02_node-event-loop.md), [HTTP Server](../16_nodejs/08_http-server.md), [Streams](../16_nodejs/06_streams.md), [Worker Threads](../17_concurrency-and-parallelism/03_worker-threads.md), [Profiling](./02_profiling.md).

## The cardinal rule: do not block the event loop

One long synchronous operation delays **every** request.

| Blocks the loop | Do instead |
|-----------------|------------|
| `fs.readFileSync`, `execSync`, other `*Sync` APIs in request paths | `fs/promises`, async child processes |
| `crypto.pbkdf2Sync`, `scryptSync`, `bcrypt` sync variants | Async versions (they use the thread pool) |
| Large `JSON.parse` / `JSON.stringify` | Stream or paginate; use NDJSON; limit body sizes |
| CPU-heavy loops (image resize, PDF generation, compression of big buffers, big sorts) | Worker threads, a job queue, or a separate service |
| Catastrophic regular expressions (ReDoS) | Safer patterns, input length limits, a linear-time engine such as RE2 |
| Huge synchronous array work (`map`/`sort` on millions of items per request) | Precompute, paginate, push work to the database or a worker |

```js
// Blocks everyone for the duration
app.get('/report', (req, res) => {
  res.json(buildHugeReport());
});

// Offload CPU work
app.get('/report', async (req, res) => {
  res.json(await pool.run({ type: 'report', params: req.query }));    // worker pool (see Worker Threads)
});
```

### Monitor the loop

```js
import { monitorEventLoopDelay, performance } from 'node:perf_hooks';

const h = monitorEventLoopDelay({ resolution: 10 });
h.enable();

let last = performance.eventLoopUtilization();

setInterval(() => {
  const now = performance.eventLoopUtilization();
  const { utilization } = performance.eventLoopUtilization(now, last);   // 0 to 1
  last = now;

  console.log({
    delayP99Ms: +(h.percentile(99) / 1e6).toFixed(1),
    utilization: +utilization.toFixed(2),
  });
  h.reset();
}, 10_000).unref();
```

Alert on rising p99 loop delay and on utilization approaching 1. Sustained utilization near 1 means the process is CPU-saturated: scale out or move work off the loop.

## Make I/O efficient

### Parallelize independent work; avoid waterfalls

```js
// Sequential: total = a + b + c
const user = await getUser(id);
const orders = await getOrders(id);
const prefs = await getPrefs(id);

// Parallel: total = max(a, b, c)
const [user2, orders2, prefs2] = await Promise.all([getUser(id), getOrders(id), getPrefs(id)]);
```

Cap parallelism when fanning out over many items (see [Concurrency Control](../17_concurrency-and-parallelism/05_concurrency-control.md)).

### Stream instead of buffering

```js
// Buffers the whole file in memory before responding
res.end(await fs.readFile('big.zip'));

// Constant memory, starts sending immediately
await pipeline(createReadStream('big.zip'), res);
```

Respect backpressure, and use `pipeline` for correct error handling ([Streams](../16_nodejs/06_streams.md)).

### Connection reuse

Creating a connection per request is slow (DNS, TCP, TLS).

```js
// Node 19+ enables keep-alive on the default http/https agents.
// For explicit control with https.request, create one shared agent:
import https from 'node:https';

const agent = new https.Agent({ keepAlive: true, maxSockets: 50 });
https.get(url, { agent }, (res) => res.resume());

// Built-in fetch is backed by undici, which pools connections automatically.
// Tune it with an undici Agent passed as `dispatcher` (requires the `undici` package).
```

- Use a **database connection pool** (size it from the database's limits and your concurrency, for example 10 to 20 per instance, not hundreds)
- Reuse clients (Redis, HTTP, SDK clients): create them **once** at startup, not per request

### Compression

```js
import zlib from 'node:zlib';
// Compress responses for text content when not handled by a reverse proxy
```

Prefer to do TLS termination, compression, and static file serving in a reverse proxy or CDN (nginx, Caddy, cloud load balancer); keep Node for application logic. Compressing large responses synchronously on the main thread costs CPU; the zlib streaming APIs use the thread pool.

### The thread pool

`fs`, `dns.lookup`, `crypto`, and `zlib` operations share libuv's pool (4 threads by default). Heavy concurrent use queues behind it.

```bash
UV_THREADPOOL_SIZE=16 node server.js      # set before the process starts; more threads means more memory and context switching
```

Raise it only if you see queueing on these operations. See [Node Event Loop](../16_nodejs/02_node-event-loop.md).

## Database and downstream calls

Most "slow Node" problems are slow queries.

| Problem | Fix |
|---------|-----|
| **N+1 queries** (one query per item in a loop) | Batch with `IN (...)`, joins, or a DataLoader-style batcher |
| Missing indexes | Use `EXPLAIN`; index filter, join, and sort columns |
| Fetching more columns or rows than needed | Select only needed fields; paginate (prefer keyset over large `OFFSET`) |
| Many round trips | Combine queries, use transactions or stored procedures judiciously |
| No timeouts | Set timeouts on every downstream call; fail fast |
| Pool exhaustion | Right-size the pool; release connections; add queueing limits |

```js
// N+1
const posts = await db.posts.list();
for (const p of posts) p.author = await db.users.get(p.authorId);     // one query per post

// Batched
const authors = await db.users.getMany([...new Set(posts.map((p) => p.authorId))]);
const byId = new Map(authors.map((u) => [u.id, u]));
for (const p of posts) p.author = byId.get(p.authorId);
```

## Caching

Avoid repeating expensive work. Layers, from nearest to farthest:

| Layer | Use |
|-------|-----|
| **In-process memory** (`Map`, LRU) | Hot, small, per-instance data; fastest; duplicated per instance |
| **Shared cache** (Redis, Memcached) | Shared across instances and workers |
| **HTTP caching** (`Cache-Control`, `ETag`, CDN) | Public or per-user responses |
| **Database query cache / materialized views** | Heavy aggregates |

```js
const cache = new Map();
const inflight = new Map();

// Cache with request coalescing: concurrent callers share one in-flight computation
async function getCached(key, compute, ttlMs = 30_000) {
  const hit = cache.get(key);
  if (hit && hit.expires > Date.now()) return hit.value;

  if (inflight.has(key)) return inflight.get(key);

  const p = compute()
    .then((value) => { cache.set(key, { value, expires: Date.now() + ttlMs }); return value; })
    .finally(() => inflight.delete(key));

  inflight.set(key, p);
  return p;
}
```

Always bound in-memory caches (size and TTL) to avoid leaks ([Memory Leaks](../18_memory-and-garbage-collection/03_memory-leaks.md)), and plan invalidation. See [Caching](../23_real-world-patterns/08_caching.md).

## HTTP server tuning

| Setting | Purpose |
|---------|---------|
| `server.keepAliveTimeout` | Keep idle connections open slightly longer than the load balancer's idle timeout to avoid intermittent 502s |
| `server.headersTimeout`, `server.requestTimeout` | Protect against slow clients |
| Request body size limits | Prevent memory exhaustion |
| Reverse proxy in front | TLS, compression, static files, buffering of slow clients |
| HTTP/2 | Multiplexing for browsers (often terminated at the proxy) |

Framework notes: **Fastify** and other schema-based frameworks serialize JSON faster through compiled serializers (`fast-json-stringify`); Express is simpler but slower per request. Choose based on needs, then measure your own workload; do not switch frameworks as a first step.

Logging cost: synchronous or verbose logging slows hot paths. Use an async, structured logger such as **Pino**, avoid logging large objects, and use log levels.

```js
// Avoid building log strings you do not need
if (logger.isLevelEnabled('debug')) logger.debug({ payload }, 'details');
```

## JSON performance

- Parsing and stringifying large payloads blocks the loop; limit sizes
- Use schema-based serializers (`fast-json-stringify`) for hot endpoints
- Send only needed fields
- For very large datasets use streaming JSON (NDJSON, `stream-json`) or a binary format (MessagePack, Protocol Buffers)
- Avoid `JSON.parse(JSON.stringify(x))` for cloning; use `structuredClone` or avoid cloning

## Scaling beyond one process

One Node process uses roughly one CPU core for JavaScript.

| Option | Use |
|--------|-----|
| **Multiple instances** behind a load balancer (containers, VMs, PM2) | Most common, simplest to reason about |
| **`cluster` module** | Several workers on one machine sharing a port ([Child Process and Cluster](../16_nodejs/09_child-process-and-cluster.md)) |
| **Worker threads** | CPU-bound tasks inside a service |
| **Message queues** (BullMQ, SQS, RabbitMQ) | Move slow or heavy jobs out of the request path |
| **Horizontal autoscaling** | Scale on CPU, event-loop delay, or request queue depth |

Keep processes **stateless** (sessions, caches, locks in external stores) so you can add instances freely.

## Memory and GC in Node

```bash
node --max-old-space-size=1536 server.js        # heap limit (MB); keep below the container limit with headroom
node --max-semi-space-size=32 server.js         # larger young generation reduces scavenge frequency (uses more memory)
node --trace-gc server.js                       # inspect GC behavior
```

- Watch `heapUsed`, `rss`, `external`, and `arrayBuffers` ([Memory Leaks](../18_memory-and-garbage-collection/03_memory-leaks.md))
- High allocation rates in hot endpoints cause frequent GC: reuse buffers, avoid unnecessary copies and large temporary arrays ([Memory Optimization](../18_memory-and-garbage-collection/04_memory-optimization.md))
- In containers, Node respects cgroup memory limits for default heap sizing in recent versions, but still verify with `v8.getHeapStatistics().heap_size_limit`

## Startup time

Matters for serverless, CLIs, and autoscaling.

- Load fewer modules at startup: lazy `import()` rarely used code
- Avoid heavy work at import time (large JSON parsing, synchronous I/O)
- Bundle server code (esbuild) for faster cold starts in serverless
- Use the V8 compile cache: `module.enableCompileCache()` (recent Node versions) to speed repeated startups
- Keep dependencies lean: audit with `npm ls`, `depcheck`, and tools like `cost-of-modules`

## Load testing

Measure under realistic traffic before and after changes.

```bash
npx autocannon -c 100 -d 30 -p 10 http://localhost:3000/api/items
# -c connections, -d duration (s), -p pipelined requests per connection
```

Other tools: **k6** (scripted scenarios), **wrk**, **Artillery**, **Gatling**.

Guidelines:

- Test a production-like build, with production-like data and a realistic mix of endpoints
- Run the load generator on a **different machine** from the server
- Warm up first; watch p50, p95, p99, error rate, CPU, memory, and event-loop delay
- Increase load gradually to find the saturation point and see how the service degrades
- Test with slow clients and timeouts, not only fast ones

## Production profiling and observability

- **Tracing** (OpenTelemetry): spans around handlers, database calls, and HTTP calls to see where time goes
- **Metrics**: request rate, error rate, latency percentiles, event-loop delay, heap and RSS, GC time, pool usage
- **Continuous profiling** (Pyroscope, Datadog, Sentry profiling): low-overhead CPU profiles in production
- **On-demand profiling**: `--cpu-prof`, `--heapsnapshot-signal`, or a protected diagnostics endpoint

Add `Server-Timing` headers or structured per-stage timing logs in development and staging.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `*Sync` APIs in handlers | Blocks all requests | Async APIs |
| CPU-heavy work on the main thread | Latency spikes under load | Worker threads, queue, or a separate service |
| N+1 queries | Many round trips | Batch or join |
| Creating clients and connections per request | Handshake overhead | Create once and reuse; use pools |
| Unlimited parallel fan-out (`Promise.all` over thousands of items) | Overloads dependencies and memory | Concurrency limits |
| No timeouts on downstream calls | Requests hang and pile up | Timeouts and `AbortSignal` |
| Buffering large bodies and files in memory | High memory, GC pressure | Streams |
| Unbounded in-memory caches | Memory leak | LRU with TTL |
| Cache stampedes on expiry | Spikes of identical heavy requests | Request coalescing, jittered TTLs |
| Heavy synchronous logging | Slows hot paths | Async structured logger, sensible levels |
| Tuning flags (`UV_THREADPOOL_SIZE`, heap sizes) without measuring | Little gain, more memory | Profile first |
| Load testing from the same machine as the server | Skewed results | Separate load generator |
| Switching frameworks as the first optimization | Rarely the bottleneck | Profile and fix hot spots |

## Key takeaways

- Keep the event loop free: no sync I/O, no heavy CPU, no giant JSON in request paths
- Most latency is waiting: parallelize independent calls, batch queries, pool and reuse connections, set timeouts
- Stream large data instead of buffering it
- Cache with bounds, TTLs, invalidation, and request coalescing
- Scale out with instances, workers, and queues; keep services stateless
- Monitor event-loop delay and utilization, latency percentiles, memory, and GC
- Load test realistically, profile in production-like conditions, and change one thing at a time

**Next:** [Optimization Patterns](./05_optimization-patterns.md)