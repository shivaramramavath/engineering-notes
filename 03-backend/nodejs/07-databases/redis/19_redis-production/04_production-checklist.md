# Production Checklist

One page to run before launch and revisit regularly. It pulls together the decisions from the whole course: architecture, memory, security, clients, monitoring, backups and operations. Use it as a **go-live gate**: every unchecked box is either fixed or consciously accepted, with an owner.

```
Design ─► Capacity ─► Durability ─► Security ─► Clients ─► Observability ─► Operations ─► Launch
```

## How to use it

1. Copy this list into your launch ticket or runbook repository
2. Mark each item **Done**, **N/A** (with a reason) or **Accepted risk** (with an owner and date)
3. Run the [preflight script](#automated-preflight-script) against the real environment
4. Hold a short review with application, platform and on-call owners
5. Re-run after major changes and at least quarterly

## 1. Design and topology

- [ ] Data class decided for each use: disposable cache, rebuildable state, important state, source of truth
- [ ] Cache and important data are on **separate instances** (or at least eviction rules protect the important keys)
- [ ] Topology matches the availability need: replica, Sentinel, Cluster or managed HA
- [ ] Failure domains: replicas and sentinels in **different hosts and zones**
- [ ] Cluster only if needed, and multi-key operations use hash tags
- [ ] RPO and RTO written down per dataset
- [ ] Redis version supported, pinned and identical across nodes
- [ ] Staging mirrors production topology, version and config

## 2. Capacity and memory

- [ ] Memory sized from measured data, with growth projection for at least 6 to 12 months
- [ ] `maxmemory` set, below the host or container limit, with fork headroom (about 60 to 70 percent of RAM when persisting)
- [ ] `maxmemory-policy` chosen on purpose (`allkeys-lru`/`lfu` for caches, `noeviction` or `volatile-*` for important data)
- [ ] Every cache and session key has a **TTL** (with jitter where mass expiry is a risk)
- [ ] No unbounded lists, streams, sorted sets or hashes (`LTRIM`, `MAXLEN`, bucketed keys)
- [ ] No big keys (`redis-cli --memkeys` clean), deletions use `UNLINK`
- [ ] `maxclients` fits the worst-case connection count (autoscaling included)
- [ ] CPU headroom: peak ops/sec benchmarked, well under one core's limit
- [ ] Network bandwidth and disk space sized for replication, backups and rewrites (2 to 3 times dataset on disk)

See [Memory Optimization](../16_performance/03_memory-optimization.md).

## 3. Host and OS

- [ ] `vm.overcommit_memory = 1`, THP disabled, swap minimal, `somaxconn` raised
- [ ] File descriptor limit above `maxclients` + 32
- [ ] Time synchronization enabled
- [ ] No startup warnings in the Redis log
- [ ] Runs as a dedicated non-root user under a supervisor (systemd, Docker restart policy, Kubernetes)
- [ ] Shutdown timeout long enough for a final save
- [ ] Containers: persistent volume at the data directory, memory limit above `maxmemory`, pinned image tag
- [ ] Kubernetes: StatefulSet with PVCs, PodDisruptionBudget, anti-affinity, probes that tolerate loading

## 4. Persistence and backups

- [ ] Persistence mode matches the data class (none for pure cache, AOF `everysec` plus RDB for important data)
- [ ] Real loss window documented (`everysec`, async replication, failover)
- [ ] `WAIT` or `min-replicas-to-write` used where acknowledged writes must survive failover
- [ ] Automated backups, taken from a replica, verified with `redis-check-rdb`
- [ ] Backups copied offsite (other zone or region), **encrypted**, versioned or immutable, with a retention policy
- [ ] Alert if the newest backup is too old or the backup job fails
- [ ] **Restore tested** end to end, with measured RTO and a post-restore sanity check
- [ ] Restore procedure documented, including the AOF caveat (restore with AOF off, then re-enable)
- [ ] Failover drill completed (Sentinel or Cluster) with results recorded

See [Backup and Disaster Recovery](./03_backup-and-disaster-recovery.md).

## 5. Security

- [ ] Not reachable from the internet. Private network, firewall or security groups, `bind` set, `protected-mode yes`
- [ ] Docker ports bound to localhost or a private interface
- [ ] TLS for clients, replication and cluster, with plain-text port disabled, certificates verified, expiry alert
- [ ] ACL users per service. `default` user disabled or restricted
- [ ] Least privilege: key patterns, channel patterns, `@dangerous` and `@admin` denied for applications
- [ ] Strong random secrets in a secret manager, rotation procedure tested
- [ ] Config file, ACL file and data directory permissions locked down. Disk and backups encrypted
- [ ] User input never forms raw keys, patterns, commands or Lua text
- [ ] No secrets or full connection strings in logs

See [Security Checklist](../17_security/03_security-checklist.md).

## 6. Client and application

- [ ] One shared client per process. Separate connections for subscribers and blocking commands
- [ ] `connectTimeout`, `commandTimeout`, bounded `maxRetriesPerRequest`, **jittered** `retryStrategy`
- [ ] `reconnectOnError` handles `READONLY` after failover. Sentinel or Cluster options configured correctly
- [ ] `error` handler attached, `connectionName` set
- [ ] Graceful shutdown: `quit()` after draining requests
- [ ] Cache calls **fail open**, with a circuit breaker or timeout so Redis problems do not become outages
- [ ] Cold-cache protection: request coalescing or locks, TTL jitter, load shedding for rebuilds
- [ ] Pipelines and `MGET` instead of loops of awaits. No `KEYS`, no unbounded `HGETALL` or `SMEMBERS`
- [ ] Atomic operations (single commands, Lua, or `SET NX`) instead of read-modify-write
- [ ] `MULTI` and pipeline results are inspected for per-command errors
- [ ] Values validated on read, with a versioned serialization format
- [ ] Locks have TTLs, unique tokens and safe release
- [ ] Queue and stream consumers acknowledge, retry with backoff, use a dead-letter path and are idempotent
- [ ] Keys built in one place with documented namespaces

See [Common Pitfalls](../18_testing-and-debugging/03_common-pitfalls.md).

## 7. Monitoring, logging and alerting

- [ ] Exporter and dashboards: health, saturation, traffic, latency, replication, persistence
- [ ] Alerts: down, memory, evictions, rejected connections, persistence failure, replication, unexpected restart
- [ ] Every paging alert has an owner and a **runbook link**. Noisy alerts removed
- [ ] Client-side latency and error metrics, labeled by operation (not by key)
- [ ] Health checks: liveness independent of Redis. Readiness reflects whether Redis is required
- [ ] Slow log and latency monitor enabled, slow log entries captured
- [ ] Server and app logs centralized, structured and redacted
- [ ] Deploy and failover markers on dashboards
- [ ] Capacity review scheduled (memory, keys, ops/sec, connections)

See [Monitoring and Logging](./02_monitoring-and-logging.md).

## 8. Testing before launch

- [ ] Integration tests run against real Redis of the production version
- [ ] Concurrency tests for counters, locks and scripts
- [ ] Failure tests: Redis down, slow, connection dropped, restarted
- [ ] **Load test** at expected peak and at 2 to 3 times that, watching latency, memory, evictions, connections
- [ ] **Soak test** long enough to see memory growth, TTL behavior and fragmentation
- [ ] **Chaos or game day**: kill the primary, block the network, fill the disk, flush staging cache under load
- [ ] Rollout and rollback plan for the application change that introduces or modifies Redis usage

See [Testing with Redis](../18_testing-and-debugging/01_testing-with-redis.md) and [Latency and Benchmarking](../16_performance/01_latency-and-benchmarking.md).

## 9. Operations and process

- [ ] Named owner and on-call rotation for Redis
- [ ] Config, ACLs, alerts and deploy pipelines in version control
- [ ] Change process: one node at a time, replicas first, verify after each step
- [ ] Upgrade plan with pre-upgrade backup and tested rollback
- [ ] Runbooks for the common incidents (see below)
- [ ] Access to production Redis limited, audited, with a read-only debug user ready
- [ ] Incident reviews feed back into tests, alerts and runbooks
- [ ] Provider limits, maintenance windows and support contacts recorded (managed services)

## Automated preflight script

A quick, repeatable check that reads the live server. Managed services often block `CONFIG`, so an unreadable setting is reported as `unknown`: verify those manually.

```ts
import Redis from "ioredis";

type Result = { check: string; status: "pass" | "warn" | "fail" | "unknown"; detail: string };

function parseInfo(raw: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const line of raw.split("\r\n")) {
    if (!line || line.startsWith("#")) continue;
    const i = line.indexOf(":");
    if (i > 0) out[line.slice(0, i)] = line.slice(i + 1);
  }
  return out;
}

async function cfg(redis: Redis, name: string): Promise<string | null> {
  try {
    const res = (await redis.config("GET", name)) as string[];
    return res[1] ?? null;
  } catch {
    return null;
  }
}

export async function preflight(redis: Redis, opts: { durable: boolean; expectReplicas?: number } = { durable: true }) {
  const results: Result[] = [];
  const add = (check: string, status: Result["status"], detail: string) => results.push({ check, status, detail });

  const info = parseInfo(await redis.info());
  add("version", "pass", info.redis_version ?? "?");

  // memory
  const maxmemory = Number(info.maxmemory ?? 0);
  const used = Number(info.used_memory ?? 0);
  if (maxmemory === 0) add("maxmemory", "fail", "unlimited; set a limit below host/container memory");
  else {
    add("maxmemory", "pass", `${(maxmemory / 2 ** 30).toFixed(2)} GiB`);
    const pct = used / maxmemory;
    add("memory usage", pct > 0.85 ? "fail" : pct > 0.7 ? "warn" : "pass", `${(pct * 100).toFixed(0)}% of maxmemory`);
  }

  const policy = info.maxmemory_policy ?? (await cfg(redis, "maxmemory-policy"));
  if (!policy) add("eviction policy", "unknown", "could not read");
  else if (opts.durable && policy !== "noeviction" && !policy.startsWith("volatile"))
    add("eviction policy", "fail", `${policy} can delete important data; use noeviction or volatile-*`);
  else add("eviction policy", "pass", policy);

  const frag = Number(info.mem_fragmentation_ratio ?? 1);
  add("fragmentation", frag > 1.5 || frag < 1 ? "warn" : "pass", `ratio ${frag}`);

  // persistence
  const aof = info.aof_enabled === "1";
  const save = await cfg(redis, "save");
  if (opts.durable && !aof && !save) add("persistence", "fail", "no AOF and no RDB schedule for durable data");
  else add("persistence", "pass", `aof=${aof} save="${save ?? "unknown"}"`);
  if (info.rdb_last_bgsave_status && info.rdb_last_bgsave_status !== "ok")
    add("last bgsave", "fail", info.rdb_last_bgsave_status);
  if (aof && info.aof_last_write_status && info.aof_last_write_status !== "ok")
    add("aof write", "fail", info.aof_last_write_status);

  // replication
  if (info.role === "master") {
    const replicas = Number(info.connected_slaves ?? 0);
    const want = opts.expectReplicas ?? (opts.durable ? 1 : 0);
    add("replicas", replicas >= want ? "pass" : "fail", `${replicas} connected (expected at least ${want})`);
  } else if (info.master_link_status !== "up") {
    add("replication link", "fail", `master_link_status=${info.master_link_status}`);
  }

  // clients
  const maxclients = Number((await cfg(redis, "maxclients")) ?? 0);
  const clients = Number(info.connected_clients ?? 0);
  if (maxclients) add("clients", clients / maxclients > 0.7 ? "warn" : "pass", `${clients}/${maxclients}`);
  else add("clients", "unknown", `${clients} connected`);

  // observability settings
  const slow = await cfg(redis, "slowlog-log-slower-than");
  add("slowlog", slow !== null && Number(slow) >= 0 ? "pass" : slow === null ? "unknown" : "warn", `threshold ${slow ?? "?"} µs`);
  const lat = await cfg(redis, "latency-monitor-threshold");
  add("latency monitor", lat === null ? "unknown" : Number(lat) > 0 ? "pass" : "warn", `threshold ${lat ?? "?"} ms`);

  // exposure and security basics
  const bind = await cfg(redis, "bind");
  if (bind && /(^|\s)(0\.0\.0\.0|\*)(\s|$)/.test(bind)) add("bind", "warn", `${bind} (confirm firewall)`);
  if ((await cfg(redis, "protected-mode")) === "no") add("protected-mode", "warn", "off");

  try {
    const users = (await redis.call("ACL", "LIST")) as string[];
    const open = users.find((u) => /^user default on\b/.test(u) && u.includes("nopass"));
    add("default user", open ? "fail" : "pass", open ? "enabled without password" : "restricted or disabled");
  } catch {
    add("default user", "unknown", "ACL LIST not available");
  }

  return results;
}

// usage
// const r = await preflight(redis, { durable: true, expectReplicas: 1 });
// console.table(r);
// if (r.some((x) => x.status === "fail")) process.exit(1);
```

Run it from CI against staging and as a scheduled job against production (read-only user permitting), and fail the pipeline on `fail`.

## Runbook skeletons

Write one per common incident, and keep them short enough to follow at 3 a.m.

### Memory full or evicting

| Step | Action |
|------|--------|
| 1 | Check `INFO memory`, `evicted_keys`, `INFO keyspace` (keys vs keys with TTL) |
| 2 | `redis-cli --memkeys` to find large or fast-growing keys |
| 3 | Did a deploy add keys without TTL, or a loop that never stops? Roll back if so |
| 4 | Short term: delete or expire the offending keys with `UNLINK` or `EXPIRE`, raise `maxmemory` if host memory allows |
| 5 | Long term: TTLs, bounds, compaction, more memory or sharding. Add a test or alert |

### Latency spike

| Step | Action |
|------|--------|
| 1 | `SLOWLOG GET`, `LATENCY DOCTOR`, `INFO commandstats`: any new expensive command? |
| 2 | `INFO persistence`: fork running? `latest_fork_usec` high? AOF fsync slow? |
| 3 | `INFO memory`: fragmentation below 1 (swap)? Eviction storm? |
| 4 | `CLIENT LIST`: blocked clients, huge output buffers, connection surge? |
| 5 | Host: CPU steal, disk latency, network. Compare client p99 to server timings |

### Failover happened

| Step | Action |
|------|--------|
| 1 | Confirm the new primary (`SENTINEL get-master-addr-by-name`, `CLUSTER NODES`) |
| 2 | Check clients reconnected (connection-ready gauge, `CLIENT LIST`, error rates) |
| 3 | Check for lost writes against your canary key and application invariants |
| 4 | Restore the failed node as a replica, wait for sync |
| 5 | Root cause the failure and record the measured downtime |

### Connections exhausted

| Step | Action |
|------|--------|
| 1 | `CLIENT LIST` grouped by `name` and `addr` to find the leaking service |
| 2 | Roll back or restart the offender. Kill idle clients if needed (`CLIENT KILL`) |
| 3 | Fix: a single shared client, pool limits, `timeout` setting |
| 4 | Consider raising `maxclients` only after fixing the cause |

### Redis down

| Step | Action |
|------|--------|
| 1 | Is the process running, did the OS OOM-kill it (`dmesg`, host logs)? |
| 2 | Is it loading (`LOADING`) or failing to start (config error, corrupt AOF, disk full)? |
| 3 | If a replica exists and failover has not happened, promote it |
| 4 | Confirm apps degrade gracefully (cache fails open). Protect the database |
| 5 | After recovery: verify data against the canary, review the persistence loss window |

## Ownership matrix

| Area | Owner | Reviews |
|------|-------|---------|
| Redis infrastructure, upgrades, backups | Platform or SRE | Quarterly |
| Key design, TTLs, data model | Application team | Each feature |
| Client configuration library | Platform plus app leads | Each Redis or ioredis upgrade |
| Alerts and runbooks | On-call team | After every incident |
| Security and access | Security plus platform | Quarterly |
| Cost and capacity | Platform plus finance | Monthly |

## Operating cadence

| Frequency | Tasks |
|-----------|-------|
| **Daily** | Glance at dashboards. Confirm backups succeeded. Triage alerts |
| **Weekly** | Review slow log and `commandstats`, evictions and hit ratio, TTL coverage, top keys by memory |
| **Monthly** | Capacity forecast. Automated restore test. Review and prune alerts. Patch review |
| **Quarterly** | Failover drill. Full DR drill with measured RTO. Access and secret rotation review. Rerun this checklist |
| **On every upgrade** | Release notes, staging test, backup, rolling upgrade, restore test with the new version |
| **After every incident** | Blameless review, then a new test, alert, runbook change or config fix |

## Launch gate

Before the first production traffic:

```
[ ] Sections 1 to 9 reviewed; failures fixed or accepted with an owner
[ ] Preflight script clean (or exceptions documented)
[ ] Latest backup restored successfully in a scratch environment
[ ] Failover drill performed and time recorded
[ ] Load test results at 2 to 3 times expected peak attached to the ticket
[ ] Dashboards live, alerts firing to the right channel (send a test alert)
[ ] Runbooks reviewed by someone who did not write them
[ ] Rollback plan for the application release agreed
[ ] On-call aware of the launch window
```

## Common mistakes at go-live

| Mistake | Consequence | Prevention |
|---------|-------------|------------|
| Launching with no backup restore ever tested | Discover broken backups during the disaster | Restore drill is a launch gate |
| Cache and critical data in one instance with `allkeys-lru` | Queue or session data silently evicted | Separate instances, or `volatile-*` with TTLs only on cache keys |
| No `maxmemory` | Kernel OOM kill | Always set it, below the container limit |
| Clients without timeouts | A Redis stall freezes the whole app | `commandTimeout`, circuit breaker, fail open |
| Staging differs from production | Surprises in topology-specific behavior | Same version and topology |
| Alerts untested | Silent failure | Fire a test alert, test the escalation |
| Single-zone deployment | A zone outage is a full outage | Spread nodes across zones |
| Credentials shared across environments | A test run damages production | Separate credentials and endpoints |
| No plan for a cold cache | Database melts after restart | Coalescing, jitter, warm-up, load shedding |
| Nobody owns Redis | Issues drift, alerts ignored | Named owner and on-call |

## Key takeaways

- A launch gate beats heroics: check design, capacity, durability, security, clients, observability and operations before traffic
- Decide data classes first. Everything else follows from what loss you can tolerate
- Automate the checks (preflight script) and rehearse the failures (restore and failover drills)
- Write short runbooks for the incidents you expect: memory, latency, failover, connections, outage
- Keep a steady cadence of reviews, drills and capacity checks, and feed every incident back into tests and alerts
- When in doubt, ask: *what happens to the users, and to the database, when Redis disappears for ten minutes?*

**Previous:** [Backup and Disaster Recovery](./03_backup-and-disaster-recovery.md) | **Next:** [Idempotency](../20_real-world-patterns/01_idempotency.md)
