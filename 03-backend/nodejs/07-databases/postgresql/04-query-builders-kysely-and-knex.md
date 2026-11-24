# Type-Safe Query Builders: Kysely vs. Knex in Node.js

> Building complex dynamic SQL queries with full TypeScript inference using Kysely and comparing against traditional Knex.js.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Query Builders** | Kysely v0.27+, Knex.js v3+ |
| **Type Safety** | Zero runtime codegen; static compile-time type inference |

---

## 1. Why Use a Query Builder Instead of an ORM?

- **ORMs (Prisma, TypeORM)**: Abstract the database away behind object models. Great for simple CRUD, but clumsy when executing complex joins, window functions, CTEs, or bulk updates.
- **Raw SQL (`pg`)**: Maximum control, but zero type safety: typos in column names or missing parameters are only caught at runtime.
- **Query Builders (Kysely)**: Strike the optimal balance. You write SQL-like code with **100% compile-time autocomplete and type safety**, compiling directly to parameterized SQL strings.

---

## 2. Setting Up Kysely with PostgreSQL

```typescript
import { Kysely, PostgresDialect } from 'kysely';
import pg from 'pg';

// 1. Define Database Schema Interfaces
export interface DatabaseSchema {
  users: {
    id: number;
    email: string;
    first_name: string;
    last_name: string;
    is_active: boolean;
    created_at: Date;
  };
  orders: {
    id: string;
    user_id: number;
    total_cents: number;
    status: 'pending' | 'completed' | 'cancelled';
  };
}

// 2. Initialize Kysely with native pg pool
export const db = new Kysely<DatabaseSchema>({
  dialect: new PostgresDialect({
    pool: new pg.Pool({ connectionString: process.env.DATABASE_URL }),
  }),
});
```

---

## 3. Dynamic Type-Safe Queries

```typescript
// Autocompletes table names, columns, and enforces type match on values!
export async function getActiveUsersWithOrders(minOrderCents: number) {
  return await db
    .selectFrom('users')
    .innerJoin('orders', 'orders.user_id', 'users.id')
    .select([
      'users.id as userId',
      'users.email',
      'orders.id as orderId',
      'orders.total_cents as orderTotal',
    ])
    .where('users.is_active', '=', true)
    .where('orders.total_cents', '>=', minOrderCents)
    .orderBy('orders.total_cents', 'desc')
    .execute();
}
```
If you accidentally misspell `'is_active'` as `'isActive'`, TypeScript throws an immediate build error.
