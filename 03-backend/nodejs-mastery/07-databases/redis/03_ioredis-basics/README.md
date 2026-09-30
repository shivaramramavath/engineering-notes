# 03 · ioredis Basics

Everything you need to use ioredis confidently: how connections behave, how to configure them, how commands map to methods, how results are returned, how to handle failures, and how to shut down cleanly.

## Lessons

| # | Lesson | You will learn |
|---|--------|----------------|
| 01 | [Connection](./01_connection.md) | Lifecycle, events, status, lazy connect, reconnection, multiple connections |
| 02 | [Configuration](./02_configuration.md) | Important options, retry strategy, timeouts, TLS, production presets |
| 03 | [Commands and Options](./03_commands-and-options.md) | Command methods, argument styles, option flags, return shapes |
| 04 | [Promises, Callbacks and Buffers](./04_promises-callbacks-buffers.md) | Async styles, binary data, type conversion |
| 05 | [Error Handling](./05_error-handling.md) | Error types, retries, fallbacks, pipeline errors |
| 06 | [Graceful Shutdown](./06_graceful-shutdown.md) | `quit` vs `disconnect`, signals, servers, workers |

## Learning outcomes

After this module you can:

- Explain what happens between `new Redis()` and the first command
- Configure retries, timeouts and offline behavior deliberately
- Read and write any data type with the right argument and return handling
- Handle Redis failures without crashing or hanging your app
- Stop a Node.js process without losing in-flight commands

## Prerequisites

- Completed [02_redis-fundamentals](../02_redis-fundamentals/README.md)
- A running Redis and the shared client from [01_setup](../01_setup/03_ioredis-setup.md)

## Next

Continue to [04_data-structures](../04_data-structures/README.md).
