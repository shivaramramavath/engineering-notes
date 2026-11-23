# Database Typing in TypeScript: Prisma, Drizzle ORM, and Kysely

> Deep dive into TypeScript database typing: Prisma generated client types, Drizzle ORM type inference (`$inferSelect`, `$inferInsert`), Kysely SQL query builder typing, and migrations.

| Metadata | Details |
| :--- | :--- |
| **Category** | Node.js Backend Typing |
| **Difficulty** | Advanced |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [16-type-safe-apis/00-api-contracts.md](../16-type-safe-apis/00-api-contracts.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In traditional web applications, SQL queries were written as plain string literals (`"SELECT * FROM users WHERE id = " + id`). This historical approach suffered from catastrophic vulnerabilities:
- **SQL Injection**: String interpolation enabled attackers to bypass authentication.
- **Silent Schema Drift**: Renaming a database column (`user_name` to `username`) produced no compiler warnings, resulting in runtime `Undefined column` crashes in production.
- **Untyped Results**: Database drivers returned rows as `any[]`, turning every database query into an unverified runtime assertion.

Modern TypeScript database libraries provide complete compile-time type safety:
1. **Prisma**: Schema-first ORM. Defines schema in `schema.prisma` and generates a dedicated `@prisma/client` package containing exact TypeScript model definitions and relation payloads.
2. **Drizzle ORM**: Code-first TypeScript-centric ORM. Defines SQL tables directly in TypeScript, automatically inferring select and insert shapes via `$inferSelect` and `$inferInsert`.
3. **Kysely**: Type-safe SQL query builder. Uses zero code-generation runtime overhead, typing arbitrary raw SQL queries using pure TypeScript interfaces.

```
+-----------------------------------------------------------+
|              TypeScript Database Typing Models            |
|                                                           |
|  Prisma:       schema.prisma  --->  npx prisma generate   |
|                                     (Generates Client)    |
|                                                           |
|  Drizzle:      pgTable('users', { id: serial()... })      |
|                type User = typeof users.$inferSelect      |
|                                                           |
|  Kysely:       interface Database { users: UserTable }    |
|                db.selectFrom('users').select('id')        |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Prisma Client Generated Payloads
Prisma generates model types and utility types for relation includes:
```typescript
import { PrismaClient, Prisma } from '@prisma/client';

const prisma = new PrismaClient();

// Extract payload with specific relations included:
type UserWithPosts = Prisma.UserGetPayload<{
  include: { posts: true }
}>;
```

### 2. Drizzle ORM Schema Type Inference
Drizzle tables are pure TypeScript objects:
```typescript
import { pgTable, serial, text, timestamp } from 'drizzle-orm/pg-core';

export const users = pgTable('users', {
  id: serial('id').primaryKey(),
  name: text('name').notNull(),
  email: text('email').notNull().unique(),
  createdAt: timestamp('created_at').defaultNow()
});

// Automatically infer types without code generation!
export type User = typeof users.$inferSelect;
export type NewUser = typeof users.$inferInsert;
```

### 3. Kysely Type-Safe Query Builder
```typescript
import { Kysely, PostgresDialect } from 'kysely';

export interface Database {
  users: {
    id: number;
    email: string;
    role: 'admin' | 'user';
  };
}

const db = new Kysely<Database>({ dialect: new PostgresDialect({ ... }) });

// Autocompleted columns; return type is Promise<{ id: number; email: string }[]>
const result = await db.selectFrom('users').select(['id', 'email']).execute();
```

---

## 3. How It Works Internally

- **Prisma**: The Prisma CLI parses `schema.prisma`, constructs an AST, and writes `.d.ts` declaration files directly into `node_modules/@prisma/client` containing thousands of overload signatures.
- **Drizzle ORM**: Uses advanced TypeScript mapped types and phantom properties (`$inferSelect`). When you invoke `db.select().from(users)`, TypeScript indexes `typeof users['$inferSelect']`.
- **Kysely**: Uses generic index access types `DB[Table]` to track selected columns at compile time, narrowing output types step-by-step through fluent query builder methods.

---

## 4. Syntax & API Reference

### Drizzle Relation Inference Helper
```typescript
import { relations } from 'drizzle-orm';

export const usersRelations = relations(users, ({ many }) => ({
  posts: many(posts)
}));
```

---

## 5. Practical Examples

### Example 1: Basic Prisma Client Query with Selection Typing
```typescript
import { PrismaClient } from '@prisma/client';

const prisma = new PrismaClient();

export async function getUserEmail(id: string): Promise<string | null> {
  const user = await prisma.user.findUnique({
    where: { id },
    select: { email: true } // Inferred type is strictly { email: string } | null
  });
  return user?.email ?? null;
}
```

### Example 2: Real-World Drizzle ORM Transaction with Inferred Types
```typescript
import { pgTable, uuid, text, integer } from 'drizzle-orm/pg-core';
import { eq, sql } from 'drizzle-orm';
import { NodePgDatabase } from 'drizzle-orm/node-postgres';

export const accounts = pgTable('accounts', {
  id: uuid('id').primaryKey().defaultRandom(),
  owner: text('owner').notNull(),
  balance: integer('balance').notNull()
});

export type Account = typeof accounts.$inferSelect;

export async function transferBalance(
  db: NodePgDatabase,
  fromId: string,
  toId: string,
  amount: number
) {
  return await db.transaction(async (tx) => {
    await tx.update(accounts)
      .set({ balance: sql`${accounts.balance} - ${amount}` })
      .where(eq(accounts.id, fromId));

    await tx.update(accounts)
      .set({ balance: sql`${accounts.balance} + ${amount}` })
      .where(eq(accounts.id, toId));
  });
}
```

### Example 3: Edge Case - Kysely Complex Group By and Aggregation
```typescript
import { Kysely, sql } from 'kysely';

interface Database {
  orders: { id: string; customerId: string; totalAmount: number; status: string };
}

export async function getCustomerOrderStats(db: Kysely<Database>) {
  return await db.selectFrom('orders')
    .select([
      'customerId',
      sql<number>`count(id)`.as('orderCount'),
      sql<number>`sum(total_amount)`.as('totalSpent')
    ])
    .where('status', '=', 'COMPLETED')
    .groupBy('customerId')
    .execute();
  // Inferred as: { customerId: string; orderCount: number; totalSpent: number }[]
}
```

### Example 4: Failure Case - Typo in Column Selection
```typescript
declare const db: Kysely<Database>;

// @ts-expect-error TS2345: Argument of type '"emial"' is not assignable to parameter of type '"id" | "customerId" | "totalAmount" | "status"'.
db.selectFrom('orders').select('emial');
```

### Example 5: Idiomatic Soft-Delete Middleware Filter in Prisma
```typescript
export function setupSoftDelete(prisma: PrismaClient) {
  prisma.$use(async (params, next) => {
    if (params.model === 'User') {
      if (params.action === 'delete') {
        params.action = 'update';
        params.args.data = { deletedAt: new Date() };
      }
      if (params.action === 'findUnique' || params.action === 'findFirst') {
        params.action = 'findFirst';
        params.args.where = { ...params.args.where, deletedAt: null };
      }
    }
    return next(params);
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Prisma Client Generation in CI/Docker**:
   Because Prisma client files are generated inside `node_modules`, running `npm install` without running `npx prisma generate` leaves `@prisma/client` missing or outdated. In Dockerfiles, always run `prisma generate` before building.
2. **Date vs Timestamp Strings**:
   PostgreSQL `TIMESTAMPTZ` columns serialize to JavaScript `Date` objects inside Prisma and Drizzle. When serializing to HTTP JSON, they turn into strings. Ensure DTO mappers account for this.
3. **Drizzle `$inferInsert` Optional Defaults**:
   Columns with `.default()` or auto-incrementing primary keys are correctly marked as **optional** in `$inferInsert`, but **required** in `$inferSelect`. Confusing the two causes compiler errors when inserting records.
4. **Kysely Raw SQL Escaping**:
   When writing raw SQL fragments in Kysely using `sql` template literals, always interpolate values as variables (`sql\`val = ${val}\``) rather than strings to ensure parameters are sent as parameterized SQL statements.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Using `any` for Query Results** | `db.query() as any` | Kysely / Drizzle / Prisma types | Disables compiler checks on database columns |
| **Missing `prisma generate` in CI** | Build fails on import `@prisma/client` | Run `npx prisma generate` in CI | `Cannot find module '@prisma/client'` |
| **Direct String SQL Injection** | `sql\`WHERE id = '\${id}'\`` | `sql\`WHERE id = \${id}\`` | Critical SQL injection vulnerability |
| **Using `$inferSelect` for Inserts** | `function create(u: User)` | `function create(u: NewUser)` | Forces client to supply auto-generated IDs |
| **Forgetting to Await Transactions** | `tx.update(...)` without await | `await tx.update(...)` | Concurrent execution order bugs in SQL |

---

## 8. Performance & Type-Checker Complexity

- **Query Execution Benchmarks**:
  - Prisma: High abstraction overhead (~1-3ms query engine latency).
  - Drizzle ORM: Zero runtime query engine; compiles directly to SQL strings in microseconds (~0.1ms).
  - Kysely: Lightweight query builder; negligible overhead (~0.05ms).
- **TSServer Memory**: Prisma's massive generated declarations can consume ~300MB of TSServer memory in large schemas (100+ tables). Drizzle and Kysely use significantly less compiler memory.

---

## 9. Compiler Internals

When Drizzle evaluates `typeof users.$inferSelect`:
The table builder attaches a phantom symbol `declare readonly $inferSelect: TSelect;`.
TypeScript reads the phantom property type during hover and assignment, extracting the exact object shape with zero runtime execution.

---

## 10. Best Practices & Design Guidelines

1. **Use Drizzle or Kysely for Performance-Critical Services**: Eliminate Prisma query engine overhead.
2. **Use `$inferInsert` for Creation DTOs**: Differentiate between database reads and inserts.
3. **Always Parameterize Raw SQL**: Use `sql` template literals safely.
4. **Centralize Database Schema Files**: Group tables by domain modules.

---

## 11. Real-World Case Study

### High-Velocity FinTech Ledger with Drizzle ORM
A high-throughput ledger executing 10,000 transactions per second with strict constraints:

```typescript
import { pgTable, uuid, bigint, timestamp, text } from 'drizzle-orm/pg-core';
import { drizzle } from 'drizzle-orm/node-postgres';
import { Pool } from 'pg';

export const ledgerEntries = pgTable('ledger_entries', {
  id: uuid('id').primaryKey().defaultRandom(),
  accountId: uuid('account_id').notNull(),
  amountCents: bigint('amount_cents', { mode: 'bigint' }).notNull(),
  direction: text('direction', { enum: ['CREDIT', 'DEBIT'] }).notNull(),
  timestamp: timestamp('timestamp', { withTimezone: true }).defaultNow().notNull()
});

export type LedgerEntry = typeof ledgerEntries.$inferSelect;
export type NewLedgerEntry = typeof ledgerEntries.$inferInsert;

export class LedgerRepository {
  private db: ReturnType<typeof drizzle>;

  constructor(pool: Pool) {
    this.db = drizzle(pool);
  }

  public async recordEntry(entry: NewLedgerEntry): Promise<LedgerEntry> {
    const [inserted] = await this.db.insert(ledgerEntries).values(entry).returning();
    return inserted;
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between Prisma and Drizzle ORM in TypeScript?</summary>

Prisma is schema-first and uses a custom DSL (`schema.prisma`) that generates client code and uses a separate Rust query engine binary. Drizzle ORM is code-first in pure TypeScript, requires no code generation, compiles directly to plain SQL strings with zero binary overhead, and infers types directly via TypeScript mapped types (`$inferSelect`).
</details>

<details>
<summary>2. What is Kysely and when is it preferred over full ORMs?</summary>

Kysely is a type-safe SQL query builder. It does not manage migrations or map relations like an ORM; instead, it focuses purely on writing compile-time verified SQL queries against user-defined TypeScript interfaces, providing full control over exact SQL queries without ORM magic.
</details>

<details>
<summary>3. What is the difference between `$inferSelect` and `$inferInsert` in Drizzle?</summary>

`$inferSelect` represents the shape of a row returned by a `SELECT` query, where all non-nullable columns and generated IDs are guaranteed present. `$inferInsert` represents the shape required to `INSERT` a row, where columns with default values, nullable columns, and auto-incrementing primary keys are optional.
</details>

<details>
<summary>4. Why should raw SQL fragments always be created using tagged template literals?</summary>

Tagged template literals (like `sql\`...\``) distinguish between static query strings and interpolated dynamic values, ensuring dynamic parameters are passed safely as parameterized variables (`$1`, `$2`), completely preventing SQL injection attacks.
</details>

<details>
<summary>5. How does Prisma handle relation loading type safety?</summary>

Prisma uses conditional utility types (like `Prisma.UserGetPayload<{ include: { posts: true } }>`). If a query specifies `include: { posts: true }`, the compiler adds `posts: Post[]` to the return type; otherwise, `posts` is absent.
</details>

<details>
<summary>6. How are PostgreSQL `bigint` columns typed in TypeScript?</summary>

JavaScript numbers lose precision above $2^{53} - 1$. PostgreSQL `bigint` columns are mapped to native JavaScript `bigint` primitives (e.g. `100n`) in Drizzle and Prisma, or string representations.
</details>

<details>
<summary>7. How do you prevent connection pool exhaustion in serverless Node.js functions?</summary>

Use connection poolers like Prisma Accelerate, PgBouncer, or Supabase Pooler, or configure database clients with low `max: 1` pool limits in serverless lambdas.
</details>

<details>
<summary>8. How do you type database migrations in TypeScript?</summary>

Using tools like Drizzle Kit or Prisma Migrate, where migrations are tracked via timestamped SQL files generated directly from the TypeScript schema definition.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Drizzle Schema Definition</summary>

**Problem:** Define a Drizzle table `tasks` with `id: serial`, `title: text`, and `completed: boolean` defaulting to false.

**Solution:**
```typescript
import { pgTable, serial, text, boolean } from 'drizzle-orm/pg-core';

export const tasks = pgTable('tasks', {
  id: serial('id').primaryKey(),
  title: text('title').notNull(),
  completed: boolean('completed').default(false).notNull()
});
```
</details>

<details>
<summary>Exercise 2 (Easy): Extract Drizzle Insert Type</summary>

**Problem:** Extract the insert type for the `tasks` table.

**Solution:**
```typescript
export type NewTask = typeof tasks.$inferInsert;
```
</details>

<details>
<summary>Exercise 3 (Medium): Kysely Database Interface</summary>

**Problem:** Define a Kysely `Database` interface for `users` and `posts` tables with foreign key relationship.

**Solution:**
```typescript
export interface Database {
  users: { id: number; email: string };
  posts: { id: number; authorId: number; title: string };
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Type-Safe SQL Join Query</summary>

**Problem:** Write a Kysely query joining `posts` to `users` and selecting `posts.title` and `users.email`.

**Solution:**
```typescript
import { Kysely } from 'kysely';

export async function getPostAuthors(db: Kysely<Database>) {
  return await db.selectFrom('posts')
    .innerJoin('users', 'users.id', 'posts.authorId')
    .select(['posts.title', 'users.email'])
    .execute();
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Prisma Dynamic Include Type Helper</summary>

**Problem:** Write a generic function that queries users with dynamic relations based on a boolean flag.

**Solution:**
```typescript
import { PrismaClient, Prisma } from '@prisma/client';

export async function fetchUsersWithPosts<TIncludePosts extends boolean>(
  prisma: PrismaClient,
  includePosts: TIncludePosts
): Promise<Prisma.UserGetPayload<{ include: { posts: TIncludePosts } }>[]> {
  return prisma.user.findMany({
    include: { posts: includePosts }
  }) as any;
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Drizzle Soft Delete Filter Wrapper</summary>

**Problem:** Write a helper function that automatically appends `where(isNull(table.deletedAt))` to any Drizzle select query.

**Solution:**
```typescript
import { isNull } from 'drizzle-orm';
import { pgTable, timestamp, text } from 'drizzle-orm/pg-core';

export const softDeletable = pgTable('items', {
  id: text('id').primaryKey(),
  deletedAt: timestamp('deleted_at')
});

export function activeOnly(table: typeof softDeletable) {
  return isNull(table.deletedAt);
}
```
</details>
