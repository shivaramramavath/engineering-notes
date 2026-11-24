---
title: "CQRS and Event Sourcing in NestJS"
description: "Command Query Responsibility Segregation, read/write model separation, and event sourcing using @nestjs/cqrs"
tags: [nestjs, architecture, cqrs, event-sourcing, command-bus, query-bus]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# CQRS and Event Sourcing in NestJS

## Conceptual Overview
Command Query Responsibility Segregation (CQRS) separates data mutation operations (Commands) from data retrieval operations (Queries):
- **Commands**: State changes with business validations, returning void or status.
- **Queries**: Read-only projections optimized for UI consumption without mutating state.

**Event Sourcing** takes CQRS further: instead of storing current state in mutable database tables, every state transition is recorded as an immutable append-only event (`AccountOpened`, `MoneyDeposited`, `MoneyWithdrawn`). The current balance is derived by replaying past events.

NestJS provides the official `@nestjs/cqrs` package.

---

## Implementation Details & NestJS APIs

```typescript
// src/orders/commands/create-order.command.ts
export class CreateOrderCommand {
  constructor(
    public readonly customerId: string,
    public readonly items: Array<{ sku: string; quantity: number }>,
  ) {}
}

// Handler
@CommandHandler(CreateOrderCommand)
export class CreateOrderHandler implements ICommandHandler<CreateOrderCommand> {
  constructor(private readonly publisher: EventPublisher) {}

  async execute(command: CreateOrderCommand) {
    const order = this.publisher.mergeObjectContext(
      new OrderAggregate(command.customerId, command.items),
    );
    order.commit(); // Dispatches domain events to EventBus
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Use CQRS when read and write workloads have drastically different scaling or indexing requirements.
- Use CommandBus for write actions, QueryBus for read projections.
- Event sourcing guarantees an infallible audit trail at the expense of replay latency and projection management.

---

## Cross-References
- [05-cqrs-module.md](../../05-advanced/03-events-and-messaging/05-cqrs-module.md) - Advanced CQRS module
- [06-event-driven-architecture.md](./06-event-driven-architecture.md) - Event-driven systems
