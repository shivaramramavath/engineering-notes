# When Not to Use Redis

Knowing the limits prevents expensive mistakes.

## Poor fits

### 1. Primary store for critical, irreplaceable data

Redis persistence is weaker than a proper database, and async replication can lose recent writes on failover.
**Use instead:** PostgreSQL, MySQL, MongoDB (keep Redis as cache or auxiliary store).

### 2. Large datasets that don't fit affordably in RAM

Memory costs far more than disk. Multi-terabyte data sets get expensive fast.
**Use instead:** a disk-based database, object storage, or a tiered approach.

### 3. Complex queries, joins and reporting

Redis has no SQL, joins or ad-hoc filtering. Secondary indexing via sets and sorted sets is manual and error-prone.
**Use instead:** SQL databases, Elasticsearch/OpenSearch. (Redis has search modules, but evaluate them carefully.)

### 4. Strong multi-key ACID transactions

`MULTI/EXEC` gives isolation but **no rollback** on command errors, and Cluster limits multi-key operations.
**Use instead:** relational databases.

### 5. Long-term event log with replay at scale

Streams live in memory and are trimmed to stay manageable.
**Use instead:** Kafka, Redpanda, Pulsar.

### 6. Large blobs (images, videos, big JSON)

Large values block the single thread, bloat memory and slow replication.
**Use instead:** S3/object storage, storing only the URL or metadata in Redis.

### 7. Guaranteed-delivery messaging

Pub/Sub is fire-and-forget: offline subscribers miss messages.
**Use instead:** Redis Streams with consumer groups, RabbitMQ or Kafka.

## Common anti-patterns

| Anti-pattern                                       | Problem                            | Fix                                  |
| -------------------------------------------------- | ---------------------------------- | ------------------------------------ |
| `KEYS *` in production                             | O(N), blocks all clients           | `SCAN` with a cursor                 |
| No TTL on cache keys                               | Memory grows until eviction or OOM | Always set expiry                    |
| Huge single keys (a set with millions of members)  | Slow ops, blocking deletes         | Split keys, use `UNLINK`             |
| One giant Lua script                               | Blocks the server                  | Keep scripts short                   |
| Storing everything as one JSON string              | Rewrite whole value for one field  | Use hashes                           |
| Treating Redis as a durable queue without acks     | Lost jobs on crash                 | Streams + acks, or BullMQ            |
| New connection per request                         | Connection churn, latency          | Reuse one shared client              |
| Ignoring `maxmemory`                               | Unexpected OOM kills               | Set limit and eviction policy        |
| Distributed lock without expiry or ownership check | Deadlocks, stolen locks            | `SET NX PX` with token + Lua release |

## Quick self-check

Ask yourself before adopting Redis:

1. Can I **rebuild this data** if Redis loses it? (If not, it needs a durable store.)
2. Does it **fit in memory** at a reasonable cost?
3. Is my access pattern **key-based**, not query-based?
4. Do I understand what happens on **failover**?
5. Is the value **small** and the operations **O(1) or O(log N)**?

If most answers are "yes", Redis is likely a good fit.

## Summary

Redis excels at fast, ephemeral, structured, key-centric data. Pair it with a durable database and a message broker where needed, rather than stretching it into roles it wasn't built for.

**Next module:** [01_setup](../01_setup/README.md)
