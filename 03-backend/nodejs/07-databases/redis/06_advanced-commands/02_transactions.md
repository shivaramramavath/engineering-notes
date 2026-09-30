# Transactions

A Redis transaction runs a **queued batch of commands as one uninterrupted unit**: no other client's command executes in the middle. It is built from `MULTI`, `EXEC`, and optionally `WATCH`.

## Basic usage

```ts
const results = await redis
  .multi()
  .set("balance:alice", 100)
  .set("balance:bob", 50)
  .incrby("stats:transfers", 1)
  .exec();

// [[null, "OK"], [null, "OK"], [null, 1]]
```

What happens on the wire:

```
MULTI                      → OK
SET balance:alice 100      → QUEUED
SET balance:bob 50         → QUEUED
INCRBY stats:transfers 1   → QUEUED
EXEC                       → [OK, OK, 1]   (all executed back to back)
```

By default ioredis sends the whole `MULTI ... EXEC` block **as a pipeline** in one round trip. To send each command as you build the transaction (rarely needed):

```ts
const tx = redis.multi({ pipeline: false });
tx.set("a", 1);
await tx.exec();
```

## What a transaction guarantees

| Property | In Redis |
|----------|----------|
| **Isolation** | Yes: no other command runs between `MULTI`'s queued commands during `EXEC` |
| **All commands are attempted** | Yes: once `EXEC` runs, every queued command executes |
| **Rollback** | **No** |
| **Conditional logic inside** | **No**: commands are queued blind, you can't read a value mid-transaction and branch |
| **Durability** | Same as normal commands (AOF/RDB policy) |

## No rollback: errors

There are two kinds of errors:

### 1. Errors while queuing (before `EXEC`)

Unknown command, wrong number of arguments, out of memory, and so on. The **whole transaction is discarded** (`EXECABORT`).

```ts
try {
  await redis.multi().set("a", 1).call("NOSUCHCMD").exec();
} catch (err: any) {
  // EXECABORT Transaction discarded because of previous errors
}
```

### 2. Errors while executing (during `EXEC`)

Runtime errors such as `WRONGTYPE`. **Only that command fails. The others are applied.**

```ts
await redis.set("name", "Ada");
const results = await redis
  .multi()
  .incr("counter")          // OK
  .lpush("name", "x")       // WRONGTYPE (name is a string)
  .incr("counter")          // still runs
  .exec();

// [[null, 1], [ReplyError WRONGTYPE...], [null, 2]]
```

Always inspect each result (use the `unwrap` helper from the [pipelines lesson](./01_pipelines-and-auto-pipelining.md)). Redis skips rollback on purpose: runtime errors are programming mistakes, not conditions to recover from, and it keeps Redis simple and fast.

## Transactions vs pipelines

| | Pipeline | Transaction (`MULTI`) |
|---|----------|----------------------|
| Round trips | 1 | 1 |
| Interleaving with other clients | Possible | **Not possible** |
| Rollback | No | No |
| Use case | Speed | Speed + isolation for a fixed batch |

## Patterns

### 1. Create data and its expiry together

```ts
await redis.multi()
  .hset(`session:${sid}`, { userId, ip })
  .expire(`session:${sid}`, 1800)
  .exec();
```

### 2. Update an object and its index together

```ts
await redis.multi()
  .hset("user:1042", { email: "new@example.com" })
  .set("idx:user:email:new@example.com", 1042)
  .unlink("idx:user:email:old@example.com")
  .exec();
```

### 3. Move a member atomically

```ts
await redis.multi().srem("todo", taskId).sadd("done", taskId).exec();
// or the single command: await redis.smove("todo", "done", taskId)
```

### 4. Atomic counter with a TTL set once (Redis 7+)

```ts
const [[, count]] = (await redis.multi()
  .incr(`rate:${ip}`)
  .expire(`rate:${ip}`, 60, "NX")     // set the TTL only if none exists
  .exec())!;
```

## Optimistic locking with `WATCH`

`MULTI` can't read then decide. `WATCH` adds **check-and-set**: watch keys, read them, then run a transaction that **aborts if any watched key changed** since the `WATCH`.

```ts
await redis.watch("stock:sku1");
const stock = Number(await redis.get("stock:sku1"));

if (stock >= 1) {
  const result = await redis.multi().decr("stock:sku1").exec();
  // result === null  → the key changed, transaction aborted, retry
} else {
  await redis.unwatch();
}
```

- `exec()` returns **`null`** when a watched key was modified
- `WATCH` is cleared by `EXEC`, `DISCARD` and `UNWATCH`
- A key **expiring** or being deleted counts as a change

### The big ioredis gotcha: WATCH is per connection

`WATCH` state lives on the **connection**, and ioredis multiplexes all your concurrent calls over **one connection**. Two operations using `WATCH` at the same time on the same client **interfere**: one's `EXEC` or `UNWATCH` clears the other's watch, and either can also silently succeed when it shouldn't.

**Do not use `WATCH` on your shared client.** Use a **dedicated connection** per attempt:

```ts
async function withWatch<T>(
  keys: string[],
  fn: (client: Redis) => Promise<T | null>,   // return null if the transaction aborted
  retries = 5
): Promise<T> {
  const client = redis.duplicate();           // its own connection
  try {
    for (let attempt = 0; attempt < retries; attempt++) {
      await client.watch(...keys);
      try {
        const result = await fn(client);
        if (result !== null) return result;   // committed
      } finally {
        await client.unwatch();
      }
      await new Promise((r) => setTimeout(r, 10 + Math.random() * 20));  // backoff + jitter
    }
    throw new Error("too much contention, gave up");
  } finally {
    client.disconnect();
  }
}

// usage
await withWatch([`stock:${sku}`], async (c) => {
  const stock = Number(await c.get(`stock:${sku}`));
  if (stock < qty) throw new Error("insufficient stock");
  return c.multi().decrby(`stock:${sku}`, qty).exec();   // null if the key changed
});
```

Opening a connection per operation is heavy. That is why, for anything hot, **a Lua script is the better answer**: atomic, one round trip, no retry loop, no dedicated connection.

### WATCH vs Lua

| | `WATCH` + `MULTI` | Lua script |
|---|------------------|------------|
| Round trips | 2+ per attempt | 1 |
| Retries under contention | Yes | No |
| Needs a dedicated connection | **Yes** (ioredis) | No |
| Complex logic | Awkward (client-side) | Natural |
| Best for | Rare, low-contention updates | Hot paths, high contention |

## Transactions in Cluster

All keys in a `MULTI` must be in the **same slot** (use hash tags). `WATCH` has the same restriction.

```ts
await cluster.multi()
  .hset("{order:9}:info", { status: "paid" })
  .sadd("{order:9}:items", "sku1")
  .exec();
```

## `DISCARD`

```ts
const tx = redis.multi({ pipeline: false });
tx.set("a", 1);
await tx.discard();     // drop everything queued
```

With the default pipelined `multi()`, simply don't call `exec()`: nothing was sent.

## When to use transactions

Good:

- A **fixed** set of writes that must be grouped (object + index, data + TTL)
- Batching without interleaving

Not the best fit:

- Read-decide-write logic → **Lua**
- Pure speed → **pipeline**
- Business rollback semantics → a real database transaction

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Expecting rollback on runtime errors | Validate first, or design idempotent steps |
| Ignoring per-command errors in the result | `unwrap()` |
| `WATCH` on the shared client | Dedicated `duplicate()` connection, or Lua |
| Not handling `null` from `exec()` | Retry with backoff, or give up |
| Trying to branch inside `MULTI` | Lua script |
| Cross-slot keys in Cluster | Hash tags |
| Long retry loops under contention | Move to Lua |

## Key takeaways

- `MULTI`/`EXEC` = isolated batch, **no rollback**, no conditionals
- Queue-time errors abort everything. Runtime errors affect only that command
- `WATCH` gives optimistic locking, but on ioredis it needs its own connection
- For read-then-write atomicity, prefer Lua

**Next:** [Lua Scripts](./03_lua-scripts.md)
