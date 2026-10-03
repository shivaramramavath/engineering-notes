# High Availability and Failover

Replication, Sentinel and Cluster are mechanisms. **High availability** is a property you design, measure and rehearse: how long the service is unavailable when something breaks (**RTO**), how much data you can lose (**RPO**), and how your application behaves in between.

## Vocabulary

| Term | Meaning |
|------|---------|
| **Availability** | The fraction of time the service works |
| **RTO** (Recovery Time Objective) | How long an outage you can tolerate |
| **RPO** (Recovery Point Objective) | How much data (measured in time or writes) you can afford to lose |
| **Failure domain** | The unit that fails together: a process, host, rack, **availability zone**, region |
| **Failover** | Moving the primary role to a healthy node |
| **Split brain** | Two nodes both believe they are the primary |

| Availability | Downtime per year |
|--------------|-------------------|
| 99% | about 3.7 days |
| 99.9% | about 8.8 hours |
| 99.99% | about 53 minutes |
| 99.999% | about 5 minutes |

A Redis failover costing 30 seconds twice a year is nothing against 99.9%, but it is **a lot** if your application turns it into a 20-minute incident. **How your clients behave matters as much as the topology.**

## What Redis HA gives you, honestly

| Property | Typical reality |
|----------|-----------------|
| **RTO** | **Seconds to tens of seconds** (detection + election + client reconnection) |
| **RPO** | **Greater than zero.** Asynchronous replication loses the unreplicated tail on failover |
| Split brain | Possible during partitions, limited by `min-replicas-to-write` and quorum design |
| Cross-region active-active | **Not in open-source Redis** (vendors offer CRDT-based products) |

Because of the RPO, ask first: **can Redis be rebuilt?** Caches, sessions, rate limits and queues with idempotent jobs tolerate small losses. If Redis holds the only copy of money or orders, **change the design**, not just the topology.

## Topologies compared

| Topology | RTO | RPO | Handles | Doesn't handle |
|----------|-----|-----|---------|----------------|
| Standalone + persistence, auto-restart | Minutes (restart and reload) | Seconds (`appendfsync everysec`), or more with RDB only | Process crash | Host or disk loss |
| Primary + replica, **manual** failover | Minutes to hours (human) | Replication lag | Host loss, with an operator | Nights and weekends |
| Primary + replicas + **Sentinel** | **10 to 30 s** | Replication lag | Process and host loss, an AZ if placed well | Data beyond one node |
| **Cluster** (with replicas) | **Seconds to ~15 s per shard** | Replication lag, per shard | Shard primary loss, scale | Cross-shard transactions |
| **Managed Multi-AZ** | **Tens of seconds** (provider-specific) | Replication lag | Host and AZ loss, operations | Region loss (unless configured) |
| Cross-region async replica | Minutes (manual or scripted promotion) | **Replication lag across regions** | Region loss | Seamless failover |

## Anatomy of a failover

Where the time goes (illustrative defaults and typical tuned values):

```
t=0      primary dies
         ├─ detection:   Sentinel down-after-milliseconds (default 30 s, tuned 5–10 s)
         │               Cluster cluster-node-timeout     (default 15 s, tuned 5–10 s)
         ├─ agreement:   ODOWN quorum / majority vote                         (~ 1 s)
         ├─ election and promotion                                            (~ 1–3 s)
         ├─ discovery:   clients learn the new primary
         │               Sentinel: re-ask on reconnect or +switch-master
         │               Cluster:  MOVED / slot-map refresh
         │               Managed:  DNS TTL + reconnect
         └─ client recovery: retry backoff, offline queue, resent commands
t=RTO    writes succeed again
```

You can shorten **detection** (at the cost of false positives on a slow network) and **client recovery** (retry settings). You can't make election instantaneous.

Tune from both ends:

| Where | Knob | Trade-off |
|-------|------|-----------|
| Sentinel | `down-after-milliseconds` | Lower is faster, with more spurious failovers during network jitter |
| Cluster | `cluster-node-timeout` | Same |
| Client | `retryStrategy`, `sentinelRetryStrategy`, `clusterRetryStrategy` | Short, jittered backoff reconnects quickly without stampeding |
| Client | `commandTimeout`, `connectTimeout` | Low values fail fast instead of hanging requests |
| Managed | Endpoint DNS TTL | Clients must re-resolve on reconnect |

## Making the client resilient (ioredis)

During a failover, commands fail, connections drop, and replies are lost. Configure ioredis for it ([Configuration](../03_ioredis-basics/02_configuration.md)):

```ts
const redis = new Redis({
  // topology options (sentinels, or use `Cluster`) go here

  connectTimeout: 5_000,
  commandTimeout: 2_000,                                  // never hang a request on a dead node
  keepAlive: 10_000,                                      // notice dead peers sooner

  retryStrategy: (n) => Math.min(n * 200, 3_000) + Math.floor(Math.random() * 200),   // fast, bounded, jittered
  reconnectOnError: (err) => (err.message.includes("READONLY") ? 2 : false),          // demoted primary: reconnect, resend

  maxRetriesPerRequest: 3,                                // fail the request after a few tries
  enableOfflineQueue: true,                               // or false to fail fast, see below
  autoResendUnfulfilledCommands: true,                    // read the warning below
});
```

### Two settings that decide whether a failover corrupts your data

**`autoResendUnfulfilledCommands`** (default `true`): commands that were **sent but unanswered** when the connection died are **resent** after reconnecting. The first attempt may have been **executed before the connection broke**, so a resent `INCR`, `LPUSH` or `XADD` can apply **twice**.

| Commands | Safe to resend? |
|----------|-----------------|
| `SET`, `GET`, `DEL`, `HSET`, `SADD`, `ZADD` (same member and score) | Yes, **idempotent** |
| `INCR`, `LPUSH`, `RPUSH`, `XADD`, `ZINCRBY`, `PUBLISH` | **No**, they duplicate |

If your workload is mostly non-idempotent writes and duplicates are worse than failures, set it to `false` and handle retries yourself, with idempotency keys where needed. Otherwise keep the default and **design operations to be idempotent**.

**`enableOfflineQueue`**: while disconnected, commands queue up and run on reconnect. That **hides short blips**, but a long outage piles up memory and stale requests, and **user requests wait**. For request paths, prefer failing fast:

| Path | Offline queue | Behavior |
|------|---------------|----------|
| HTTP request handlers | `false`, or a low `maxRetriesPerRequest` with `commandTimeout` | Fail fast, degrade or return `503` |
| Background workers and BullMQ workers | `true` and `maxRetriesPerRequest: null` | Wait patiently |

### Application-level behavior

| Pattern | Use |
|---------|-----|
| **Fail open** for caches | A Redis problem becomes a miss ([Cache-Aside](../07_caching/01_cache-aside.md)) |
| **Fail closed** for locks and security checks | No Redis means no lock and no session verification ([Locks](../11_distributed-locks/README.md), [Sessions](../13_session-management/01_session-storage.md#when-redis-is-unavailable)) |
| **Local fallbacks** | An in-memory rate limiter while Redis is away ([Distributed Rate Limiter](../12_rate-limiting/03_distributed-rate-limiter.md#a-local-fallback)) |
| **Circuit breaker** | Stop hammering a failing Redis, and recover gradually |
| **Idempotency keys** | Make retried writes safe ([Stream Patterns](../10_streams/04_stream-patterns.md#idempotent-consumers)) |
| **Outbox** | Don't lose events when the broker is briefly unavailable ([Outbox relay](../10_streams/04_stream-patterns.md#outbox-relay)) |
| **Short timeouts everywhere** | A slow Redis is worse than a dead one |
| **Graceful degradation** | Serve stale or reduced features, and show clear errors |

## The data-loss window

Walk through what is lost:

```
t0  client writes X → primary acks → (X is in flight to replicas, not yet applied)
t1  primary crashes
t2  a replica without X is promoted
t3  X is gone, even though the client was told "OK"
```

Ways to narrow it, with costs:

| Mitigation | Effect | Cost |
|------------|--------|------|
| `appendonly yes`, `appendfsync everysec` | Survive a **restart** (lose about 1 s locally) | Slight overhead. Doesn't help if the node is gone |
| `appendfsync always` | Strongest local durability | Significant throughput cost |
| `WAIT numreplicas timeout` | The write is on ≥ N replicas before you proceed | Latency, and still not a strict guarantee |
| `WAITAOF` (Redis 7.2+) | Also wait for `fsync` locally and/or on replicas | More latency |
| `min-replicas-to-write` | Primary refuses writes when replicas are missing or lagging | **Availability** (writes fail instead of risking loss) |
| More replicas in different AZs | Higher chance a copy survives | Cost, and cross-AZ latency |
| **Idempotent, replayable writes** | Rebuild or replay what was lost | Application design |
| **Source of truth elsewhere** | Redis can always be rebuilt | Architecture |

**There is no setting that turns Redis into a synchronous, zero-loss store.** Choose your RPO deliberately, write it down, and design the application to live with it.

## Split brain and fencing

During a partition, an isolated old primary may accept writes that vanish at heal time. Defenses:

- Quorum-based Sentinel placement (three failure domains) and Cluster's majority rule
- `min-replicas-to-write` and `min-replicas-max-lag`
- **Fencing tokens** and conditional writes for anything that must not be applied by a stale actor, in particular **distributed locks**, which can be granted twice across a failover ([Lock Expiration and Renewal](../11_distributed-locks/02_lock-expiration-and-renewal.md#fencing-tokens))

## Multi-AZ and multi-region

### Across availability zones (recommended)

- Put the primary, replicas and **Sentinels** in **different AZs**, so one AZ failure leaves a majority
- Cross-AZ latency adds a little to replication lag and to client round trips. It is usually worth it
- Keep **app instances in the same AZs**, and prefer replica reads from the **local** AZ where your platform supports it
- Managed services offer **Multi-AZ** replication groups, so enable it

### Across regions

- Open-source Redis doesn't do active-active. Typical designs are **active-passive**: an asynchronous replica in another region for **disaster recovery** (higher lag, manual or scripted promotion, and DNS or config changes)
- **Don't stretch** a Sentinel or Cluster across high-latency regions. Failure detection becomes unreliable
- For active-active needs, either **partition by region** (each region owns its own users and data) or use a vendor product with conflict resolution
- Practice **region failover** too: data lag, DNS TTLs, cold caches and capacity in the target region

## Measuring your real RPO and RTO

Don't guess. A **continuous writer with sequence numbers** tells you both:

```ts
// chaos-writer.ts: run it, then trigger a failure
import { Redis } from "ioredis";

const redis = new Redis({ /* your topology */ commandTimeout: 1_000, maxRetriesPerRequest: 1, autoResendUnfulfilledCommands: false });
redis.on("error", () => {});

const acked = new Set<number>();
let seq = 0, errors = 0, firstErrorAt = 0, recoveredAt = 0;
const KEY = `chaos:log:${Date.now()}`;

const timer = setInterval(async () => {
  const n = seq++;
  try {
    await redis.rpush(KEY, String(n));                 // an acknowledged write
    acked.add(n);
    if (firstErrorAt && !recoveredAt) recoveredAt = Date.now();
  } catch {
    errors++;
    if (!firstErrorAt) firstErrorAt = Date.now();
  }
}, 10);

process.on("SIGINT", async () => {
  clearInterval(timer);
  await new Promise((r) => setTimeout(r, 3_000));      // let the topology settle
  const stored = new Set((await redis.lrange(KEY, 0, -1)).map(Number));

  const lost = [...acked].filter((n) => !stored.has(n));
  console.log({
    attempted: seq,
    acknowledged: acked.size,
    errors,
    lostAcknowledgedWrites: lost.length,              // your empirical RPO, in writes
    outageSeconds: firstErrorAt && recoveredAt ? (recoveredAt - firstErrorAt) / 1000 : 0,   // your empirical RTO
  });
  process.exit(0);
});
```

- `lostAcknowledgedWrites` is your **measured RPO**: acknowledged by Redis, absent afterwards
- `outageSeconds` is your **measured RTO** as the application experienced it
- Duplicates (from resends) show up if you also check the list for repeated values. Set `autoResendUnfulfilledCommands: true` and compare

Run it again with `WAIT`, `min-replicas-to-write` and different timeouts, and **keep the numbers**.

## Game days: test failure on purpose

| Scenario | How | What to watch |
|----------|-----|---------------|
| Primary process killed | `kill -9`, or `docker kill` | Detection time, promotion, client reconnect, lost writes |
| Primary host stopped | `docker stop`, or stop the instance | Same, plus the old primary rejoining as a replica |
| **Network partition** | `docker network disconnect`, `iptables`, [Toxiproxy](https://github.com/Shopify/toxiproxy) | Split brain, writes to the isolated primary, cleanup after heal |
| Frozen primary | `redis-cli DEBUG SLEEP 60`, or `SIGSTOP` | A **slow, not dead** node, and spurious failovers |
| Replica lost | Stop a replica | Read fallback, resync behavior |
| Sentinel loss | Stop 1, then 2 of 3 | Failover still works, then stops (quorum lost) |
| Disk full or slow | Fill the volume, throttle I/O | AOF write failures, blocked writes |
| Out of memory | Lower `maxmemory`, or a container memory limit | `OOM` errors, eviction behavior, OOM kills |
| AZ outage | Take down everything in one zone | Does a majority survive? |
| Full resync | Wipe a replica | Primary load, latency impact |
| DNS or endpoint change (managed) | Trigger a managed failover | Clients re-resolve and reconnect |

For each: **write the expected outcome first**, run it, compare, then fix the gap. Rehearse in an environment shaped like production, and make it a **recurring** exercise, since topologies and code both drift.

## Runbooks

### After an automatic failover

1. Confirm the new topology: `INFO replication` on **every** node (exactly **one** primary), `SENTINEL master mymaster` or `CLUSTER NODES`
2. Check the **old primary**: did it rejoin as a replica? Why did it fail (OOM kill, disk, host, network)?
3. Check replication **lag and offsets** on the new replicas
4. Check **application errors** and recovery: did every service reconnect without a restart?
5. Estimate the **loss window** (writes in flight around the failure). Reconcile if needed (replay idempotent work, check queues and sessions)
6. Restore redundancy (replace the lost node), and write the post-incident notes

### Planned maintenance

1. Upgrade or patch **replicas first**
2. **Fail over deliberately** (`SENTINEL failover mymaster`, or `CLUSTER FAILOVER` on a replica), during a quiet period
3. Upgrade the **old primary** (now a replica)
4. Verify topology, lag and client behavior

Never restart the primary "to see if it helps" in a replicated setup without checking that replicas are in sync and that the primary has persistence, or an empty restart can propagate emptiness ([hazard](./01_replication.md#1-a-primary-that-restarts-empty-can-wipe-its-replicas)).

## Monitoring and alerts

| Alert | Why |
|-------|-----|
| Primary count ≠ 1 (per shard) | Split brain, or no primary |
| Replica count below target | Redundancy lost |
| Replication lag above budget | Higher RPO, stale reads |
| `+switch-master` or Cluster failover events | Investigate every one |
| Sentinel quorum unhealthy (`ckquorum` fails) | Failover is currently impossible |
| `cluster_state` ≠ ok | The cluster is not serving |
| Client error rate and latency | The user-visible impact |
| Memory above ~80% of `maxmemory`, or OOM kills | The most common cause of "mysterious" failovers |
| Persistence errors (`aof_last_write_status`, `rdb_last_bgsave_status`) | The safety net is broken |
| Disk space on data volumes | AOF and RDB need room |

Alert on **symptoms** (error rate, latency) as well as **causes** (lag, node state), because each catches things the other misses.

## Choosing for your situation

| Situation | Reasonable design |
|-----------|-------------------|
| Pure cache, rebuildable, tolerant of a cold start | Standalone (or managed) with a warm-up plan. HA optional |
| Sessions, rate limits, queues, **data fits on one node** | Primary + replicas + Sentinel (3 AZs), or **managed Multi-AZ** |
| Data or throughput exceeds one node | **Cluster** (or managed cluster mode) with replicas |
| Redis holds the **only copy** of important data | Reconsider: keep the source of truth in a durable database |
| Regional disaster recovery needed | Cross-region async replica, plus rehearsed promotion |
| Small team, limited ops time | **Managed service**, and spend the effort on client resilience and testing |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Believing HA means zero data loss | Define RPO, measure it, design around it |
| Never testing failover | Game days with a sequence-number writer |
| Default client settings during failover | Tuned timeouts, jittered retries, `reconnectOnError` |
| `autoResendUnfulfilledCommands` with non-idempotent writes | Idempotent operations, or turn it off |
| Offline queue holding user requests for minutes | Fail fast on request paths |
| Detection set too aggressively (false failovers) or too lazily (long outages) | Tune with real network data, and test |
| All nodes and Sentinels in one AZ | Spread across failure domains |
| Stretching HA across high-latency regions | Active-passive DR instead |
| Replication treated as backup | Snapshots and off-host backups ([Backup and Disaster Recovery](../19_redis-production/03_backup-and-disaster-recovery.md)) |
| Ignoring memory headroom | OOM kills trigger failovers, so size for fork and growth |
| Only alerting on node state | Add client-visible error and latency alerts |
| Treating Redis as a system of record | Keep truth in a database |

## Key takeaways

- HA is a **property you measure**: RTO (seconds to tens of seconds) and RPO (greater than zero with async replication)
- Failover time = **detection + election + client rediscovery**, so tune both server and **client** settings
- Watch `autoResendUnfulfilledCommands` and the offline queue, because they decide whether a failover duplicates or hangs requests
- Narrow the loss window with `WAIT`, `min-replicas-to-write` and persistence, but **design for some loss**, with idempotency and a durable source of truth
- **Test failures on purpose** with a sequence-number writer, and keep runbooks current
- When in doubt, a **managed Multi-AZ service plus a resilient client** is the best effort-to-reliability trade

**Next module:** [16_performance](../16_performance/README.md)
