# 19 Redis in Production

Running Redis well is a different skill from using it well. In development a restart costs nothing and memory is unlimited. In production a restart can mean a cold cache that melts your database, a full disk can block writes, and a failover you never rehearsed can take an hour. This chapter covers how to **deploy**, **observe**, **protect** and **operate** Redis so that failures are boring.

```
        ┌───────────┐     ┌───────────┐     ┌───────────┐     ┌───────────┐
        │  Deploy   │ ──► │  Observe  │ ──► │  Protect  │ ──► │  Operate  │
        │ topology, │     │ metrics,  │     │ backups,  │     │ runbooks, │
        │ sizing,   │     │ logs,     │     │ restores, │     │ upgrades, │
        │ config    │     │ alerts    │     │ failover  │     │ drills    │
        └───────────┘     └───────────┘     └───────────┘     └───────────┘
              ▲                                                     │
              └──────────── learn from every incident ──────────────┘
```

## What you will learn

- Pick a topology (standalone, replication, Sentinel, Cluster, managed) and size it
- Tune the OS and write a production `redis.conf`
- Deploy with systemd, Docker and Kubernetes
- Configure ioredis clients that survive restarts and failovers
- Monitor the metrics that matter, and alert without drowning in noise
- Back up, restore and **prove** the restore works
- Run a go-live review and keep a steady operational routine

## Contents

| # | File | Topic |
|---|------|-------|
| 01 | [Deployment](./01_deployment.md) | Topologies, sizing, OS tuning, config, systemd, Docker, Kubernetes, upgrades |
| 02 | [Monitoring and Logging](./02_monitoring-and-logging.md) | Key metrics, Prometheus, alerts, app-side metrics, health checks, logs |
| 03 | [Backup and Disaster Recovery](./03_backup-and-disaster-recovery.md) | RDB and AOF, backups, restores, failover drills, DR planning |
| 04 | [Production Checklist](./04_production-checklist.md) | Go-live gate, preflight script, runbooks, operating cadence |

## Principles

1. **Everything fails.** Plan for the process, the host, the zone and the operator
2. **Replication is not a backup.** A bad write replicates instantly
3. **An untested backup is a hope, not a backup.** Rehearse restores
4. **Measure before you alert, alert before you page.** Every page needs a runbook
5. **Decide what Redis is for.** A disposable cache and a primary datastore need different durability, so keep them on separate instances
6. **Automate and codify.** Config, deploys, backups and alerts belong in version control
7. **Change slowly.** Roll out upgrades and config changes one node at a time, replicas first

## Know your data class

Almost every production decision depends on one question: *what happens if this data is lost?*

| Data class | Examples | Loss impact | Typical setup |
|------------|----------|-------------|---------------|
| Disposable cache | API response cache, rendered fragments | Slower for a while | No persistence needed, `allkeys-lru`, protect the database from a cold start |
| Rebuildable state | Leaderboards, counters, rate limits | Annoying, recoverable | RDB or AOF, replica |
| Important state | Sessions, queues, idempotency keys | User-visible damage | AOF `everysec`, replicas in other zones, backups, `noeviction` |
| Source of truth | Data that exists only in Redis | Business loss | Strongest durability, multiple backups, strict drills. Question whether Redis should hold it |

## Maturity levels

| Level | What it looks like |
|-------|--------------------|
| **1. Basic** | Single instance, auth and TLS, `maxmemory`, TTLs, basic metrics (memory, clients, up/down) |
| **2. Solid** | Replica or managed HA, automated backups, dashboards, alerts with runbooks, pinned versions, infrastructure as code |
| **3. Resilient** | Multi-zone HA with rehearsed failover, tested restores with measured RTO, capacity forecasting, load tests, game days, SLOs |

Aim for level 2 before launch and level 3 for anything that matters to your business.

## Prerequisites

- [Redis Architecture](../15_redis-architecture/01_replication.md): replication, Sentinel and Cluster
- [Performance](../16_performance/01_latency-and-benchmarking.md) and [Memory Optimization](../16_performance/03_memory-optimization.md)
- [Security](../17_security/README.md): ACL and TLS are assumed throughout this chapter
- [Testing and Debugging](../18_testing-and-debugging/README.md): the diagnostic commands used here

**Start:** [Deployment](./01_deployment.md)
