# Kysely and Type-Safe SQL with NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 04-intermediate / 02-database-foundations
- **Prerequisites**: [01-database-module-pattern](./01-database-module-pattern.md), [04-drizzle-with-nestjs](./04-drizzle-with-nestjs.md), [00-prerequisites/01-typescript-essentials](../../00-prerequisites/01-typescript-essentials.md)

---

## 1. Context & Purpose

When building complex database systems—involving window functions, recursive Common Table Expressions (CTEs), multi-table joins, and intricate aggregation queries—traditional ORMs often hit a wall. Developers are forced to drop down into raw SQL strings (`queryRunner.query("SELECT ...")`), completely forfeiting TypeScript type safety and autocompletion.

**Kysely** (pronounced "Key-seh-lee") is a type-safe TypeScript SQL query builder. Unlike traditional ORMs that hide SQL behind complex object-relational abstraction layers, Kysely embraces SQL. It infers types from your existing database schema interfaces, providing compile-time type validation for every table name, column, join condition, and subquery with zero runtime overhead and zero schema generation steps.

---

## 2. Theoretical Foundations

Kysely maps pure TypeScript type definitions directly to SQL syntax:
- **Zero Runtime Magic**: Kysely does not inspect prototypes, instantiate classes, or maintain identity maps. It strictly translates chainable function calls into parameterized SQL.
- **Type-Safe Autocompletion**: If you typo a column name (`db.selectFrom('users').select('emial')`), TypeScript catches it at compile time.
- **Support for Advanced SQL**: First-class support for CTEs (`with`), window functions (`over`), lateral joins, JSON operators, and dialect-specific extensions for PostgreSQL, MySQL, and SQLite.

---

## 3. Architecture & Internal Mechanics

```
TypeScript Database Interface
interface Database {
  users: UsersTable;
  orders: OrdersTable;
}
              │
              ▼
Kysely Query Builder: Kysely<Database>
              │
              ├─> Validates table and column names at compile time
              ├─> Translates chainable API into Parameterized SQL AST
              │
              ▼
Database Driver Dialect (pg.Pool / mysql2)
              │
              ▼
Emitted Parameterized SQL:
SELECT id, email FROM users WHERE is_active = $1
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save kysely pg
npm install --save-dev @types/pg kysely-codegen
```

### 4.2 Defining Database Schema Interfaces

```typescript
// src/database/types.ts
import { Generated, ColumnType } from 'kysely';

export interface UsersTable {
  id: Generated<string>;
  email: string;
  name: string;
  is_active: Generated<boolean>;
  created_at: ColumnType<Date, string | undefined, never>;
}

export interface OrdersTable {
  id: Generated<string>;
  user_id: string;
  total_amount: number;
  status: 'PENDING' | 'COMPLETED' | 'CANCELLED';
  created_at: Generated<Date>;
}

export interface Database {
  users: UsersTable;
  orders: OrdersTable;
}
```

---

### 4.3 Building the Injectable NestJS Kysely Module

```typescript
// src/database/kysely.module.ts
import { Module, Global, OnApplicationShutdown, Inject } from '@nestjs/common';
import { ConfigModule, ConfigService } from '@nestjs/config';
import { Kysely, PostgresDialect } from 'kysely';
import { Pool } from 'pg';
import { Database } from './types';

export const KYSELY_DB = Symbol('KYSELY_DB');

@Global()
@Module({
  imports: [ConfigModule],
  providers: [
    {
      provide: KYSELY_DB,
      useFactory: (config: ConfigService) => {
        const pool = new Pool({
          connectionString: config.getOrThrow<string>('DATABASE_URL'),
          max: 20,
        });

        return new Kysely<Database>({
          dialect: new PostgresDialect({ pool }),
        });
      },
      inject: [ConfigService],
    },
  ],
  exports: [KYSELY_DB],
})
export class KyselyModule implements OnApplicationShutdown {
  constructor(@Inject(KYSELY_DB) private readonly db: Kysely<Database>) {}

  async onApplicationShutdown() {
    await this.db.destroy(); // Cleanly terminates PG connection pool
  }
}
```

---

### 4.4 Advanced Queries in Services

```typescript
// src/orders/orders.service.ts
import { Injectable, Inject, NotFoundException } from '@nestjs/common';
import { Kysely, sql } from 'kysely';
import { KYSELY_DB } from '../database/kysely.module';
import { Database } from '../database/types';

@Injectable()
export class OrdersService {
  constructor(
    @Inject(KYSELY_DB)
    private readonly db: Kysely<Database>,
  ) {}

  // 1. Type-Safe Join & Aggregate Query
  async getCustomerSpendingReport() {
    return this.db
      .selectFrom('users')
      .innerJoin('orders', 'orders.user_id', 'users.id')
      .select([
        'users.id as userId',
        'users.email',
        this.db.fn.count<number>('orders.id').as('orderCount'),
        this.db.fn.sum<number>('orders.total_amount').as('totalSpent'),
      ])
      .groupBy(['users.id', 'users.email'])
      .having(this.db.fn.sum('orders.total_amount'), '>', 1000)
      .execute();
  }

  // 2. Complex Common Table Expression (CTE) Query
  async getTopCustomersWithRank() {
    return this.db
      .with('ranked_orders', (db) =>
        db
          .selectFrom('orders')
          .select([
            'user_id',
            'total_amount',
            sql<number>`rank() over (partition by user_id order by total_amount desc)`.as(
              'rank',
            ),
          ]),
      )
      .selectFrom('ranked_orders')
      .innerJoin('users', 'users.id', 'ranked_orders.user_id')
      .where('rank', '=', 1)
      .select(['users.name', 'users.email', 'ranked_orders.total_amount'])
      .execute();
  }

  // 3. Atomic Transaction
  async createOrderWithAudit(userId: string, totalAmount: number) {
    return this.db.transaction().execute(async (trx) => {
      const order = await trx
        .insertInto('orders')
        .values({
          user_id: userId,
          total_amount: totalAmount,
          status: 'PENDING',
        })
        .returningAll()
        .executeTakeFirstOrThrow();

      return order;
    });
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Missing `executeTakeFirstOrThrow()`
Using `.execute()` returns an array. If you expect a single row, calling `.executeTakeFirst()` returns `T | undefined`. Forgetting null checks will cause runtime errors.
- **Handling**: Use `.executeTakeFirstOrThrow()` which throws a `NoResultError` automatically if the record is missing.

### 5.2 Dynamic Raw SQL Fragments
Using raw strings inside `sql` fragments without parameterization opens SQL Injection vulnerabilities:
```typescript
// ❌ Dangerous
sql`SELECT * FROM users WHERE email = '${email}'`;

// ✅ Safe (Parameterized)
sql`SELECT * FROM users WHERE email = ${email}`;
```

---

## 6. Performance & Memory Implications

- **Near-Zero Runtime Overhead**: Kysely compiles query ASTs into parameterized SQL strings in microseconds, producing performance identical to hand-written `pg.query()` calls.
- **Memory Footprint**: Does not retain instances or maintain dirty check snapshots, freeing memory immediately upon query resolution.

---

## 7. Anti-Patterns & Common Mistakes

- **Hand-Writing Complex Type Interfaces**: Manually typing 100 database tables instead of running `kysely-codegen` to automatically generate TypeScript interfaces from your live database schema.
- **Leaking Database Pool Connections**: Forgetting to implement `OnApplicationShutdown` to execute `await db.destroy()`, leaving connections open upon pod termination.

---

## 8. Production Checklist & Operational Readiness

- [ ] Connection pool limits configured via environment variables.
- [ ] Database schema interfaces generated via `kysely-codegen`.
- [ ] Shutdown hook implements `db.destroy()`.
- [ ] Parameterized bindings used in all custom `sql` template tags.
- [ ] Complex CTE queries verified using `explain analyze`.

---

## 9. Migration & Version History

- **Kysely 0.20+**: Introduced strict type-safe transactions and improved plugin architecture.
- **Kysely 0.27+**: First-class support for modern Node 20/22 ESM runtimes and NestJS 11 integration.

---

## 10. Verification & Testing

Unit testing a Kysely service with mock query runner:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { OrdersService } from './orders.service';
import { KYSELY_DB } from '../database/kysely.module';

describe('OrdersService (Kysely Test)', () => {
  let service: OrdersService;
  let mockDb: any;

  beforeEach(async () => {
    mockDb = {
      selectFrom: vi.fn().mockReturnThis(),
      innerJoin: vi.fn().mockReturnThis(),
      select: vi.fn().mockReturnThis(),
      groupBy: vi.fn().mockReturnThis(),
      having: vi.fn().mockReturnThis(),
      execute: vi.fn().mockResolvedValue([{ userId: 'u1', email: 'a@b.com', totalSpent: 1500 }]),
      fn: {
        count: vi.fn().mockReturnValue('count'),
        sum: vi.fn().mockReturnValue('sum'),
      },
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        OrdersService,
        { provide: KYSELY_DB, useValue: mockDb },
      ],
    }).compile();

    service = module.get<OrdersService>(OrdersService);
  });

  it('queries spending report successfully', async () => {
    const report = await service.getCustomerSpendingReport();
    expect(report.length).toBe(1);
    expect(report[0].totalSpent).toBe(1500);
  });
});
```

---

## 11. Comprehensive Examples

### Generating Type Definitions with `kysely-codegen`

Add script to `package.json`:
```json
{
  "scripts": {
    "db:types": "kysely-codegen --dialect postgres --out-file src/database/types.ts"
  }
}
```

This introspects your live PostgreSQL database and produces exact TypeScript interfaces for all tables, views, and enum types automatically.

---

## 12. Cross-References & Further Reading

- [01-database-module-pattern.md](./01-database-module-pattern.md) - Database architecture
- [04-drizzle-with-nestjs.md](./04-drizzle-with-nestjs.md) - Drizzle ORM
- [09-sql/01-postgres](../../09-sql) - PostgreSQL reference
- [Kysely Official Documentation](https://kysely.dev)
