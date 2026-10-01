# 09 · Pub/Sub

Redis Pub/Sub is the lightest messaging tool in Redis: publish to a channel, and every connected subscriber receives it **right now**. It is fast, simple, and **forgetful**, and understanding exactly what it forgets is the whole skill.

## Lessons

| # | Lesson | Core idea |
|---|--------|-----------|
| 01 | [Pub/Sub Fundamentals](./01_pub-sub-fundamentals.md) | Channels, patterns, delivery semantics, a robust subscriber wrapper |
| 02 | [Event-Driven Node.js](./02_event-driven-nodejs.md) | Typed event buses, local plus remote events, ordering, failure handling |
| 03 | [Socket.IO with Redis](./03_socketio-with-redis.md) | Scaling real-time apps across instances, rooms, presence |

## Learning outcomes

After this module you can:

- Explain at-most-once delivery and what it means for your design
- Build a subscriber that doesn't leak handlers, swallow errors or duplicate messages
- Design typed event buses that work in-process and across instances
- Choose between Pub/Sub, Streams, queues and plain `EventEmitter`
- Scale Socket.IO horizontally with the Redis adapter, including sticky sessions and presence

## The one-minute mental model

```
publisher ──PUBLISH──►  channel  ──►  subscriber A   (connected now: receives it)
                           │     ──►  subscriber B   (connected now: receives it)
                           └─────X    subscriber C   (was offline: never receives it)
```

- **Fan-out**: every current subscriber gets a copy
- **No storage**: Redis keeps nothing, and a message with no listeners vanishes
- **No acknowledgement**: the publisher never learns whether handling succeeded

## When Pub/Sub fits

| Good fit | Poor fit |
|----------|----------|
| Live notifications where a missed one is harmless | Payments, orders, anything that must not be lost |
| Cache invalidation hints with a TTL backstop | Work distribution (one worker per job) |
| Real-time UI fan-out (chat, presence, typing) | Replaying history or catching up after downtime |
| Config reload signals | Messages that need acknowledgement and retries |
| "Something changed, go look" wake-ups | Large payloads |

For anything durable, use [Streams](../10_streams/README.md) or a [queue](../14_queues-and-workers/README.md).

## Prerequisites

- Completed [08_nodejs-integration](../08_nodejs-integration/README.md), especially [connection management](../08_nodejs-integration/01_connection-management.md) (subscribers need their own connection)
- Comfortable with [error handling](../03_ioredis-basics/05_error-handling.md) and [graceful shutdown](../03_ioredis-basics/06_graceful-shutdown.md)

## Next

Continue to [10_streams](../10_streams/README.md).
