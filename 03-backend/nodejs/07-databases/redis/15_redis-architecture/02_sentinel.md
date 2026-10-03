# Sentinel

Replication copies data, but **someone has to notice the primary died, pick a replica, promote it, and tell clients**. **Redis Sentinel** is a set of watcher processes that does exactly that.

## What Sentinel does

| Job | Meaning |
|-----|---------|
| **Monitoring** | Pings the primary and replicas, and each other |
| **Failure detection** | Agrees, by quorum, that the primary is down |
| **Automatic failover** | Elects a leader Sentinel, which promotes the best replica and reconfigures the rest |
| **Configuration provider** | Clients ask Sentinel "who is the primary right now?" |
| **Notification** | Publishes events (`+switch-master`, `+sdown`, …) you can subscribe to |

What it does **not** do: it doesn't **shard** (all data still lives on one primary), and it doesn't make replication synchronous (the loss window remains).

## Topology

```
                       ┌───────────┐ ┌───────────┐ ┌───────────┐
                       │ sentinel 1│ │ sentinel 2│ │ sentinel 3│    ≥ 3, in independent failure domains
                       └─────┬─────┘ └─────┬─────┘ └─────┬─────┘
                  monitor    │             │             │
            ┌────────────────┴─────────────┴─────────────┘
            ▼                      ▼                     ▼
       ┌─────────┐   async    ┌─────────┐          ┌─────────┐
       │ PRIMARY │ ─────────► │ replica │          │ replica │
       └─────────┘            └─────────┘          └─────────┘
 clients: ask a Sentinel for the primary's address, then connect to it
```

### How many Sentinels?

Use an **odd number, at least three**, on **separate machines or availability zones**. Two Sentinels can never form a majority after losing one.

| Sentinels | Quorum (typical) | Majority needed to authorize a failover | Tolerates losing |
|-----------|------------------|------------------------------------------|------------------|
| 3 | 2 | 2 | 1 Sentinel |
| 5 | 3 | 3 | 2 Sentinels |

**Quorum** is how many Sentinels must agree the primary is down (**ODOWN**). Authorizing and running the failover additionally needs a **majority of all Sentinels**. A single Sentinel on the same host as the primary is useless, because it dies together with it.

## Configuration

`sentinel.conf` (each Sentinel needs a **writable** copy, because it rewrites it as it learns the topology):

```
port 26379
sentinel resolve-hostnames yes
sentinel announce-hostnames yes

sentinel monitor mymaster 10.0.0.1 6379 2          # name, primary address, quorum
sentinel auth-pass mymaster <redis password>       # if the data nodes require AUTH
sentinel down-after-milliseconds mymaster 5000     # how long unresponsive before "subjectively down" (default 30000)
sentinel failover-timeout mymaster 30000
sentinel parallel-syncs mymaster 1                 # how many replicas resync from the new primary at once
```

| Setting | Effect |
|---------|--------|
| `down-after-milliseconds` | **Detection time.** Lower means faster failover and more risk of false positives on a slow network. The default (30 s) is conservative, and 5 to 10 s is common |
| `failover-timeout` | Time limits for failover phases and retries |
| `parallel-syncs` | `1` keeps most replicas serving reads during resync. Higher is faster but leaves fewer replicas available |
| `quorum` (last arg of `monitor`) | Agreement needed to declare the primary down |
| `replica-priority` (on the data nodes) | Lower is preferred for promotion, and `0` never promotes |

Also **secure Sentinel itself** (`requirepass`, ACLs, private network), because anyone who can talk to it can trigger failovers.

### A local lab (Docker Compose)

```yaml
services:
  redis-primary:
    image: redis:7
    command: ["redis-server", "--appendonly", "yes", "--requirepass", "devpass", "--masterauth", "devpass"]

  redis-replica-1:
    image: redis:7
    command: ["redis-server", "--appendonly", "yes", "--requirepass", "devpass", "--masterauth", "devpass",
              "--replicaof", "redis-primary", "6379"]
    depends_on: [redis-primary]

  redis-replica-2:
    image: redis:7
    command: ["redis-server", "--appendonly", "yes", "--requirepass", "devpass", "--masterauth", "devpass",
              "--replicaof", "redis-primary", "6379"]
    depends_on: [redis-primary]

  sentinel-1: &sentinel
    image: redis:7
    depends_on: [redis-primary, redis-replica-1, redis-replica-2]
    command: >
      sh -c "printf 'port 26379\nsentinel resolve-hostnames yes\nsentinel announce-hostnames yes\nsentinel monitor mymaster redis-primary 6379 2\nsentinel auth-pass mymaster devpass\nsentinel down-after-milliseconds mymaster 5000\nsentinel failover-timeout mymaster 30000\nsentinel parallel-syncs mymaster 1\n' > /tmp/sentinel.conf && redis-sentinel /tmp/sentinel.conf"
  sentinel-2: *sentinel
  sentinel-3: *sentinel
```

Run your **app inside the same Compose network** (it resolves `redis-primary` and `sentinel-*`). From the host machine those names don't resolve, and the Sentinels return container-internal addresses. Use ioredis's `natMap` to translate them, or publish ports and map accordingly.

## Inspecting Sentinel

```bash
redis-cli -p 26379 SENTINEL get-master-addr-by-name mymaster      # the current primary
redis-cli -p 26379 SENTINEL master mymaster                       # state, flags (s_down, o_down), num-slaves, quorum
redis-cli -p 26379 SENTINEL replicas mymaster                     # known replicas and their state
redis-cli -p 26379 SENTINEL sentinels mymaster                    # the other Sentinels
redis-cli -p 26379 SENTINEL ckquorum mymaster                     # can a failover be authorized right now?
redis-cli -p 26379 SENTINEL failover mymaster                     # trigger a manual failover (maintenance, testing)
```

## The failover sequence

```
1. A Sentinel sees no reply for down-after-milliseconds   → SDOWN (subjectively down)
2. ≥ quorum Sentinels agree                                → ODOWN (objectively down)
3. Sentinels elect a leader (majority vote, per config epoch)
4. The leader picks the best replica:
      lowest replica-priority → highest replication offset (most data) → lowest run ID
5. REPLICAOF NO ONE on the chosen replica                  → it is the new primary
6. Other replicas are reconfigured to follow it (parallel-syncs at a time)
7. Sentinels publish +switch-master, and clients learn the new address
8. The old primary, when it returns, is demoted to a replica of the new one
```

Total write unavailability is roughly `down-after-milliseconds` + election and promotion (seconds) + **client rediscovery and reconnection**. Plan for **10 to 30 seconds** unless you've tuned and tested it.

Replicas with the **most recent offset** are preferred, which minimizes (but doesn't eliminate) lost writes.

## Split brain and the isolated primary

```
network partition:   [ old PRIMARY + some clients ]  |  [ Sentinel majority + replicas ]
                         still accepts writes         |   promotes a replica → new primary
```

Once the partition heals, the old primary is demoted and **everything it accepted during the partition is discarded**. Reduce the damage with, on the primary:

```
min-replicas-to-write 1
min-replicas-max-lag 10
```

An isolated primary then stops accepting writes after about 10 seconds with no healthy replica. Put Sentinels in **three failure domains** so a single zone loss never splits the majority, and size `min-replicas-*` knowing it makes the primary refuse writes if all replicas fail.

## Connecting with ioredis

```ts
import { Redis } from "ioredis";

const redis = new Redis({
  sentinels: [
    { host: "sentinel-1", port: 26379 },
    { host: "sentinel-2", port: 26379 },
    { host: "sentinel-3", port: 26379 },
  ],
  name: "mymaster",                                   // the primary's name in sentinel.conf
  password: process.env.REDIS_PASSWORD,               // password for the data nodes
  sentinelPassword: process.env.SENTINEL_PASSWORD,    // password for Sentinel itself, if set
  role: "master",                                     // the default: connect to the primary

  sentinelRetryStrategy: (times) => Math.min(times * 100, 2_000),     // retry reaching the Sentinels
  failoverDetector: true,                             // actively subscribe to +switch-master (default is off)
  reconnectOnError: (err) => (err.message.includes("READONLY") ? 2 : false),

  maxRetriesPerRequest: 3,
  retryStrategy: (n) => Math.min(n * 200, 3_000) + Math.floor(Math.random() * 200),
  connectTimeout: 5_000,
});

redis.on("error", (err) => logger.warn({ err }, "redis error"));
redis.on("reconnecting", () => logger.warn("redis reconnecting"));
```

Check the option names against your ioredis version. They are among the more version-sensitive ones.

What ioredis does:

1. Connects to a Sentinel from your list (trying them in turn) and asks `SENTINEL get-master-addr-by-name`
2. Connects to the address returned and **verifies with `ROLE`** that it really is a primary
3. When the connection drops, it **asks the Sentinels again**, so after a failover the next reconnect lands on the new primary
4. With `failoverDetector: true` it also **subscribes to Sentinel events** and reacts to `+switch-master` immediately rather than waiting for a connection error
5. It updates its Sentinel list from the Sentinels it discovers (`updateSentinels`, on by default)

### Reading from replicas

Use a second client with `role: "slave"`:

```ts
const replicaClient = new Redis({
  sentinels: [...],
  name: "mymaster",
  password: process.env.REDIS_PASSWORD,
  role: "slave",
  preferredSlaves: [{ ip: "10.0.1.12", port: "6379", prio: 1 }],     // optional: prefer certain replicas, by priority
});
```

The same staleness rules apply ([Replication](./01_replication.md#reading-from-replicas)). Reads can lag, so keep correctness-sensitive reads on the primary client.

### Pub/Sub, blocking commands and workers

Give each its own client with the same Sentinel options (`redis.duplicate()` copies them). On failover the client reconnects and, with `autoResubscribe` (the default), **resubscribes**. Messages published during the gap are lost, as always with Pub/Sub ([Pub/Sub Fundamentals](../09_pub-sub/01_pub-sub-fundamentals.md#what-can-go-wrong)). BullMQ and other libraries accept an ioredis instance or options, so pass the Sentinel configuration through.

### TLS, NAT and containers

If clients cannot reach the addresses Sentinels announce (Docker Desktop, port-forwarding, cloud NAT), translate them:

```ts
new Redis({
  sentinels: [{ host: "127.0.0.1", port: 26379 }],
  name: "mymaster",
  natMap: { "redis-primary:6379": { host: "127.0.0.1", port: 6379 } },
});
```

For TLS to the data nodes, use `tls`. For TLS to Sentinels, ioredis has separate options (`enableTLSForSentinelMode`, `sentinelTLS`), so check the docs for your version.

## Subscribing to Sentinel events

Log failovers for your own monitoring:

```ts
const watcher = new Redis({ host: "sentinel-1", port: 26379, password: process.env.SENTINEL_PASSWORD });

await watcher.psubscribe("+switch-master", "+sdown", "-sdown", "+odown", "-odown", "+failover-end", "+tilt");
watcher.on("pmessage", (_pattern, channel, message) => {
  logger.warn({ channel, message }, "sentinel event");          // +switch-master mymaster oldip oldport newip newport
  if (channel === "+switch-master") metrics.increment("redis_failovers");
});
```

(`+tilt` means Sentinel detected a time anomaly and is temporarily not acting.)

## Observability

| Signal | Source | Alert when |
|--------|--------|------------|
| Exactly **one** primary | `INFO replication` on every data node | Zero or two (a split brain) |
| Replica count and state | `SENTINEL replicas` | Fewer than expected, or `s_down` |
| Sentinel quorum health | `SENTINEL ckquorum` | Fails |
| Number of Sentinels seen | `SENTINEL master` (`num-other-sentinels`) | Below expected |
| Failovers | `+switch-master` events | Any, since each should be investigated |
| Replication lag | [Replication monitoring](./01_replication.md#monitoring-replication) | Above budget |
| Client errors during failover | App metrics | Longer than your failover target |

## Operating Sentinel

- **Planned maintenance:** upgrade or restart **replicas first**, then trigger a manual failover (`SENTINEL failover mymaster`), then do the old primary
- **Adding a replica:** start it with `replicaof`. Sentinels discover it automatically
- **Replacing a Sentinel:** start a new one with the same `monitor` line. After removing the old one, run `SENTINEL reset mymaster` on the others so they forget it
- **Config files change at runtime:** keep them writable, and don't redeploy them from a read-only image without a persistent path
- **Never** run a Sentinel inside the same failure domain as the data node it protects

## Limitations

- **No sharding**: one primary holds all the data and takes all the writes
- **Loss window remains** (asynchronous replication)
- **Failover takes time**: writes fail for the detection plus election plus rediscovery period
- **Clients must be Sentinel-aware** (ioredis is)
- **Operational surface**: Sentinels, replicas, configs and quorum all need monitoring

## Managed services

Many providers (ElastiCache replication groups with Multi-AZ, Google Memorystore HA, Azure Cache, Redis Cloud) **run failover for you** and usually expose a **primary endpoint (and a reader endpoint)** rather than Sentinel. Your client just uses the endpoint and **reconnects after failover** (often with a DNS change), so the guidance from [High Availability and Failover](./04_high-availability-and-failover.md) applies: short timeouts, `retryStrategy`, `reconnectOnError`, and re-resolving the endpoint on reconnect. Check your provider's documentation for the endpoint behavior and failover time, because these vary.

## Testing a failover

Do this **before** production, in an environment shaped like production:

```bash
# Option A: planned failover
redis-cli -p 26379 SENTINEL failover mymaster

# Option B: crash the primary
docker stop redis-primary                  # or: kill -9, or: redis-cli DEBUG SLEEP 60

# Watch what happened
redis-cli -p 26379 SENTINEL get-master-addr-by-name mymaster
```

Run a **continuous writer** during the test ([chaos script](./04_high-availability-and-failover.md#measuring-your-real-rpo-and-rto)), and record how long writes failed, whether any acknowledged writes were lost, and whether ioredis reconnected on its own with no restart.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Two Sentinels | At least three, on separate hosts or zones |
| Sentinel on the same machine as the primary | Independent failure domains |
| A read-only or ephemeral `sentinel.conf` | A writable, persistent path |
| `down-after-milliseconds` left at 30 s without a decision | Choose it for your failover target and network |
| Clients hardcoded to the primary's IP | Sentinel mode in ioredis |
| No `reconnectOnError` for `READONLY` | `return 2` to reconnect and resend |
| Using replica clients for correctness-sensitive reads | Read those from the primary |
| No `min-replicas-to-write` and a partition-prone network | Consider it, accepting reduced availability |
| Docker or NAT addresses unreachable from clients | `natMap`, or run the app in the same network |
| Never testing failover | Game days, with a continuous writer |
| Treating Sentinel as a sharding or backup solution | It is neither |

## Key takeaways

- Sentinel = **monitoring, quorum-based failure detection, automatic promotion, and discovery**, for a **single-primary** topology
- Run **three or more** Sentinels in separate failure domains. Quorum decides "down", a majority authorizes failover
- ioredis supports Sentinel natively: `sentinels`, `name`, `role`, `failoverDetector`, plus `reconnectOnError` for `READONLY`
- Expect **10 to 30 seconds** of write unavailability and a **small loss window**, and test it
- When data or throughput outgrows one node, move to **Cluster**

**Next:** [Cluster](./03_cluster.md)
