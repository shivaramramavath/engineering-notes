# Redlock

A single-instance Redis lock has one weak spot: if that Redis fails over, the lock can be **lost or granted twice**. **Redlock** is an algorithm that acquires a lock on **several independent Redis instances** and treats it as held only if a **majority** agree.

It is also one of the most debated algorithms in distributed systems. This lesson explains how it works, what the criticism is, and how to choose sensibly.

## The problem Redlock targets

```
Single Redis, async replication:

A: SET lock ─► primary ✓            primary crashes before replicating the key
                                    replica promoted (no lock key)
B: SET lock ─► new primary ✓        ← A and B both hold "the lock"
```

Redlock's idea: don't trust one node, ask several.

## The algorithm

Setup: **N independent masters** (typically 5). They are **not replicas of each other** and not nodes of one coordinated cluster. They fail independently.

To acquire a lock with TTL `T`:

1. Record the start time
2. Try to acquire the lock **on each instance, one after another**, with the **same key and random token**, using `SET key token NX PX T` and a **short per-instance timeout** (much smaller than `T`), so a dead instance doesn't stall you
3. Compute the **elapsed time**
4. The lock is **acquired** only if you got it on a **majority** (`N/2 + 1`, so 3 of 5) **and** the elapsed time is **less than `T`** (after allowing for clock drift)
5. The lock's **validity time** is `T − elapsed − drift`
6. If acquisition fails, **release the lock on all instances** (including ones you think failed) and retry after a random delay

Release: run the compare-and-delete script on **every** instance.

```
         ┌─ Redis 1 ✓
client ──┼─ Redis 2 ✓      3 of 5 granted within the time budget → acquired
         ├─ Redis 3 ✓      validity = TTL − elapsed − drift
         ├─ Redis 4 ✗ (timeout)
         └─ Redis 5 ✗
```

Why it is meant to be safer: losing the lock now needs a **majority** of independent nodes to lose it, not one failover.

## Using it from Node.js

The widely used library is **`redlock`** (node-redlock) v5, which works with ioredis. Check its README for the exact API of the version you install.

```bash
npm install redlock ioredis
```

```ts
import Redlock from "redlock";
import { Redis } from "ioredis";

const clients = [
  new Redis({ host: "redis-a.internal" }),
  new Redis({ host: "redis-b.internal" }),
  new Redis({ host: "redis-c.internal" }),
  new Redis({ host: "redis-d.internal" }),
  new Redis({ host: "redis-e.internal" }),
];

const redlock = new Redlock(clients, {
  driftFactor: 0.01,                     // clock drift allowance, as a fraction of the TTL
  retryCount: 10,
  retryDelay: 200,                       // ms between attempts
  retryJitter: 200,                      // random extra delay
  automaticExtensionThreshold: 500,      // when using(), extend if less than this remains (ms)
});

redlock.on("error", (err) => console.error("redlock error", err));   // required: otherwise errors can go unnoticed
```

### `using`: acquire, auto-extend, release

```ts
await redlock.using(["shop:lock:order:1042"], 10_000, async (signal) => {
  const order = await loadOrder(1042);

  if (signal.aborted) throw signal.error;            // the lock could not be extended
  await processOrder(order);

  if (signal.aborted) throw signal.error;
});
```

`using` extends the lock automatically while your routine runs and **aborts the `signal`** if it fails to, the same idea as the [`LockManager` watchdog](./02_lock-expiration-and-renewal.md#the-lock-handle).

### Manual control

```ts
const lock = await redlock.acquire(["shop:lock:report"], 30_000);
try {
  await buildReport();
  await lock.extend(30_000);                         // extend the lease if needed
} finally {
  await lock.release();
}
```

### With a single Redis instance

Passing **one** client is valid, and gives you a tested implementation of token ownership, safe release and extension, with the same guarantees as a single-instance lock:

```ts
const redlock = new Redlock([redis]);                // single-node mode
```

For most applications this is the practical way to use the library: you get a vetted lock without running five Redis servers.

### Operating the five nodes

| Topic | Guidance |
|-------|----------|
| **Independence** | Separate hosts or availability zones, separate failure domains. Not five replicas of one primary |
| **Not a Redis Cluster** | Redlock needs independent instances where each client talks to all of them. A Cluster routes one key to one shard |
| **Persistence** | A node that **restarts and forgets** its locks can break the majority guarantee. Use AOF with `fsync always`, or **delay restarts** of a crashed node for longer than the maximum lock TTL |
| **Latency** | Acquisition takes about as long as the slowest instance needed for a majority. Keep them close |
| **Managed services** | Many providers expose a single endpoint, so five independent instances may mean five separate deployments |
| **Cost** | Five servers (plus monitoring) to guard one kind of operation |

## The safety debate

Redlock's safety was challenged publicly by Martin Kleppmann, and defended by its author, Salvatore Sanfilippo (antirez). The useful takeaways, without taking sides on every point:

**The criticism**

- Redlock's safety relies on **timing assumptions** (bounded clock drift, bounded pauses, bounded network delays). Real systems have process pauses, clock jumps and delayed packets that break those assumptions
- Redlock hands out **random tokens, not monotonically increasing ones**, so there's no natural **fencing token** for the protected resource to compare
- Consequently it can be **too heavy for efficiency locks** (a single Redis is simpler) and **not safe enough for correctness locks** (use a consensus system, plus fencing)

**The defense**

- A client can check the **validity time** after acquiring, and abort if too much time was spent
- The random token can still be used for **check-and-set style protection** at the resource (compare the token you hold with what's stored)
- The timing assumptions are reasonable for many deployments, and the algorithm is a clear improvement over a single node

**Where both views agree**

- For **efficiency**, either a single instance or Redlock is acceptable, and the **single instance is simpler**
- For **hard correctness**, **don't rely on any lock's timing alone**. Make the **protected resource** enforce safety (fencing, conditional writes, idempotency)
- No lock removes the need to design for a **holder that is paused or slow**

## Choosing: a decision guide

```
What happens if two processes occasionally run at once?
│
├─ "A duplicate email / wasted work. Fine."          ─► single Redis lock (lessons 01 and 02)
│
├─ "Bad, but the protected resource can check"       ─► single Redis lock + fencing tokens or conditional writes
│     (database row, versioned API)
│
├─ "Bad, and we want fewer failover windows,         ─► Redlock (or a library using several nodes)
│   we already operate several independent Redis"       + still fence the resource
│
└─ "Data corruption or money. Must be right."        ─► consensus-based coordination (etcd, ZooKeeper, Consul),
                                                         database transactions/locks, + fencing and idempotency
```

| Option | Strength | Weakness |
|--------|----------|----------|
| **Single Redis lock** | Simple, fast, one dependency you already have | Failover can grant it twice |
| **Single Redis + fencing** | Safe against stale holders at the resource | The resource must enforce the token |
| **Redlock** | Survives loss of a minority of nodes | Operational cost, timing assumptions, no fencing token |
| **Database lock or advisory lock** | Strongly consistent with the data it protects | Ties locking to DB connections and load |
| **etcd, ZooKeeper, Consul** | Built for coordination, real leases, ordered tokens | Extra infrastructure and operational skill |
| **Kubernetes Lease** | Built-in leader election for pods | Kubernetes only |
| **Queue or partitioned stream** | Removes the lock by serializing work | Needs an asynchronous design |
| **Idempotency + unique constraints** | Often removes the need for locking | Needs careful design |

## Practical recommendations

1. **Avoid locks where you can.** An atomic command, unique constraint, idempotency key or queue is usually simpler and safer
2. **Be explicit** whether a lock is for **efficiency** or **correctness**, and write it down next to the code
3. **Keep critical sections short** and bounded, with timeouts on every call inside them
4. **Use short leases with renewal and an abort signal** ([renewal](./02_lock-expiration-and-renewal.md))
5. **For correctness, make the resource enforce it:** fencing tokens, conditional updates, idempotency keys
6. **Lock the smallest resource**, never "everything"
7. **Monitor** lock-lost events, renewal failures, hold times and contention
8. **Test failure**: kill Redis, fail over, freeze a process, jump a clock, and observe what your protected data does

## Failure scenarios

| Event | Single Redis lock | Redlock | With fencing at the resource |
|-------|-------------------|---------|------------------------------|
| Holder crashes | Lock expires after the TTL | Same | Same |
| Holder paused past the TTL | Two holders may write | Two holders may write | **Stale write rejected** |
| Redis primary fails over (async replication) | May be granted twice | Survives (a minority loss is tolerated) | Rejected if the fence is monotonic |
| Redis node restarts without persistence | Lock forgotten, may be granted twice | Possible with unlucky timing, so delay restarts | Rejected if the fence is monotonic |
| Clock jump on a Redis node | A TTL can expire early or late | Weakens the majority and validity reasoning | Rejected if the fence is monotonic |
| Network partition around the client | Client can't renew, and loses the lock | Same | Rejected if the fence is monotonic |

The last column is the point: **only the protected resource can make a stale holder harmless**, regardless of which lock you choose.

## Testing

- Reuse the [mutual exclusion and stale release tests](./01_locking-fundamentals.md#testing-locks), which also work against Redlock
- Run Redlock against **several real Redis containers** (Testcontainers) and **stop two of five** mid-test to confirm the lock is still granted, then stop three to confirm it is refused
- Freeze a holder (a sleep longer than the lease with renewal off) and verify **the resource**, not the lock, rejects the stale write
- Restart one Redis node during a held lock and observe

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Using Redlock for correctness without fencing | Make the resource enforce safety |
| "Five Redis" that are replicas or Cluster shards | Independent masters, separate failure domains |
| No error listener on the Redlock instance | `redlock.on("error", ...)` |
| Nodes restarting immediately with an empty dataset | Delayed restarts or durable persistence |
| Very long TTLs to avoid renewal | Short leases with `using` or renewal |
| Treating Redlock as free safety | It adds cost and assumptions, so decide deliberately |
| Ignoring `signal.aborted` inside `using` | Check it between steps, and pass it to I/O |
| Using Redlock where a unique constraint would do | Remove the lock |
| Skipping failure testing | Chaos-test the holder, Redis nodes and clocks |

## Key takeaways

- Redlock acquires on a **majority of independent Redis instances** within a validity window
- It improves on single-node failover behavior, but **relies on timing assumptions and offers no fencing token**
- For **efficiency**, a single Redis lock is simpler. For **correctness**, **fence at the resource** and consider a consensus system or database locks
- The library with **one** client is a convenient, vetted single-node lock
- The best lock is often **no lock**: atomic operations, constraints, idempotency, or a queue

**Next module:** [12_rate-limiting](../12_rate-limiting/README.md)
