# Deployment

Deploying Redis means choosing a topology, sizing it with room to spare, tuning the host, writing a reviewed config and running it under something that restarts it. Then you configure your clients so that restarts and failovers do not become outages.

```
Decide data class ─► Pick topology ─► Size ─► Tune OS ─► Config ─► Run (systemd/Docker/K8s)
                                                                          │
                              Client settings ◄─── Monitoring ◄───────────┘
```

## Choose a topology

| Topology | What you get | Cost | Use when |
|----------|--------------|------|----------|
| **Standalone** | Simplest. One process | Any failure means downtime | Dev, tests, disposable caches that can tolerate a restart |
| **Primary + replica** | Warm copy, read scaling, backup source | Failover is manual | You can accept a manual switch, or as a building block |
| **Sentinel** | Automatic failover for one dataset | 3 sentinels to run, client must be Sentinel-aware, no sharding | Data fits on one node, you need HA |
| **Cluster** | Sharding **and** failover | Multi-key limits (hash slots), more operations | Dataset or throughput beyond one node |
| **Managed service** | Provider runs HA, patching, backups | Cost, less control, some commands restricted | You want less operational work |

A good default path: **start with a managed or Sentinel setup, and move to Cluster only when one node is truly not enough.** Cluster adds constraints (`CROSSSLOT`, one database) that touch application code. See [Replication](../15_redis-architecture/01_replication.md), [Sentinel](../15_redis-architecture/02_sentinel.md) and [Cluster](../15_redis-architecture/03_cluster.md).

## Sizing

### Memory

```
RAM needed  ≈  dataset in Redis  ×  1.2 to 1.5   (encoding overhead, fragmentation)
             +  client and replication buffers
             +  headroom for fork (copy-on-write) when persistence is on
```

- Measure real memory from a representative load (`MEMORY USAGE`, `INFO memory`), not from estimates of raw JSON size
- With RDB or AOF rewrites enabled, set `maxmemory` to roughly **60 to 70 percent of host RAM**, because a fork can temporarily need much more memory under heavy writes
- A pure cache with no persistence can use more of the machine, but still leave room for the OS and buffers
- Inside containers, the **container memory limit** is the real ceiling. Set `maxmemory` clearly below it, or the kernel's OOM killer will end Redis instead of Redis evicting keys

### CPU

- Command execution is **single-threaded**, so prefer fewer, faster cores over many slow ones
- `io-threads` can offload network I/O when many clients saturate a core. It is off by default, so benchmark before enabling it
- If one core is the bottleneck, scale out with Cluster or split workloads across instances rather than expecting more cores to help

### Network

- Keep Redis and its clients in the same region and, ideally, the same zone. Every command is a network round trip
- A 1 Gbit link carries about 125 MB/s. Large values and many replicas consume it quickly (each replica receives a full copy of the write stream)

### Disk

- Use SSD-backed storage for RDB and AOF
- Plan disk space for RDB snapshots plus the AOF, with room for a rewrite and for a copy during backups (roughly **2 to 3 times the dataset** is a safe starting point)
- Do not share the data disk with noisy neighbors (logs, databases, backup jobs)

### Connections

```
connections ≈ app instances × connections per instance (usually 1 to 3)
```

Compare with `maxclients` (default 10000). Autoscaling can multiply connections quickly, so check the worst case.

## Tune the operating system

Redis prints warnings at startup when these are wrong. Fix them.

```bash
# /etc/sysctl.d/99-redis.conf
vm.overcommit_memory = 1        # allow fork() for background saves even when memory looks tight
net.core.somaxconn = 1024       # match or exceed tcp-backlog
vm.swappiness = 1               # avoid swapping Redis pages out
```

```bash
sudo sysctl --system

# Disable Transparent Huge Pages (causes latency spikes and memory bloat with fork)
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
```

Make the THP setting survive reboots with a boot parameter (`transparent_hugepage=never`) or a small systemd unit that runs at boot.

| Setting | Why |
|---------|-----|
| `vm.overcommit_memory = 1` | Without it, `BGSAVE` can fail with "Can't save in background: fork: Cannot allocate memory" |
| THP disabled | THP makes copy-on-write pages huge, causing spikes and extra memory |
| Swap off or minimal | Swapped Redis memory means multi-millisecond to second latencies |
| File descriptor limit above `maxclients` + 32 | Otherwise Redis lowers `maxclients` |
| Time sync (NTP or chrony) | Needed for sensible logs, TTL behavior across replicas and certificates |
| Dedicated host or guaranteed CPU | Noisy neighbors cause latency you cannot see from inside |

## Production `redis.conf`

A starting point to review line by line, not to paste blindly.

```bash
# --- network ---
bind 10.0.1.15 127.0.0.1
port 0
tls-port 6379
tls-cert-file /etc/redis/tls/redis.crt
tls-key-file  /etc/redis/tls/redis.key
tls-ca-cert-file /etc/redis/tls/ca.crt
tls-protocols "TLSv1.2 TLSv1.3"
tls-replication yes
protected-mode yes
tcp-backlog 1024
tcp-keepalive 300
timeout 300
maxclients 10000

# --- identity (see 17_security) ---
aclfile /etc/redis/users.acl

# --- memory ---
maxmemory 12gb
maxmemory-policy allkeys-lru          # cache; use noeviction for queues and primary data
lazyfree-lazy-eviction yes
lazyfree-lazy-expire yes
lazyfree-lazy-server-del yes
replica-lazy-flush yes
activedefrag yes

# --- persistence ---
dir /var/lib/redis
save 3600 1 300 100 60 10000
appendonly yes
appendfsync everysec
aof-use-rdb-preamble yes
auto-aof-rewrite-percentage 100
auto-aof-rewrite-min-size 256mb

# --- replication ---
replica-read-only yes
repl-backlog-size 128mb               # larger backlog allows partial resyncs after short disconnects
# min-replicas-to-write 1             # trade-off: refuse writes if no replica is in sync
# min-replicas-max-lag 10

# --- observability ---
loglevel notice
logfile ""                            # stdout (containers/systemd); or a file path
slowlog-log-slower-than 10000         # 10 ms, in microseconds
slowlog-max-len 256
latency-monitor-threshold 100         # ms
```

Choices to make deliberately:

| Setting | Question to answer |
|---------|--------------------|
| `maxmemory-policy` | Can Redis delete any key (cache) or must it refuse writes (data that matters)? |
| `appendonly` and `appendfsync` | How much loss is acceptable? `everysec` loses about a second, `always` is safest but slowest |
| `save` | Do you want periodic snapshots as well? They make restores and backups fast |
| `min-replicas-to-write` | Do you prefer rejecting writes over accepting writes that exist on only one node? |
| `activedefrag` | Needs the bundled allocator and extra CPU. Enable if fragmentation grows |

Keep config in version control, render it from templates per environment, and validate it before rollout. Redis has no dry-run flag for config files, so the simplest check is to start a throwaway instance with the file on another port and confirm it boots without errors.

Changing settings at runtime:

```bash
redis-cli CONFIG SET maxmemory 14gb
redis-cli CONFIG REWRITE         # persist runtime changes back to redis.conf
```

Runtime changes are lost on restart unless they are also in the file, so treat the file as the source of truth.

## Running under systemd

```ini
# /etc/systemd/system/redis.service
[Unit]
Description=Redis
After=network-online.target
Wants=network-online.target

[Service]
Type=notify
User=redis
Group=redis
ExecStart=/usr/bin/redis-server /etc/redis/redis.conf --supervised systemd --daemonize no
Restart=always
RestartSec=2
LimitNOFILE=65535
TimeoutStopSec=120              # leave time for a final save on shutdown
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=full
ReadWritePaths=/var/lib/redis /var/log/redis

[Install]
WantedBy=multi-user.target
```

Notes:

- `Restart=always` brings Redis back, and with AOF it reloads its data. Loading a large dataset takes time, and clients receive `LOADING` errors until it finishes
- A long `TimeoutStopSec` prevents systemd from killing Redis in the middle of a snapshot
- See [Security Checklist](../17_security/03_security-checklist.md) for further hardening options

## Running in Docker

```yaml
# docker-compose.yml
services:
  redis:
    image: redis:7.4                       # pin the major.minor (better: an exact tag or digest), never :latest
    command: ["redis-server", "/usr/local/etc/redis/redis.conf"]
    volumes:
      - redis-data:/data                   # persistence lives in a volume, not the container layer
      - ./redis.conf:/usr/local/etc/redis/redis.conf:ro
    ports:
      - "127.0.0.1:6379:6379"              # never publish on 0.0.0.0 without a firewall plan
    restart: unless-stopped
    ulimits:
      nofile: { soft: 65535, hard: 65535 }
    deploy:
      resources:
        limits:
          memory: 2g                       # set maxmemory in redis.conf well below this (for example 1200mb)
    healthcheck:
      test: ["CMD-SHELL", "redis-cli ping | grep -q PONG"]   # add auth through the REDISCLI_AUTH environment variable
      interval: 10s
      timeout: 3s
      retries: 5
volumes:
  redis-data:
```

Docker pitfalls:

- Without a volume, **all data disappears** with the container
- A memory limit lower than `maxmemory` plus overhead leads to OOM kills
- Set `dir /data` in the config so files land in the volume (the official image uses `/data`)
- Docker's host networking and published ports can bypass host firewalls. Bind to `127.0.0.1` or a private interface
- Stop with `docker stop` (SIGTERM) and allow time for the save: `stop_grace_period: 60s`

## Running on Kubernetes

Redis is stateful, so a `StatefulSet` with persistent volumes is the minimum. For HA (replicas, failover, Cluster), strongly consider a well-maintained **operator** or Helm chart instead of hand-writing everything.

```yaml
apiVersion: apps/v1
kind: StatefulSet
metadata: { name: redis }
spec:
  serviceName: redis
  replicas: 1
  selector: { matchLabels: { app: redis } }
  template:
    metadata: { labels: { app: redis } }
    spec:
      terminationGracePeriodSeconds: 90      # time for a final save
      securityContext: { runAsNonRoot: true, runAsUser: 999, fsGroup: 999 }
      containers:
        - name: redis
          image: redis:7.4
          args: ["/etc/redis/redis.conf"]
          ports: [{ containerPort: 6379, name: redis }]
          resources:
            requests: { cpu: "1", memory: 2Gi }
            limits:   { memory: 2Gi }        # equal request and limit for memory; maxmemory well below this
          readinessProbe:
            exec: { command: ["sh", "-c", "redis-cli ping | grep -q PONG"] }
            periodSeconds: 5
          livenessProbe:
            exec: { command: ["sh", "-c", "redis-cli ping | grep -q PONG"] }
            initialDelaySeconds: 30
            periodSeconds: 10
            failureThreshold: 6              # generous: do not restart a node that is merely loading data
          volumeMounts:
            - { name: data, mountPath: /data }
            - { name: config, mountPath: /etc/redis }
      volumes:
        - name: config
          configMap: { name: redis-config }
  volumeClaimTemplates:
    - metadata: { name: data }
      spec:
        accessModes: ["ReadWriteOnce"]
        resources: { requests: { storage: 20Gi } }
---
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata: { name: redis }
spec:
  maxUnavailable: 1
  selector: { matchLabels: { app: redis } }
```

Kubernetes-specific points:

- **Memory limit** must cover `maxmemory`, buffers and fork overhead. An OOM-killed Redis restarts and reloads, which looks like random latency and data loss
- A liveness probe that is too aggressive restarts Redis while it is loading a large AOF, which can loop forever. Use a generous `failureThreshold`, or a startup probe
- Spread replicas across nodes and zones with anti-affinity or topology spread constraints
- Use a `PodDisruptionBudget` so node drains do not remove every replica at once
- Use storage classes with predictable performance, and test `fsync` latency on them
- With Sentinel on Kubernetes, enable hostname support (`sentinel resolve-hostnames yes`, Redis 6.2+) and announce stable names, because pod IPs change

## Sentinel and Cluster quick setup

### Sentinel (three sentinels on separate hosts or zones)

```bash
# sentinel.conf
port 26379
sentinel monitor mymaster 10.0.1.10 6379 2          # quorum: 2 of 3
sentinel auth-user mymaster sentinel-user
sentinel auth-pass mymaster ${REDIS_SENTINEL_PASSWORD}
sentinel down-after-milliseconds mymaster 5000
sentinel failover-timeout mymaster 60000
sentinel parallel-syncs mymaster 1
```

- Run an **odd number (3 or 5)** in independent failure domains. Two sentinels on one host protect nothing
- Replicas need `masterauth` and `masteruser` set to match the ACL user used for replication
- Test with `redis-cli -p 26379 SENTINEL failover mymaster`

### Cluster

```bash
# redis.conf on each node
cluster-enabled yes
cluster-config-file nodes.conf
cluster-node-timeout 15000
cluster-require-full-coverage no       # trade-off: keep serving the slots that are still available

# create 3 primaries + 3 replicas
redis-cli --cluster create \
  n1:6379 n2:6379 n3:6379 n4:6379 n5:6379 n6:6379 --cluster-replicas 1

redis-cli --cluster check n1:6379
```

- Each node needs two open ports: the client port and the cluster bus (client port + 10000)
- Place each primary and its replica in **different zones**
- Back up `nodes.conf` along with data

## Client configuration for production

Servers fail over and restart. Your clients decide whether that is a blip or an outage.

```ts
import Redis from "ioredis";

export function createRedis(overrides: Partial<ConstructorParameters<typeof Redis>[0]> = {}) {
  const redis = new Redis({
    host: process.env.REDIS_HOST,
    port: Number(process.env.REDIS_PORT ?? 6379),
    username: process.env.REDIS_USER,
    password: process.env.REDIS_PASSWORD,
    tls: process.env.REDIS_TLS === "true" ? {} : undefined,

    connectionName: `${process.env.SERVICE_NAME ?? "app"}-${process.pid}`,
    connectTimeout: 5_000,
    commandTimeout: 2_000,                 // never hang a request forever on Redis
    keepAlive: 10_000,
    maxRetriesPerRequest: 2,               // fail commands fast during an outage
    retryStrategy: (times) =>              // backoff with jitter, avoids reconnect storms
      Math.min(times * 100, 2_000) + Math.floor(Math.random() * 200),
    reconnectOnError: (err) =>             // after a failover, a stale connection reports READONLY
      err.message.includes("READONLY") ? 2 : false,
    ...overrides,
  });

  redis.on("error", (err) => log.error({ err: err.message }, "redis error"));
  return redis;
}
```

| Setting | Why it matters in production |
|---------|-----------------------------|
| `commandTimeout` | A hung Redis should not hang every request |
| `maxRetriesPerRequest` | Bounds how long a command waits during reconnects (queue workers and blocking connections need `null`) |
| `retryStrategy` with jitter | Hundreds of pods reconnecting at once can overload a recovering server |
| `reconnectOnError` for `READONLY` | Reconnects after a failover so writes reach the new primary |
| `connectionName` | Lets you identify clients in `CLIENT LIST` |
| `enableOfflineQueue` (default true) | While disconnected, commands queue in memory. Disable if you prefer to fail fast |

For Sentinel and Cluster:

```ts
// Sentinel
new Redis({
  sentinels: [{ host: "s1", port: 26379 }, { host: "s2", port: 26379 }, { host: "s3", port: 26379 }],
  name: "mymaster",
  username: "app", password: process.env.REDIS_PASSWORD,
  sentinelPassword: process.env.SENTINEL_PASSWORD,
  role: "master",
});

// Cluster
new Redis.Cluster([{ host: "n1", port: 6379 }, { host: "n2", port: 6379 }], {
  redisOptions: { username: "app", password: process.env.REDIS_PASSWORD, tls: {} },
  scaleReads: "master",                    // "slave" trades freshness for read throughput
  slotsRefreshTimeout: 2_000,
});
```

Also build in **graceful degradation**: cache calls should fail open (fall back to the source) with a timeout, and a circuit breaker can stop calling Redis for a few seconds after repeated failures. See [Graceful Shutdown](../03_ioredis-basics/06_graceful-shutdown.md) and [Connection Management](../08_nodejs-integration/01_connection-management.md).

## Protect the database from a cold cache

A restart, failover or flush of a cache can send **all** traffic to your database at once.

- Use per-key locks or request coalescing so one request rebuilds each value (see [Cache Problems](../07_caching/04_cache-problems.md))
- Add TTL jitter so keys do not all expire together
- Consider warming hot keys before shifting traffic
- Rate-limit or queue expensive rebuilds
- Rehearse it: flush a staging cache under load and watch the database

## Upgrades and maintenance

### Planning

- Read the release notes, especially breaking changes and deprecated commands
- Test on staging with production-like data and the same client library versions
- Keep versions **identical** across replicas in steady state, and plan for short mixed-version windows only
- RDB files written by a newer version may not load on an older one, so rolling back after upgrade needs a pre-upgrade backup

### Rolling upgrade with Sentinel or replication

1. Take a backup and confirm replication is healthy (`master_link_status:up`, small lag)
2. Upgrade **replicas** one at a time, waiting for each to resync
3. Trigger a failover so an upgraded replica becomes primary (`SENTINEL failover mymaster`)
4. Upgrade the old primary (now a replica)
5. Verify clients, metrics and replication, then repeat for the next shard

### Cluster

Upgrade replicas first, `CLUSTER FAILOVER` on each upgraded replica, then upgrade the former primary. Check `redis-cli --cluster check` after every step.

### Config changes

Change one node at a time. Use `CONFIG SET` plus `CONFIG REWRITE` where possible, restart where required, and confirm the effect in metrics.

## Environments

| Environment | Purpose | Rule |
|-------------|---------|------|
| Development | Fast iteration | Disposable, can be a container |
| Test and CI | Automated tests | Throwaway, same Redis version as production |
| Staging | Dress rehearsal | Same topology, version and config as production at smaller scale |
| Production | Real traffic | Locked down, monitored, changed through pipelines |

Never share a Redis instance or credentials between environments. A test run that issues `FLUSHALL` against production is a classic disaster.

## Managed Redis: what to check

| Question | Why |
|----------|-----|
| Which Redis version and upgrade policy? | Feature availability and forced upgrades |
| What is the HA model and failover time? | Sets your outage window |
| Backup frequency, retention, point-in-time recovery, cross-region copy? | Your RPO |
| Which commands or configs are blocked (`CONFIG`, `ACL`, `DEBUG`)? | Affects tooling and tuning |
| Encryption in transit and at rest? How are certificates managed? | Compliance |
| Maximum connections and bandwidth by size? | Hidden limits |
| Eviction and memory behavior at the limit? | Avoids surprises |
| Network access (private endpoints, VPC peering)? | Security and latency |
| Maintenance windows? | Planned disruption |
| Cost at your expected scale and at 3 times that? | Budgeting |

Managed services still need your monitoring, backups strategy review and failover drills. The provider runs the servers, but **your client configuration and data design remain yours.**

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| `maxmemory` equal to container or host memory | Leave headroom for fork, buffers and the OS |
| No volume in Docker | Mount a persistent volume at `/data` |
| Using `:latest` images | Pin versions, upgrade deliberately |
| Aggressive liveness probe | Generous thresholds or a startup probe, so loading is not killed |
| THP enabled, swap on | Disable THP, minimize swap |
| Two sentinels, or all sentinels on one host | Three or more, in separate failure domains |
| All replicas in one zone | Spread across zones |
| Clients without timeouts | `commandTimeout`, `connectTimeout` |
| Reconnect storms after an outage | Backoff with jitter |
| No plan for a cold cache | Locks, jitter, warm-up, load shedding |
| Mixed versions left in place | Finish the rolling upgrade |
| Runtime `CONFIG SET` never written to the file | `CONFIG REWRITE` and version control |
| Testing against a different topology than production | Staging mirrors production |

## Key takeaways

- Decide how much data loss you can accept before choosing a topology and persistence
- Size memory with headroom for fork and buffers, and keep `maxmemory` below the real limit
- Tune the OS (overcommit, THP, swap, file descriptors) and treat startup warnings as bugs
- Keep config in version control, run under a supervisor and give shutdown enough time to save
- Configure clients with timeouts, bounded retries and jittered backoff, and make cache calls fail open
- Upgrade replicas first, fail over, then upgrade the old primary
- Rehearse the cold-cache scenario before it happens for real

**Previous:** [Production README](./README.md) | **Next:** [Monitoring and Logging](./02_monitoring-and-logging.md)
