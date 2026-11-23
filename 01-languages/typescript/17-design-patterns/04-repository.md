# Repository and Unit of Work Patterns in TypeScript

> Decoupling data persistence from domain logic using generic repositories, Unit of Work transaction boundaries, specification patterns, and ORM isolation in TypeScript.

| Metadata | Details |
| :--- | :--- |
| **Category** | Design Patterns |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md), [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [12-async-and-iteration/01-promises.md](../12-async-and-iteration/01-promises.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
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

The **Repository Pattern** mediates between the domain and data mapping layers, acting like an in-memory domain object collection. In modern TypeScript full-stack applications, developers often make the mistake of calling ORMs (Prisma, TypeORM, Drizzle, or Mongoose) directly inside business services or HTTP controllers:
```typescript
// Controller tightly coupled to database schema
const user = await prisma.user.findUnique({ where: { id } });
```

This direct coupling creates critical architectural liabilities:
1. **Un-Testable Business Logic**: Unit tests require a running database or complex database mocking harnesses.
2. **Persistence Leaks**: Database-specific concepts (raw SQL queries, foreign key IDs, connection handles) leak into domain logic.
3. **Multi-Store Impossibility**: Switching from PostgreSQL to DynamoDB or adding Redis caching requires rewriting every single service.

The **Unit of Work Pattern** maintains a list of business objects affected by a business transaction and coordinates the writing out of changes and the resolution of concurrency problems.

```
+--------------------+           queries           +-----------------------+
|   Domain Service   |  ------------------------>  |  UserRepositoryPort   |
| (Business Logic)   |                             |      (Interface)      |
+--------------------+                             +-----------------------+
                                                               ^
                                                               | implements
                                                   +-----------------------+
                                                   | PostgresUserRepository|
                                                   |     (SQL / Prisma)    |
                                                   +-----------------------+
```

---

## 2. Core Concepts

### 1. Generic Repository Interface
Defines fundamental CRUD operations parameterized over an entity type `T` and ID type `TId`:
```typescript
export interface Repository<T, TId> {
  findById(id: TId): Promise<T | null>;
  findAll(): Promise<readonly T[]>;
  save(entity: T): Promise<T>;
  delete(id: TId): Promise<boolean>;
}
```

### 2. Specification Pattern
Encapsulates business query criteria into reusable, composable specification objects instead of scattering raw SQL `WHERE` clauses across services:
```typescript
export interface Specification<T> {
  isSatisfiedBy(candidate: T): boolean;
  toSql(): string;
}
```

### 3. Unit of Work
Coordinates multiple repositories within a single database transaction, ensuring atomicity (ACID properties):
```typescript
export interface UnitOfWork {
  users: UserRepository;
  orders: OrderRepository;
  commit(): Promise<void>;
  rollback(): Promise<void>;
}
```

---

## 3. How It Works Internally

The domain layer depends strictly on an abstraction (`IUserRepository`), while the infrastructure layer provides a concrete implementation (`PrismaUserRepository`). Dependency Injection injects the concrete repository at application startup.

```
       Application Boundary
  +-------------------------------+
  |  Domain Service               |
  |  Depends on: IUserRepository  |
  +-------------------------------+
                 | (Runtime DI)
  +-------------------------------+
  |  PrismaUserRepository         |
  |  Implements: IUserRepository  |
  |  Talks to: PostgreSQL Database|
  +-------------------------------+
```

When unit testing, we substitute an `InMemoryUserRepository` that uses a simple JavaScript `Map<string, User>`, executing tests in milliseconds without touching a database.

---

## 4. Syntax & API Reference

### Generic Base Entity and Repository
```typescript
export interface BaseEntity<TId = string> {
  readonly id: TId;
  readonly createdAt: Date;
  readonly updatedAt: Date;
}

export interface IWriteRepository<T extends BaseEntity<TId>, TId = string> {
  create(entity: T): Promise<T>;
  update(entity: T): Promise<T>;
  delete(id: TId): Promise<boolean>;
}

export interface IReadRepository<T extends BaseEntity<TId>, TId = string> {
  findById(id: TId): Promise<T | null>;
  findMany(filter?: Partial<T>): Promise<readonly T[]>;
}

export type ICrudRepository<T extends BaseEntity<TId>, TId = string> = IWriteRepository<T, TId> & IReadRepository<T, TId>;
```

---

## 5. Practical Examples

### Example 1: Basic In-Memory Repository Implementation
```typescript
export interface UserEntity extends BaseEntity<string> {
  email: string;
  name: string;
}

export class InMemoryUserRepository implements ICrudRepository<UserEntity, string> {
  private store = new Map<string, UserEntity>();

  public async findById(id: string): Promise<UserEntity | null> {
    return this.store.get(id) ?? null;
  }

  public async findMany(filter?: Partial<UserEntity>): Promise<readonly UserEntity[]> {
    const all = Array.from(this.store.values());
    if (!filter) return all;
    return all.filter((item) =>
      Object.entries(filter).every(([key, val]) => (item as any)[key] === val)
    );
  }

  public async create(entity: UserEntity): Promise<UserEntity> {
    this.store.set(entity.id, entity);
    return entity;
  }

  public async update(entity: UserEntity): Promise<UserEntity> {
    this.store.set(entity.id, entity);
    return entity;
  }

  public async delete(id: string): Promise<boolean> {
    return this.store.delete(id);
  }
}
```

### Example 2: Real-World Prisma/SQL Repository with Mappers
```typescript
interface PrismaClientMock {
  user: {
    findUnique(args: { where: { id: string } }): Promise<any>;
    create(args: { data: any }): Promise<any>;
  };
}

export class PrismaUserRepository {
  constructor(private prisma: PrismaClientMock) {}

  public async findById(id: string): Promise<UserEntity | null> {
    const raw = await this.prisma.user.findUnique({ where: { id } });
    if (!raw) return null;
    return {
      id: raw.id,
      email: raw.email,
      name: raw.name,
      createdAt: new Date(raw.createdAt),
      updatedAt: new Date(raw.updatedAt)
    };
  }

  public async create(user: UserEntity): Promise<UserEntity> {
    await this.prisma.user.create({
      data: {
        id: user.id,
        email: user.email,
        name: user.name
      }
    });
    return user;
  }
}
```

### Example 3: Edge Case - Composable Specification Pattern
```typescript
export interface ISpecification<T> {
  isSatisfiedBy(candidate: T): boolean;
  and(other: ISpecification<T>): ISpecification<T>;
  or(other: ISpecification<T>): ISpecification<T>;
}

export abstract class BaseSpecification<T> implements ISpecification<T> {
  abstract isSatisfiedBy(candidate: T): boolean;

  public and(other: ISpecification<T>): ISpecification<T> {
    return new AndSpecification(this, other);
  }

  public or(other: ISpecification<T>): ISpecification<T> {
    return new OrSpecification(this, other);
  }
}

class AndSpecification<T> extends BaseSpecification<T> {
  constructor(private left: ISpecification<T>, private right: ISpecification<T>) { super(); }
  isSatisfiedBy(candidate: T): boolean {
    return this.left.isSatisfiedBy(candidate) && this.right.isSatisfiedBy(candidate);
  }
}

class OrSpecification<T> extends BaseSpecification<T> {
  constructor(private left: ISpecification<T>, private right: ISpecification<T>) { super(); }
  isSatisfiedBy(candidate: T): boolean {
    return this.left.isSatisfiedBy(candidate) || this.right.isSatisfiedBy(candidate);
  }
}
```

### Example 4: Failure Case - Type Discrepancy on ID Constraint
```typescript
interface CustomEntity extends BaseEntity<number> {
  title: string;
}

declare const repo: ICrudRepository<CustomEntity, number>;

// @ts-expect-error TS2345: Argument of type 'string' is not assignable to parameter of type 'number'.
repo.findById("uuid-1234");
```

### Example 5: Idiomatic Unit of Work Transaction Coordinator
```typescript
export interface ITransactionalSession {
  executeInTransaction<T>(work: (session: ITransactionalSession) => Promise<T>): Promise<T>;
}

export class OrderService {
  constructor(
    private session: ITransactionalSession,
    private userRepo: ICrudRepository<UserEntity>,
    private orderRepo: ICrudRepository<BaseEntity>
  ) {}

  public async placeOrder(userId: string, amount: number) {
    return await this.session.executeInTransaction(async () => {
      const user = await this.userRepo.findById(userId);
      if (!user) throw new Error('User not found');

      const order = await this.orderRepo.create({
        id: crypto.randomUUID(),
        createdAt: new Date(),
        updatedAt: new Date()
      });

      return order;
    });
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Leaking ORM Models as Domain Entities**: If your repository returns Prisma or TypeORM active record models directly, your domain layer is still coupled to the ORM. You must map ORM entities to pure domain entities inside the repository.
2. **The $N+1$ Query Problem in Generic Repositories**: Generic repositories with naive `.findById()` calls easily cause $N+1$ query issues when loading parent entities with collections. Specialized repository methods (e.g. `findWithOrderItems(id)`) or dataloaders must be used.
3. **Transaction Context Loss**: In asynchronous JavaScript, sharing a database transaction across multiple repository calls requires either passing a `tx` context parameter or using `AsyncLocalStorage` to store the active transaction implicitly.
4. **Over-Abstracting Everything**: Writing 10 generic interface layers for an application that will only ever talk to a single PostgreSQL database can create unnecessary indirection.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Direct ORM Import in Service** | `prisma.user.update(...)` in controller | `userRepo.update(user)` | Cannot unit test without running live database |
| **Leaking Query Builders** | `repo.query(q => q.where(...))` | Specification or dedicated method `findActiveUsers()` | Exposes persistence dialect to the domain |
| **Ignoring Atomic Transactions** | Multiple `await repo.save()` without Unit of Work | Execute within `unitOfWork.execute()` | Partial failure leaves corrupted database state |
| **Returning Null Without Type Check** | `findById(id): Promise<T>` | `findById(id): Promise<T \| null>` | Uncaught `Cannot read property of null` |
| **Mixing Read and Write Models in Cache** | Caching write entities directly | Segregated Read models (CQRS) | Stale cache and race conditions on concurrent writes |
| **Unbounded `findAll()`** | `findAll(): Promise<T[]>` on 1M rows | Paginated queries `findAll(page, limit)` | Node.js process out of memory (OOM) |

---

## 8. Performance & Type-Checker Complexity

- **Mapping Overhead**: Mapping database rows to domain entities incurs a minor allocation cost (~1-2 microseconds per 100 rows), which is vastly outweighed by database network latency (5-20 milliseconds).
- **Type Checker**: Keep generic constraints shallow (`T extends BaseEntity`). Deeply recursive entity types slow down type checking during repository instantiation.

---

## 9. Compiler Internals

TypeScript's generic constraints:
```typescript
interface Repository<T extends BaseEntity<TId>, TId> { ... }
```
During semantic analysis, the compiler checks the constraint `T extends BaseEntity<TId>`. If `T` lacks the `id`, `createdAt`, or `updatedAt` properties matching `TId`, type checking halts with `TS2344: Type 'T' does not satisfy the constraint 'BaseEntity<TId>'`.

---

## 10. Best Practices & Design Guidelines

1. **Keep Interfaces Focused**: Do not dump 40 methods into a single `IUserRepository`. Split into read, write, and domain-specific query interfaces.
2. **Always Map to Pure Domain Entities**: Strip ORM baggage before returning entities from the repository.
3. **Use AsyncLocalStorage for Transparent Transactions**: Avoid passing `tx` handles through every domain function signature.
4. **Use In-Memory Implementations for Unit Tests**: Achieve 100% test coverage with sub-second execution times.

---

## 11. Real-World Case Study

### Multi-Database Polyglot Architecture
An IoT enterprise tracks sensor telemetry in Cassandra while storing customer accounts in PostgreSQL:

```typescript
export interface DeviceRecord extends BaseEntity<string> {
  serialNumber: string;
  firmwareVersion: string;
}

export interface TelemetryPoint {
  deviceId: string;
  temperature: number;
  recordedAt: Date;
}

export interface IDeviceRepository extends ICrudRepository<DeviceRecord> {}
export interface ITelemetryRepository {
  record(point: TelemetryPoint): Promise<void>;
  getHistory(deviceId: string, limit: number): Promise<readonly TelemetryPoint[]>;
}

// Ingestion Service cleanly coordinates both without knowing database engines!
export class IngestionService {
  constructor(
    private deviceRepo: IDeviceRepository,
    private telemetryRepo: ITelemetryRepository
  ) {}

  public async ingest(serial: string, temp: number): Promise<void> {
    const devices = await this.deviceRepo.findMany({ serialNumber: serial });
    if (devices.length === 0) throw new Error('Unknown device');

    await this.telemetryRepo.record({
      deviceId: devices[0].id,
      temperature: temp,
      recordedAt: new Date()
    });
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the main difference between the Active Record pattern and the Data Mapper / Repository pattern?</summary>

In Active Record (e.g. Ruby on Rails, TypeORM Active Record), entities represent database tables directly and contain their own persistence methods (`user.save()`, `user.delete()`). In Data Mapper / Repository pattern, domain entities are pure business objects unaware of the database; an external repository coordinates saving and loading them. Repository achieves complete separation of concerns and superior testability.
</details>

<details>
<summary>2. How does the Unit of Work pattern handle database transactions?</summary>

Unit of Work tracks all modified, inserted, and deleted entities during a business transaction. When `commit()` is called, it opens a single atomic database transaction, executes all queries in order, and commits. If any operation fails, it issues a `rollback()`, ensuring database consistency.
</details>

<details>
<summary>3. Why should a repository return a Domain Entity rather than an ORM database model?</summary>

Because ORM models are coupled to the physical database schema and often contain active-record methods, dirty tracking flags, or circular references. Domain entities enforce business invariants and protect the domain layer from breaking changes when the database schema changes.
</details>

<details>
<summary>4. How can you implement database transactions across multiple repositories in Node.js without passing the transaction object everywhere?</summary>

By using Node.js's built-in `AsyncLocalStorage`. The Unit of Work initiates a transaction and stores the client handle in `AsyncLocalStorage`. Repositories retrieve the active transaction from storage automatically.
</details>

<details>
<summary>5. What is the Specification Pattern and how does it integrate with Repositories?</summary>

The Specification pattern encapsulates a business query rule (e.g., `OverdueInvoiceSpecification`) into a standalone object with an `isSatisfiedBy(entity)` method and a `toSql()` / `toQuery()` method. Repositories accept specifications to query the database cleanly without repeating query logic.
</details>

<details>
<summary>6. How do you prevent $N+1$ query issues when using the Repository pattern?</summary>

By providing explicit batch-loading methods (`findByIds(ids)`), utilizing GraphQL DataLoaders, or writing specific aggregate root repository methods that execute SQL joins rather than relying on lazy loading.
</details>

<details>
<summary>7. What are the drawbacks of using an In-Memory Repository in unit tests?</summary>

In-memory repositories do not test database-specific features like foreign key cascades, unique constraint collisions, transactional locking, or raw SQL syntax errors. They should be complemented with integration tests against a real test database (e.g. using Testcontainers).
</details>

<details>
<summary>8. How do you type a repository method that accepts dynamic sort orders safely?</summary>

By constraining sort keys to valid entity properties:
```typescript
type SortOrder<T> = {
  column: keyof T;
  direction: 'ASC' | 'DESC';
};
```
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic In-Memory Key-Value Repository</summary>

**Problem:** Create a generic in-memory repository implementing `get(id: string): Promise<T | null>` and `set(id: string, item: T): Promise<void>`.

**Solution:**
```typescript
export class SimpleRepo<T> {
  private map = new Map<string, T>();
  async get(id: string): Promise<T | null> { return this.map.get(id) ?? null; }
  async set(id: string, item: T): Promise<void> { this.map.set(id, item); }
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Entity ID Extraction Type</summary>

**Problem:** Write a utility type `ExtractId<T>` that extracts the type of property `id` from any entity extending `BaseEntity`.

**Solution:**
```typescript
type ExtractId<T extends BaseEntity<any>> = T['id'];
```
</details>

<details>
<summary>Exercise 3 (Medium): Read-Only Repository Wrapper</summary>

**Problem:** Create a decorator/wrapper class `ReadOnlyRepository<T, TId>` that wraps a mutable repository but only exposes read methods, preventing mutation at compile-time and runtime.

**Solution:**
```typescript
export class ReadOnlyRepository<T extends BaseEntity<TId>, TId> implements IReadRepository<T, TId> {
  constructor(private inner: ICrudRepository<T, TId>) {}

  public findById(id: TId): Promise<T | null> {
    return this.inner.findById(id);
  }

  public findMany(filter?: Partial<T>): Promise<readonly T[]> {
    return this.inner.findMany(filter);
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Event-Dispatching Repository</summary>

**Problem:** Implement a repository decorator that emits an `'ENTITY_SAVED'` event every time an entity is saved.

**Solution:**
```typescript
export class EventSourcedRepo<T extends BaseEntity> implements IWriteRepository<T> {
  constructor(
    private inner: IWriteRepository<T>,
    private onSave: (entity: T) => void
  ) {}

  async create(e: T): Promise<T> {
    const res = await this.inner.create(e);
    this.onSave(res);
    return res;
  }
  async update(e: T): Promise<T> {
    const res = await this.inner.update(e);
    this.onSave(res);
    return res;
  }
  async delete(id: string): Promise<boolean> {
    return this.inner.delete(id);
  }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Specification Evaluator</summary>

**Problem:** Build an in-memory specification evaluator that checks an entity against complex nested AND/OR rules and returns a boolean.

**Solution:**
```typescript
export type Predicate<T> = (item: T) => boolean;

export function andSpec<T>(...specs: Predicate<T>[]): Predicate<T> {
  return (item: T) => specs.every((fn) => fn(item));
}

export function orSpec<T>(...specs: Predicate<T>[]): Predicate<T> {
  return (item: T) => specs.some((fn) => fn(item));
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Unit of Work with Rollback Stack</summary>

**Problem:** Implement a Unit of Work coordinator that collects compensation/rollback actions for each repository mutation, executing all compensations in reverse order if any step throws.

**Solution:**
```typescript
export class CompensatingUnitOfWork {
  private rollbacks: (() => Promise<void>)[] = [];

  public registerCompensatingAction(action: () => Promise<void>): void {
    this.rollbacks.push(action);
  }

  public async execute<T>(work: () => Promise<T>): Promise<T> {
    try {
      const result = await work();
      this.rollbacks = [];
      return result;
    } catch (err) {
      console.warn('Execution failed, rolling back compensations...');
      while (this.rollbacks.length > 0) {
        const rollback = this.rollbacks.pop()!;
        try { await rollback(); } catch (e) { console.error('Rollback error:', e); }
      }
      throw err;
    }
  }
}
```
</details>
