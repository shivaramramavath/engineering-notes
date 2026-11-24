---
title: "Database Performance and Query Optimization in NestJS"
description: "Connection pool sizing, N+1 query elimination, DataLoader, cursor pagination, and indexing strategies with TypeORM, Prisma, and Kysely"
tags: [nestjs, performance, database, connection-pooling, n-plus-one, dataloader, indexing]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Database Performance and Query Optimization in NestJS

## Conceptual Overview
The database tier is the single most common throughput bottleneck in backend systems. Inefficient queries, unindexed foreign keys, connection pool starvation, and the notorious N+1 query problem can bring an enterprise API to a halt under load.

Optimizing database performance in NestJS requires:
- **Connection Pool Tuning**: Sizing connection pools to match database CPU cores and network latency.
- **N+1 Elimination**: Using eager joins or request-scoped `DataLoader` batching.
- **Efficient Pagination**: Replacing costly `OFFSET / LIMIT` scans with Keyset/Cursor-based pagination.
- **Read Replicas & Secondary Caches**: Offloading analytical and read queries to read replicas and Redis.

---

## Core Mechanisms & Architecture

### 1. Connection Pool Sizing Equation
A widespread misconception is that larger connection pools equal higher throughput. In reality, oversized pools degrade database performance due to CPU context switching and disk I/O thrashing.

The PostgreSQL recommended formula:
$$	ext{Pool Size} = (2 	imes 	ext{Core Count}) + 	ext{Disk Spindle Effective Count}$$

For a 4-core database server, a pool size between 10 and 20 connections per NestJS instance is optimal.

### 2. Cursor Pagination vs Offset Pagination
```sql
-- SLOW: Scans and discards 100,000 rows (O(N) cost)
SELECT * FROM orders ORDER BY created_at DESC LIMIT 20 OFFSET 100000;

-- FAST: Indexed B-Tree Seek (O(log N) cost)
SELECT * FROM orders WHERE created_at < '2026-03-01T12:00:00Z' ORDER BY created_at DESC LIMIT 20;
```

---

## Key Takeaways & Cheat Sheet

- Never use unbounded `OFFSET` pagination for large datasets; use Keyset/Cursor pagination.
- Eliminate N+1 queries using joins or `DataLoader` batching.
- Tune database connection pools to match database CPU core capacity.
- Cache high-frequency, low-mutability queries in Redis.

---

## Cross-References
- [02-dataloader-and-n-plus-one.md](../../05-advanced/05-graphql/02-dataloader-and-n-plus-one.md) - DataLoader mechanics
- [01-caching-fundamentals.md](../../05-advanced/01-caching/01-caching-fundamentals.md) - Cache strategies
