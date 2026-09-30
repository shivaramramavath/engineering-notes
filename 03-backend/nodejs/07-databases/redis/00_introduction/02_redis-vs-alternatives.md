# Redis vs Alternatives

No tool wins everywhere. This lesson helps you choose deliberately.

## Redis vs Memcached

|                            | Redis                                                   | Memcached                 |
| -------------------------- | ------------------------------------------------------- | ------------------------- |
| Data types                 | Strings, hashes, lists, sets, sorted sets, streams, ... | Strings only              |
| Persistence                | Optional (RDB / AOF)                                    | None                      |
| Replication / HA           | Yes (replicas, Sentinel, Cluster)                       | Client-side sharding only |
| Pub/Sub, Lua, transactions | Yes                                                     | No                        |
| Threading                  | Single-threaded commands, optional I/O threads          | Multi-threaded            |

**Choose Memcached** only for a simple, huge, ephemeral string cache where multi-threading matters. In most cases Redis covers it and more.

## Redis vs SQL databases (PostgreSQL, MySQL)

|              | Redis                           | SQL                                  |
| ------------ | ------------------------------- | ------------------------------------ |
| Storage      | RAM (disk for persistence)      | Disk (RAM for cache)                 |
| Query model  | Key-based commands              | Declarative SQL, joins, aggregations |
| Latency      | Sub-millisecond                 | Milliseconds                         |
| Durability   | Configurable, weaker by default | Strong ACID                          |
| Dataset size | Bounded by memory cost          | Bounded by disk                      |

**Pattern:** SQL is the _source of truth_; Redis sits in front for speed (cache-aside) or handles ephemeral state (sessions, counters, locks).

## Redis vs MongoDB / document stores

- MongoDB: flexible documents, rich queries and indexes, disk-first.
- Redis: key-first access, in-memory, atomic structure operations.
- Use MongoDB to store and query documents; use Redis to cache them or coordinate around them.

## Redis vs Kafka

|            | Redis Streams / Pub/Sub                | Kafka                                 |
| ---------- | -------------------------------------- | ------------------------------------- |
| Scale      | Moderate                               | Very large, partitioned, multi-broker |
| Retention  | Bounded by memory (trim with `MAXLEN`) | Long-term on disk                     |
| Replay     | Streams: yes; Pub/Sub: no              | Yes, first-class                      |
| Operations | Simple                                 | Heavier                               |
| Latency    | Very low                               | Low, tuned for throughput             |

**Choose Redis Streams** for lightweight event flows and job pipelines. **Choose Kafka** for durable event logs, high-volume pipelines and multi-consumer replay at scale.

## Redis vs RabbitMQ

|                      | Redis (BullMQ / Streams)      | RabbitMQ                          |
| -------------------- | ----------------------------- | --------------------------------- |
| Routing              | Simple                        | Rich (exchanges, topics, headers) |
| Protocols            | RESP                          | AMQP, MQTT, STOMP                 |
| Delivery guarantees  | Good with Streams + acks      | Strong, mature ack model          |
| Extra infrastructure | None if you already run Redis | Separate broker                   |

**Choose Redis** when you already have it and need simple job queues. **Choose RabbitMQ** for complex routing or strict messaging semantics.

## Redis vs in-process memory (`Map`, `lru-cache`)

- In-process: fastest, zero network, but per-instance and lost on restart.
- Redis: shared across instances, survives app restarts, one network hop.
- Common combo: a small in-process L1 cache in front of Redis as L2.

## Redis and its forks

Redis licensing changed in 2024 (moving away from BSD), which led to community forks such as **Valkey** (Linux Foundation). Both speak the same protocol, and ioredis works with them. Check the current license terms before adopting a specific distribution.

## Decision cheat sheet

| Need                                       | Best fit       |
| ------------------------------------------ | -------------- |
| Cache with TTL and shared state            | Redis          |
| Complex queries, joins, transactions       | SQL            |
| Flexible documents                         | MongoDB        |
| Durable, replayable, high-volume event log | Kafka          |
| Complex message routing                    | RabbitMQ       |
| Simple background jobs                     | Redis + BullMQ |
| Rate limits, locks, leaderboards           | Redis          |

**Next:** [Redis Architecture](./03_redis-architecture.md)
