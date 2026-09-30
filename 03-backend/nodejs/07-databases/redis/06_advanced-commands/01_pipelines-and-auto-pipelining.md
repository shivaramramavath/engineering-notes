# Pipelines and Auto Pipelining

Most Redis latency is **network round trips**, not command execution. A pipeline sends many commands in one write and reads all replies together.

## The round-trip problem

```
Sequential:  send → wait → reply → send → wait → reply ...      N round trips
Pipelined:   send, send, send ... → wait → reply, reply, reply   1 round trip
```

Illustration (assumed numbers, measure your own): with a 0.5 ms round trip, 1,000 sequential `GET`s cost about 500 ms of waiting. Pipelined, the same 1,000 commands take a few milliseconds because Redis executes each in microseconds.

**A pipeline reduces latency. It does not make commands atomic.**

## Basic pipeline

```ts
const results = await redis
  .pipeline()
  .set("a", 1)
  .incr("a")
  .get("a")
  .hgetall("user:1")
  .exec();

// results: Array<[Error | null, unknown]>
// [[null, "OK"], [null, 2], [null, "2"], [null, { name: "Ada" }]]
```

Array form:

```ts
await redis.pipeline([
  ["set", "a", "1"],
  ["get", "a"],
]).exec();
```

Build it dynamically:

```ts
const pipeline = redis.pipeline();
for (const id of ids) pipeline.hgetall(`user:${id}`);
const results = await pipeline.exec();
const users = results!.map(([, user]) => user);
```

`pipeline.length` tells you how many commands are queued.

## Reading results and errors

`exec()` **resolves** even if some commands fail. Each entry is `[error, result]`:

```ts
const results = await redis.pipeline().set("k", "v").lpush("k", "x").get("k").exec();
// [[null, "OK"], [ReplyError WRONGTYPE ...], [null, "v"]]
```

A small helper keeps call sites clean:

```ts
export function unwrap<T = unknown>(results: [Error | null, unknown][] | null): T[] {
  if (!results) throw new Error("pipeline/transaction aborted");
  return results.map(([err, value]) => {
    if (err) throw err;
    return value as T;
  });
}

const [ok, count] = unwrap<string | number>(
  await redis.pipeline().set("a", 1).incr("a").exec()
);
```

Only connection-level failures reject `exec()` itself.

## Pipelines are not atomic

Other clients' commands can run **between** your pipelined commands:

```
your SET ─ other client's DEL ─ your GET      ← possible
```

If you need "no interleaving", use [`MULTI`](./02_transactions.md) or [Lua](./03_lua-scripts.md).

## Batch size

Huge pipelines cost memory on both sides: Redis buffers replies, and your process buffers the queued commands and results. Chunk large jobs:

```ts
async function pipelineInChunks<T>(
  items: T[],
  build: (p: ReturnType<Redis["pipeline"]>, item: T) => void,
  size = 1000
) {
  const all: [Error | null, unknown][] = [];
  for (let i = 0; i < items.length; i += size) {
    const p = redis.pipeline();
    items.slice(i, i + size).forEach((item) => build(p, item));
    const res = await p.exec();
    all.push(...(res ?? []));
  }
  return all;
}

await pipelineInChunks(userIds, (p, id) => p.hgetall(`user:${id}`), 1000);
```

Typical chunk sizes: a few hundred to a few thousand commands. Tune by measuring, and keep payloads in mind (many large values means smaller chunks).

## When to use what

| Situation | Best option |
|-----------|-------------|
| Same command, many keys (strings) | `MGET` / `MSET` (one command) |
| Different commands, independent | Pipeline |
| Concurrent `await`s in normal code | Auto pipelining |
| Must not interleave | `MULTI`/`EXEC` or Lua |
| Result of one command feeds the next | Separate calls or Lua |

`MGET key1 key2 ...` is a single command and usually the cheapest way to read many strings. For hashes, sorted sets and mixed types, use a pipeline.

## Auto pipelining

Auto pipelining batches commands **issued in the same event-loop tick** into one pipeline, with no code changes:

```ts
const redis = new Redis({ enableAutoPipelining: true });

const values = await Promise.all(
  ids.map((id) => redis.get(`user:${id}`))
);
// all GETs were flushed together in one round trip
```

Each call still returns its own Promise with its own result or error.

### The rule that catches everyone

Batching only happens for commands that are **in flight at the same time**. Sequential awaits get no benefit:

```ts
// NOT batched: each await finishes before the next command starts
for (const id of ids) {
  await redis.get(`user:${id}`);
}

// Batched
await Promise.all(ids.map((id) => redis.get(`user:${id}`)));
```

Concurrent request handlers in a web server naturally overlap, so under load auto pipelining batches commands from **different requests** together, which is where it shines.

### Options

```ts
new Redis({
  enableAutoPipelining: true,
  autoPipeliningIgnoredCommands: ["scan"],   // commands that should skip batching
});
```

Notes:

- Exclude commands that shouldn't be queued behind others (for example blocking commands like `BLPOP`, and long-running scans). Use a separate client for blocking work anyway
- In Cluster, auto pipelining groups commands per node or slot for you
- Enabled per client, so you can keep one client with it on and another off

### Trade-offs

| Pros | Cons |
|------|------|
| Free batching for concurrent code | Adds up to one tick of latency for a lone command |
| No refactor needed | Harder to reason about which commands travel together |
| Great for high-concurrency APIs | Doesn't help sequential code |
| Works with Cluster | Not atomic (same as pipelines) |

## Pipelines and Cluster

A pipeline sends everything through one connection, so in Cluster **all keys in one `pipeline()` must belong to the same slot** (ioredis rejects a pipeline whose keys span slots). Options:

- Use **hash tags** for keys used together (`{user:1}:profile`, `{user:1}:cart`)
- Group commands by slot or node yourself, then run one pipeline per group
- Use **auto pipelining**, which groups for you

```ts
const cluster = new Redis.Cluster(nodes, { enableAutoPipelining: true });
await Promise.all(keys.map((k) => cluster.get(k)));
```

## Patterns

### 1. Bulk load

```ts
await pipelineInChunks(rows, (p, r) => {
  p.hset(`product:${r.id}`, { name: r.name, price: r.price });
  p.expire(`product:${r.id}`, 3600);
}, 500);
```

### 2. Fetch many objects

```ts
const p = redis.pipeline();
ids.forEach((id) => p.hgetall(`user:${id}`));
const users = unwrap<Record<string, string>>(await p.exec());
```

### 3. Write-and-forget metrics

```ts
redis.pipeline()
  .incr(`metrics:hits:${route}`)
  .zadd("metrics:recent", Date.now(), route)
  .exec()
  .catch((err) => logger.warn({ err }, "metrics write failed"));
```

### 4. Measure your own gain

```ts
async function bench(n = 5000) {
  const keys = Array.from({ length: n }, (_, i) => `bench:${i}`);

  let t = performance.now();
  for (const k of keys) await redis.get(k);
  console.log("sequential", (performance.now() - t).toFixed(0), "ms");

  t = performance.now();
  await Promise.all(keys.map((k) => redis.get(k)));           // concurrent (multiplexed)
  console.log("Promise.all", (performance.now() - t).toFixed(0), "ms");

  t = performance.now();
  const p = redis.pipeline();
  keys.forEach((k) => p.get(k));
  await p.exec();
  console.log("pipeline", (performance.now() - t).toFixed(0), "ms");
}
```

Run it against your real deployment: latency to Redis (same host, same AZ, cross-region) changes the result dramatically.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Treating a pipeline as a transaction | `MULTI` or Lua |
| Ignoring per-command errors in `exec()` | `unwrap()` or inspect each pair |
| Giant pipelines (100k+ commands) | Chunk to hundreds or thousands |
| Sequential `await` with auto pipelining on | Use `Promise.all` |
| Cross-slot keys in a Cluster pipeline | Hash tags, grouping, or auto pipelining |
| Using the result of command 1 as an argument in command 2 | Not possible in one pipeline. Use Lua |
| Blocking commands in a pipeline | Dedicated connection |

## Key takeaways

- Pipelines cut round trips and are **not atomic**
- `exec()` returns `[err, result]` pairs, so always check them
- Auto pipelining batches concurrent commands automatically, but sequential `await`s don't benefit
- Chunk big jobs, and mind slot rules in Cluster

**Next:** [Transactions](./02_transactions.md)
