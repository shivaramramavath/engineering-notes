# 01 · Setup

Get a working Redis server, learn the tools to inspect it, and connect from Node.js with ioredis. By the end you will have a project you can reuse for every later module.

## Lessons

| #   | Lesson                                                 | You will learn                                                                       |
| --- | ------------------------------------------------------ | ------------------------------------------------------------------------------------ |
| 01  | [Redis Installation](./01_redis-installation.md)       | Run Redis locally (macOS, Linux, Windows/WSL) or with Docker                         |
| 02  | [Redis CLI and Insight](./02_redis-cli-and-insight.md) | Inspect and debug data with `redis-cli` and the Redis Insight GUI                    |
| 03  | [ioredis Setup](./03_ioredis-setup.md)                 | Install ioredis, configure TypeScript, structure your project, verify the connection |

## Learning outcomes

After this module you can:

- Start and stop a local Redis instance in under a minute
- Run basic commands from the terminal and browse keys visually
- Connect a Node.js (JavaScript or TypeScript) app to Redis
- Use a project layout and environment config that scales to later modules

## Prerequisites

- Node.js 18 or newer
- Docker (recommended, but not required)

## Recommended path

Use **Docker** for the server, **redis-cli** for quick checks, **Redis Insight** for browsing, and **ioredis** in your code.

## Next

Continue to [02_redis-fundamentals](../02_redis-fundamentals/README.md).
