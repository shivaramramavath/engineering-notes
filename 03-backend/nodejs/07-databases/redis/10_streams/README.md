# 10 · Streams

Redis Streams are an **append-only log** with blocking reads, replay, and consumer groups that give you **at-least-once delivery with acknowledgements**. They are the durable counterpart to [Pub/Sub](../09_pub-sub/README.md), and the foundation for event pipelines, job processing and change feeds on Redis.

[04_data-structures/08_streams-overview.md](../04_data-structures/08_streams-overview.md) introduced the commands. This module turns them into production patterns.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Stream Fundamentals](./01_stream-fundamentals.md) | Entries, IDs, ranges, trimming, memory, persistence |
| 02 | [Producers and Consumers](./02_producers-and-consumers.md) | Writing reliably, tailing a stream, checkpoints, SSE with resume |
| 03 | [Consumer Groups and Acks](./03_consumer-groups-and-acks.md) | Shared work, pending entries, reclaiming, dead letters, monitoring |
| 04 | [Stream Patterns](./04_stream-patterns.md) | Work queues, fan-out, outbox, idempotency, partitioning, operations |

## Learning outcomes

After this module you can:

- Write to and read from streams with correct ID handling and bounded retention
- Build a consumer that survives restarts and resumes from a checkpoint
- Run a consumer-group worker with acknowledgements, reclaiming and a dead-letter stream
- Make consumers idempotent, because delivery is at-least-once
- Monitor lag and pending entries, and scale with partitioned streams
- Decide between Streams, Pub/Sub, BullMQ and a full broker like Kafka

## The mental model

```
producers ──XADD──►  [ 1-0 ][ 2-0 ][ 3-0 ][ 4-0 ][ 5-0 ] ...   ◄── append-only log, kept until trimmed
                                    ▲                ▲
                    group "billing" cursor      group "email" cursor     each group sees EVERY entry
                         │                          │
                  ┌──────┴──────┐             ┌─────┴──────┐
              worker-1  worker-2          worker-1   worker-2     within a group, each entry goes to ONE worker
```

- **Entries stay** after being read (until you trim), so new consumers can replay
- A **group** divides entries among its workers. **Different groups** each get everything
- Delivered-but-not-acknowledged entries wait in the group's **pending list** so they can be retried

## Streams vs the alternatives

| Need | Choose |
|------|--------|
| Fire-and-forget fan-out, loss is fine | [Pub/Sub](../09_pub-sub/README.md) |
| Simple queue, losing a job on a crash is acceptable | List |
| Durable events, replay, several independent consumers | **Streams** |
| Shared work with acks and retries, built on Redis | **Streams** (consumer groups) |
| Delays, priorities, rate limits, dashboards, retries out of the box | [BullMQ](../14_queues-and-workers/README.md) |
| Very high volume, long retention, large ecosystem | Kafka or similar |

## Prerequisites

- Completed [09_pub-sub](../09_pub-sub/README.md)
- Comfortable with [blocking connections](../08_nodejs-integration/01_connection-management.md) (`XREAD BLOCK` and `XREADGROUP BLOCK` occupy a connection) and [graceful shutdown](../03_ioredis-basics/06_graceful-shutdown.md)

## Conventions in the examples

```ts
import { Redis } from "ioredis";
const redis = new Redis();            // normal commands
const blocker = redis.duplicate();    // dedicated to blocking reads
const STREAM = "shop:stream:orders";  // build real names with your key builder
```

## Next

Continue to [11_distributed-locks](../11_distributed-locks/README.md).
