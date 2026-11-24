# Amazon ElastiCache: Redis and Memcached In-Memory Caching Architecture

> **Module**: `03-storage-and-databases`  
> **Topic**: In-Memory Caching, Sub-Millisecond Datastores, and Cache Topologies  
> **Target Audience**: Backend Engineers, Systems Architects, Performance Specialists  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Relational databases (RDS/Aurora) and NoSQL stores (DynamoDB) store data on solid-state drives (SSDs). While modern SSDs deliver single-digit millisecond response times (1–10 ms), high-concurrency internet applications—such as real-time gaming leaderboards, session management stores, financial order routing, and e-commerce shopping carts—require sub-millisecond (microsecond) latency and the ability to absorb millions of read queries per second without overloading backend disks.

**Amazon ElastiCache** is a fully managed, in-memory caching service providing ultra-fast, sub-millisecond data retrieval.

By caching frequently read database query results, user authentication sessions, and computational outputs in RAM, ElastiCache delivers two critical architectural benefits:
1. **Sub-Millisecond Response Times**: Serves reads and writes directly from volatile RAM memory (typically 200–800 microseconds), unlocking radical application responsiveness.
2. **Database Offloading**: Absorbs up to 90% of repeated read traffic, drastically reducing the required compute size and licensing expenses of underlying primary relational databases.

---

## 2. Core Concepts

### 2.1 Redis vs. Memcached
ElastiCache supports two open-source in-memory engines:
- **Redis (and Valkey)**:
  - Advanced in-memory data structures: Strings, Hashes, Lists, Sets, Sorted Sets (`ZSET`), Bitmaps, HyperLogLogs, Geospatial indexes, and Streams.
  - Multi-AZ replication with automated failover.
  - Persistence options (RDB point-in-time snapshots and AOF logs).
  - Native Pub/Sub messaging and Lua scripting.
- **Memcached**:
  - Simple, lightweight, pure key-value cache.
  - Multithreaded architecture (scales vertically with multiple CPU cores per node).
  - No persistence and no replication (pure volatile memory).
  - Partitioned across multiple nodes via client-side consistent hashing.

### 2.2 Redis Cluster Topologies
1. **Cluster Mode Disabled (Primary-Replica)**:
   - Contains a single shard with **1 Primary Writer** and up to **5 Read Replicas**.
   - Read scaling is horizontal (via replicas), but write throughput is bounded by the single primary node's memory and CPU.
   - Total dataset must fit into the RAM of a single node (up to ~500 GB).
2. **Cluster Mode Enabled (Sharded Architecture)**:
   - Distributes keys across up to **500 shards** using 16,384 internal hash slots.
   - Each shard contains 1 primary writer and up to 5 read replicas.
   - Scales horizontally for **both reads and writes**, supporting datasets up to **310 TB in RAM** and tens of millions of IOPS.
3. **ElastiCache Serverless**:
   - Eliminates node sizing and cluster topology planning.
   - Automatically scales compute and memory in seconds to meet traffic spikes, billing strictly for data stored (GB-hours) and ElastiCache Processing Units (ECPUs) consumed.

### 2.3 Caching Design Patterns
- **Cache-Aside (Lazy Loading)**:
  - Application queries the cache first.
  - **Cache Hit**: Returns data immediately.
  - **Cache Miss**: Application queries the primary database, stores the result in cache with a TTL, and returns data to the client.
  - Data in cache is strictly read-on-demand; avoids filling cache with unread data.
- **Write-Through**:
  - Application updates the database and cache simultaneously in a single transaction.
  - Cache is always fresh, but write latency increases and unused data may consume cache memory.
- **Write-Behind (Write-Back)**:
  - Application writes directly to the in-memory cache, which acknowledges immediately. A background worker batches and flushes writes asynchronously to the database. Delivers maximum write throughput but risks data loss during cache crashes.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        ElastiCache Redis Cluster Architecture                     |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Client Application (Redis Cluster-Aware Driver) ]                              |
|           |                                                                       |
|           v Calculates Hash Slot: CRC16(Key) mod 16384                            |
|  +-----------------------------------------------------------------------------+  |
|  | Sharded Amazon ElastiCache Redis Cluster (Cluster Mode Enabled)             |  |
|  |                                                                             |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  | Shard 1 (Hash Slots: 0 - 5460)                                        |  |  |
|  |  | - Primary Node (AZ 1a) [RW] ---> Replicas (AZ 1b / 1c) [Read Only]     |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |                                                                             |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  | Shard 2 (Hash Slots: 5461 - 10922)                                    |  |  |
|  |  | - Primary Node (AZ 1b) [RW] ---> Replicas (AZ 1a / 1c) [Read Only]     |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |                                                                             |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  | Shard 3 (Hash Slots: 10923 - 16383)                                   |  |  |
|  |  | - Primary Node (AZ 1c) [RW] ---> Replicas (AZ 1a / 1b) [Read Only]     |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  - Sub-millisecond latency: Memory-backed execution (200-800 microseconds)       |
|  - Automatic failover: Multi-AZ promotion in under 15 seconds                     |
+-----------------------------------------------------------------------------------+
```

### Cache-Aside (Lazy Loading) Logic Flow
```
[Client Request] ---> Checks Cache (GET key)
                            |
             +--------------+--------------+
             |                             |
      (Cache Hit)                   (Cache Miss)
             |                             |
             v                             v
      Return Data              Query Primary Database (RDS / DynamoDB)
                                           |
                                           v
                               Populate Cache (SETEX key 300 data)
                                           |
                                           v
                                      Return Data
```

---

## 4. Syntax & Usage

### 4.1 Creating an ElastiCache Serverless Redis Cache via AWS CLI
```bash
aws elasticache create-serverless-cache \
  --serverless-cache-name "prod-api-cache" \
  --engine "redis" \
  --major-engine-version "7" \
  --description "Serverless Redis cache for production API" \
  --subnet-ids "subnet-01a" "subnet-01b" "subnet-01c" \
  --security-group-ids "sg-0123456789abcdef0" \
  --daily-snapshot-time "03:00" \
  --snapshot-retention-limit 7
```

### 4.2 Connecting and Executing Operations via `redis-cli`
Connect securely to ElastiCache over an in-transit encrypted TLS connection:
```bash
# Connect using redis-cli with TLS enabled and AUTH token
redis-cli -h prod-api-cache.abc123.serverless.use1.cache.amazonaws.com \
  -p 6379 \
  --tls \
  -a "MyAuthToken12345"

# Basic String Caching with TTL (300 seconds)
SET user:1001:session "eyJhbGciOi..." EX 300
GET user:1001:session

# High-Performance Leaderboard using Sorted Set (ZSET)
ZADD leaderboard:weekly 1500 "Player_Alpha"
ZADD leaderboard:weekly 2400 "Player_Bravo"
ZADD leaderboard:weekly 1850 "Player_Charlie"

# Query Top 2 Players
ZREVRANGE leaderboard:weekly 0 1 WITHSCORES
# Output:
# 1) "Player_Bravo"
# 2) "2400"
# 3) "Player_Charlie"
# 4) "1850"
```

---

## 5. Practical Examples

### Example 1: Robust Python Cache-Aside Pattern with Redis
Implementation of the Cache-Aside pattern connecting to ElastiCache with automatic fallback.

```python
import redis
import json
import psycopg2

# Connect to ElastiCache cluster with TLS
cache = redis.Redis(
    host='prod-api-cache.abc123.serverless.use1.cache.amazonaws.com',
    port=6379,
    ssl=True,
    decode_responses=True,
    socket_timeout=2
)

def get_product_details(product_id):
    cache_key = f"product:{product_id}"
    
    # 1. Check in-memory cache
    try:
        cached_data = cache.get(cache_key)
        if cached_data:
            return json.loads(cached_data), "CACHE_HIT"
    except redis.RedisError as e:
        print(f"Redis cache read error: {e}")
        
    # 2. Cache Miss: Query primary RDS PostgreSQL database
    db_conn = psycopg2.connect("host=mydb.rds.amazonaws.com dbname=shop user=app")
    cursor = db_conn.cursor()
    cursor.execute("SELECT id, name, price, stock FROM products WHERE id = %s", (product_id,))
    row = cursor.fetchone()
    db_conn.close()
    
    if not row:
        return None, "NOT_FOUND"
        
    product = {
        "id": row[0],
        "name": row[1],
        "price": float(row[2]),
        "stock": row[3]
    }
    
    # 3. Populate cache with 10-minute TTL (600 seconds)
    try:
        cache.setex(cache_key, 600, json.dumps(product))
    except redis.RedisError as e:
        print(f"Redis cache write error: {e}")
        
    return product, "CACHE_MISS"
```

### Example 2: Declarative Terraform Definition for Redis Cluster (Cluster Mode Enabled)
Provision a 3-shard Redis cluster with Multi-AZ automated failover and encryption.

```hcl
resource "aws_elasticache_subnet_group" "cache_subnets" {
  name       = "redis-cluster-subnet-group"
  subnet_ids = var.private_subnet_ids
}

resource "aws_elasticache_replication_group" "redis_cluster" {
  replication_group_id = "prod-redis-clustered"
  description          = "High availability clustered Redis cache"
  node_type            = "cache.r7g.large"
  port                 = 6379
  parameter_group_name = "default.redis7.cluster.on"
  subnet_group_name    = aws_elasticache_subnet_group.cache_subnets.name
  security_group_ids   = [var.cache_security_group_id]

  # Cluster Mode Enabled: 3 shards, 1 replica per shard (Total 6 nodes)
  num_node_groups         = 3
  replicas_per_node_group = 1

  automatic_failover_enabled = true
  multi_az_enabled           = true
  transit_encryption_enabled = true
  at_rest_encryption_enabled = true
  kms_key_id                 = var.kms_key_arn

  auto_minor_version_upgrade = true
  maintenance_window         = "sun:05:00-sun:06:00"
  snapshot_retention_limit   = 7
  snapshot_window            = "03:00-04:00"
}
```

### Example 3: Rate Limiting via Redis Sliding Window Counter
Protect downstream APIs from DDoS abuse using atomic Redis transaction pipelines.

```python
import time
import redis

r = redis.Redis(host='localhost', port=6379)

def is_rate_limited(user_id, limit=100, window_seconds=60):
    current_time = time.time()
    key = f"rate_limit:{user_id}"
    clear_before = current_time - window_seconds
    
    pipe = r.pipeline()
    # 1. Remove elements outside the current sliding window
    pipe.zremrangebyscore(key, 0, clear_before)
    # 2. Add current request timestamp
    pipe.zadd(key, {str(current_time): current_time})
    # 3. Count requests in current window
    pipe.zcard(key)
    # 4. Set TTL on the key to clean up idle users
    pipe.expire(key, window_seconds)
    
    _, _, request_count, _ = pipe.execute()
    
    return request_count > limit
```

### Example 4: Memcached Multithreaded Key-Value Caching
Python script demonstrating Memcached client usage for simple string object caching.

```python
from pymemcache.client.base import Client
import json

# Connect to ElastiCache Memcached Configuration Endpoint
memcached_client = Client(
    ('prod-memcached.abc123.cfg.use1.cache.amazonaws.com', 11211),
    connect_timeout=1,
    timeout=1
)

def cache_session(session_id, user_data):
    # Store session serialized as JSON for 3600 seconds
    memcached_client.set(session_id, json.dumps(user_data), expire=3600)

def get_session(session_id):
    result = memcached_client.get(session_id)
    return json.loads(result) if result else None
```

### Example 5: Handling Redis Evictions (`maxmemory-policy`)
Configure eviction policies to automatically purge old items when memory is exhausted:

```bash
# In the ElastiCache Parameter Group:
# Set maxmemory-policy to 'volatile-lru' (removes least recently used keys with an expire set)
# or 'allkeys-lru' (removes least recently used keys regardless of TTL)
aws elasticache modify-cache-parameter-group \
  --cache-parameter-group-name "custom-redis-params" \
  --parameter-name-values "ParameterName=maxmemory-policy,ParameterValue=allkeys-lru"
```

---

## 6. Best Practices

1. **Always Set TTLs on Every Cached Key**: Never write to cache without an expiration timer (`EX` or `setex`). Keys without TTLs remain in memory indefinitely, consuming RAM and eventually triggering aggressive eviction loops or out-of-memory errors.
2. **Standardize on Graviton (`cache.r7g`, `cache.m7g`)**: AWS Graviton instances provide up to 25% higher throughput and 20% lower cost per gigabyte of memory compared to x86 alternatives.
3. **Use Cluster Mode Enabled for Scalability**: For applications anticipating growth, choose **Cluster Mode Enabled** from day one. Sharding keys across multiple nodes allows you to add or remove shards online with zero downtime as data expands.
4. **Enforce In-Transit and At-Rest Encryption**: Always enable `transit_encryption_enabled = true` (TLS) and `at_rest_encryption_enabled = true` (KMS). Use Redis AUTH tokens or IAM authentication to control cluster access.
5. **Tune `reserved-memory-percent` on Redis Nodes**: Reserve at least **25% of node memory** (`reserved-memory-percent = 25`) in the parameter group. During snapshotting, failover synchronization, or background rewrites, Redis uses copy-on-write memory; if all memory is consumed by data, Redis crashes with out-of-memory errors during backups.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Omitting TTLs on Cache Keys** | Memory fills completely, forcing Redis to evict active keys or reject new writes with OOM errors. | Always define an explicit expiration (`EX` seconds) on every write operation. |
| **Connecting Without Cluster-Aware Drivers in Cluster Mode** | Client driver sends queries to the wrong node, receiving `MOVED` or `ASK` redirection errors. | Use a Redis cluster-aware client library (e.g., `redis-py` with `RedisCluster`) that supports hash slot routing. |
| **Using ElastiCache as a Permanent Database** | Node failures or eviction policies cause irreversible data loss; ElastiCache is not an ACID datastore. | Treat ElastiCache as an ephemeral caching acceleration layer backed by a durable store (DynamoDB or RDS). |
| **Setting `reserved-memory-percent` to 0%** | Backups and replication synchronization crash nodes due to copy-on-write memory exhaustion. | Set `reserved-memory-percent` to **25%** to guarantee overhead for Redis background operations. |
| **Deploying Single-AZ Redis in Production** | Host replacement or AZ disruption takes the entire cache offline, causing a thundering herd on the database. | Enable **Multi-AZ with Automatic Failover** across at least 2 Availability Zones. |
| **Cache Stampede (Thundering Herd)** | When a hot key expires, thousands of concurrent threads miss simultaneously and overwhelm the database. | Implement mutual exclusion locks (Mutex) or probabilistic early expiration (XFetch algorithm). |

---

## 8. Performance, Security & Cost Notes

### Latency Profiles
- **Memory Access**: 200–800 microseconds average response time.
- **Failover Speeds**: Multi-AZ Redis failover promotes a replica and re-points DNS in **under 15 seconds**.

### Security Hardening
- **VPC Isolation**: ElastiCache nodes do not possess public IP addresses and cannot be accessed over the public internet. Access is strictly confined to private VPC subnets.
- **IAM Authentication**: Modern ElastiCache supports IAM authentication for Redis 7+, allowing EC2/ECS workloads to authenticate using IAM roles rather than static plaintext AUTH passwords.

### Cost Dynamics
- **Dedicated Nodes**: Billed per node-hour based on instance class (e.g., `cache.r7g.large`).
- **ElastiCache Serverless**:
  - Data Storage: $0.125 / GB-hour.
  - Compute: $0.0034 per million ElastiCache Processing Units (ECPUs). Minimum charge of 1 GB storage.

---

## 9. Comparison Table

| Dimension | Redis (ElastiCache) | Memcached (ElastiCache) | Amazon MemoryDB |
|:---|:---|:---|:---|
| **Data Structures** | Complex (Strings, Sets, Hashes, ZSET)| Simple Key-Value strings only | Complex (Redis-compatible) |
| **Thread Architecture** | Single-threaded core (with IO threads)| Multithreaded (scales with vCPUs) | Distributed Multi-AZ Paxos Log |
| **Durability & Persistence**| Optional snapshots (RDB/AOF) | None (Pure volatile RAM) | **Primary Datastore** (Zero loss) |
| **Replication & Failover** | Multi-AZ Automated Failover (<15s)| None (Client-side node hashing) | Multi-AZ Distributed Quorum |
| **Primary Use Case** | Advanced caching, session, queues | Simple, massive web object cache | Transactional primary database |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the fundamental architectural differences between Redis and Memcached in Amazon ElastiCache?</strong></summary>

**Answer:**
- **Redis**: A feature-rich data structure server. Supports complex types (Hashes, Lists, Sets, Sorted Sets, Bitmaps). Features single-threaded core execution (guaranteeing atomic operations), Multi-AZ replication with automated failover, optional persistence via RDB/AOF, Pub/Sub messaging, and Lua scripting.
- **Memcached**: A simple, high-throughput, multithreaded memory caching system. It handles pure key-value pairs without advanced data structures. It does not support replication or disk persistence; if a node crashes, all data on that node is lost. It scales vertically across multiple CPU cores on a single instance and distributes data horizontally across nodes using client-side consistent hashing.
</details>

<details>
<summary><strong>Q2: How does Redis Cluster Mode Enabled partition keys across multiple nodes?</strong></summary>

**Answer:**
In Cluster Mode Enabled, the entire keyspace is divided into **16,384 logical hash slots**:
1. When a client writes a key (e.g., `user:1001:profile`), the client driver calculates the hash slot using the CRC16 algorithm:
   $$\text{Hash Slot} = \text{CRC16}(\text{key}) \pmod{16384}$$
2. The 16,384 slots are evenly distributed across the cluster's primary shards (e.g., in a 3-shard cluster, Shard 1 owns slots 0–5460, Shard 2 owns 5461–10922, and Shard 3 owns 10923–16383).
3. The cluster-aware client routes the query directly to the primary node owning that specific slot. If a key is sent to the wrong node, the node returns a `-MOVED` redirection response instructing the client to redirect.
</details>

<details>
<summary><strong>Q3: What is a Cache Stampede (Thundering Herd) and how can it be prevented?</strong></summary>

**Answer:**
A Cache Stampede occurs when a heavily queried, highly popular cached key expires:
- Hundreds or thousands of concurrent application threads query the cache simultaneously, encounter a **Cache Miss**, and simultaneously execute the expensive backend SQL query against the primary database.
- The sudden flood of identical heavy queries can overwhelm database CPU, exhaust connection pools, and take the database offline.

**Prevention Strategies**:
1. **Mutex Locking**: The first thread to encounter a miss acquires a short-lived distributed lock (via `SET NX`) in Redis, executes the database query, and updates the cache. Other threads wait or serve stale data until the lock releases.
2. **Probabilistic Early Expiration (XFetch Algorithm)**: Applications calculate an optimal refresh window and asynchronously refresh the cache key *before* it physically expires.
3. **Background Prefetch Worker**: A scheduled background worker computes and refreshes hot keys continuously before expiration.
</details>

<details>
<summary><strong>Q4: Why is setting `reserved-memory-percent` critical for ElastiCache Redis clusters?</strong></summary>

**Answer:**
During background operations—such as taking RDB snapshots, executing failover replication synchronization, or performing node maintenance—Redis forks its main process using Linux `copy-on-write` (COW) memory mechanics.
If the instance is running at 100% memory utilization and heavy writes arrive, the operating system must allocate additional memory to track modified pages. If no spare RAM exists, the Linux kernel throws out-of-memory (OOM) errors and terminates the Redis process. Setting **`reserved-memory-percent = 25`** reserves 25% of the node's physical RAM exclusively for copy-on-write overhead, ensuring cluster stability during backups and failovers.
</details>

<details>
<summary><strong>Q5: What is the difference between Cache-Aside and Write-Through caching?</strong></summary>

**Answer:**
- **Cache-Aside (Lazy Loading)**: The application code manages the relationship. It queries the cache first; on a miss, it queries the database and writes the result to cache with a TTL. Data is loaded into the cache only when requested, saving memory for unread data. However, the first request experiences miss latency, and data can become stale if updated in the database directly.
- **Write-Through**: The application writes to the database and the cache simultaneously. The cache is always fresh and synchronized with the database. However, write operations incur higher latency (two network hops), and infrequently read data pollutes valuable in-memory cache space.
</details>

<details>
<summary><strong>Q6: How does ElastiCache Serverless differ from provisioned ElastiCache clusters?</strong></summary>

**Answer:**
- **Provisioned ElastiCache**: Requires pre-selecting instance families (`cache.r7g.large`), planning node counts, configuring shards, and tuning parameter groups. Over-provisioning incurs idle costs; under-provisioning causes out-of-memory evictions.
- **ElastiCache Serverless**: Fully managed, auto-scaling abstraction. It monitors memory and CPU utilization in real time and scales capacity up or down in seconds. You are billed strictly for the memory consumed (GB-hours) and the compute processing units (ECPUs) executed, with zero infrastructure management.
</details>

<details>
<summary><strong>Q7: What is Amazon MemoryDB for Redis and how does it differ from ElastiCache?</strong></summary>

**Answer:**
- **ElastiCache for Redis**: Primarily an **in-memory cache**. While it supports optional snapshots, data can be lost if a catastrophic multi-node hardware crash occurs before logs replicate. It is not designed to be the authoritative system of record.
- **Amazon MemoryDB for Redis**: A **primary, durable transactional database**. It uses a multi-AZ distributed transactional transaction log (similar to Amazon Aurora). When a write occurs, it is committed durably to the transaction log before acknowledging success to the client. It guarantees **zero data loss** across AZ failures while maintaining microsecond read latencies, functioning as an authoritative primary database.
</details>

<details>
<summary><strong>Q8: How can an application guarantee that multiple keys reside on the same Redis shard in Cluster Mode?</strong></summary>

**Answer:**
In Cluster Mode Enabled, keys are hashed across 16,384 slots. If an application must execute a multi-key operation (e.g., `MGET`, transactions via `MULTI/EXEC`, or Lua scripts) across `user:1001:profile` and `user:1001:orders`, Redis throws a `CROSSSLOT Keys in request don't hash to the same slot` error if they reside on different shards.

To guarantee keys map to the exact same shard, use **Redis Hash Tags**:
- Enclose the common substring within curly braces `{}`:
  - `{user:1001}:profile`
  - `{user:1001}:orders`
- Redis hashes *only* the string inside the curly braces (`user:1001`), guaranteeing that both keys hash to the identical slot and physical shard.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an ElastiCache Serverless Redis Instance via CLI</strong></summary>

**Objective**: Provision a serverless Redis cache.

**Command**:
```bash
aws elasticache create-serverless-cache \
  --serverless-cache-name "lab-serverless-cache" \
  --engine "redis" \
  --subnet-ids "subnet-01a" "subnet-01b" \
  --security-group-ids "sg-0123456789abcdef0"
```
</details>

<details>
<summary><strong>Exercise 2: Test In-Transit Encrypted Redis Connection</strong></summary>

**Objective**: Connect via CLI using TLS and authenticate.

**Command**:
```bash
redis-cli -h lab-serverless-cache.abc123.serverless.use1.cache.amazonaws.com -p 6379 --tls
```
</details>

<details>
<summary><strong>Exercise 3: Implement Atomic Counter with TTL</strong></summary>

**Objective**: Create an expiring rate limiter key.

**Commands**:
```bash
INCR rate:user:991
EXPIRE rate:user:991 60
TTL rate:user:991
```
</details>

<details>
<summary><strong>Exercise 4: Test Sorted Set Leaderboard Operations</strong></summary>

**Objective**: Practice ranking calculations.

**Commands**:
```bash
ZADD game:scores 500 "UserA" 850 "UserB" 1200 "UserC"
ZRANK game:scores "UserA"
ZREVRANK game:scores "UserC"
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Cluster Topology and Slots</strong></summary>

**Objective**: Query shard mappings on a clustered Redis instance.

**Command**:
```bash
CLUSTER SLOTS
```
</details>

<details>
<summary><strong>Exercise 6: Test Redis Hash Tag Routing</strong></summary>

**Objective**: Verify that hash tags prevent cross-slot transaction errors.

**Commands**:
```bash
# Verify both keys hash to identical slot
CLUSTER KEYSLOT "{tenant1}:config"
CLUSTER KEYSLOT "{tenant1}:users"
```
*Observe that both commands return the exact same hash slot integer.*
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                             ElastiCache Architecture Cheat Sheet                      |
+---------------------------------------------------------------------------------------+
|  Dimension             | Redis                             | Memcached                |
|------------------------+-----------------------------------+--------------------------|
|  Latency               | Sub-millisecond (200-800 μs)      | Sub-millisecond          |
|  Data Structures       | Hashes, Lists, Sets, ZSET, Strings| Simple Strings only      |
|  High Availability     | Multi-AZ Auto-Failover (<15s)     | None (Volatile cache)    |
|  Thread Architecture   | Single-threaded core              | Multithreaded            |
|  Scaling               | Up to 500 shards (Cluster Mode)   | Add nodes (Consistent H) |
|  Serverless Option     | ElastiCache Serverless            | ElastiCache Serverless   |
|  Primary Datastore     | Use Amazon MemoryDB for Redis     | Never                    |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon ElastiCache User Guide](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/WhatIs.html)
- [Amazon ElastiCache Serverless Announcement](https://aws.amazon.com/blogs/aws/amazon-elasticache-serverless-for-redis-and-memcached/)
- [Redis Clustering and Hash Slot Architecture](https://redis.io/docs/reference/cluster-spec/)
- [Managing Memory in Amazon ElastiCache for Redis](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/ParameterGroups.Redis.html)
- [Caching Strategies and Best Practices on AWS](https://aws.amazon.com/caching/)
