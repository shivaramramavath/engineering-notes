# Node.js Database Selection Guide: Relational vs. Document vs. Key-Value

> Architectural decision framework for selecting PostgreSQL, MongoDB, Redis, SQLite, DynamoDB, or Elasticsearch for Node.js backend services.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Covered Databases** | PostgreSQL, MongoDB, Redis, SQLite, DynamoDB, OpenSearch |
| **Level** | Staff Systems Architecture |

---

## 1. Database Selection Decision Matrix

| Database | Primary Paradigm | Best Use Cases | When NOT to Use |
| :--- | :--- | :--- | :--- |
| **PostgreSQL** | Relational + JSONB | Core transactional business data, auth, billing, complex joins. | Ultra-high ingestion rates (>100k events/sec) without sharding. |
| **MongoDB** | Document (BSON) | Dynamic polymorphic schemas, content catalogs, CMS, rapid prototyping.| Highly normalized relational data with deep foreign keys. |
| **Redis** | In-Memory Key-Value | Caching, session stores, rate limiters, BullMQ background queues. | Primary long-term relational persistence without snapshots. |
| **SQLite** | In-Process Relational | CLI tools, embedded devices, local development, read-heavy low write.| Distributed multi-pod Kubernetes clusters with concurrent writes. |
| **DynamoDB** | Distributed Key-Value | Serverless microservices, predictable latency at any scale. | Ad-hoc analytics, full-table scans, dynamic multi-column sorting. |
| **Elasticsearch**| Distributed Search Index| Free-text search, typo tolerance, faceted filtering, log analytics. | Primary system of record for financial transactions. |

---

## 2. Polyglot Persistence Architecture in Node.js

In modern enterprise architectures, services combine multiple databases according to access patterns:

```
[ Client Request ] ──> [ Node.js Express/Fastify API ]
                             │
       ┌─────────────────────┼─────────────────────┐
       ▼                     ▼                     ▼
[ PostgreSQL ]         [ Redis Cluster ]     [ OpenSearch / ES ]
(Transactions & DAL)   (Session & Queues)    (Full-Text Search)
```
- **PostgreSQL**: Authoritative source of truth for accounts, orders, and payment records.
- **Redis**: Low-latency cache layer, rate-limiting tokens, and BullMQ queue backend.
- **Elasticsearch**: Asynchronously indexed search replica powering user-facing catalog search.
