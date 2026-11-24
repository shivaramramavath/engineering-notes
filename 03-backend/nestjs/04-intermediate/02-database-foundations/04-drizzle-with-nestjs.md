# Drizzle ORM Integration with NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 04-intermediate / 02-database-foundations
- **Prerequisites**: [01-database-module-pattern](./01-database-module-pattern.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

Traditional heavy ORMs (TypeORM, Hibernate-style frameworks) introduce substantial abstraction overhead: reflection-based metadata scanning, hidden query emissions (N+1 queries), complex identity maps, and large startup bundle footprints.

**Drizzle ORM** has gained massive popularity in the TypeScript ecosystem as a modern, lightweight, **zero-overhead** SQL query builder and ORM. Drizzle is "If you know SQL, you know Drizzle." It has zero dependencies, emits exactly the SQL queries you write, provides compile-time TypeScript type inference without code generation steps, and boasts instantaneous startup times, making it ideal for microservices and cloud workloads.

---

## 2. Theoretical Foundations

Drizzle is built around pure TypeScript schema declarations:
- **Schema as Code**: Database tables, columns, indexes, and relations are declared using pure TypeScript functions (`pgTable`, `varchar`, `uuid`, `relations`).
- **No Hidden Magic**: Drizzle generates pure SQL queries with parameterization. What you write in TypeScript maps 1:1 to emitted SQL.
- **Inferred Types**: Query result types are inferred automatically using advanced TypeScript conditional types (`typeof users.$inferSelect`, `typeof users.$inferInsert`).
- **Connection Agnostic**: Works directly with raw drivers like `pg`, `postgres.js`, or `@neondatabase/serverless`.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   schema/users.ts                      │
│     pgTable('users', { id: uuid(), name: text() })     │
└───────────────────────────┬────────────────────────────┘
                            │
              TypeScript Type Inference ($inferSelect)
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                    DrizzleModule                       │
│    Provides DRIZZLE_ORM Token via postgres.js pool     │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                    UsersService                        │
│    constructor(@Inject(DRIZZLE_ORM) private db)        │
│    await db.select().from(users).where(...)            │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
               Direct SQL Execution on PostgreSQL
               SELECT id, name FROM users WHERE ...
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save drizzle-orm postgres
npm install --save-dev drizzle-kit @types/pg
```

### 4.2 Defining Schemas

```typescript
// src/drizzle/schema/users.schema.ts
import { pgTable, uuid, text, timestamp, boolean } from 'drizzle-orm/pg-core';
import { relations } from 'drizzle-orm';
import { posts } from './posts.schema';

export const users = pgTable('users', {
  id: uuid('id').defaultRandom().primaryKey(),
  email: text('email').notNull().unique(),
  name: text('name').notNull(),
  isActive: boolean('is_active').default(true).notNull(),
  createdAt: timestamp('created_at').defaultNow().notNull(),
});

export const usersRelations = relations(users, ({ many }) => ({
  posts: many(posts),
}));

export type User = typeof users.$inferSelect;
export type NewUser = typeof users.$inferInsert;
```

---

### 4.3 Implementing the Database Module

Create a clean, injectable Drizzle provider:

```typescript
// src/drizzle/drizzle.module.ts
import { Module, Global } from '@nestjs/common';
import { ConfigModule, ConfigService } from '@nestjs/config';
import { drizzle } from 'drizzle-orm/postgres-js';
import postgres from 'postgres';
import * as schema from './schema';

export const DRIZZLE_ORM = Symbol('DRIZZLE_ORM');

@Global()
@Module({
  imports: [ConfigModule],
  providers: [
    {
      provide: DRIZZLE_ORM,
      useFactory: (configService: ConfigService) => {
        const connectionString = configService.getOrThrow<string>('DATABASE_URL');
        const client = postgres(connectionString, { max: 20 });
        return drizzle(client, { schema });
      },
      inject: [ConfigService],
    },
  ],
  exports: [DRIZZLE_ORM],
})
export class DrizzleModule {}
```

---

### 4.4 Consuming Drizzle in NestJS Services

```typescript
// src/users/users.service.ts
import { Injectable, Inject, NotFoundException } from '@nestjs/common';
import { eq, desc } from 'drizzle-orm';
import { PostgresJsDatabase } from 'drizzle-orm/postgres-js';
import { DRIZZLE_ORM } from '../drizzle/drizzle.module';
import * as schema from '../drizzle/schema';
import { User, NewUser } from '../drizzle/schema/users.schema';

@Injectable()
export class UsersService {
  constructor(
    @Inject(DRIZZLE_ORM)
    private readonly db: PostgresJsDatabase<typeof schema>,
  ) {}

  async findAll(): Promise<User[]> {
    return this.db.select().from(schema.users).orderBy(desc(schema.users.createdAt));
  }

  async findById(id: string): Promise<User> {
    const [user] = await this.db
      .select()
      .from(schema.users)
      .where(eq(schema.users.id, id))
      .limit(1);

    if (!user) {
      throw new NotFoundException(`User with ID ${id} not found`);
    }

    return user;
  }

  async create(dto: NewUser): Promise<User> {
    const [created] = await this.db
      .insert(schema.users)
      .values(dto)
      .returning();

    return created;
  }

  // Relational Queries using Drizzle Query API
  async findWithPosts(id: string) {
    return this.db.query.users.findFirst({
      where: eq(schema.users.id, id),
      with: {
        posts: true,
      },
    });
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Array Destructuring on Insert/Select
Drizzle's `.returning()` and `.select()` always return an array of results. Forgetting destructuring (`const [user] = await ...`) means variables hold arrays instead of single entity objects.
- **Handling**: Always destructure single-result queries or use the Relational Query API (`db.query.users.findFirst()`).

### 5.2 Transaction Scope Propagation
Unlike TypeORM, transactions in Drizzle require passing the `tx` client explicitly through subsequent queries:
```typescript
await this.db.transaction(async (tx) => {
  await tx.insert(schema.accounts).values(...);
  await tx.insert(schema.auditLogs).values(...);
});
```
Querying `this.db` inside a transaction block executes outside the transaction!

---

## 6. Performance & Memory Implications

- **Cold Start Latency**: Near-zero bootstrap overhead. Drizzle has no entity metadata scanning, AST reflection, or schema recompilation steps, booting in < 5ms.
- **Execution Speed**: Benchmarks consistently demonstrate Drizzle executing queries at native driver speeds, outperforming TypeORM by 2x–3x under high concurrent loads.

---

## 7. Anti-Patterns & Common Mistakes

- **Forgetting `returning()` on Inserts**: Calling `await db.insert(users).values(...)` and expecting the inserted row to be returned. In PostgreSQL, omitting `.returning()` yields only an empty execution metadata object.
- **Accidental Schema Imports in Bundles**: Importing `drizzle-kit` CLI tools in application runtime code. `drizzle-kit` is strictly a development migration tool.

---

## 8. Production Checklist & Operational Readiness

- [ ] Connection pool limit (`max: 20`) tuned to database capacity and replica pod count.
- [ ] Schema changes managed via `drizzle-kit generate` and `drizzle-kit migrate`.
- [ ] Transactions execute strictly on the `tx` parameter context.
- [ ] Prepared statements utilized for high-frequency hot queries.
- [ ] Connection cleanup implemented in NestJS `onApplicationShutdown` lifecycle.

---

## 9. Migration & Version History

- **Drizzle ORM 0.28+**: Introduced the Relational Query API (`db.query.*`).
- **Drizzle ORM 0.30+ / 1.0**: Full TypeScript 5+ type inference, multi-driver support, and complete NestJS 11 compatibility.

---

## 10. Verification & Testing

Mocking Drizzle ORM in unit tests:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { UsersService } from './users.service';
import { DRIZZLE_ORM } from '../drizzle/drizzle.module';

describe('UsersService (Drizzle Mock)', () => {
  let service: UsersService;
  let mockDb: any;

  beforeEach(async () => {
    mockDb = {
      select: vi.fn().mockReturnThis(),
      from: vi.fn().mockReturnThis(),
      orderBy: vi.fn().mockResolvedValue([{ id: '1', name: 'Alice' }]),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        UsersService,
        { provide: DRIZZLE_ORM, useValue: mockDb },
      ],
    }).compile();

    service = module.get<UsersService>(UsersService);
  });

  it('retrieves users successfully', async () => {
    const users = await service.findAll();
    expect(users).toEqual([{ id: '1', name: 'Alice' }]);
  });
});
```

---

## 11. Comprehensive Examples

### Drizzle Configuration File (`drizzle.config.ts`)

```typescript
// drizzle.config.ts
import { defineConfig } from 'drizzle-kit';

export default defineConfig({
  schema: './src/drizzle/schema/index.ts',
  out: './drizzle/migrations',
  dialect: 'postgresql',
  dbCredentials: {
    url: process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/db',
  },
  verbose: true,
  strict: true,
});
```

Execute migrations:
```bash
npx drizzle-kit generate
npx drizzle-kit migrate
```

---

## 12. Cross-References & Further Reading

- [01-database-module-pattern.md](./01-database-module-pattern.md) - Database architectural patterns
- [05-kysely-and-typed-sql.md](./05-kysely-and-typed-sql.md) - Kysely SQL query builder
- [03-typeorm/01-typeorm-setup.md](../03-typeorm/01-typeorm-setup.md) - TypeORM comparison
- [Drizzle ORM Documentation](https://orm.drizzle.team)
