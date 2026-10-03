# 15 · Redis Architecture

A single Redis instance is fast and simple, and it is also a **single point of failure** with a **single machine's memory**. This module covers how Redis is scaled and made highly available, and exactly how to configure **ioredis** for each topology.

[00_introduction/03_redis-architecture.md](../00_introduction/03_redis-architecture.md) sketched the pieces. Here we operate them.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Replication](./01_replication.md) | Primary and replicas, asynchronous copying, read scaling, lag, `WAIT` |
| 02 | [Sentinel](./02_sentinel.md) | Automatic failover and service discovery for a single-primary setup |
| 03 | [Cluster](./03_cluster.md) | Hash-slot sharding across many primaries, redirects, `CROSSSLOT`, resharding |
| 04 | [High Availability and Failover](./04_high-availability-and-failover.md) | RTO and RPO, failover anatomy, client resilience, testing failures |

## Learning outcomes

After this module you can:

- Set up and monitor primary/replica replication, and explain what is lost on failover
- Run Sentinel and connect ioredis in Sentinel mode
- Run a Redis Cluster, design keys for it, and use `ioredis.Cluster` correctly
- Choose among standalone, replication, Sentinel, Cluster and managed services for a given workload
- Tune detection and reconnection so failovers are short, and apps degrade instead of crashing
- Test failures deliberately and measure downtime and data loss

## Three different problems

| Problem | Solution | Redis feature |
|---------|----------|---------------|
| **A node can die**, and I need the service to continue | Redundancy plus automatic promotion | Replication, **Sentinel**, Cluster failover |
| **One machine can't hold all the data** or handle all the traffic | Spread data across machines | **Cluster** (or client-side sharding) |
| **Reads overwhelm the primary** | Serve reads from copies | **Replicas** (with stale-read trade-offs) |

Replication copies data. Sentinel adds **failover**. Cluster adds **sharding** (and its own failover). They are layers, not alternatives to mix freely.

## Choosing a topology

| Topology | Survives node loss? | Scales memory? | Scales reads? | Complexity | Typical use |
|----------|---------------------|----------------|---------------|------------|-------------|
| **Standalone + persistence** | No (restart and reload) | No | No | Lowest | Dev, small caches |
| **Primary + replicas** | Manual failover | No | **Yes** | Low | Read scaling, backups, base for the rest |
| **Primary + replicas + Sentinel** | **Yes**, automatic | No | Yes | Medium | Most production apps whose data **fits on one node** |
| **Cluster** | **Yes**, per shard | **Yes** | Yes | High | Datasets or throughput beyond one node |
| **Managed service** (ElastiCache, Memorystore, Azure, Redis Cloud) | Provider handles it | Depends on the plan | Yes | Lowest operational effort | Most teams, most of the time |

```
Does the data fit on one machine, with room to grow, and one primary can handle the writes?
├─ yes ─► Replication (+ Sentinel, or a managed Multi-AZ service)
└─ no  ─► Cluster (or a managed cluster-mode service)

Is Redis only a disposable cache that can be rebuilt?
└─ you may not need HA at all: a single instance with a warm-up plan can be enough
```

## One truth to carry through the whole module

Redis replication is **asynchronous**. When a primary fails, writes it **acknowledged but had not yet copied** to a replica are **lost**. Every topology here narrows that window, but none of the open-source options eliminates it. Design your use of Redis so that a small loss is **survivable** (caches, sessions, queues with idempotent jobs, rebuildable state), or keep the source of truth in a database ([04_high-availability-and-failover.md](./04_high-availability-and-failover.md)).

## Prerequisites

- Completed [14_queues-and-workers](../14_queues-and-workers/README.md)
- Comfortable with [Persistence](../02_redis-fundamentals/04_persistence.md), [Connection Management](../08_nodejs-integration/01_connection-management.md), and [ioredis configuration](../03_ioredis-basics/02_configuration.md) (`retryStrategy`, `reconnectOnError`)

## Next

Continue to [16_performance](../16_performance/README.md).
