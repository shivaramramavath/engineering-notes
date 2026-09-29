# 00 · Introduction

Before writing any `ioredis` code, understand **what Redis is, why it exists, how it works internally, and when it is the wrong tool**. This module gives you the mental model that the rest of the course builds on.

## Lessons

| #   | Lesson                                                 | You will learn                                                        |
| --- | ------------------------------------------------------ | --------------------------------------------------------------------- |
| 01  | [Why Redis](./01_why-redis.md)                         | The problems Redis solves and its most common use cases               |
| 02  | [Redis vs Alternatives](./02_redis-vs-alternatives.md) | How it compares to Memcached, SQL/NoSQL databases, Kafka and RabbitMQ |
| 03  | [Redis Architecture](./03_redis-architecture.md)       | Event loop, RESP protocol, memory model, persistence, scaling         |
| 04  | [When Not to Use Redis](./04_when-not-to-use-redis.md) | Anti-patterns and better-fit tools                                    |

## Learning outcomes

After this module you can:

- Explain why Redis is fast and what "single-threaded" really means
- Pick Redis (or something else) for a given problem with a clear justification
- Describe how a command travels from your Node.js app to Redis and back
- Recognize the limits of an in-memory data store

## Prerequisites

- Basic Node.js (async/await, modules)
- Familiarity with any database (SQL or NoSQL)

## Next

Continue to [01_setup](../01_setup/README.md).
