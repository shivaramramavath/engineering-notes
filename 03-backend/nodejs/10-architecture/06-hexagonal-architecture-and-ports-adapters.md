# Hexagonal Architecture (Ports & Adapters) in Node.js

> Decoupling core business logic from framework and infrastructure concerns using Ports (Interfaces) and Adapters (Implementations).

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Pattern** | Ports and Adapters (Alistair Cockburn) |
| **Level** | Enterprise Architecture |

---

## 1. Architectural Philosophy

The Hexagonal Architecture places the domain model and application business logic inside an isolated core:
- **Inside the Hexagon**: Domain Entities, Value Objects, Use Cases, and Port Interfaces. Pure TypeScript/JavaScript with zero dependencies on Express, PostgreSQL, or AWS.
- **Outside the Hexagon**: Adapters that talk to external systems (HTTP controllers, SQL databases, Redis, message queues, third-party APIs).
- **Dependency Rule**: Dependencies always point **inward** toward the domain. The domain never imports an adapter.

```
+-------------------------------------------------------------------------+
|                              The Hexagon                                |
|                                                                         |
|  [ Inbound Adapter ]                                                    |
|  (Express/Fastify) ──(Calls)──> [ Inbound Port (Use Case Interface) ]   |
|                                                │                        |
|                                                ▼                        |
|                                      [ Core Domain Logic ]              |
|                                                │                        |
|  [ Outbound Adapter ] <──(Implements)─ [ Outbound Port (Repository) ]  |
|  (PostgreSQL / pg)                                                      |
+-------------------------------------------------------------------------+
```

---

## 2. Code Implementation

### Inbound & Outbound Ports (`domain/ports.ts`)
```typescript
// Outbound Port (What the domain needs from storage)
export interface OrderRepositoryPort {
  save(order: Order): Promise<void>;
  findById(id: string): Promise<Order | null>;
}

// Inbound Port (What external callers can execute)
export interface PlaceOrderUseCasePort {
  execute(command: { userId: string; items: OrderItem[] }): Promise<Order>;
}
```

### Domain Core Service (`domain/services/PlaceOrderService.ts`)
```typescript
import type { OrderRepositoryPort, PlaceOrderUseCasePort } from '../ports.ts';

export class PlaceOrderService implements PlaceOrderUseCasePort {
  // Dependency Injection via constructor
  constructor(private readonly orderRepo: OrderRepositoryPort) {}

  async execute({ userId, items }) {
    // Pure business validation
    const order = new Order({ userId, items, status: 'pending' });
    await this.orderRepo.save(order);
    return order;
  }
}
```

### Outbound Adapter (`infrastructure/PostgresOrderRepository.ts`)
```typescript
import type { OrderRepositoryPort } from '../domain/ports.ts';
import { db } from './db.ts';

export class PostgresOrderRepository implements OrderRepositoryPort {
  async save(order: Order): Promise<void> {
    await db.query('INSERT INTO orders (id, user_id, status) VALUES ($1, $2, $3)', [
      order.id, order.userId, order.status
    ]);
  }
}
```
You can now swap PostgreSQL for MongoDB or an in-memory test double by writing a new adapter without changing a single line of domain logic!
