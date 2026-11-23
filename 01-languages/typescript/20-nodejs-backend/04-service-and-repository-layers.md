# Layered Architecture: Controllers, Services, and Repositories in TypeScript

> Structuring enterprise Node.js applications with TypeScript: Separation of Concerns, Domain-Driven Design (DDD) layer boundaries, dependency inversion, and DTO transformations.

| Metadata | Details |
| :--- | :--- |
| **Category** | Node.js Backend Typing |
| **Difficulty** | Advanced |
| **Prerequisites** | [17-design-patterns/04-repository.md](../17-design-patterns/04-repository.md), [17-design-patterns/05-dependency-injection.md](../17-design-patterns/05-dependency-injection.md) |
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

In poorly structured Node.js backends ("Anemic Architecture"), route handlers become monolithic dumping grounds: a single Express controller parses request bodies, validates fields, hashes passwords, executes raw SQL queries, and sends emails.

This tight coupling results in:
1. **Zero Testability**: Testing a single business rule requires spinning up an entire HTTP server and live database.
2. **Duplicated Business Logic**: If an event consumer or CLI command needs to create a user, it cannot reuse the logic buried inside an HTTP controller.
3. **Leaky Infrastructure**: Database ORM models leak directly into client responses, exposing sensitive password hashes.

**Layered Architecture (N-Tier / Clean Architecture)** strictly separates responsibilities into three distinct layers:
1. **Presentation Layer (Controllers)**: Translates HTTP/WebSockets to domain commands; returns HTTP status codes.
2. **Domain Layer (Services)**: Pure business rules, invariants, and workflows. Completely independent of HTTP or database engines.
3. **Infrastructure Layer (Repositories)**: Persists and queries data using SQL, Prisma, Redis, or external third-party SDKs.

```
+-----------------------------------------------------------+
|                   Layered Architecture                    |
|                                                           |
|  [ Controller ]  (HTTP, express, req/res, Status Codes)   |
|        |                                                  |
|        v (passes DTO)                                     |
|  [ Service ]     (Pure Business Rules, Calculations)      |
|        |                                                  |
|        v (calls Interface)                                |
|  [ Repository ]  (Prisma, SQL, Redis, Database models)    |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. The Dependency Inversion Principle (DIP)
High-level business modules (Services) must **not** depend on low-level modules (SQL Repositories). Both must depend on abstractions (TypeScript interfaces).
```typescript
// Domain Service depends on Interface:
export class UserService {
  constructor(private userRepo: IUserRepository) {}
}

// Infrastructure implements Interface:
export class PrismaUserRepository implements IUserRepository { ... }
```

### 2. DTO Transformations at Layer Boundaries
Data Transfer Objects cross layer boundaries:
- `CreateUserRequestDTO` -> Passed from Controller to Service.
- `UserEntity` -> Manipulated inside Service and stored by Repository.
- `UserResponseDTO` -> Mapped and returned by Controller to the client.

---

## 3. How It Works Internally

1. Controller receives `POST /api/users`. Validates input against `CreateUserSchema`.
2. Controller passes clean typed DTO `service.createUser(dto)`.
3. Service executes business validation (e.g. asserts email is unique). Calls `repo.save(user)`.
4. Service returns pure domain entity. Controller maps entity to response DTO and responds with 201 Created.

---

## 4. Syntax & API Reference

### Pure Domain Model Interface
```typescript
export interface DomainUser {
  id: string;
  email: string;
  hashedPassword: string;
  createdAt: Date;
}

export interface IUserRepository {
  findByEmail(email: string): Promise<DomainUser | null>;
  save(user: DomainUser): Promise<void>;
}
```

---

## 5. Practical Examples

### Example 1: Basic Domain Entity and Interface Definition
```typescript
export interface Account {
  id: string;
  balanceCents: bigint;
  status: 'ACTIVE' | 'SUSPENDED';
}

export interface IAccountRepository {
  getById(id: string): Promise<Account | null>;
  update(account: Account): Promise<void>;
}
```

### Example 2: Real-World Service Layer Implementation with Pure Invariants
```typescript
export class AccountService {
  constructor(private repo: IAccountRepository) {}

  public async transferFunds(sourceId: string, targetId: string, amountCents: bigint): Promise<void> {
    if (amountCents <= 0n) throw new RangeError('Transfer amount must be positive');

    const source = await this.repo.getById(sourceId);
    if (!source) throw new Error('Source account not found');
    if (source.status !== 'ACTIVE') throw new Error('Source account is suspended');
    if (source.balanceCents < amountCents) throw new Error('Insufficient funds');

    const target = await this.repo.getById(targetId);
    if (!target) throw new Error('Target account not found');
    if (target.status !== 'ACTIVE') throw new Error('Target account is suspended');

    source.balanceCents -= amountCents;
    target.balanceCents += amountCents;

    await this.repo.update(source);
    await this.repo.update(target);
  }
}
```

### Example 3: Edge Case - Controller Layer Mapping and HTTP Status Translation
```typescript
import type { Request, Response } from 'express';

export class AccountController {
  constructor(private service: AccountService) {}

  public transfer = async (req: Request, res: Response) => {
    try {
      const { sourceId, targetId, amountCents } = req.body;
      await this.service.transferFunds(sourceId, targetId, BigInt(amountCents));
      res.status(200).json({ success: true });
    } catch (err: any) {
      if (err.message === 'Insufficient funds') {
        res.status(400).json({ error: err.message });
      } else if (err.message.includes('not found')) {
        res.status(404).json({ error: err.message });
      } else {
        res.status(500).json({ error: 'Internal server error' });
      }
    }
  };
}
```

### Example 4: Failure Case - Direct ORM Import inside Service
```typescript
// ARCHITECTURAL VIOLATION:
// import { prisma } from '../db';
// class BadService {
//   async create() { await prisma.user.create(...); } // Tightly couples service to Prisma!
// }
```

### Example 5: Idiomatic In-Memory Repository for Lightning-Fast Unit Testing
```typescript
export class InMemoryAccountRepository implements IAccountRepository {
  private accounts = new Map<string, Account>();

  public async getById(id: string): Promise<Account | null> {
    const acc = this.accounts.get(id);
    return acc ? { ...acc } : null; // Return clone to prevent shared mutation
  }

  public async update(account: Account): Promise<void> {
    this.accounts.set(account.id, { ...account });
  }

  // Test helper:
  public seed(account: Account) {
    this.accounts.set(account.id, account);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Passing Express `req` or `res` into Service Methods**:
   Never pass `req` or `res` into service functions (`service.doSomething(req)`). This couples your business logic to the Express HTTP framework, preventing the service from being used in background queues, WebSocket handlers, or CLI commands. Pass plain DTOs.
2. **Leaking Database Entities to Clients**:
   Returning raw Prisma models directly to HTTP responses leaks internal columns (like password hashes or tenant IDs). Always map to explicit response DTOs.
3. **Anemic Domain Anti-Pattern**:
   Creating services that are merely pass-through proxies with zero logic (`service.get(id) { return repo.get(id) }`). If an entity has no business invariants, simpler architectures (like vertical slice or direct query handlers) may be preferred.
4. **Transaction Coordination Across Repositories**:
   Managing transactions without coupling services to specific SQL dialects requires Unit of Work abstractions or `AsyncLocalStorage`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Passing `req`/`res` to Service** | `service.createUser(req, res)` | `service.createUser(dto)` | Couplings business logic to HTTP framework |
| **Direct ORM in Controller** | `prisma.user.findFirst()` in route | Route calls Service -> calls Repo | Controller knows about database schema |
| **Fat Controller, Empty Service** | 100 lines of business logic in route | Logic in Service, HTTP routing in Controller | Cannot reuse or unit test business logic |
| **Leaking DB Model to JSON** | `res.json(userEntity)` | `res.json(UserMapper.toDTO(user))` | Leaks password hashes and internal secrets |
| **Hardcoding `new Repository()`** | `new UserService(new PostgresRepo())` | Inject via interface in composition root | Cannot mock repository in unit tests |

---

## 8. Performance & Type-Checker Complexity

- **Allocation Overhead**: Mapping domain entities to DTOs takes sub-microsecond time.
- **Type Checking**: Interfaces and classes check cleanly with zero compiler drag.

---

## 9. Compiler Internals

When a class implements an interface:
```typescript
class PostgresRepo implements IUserRepository { ... }
```
TypeScript verifies that every method in `IUserRepository` exists on `PostgresRepo` with compatible parameter and return types.

---

## 10. Best Practices & Design Guidelines

1. **Keep Services Pure**: Services should have zero dependencies on Express, Fastify, or HTTP headers.
2. **Use Interfaces for Repositories**: Program to interfaces, not concrete implementations.
3. **Always Map to Response DTOs**: Never return database rows directly to the client.
4. **Write In-Memory Repositories for Unit Tests**: Test all business workflows in milliseconds without database containers.

---

## 11. Real-World Case Study

### Multi-Channel Order Placement Pipeline
An order placement service invoked identically from both an Express REST API and a Kafka event consumer:

```typescript
export interface OrderDTO { customerId: string; totalCents: number; items: string[]; }
export interface IOrderRepository { save(order: OrderDTO): Promise<string>; }
export interface INotificationPort { sendReceipt(customerId: string, orderId: string): Promise<void>; }

export class PlaceOrderService {
  constructor(
    private orderRepo: IOrderRepository,
    private notifier: INotificationPort
  ) {}

  public async execute(dto: OrderDTO): Promise<string> {
    if (dto.items.length === 0) throw new Error('Cannot place empty order');
    const orderId = await this.orderRepo.save(dto);
    await this.notifier.sendReceipt(dto.customerId, orderId);
    return orderId;
  }
}

// Can be called from HTTP Controller:
// app.post('/orders', async (req, res) => { const id = await service.execute(req.body); res.json({ id }); });

// Can be called identically from Kafka consumer:
// kafka.on('order.received', async (event) => { await service.execute(event.data); });
```

---

## 12. Interview Questions

<details>
<summary>1. What is Layered Architecture and why is it standard in enterprise backend systems?</summary>

Layered Architecture organizes code into distinct horizontal layers with specific responsibilities: Controllers (HTTP/Presentation), Services (Business Logic), and Repositories (Data Access/Infrastructure). It enforces separation of concerns, decouples business rules from databases and web frameworks, and enables fast, isolated unit testing.
</details>

<details>
<summary>2. Why should service layer methods never accept `Request` or `Response` objects from Express?</summary>

Passing `Request` or `Response` tightly couples the service to the Express framework and HTTP transport. Business logic should be transport-agnostic so it can be reused across WebSockets, Kafka consumers, CLI scripts, or background workers without mocking HTTP request objects.
</details>

<details>
<summary>3. What is the Dependency Inversion Principle (DIP)?</summary>

DIP states that high-level modules should not depend on low-level modules; both should depend on abstractions (interfaces). In layered architecture, the Service layer defines the `IRepository` interface it needs, and the Infrastructure layer implements it.
</details>

<details>
<summary>4. What is the difference between a Domain Entity and a DTO?</summary>

A **Domain Entity** is a rich business model that enforces invariants and encapsulates core business logic. A **DTO (Data Transfer Object)** is an anemic data carrier used purely to transport data across system boundaries (such as between client and server or controller and service).
</details>

<details>
<summary>5. How does Layered Architecture simplify unit testing?</summary>

Because services depend on repository interfaces rather than concrete databases, services can be tested by injecting an `InMemoryRepository` (backed by a simple Map). Tests run in sub-milliseconds without network latency or database migrations.
</details>

<details>
<summary>6. Where should input validation (e.g. checking that an email is valid) occur in layered architecture?</summary>

Syntactic validation (e.g. checking email format or required fields) belongs in the Controller / Presentation layer using schemas like Zod. Semantic business validation (e.g. checking if an email is already registered) belongs in the Service layer.
</details>

<details>
<summary>7. What is an Anemic Domain Model and why is it criticized?</summary>

An Anemic Domain Model is where entities are merely dumb data bags with getters and setters, and all logic is dumped into procedural service classes. While common, Domain-Driven Design advocates for rich domain models where entities encapsulate their own validation and business invariants.
</details>

<details>
<summary>8. Where should database transactions be managed in layered architecture?</summary>

Transactions represent business transaction boundaries and should be orchestrated in the Service layer (using Unit of Work or transactional session abstractions), but the low-level SQL transaction handling belongs in the Infrastructure layer.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Service Interface</summary>

**Problem:** Define an interface `IUserService` with a method `registerUser(email: string): Promise<{ id: string }>`.

**Solution:**
```typescript
export interface IUserService {
  registerUser(email: string): Promise<{ id: string }>;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Pure Domain Calculator Method</summary>

**Problem:** Implement a pure function `calculateInvoiceTotal(subtotal: number, taxRate: number, discount: number): number`.

**Solution:**
```typescript
export function calculateInvoiceTotal(subtotal: number, taxRate: number, discount: number): number {
  if (subtotal < 0 || taxRate < 0 || discount < 0) throw new RangeError('Inputs must be non-negative');
  const taxed = subtotal * (1 + taxRate);
  return Math.max(0, taxed - discount);
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Controller with Injected Service</summary>

**Problem:** Write an Express controller class that accepts `IUserService` via its constructor and exposes a `postUser` route handler.

**Solution:**
```typescript
import type { Request, Response } from 'express';

export class UserController {
  constructor(private userService: { registerUser: (email: string) => Promise<{ id: string }> }) {}

  public postUser = async (req: Request, res: Response) => {
    const user = await this.userService.registerUser(req.body.email);
    res.status(201).json(user);
  };
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Layered DTO Mapper Class</summary>

**Problem:** Create a mapper class `UserMapper` that transforms a domain entity `{ id, email, passwordHash, createdAt }` into a public DTO `{ id, email, createdAtIso }`.

**Solution:**
```typescript
export class UserMapper {
  public static toDTO(entity: { id: string; email: string; createdAt: Date }) {
    return {
      id: entity.id,
      email: entity.email,
      createdAtIso: entity.createdAt.toISOString()
    };
  }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Fully-Decoupled Business Service Test</summary>

**Problem:** Write a complete unit test for a `PasswordResetService` that verifies token generation against an in-memory repository without external mocks.

**Solution:**
```typescript
import { it, expect } from 'vitest';

class InMemoryTokenRepo {
  public tokens = new Map<string, string>();
  async save(userId: string, token: string) { this.tokens.set(userId, token); }
}

class PasswordResetService {
  constructor(private repo: InMemoryTokenRepo) {}
  async generateReset(userId: string): Promise<string> {
    const token = 'token_123';
    await this.repo.save(userId, token);
    return token;
  }
}

it('generates and saves reset token', async () => {
  const repo = new InMemoryTokenRepo();
  const service = new PasswordResetService(repo);
  const token = await service.generateReset('usr_1');
  expect(token).toBe('token_123');
  expect(repo.tokens.get('usr_1')).toBe('token_123');
});
```
</details>

<details>
<summary>Exercise 6 (Hard): Composition Root Entrypoint Wiring</summary>

**Problem:** Implement a composition root function that instantiates the database adapter, wraps it in a repository, passes it to the service, and attaches it to the controller.

**Solution:**
```typescript
class DatabaseClient { query() {} }
class SqlProductRepo { constructor(db: DatabaseClient) {} }
class ProductService { constructor(repo: SqlProductRepo) {} }
class ProductController { constructor(service: ProductService) {} }

export function buildProductModule() {
  const db = new DatabaseClient();
  const repo = new SqlProductRepo(db);
  const service = new ProductService(repo);
  return new ProductController(service);
}
```
</details>
