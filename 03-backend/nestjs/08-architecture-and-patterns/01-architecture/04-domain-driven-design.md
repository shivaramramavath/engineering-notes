---
title: "Domain-Driven Design (DDD) in NestJS"
description: "Implementing DDD tactical patterns: Entities, Value Objects, Aggregates, Domain Events, and Repositories in NestJS"
tags: [nestjs, architecture, ddd, domain-driven-design, aggregates, value-objects]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Domain-Driven Design (DDD) in NestJS

## Conceptual Overview
Domain-Driven Design (DDD) aligns software development with complex business domains. Tactical DDD provides concrete structural patterns:
- **Entities**: Objects possessing unique identity that persists across state changes (e.g. `User(id: "usr_123")`).
- **Value Objects**: Immutable objects defined solely by their attributes with no identity (e.g. `Money(amount: 100, currency: "USD")`).
- **Aggregates & Aggregate Roots**: Clusters of domain objects bound by transactional consistency boundaries. External access must go through the Aggregate Root.
- **Domain Events**: Records of significant domain occurrences (`OrderPlacedEvent`).

---

## Implementation: Pure Value Object

```typescript
// src/domain/value-objects/money.vo.ts
export class Money {
  constructor(
    public readonly amount: number,
    public readonly currency: string,
  ) {
    if (amount < 0) {
      throw new Error('Amount cannot be negative');
    }
  }

  equals(other: Money): boolean {
    return this.amount === other.amount && this.currency === other.currency;
  }

  add(other: Money): Money {
    if (this.currency !== other.currency) {
      throw new Error('Cannot add amounts with different currencies');
    }
    return new Money(this.amount + other.amount, this.currency);
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Enforce business invariants inside Aggregate Roots and Value Objects, not in controllers.
- Value Objects are immutable: operations return fresh instances.
- Never access entities inside an aggregate boundary directly from outside; route all calls through the Aggregate Root.

---

## Cross-References
- [03-clean-and-hexagonal-architecture.md](./03-clean-and-hexagonal-architecture.md) - Clean architecture
- [05-cqrs-and-event-sourcing.md](./05-cqrs-and-event-sourcing.md) - CQRS and event sourcing
