---
title: "Performance and Caching Best Practices in NestJS"
description: "Maximizing throughput: multi-layer caching, event loop optimization, and streaming large payloads in NestJS"
tags: [nestjs, best-practices, performance, caching, redis, streaming]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Performance and Caching Best Practices in NestJS

## Core Principles
1. **Cache Early, Invalidate Reliably**: Use Redis distributed caching with key versioning or TTL.
2. **Monitor Event Loop Lag**: Ensure lag remains $< 20\text{ms}$.
3. **Stream Large Datasets**: Use `StreamableFile` or Node streams for large file exports.

---

## Cross-References
- [01-caching-fundamentals.md](../../05-advanced/01-caching/01-caching-fundamentals.md) - Caching
