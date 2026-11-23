# Domain Modeling in TypeScript: Value Objects, Entities, and Aggregates

> Deep dive into Domain-Driven Design (DDD) in TypeScript: modeling Value Objects, Entities, Aggregates, domain invariants, nominal identity, and functional domain modeling.

| Metadata | Details |
| :--- | :--- |
| **Category** | Real-World Patterns |
| **Difficulty** | Advanced |
| **Prerequisites** | [24-best-practices/00-type-design-principles.md](../24-best-practices/00-type-design-principles.md), [10-advanced-types/01-nominal-typing.md](../10-advanced-types/01-nominal-typing.md) |
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

In traditional "anemic" domain models, business logic is separated from data: entities are plain property bags (`interface Order { items: Item[]; total: number }`), while validation, business rules, and state transitions are scattered across disparate service layers. This causes:
- Inconsistent state mutation (e.g., modifying order items without recalculating order totals).
- Primitives obsession (passing plain strings for money, currency, or email addresses).
- Broken domain invariants in concurrent workflows.

**Domain-Driven Design (DDD)** in TypeScript uses the type system to enforce business rules directly inside domain models:
- **Value Objects**: Immutable types defined solely by their values (e.g. `Money`, `EmailAddress`).
- **Entities**: Objects possessing unique identity that persist across state mutations (e.g. `User`, `Account`).
- **Aggregates**: Clusters of entities and value objects treated as a single transactional unit with a designated **Aggregate Root** protecting internal invariants.

---

## 2. Core Concepts

### Value Objects vs. Entities
- **Value Object**: No conceptual identity. Two value objects with identical properties are completely equal. They are strictly immutable.
- **Entity**: Defined by a unique identifier (`id`). Two entities with identical attributes but different IDs are distinct entities.

```
+-------------------------------------------------------------------------+
|                  Domain-Driven Design Model Hierarchy                   |
+-------------------------------------------------------------------------+
|                                                                         |
|                          [ Aggregate Root: Order ]                      |
|                          id: OrderId (Entity)                           |
|                          status: OrderStatus                            |
|                                     |                                   |
|                +--------------------+--------------------+              |
|                |                                         |              |
|                v                                         v              |
|     [ Entity: OrderLine ]                      [ Value Object: Money ]  |
|     lineId: LineId                             amount: 5000 (cents)     |
|     quantity: PositiveInteger                  currency: "USD"          |
|                                                                         |
|  Rule: External callers NEVER mutate OrderLine directly!                |
|  All operations route through Order.addItem(...)                        |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Nominal Branding for IDs**: To prevent mixing up different entity IDs (`UserId` vs `OrderId`), phantom branding attaches compile-time labels without runtime memory cost.
2. **Private Constructors & Factory Methods**: Hiding constructors forces instantiation through static factory methods (`Money.create(amount, currency)`), guaranteeing that invalid objects cannot be constructed.

---

## 4. Syntax & API Reference

### Brand Helper
```typescript
declare const __brand: unique symbol;
export type Brand<T, B extends string> = T & { readonly [__brand]: B };

export type OrderId = Brand<string, "OrderId">;
export type CustomerId = Brand<string, "CustomerId">;
```

### Immutable Value Object Implementation (`Money`)
```typescript
export class Money {
  private constructor(
    public readonly amountInCents: number,
    public readonly currency: "USD" | "EUR" | "GBP"
  ) {
    Object.freeze(this);
  }

  public static create(amountInCents: number, currency: "USD" | "EUR" | "GBP"): Money {
    if (!Number.isInteger(amountInCents) || amountInCents < 0) {
      throw new Error(`Invalid money amount: ${amountInCents}. Must be positive integer cents.`);
    }
    return new Money(amountInCents, currency);
  }

  public add(other: Money): Money {
    if (this.currency !== other.currency) {
      throw new Error(`Cannot add distinct currencies: ${this.currency} and ${other.currency}`);
    }
    return new Money(this.amountInCents + other.amountInCents, this.currency);
  }

  public equals(other: Money): boolean {
    return this.amountInCents === other.amountInCents && this.currency === other.currency;
  }
}
```

---

## 5. Practical Examples

### Example 1: Aggregate Root Enforcing Invariants
```typescript
import { Money } from "./money.js";
import { OrderId, CustomerId } from "./branding.js";

export interface OrderItem {
  readonly productId: string;
  readonly price: Money;
  readonly quantity: number;
}

export class OrderAggregate {
  private items: OrderItem[] = [];
  private status: "DRAFT" | "SUBMITTED" | "CANCELLED" = "DRAFT";

  public constructor(
    public readonly id: OrderId,
    public readonly customerId: CustomerId
  ) {}

  public addItem(productId: string, price: Money, quantity: number): void {
    if (this.status !== "DRAFT") {
      throw new Error("Cannot add items to non-draft order");
    }
    if (quantity <= 0) {
      throw new Error("Quantity must be positive");
    }
    this.items.push({ productId, price, quantity });
  }

  public calculateTotal(): Money {
    return this.items.reduce(
      (total, item) => total.add(Money.create(item.price.amountInCents * item.quantity, item.price.currency)),
      Money.create(0, "USD")
    );
  }

  public submit(): void {
    if (this.items.length === 0) {
      throw new Error("Cannot submit an empty order");
    }
    this.status = "SUBMITTED";
  }
}
```

### Example 2: Functional Value Object Pattern (No Classes)
```typescript
export interface Coordinate {
  readonly latitude: number;
  readonly longitude: number;
}

export function createCoordinate(lat: number, lng: number): Coordinate {
  if (lat < -90 || lat > 90) throw new Error("Latitude out of range [-90, 90]");
  if (lng < -180 || lng > 180) throw new Error("Longitude out of range [-180, 180]");
  return Object.freeze({ latitude: lat, longitude: lng });
}

export function areCoordinatesEqual(a: Coordinate, b: Coordinate): boolean {
  return a.latitude === b.latitude && a.longitude === b.longitude;
}
```

### Example 3: Edge Case - Discriminated State Transition Model
```typescript
export type Subscription =
  | { status: "TRIAL"; trialEndsAt: Date }
  | { status: "ACTIVE"; currentPeriodEnd: Date; paymentMethodId: string }
  | { status: "CANCELED"; canceledAt: Date };

export function renewSubscription(sub: Subscription): Subscription {
  if (sub.status !== "ACTIVE") {
    throw new Error("Only active subscriptions can be renewed");
  }
  return {
    ...sub,
    currentPeriodEnd: new Date(sub.currentPeriodEnd.getTime() + 30 * 86400000),
  };
}
```

### Example 4: Compiler Error & Distinct ID Rejection
```typescript
const orderId = "ord_101" as OrderId;
const customerId = "cust_505" as CustomerId;

function fetchOrderHistory(cust: CustomerId): void {
  console.log(`History for ${cust}`);
}

// @ts-expect-error - TS2345: Argument of type 'OrderId' is not assignable to parameter of type 'CustomerId'.
fetchOrderHistory(orderId);
```

### Example 5: Domain Event Dispatch Pattern
```typescript
export interface DomainEvent {
  readonly eventId: string;
  readonly occurredAt: Date;
  readonly aggregateId: string;
}

export interface OrderPlacedEvent extends DomainEvent {
  readonly totalCents: number;
}

export class OrderWithEvents {
  private events: DomainEvent[] = [];

  public placeOrder(orderId: string, total: Money): void {
    this.events.push({
      eventId: crypto.randomUUID(),
      occurredAt: new Date(),
      aggregateId: orderId,
      totalCents: total.amountInCents,
    } as OrderPlacedEvent);
  }

  public pullEvents(): readonly DomainEvent[] {
    const list = [...this.events];
    this.events = [];
    return list;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Direct Array Leaks in Aggregates**:
   Returning internal arrays directly (`return this.items`) allows callers to call `.push()` or `.splice()` externally, bypassing invariant checks. Always return `readonly` slices (`return [...this.items]`).
2. **Floating Point Rounding in Financial Models**:
   Never store money as floating point numbers (`amount: 19.99`). Always store money in integer cents or use BigInt to avoid IEEE 754 precision loss.
3. **Over-Modeling Value Objects**:
   Do not wrap every string in a class. Reserve Value Objects for concepts with business constraints or multi-property combinations.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Anemic Domain Model** | Entities with only getters/setters | Rich entities protecting business rules | Invariant violations across services |
| **Primitive Obsession** | Using `string` for all IDs | Branded ID types (`OrderId`) | Swapped parameters go undetected |
| **Floating Point Currency** | `price: 19.99` | `amountInCents: 1999` | Decimal arithmetic rounding errors |
| **Exposing Mutable State** | `getItems() { return this.items }` | Return `readonly OrderItem[]` copy | External tampering of aggregate state |
| **Public Constructors on VO** | Allowing `new Money(-500)` | Private constructor + factory method | Unvalidated invalid domain state |
| **Value Object Mutation** | Modifying value object property | Return a new instance on operation | Value objects must be immutable |

---

## 8. Performance & Type-Checker Complexity

- Nominal branding via phantom symbol properties adds zero runtime memory overhead.
- Returning shallow copies of aggregate items (`[...items]`) incurs minimal garbage collection impact while guaranteeing complete encapsulation.

---

## 9. Compiler Internals

When using branded Value Object types:
1. TypeScript's TypeChecker checks assignability by comparing symbols and property keys.
2. Because the phantom brand property (`[__brand]`) has no runtime existence, the compiler emits clean native JavaScript literals and standard function calls.

---

## 10. Best Practices & Design Guidelines

1. **Store Currency as Integer Smallest Units (Cents)**.
2. **Encapsulate Domain Invariants in Aggregate Roots**.
3. **Make Value Objects Strictly Immutable**: Use `Object.freeze` and `readonly` properties.
4. **Brand All Entity Identifiers**: Prevent identifier confusion.

---

## 11. Real-World Case Study

A subscription management platform encountered catastrophic billing anomalies: customers were double-billed or charged negative amounts because invoice generation was handled across 4 independent microservices modifying anemic database records.

**Domain Model Refactor**:
- Introduced `InvoiceAggregate` with private constructor and factory methods.
- Refactored `Money` into an immutable Value Object with integer cents and currency checks.
- Encapsulated invoice line addition and finalization inside the aggregate.

**Outcome**: Invoice calculation bugs dropped to **zero**, and financial audit compliance was achieved on the first pass.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between an Entity and a Value Object in DDD?</summary>

An Entity possesses a unique identity (like an ID) that remains constant even if all other attributes change. A Value Object has no identity and is defined exclusively by its properties; if its properties are equal, two value objects are considered identical. Value objects are always immutable.
</details>

<details>
<summary>2. What is an Aggregate Root and what responsibility does it hold?</summary>

An Aggregate Root is the primary entity within a cluster of domain objects (the aggregate). It acts as the single gateway for external operations, ensuring that all business invariants and consistency rules across the aggregate's internal entities and value objects are strictly maintained.
</details>

<details>
<summary>3. Why is Primitive Obsession dangerous in enterprise applications?</summary>

Primitive Obsession occurs when domain concepts (money, email, postal code, identifiers) are represented with raw primitive types (string, number). It allows invalid data to be assigned without validation and permits accidental swapping of identically typed parameters (e.g. passing a `CustomerId` to a function expecting an `OrderId`).
</details>

<details>
<summary>4. Why should money always be modeled as integer cents rather than floating point numbers?</summary>

JavaScript numbers are 64-bit IEEE 754 floating-point values. Floating-point arithmetic introduces rounding inaccuracies (e.g., `0.1 + 0.2 === 0.30000000000000004`). In financial applications, representing money in smallest integer units (e.g., cents, satoshis) eliminates rounding drift.
</details>

<details>
<summary>5. How can you ensure that an entity's internal state cannot be mutated externally?</summary>

By declaring internal properties as `private` and exposing state only through accessor methods that return readonly snapshots or defensive copies (`[...this.items]`), while performing mutations strictly via domain methods.
</details>

<details>
<summary>6. How do factory methods enforce domain invariants?</summary>

By making the constructor `private` and requiring instantiation through a static `create()` method, the factory can execute validation checks, throw errors, or return a `Result<T, Error>` before any instance is created, guaranteeing invalid objects cannot exist in memory.
</details>

<details>
<summary>7. What are Domain Events and why are they used?</summary>

Domain Events capture something meaningful that occurred in the domain (e.g. `OrderPlaced`, `PaymentFailed`). They allow aggregates to record state changes and notify external systems or other aggregates asynchronously without direct coupling.
</details>

<details>
<summary>8. Can you implement Value Objects in TypeScript without using classes?</summary>

Yes. Value Objects can be implemented using pure TypeScript interfaces with `readonly` properties combined with factory functions that call `Object.freeze()`, paired with standalone pure functions for operations and equality checks.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Create an immutable Temperature Value Object</summary>

**Task**: Implement a `Temperature` value object in Celsius that disallows values below absolute zero (-273.15°C).

**Solution**:
```typescript
export class Temperature {
  private constructor(public readonly celsius: number) {
    Object.freeze(this);
  }

  public static fromCelsius(celsius: number): Temperature {
    if (celsius < -273.15) {
      throw new Error("Temperature below absolute zero is impossible");
    }
    return new Temperature(celsius);
  }

  public toFahrenheit(): number {
    return (this.celsius * 9) / 5 + 32;
  }
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Brand CustomerId and ProductId</summary>

**Task**: Define branded types for `CustomerId` and `ProductId`.

**Solution**:
```typescript
declare const __brand: unique symbol;

export type CustomerId = string & { readonly [__brand]: "CustomerId" };
export type ProductId = string & { readonly [__brand]: "ProductId" };
```
</details>

<details>
<summary>Challenge 3 (Medium): Functional Value Object with Deep Equality</summary>

**Task**: Implement a functional 2D Vector value object with addition and equality functions.

**Solution**:
```typescript
export interface Vector2D {
  readonly x: number;
  readonly y: number;
}

export function createVector(x: number, y: number): Vector2D {
  return Object.freeze({ x, y });
}

export function addVectors(a: Vector2D, b: Vector2D): Vector2D {
  return createVector(a.x + b.x, a.y + b.y);
}

export function areVectorsEqual(a: Vector2D, b: Vector2D): boolean {
  return a.x === b.x && a.y === b.y;
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Bank Account Aggregate Invariant</summary>

**Task**: Implement a `BankAccount` aggregate with `deposit` and `withdraw` methods enforcing that balance never falls below zero.

**Solution**:
```typescript
export class BankAccount {
  private balanceCents = 0;

  constructor(public readonly accountId: string) {}

  public deposit(cents: number): void {
    if (cents <= 0) throw new Error("Deposit must be positive");
    this.balanceCents += cents;
  }

  public withdraw(cents: number): void {
    if (cents <= 0) throw new Error("Withdrawal must be positive");
    if (this.balanceCents - cents < 0) {
      throw new Error("Insufficient funds");
    }
    this.balanceCents -= cents;
  }

  public getBalance(): number {
    return this.balanceCents;
  }
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Generic Aggregate Root Base Class with Domain Events</summary>

**Task**: Implement an abstract `AggregateRoot<TId>` base class that accumulates and flushes domain events.

**Solution**:
```typescript
export interface DomainEvent {
  id: string;
  timestamp: Date;
  name: string;
}

export abstract class AggregateRoot<TId> {
  private _domainEvents: DomainEvent[] = [];

  constructor(public readonly id: TId) {}

  protected recordEvent(event: Omit<DomainEvent, "id" | "timestamp">): void {
    this._domainEvents.push({
      ...event,
      id: crypto.randomUUID(),
      timestamp: new Date(),
    });
  }

  public pullEvents(): readonly DomainEvent[] {
    const events = [...this._domainEvents];
    this._domainEvents = [];
    return events;
  }
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Exhaustive State Machine Entity</summary>

**Task**: Implement a typed Jira-like `Ticket` entity whose allowed transitions (`OPEN` -> `IN_PROGRESS` -> `RESOLVED` / `CLOSED`) are verified at compile time.

**Solution**:
```typescript
export type TicketState = "OPEN" | "IN_PROGRESS" | "RESOLVED" | "CLOSED";

export class Ticket {
  private constructor(
    public readonly id: string,
    private _state: TicketState
  ) {}

  public static create(id: string): Ticket {
    return new Ticket(id, "OPEN");
  }

  public startWork(): void {
    if (this._state !== "OPEN") {
      throw new Error(`Cannot start work on ticket in state ${this._state}`);
    }
    this._state = "IN_PROGRESS";
  }

  public resolve(): void {
    if (this._state !== "IN_PROGRESS") {
      throw new Error(`Cannot resolve ticket in state ${this._state}`);
    }
    this._state = "RESOLVED";
  }

  public close(): void {
    if (this._state !== "RESOLVED") {
      throw new Error(`Cannot close ticket before resolution`);
    }
    this._state = "CLOSED";
  }

  public get state(): TicketState {
    return this._state;
  }
}
```
</details>
