# Domain-Driven Design (DDD) in Node.js

> Implementing DDD tactical patterns: Entities, Value Objects, Aggregates, Domain Events, and Repositories in TypeScript.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Concepts** | Ubiquitous Language, Bounded Contexts, Aggregates, Value Objects |

---

## 1. Tactical DDD Building Blocks

- **Entity**: An object defined by its persistent identity (`userId`), whose attributes may change over time.
- **Value Object**: An immutable object defined entirely by its structural attributes (e.g. `Money`, `EmailAddress`). Two value objects with identical properties are equal.
- **Aggregate Root**: The parent entity that guards business invariants across an entire cluster of related entities (e.g. `Order` contains `OrderItems`; external callers can only mutate `OrderItems` through the `Order` root).
- **Domain Event**: A record representing something meaningful that occurred in the domain (`OrderPlaced`, `PaymentFailed`).

---

## 2. Implementing Value Objects & Aggregates

### Value Object: `Money`
```typescript
export class Money {
  constructor(public readonly amountInCents: number, public readonly currency: string = 'USD') {
    if (amountInCents < 0) throw new Error('Money amount cannot be negative');
  }

  add(other: Money): Money {
    if (this.currency !== other.currency) throw new Error('Currency mismatch');
    return new Money(this.amountInCents + other.amountInCents, this.currency);
  }

  equals(other: Money): boolean {
    return this.amountInCents === other.amountInCents && this.currency === other.currency;
  }
}
```

### Aggregate Root: `Order`
```typescript
export class Order {
  private readonly items: OrderItem[] = [];

  constructor(public readonly id: string, public readonly customerId: string) {}

  addItem(productId: string, unitPrice: Money, quantity: number) {
    if (quantity <= 0) throw new Error('Quantity must be greater than zero');
    // Enforce aggregate invariant: maximum 50 items per order
    if (this.items.length >= 50) throw new Error('Order exceeds max items limit');

    this.items.push(new OrderItem(productId, unitPrice, quantity));
  }

  calculateTotal(): Money {
    return this.items.reduce(
      (acc, item) => acc.add(item.subtotal()),
      new Money(0)
    );
  }
}
```
Invariants are strictly protected within the domain model rather than scattered across controllers or SQL queries.
