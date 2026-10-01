# Consumer Groups and Acks

A **consumer group** lets several workers share one stream. Redis hands each entry to **one** worker in the group, remembers what is **in flight**, and lets you **acknowledge** entries when the work is done. Entries that are never acknowledged can be **reclaimed** and retried.

This is Redis's answer to "a job queue with at-least-once delivery".

## The model

```
Stream:  [1-0][2-0][3-0][4-0][5-0][6-0]
                          ▲
            group "workers": last-delivered-id = 4-0
                          │
        ┌─────────────────┴────────────────────────┐
   consumer "w1"                              consumer "w2"
   pending: 2-0, 4-0                          pending: 3-0
   (delivered, not yet XACKed)                (delivered, not yet XACKed)
```

| Piece | Meaning |
|-------|---------|
| **Group** | A named cursor over the stream, shared by its consumers |
| **Consumer** | A named worker inside a group (created automatically on first read) |
| **Last-delivered ID** | How far the group has handed out new entries |
| **PEL** (pending entries list) | Entries delivered to a consumer but not yet acknowledged, with idle time and delivery count |
| **ACK** | "Done": removes the entry from the PEL |

Delivery is **at-least-once**: an entry can be delivered **more than once** (a crash before the ack, a reclaim). So handlers must be idempotent ([patterns](./04_stream-patterns.md#idempotent-consumers)).

## Creating a group

```ts
await redis.xgroup("CREATE", STREAM, "workers", "$", "MKSTREAM");
```

| Argument | Meaning |
|----------|---------|
| `$` | The group only receives entries added **after** creation |
| `0` | The group starts at the **beginning** (processes the whole retained history) |
| a specific ID | Start after that ID |
| `MKSTREAM` | Create the stream if it doesn't exist yet |

Creating a group that already exists fails with `BUSYGROUP`. Make creation **idempotent**, so every worker can call it at startup:

```ts
export async function ensureGroup(redis: Redis, stream: string, group: string, startId: "$" | "0" = "$") {
  try {
    await redis.xgroup("CREATE", stream, group, startId, "MKSTREAM");
  } catch (err: any) {
    if (!String(err?.message).includes("BUSYGROUP")) throw err;
  }
}
```

Pick the start ID deliberately: **`$` for a new queue**, **`0` when the group must process existing history** (a new consumer service replaying events).

## Reading: `XREADGROUP`

```ts
const res = await blocker.xreadgroup(
  "GROUP", "workers", "worker-1",
  "COUNT", 10, "BLOCK", 5000,
  "STREAMS", STREAM, ">"
);
// [[ STREAM, [ [id, fields], ... ] ]]  or null on timeout
```

The last argument is the key:

| ID | Returns |
|----|---------|
| `>` | **New** entries never delivered to anyone in this group |
| `0` (or any other ID) | **This consumer's own pending entries** after that ID (its history), without blocking |

`NOACK` skips the pending list entirely (fire-and-forget, loses the safety net). Avoid it unless loss is acceptable.

## Acknowledging

```ts
await redis.xack(STREAM, "workers", id);                  // returns the number of entries acknowledged
await redis.xack(STREAM, "workers", id1, id2, id3);       // several at once
```

**Acknowledge only after the work succeeded**, and after any side effect is durable:

```
read ─► process ─► side effect committed ─► XACK
                         ▲
        crash anywhere before XACK → the entry stays pending → it is retried
```

Acknowledging **before** processing turns the system into at-most-once. A crash then loses the work.

## A production-shaped worker

This worker does five things that a real consumer needs:

1. Creates its group idempotently
2. **Drains its own pending entries first** after a restart
3. Reads new entries with a blocking call
4. **Reclaims** entries stuck on dead or slow consumers
5. Sends entries that keep failing to a **dead-letter stream**

```ts
import type { Redis } from "ioredis";
import { ensureGroup } from "./groups.js";
import { fieldsToObject, type RawEntry, type StreamEntry } from "./fields.js";

export interface WorkerOptions {
  stream: string;
  group: string;
  consumer: string;                 // stable and unique per worker, e.g. `${hostname}-${pid}`
  batch?: number;                   // entries per read, default 10
  blockMs?: number;                 // default 3000
  concurrency?: number;             // handlers running at once within a batch, default 1
  minIdleMs?: number;               // reclaim entries idle longer than this, default 60_000
  reclaimEveryMs?: number;          // default 15_000
  maxDeliveries?: number;           // then dead-letter, default 5
  dlqStream?: string;               // default `${stream}:dlq`
}

type Logger = { error: (o: unknown, m?: string) => void; warn: (o: unknown, m?: string) => void };
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export class GroupWorker {
  private running = false;
  private done?: Promise<void>;

  constructor(
    private blocker: Redis,       // dedicated: XREADGROUP BLOCK holds this connection
    private redis: Redis,         // normal commands: XACK, XAUTOCLAIM, XPENDING, XADD
    private handler: (e: StreamEntry) => Promise<void>,
    private o: WorkerOptions,
    private log: Logger = console
  ) {}

  async start() {
    await ensureGroup(this.redis, this.o.stream, this.o.group);
    this.running = true;
    this.done = Promise.all([this.consumeLoop(), this.reclaimLoop()]).then(() => undefined);
  }

  async stop() {
    this.running = false;
    await this.done;              // exits within one blockMs / sleep slice
  }

  // ---------- read loop ----------
  private async consumeLoop() {
    let cursor = "0";             // first drain OUR pending history, then switch to ">"
    let backoff = 100;

    while (this.running) {
      try {
        const res = await this.blocker.xreadgroup(
          "GROUP", this.o.group, this.o.consumer,
          "COUNT", this.o.batch ?? 10, "BLOCK", this.o.blockMs ?? 3000,
          "STREAMS", this.o.stream, cursor
        );
        backoff = 100;

        const entries = ((res as [string, RawEntry[]][] | null)?.[0]?.[1] ?? []) as RawEntry[];

        if (cursor !== ">") {
          // history mode: an empty reply means our pending list is drained
          if (entries.length === 0) { cursor = ">"; continue; }
          cursor = entries[entries.length - 1]![0];      // move past what we just read, even if it failed
        }

        if (entries.length) await this.processBatch(entries);
      } catch (err) {
        this.log.error({ err }, "xreadgroup failed, backing off");
        await sleep(backoff);
        backoff = Math.min(backoff * 2, 5000);
      }
    }
  }

  private async processBatch(entries: RawEntry[]) {
    const queue = [...entries];
    const lanes = Math.min(this.o.concurrency ?? 1, queue.length);
    await Promise.all(
      Array.from({ length: lanes }, async () => {
        for (let e = queue.shift(); e; e = queue.shift()) await this.handleOne(e);
      })
    );
  }

  private async handleOne([id, fields]: RawEntry) {
    try {
      await this.handler({ id, data: fieldsToObject(fields) });
      await this.redis.xack(this.o.stream, this.o.group, id);          // only after success
    } catch (err) {
      // leave it pending: the reclaim loop retries it after minIdleMs
      this.log.error({ err, id }, "handler failed, entry stays pending");
    }
  }

  // ---------- reclaim loop ----------
  private async reclaimLoop() {
    const every = this.o.reclaimEveryMs ?? 15_000;

    while (this.running) {
      for (let waited = 0; this.running && waited < every; waited += 250) await sleep(250);
      if (!this.running) break;

      try {
        let start = "0-0";
        do {
          const res = (await this.redis.xautoclaim(
            this.o.stream, this.o.group, this.o.consumer,
            this.o.minIdleMs ?? 60_000, start, "COUNT", this.o.batch ?? 10
          )) as [string, (RawEntry | [string, null])[], string[]?];

          start = res[0];

          for (const [id, fields] of res[1]) {
            if (!fields) {                                             // entry was trimmed or deleted
              await this.redis.xack(this.o.stream, this.o.group, id);
              continue;
            }
            const deliveries = await this.deliveryCount(id);
            if (deliveries > (this.o.maxDeliveries ?? 5)) await this.deadLetter(id, fields, deliveries);
            else await this.handleOne([id, fields]);
          }
        } while (start !== "0-0" && this.running);
      } catch (err) {
        this.log.error({ err }, "reclaim failed");
      }
    }
  }

  private async deliveryCount(id: string): Promise<number> {
    const r = (await this.redis.xpending(this.o.stream, this.o.group, id, id, 1)) as [string, string, number, number][];
    return r[0]?.[3] ?? 0;
  }

  private async deadLetter(id: string, fields: string[], deliveries: number) {
    const dlq = this.o.dlqStream ?? `${this.o.stream}:dlq`;
    await this.redis.multi()
      .xadd(dlq, "MAXLEN", "~", 10_000, "*",
        "origId", id, "stream", this.o.stream, "group", this.o.group,
        "deliveries", String(deliveries), "failedAt", String(Date.now()),
        "orig", JSON.stringify(fieldsToObject(fields)))
      .xack(this.o.stream, this.o.group, id)
      .exec();
    this.log.warn({ id, deliveries, dlq }, "entry moved to dead-letter stream");
  }
}
```

Usage:

```ts
const worker = new GroupWorker(
  connections.create("orders-worker", "blocking"),
  redis,
  async ({ id, data }) => { await fulfil(JSON.parse(data.data), data.msgId); },
  { stream: STREAM, group: "fulfilment", consumer: `${os.hostname()}-${process.pid}`, concurrency: 4 }
);

await worker.start();
// on shutdown: await worker.stop();
```

### Why each design choice

| Choice | Reason |
|--------|--------|
| History read with `0` first | After a restart, entries this consumer had in flight (delivered but unacknowledged) are processed before new work |
| Cursor moves past failed entries | Otherwise a failing entry is re-read in a tight loop |
| Failures **stay pending** | They become eligible for reclaim after `minIdleMs`, which doubles as a retry delay |
| Separate reclaim loop | Handles consumers that **died** or are stuck, and our own earlier failures |
| `deliveryCount` check | Detects **poison entries** (always failing) |
| DLQ write and `XACK` in one `MULTI` | Never both lost and acknowledged, never in the DLQ twice |
| `fields` may be `null` | An entry can be trimmed while pending (older Redis returns `null`), so acknowledge it to clear the pending list |
| Blocking connection separate from the normal one | `XACK` and reclaim must not wait behind `XREADGROUP BLOCK` |

`XAUTOCLAIM` returns `[nextStartId, entries, deletedIds?]`. The third element exists on Redis 7.0 and newer, which is why the code only relies on the first two.

## Reclaiming and retries

| Command | Use |
|---------|-----|
| `XAUTOCLAIM stream group consumer minIdle start [COUNT n]` | Scan the pending list from `start` and take ownership of entries idle longer than `minIdle`. Redis 6.2+ |
| `XCLAIM stream group consumer minIdle id ...` | Claim specific IDs |
| `XPENDING stream group` | Summary: count, smallest and largest pending ID, per-consumer counts |
| `XPENDING stream group start end count [consumer]` | Detail: `[id, consumer, idleMs, deliveryCount]` |

```ts
await redis.xpending(STREAM, "workers");
// [ totalPending, minId, maxId, [[consumer, count], ...] ]

await redis.xpending(STREAM, "workers", "-", "+", 20);
// [[id, consumer, idleMs, deliveryCount], ...]
```

### Choosing `minIdleMs`

It must be **longer than your slowest legitimate handler**, or a still-working consumer's entry gets claimed by another and processed twice.

| Handler duration | `minIdleMs` |
|------------------|-------------|
| Under 1 s | 30 to 60 s |
| Up to 30 s | 2 to 5 minutes |
| Minutes | Much larger, or **split the work** into smaller entries |

Idle time is measured since the last delivery or claim. Claiming one of your own entries with `min-idle-time` 0 resets its idle clock, so a long task can do that periodically as a heartbeat. Verify the behavior (including the delivery counter) on your Redis version before relying on it.

### Retry policy

There's no built-in backoff. Common approaches:

| Approach | How |
|----------|-----|
| **Idle-based** (above) | A failed entry waits `minIdleMs` before the next attempt |
| **Longer waits per attempt** | In the reclaim loop, skip entries whose idle time is below `backoff(deliveries)`, using the extended `XPENDING ... ` data |
| **Re-queue with a delay** | Acknowledge, then schedule a new entry via a sorted set of due times ([delayed jobs](../14_queues-and-workers/README.md)) |
| **Cap attempts** | After `maxDeliveries`, dead-letter |

## Dead-letter handling

An entry that exhausts its attempts moves to `stream:dlq` with its original fields and failure metadata. Treat the DLQ as an **inbox for humans or repair jobs**:

```ts
// inspect
const dead = await redis.xrevrange(`${STREAM}:dlq`, "+", "-", "COUNT", 20);

// replay one entry back into the main stream after fixing the cause
async function replay(dlqId: string) {
  const [[, fields]] = (await redis.xrange(`${STREAM}:dlq`, dlqId, dlqId)) as RawEntry[];
  const orig = JSON.parse(fieldsToObject(fields).orig);
  await redis.multi()
    .xadd(STREAM, "*", ...Object.entries(orig).flatMap(([k, v]) => [k, String(v)]))
    .xdel(`${STREAM}:dlq`, dlqId)
    .exec();
}
```

Alert on **DLQ growth**. A DLQ nobody reads is just a slower way to lose data.

## Several groups on one stream

Each group has its own cursor, so **every group receives every entry**:

```ts
await ensureGroup(redis, STREAM, "billing",   "0");
await ensureGroup(redis, STREAM, "email",     "0");
await ensureGroup(redis, STREAM, "analytics", "0");
```

```
order.paid ──► billing group   (shares work among billing workers)
          ├──► email group     (shares work among email workers)
          └──► analytics group
```

This is the stream equivalent of publish/subscribe, but **durable**, and each service scales independently.

## Scaling and ordering

- **Add workers** to a group and Redis spreads entries across them
- Ordering within a group holds only for a **single consumer**. With several, entry 5 can finish before entry 4
- For **per-entity ordering at scale**, partition into several streams by key and run one consumer per partition ([Stream Patterns](./04_stream-patterns.md#partitioned-streams))
- One consumer's `concurrency > 1` also loses order within its batch

## Consumer lifecycle

| Topic | Guidance |
|-------|----------|
| **Naming** | Use a **stable, unique** name (`hostname-pid`, or the pod name). A stable name lets a restarted worker pick up its own pending entries |
| **Random names per start** | Each restart leaves an orphan consumer with pending entries, which reclaim must rescue. Acceptable, but noisy |
| **Cleaning up** | Remove consumers that are gone for good, **after** claiming their pending entries: `XGROUP DELCONSUMER` |
| **Replay** | `XGROUP SETID stream group 0` rewinds the group, so the whole retained history is delivered again. Use only with idempotent consumers |

```ts
await redis.xgroup("DELCONSUMER", STREAM, "workers", "old-consumer");
await redis.xgroup("SETID", STREAM, "workers", "0");
```

## Monitoring

`XINFO` replies are flat arrays. Turn them into objects:

```ts
const toObj = (flat: unknown[]) => {
  const o: Record<string, unknown> = {};
  for (let i = 0; i < flat.length; i += 2) o[String(flat[i])] = flat[i + 1];
  return o;
};

export async function streamHealth(redis: Redis, stream: string) {
  const [len, groupsRaw, dlqLen] = await Promise.all([
    redis.xlen(stream),
    redis.xinfo("GROUPS", stream) as Promise<unknown[][]>,
    redis.xlen(`${stream}:dlq`).catch(() => 0),
  ]);

  const groups = groupsRaw.map(toObj).map((g) => ({
    group: g.name,
    consumers: g.consumers,
    pending: g.pending,
    lastDeliveredId: g["last-delivered-id"],
    lag: g.lag ?? null,                       // entries not yet delivered (Redis 7.0+, can be null)
  }));

  return { length: len, groups, deadLetters: dlqLen };
}
```

And the oldest pending entry per group:

```ts
const [count, minId] = (await redis.xpending(stream, group)) as [number, string | null, ...unknown[]];
const oldestPendingAgeMs = minId ? Date.now() - Number(minId.split("-")[0]) : 0;
```

| Signal | Meaning | Action |
|--------|---------|--------|
| **Lag** rising | Consumers can't keep up | Add workers, speed up the handler, partition |
| **Pending** rising | Entries delivered but not acknowledged | Slow or failing handlers, or dead consumers |
| **Oldest pending age** large | Something is stuck | Check reclaim, `minIdleMs`, poison entries |
| **DLQ length** > 0 | Entries exhausting retries | Investigate and replay |
| **Stream length** near the retention cap | Trimming may be eating unread entries | Raise retention or scale consumers |
| **Consumer idle time** high | A worker has stopped reading | Restart or investigate |

Alert on lag **relative to retention**: if lag approaches the number of entries you keep, the next trim will delete unread work.

## Failure modes

| Event | Result | Defense |
|-------|--------|---------|
| Worker crashes **after** the side effect, **before** `XACK` | Redelivered, so the work runs twice | Idempotent handlers |
| Worker crashes **before** the side effect | Redelivered, runs once | Reclaim and `minIdleMs` |
| Handler slower than `minIdleMs` | Another worker claims it, so it runs twice concurrently | Larger `minIdleMs`, smaller units of work |
| Redis failover (async replication) | Recent entries or acks may be lost, so duplicates or missing work | Idempotent consumers, AOF plus replicas, `WAIT` for critical writes |
| Trim removes unread entries | Silent data loss | Retention sized for lag, with alerts |
| Poison entry | Retried forever | `maxDeliveries` and a DLQ |
| Consumer deleted with pending entries | Entries orphaned | Claim before deleting |

## Testing

```ts
it("redelivers an entry that was never acknowledged", async () => {
  const id = await redis.xadd(S, "*", "n", "1") as string;
  await ensureGroup(redis, S, "g", "0");

  // consumer A reads but "crashes" without XACK
  await redis.xreadgroup("GROUP", "g", "a", "COUNT", 1, "STREAMS", S, ">");

  await sleep(50);
  const [, claimed] = (await redis.xautoclaim(S, "g", "b", 10, "0-0")) as [string, RawEntry[]];

  expect(claimed.map(([i]) => i)).toEqual([id]);        // consumer B can take it over
});
```

Use a real Redis, because pending lists, idle time and delivery counters are the behavior under test.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Acknowledging before the work completes | `XACK` only after success |
| Non-idempotent handlers | Deduplicate on `msgId` ([patterns](./04_stream-patterns.md#idempotent-consumers)) |
| No reclaim, so entries of dead workers stick forever | Run `XAUTOCLAIM` periodically |
| `minIdleMs` shorter than the slowest handler | Longer idle time, or smaller entries |
| Creating the group with `$` when history matters | `0` |
| Ignoring `BUSYGROUP` on startup | An idempotent `ensureGroup` |
| Failing entries retried forever | `maxDeliveries` and a DLQ |
| Trimming without tracking lag | Monitor lag, and size retention for it |
| Blocking connection reused for `XACK` | Separate connections |
| Deleting a consumer that still has pending entries | Claim first |
| Expecting order across multiple consumers | Partition streams by key |
| Random consumer names per restart | Stable names (or accept orphans plus reclaim) |

## Key takeaways

- A group divides one stream among workers, and tracks delivered-but-unacknowledged entries in the PEL
- **Read with `>`, process, then `XACK`**. Delivery is **at-least-once**
- Reclaim with `XAUTOCLAIM`, cap attempts, and dead-letter poison entries
- Monitor **lag, pending, oldest pending age and DLQ size**, and size retention for lag

**Next:** [Stream Patterns](./04_stream-patterns.md)
