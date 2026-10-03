# Monitoring and Logging

You cannot operate what you cannot see. Good Redis monitoring answers four questions at any moment: **Is it up? Is it fast? Is it healthy? Is it about to run out of something?** Logging answers the fifth: **what just happened?**

```
Redis server ──INFO──► exporter ──► Prometheus ──► Grafana (dashboards)
     │                                   │
     └── logs ──► log pipeline           └──► Alertmanager ──► on-call
Your app ── client metrics, health checks, structured logs ──┘
```

## What to watch: the four signals

| Signal | Questions | Key metrics |
|--------|-----------|-------------|
| **Latency** | Are commands fast? | Client-side p50/p95/p99, `SLOWLOG`, `LATENCY`, fork time |
| **Traffic** | How much work? | Ops/sec, network in/out, connected clients, command mix |
| **Errors** | What is failing? | Rejected connections, `OOM` and `MISCONF` errors, ACL denials, client error rates |
| **Saturation** | How close to the limit? | Memory vs `maxmemory`, evictions, CPU, clients vs `maxclients`, output buffers, disk |

Plus Redis-specific health: **persistence status**, **replication state**, **hit ratio** and **fragmentation**.

## Metrics that matter

Source: `INFO` sections, scraped by an exporter or read manually.

| Area | `INFO` field | Why | Starting alert idea |
|------|--------------|-----|---------------------|
| Availability | process up, `uptime_in_seconds` | Restarts reset memory and caches | Down, or uptime dropped (unexpected restart) |
| Memory | `used_memory`, `maxmemory` | Capacity | Above 80 to 85 percent of `maxmemory` |
| Memory | `mem_fragmentation_ratio` | Wasted RSS, or swapping if below 1 | Above 1.5 for a while, or below 1 |
| Evictions | `evicted_keys` | Cache is memory-bound, or data loss for non-cache data | Any eviction on `noeviction`-style or queue instances, a sustained rate on caches |
| Expiry | `expired_keys` | Expected churn. Sudden spikes mean mass expiry | Trend only |
| Hit ratio | `keyspace_hits`, `keyspace_misses` | Cache effectiveness | Sustained drop from your baseline |
| Clients | `connected_clients`, `blocked_clients` | Leaks, stuck consumers | Approaching `maxclients`, rapid growth |
| Clients | `rejected_connections` | `maxclients` reached | Any increase |
| Throughput | `instantaneous_ops_per_sec` | Load and anomalies | Large deviation from baseline |
| Persistence | `rdb_last_bgsave_status`, `aof_last_write_status` | Failed saves can block writes | Anything not `ok` |
| Persistence | `rdb_last_save_time`, `rdb_changes_since_last_save` | Backup freshness, data at risk | No save for longer than expected |
| Persistence | `latest_fork_usec` | Fork cost causes latency spikes | Rising trend |
| Replication | `connected_slaves`, `master_link_status`, `master_repl_offset` | Replica health and lag | Fewer replicas than expected, link `down`, lag growing |
| Replication | `repl_backlog_size` vs write rate | Partial resync possible? | Frequent full resyncs |
| Cluster | `cluster_state`, `cluster_slots_fail` | Slot coverage | `cluster_state` is not `ok` |
| CPU | `used_cpu_sys`, `used_cpu_user` (rates), host CPU | One saturated core means a bottleneck | Sustained high core usage |
| Network | `total_net_input_bytes`, `total_net_output_bytes` | Bandwidth | Near link limits |
| Commands | `INFO commandstats` | Which commands cost time | New expensive commands (`KEYS`) |

Treat the alert ideas as starting points. Tune them from your own baselines so alerts fire on **changes that matter**, not on normal variation.

## Quick manual checks

```bash
redis-cli INFO memory       | egrep 'used_memory_human|maxmemory_human|mem_fragmentation_ratio'
redis-cli INFO stats        | egrep 'instantaneous_ops|evicted_keys|rejected_connections|keyspace_(hits|misses)'
redis-cli INFO clients
redis-cli INFO persistence  | egrep 'rdb_last_bgsave_status|aof_last_write_status|loading|latest_fork_usec'
redis-cli INFO replication
redis-cli SLOWLOG GET 10
redis-cli LATENCY DOCTOR
redis-cli --stat
```

## Prometheus and an exporter

The widely used `redis_exporter` reads `INFO` (and more) and exposes Prometheus metrics, by default on port 9121.

```yaml
# docker-compose.yml
services:
  redis-exporter:
    image: oliver006/redis_exporter:latest      # pin a specific version in production
    environment:
      REDIS_ADDR: "rediss://redis.internal:6379"   # use rediss:// for TLS
      REDIS_USER: "monitoring"
      REDIS_PASSWORD: "${REDIS_MONITORING_PASSWORD}"
    ports:
      - "127.0.0.1:9121:9121"
```

```yaml
# prometheus.yml
scrape_configs:
  - job_name: redis
    scrape_interval: 15s
    static_configs:
      - targets: ["redis-exporter:9121"]
        labels: { service: "orders", role: "cache" }
```

For Cluster, run the exporter in cluster mode or point one exporter per node. For Sentinel, scrape each sentinel as well.

Create a **dedicated, read-only ACL user** for the exporter:

```
user monitoring on >... ~* &* -@all +info +ping +config|get +client|list +slowlog|get +slowlog|len +latency|latest +memory|usage +dbsize +scan +type +ttl +pttl +xinfo|stream +llen +hlen +scard +zcard +strlen +xlen
```

Key rules need Redis 7.0+ syntax for `command|subcommand`. Exact needs depend on the exporter flags you enable, so check `ACL LOG` for denials after deployment.

### Useful metric names

Names can differ between exporter versions, so confirm them on your `/metrics` endpoint.

| Metric | Meaning |
|--------|---------|
| `redis_up` | Exporter can reach Redis (1 or 0) |
| `redis_memory_used_bytes`, `redis_memory_max_bytes` | Memory and limit |
| `redis_mem_fragmentation_ratio` | Fragmentation |
| `redis_connected_clients`, `redis_blocked_clients` | Clients |
| `redis_rejected_connections_total` | Connections refused at `maxclients` |
| `redis_commands_processed_total` | Throughput (use `rate()`) |
| `redis_keyspace_hits_total`, `redis_keyspace_misses_total` | Hit ratio inputs |
| `redis_evicted_keys_total`, `redis_expired_keys_total` | Evictions and expiries |
| `redis_rdb_last_bgsave_status`, `redis_aof_last_write_status` | Persistence health (1 is ok) |
| `redis_connected_slaves`, `redis_master_link_up` | Replication |
| `redis_commands_duration_seconds_total` by command | Time spent per command |

## Alert rules

```yaml
# redis-alerts.yml
groups:
  - name: redis
    rules:
      - alert: RedisDown
        expr: redis_up == 0
        for: 1m
        labels: { severity: page }
        annotations: { summary: "Redis {{ $labels.instance }} is down", runbook: "https://wiki/runbooks/redis-down" }

      - alert: RedisMemoryHigh
        expr: redis_memory_max_bytes > 0 and redis_memory_used_bytes / redis_memory_max_bytes > 0.85
        for: 10m
        labels: { severity: ticket }
        annotations: { summary: "Redis memory above 85% of maxmemory" }

      - alert: RedisEvicting
        expr: rate(redis_evicted_keys_total[5m]) > 0
        for: 10m
        labels: { severity: ticket }
        annotations: { summary: "Redis is evicting keys (memory pressure)" }

      - alert: RedisRejectingConnections
        expr: increase(redis_rejected_connections_total[5m]) > 0
        labels: { severity: page }
        annotations: { summary: "Redis rejected connections (maxclients reached)" }

      - alert: RedisPersistenceFailing
        expr: redis_rdb_last_bgsave_status == 0 or redis_aof_last_write_status == 0
        for: 5m
        labels: { severity: page }
        annotations: { summary: "Redis persistence is failing, writes may be blocked" }

      - alert: RedisReplicaLinkDown
        expr: redis_master_link_up == 0
        for: 5m
        labels: { severity: ticket }
        annotations: { summary: "Replica lost its link to the primary" }

      - alert: RedisUnexpectedRestart
        expr: changes(redis_uptime_in_seconds[10m]) > 0 and redis_uptime_in_seconds < 600
        labels: { severity: ticket }
        annotations: { summary: "Redis restarted recently" }

      - alert: RedisHitRatioDrop
        expr: |
          rate(redis_keyspace_hits_total[10m])
          / clamp_min(rate(redis_keyspace_hits_total[10m]) + rate(redis_keyspace_misses_total[10m]), 1) < 0.7
        for: 15m
        labels: { severity: ticket }
        annotations: { summary: "Cache hit ratio dropped below 70% (tune to your baseline)" }
```

### Page or ticket?

| Page someone (act now) | Ticket (act this week) |
|------------------------|------------------------|
| Redis down, persistence failing, rejecting connections | Memory trending above 80 percent |
| Cluster state failed, no primary | Evictions on a cache |
| Errors visible to users | Fragmentation growing, hit ratio drifting |
| Replication broken on a durable dataset | Replica lag creeping up |

Every paging alert needs a **runbook link**, a clear owner and a threshold you trust. Delete or downgrade alerts nobody acts on.

## Application-side metrics

Server metrics show how Redis feels. Client metrics show how **your app** experiences it, including network and queueing.

```ts
import client from "prom-client";

const redisDuration = new client.Histogram({
  name: "app_redis_command_duration_seconds",
  help: "Redis command latency as seen by the app",
  labelNames: ["op", "outcome"],
  buckets: [0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1],
});

const redisErrors = new client.Counter({
  name: "app_redis_errors_total",
  help: "Redis errors by type",
  labelNames: ["op", "kind"],
});

export async function instrument<T>(op: string, fn: () => Promise<T>): Promise<T> {
  const end = redisDuration.startTimer({ op });
  try {
    const result = await fn();
    end({ outcome: "ok" });
    return result;
  } catch (err) {
    end({ outcome: "error" });
    const kind = err instanceof Error ? err.message.split(" ")[0] : "unknown";  // for example WRONGPASS, OOM
    redisErrors.inc({ op, kind });
    throw err;
  }
}

const user = await instrument("user.get", () => redis.hgetall(`user:${id}`));
```

Use **operation names** (`user.get`), never raw keys, as labels. Keys have unbounded cardinality and will overwhelm your metrics system.

Also export connection state and cache effectiveness:

```ts
new client.Gauge({
  name: "app_redis_connection_ready",
  help: "1 when the Redis client is ready",
  collect() { this.set(redis.status === "ready" ? 1 : 0); },
});

const cacheResult = new client.Counter({
  name: "app_cache_requests_total",
  help: "Cache lookups",
  labelNames: ["cache", "result"],        // result: hit | miss | error
});
```

Compare the **client p99** with Redis' own `SLOWLOG`. High client latency with an empty slow log points to the network, the Node event loop or connection queueing, not Redis.

For distributed tracing, OpenTelemetry has an ioredis instrumentation package that adds a span per command. Add it if you already run tracing.

## Health checks

Decide what Redis health means for your service **before** wiring it to restarts.

```ts
const withTimeout = <T>(p: Promise<T>, ms: number) =>
  Promise.race([p, new Promise<T>((_, rej) => setTimeout(() => rej(new Error("timeout")), ms))]);

// Liveness: is the process healthy? Do NOT depend on Redis here.
app.get("/healthz/live", (_req, res) => res.sendStatus(200));

// Readiness: can this instance serve traffic?
const REDIS_REQUIRED = process.env.REDIS_REQUIRED === "true";   // false for a pure cache

app.get("/healthz/ready", async (_req, res) => {
  try {
    await withTimeout(redis.ping(), 500);
    res.json({ redis: "up" });
  } catch {
    if (REDIS_REQUIRED) return res.status(503).json({ redis: "down" });
    res.json({ redis: "degraded" });                             // cache: keep serving from the source
  }
});
```

Mistakes to avoid:

- Tying **liveness** to Redis. A Redis blip then restarts every pod at once and makes the outage worse
- Failing **readiness** for a cache-only dependency, which removes all instances from the load balancer for no benefit
- A health check that runs expensive commands (`KEYS`, big reads). Use `PING`

## Logging

### Server logs

```bash
loglevel notice            # notice is a good production default
logfile ""                 # stdout in containers and systemd, collected by the platform
# or: logfile /var/log/redis/redis.log  (rotate with logrotate)
syslog-enabled no
```

Ship logs to your central system and keep them. Redis logs are short, and they explain incidents that metrics only hint at.

| Log message (paraphrased) | Meaning | Action |
|---------------------------|---------|--------|
| Warnings about `overcommit_memory`, Transparent Huge Pages, `somaxconn` at startup | OS not tuned | Fix before production |
| "Can't save in background: fork" | Not enough memory to fork | Lower `maxmemory`, add RAM, check `overcommit_memory` |
| "Background saving error" | RDB write failed | Disk space, permissions, memory |
| "Asynchronous AOF fsync is taking too long" | Disk is slow or busy | Faster disk, separate noisy neighbors |
| Replica "lost connection" or "sync started" | Replication hiccups, full resyncs | Check network, backlog size, replica lag |
| "Possible SECURITY ATTACK detected" | Something is speaking HTTP or another protocol to Redis | Check exposure and firewall now |
| Loading messages after a restart | Dataset loading | Expect `LOADING` errors for clients |
| Failover messages (Sentinel or Cluster) | Primary changed | Verify clients reconnected, find the root cause |
| Out-of-memory or OOM kill lines in the host log | Kernel killed Redis | Fix limits and `maxmemory` |

Create alerts on a small set of these patterns (fork failures, security warnings, persistence errors).

### Slow log and latency monitor

Keep them enabled (see [Deployment](./01_deployment.md)), and capture them periodically, because the slow log is a ring buffer that old entries fall out of.

```ts
const seen = new Set<number>();

setInterval(async () => {
  const entries = (await redis.call("SLOWLOG", "GET", "50")) as any[];
  for (const [id, ts, micros, args] of entries) {
    if (seen.has(id)) continue;
    seen.add(id);
    log.warn({ id, at: new Date(ts * 1000).toISOString(), ms: micros / 1000, cmd: args.slice(0, 2) }, "redis slow command");
  }
}, 60_000);
```

Log only the **command name and first argument at most**. Arguments can contain personal data or secrets.

### Application logs

- Log **structured** events (JSON) with a request or correlation ID
- Log connection lifecycle events (`ready`, `close`, `reconnecting`, `end`) at info or warn level
- Log errors with `err.name`, `err.code` and `err.message`
- **Never** log values, passwords, tokens or full connection URLs
- Sample or rate-limit noisy logs (a down Redis can produce thousands of identical errors per second)

```ts
redis.on("reconnecting", (delay: number) => log.warn({ delay, status: redis.status }, "redis reconnecting"));
redis.on("end", () => log.error("redis connection ended, no further retries"));
```

### Keyspace notifications (use sparingly)

```bash
notify-keyspace-events Ex      # publish expired-key events (costs CPU and bandwidth)
```

Useful for debugging expiry or driving features, but enabling broad classes (`KEA`) on a busy server adds measurable overhead. Pub/Sub delivery is also not guaranteed, so do not build critical logic on it.

## Dashboard layout

A single dashboard that works for on-call:

| Row | Panels |
|-----|--------|
| **Health** | Up/down, uptime, role, replicas connected, cluster state, persistence status |
| **Saturation** | Memory used vs max, fragmentation, evictions/s, clients vs `maxclients`, CPU, network |
| **Traffic** | Ops/sec, commands by type (top 5), hit ratio, expired keys/s |
| **Latency** | Client p50/p95/p99 by operation, slow log count, fork time |
| **Replication** | Lag, offsets, full resyncs, link status |
| **Keyspace** | Keys per DB, keys with TTL, key count growth |
| **Application** | Client errors by kind, connection-ready gauge, cache hit/miss/error |

Show **deploy and failover markers** on the graphs. Most incidents correlate with a change.

## Capacity planning

Review monthly:

- Memory growth rate: when will you reach 80 percent?
- Key count growth, and whether TTL coverage (`expires` vs `keys` in `INFO keyspace`) is stable
- Peak ops/sec versus what a single core can sustain (benchmark with your workload)
- Connection growth with new services and autoscaling
- Network and disk headroom for backups and resyncs

```ts
const ks = parseInfo(await redis.info("keyspace"));   // parseInfo is defined in 18_testing-and-debugging/02_debugging.md
// db0: "keys=125000,expires=124800,avg_ttl=1795000"
```

Keys without TTL growing steadily in a cache usually mean a code path forgot `EX`.

## Monitoring Sentinel and Cluster

```bash
# Sentinel
redis-cli -p 26379 SENTINEL masters
redis-cli -p 26379 SENTINEL replicas mymaster
redis-cli -p 26379 SENTINEL ckquorum mymaster

# Cluster
redis-cli CLUSTER INFO | egrep 'cluster_state|cluster_slots_(ok|fail)|cluster_known_nodes'
redis-cli --cluster check node1:6379
```

Alert on: fewer than the expected sentinels, quorum not reachable, `cluster_state` not `ok`, any failed slots and unbalanced slot or memory distribution between shards.

## Managed services

Providers expose their own metrics (CPU, memory, connections, evictions, replication lag, snapshots). Map them to the signals above, set alerts on them and still add **client-side** metrics. The provider cannot see your application's latency.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Only monitoring "is it up" | Add memory, evictions, persistence, replication, latency |
| Alerts on raw thresholds that fire constantly | Base on baselines and `for:` durations, then prune noisy alerts |
| Paging without runbooks | Every page links to a runbook |
| Key names as metric labels | Use operation names with bounded cardinality |
| Liveness probe depends on Redis | Liveness checks the process only |
| Logging values, secrets or URLs | Redact, log metadata only |
| `MONITOR` as a monitoring tool | Never in production. Use metrics and slow log |
| Slow log not captured | Poll and persist entries, since it is a ring buffer |
| No client-side latency metrics | Instrument operations in the app |
| Ignoring `commandstats` | Review for expensive or unexpected commands |
| Dashboards without deploy markers | Annotate deploys and failovers |
| Monitoring user with write access | Read-only ACL user |

## Key takeaways

- Watch latency, traffic, errors and saturation, plus persistence, replication and hit ratio
- Scrape `INFO` with an exporter, graph it, and alert only on symptoms and risks you can act on
- Add client-side metrics and health checks, and keep liveness independent of Redis
- Keep the slow log and latency monitor on, and collect their output
- Ship server and app logs centrally, structured and free of secrets
- Review capacity trends monthly, before memory or connections run out

**Previous:** [Deployment](./01_deployment.md) | **Next:** [Backup and Disaster Recovery](./03_backup-and-disaster-recovery.md)
