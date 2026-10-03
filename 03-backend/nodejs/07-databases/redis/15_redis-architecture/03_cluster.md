# Cluster

Replication and Sentinel give you copies of **one** dataset on **one** primary. When the data no longer fits in one machine's memory, or one primary can't absorb the write rate, you need **sharding**: splitting the keyspace across many primaries. **Redis Cluster** does that natively, with built-in failover per shard.

## The model

```
                     16384 hash slots
   slots 0–5460          slots 5461–10922        slots 10923–16383
  ┌────────────┐         ┌────────────┐          ┌────────────┐
  │ PRIMARY  A │         │ PRIMARY  B │          │ PRIMARY  C │
  └─────┬──────┘         └─────┬──────┘          └─────┬──────┘
        │ async                │ async                 │ async
  ┌─────▼──────┐         ┌─────▼──────┐          ┌─────▼──────┐
  │ replica  A'│         │ replica  B'│          │ replica  C'│
  └────────────┘         └────────────┘          └────────────┘
         ◄──────────── gossip over the cluster bus ───────────►
```

- Every key maps to a slot: **`slot = CRC16(key) mod 16384`**
- Each **primary owns a range of slots**, and its replicas copy it
- Nodes gossip over a **cluster bus** (client port + 10000) to detect failures and agree on who owns what
- There is **no proxy**. Clients learn the slot map and talk **directly** to the right node
- Failover is built in: a primary's replica is promoted if a **majority of primaries** agree the primary is down

Minimum sensible setup: **3 primaries + 3 replicas** (6 nodes).

## Hash slots and hash tags

```bash
redis-cli CLUSTER KEYSLOT user:1042          # 4955
redis-cli CLUSTER KEYSLOT "{user:1042}:cart" # the same slot as every key containing {user:1042}
```

If a key contains `{...}`, **only the text between the first braces is hashed**. That lets related keys share a slot:

```
{user:1042}:profile      ┐
{user:1042}:cart         ├─ one slot, one node
{user:1042}:orders       ┘
```

This matters because **multi-key operations require all keys in one slot** (below). Design tags carefully ([Key Design](../05_key-management/01_key-design.md#cluster-safe-keys-and-hash-tags), [Key Builder](../08_nodejs-integration/05_redis-key-builder.md)).

## A local lab

Static IPs make cluster creation reliable (a hostname-only setup needs extra announce settings):

```yaml
# docker-compose.yml
x-node: &node
  image: redis:7
  command: ["redis-server", "--port", "6379", "--cluster-enabled", "yes",
            "--cluster-config-file", "nodes.conf", "--cluster-node-timeout", "5000",
            "--appendonly", "yes"]

services:
  node1: { <<: *node, networks: { redisnet: { ipv4_address: 172.28.0.11 } } }
  node2: { <<: *node, networks: { redisnet: { ipv4_address: 172.28.0.12 } } }
  node3: { <<: *node, networks: { redisnet: { ipv4_address: 172.28.0.13 } } }
  node4: { <<: *node, networks: { redisnet: { ipv4_address: 172.28.0.14 } } }
  node5: { <<: *node, networks: { redisnet: { ipv4_address: 172.28.0.15 } } }
  node6: { <<: *node, networks: { redisnet: { ipv4_address: 172.28.0.16 } } }

networks:
  redisnet:
    name: redis-cluster-net
    ipam: { config: [{ subnet: 172.28.0.0/16 }] }
```

```bash
docker compose up -d

# form the cluster: 3 primaries, each with 1 replica
docker run --rm --network redis-cluster-net redis:7 redis-cli --cluster create \
  172.28.0.11:6379 172.28.0.12:6379 172.28.0.13:6379 \
  172.28.0.14:6379 172.28.0.15:6379 172.28.0.16:6379 \
  --cluster-replicas 1 --cluster-yes

docker run --rm --network redis-cluster-net redis:7 redis-cli -h 172.28.0.11 cluster info
```

Run your **application on the same Docker network**. Nodes advertise internal IPs, which a client on your laptop can't reach. From the host, use ioredis's `natMap` (below).

## Inspecting a cluster

```bash
redis-cli -c -h 172.28.0.11 CLUSTER INFO          # cluster_state:ok, cluster_slots_assigned:16384, cluster_known_nodes ...
redis-cli -h 172.28.0.11 CLUSTER NODES            # every node: id, address, flags (master/slave), slots, link state
redis-cli -h 172.28.0.11 CLUSTER SHARDS           # structured view (Redis 7.0+)
redis-cli --cluster check 172.28.0.11:6379        # consistency and coverage check
redis-cli -c -h 172.28.0.11                       # -c: follow redirects in the CLI
```

Key health fields: `cluster_state` (`ok` or `fail`), `cluster_slots_ok`, `cluster_slots_fail`, and `cluster_known_nodes`.

## Redirects: MOVED and ASK

Any node can receive any command. If it doesn't own the key's slot, it replies with a redirect instead of forwarding:

| Reply | Meaning | Client action |
|-------|---------|---------------|
| `-MOVED 3999 172.28.0.12:6379` | The slot **permanently** belongs to another node | Update the slot map, retry there |
| `-ASK 3999 172.28.0.13:6379` | The slot is **mid-migration**, and this key already moved | Send `ASKING` plus the command to that node **once**, without updating the map |
| `-CLUSTERDOWN` | The cluster can't serve this slot | Wait, retry, or alert |
| `-TRYAGAIN` | A multi-key operation hit a slot mid-migration | Retry shortly |
| `-CROSSSLOT` | Keys in the command are in different slots | **Fix the application**, since this is never transient |

A **smart client** caches the slot-to-node map and goes straight to the right node, refreshing on `MOVED`. ioredis does all this for you.

## Connecting with ioredis

```ts
import { Cluster } from "ioredis";

const cluster = new Cluster(
  [
    { host: "172.28.0.11", port: 6379 },
    { host: "172.28.0.12", port: 6379 },
    { host: "172.28.0.13", port: 6379 },       // a few seed nodes. The client discovers the rest
  ],
  {
    redisOptions: {
      password: process.env.REDIS_PASSWORD,
      tls: process.env.REDIS_TLS === "true" ? {} : undefined,
      commandTimeout: 2_000,
      connectTimeout: 5_000,
    },
    scaleReads: "master",                      // "slave" or "all" to read from replicas (stale reads possible)
    maxRedirections: 16,
    clusterRetryStrategy: (times) => Math.min(100 * 2 ** times, 3_000),
    retryDelayOnFailover: 200,
    retryDelayOnClusterDown: 200,
    retryDelayOnTryAgain: 100,
    enableAutoPipelining: true,                // batches per node, see below
  }
);

cluster.on("error", (err) => logger.warn({ err }, "cluster error"));
cluster.on("node error", (err, address) => logger.warn({ err, address }, "cluster node error"));
cluster.on("+node", (node) => logger.info({ node: node.options.host }, "node added"));
cluster.on("-node", (node) => logger.warn({ node: node.options.host }, "node removed"));
```

The option set is rich and version-dependent, so check the ioredis documentation for defaults and exact names.

| Option | Purpose |
|--------|---------|
| `scaleReads` | Where reads go: `"master"` (default), `"slave"` (replicas only), `"all"`, or a function. **Replica reads can be stale** |
| `maxRedirections` | Redirect hops before giving up |
| `retryDelayOnFailover` / `OnClusterDown` / `OnTryAgain` | Delays before retrying those conditions |
| `clusterRetryStrategy` | Backoff for reconnecting to the cluster as a whole |
| `slotsRefreshTimeout` / `slotsRefreshInterval` | How the slot map is refreshed |
| `redisOptions` | Options applied to **each node connection** (password, TLS, timeouts) |
| `natMap` | Translates addresses the cluster advertises into ones you can reach |
| `dnsLookup` | Custom DNS handling (needed for TLS to some managed services, below) |
| `enableAutoPipelining` | Per-node automatic batching |

### NAT, containers and managed TLS clusters

```ts
// connecting from your laptop to the Docker lab
new Cluster([{ host: "127.0.0.1", port: 7000 }], {
  natMap: {
    "172.28.0.11:6379": { host: "127.0.0.1", port: 7000 },
    "172.28.0.12:6379": { host: "127.0.0.1", port: 7001 },
    // ...one entry per node, with ports published accordingly
  },
});

// TLS clusters on some managed services (for example ElastiCache): skip ioredis's own DNS pre-resolution
new Cluster([{ host: "my-cluster.xxxx.cache.amazonaws.com", port: 6379 }], {
  dnsLookup: (address, callback) => callback(null, address),
  redisOptions: { tls: {}, password: process.env.REDIS_PASSWORD },
});
```

### Seeing the topology

```ts
for (const node of cluster.nodes("master")) {
  console.log(node.options.host, node.options.port, await node.dbsize());
}

await cluster.cluster("KEYSLOT", "{user:1}:cart");    // which slot?
```

## The rules of life in a Cluster

### 1. Multi-key commands need one slot

```ts
await cluster.mget("user:1", "user:2");                    // ✗ CROSSSLOT (different slots)
await cluster.mget("{u}:1", "{u}:2");                      // ✓ same tag, same slot

await Promise.all(["user:1", "user:2"].map((k) => cluster.get(k)));    // ✓ individual commands, routed per key
```

This applies to `MGET`/`MSET`, `DEL`/`UNLINK` with several keys, set operations (`SINTER`, `SUNION`), `RENAME`, `SMOVE`, `LMOVE`, `ZUNIONSTORE`, `BITOP`, `PFMERGE`, `SORT ... STORE`, and more.

### 2. Transactions, Lua and pipelines

- `MULTI`/`EXEC`, `WATCH`, and Lua scripts need **every key in the same slot** (declare them in `KEYS`, tagged)
- An ioredis **pipeline** is sent over one connection, so its keys should share a slot. For many unrelated keys, use **auto pipelining**, which groups commands **per node** automatically ([Pipelines](../06_advanced-commands/01_pipelines-and-auto-pipelining.md#pipelines-and-cluster))

```ts
await cluster.multi()
  .hset("{order:9}:info", { status: "paid" })
  .sadd("{order:9}:items", "sku1")
  .exec();                                                  // ✓ one slot
```

### 3. Only database 0

`SELECT` isn't supported. Use key prefixes, not logical databases.

### 4. `KEYS`, `SCAN`, `FLUSHALL`, `DBSIZE` are per node

Run them on **every primary**:

```ts
async function scanCluster(pattern: string, onKeys: (keys: string[]) => Promise<void>) {
  await Promise.all(cluster.nodes("master").map(async (node) => {
    for await (const keys of node.scanStream({ match: pattern, count: 500 })) await onKeys(keys);
  }));
}

const totalKeys = (await Promise.all(cluster.nodes("master").map((n) => n.dbsize()))).reduce((a, b) => a + b, 0);
```

### 5. Pub/Sub

Classic `PUBLISH` is **broadcast to every node**, which costs more as the cluster grows. **Sharded Pub/Sub** (`SPUBLISH`/`SSUBSCRIBE`, Redis 7.0+) keeps a channel on its slot's shard ([Pub/Sub Fundamentals](../09_pub-sub/01_pub-sub-fundamentals.md#pubsub-in-cluster)).

### 6. Big keys and hot keys don't shard

A single key lives on **one node**. A 10 GB sorted set or a viral counter overloads one shard no matter how many nodes you add. Split big structures across keys ([bucketing](../05_key-management/01_key-design.md#cardinality-and-bucketing)), and spread hot keys (`counter:0..N` summed on read).

## Designing keys for a Cluster

| Goal | Technique |
|------|-----------|
| Operate on an entity's keys together (transactions, scripts) | One hash tag per entity: `{user:1042}:…` |
| Spread load evenly | Many distinct tags (entities), not one global tag |
| Cross-entity aggregation | Do it in the application, with several commands and no transaction |
| Queue per tenant | `{q:tenantA}:wait`, `{q:tenantA}:active`, … ([queue keys](../14_queues-and-workers/01_queue-fundamentals.md#data-layout)) |
| Avoid | `{app}` or `{global}` tags, since everything lands on one node |

**Test with real topology.** Code that works on a single node fails with `CROSSSLOT` the first day on a Cluster, so run your test suite against one too (Testcontainers or the lab above).

## Failure and availability

| Event | Result |
|-------|--------|
| A **replica** fails | Nothing visible, unless it was serving reads |
| A **primary** fails | After `cluster-node-timeout`, its replica is promoted by a **majority of the other primaries**. Writes to those slots fail until then |
| A primary and **all its replicas** fail | Those slots are **uncovered**. With `cluster-require-full-coverage yes` (default) the **whole cluster stops serving**. With `no`, only those slots fail |
| **Minority partition** | Nodes on the minority side stop accepting writes after `node-timeout`, because they can't prove a majority |
| Partition heals | The old primary becomes a replica, and writes it accepted on the minority side are lost |

Key settings:

| Setting | Default | Effect |
|---------|---------|--------|
| `cluster-node-timeout` | 15000 ms | Detection time. Lower is faster failover and more false positives |
| `cluster-require-full-coverage` | `yes` | `no` keeps serving the slots that still have owners |
| `cluster-replica-validity-factor` | 10 | A replica too far behind won't promote itself |
| `cluster-migration-barrier` | 1 | Replicas may migrate to orphaned primaries only above this count |
| `cluster-allow-reads-when-down` | `no` | Serve reads even when the cluster is down |

As elsewhere, replication is **asynchronous**: writes acknowledged by a failed primary but not yet replicated are **lost** ([Replication](./01_replication.md#what-replication-guarantees-and-doesnt)).

## Operating a Cluster

### Scaling out (adding capacity)

```bash
redis-cli --cluster add-node NEW_IP:6379 EXISTING_IP:6379                    # add a node (as a primary)
redis-cli --cluster rebalance EXISTING_IP:6379 --cluster-use-empty-masters   # move slots onto it
redis-cli --cluster reshard EXISTING_IP:6379                                  # or move specific slots interactively
redis-cli --cluster add-node NEW_REPLICA:6379 EXISTING_IP:6379 --cluster-slave --cluster-master-id <id>
```

During a slot migration the keys move **one by one** (`MIGRATE`), clients may see `ASK` redirects, and a **very large key** blocks while it migrates. Resharding is **online** and ioredis follows redirects, but do it **off-peak**, watch latency, and rehearse first.

### Scaling in and replacing nodes

```bash
redis-cli --cluster reshard ...                      # drain a node's slots first
redis-cli --cluster del-node EXISTING_IP:6379 <node-id>
redis-cli --cluster check EXISTING_IP:6379            # always verify afterward
redis-cli --cluster fix EXISTING_IP:6379              # repair open slots (use with care)
```

### Manual failover (planned maintenance)

Run on a **replica** to take over its primary gracefully, with no data loss:

```bash
redis-cli -h REPLICA CLUSTER FAILOVER           # coordinated: waits for the replica to catch up
redis-cli -h REPLICA CLUSTER FAILOVER FORCE     # primary may be unreachable, skips the handshake
redis-cli -h REPLICA CLUSTER FAILOVER TAKEOVER  # without cluster consensus, for emergencies only
```

### Sizing

| Topic | Guidance |
|-------|----------|
| Primaries | At least 3. Most clusters run 3 to 12. The practical ceiling is on the order of a thousand nodes |
| Replicas | At least 1 per primary for HA |
| Memory per node | Leave headroom for fork copy-on-write and growth ([Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md)) |
| Slots | Fixed at 16384, so aim for roughly even load per node |
| Network | Each node needs reachability to all others on the client port and the bus port (client port + 10000) |

## Monitoring

| Signal | Alert when |
|--------|------------|
| `cluster_state` | Not `ok` |
| `cluster_slots_fail` / `cluster_slots_pfail` | Above 0 |
| Known nodes and node flags (`fail`, `fail?`) | Unexpected |
| Per-node memory, ops/sec, latency | **Skew** between nodes (hot shard) |
| Slot distribution | Uneven after changes |
| Replication lag per shard | Above budget |
| Client `MOVED`/`ASK` rate | Sustained high (a stale map or active migration) |
| Client `CROSSSLOT` errors | **Any**, since that's a bug in the application |

## Sharding approaches compared

| Approach | How | Pros | Cons |
|----------|-----|------|------|
| **Redis Cluster** | Native slots, smart clients, built-in failover | No extra components, online resharding | Multi-key limits, operational complexity |
| **Client-side sharding** | The app picks a node by hashing the key | Simple, no cluster protocol | No failover, resharding **moves keys manually**, every client must agree |
| **Proxy** (Twemproxy, Envoy, vendor proxies) | A proxy routes commands | Thin clients, hides topology | Another hop and component, limited command support |
| **Managed cluster mode** | The provider runs Cluster | Least effort | Provider limits and cost |

A minimal client-side sharder using **rendezvous (highest-random-weight) hashing**, which moves only about `1/N` of the keys when a shard is added or removed:

```ts
import { createHash } from "node:crypto";

const score = (key: string, shard: string) =>
  createHash("sha1").update(`${shard}:${key}`).digest().readUInt32BE(0);

export function pickShard<T extends { id: string }>(key: string, shards: T[]): T {
  let best = shards[0]!, bestScore = -1;
  for (const s of shards) {
    const sc = score(key, s.id);
    if (sc > bestScore) { best = s; bestScore = sc; }
  }
  return best;
}

const shards = [{ id: "a", client: redisA }, { id: "b", client: redisB }, { id: "c", client: redisC }];
await pickShard("user:1042", shards).client.set("user:1042", "...");
```

Use it only for **cache-like, rebuildable data**, where losing a shard or moving keys just causes misses. For real data, use Cluster or a managed service.

## Cluster vs Sentinel

| | Sentinel (replication) | Cluster |
|---|------------------------|---------|
| Data size limit | One node's memory | Sum of all primaries |
| Write scaling | One primary | **Many primaries** |
| Failover | Sentinel processes | Built into the nodes |
| Multi-key commands, Lua, transactions | **Anywhere** | **Same slot only** |
| `SELECT` / multiple databases | Yes | No (DB 0) |
| Client complexity | Low | Higher (redirects, slots) |
| Operational complexity | Medium | **High** |
| Use when | Data fits on a node | Data or throughput exceeds one node |

**Don't adopt Cluster early.** If one node, with replicas, is enough, Sentinel or a managed Multi-AZ service is far simpler. Also consider vertical scaling and better data modeling (smaller values, TTLs, compression) before sharding.

## Testing

- Run your **whole test suite on a real cluster** to catch `CROSSSLOT` and per-node commands (`SCAN`, `KEYS`, `DBSIZE`)
- Unit-test **key builders**: keys that must be co-located share a tag (`CLUSTER KEYSLOT` equality)
- **Kill a primary** (`docker stop node1`) during a write loop, and measure failover time and lost writes ([chaos script](./04_high-availability-and-failover.md#measuring-your-real-rpo-and-rto))
- **Reshard** under load and watch latency and `ASK`/`MOVED` rates
- Verify ioredis recovers **without a process restart**

```ts
it("keeps an entity's keys in one slot", async () => {
  const slots = await Promise.all(
    [keys.userGroup.profile(1), keys.userGroup.cart(1), keys.userGroup.orders(1)].map((k) => cluster.cluster("KEYSLOT", k))
  );
  expect(new Set(slots).size).toBe(1);
});
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `CROSSSLOT` errors in production | Hash tags, per-key commands, auto pipelining. Test on a cluster |
| One global hash tag | Tags per entity, to spread load |
| `SCAN`/`KEYS`/`DBSIZE` against one node | Iterate every primary |
| Huge or hot single keys | Split and bucket |
| Using `SELECT` or several databases | Prefixes, DB 0 only |
| Reading replicas for correctness-critical data | `scaleReads: "master"` for those |
| Nodes advertising unreachable addresses | `natMap`, or run clients in the network |
| `cluster-require-full-coverage yes` taking everything down when one shard is lost | Choose consciously (`no` serves partial availability) |
| Resharding at peak with huge keys | Off-peak, rehearsed, and split big keys first |
| Adopting Cluster before you need it | Sentinel or managed HA first |
| Only 2 primaries | At least 3 so a majority exists |
| Blocked cluster bus port (client port + 10000) | Open it between nodes |

## Key takeaways

- Cluster **shards the keyspace into 16384 slots** across primaries, each with replicas, and has **built-in failover**
- Clients are **smart**: they cache the slot map and follow `MOVED`/`ASK`. ioredis's `Cluster` does this
- **Multi-key commands, transactions and Lua need keys in one slot**, so design **hash tags per entity**
- `SCAN`, `KEYS` and friends are **per node**, and big or hot keys don't shard
- Replication is still **asynchronous**, so some loss is possible on failover
- Adopt Cluster **when you must**, since Sentinel or managed HA is much simpler

**Next:** [High Availability and Failover](./04_high-availability-and-failover.md)
