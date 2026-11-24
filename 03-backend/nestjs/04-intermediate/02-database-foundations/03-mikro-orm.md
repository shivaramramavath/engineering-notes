# MikroORM Integration with NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 04-intermediate / 02-database-foundations
- **Prerequisites**: [01-database-module-pattern](./01-database-module-pattern.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

While TypeORM and Prisma are widely adopted in the Node.js ecosystem, enterprise applications managing high data integrity, complex relational domain models, and strict transactional consistency often suffer under active-record or simplified ORM paradigms.

**MikroORM** is a modern TypeScript Data Mapper and Unit of Work ORM inspired by Doctrine and Hibernate. It brings identity mapping, automatic change tracking (dirty checking), cascading persistence, and first-class NestJS integration via `@mikro-orm/nestjs`. It guarantees that a single database entity is never loaded twice into memory within the same request context, eliminating stale updates and accidental race conditions.

---

## 2. Theoretical Foundations

MikroORM operates on three foundational software engineering design patterns:
1. **Data Mapper Pattern**: Entities are pure domain model classes decoupled from database query logic (unlike Active Record where entities carry `save()` and `delete()` methods).
2. **Unit of Work (UoW)**: Maintains an in-memory snapshot of all managed entities during a request. When `em.flush()` is called, MikroORM computes the minimal SQL `UPDATE` / `INSERT` diffs automatically.
3. **Identity Map**: Ensures every entity loaded from the database has exactly one in-memory object instance within a single request context, preventing contradictory entity mutations.

---

## 3. Architecture & Internal Mechanics

```
Incoming Request
       │
       ▼
RequestContext.create() [Middleware / Interceptor]
       │
       ├─> Spawns a dedicated request-scoped EntityManager fork
       │
       ▼
Service calls em.find(User, 1)
       │
       ├─> Queries DB ──> Caches instance in Identity Map
       │
       ▼
Later in same request: em.find(User, 1)
       │
       ├─> Returns exact same object reference from Identity Map!
       │
       ▼
Entity Mutation: user.name = 'New Name'
       │
       ▼
await em.flush()
       │
       ├─> Unit of Work dirty checks original snapshot vs current state
       ├─> Emits atomic UPDATE "users" SET "name" = $1 WHERE "id" = $2
       │
       ▼
Request Ends -> RequestContext garbage collected
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @mikro-orm/core @mikro-orm/postgresql @mikro-orm/nestjs
npm install --save-dev @mikro-orm/cli
```

### 4.2 Defining Entities

```typescript
// src/users/entities/user.entity.ts
import { Entity, PrimaryKey, Property, OneToMany, Collection } from '@mikro-orm/core';
import { Post } from './post.entity';

@Entity({ tableName: 'users' })
export class User {
  @PrimaryKey({ type: 'uuid', defaultRaw: 'gen_random_uuid()' })
  id!: string;

  @Property({ unique: true })
  email!: string;

  @Property()
  name!: string;

  @Property({ default: true })
  isActive: boolean = true;

  @OneToMany(() => Post, (post) => post.author)
  posts = new Collection<Post>(this);

  @Property({ onCreate: () => new Date() })
  createdAt: Date = new Date();

  @Property({ onUpdate: () => new Date() })
  updatedAt: Date = new Date();
}
```

---

### 4.3 Registering `MikroOrmModule` in NestJS

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { MikroOrmModule } from '@mikro-orm/nestjs';
import { PostgreSqlDriver } from '@mikro-orm/postgresql';
import { User } from './users/entities/user.entity';

@Module({
  imports: [
    MikroOrmModule.forRoot({
      driver: PostgreSqlDriver,
      dbName: process.env.DB_NAME || 'enterprise_db',
      user: process.env.DB_USER || 'postgres',
      password: process.env.DB_PASSWORD || 'postgres',
      host: process.env.DB_HOST || 'localhost',
      port: 5432,
      entities: [User],
      debug: process.env.NODE_ENV !== 'production',
      registerRequestContext: true, // Automatically registers RequestContext middleware!
    }),
    MikroOrmModule.forFeature([User]),
  ],
})
export class AppModule {}
```

---

### 4.4 Using Repositories and Entity Manager in Services

```typescript
// src/users/users.service.ts
import { Injectable, NotFoundException } from '@nestjs/common';
import { InjectRepository } from '@mikro-orm/nestjs';
import { EntityRepository, EntityManager } from '@mikro-orm/postgresql';
import { User } from './entities/user.entity';

@Injectable()
export class UsersService {
  constructor(
    @InjectRepository(User)
    private readonly userRepository: EntityRepository<User>,
    private readonly em: EntityManager,
  ) {}

  async createUser(email: string, name: string): Promise<User> {
    const user = this.userRepository.create({ email, name });
    // Persist and flush via Unit of Work
    await this.em.persistAndFlush(user);
    return user;
  }

  async updateUserName(id: string, newName: string): Promise<User> {
    const user = await this.userRepository.findOne({ id });
    if (!user) throw new NotFoundException('User not found');

    // Dirty checking in action: simply mutate the domain property!
    user.name = newName;

    // Unit of Work automatically detects the mutation and generates UPDATE
    await this.em.flush();
    return user;
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 The `Forked EntityManager` Dilemma in Background Workers
When executing background jobs (e.g. BullMQ processors or Cron tasks), there is no incoming HTTP request to create a `RequestContext`. Using the global root `EntityManager` across multiple async jobs can cause concurrent entity collisions in the Identity Map.
- **Handling**: Explicitly wrap worker jobs in `RequestContext.createAsync()` or use `em.fork()`:
```typescript
await RequestContext.createAsync(this.em, async () => {
  await this.usersService.processJob();
});
```

### 5.2 Uninitialized Collection Access
Accessing unpopulated relations (`user.posts.getItems()`) throws an error if they were not eagerly fetched via `populate: ['posts']`.
- **Handling**: Populate relations explicitly:
```typescript
const user = await this.userRepository.findOne({ id }, { populate: ['posts'] });
```

---

## 6. Performance & Memory Implications

- **Identity Map Deduplication**: Eliminates duplicate object allocations in memory during complex queries.
- **Batched Flushes**: Multiple entity updates across the request are consolidated into a single transactional SQL batch during `em.flush()`, drastically reducing database roundtrips.

---

## 7. Anti-Patterns & Common Mistakes

- **Calling `persist()` without `flush()`**: Calling `em.persist(user)` and expecting the row to be in the database. `persist()` marks the entity as managed; `flush()` commits changes to SQL.
- **Using Global Root `EntityManager` Across Concurrent Tasks**: Sharing one root EntityManager without forks across simultaneous background promises.
- **Manual UPDATE Queries for Simple Field Changes**: Writing raw SQL queries when simply updating property values on loaded entities triggers clean dirty checking.

---

## 8. Production Checklist & Operational Readiness

- [ ] `registerRequestContext: true` enabled in root module options.
- [ ] Background workers and crons wrapped in `RequestContext.createAsync()`.
- [ ] Database connection pool limits configured to match database server capacity.
- [ ] Migrations managed via `@mikro-orm/cli` in continuous delivery pipelines.
- [ ] Slow query logging enabled in staging and production.

---

## 9. Migration & Version History

- **MikroORM 5.x**: Modernized relations and introduced native AsyncLocalStorage integration.
- **MikroORM 6.x**: Strict TypeScript typing, complete ESM support, and optimized NestJS 11 integration.

---

## 10. Verification & Testing

Testing with SQLite in-memory database:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { MikroOrmModule } from '@mikro-orm/nestjs';
import { SqliteDriver } from '@mikro-orm/sqlite';
import { UsersService } from './users.service';
import { User } from './entities/user.entity';

describe('UsersService (MikroORM Test)', () => {
  let service: UsersService;

  beforeAll(async () => {
    const module: TestingModule = await Test.createTestingModule({
      imports: [
        MikroOrmModule.forRoot({
          driver: SqliteDriver,
          dbName: ':memory:',
          entities: [User],
          allowGlobalContext: true, // Test only
        }),
        MikroOrmModule.forFeature([User]),
      ],
      providers: [UsersService],
    }).compile();

    service = module.get<UsersService>(UsersService);
    // Initialize schema
    const orm = module.get(MikroOrmModule);
    await (orm as any).getSchemaGenerator().createSchema();
  });

  it('creates and finds a user entity', async () => {
    const user = await service.createUser('test@example.com', 'Tester');
    expect(user.id).toBeDefined();
    expect(user.email).toBe('test@example.com');
  });
});
```

---

## 11. Comprehensive Examples

### Transactional Boundary with `em.transactional()`

```typescript
// src/banking/transfer.service.ts
import { Injectable } from '@nestjs/common';
import { EntityManager } from '@mikro-orm/postgresql';
import { Account } from './account.entity';

@Injectable()
export class TransferService {
  constructor(private readonly em: EntityManager) {}

  async transferFunds(fromId: string, toId: string, amount: number) {
    // Atomically execute inside SQL transaction
    return this.em.transactional(async (txEm) => {
      const from = await txEm.findOneOrFail(Account, { id: fromId });
      const to = await txEm.findOneOrFail(Account, { id: toId });

      if (from.balance < amount) {
        throw new Error('Insufficient funds');
      }

      from.balance -= amount;
      to.balance += amount;

      // Automatically flushes and commits transaction on block exit
    });
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-database-module-pattern.md](./01-database-module-pattern.md) - Database architecture
- [04-drizzle-with-nestjs.md](./04-drizzle-with-nestjs.md) - Drizzle lightweight alternative
- [04-prisma/01-prisma-setup.md](../04-prisma/01-prisma-setup.md) - Prisma ORM
- [MikroORM Official Documentation](https://mikro-orm.io)
