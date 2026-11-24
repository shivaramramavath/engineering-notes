# Node.js ORM Architecture: Prisma, Drizzle & TypeORM Compared

> Comprehensive architectural evaluation of modern TypeScript ORMs: Drizzle ORM, Prisma v6, and legacy TypeORM.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **ORMs** | Drizzle ORM v0.38+, Prisma v6+, TypeORM v0.3+ |
| **Level** | Staff Architectural Decision |

---

## 1. Philosophical Differences

| Dimension | Drizzle ORM | Prisma v6 | TypeORM (Legacy) |
| :--- | :--- | :--- | :--- |
| **Paradigm** | Code-first (pure TypeScript) | Schema-first (`schema.prisma` DSL) | Class Decorators (ActiveRecord/DataMapper)|
| **Query Style** | SQL-like query builder | Abstracted method chaining (`findMany`) | Repository query builder |
| **Runtime Overhead**| **Near zero (~8kB)** | Moderate (compiled client + engine) | Heavy (reflect-metadata) |
| **Cold Start / Serverless**| **Instant (< 10ms)** | Good (improved in v6) | Slow |
| **PostgreSQL Features**| 100% native (JSONB, pgvector, CTEs)| Good (with typed `$queryRaw`) | Clunky for modern PG features |
| **Type Generation**| Pure compile-time inference | Requires `prisma generate` step | TypeScript runtime reflection |

---

## 2. Code Comparison: Fetching User with Recent Orders

### Drizzle ORM (SQL-Like Transparency)
```typescript
import { eq, desc } from 'drizzle-orm';
import { db } from './db';
import { users, orders } from './schema';

const result = await db.query.users.findFirst({
  where: eq(users.email, 'elena@example.com'),
  with: {
    orders: {
      orderBy: [desc(orders.createdAt)],
      limit: 5,
    },
  },
});
```

### Prisma v6 (High-Level Ergonomics)
```typescript
import { prisma } from './prisma';

const result = await prisma.user.findUnique({
  where: { email: 'elena@example.com' },
  include: {
    orders: {
      orderBy: { createdAt: 'desc' },
      take: 5,
    },
  },
});
```

---

## 3. Decision Framework

1. **Choose Drizzle ORM when**:
   - Building high-throughput Node.js APIs or serverless/edge functions where bundle size and cold starts matter.
   - The team knows SQL and wants complete transparency over the executed database statements.
   - Utilizing advanced PostgreSQL features (e.g. `pgvector` for AI embeddings).
2. **Choose Prisma 6 when**:
   - The team values an abstracted, highly readable declarative schema file as the single source of truth.
   - Building applications with standard relational models where automated migrations and built-in Studio GUI provide maximum velocity.
3. **Avoid TypeORM for greenfield projects**:
   - High technical debt, issues with circular relations, and heavy decorator overhead.
