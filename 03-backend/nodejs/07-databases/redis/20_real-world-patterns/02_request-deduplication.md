# Request Deduplication

Deduplication means **recognizing something you have already handled and ignoring the repeat**. Redis is a natural fit: one atomic `SET NX EX` answers "have I seen this?" in a single round trip and forgets automatically.

```
event arrives ─► SET dedup:{id} 1 NX EX 86400
                    │
         OK (first) ┴── nil (seen before)
            │                  │
         process            ignore (acknowledge)
```

## The problem

| Source of duplicates | Example |
|----------------------|---------|
| User behavior | Double-click on "Submit", refresh after POST |
| Client retries | Timeouts and automatic retries |
| Webhooks | Providers redeliver until they see a `2xx` |
| At-least-once messaging | Queues and streams redeliver after a crash |
| Multiple producers | Two services enqueue the same job |
| Noisy monitoring | The same alert fires every 10 seconds |
| Expensive identical requests | 500 users ask for the same report at once |

Deduplication differs from [idempotency](./01_idempotency.md): a duplicate is simply **dropped**, no stored response is replayed.

## Techniques at a glance

| Technique | Redis tools | Good for | Watch out for |
|-----------|-------------|----------|---------------|
| Seen-key with TTL | `SET NX EX` | Webhooks, messages, form submits | Window must exceed redelivery delay |
| Content hash | `SET NX EX` on `sha256(payload)` | Same payload from different senders | Normalize before hashing |
| Daily set | `SADD` and `EXPIRE` | Counting, auditing what was seen | Memory grows with IDs |
| Leading-edge throttle | `SET NX PX` | "At most once per window" | First event wins, later ones are dropped |
| Trailing-edge debounce | `SET` plus a delayed check | "Run once after a burst ends" | Needs a timer or delayed job |
| Cooldown with counter | `SET NX EX` plus `INCR` | Alert suppression with a count | Counter lifecycle |
| In-flight coalescing | Lock plus result key | Many identical expensive requests | Waiters need timeouts |
| Job ID | BullMQ `jobId` | Enqueue the same job once | IDs have format rules |
| Probabilistic filter | Bloom filter | Huge volumes, soft requirements | False positives drop real events |

## The basic pattern

```ts
export async function firstTime(redis: Redis, key: string, ttlSec: number): Promise<boolean> {
  return (await redis.set(key, "1", "EX", ttlSec, "NX")) === "OK";
}

if (!(await firstTime(redis, `dedup:order:${orderId}`, 3600))) {
  return;                       // duplicate: ignore
}
await handleOrder(orderId);
```

Why it works: `SET ... NX` is **one atomic command**. Two concurrent callers cannot both get `OK`. The TTL makes the memory self-cleaning.

Do not write it as `EXISTS` then `SET`. Two callers can both see "missing" and both proceed.

### Choosing the window

The dedup window must be **at least as long as the longest delay after which a duplicate can arrive**:

| Source | Typical window |
|--------|----------------|
| Double-click | 2 to 10 seconds |
| HTTP client retries | A few minutes |
| Webhook redelivery | Check provider docs, often hours to days. 24 to 72 h is common |
| Queue redelivery | Longer than visibility timeout and retry schedule |

Memory is roughly the number of IDs in the window times the per-key cost (key length plus about 50 to 100 bytes of overhead, so measure with `MEMORY USAGE`). 10 million IDs for 24 hours is a real cost.

## Mark before or after processing?

This decision decides which kind of failure you get.

| Order | If processing fails or the process crashes | Result |
|-------|--------------------------------------------|--------|
| Mark **first**, then process | The ID is marked but the work was never done | **Lost event** (at-most-once) |
| Process, then mark | A redelivery sees no mark and runs again | **Duplicate work** (at-least-once) |

For most systems, the safer answer is **claim, process, confirm**:

```
claim (short TTL) ──► process ──► mark done (long TTL) ──► release claim
       │                                  
       └── crash: the claim expires, a redelivery can retry
```

```ts
const RELEASE = `
if redis.call("GET", KEYS[1]) == ARGV[1] then return redis.call("DEL", KEYS[1]) end
return 0
`;

export async function processOnce(
  redis: Redis,
  id: string,
  work: () => Promise<void>,
  opts = { claimTtlSec: 30, doneTtlSec: 7 * 86_400 },
): Promise<"processed" | "duplicate" | "in-flight"> {
  const claimKey = `dedup:claim:${id}`;
  const doneKey = `dedup:done:${id}`;

  if (await redis.exists(doneKey)) return "duplicate";

  const token = randomUUID();
  if ((await redis.set(claimKey, token, "EX", opts.claimTtlSec, "NX")) !== "OK") return "in-flight";

  try {
    await work();
    await redis.set(doneKey, "1", "EX", opts.doneTtlSec);
    return "processed";
  } finally {
    await redis.eval(RELEASE, 1, claimKey, token);   // never delete someone else's claim
  }
}
```

If `work()` throws, the done marker is **not** written, so the redelivery retries. A crash between `work()` and writing `doneKey` can still repeat the work once, so keep the side effect itself safe to repeat where it matters (see [Idempotency](./01_idempotency.md)).

## Recipes

### Webhook receiver

Providers retry until they get a `2xx`, so **acknowledge duplicates** rather than failing them.

```ts
app.post("/webhooks/provider", express.raw({ type: "application/json" }), async (req, res) => {
  if (!verifySignature(req.header("X-Signature"), req.body)) return res.sendStatus(401);

  const event = JSON.parse(req.body.toString("utf8"));
  const key = `dedup:webhook:provider:${event.id}`;

  if (!(await firstTime(redis, key, 3 * 86_400))) {
    return res.sendStatus(200);                         // already handled: acknowledge
  }

  try {
    await queue.add("process-webhook", event, { jobId: `wh-${event.id}` });   // do the work asynchronously
    res.sendStatus(200);
  } catch (err) {
    await redis.del(key);                               // enqueue failed: allow the redelivery through
    res.sendStatus(500);
  }
});
```

Respond quickly and push heavy work to a queue. See [BullMQ](../14_queues-and-workers/04_bullmq.md).

### Deduplicate by content

When the sender provides no ID, derive one from the payload:

```ts
import { createHash } from "node:crypto";

// recursively sort object keys so equal payloads serialize identically
function sortKeys(v: unknown): unknown {
  if (Array.isArray(v)) return v.map(sortKeys);
  if (v && typeof v === "object") {
    return Object.fromEntries(Object.entries(v as object).sort(([a], [b]) => a.localeCompare(b)).map(([k, x]) => [k, sortKeys(x)]));
  }
  return v;
}

function contentKey(prefix: string, payload: unknown) {
  const canonical = JSON.stringify(sortKeys(payload));          // stable key order
  return `${prefix}:${createHash("sha256").update(canonical).digest("hex")}`;
}

if (!(await firstTime(redis, contentKey("dedup:comment", { userId, postId, text }), 30))) {
  return res.status(409).json({ error: "Duplicate submission" });
}
```

Normalize before hashing (trim, lowercase where appropriate, drop volatile fields like timestamps), or near-duplicates slip through.

### Leading-edge throttle: "at most once per window"

```ts
// at most one "password reset email" per user per 60 seconds
const allowed = (await redis.set(`throttle:reset:${userId}`, "1", "EX", 60, "NX")) === "OK";
if (!allowed) return res.status(429).json({ error: "Please wait before requesting another email" });
await sendResetEmail(userId);
```

For smooth limits across many requests, use a rate limiter instead (see [Fixed and Sliding Window](../12_rate-limiting/01_fixed-and-sliding-window.md)). This pattern is for "one per window".

### Trailing-edge debounce: "run once after the burst ends"

Useful for "re-index this document after the user stops editing".

```ts
async function touch(docId: string, windowMs = 5000) {
  const stamp = `${Date.now()}-${randomUUID()}`;
  await redis.set(`debounce:doc:${docId}`, stamp, "PX", windowMs * 4);
  await queue.add("reindex", { docId, stamp }, { delay: windowMs });          // delayed job
}

// worker
async function reindexJob({ docId, stamp }: { docId: string; stamp: string }) {
  const latest = await redis.get(`debounce:doc:${docId}`);
  if (latest !== stamp) return;          // a newer edit arrived: its own job will run
  await reindex(docId);
}
```

Every event schedules a job, but only the one carrying the latest stamp does the work.

### Alert suppression with a counter

```ts
export async function shouldNotify(fingerprint: string, cooldownSec = 600) {
  const first = (await redis.set(`alert:cool:${fingerprint}`, "1", "EX", cooldownSec, "NX")) === "OK";
  if (!first) {
    await redis.incr(`alert:suppressed:${fingerprint}`);
    await redis.expire(`alert:suppressed:${fingerprint}`, cooldownSec * 2);
    return { send: false as const };
  }
  const suppressed = Number((await redis.getdel(`alert:suppressed:${fingerprint}`)) ?? 0);   // GETDEL: Redis 6.2+
  return { send: true as const, suppressed };           // "…and 37 similar alerts since the last one"
}
```

The fingerprint should be stable and low-cardinality (for example `service:errorType`), not include timestamps or request IDs.

### A daily set of seen IDs

When you also need to **list or count** what was seen:

```ts
async function markSeen(id: string): Promise<boolean> {
  const day = new Date().toISOString().slice(0, 10);
  const key = `seen:orders:${day}`;
  const [[, added]] = (await redis.multi().sadd(key, id).expire(key, 3 * 86_400, "NX").exec())!;   // EXPIRE NX: Redis 7.0+
  return added === 1;
}
```

IDs delivered a day late can slip past the boundary, so check yesterday's set too (`SISMEMBER`) when your window spans midnight. Rotating daily keys expire in one piece, which is cheaper than millions of individual keys. Very large sets are better bucketed (see [Memory Optimization](../16_performance/03_memory-optimization.md)).

### Coalescing identical in-flight requests

When many callers want the same expensive result, let one compute it and the rest wait. This is the same idea as cache-stampede protection (see [Cache Problems](../07_caching/04_cache-problems.md)).

```ts
export async function coalesced<T>(key: string, compute: () => Promise<T>, waitMs = 5000): Promise<T> {
  const lockKey = `coalesce:lock:${key}`;
  const resultKey = `coalesce:result:${key}`;

  const cached = await redis.get(resultKey);
  if (cached) return JSON.parse(cached);

  const token = randomUUID();
  if ((await redis.set(lockKey, token, "PX", 15_000, "NX")) === "OK") {
    try {
      const value = await compute();
      await redis.set(resultKey, JSON.stringify(value), "PX", 10_000);   // short-lived shared result
      return value;
    } finally {
      await redis.eval(RELEASE, 1, lockKey, token);
    }
  }

  const deadline = Date.now() + waitMs;                                   // someone else is computing
  while (Date.now() < deadline) {
    const hit = await redis.get(resultKey);
    if (hit) return JSON.parse(hit);
    await new Promise((r) => setTimeout(r, 50));
  }
  return compute();                                                      // timed out: do it ourselves
}
```

Always bound the wait, and have a fallback so a crashed leader cannot stall everyone.

### Job deduplication in queues

With BullMQ, a custom `jobId` makes adding the same job again a no-op while the original still exists:

```ts
await queue.add("send-invoice", { invoiceId }, { jobId: `invoice-${invoiceId}` });
```

Notes: custom IDs cannot contain a colon, and a completed job removed by `removeOnComplete` frees the ID for reuse. Recent BullMQ versions also offer a dedicated deduplication option with its own TTL, so check the docs for your version. See [BullMQ](../14_queues-and-workers/04_bullmq.md).

### Probabilistic filters (use with care)

A Bloom filter answers "definitely new" or "probably seen" in very little memory:

```ts
await redis.call("BF.RESERVE", "seen:events", "0.001", "10000000");   // 0.1% false positives, 10M items
const isNew = (await redis.call("BF.ADD", "seen:events", id)) === 1;
```

Availability depends on your Redis version and distribution (a built-in or module data type). The catch: **false positives silently drop real events**. Use it for soft cases (analytics, "have we shown this banner") and never where losing one event is unacceptable. `HyperLogLog` counts unique items but cannot tell you whether a specific item was seen, so it is not a dedup filter.

## Where to deduplicate

| Layer | Pros | Cons |
|-------|------|------|
| Edge or API gateway | Cheap, protects everything behind it | Limited business context |
| Request handler | Rich context, can return meaningful errors | Each handler must remember |
| Message consumer | Covers queue redelivery | Needs stable message IDs |
| Database unique constraint | The real guarantee | Slower, errors need handling |

Use Redis for the **fast, early** filter and keep a database constraint behind anything where a duplicate is costly.

## Testing

```ts
it("lets exactly one concurrent caller through", async () => {
  const results = await Promise.all(Array.from({ length: 50 }, () => firstTime(t.redis, "dedup:x", 30)));
  expect(results.filter(Boolean)).toHaveLength(1);
});

it("allows a retry after processing fails", async () => {
  await expect(processOnce(t.redis, "m1", async () => { throw new Error("boom"); })).rejects.toThrow();
  expect(await processOnce(t.redis, "m1", async () => {})).toBe("processed");
});

it("reports duplicates after success", async () => {
  await processOnce(t.redis, "m2", async () => {});
  expect(await processOnce(t.redis, "m2", async () => {})).toBe("duplicate");
});

it("forgets after the window", async () => {
  await t.redis.set("dedup:y", "1", "PX", 50, "NX");
  await waitFor(() => t.redis.exists("dedup:y"), (n) => n === 0);
  expect(await firstTime(t.redis, "dedup:y", 30)).toBe(true);
});
```

Also test the webhook route by posting the same event several times in parallel and asserting that only one job was enqueued.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `EXISTS` then `SET` | A single `SET NX` |
| Window shorter than the redelivery delay | Size it from the source's retry behavior |
| Marking before processing with no rollback | Claim with a short TTL, mark done after success, delete on failure |
| Failing duplicate webhooks with `4xx` or `5xx` | Acknowledge with `200` so the sender stops retrying |
| Hashing unnormalized payloads | Canonicalize, drop volatile fields |
| Unbounded sets of IDs | TTL and rotating keys, or bucketing |
| Using a Bloom filter where every event matters | Exact keys, or a database constraint |
| Dedup keys without a namespace | `dedup:{source}:{id}` to avoid collisions |
| Waiting forever on a coalescing leader | Bound the wait and fall back |
| Treating dedup as exactly-once | Still make side effects safe to repeat |

## Key takeaways

- `SET key value NX EX seconds` is the atomic building block, never check then set
- Pick the window from the longest realistic duplicate delay, and mind the memory it implies
- Claim, process, then confirm, so failures can be retried and crashes do not lose events
- Acknowledge duplicate webhooks, suppress duplicate alerts with a counter and use throttle, debounce and coalescing for bursts
- Redis is the fast filter, and a database constraint is the guarantee
- Test with parallel callers, failure and expiry

**Previous:** [Idempotency](./01_idempotency.md) | **Next:** [Leaderboard](./03_leaderboard.md)
