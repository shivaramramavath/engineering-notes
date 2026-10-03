# Replication

Replication keeps **one or more copies** of a primary's dataset on **replicas**. It is the foundation of every Redis high-availability setup, and it also scales reads and enables backups that don't load the primary.

(Older documentation says *master* and *slave*. Current Redis says **primary** and **replica**. Commands such as `REPLICAOF` and some `INFO` fields (`slave0`, `connected_slaves`) keep the legacy words.)

## How it works

```
                    ┌──► replica 1   (read-only copy)
clients ─► PRIMARY ─┤
 (writes)           └──► replica 2   (read-only copy)
              async stream of write commands
```

1. A replica connects to the primary and sends `PSYNC` with the **replication ID** and **offset** it has seen
2. If it is new or too far behind, a **full resynchronization** happens: the primary produces an **RDB snapshot** (forking, as in `BGSAVE`), sends it, then streams the writes that arrived meanwhile
3. Otherwise a **partial resynchronization** sends only the missing commands from the primary's **replication backlog**
4. From then on, the primary **streams every write command** to each replica, **asynchronously**, without waiting for acknowledgement

| Term | Meaning |
|------|---------|
| **Replication ID** | Identifies a dataset history. It changes when a replica is promoted |
| **Offset** | A byte position in the replication stream. Replicas report theirs, so lag can be measured |
| **Backlog** | A ring buffer of recent writes on the primary, used for partial resyncs |
| **Full sync** | Copy the whole dataset. Expensive (fork, disk or network, replica load) |
| **Partial sync** | Catch up from the backlog. Cheap |

**Asynchronous** is the crucial word. The primary acknowledges your write **before** replicas have it.

## Setting it up

On a replica (config or runtime):

```
replicaof 10.0.0.1 6379          # or: REPLICAOF 10.0.0.1 6379  at runtime
masterauth <primary password>    # if the primary requires AUTH
masteruser replication-user      # if you use ACL users (Redis 6+)
```

```bash
redis-cli REPLICAOF 10.0.0.1 6379       # become a replica
redis-cli REPLICAOF NO ONE              # stop replicating and become a primary (promotion)
```

### A local lab with Docker Compose

```yaml
services:
  redis-primary:
    image: redis:7
    command: ["redis-server", "--appendonly", "yes", "--requirepass", "devpass", "--masterauth", "devpass"]
    ports: ["6379:6379"]

  redis-replica-1:
    image: redis:7
    command: ["redis-server", "--appendonly", "yes", "--requirepass", "devpass", "--masterauth", "devpass",
              "--replicaof", "redis-primary", "6379"]
    depends_on: [redis-primary]
    ports: ["6380:6379"]

  redis-replica-2:
    image: redis:7
    command: ["redis-server", "--appendonly", "yes", "--requirepass", "devpass", "--masterauth", "devpass",
              "--replicaof", "redis-primary", "6379"]
    depends_on: [redis-primary]
    ports: ["6381:6379"]
```

Check it:

```bash
redis-cli -a devpass INFO replication            # on the primary: role:master, connected_slaves:2
redis-cli -a devpass -p 6380 INFO replication    # on a replica: role:slave, master_link_status:up
redis-cli -a devpass SET hello world
redis-cli -a devpass -p 6380 GET hello           # "world"
redis-cli -a devpass -p 6380 SET x 1             # (error) READONLY You can't write against a read only replica.
```

## Important settings

| Setting | Default | Purpose |
|---------|---------|---------|
| `replica-read-only` | `yes` | Replicas reject writes. Keep it `yes` |
| `replica-serve-stale-data` | `yes` | Answer reads even while out of sync. `no` returns `MASTERDOWN` instead |
| `repl-backlog-size` | 1 MB | Size of the backlog. **Increase it** so brief disconnects resync partially |
| `repl-backlog-ttl` | 3600 s | How long the backlog survives with no replicas |
| `repl-diskless-sync` | `yes` (Redis 7) | Stream the RDB over the socket instead of writing a file first |
| `repl-diskless-sync-delay` | 5 s | Wait to let several replicas share one transfer |
| `min-replicas-to-write` | 0 | Refuse writes unless this many replicas are connected |
| `min-replicas-max-lag` | 10 s | A replica counts only if its lag is within this |
| `replica-priority` | 100 | Lower numbers are preferred for promotion (0 = never promote) |
| `client-output-buffer-limit replica` | `256mb 64mb 60` | Disconnects a replica that falls too far behind |
| `tls-replication` | `no` | Encrypt replication traffic |

### Sizing the backlog

A replica that disconnects for `T` seconds needs a backlog of at least `write rate (bytes/sec) × T`, or it must do a **full resync**. With 5 MB/s of writes and a 60 s network blip, you need about 300 MB, not 1 MB. Full resyncs on a big dataset are expensive and can overload the primary (fork, memory spike) and the network.

```
repl-backlog-size 512mb
```

## What replication guarantees (and doesn't)

| Statement | True? |
|-----------|-------|
| Replicas eventually contain what the primary had | Yes, if they stay connected |
| A write acknowledged to the client is on a replica | **No**, not necessarily |
| A replica read always shows the latest write | **No**, there is lag |
| Failover never loses data | **No**, the unreplicated tail is lost |

### Shrinking the loss window

```ts
// Wait until at least 1 replica has acknowledged this connection's writes, for up to 100 ms
await redis.set("order:9001", "paid");
const acked = await redis.wait(1, 100);          // number of replicas that acknowledged
if (acked < 1) { /* treat as "possibly not durable" */ }
```

`WAIT` makes a write **more durable**, but it is **not strong consistency**: the write is still applied on the primary first, and a failover can still pick a replica that lacks it in unusual cases. Redis 7.2 adds `WAITAOF` to also wait for `fsync` on the local primary and/or replicas.

Server-side:

```
min-replicas-to-write 1
min-replicas-max-lag 10
```

The primary then **refuses writes** (`NOREPLICAS`) if it has fewer than one replica within 10 seconds of lag. This prevents an **isolated primary from accepting writes** that no one else will ever see, trading availability for a smaller loss window. Use it deliberately.

## Reading from replicas

| Benefit | Cost |
|---------|------|
| Spreads read load across machines | Reads can be **stale** (replication lag) |
| Backups, analytics and `KEYS`-style jobs stop loading the primary | **Read-your-writes** breaks: a user writes, then reads an old value from a replica |
| Lets you size replicas for reads | Expiry: replicas don't expire keys themselves, they hide logically expired keys and wait for the primary's `DEL` |

### ioredis with plain replication

A standalone `Redis` client talks to **one** server. For read/write splitting, use **two clients**:

```ts
const primary = new Redis({ host: "redis-primary", password });
const replica = new Redis({ host: "redis-replica", password, readOnly: true });
```

`readOnly: true` is only needed for Cluster replicas (it issues the `READONLY` command). For ordinary replication, a normal connection to the replica can already run reads.

### A read-your-writes router

```ts
export class ReadWriteRedis {
  private lastWrite = new Map<string, number>();                    // user → time of their last write

  constructor(private primary: Redis, private replica: Redis, private stickyMs = 2_000) {}

  /** Call after a user's write. */
  wrote(userId: string) {
    this.lastWrite.set(userId, Date.now());
    if (this.lastWrite.size > 50_000) this.lastWrite.clear();       // crude bound
  }

  /** Reads right after a write go to the primary; others may use a replica. */
  readerFor(userId?: string): Redis {
    const t = userId ? this.lastWrite.get(userId) : undefined;
    return t && Date.now() - t < this.stickyMs ? this.primary : this.replica;
  }
}

await rw.primary.set(`profile:${id}`, json);
rw.wrote(id);
const v = await rw.readerFor(id).get(`profile:${id}`);              // from the primary for 2 s, then the replica
```

The in-memory map is per process, which is enough when a user's requests are sticky or the window is short. For a stricter guarantee, **read from the primary** for anything where staleness is harmful (permissions, balances, locks, idempotency, rate limits).

| Read from a replica | Read from the primary |
|---------------------|-----------------------|
| Cached pages, product data, feeds | Sessions and authorization checks |
| Leaderboards, counters shown to users | Locks, idempotency keys, rate limits |
| Analytics, exports, `SCAN` audits | Anything immediately after a write that must be seen |

With Sentinel and Cluster, ioredis can do replica reads for you (`role: "slave"`, `scaleReads`), see the next lessons.

## Monitoring replication

```ts
export function parseInfo(text: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const line of text.split("\r\n")) {
    if (!line || line.startsWith("#")) continue;
    const i = line.indexOf(":");
    if (i > 0) out[line.slice(0, i)] = line.slice(i + 1);
  }
  return out;
}

export async function replicationStatus(primary: Redis) {
  const info = parseInfo(await primary.info("replication"));
  const primaryOffset = Number(info.master_repl_offset);

  const replicas = Object.entries(info)
    .filter(([k]) => /^slave\d+$/.test(k))                                   // slave0:ip=...,port=...,state=online,offset=...,lag=0
    .map(([, v]) => Object.fromEntries(v.split(",").map((p) => p.split("=") as [string, string])));

  return {
    role: info.role,
    replicas: replicas.map((r) => ({
      ip: r.ip, port: r.port, state: r.state,
      lagBytes: primaryOffset - Number(r.offset),
      lagSec: Number(r.lag),
    })),
  };
}
```

On a replica, watch `master_link_status` (`up` or `down`), `master_last_io_seconds_ago`, and `master_sync_in_progress`.

| Signal | Alert when |
|--------|------------|
| `connected_slaves` | Fewer than expected |
| Replica `state` | Not `online` |
| `lagSec` / `lagBytes` | Growing or above your read-staleness budget |
| `master_link_status` (replica) | `down` for more than a few seconds |
| Full syncs (`sync_full` in `INFO stats`) | Increasing, which signals backlog or network problems |

## Hazards worth knowing

### 1. A primary that restarts empty can wipe its replicas

If the primary **restarts with no data** (no persistence, wiped disk, a container without a volume) and is automatically rejoined as the primary, **its replicas sync from it and delete their data too**. The classic outage.

Mitigations: **persistence on the primary** (and a volume), don't auto-restart a primary into the same role without checks, and run **Sentinel** so a replica is promoted instead ([Sentinel](./02_sentinel.md)).

### 2. Full resync storms

A small backlog plus a flaky network or restarts cause repeated full syncs, each forking the primary and moving the whole dataset. Raise `repl-backlog-size`, keep memory headroom for fork copy-on-write ([Persistence](../02_redis-fundamentals/04_persistence.md)), and stagger replica restarts.

### 3. Slow replicas

If a replica can't keep up, the primary's output buffer for it grows until `client-output-buffer-limit replica` disconnects it, triggering a resync. Give replicas the **same hardware** as the primary, and watch lag.

### 4. Expiry and stale reads

A key logically expired on the primary may still be visible on a lagging replica until the primary's `DEL` arrives. Don't use replica reads for TTL-sensitive correctness.

### 5. Persistence interplay

Replication is **not a backup**. A bad `FLUSHALL` or buggy delete **replicates immediately**. Keep RDB/AOF snapshots and off-host backups too ([Backup and Disaster Recovery](../19_redis-production/03_backup-and-disaster-recovery.md)).

## Chained replication

A replica can have its own replicas, which reduces load on the primary when you have many readers. Each hop adds lag and a failure link. Use sparingly.

## Security

- `requirepass` or ACLs on the primary, with `masterauth`/`masteruser` on replicas
- `tls-replication yes` when traffic crosses untrusted networks
- Bind replicas to private interfaces. A reachable replica is a reachable copy of all your data ([17_security](../17_security/README.md))

## Failure behavior and ioredis

| Event | What your app sees |
|-------|--------------------|
| A replica dies | Reads routed to it fail. The app should fall back to the primary |
| The primary dies (no Sentinel) | Writes fail until an operator promotes a replica (`REPLICAOF NO ONE`) and clients are repointed |
| A replica is promoted while clients still point at the old primary | Writes to the old primary may succeed on a **dead-end** node, or fail with `READONLY` if it came back as a replica |

For the `READONLY` case, make ioredis reconnect and resend ([Configuration](../03_ioredis-basics/02_configuration.md#reconnectonerror)):

```ts
new Redis({ reconnectOnError: (err) => (err.message.includes("READONLY") ? 2 : false) });
```

Automatic promotion and client discovery are what Sentinel adds.

## Testing

- Stop a replica and confirm the app falls back to the primary, with no errors for users
- Write continuously to the primary while restarting a replica, and check that it **resyncs partially** (`INFO stats`: `sync_partial_ok` rises, `sync_full` doesn't)
- Measure lag under load (a write loop plus `replicationStatus`) and compare it to your staleness budget
- Verify that writing to a replica fails with `READONLY` (and that your code handles it)

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Assuming acknowledged writes are on replicas | `WAIT`, `min-replicas-to-write`, or accept the loss window |
| Reading permissions, locks or balances from replicas | Read those from the primary |
| Primary restarting empty and replicas syncing from it | Persistence plus a volume, and Sentinel |
| Tiny `repl-backlog-size` | Size it for `write rate × outage window` |
| Replicas on weaker hardware than the primary | Match them |
| Treating replication as a backup | Separate snapshots and off-host copies |
| Replica clients with no fallback | Fall back to the primary on error |
| No lag monitoring | Alert on `lagSec`, `lagBytes` and link status |
| Exposing replicas publicly | Private network, auth, TLS |

## Key takeaways

- Replication is **asynchronous**: acknowledged writes can be lost when the primary fails
- It scales **reads** and offloads backups, at the cost of **stale reads**
- `WAIT` and `min-replicas-to-write` narrow, but never close, the loss window
- Size the backlog, monitor lag, protect against an **empty primary** wiping replicas
- Automatic failover needs **Sentinel** (next) or Cluster

**Next:** [Sentinel](./02_sentinel.md)
