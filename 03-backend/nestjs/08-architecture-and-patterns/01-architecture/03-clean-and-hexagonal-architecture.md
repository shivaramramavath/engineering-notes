---
title: "Clean and Hexagonal Architecture in NestJS"
description: "Implementing Ports and Adapters, pure domain models, dependency inversion, and framework-free enterprise business logic in NestJS"
tags: [nestjs, architecture, clean-architecture, hexagonal, ports-and-adapters]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Clean and Hexagonal Architecture in NestJS

## Conceptual Overview
Hexagonal Architecture (Ports and Adapters) and Clean Architecture isolate the core business domain from external technical concerns (databases, web frameworks, message brokers, UI).

The fundamental rule: **Dependencies point strictly inward**.
The Domain Core has **zero dependencies** on NestJS, TypeORM, Prisma, or Express. It contains pure TypeScript entities and business logic.

```
+-------------------------------------------------------------------------------+
|                       Hexagonal Architecture (Ports & Adapters)               |
|                                                                               |
|  [ Driving / Primary Adapters ]                                               |
|  - HTTP Controller (NestJS)    \                                              |
|  - CLI Command                  --> [ Primary Port: OrderUseCase (Interface)] |
|  - Queue Consumer              /                      |                       |
|                                                       v                       |
|                                             [ Domain Core (Pure TS) ]         |
|                                             - Order Entity                    |
|                                             - Price Value Object              |
|                                                       |                       |
|  [ Driven / Secondary Adapters ]                      v                       |
|  - PostgresTypeOrmRepo         <-- [ Secondary Port: OrderRepository (Intf) ] |
|  - StripePaymentClient         <-- [ Secondary Port: PaymentGateway (Intf) ]  |
+-------------------------------------------------------------------------------+
```

---

## Implementation Details & NestJS APIs

### 1. Inverting Dependencies with Custom Providers

```typescript
// Core Port (Pure TypeScript interface, no NestJS decorators)
export interface OrderRepositoryPort {
  findById(id: string): Promise<Order | null>;
  save(order: Order): Promise<void>;
}
export const ORDER_REPOSITORY_PORT = Symbol('ORDER_REPOSITORY_PORT');

// Driven Adapter (NestJS & TypeORM implementation)
@Injectable()
export class PostgresOrderRepositoryAdapter implements OrderRepositoryPort {
  constructor(
    @InjectRepository(OrderOrmEntity)
    private readonly repo: Repository<OrderOrmEntity>,
  ) {}

  async findById(id: string): Promise<Order | null> {
    const entity = await this.repo.findOneBy({ id });
    return entity ? OrderMapper.toDomain(entity) : null;
  }

  async save(order: Order): Promise<void> {
    const ormEntity = OrderMapper.toOrm(order);
    await this.repo.save(ormEntity);
  }
}
```

```typescript
// Wiring the port to adapter in NestJS module
@Module({
  providers: [
    OrderUseCases,
    {
      provide: ORDER_REPOSITORY_PORT,
      useClass: PostgresOrderRepositoryAdapter,
    },
  ],
})
export class OrderModule {}
```

---

## Key Takeaways & Cheat Sheet

- Keep the Domain Core 100% free of `@Injectable()`, `@Entity()`, or third-party library imports.
- Use Symbols as injection tokens for Port interfaces.
- Adapters translate external data representations to pure domain entities via Mappers.

---

## Cross-References
- [04-domain-driven-design.md](./04-domain-driven-design.md) - Domain-Driven Design
- [01-layered-architecture.md](./01-layered-architecture.md) - Layered architecture
