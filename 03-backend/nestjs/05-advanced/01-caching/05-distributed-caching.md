# Distributed Caching Architecture

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 01-caching
- **Prerequisites**: [01-caching-fundamentals](./01-caching-fundamentals.md), [03-redis](./03-redis.md), [04-cache-invalidation](./04-cache-invalidation.md)

---

## 1. Context & Purpose

As microservices scale horizontally across dozens of containers and multiple cloud regions, single-node caching architectures break down. A centralized standalone Redis instance becomes a single point of failure (SPOF) and a network bottleneck, while isolated in-memory caches suffer from low hit rates and state inconsistency.

**Distributed Caching** partitions cached datasets across multiple nodes, coordinates cache state across replicas, and implements multi-tier caching (L1 local process memory + L2 shared distributed cluster). This guide covers distributed caching topologies, consistent hashing, Redis Cluster configuration, and dual-tier L1/L2 cache synchronization with Redis Pub/Sub in NestJS 11.

---

## 2. Theoretical Foundations

### 2.1 Consistent Hashing
In a distributed cache cluster, keys are mapped to storage nodes using hash slots:
$$\text{Slot} = \text{CRC16}(\text{key}) \pmod{16384}$$
Redis Cluster partitions these 16,384 slots across master nodes. Consistent hashing ensures that adding or removing a cache node only migrates a fraction ($1/N$) of keys, avoiding full cache evictions.

### 2.2 Multi-Tier Caching (L1 + L2)
- **L1 (Local In-Memory Cache)**: Stored inside the Node.js process heap (e.g. LRU memory store). Latency: $< 0.01\text{ms}$. Zero network hop.
- **L2 (Shared Distributed Cache)**: Stored in a Redis Cluster shared by all service pods. Latency: $1\text{ms}–3\text{ms}$.
- **Invalidation Sync via Pub/Sub**: When Pod A mutates data, it writes to L2 and publishes an invalidation event over Redis Pub/Sub. Pods B, C, and D receive the event and purge their local L1 caches immediately.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   Kubernetes Cluster                   │
│                                                        │
│   ┌─────────────────────┐    ┌─────────────────────┐   │
│   │     Pod 1 (NestJS)  │    │     Pod 2 (NestJS)  │   │
│   │  ┌───────────────┐  │    │  ┌───────────────┐  │   │
│   │  │   L1 (Heap)   │  │    │  │   L1 (Heap)   │  │   │
│   │  └───────┬───────┘  │    │  └───────┬───────┘  │   │
│   └──────────┼──────────┘    └──────────┼──────────┘   │
└──────────────┼──────────────────────────┼──────────────┘
               │                          │
        Network Reads / Writes (L2)       │
               │                          │
               ▼                          ▼
┌────────────────────────────────────────────────────────┐
│               Redis Cluster (Shared L2)                │
│   ┌───────────────┐  ┌───────────────┐  ┌───────────┐  │
│   │ Master Node 1 │  │ Master Node 2 │  │ Master 3  │  │
│   │ Slots 0-5460  │  │ Slots 5461-.. │  │ ..16383   │  │
│   └───────┬───────┘  └───────────────┘  └───────────┘  │
│           │                                            │
│           ▼ (Invalidation Pub/Sub Channel)             │
│    "cache:invalidate:product:42"                       │
└────────────────────────────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Configuring Redis Cluster in NestJS with `ioredis`

```typescript
// src/common/redis/redis-cluster.module.ts
import { Module, Global, OnApplicationShutdown, Inject } from '@nestjs/common';
import { ConfigModule, ConfigService } from '@nestjs/config';
import { Cluster } from 'ioredis';

export const REDIS_CLUSTER = Symbol('REDIS_CLUSTER');

@Global()
@Module({
  imports: [ConfigModule],
  providers: [
    {
      provide: REDIS_CLUSTER,
      useFactory: (config: ConfigService) => {
        const clusterNodes = [
          { host: config.get<string>('REDIS_NODE_1', 'localhost'), port: 7000 },
          { host: config.get<string>('REDIS_NODE_2', 'localhost'), port: 7001 },
          { host: config.get<string>('REDIS_NODE_3', 'localhost'), port: 7002 },
        ];

        return new Cluster(clusterNodes, {
          redisOptions: {
            password: config.get<string>('REDIS_PASSWORD'),
          },
          scaleReads: 'slave', // Offload read traffic to replica nodes
          enableReadyCheck: true,
          slotsRefreshTimeout: 2000,
        });
      },
      inject: [ConfigService],
    },
  ],
  exports: [REDIS_CLUSTER],
})
export class RedisClusterModule implements OnApplicationShutdown {
  constructor(@Inject(REDIS_CLUSTER) private readonly cluster: Cluster) {}

  async onApplicationShutdown() {
    await this.cluster.quit();
  }
}
```

---

### 4.2 Two-Tier (L1 + L2) Synchronized Cache Service

```typescript
// src/common/cache/tiered-cache.service.ts
import { Injectable, Inject, OnModuleInit } from '@nestjs/common';
import { Cluster, Redis } from 'ioredis';
import { REDIS_CLUSTER } from '../redis/redis-cluster.module';

@Injectable()
export class TieredCacheService implements OnModuleInit {
  // L1 Local In-Memory Cache (RAM)
  private readonly l1 = new Map<string, { value: any; expiry: number }>();
  private subClient!: Redis;

  constructor(@Inject(REDIS_CLUSTER) private readonly l2: Cluster) {}

  async onModuleInit() {
    // Dedicated subscriber connection for invalidation messages
    this.subClient = new Redis({
      host: process.env.REDIS_HOST || 'localhost',
      port: 6379,
    });

    await this.subClient.subscribe('cache:l1:invalidate');
    this.subClient.on('message', (channel, key) => {
      if (channel === 'cache:l1:invalidate') {
        this.l1.delete(key);
      }
    });
  }

  async get<T>(key: string): Promise<T | null> {
    const now = Date.now();

    // 1. Check L1 Memory Cache (< 0.05ms)
    const l1Record = this.l1.get(key);
    if (l1Record && l1Record.expiry > now) {
      return l1Record.value as T;
    }

    // 2. Check L2 Redis Cluster (~1-2ms)
    const l2Record = await this.l2.get(key);
    if (!l2Record) {
      return null;
    }

    const parsed = JSON.parse(l2Record) as T;

    // 3. Populate L1 with short TTL (e.g. 10s) to absorb micro-bursts
    this.l1.set(key, { value: parsed, expiry: now + 10000 });

    return parsed;
  }

  async set(key: string, value: any, ttlSeconds: number): Promise<void> {
    const stringified = JSON.stringify(value);

    // Write to L2 Redis Cluster
    await this.l2.set(key, stringified, 'EX', ttlSeconds);

    // Populate local L1
    this.l1.set(key, { value, expiry: Date.now() + 10000 });

    // Broadcast invalidation to all sibling pods via Pub/Sub
    await this.l2.publish('cache:l1:invalidate', key);
  }

  async del(key: string): Promise<void> {
    await this.l2.del(key);
    this.l1.delete(key);
    await this.l2.publish('cache:l1:invalidate', key);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Redis Cluster Split-Brain
During network partitions, two master nodes may believe they own the same hash slot, causing divergent writes.
- **Handling**: Configure `min-replicas-to-write 1` on Redis master nodes to refuse writes if an active replica is lost.

### 5.2 Multi-Key Operations Across Hash Slots
Commands like `MGET` or transactions (`MULTI/EXEC`) fail with `CROSSSLOT Keys in request don't hash to the same slot` if keys reside in different slots.
- **Handling**: Use Redis **Hash Tags** (`{user:100}:profile`, `{user:100}:orders`) to force related keys into the exact same hash slot.

---

## 6. Performance & Memory Implications

- **Read Scaling**: Configuring `scaleReads: 'slave'` offloads read queries to replica nodes, multiplying read capacity linearly with each added replica.
- **Memory Footprint**: Multi-tier L1 absorbs up to 95% of read volume locally, allowing a modest 3-node Redis cluster to support 100,000+ requests per second.

---

## 7. Anti-Patterns & Common Mistakes

- **Executing Multi-Key Commands Without Hash Tags**: Running `MGET keyA keyB` on Redis Cluster, producing immediate `CROSSSLOT` runtime errors.
- **Omitting Dedicated Connection for Pub/Sub**: Using the standard Redis client for both commands and subscriptions. A Redis client placed in subscriber mode rejects standard commands (`GET`, `SET`).
- **Ignoring Slot Migration Timeouts**: Hardcoding node IPs instead of configuring cluster auto-discovery (`slotsRefreshTimeout`).

---

## 8. Production Checklist & Operational Readiness

- [ ] Redis Cluster nodes distributed across multiple cloud availability zones (AZs).
- [ ] Related multi-key queries use `{hash_tags}`.
- [ ] Dedicated Redis client instance used for Pub/Sub listeners.
- [ ] Read scaling enabled on read-heavy workloads (`scaleReads: 'slave'`).
- [ ] L1 local memory bounded by maximum item count (LRU cache).

---

## 9. Migration & Version History

- **Redis 7.x**: Multi-part AOF and enhanced cluster sharding.
- **ioredis 5.x**: Robust cluster reconnection and failover handling.

---

## 10. Verification & Testing

Testing multi-tier cache synchronization:

```typescript
import { Test } from '@nestjs/testing';
import { TieredCacheService } from './tiered-cache.service';
import { REDIS_CLUSTER } from '../redis/redis-cluster.module';

describe('TieredCacheService', () => {
  let service: TieredCacheService;
  let mockCluster: any;

  beforeEach(async () => {
    mockCluster = {
      get: vi.fn().mockResolvedValue(JSON.stringify({ data: 'ok' })),
      set: vi.fn().mockResolvedValue('OK'),
      publish: vi.fn().mockResolvedValue(1),
    };

    const module = await Test.createTestingModule({
      providers: [
        TieredCacheService,
        { provide: REDIS_CLUSTER, useValue: mockCluster },
      ],
    }).compile();

    service = module.get<TieredCacheService>(TieredCacheService);
  });

  it('populates L1 from L2 on cache hit', async () => {
    const val = await service.get('test_key');
    expect(val).toEqual({ data: 'ok' });
    expect(mockCluster.get).toHaveBeenCalledTimes(1);

    // Second call hit L1 directly (L2 not called again)
    const val2 = await service.get('test_key');
    expect(val2).toEqual({ data: 'ok' });
    expect(mockCluster.get).toHaveBeenCalledTimes(1);
  });
});
```

---

## 11. Comprehensive Examples

### Hash Tag Query Example for Multi-Key Transactions

```typescript
// src/orders/orders-cache.service.ts
import { Injectable, Inject } from '@nestjs/common';
import { Cluster } from 'ioredis';
import { REDIS_CLUSTER } from '../redis/redis-cluster.module';

@Injectable()
export class OrdersCacheService {
  constructor(@Inject(REDIS_CLUSTER) private readonly cluster: Cluster) {}

  async cacheOrderAndCustomer(customerId: string, orderId: string, orderData: any) {
    // Both keys contain {customer:123}, guaranteeing identical hash slots
    const customerKey = `{customer:${customerId}}:profile`;
    const orderKey = `{customer:${customerId}}:order:${orderId}`;

    const pipeline = this.cluster.pipeline();
    pipeline.set(customerKey, 'active', 'EX', 3600);
    pipeline.set(orderKey, JSON.stringify(orderData), 'EX', 3600);

    // Safe to execute across cluster because hash slots match
    await pipeline.exec();
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-caching-fundamentals.md](./01-caching-fundamentals.md) - Caching theory
- [03-redis.md](./03-redis.md) - Single node Redis patterns
- [04-cache-invalidation.md](./04-cache-invalidation.md) - Invalidation techniques
- [Redis Cluster Specification](https://redis.io/docs/reference/cluster-spec/)
