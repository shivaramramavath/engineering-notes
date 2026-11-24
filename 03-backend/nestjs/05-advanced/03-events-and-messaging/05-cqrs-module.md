# CQRS Module and Event Sourcing Patterns

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 03-events-and-messaging
- **Prerequisites**: [01-event-emitter](./01-event-emitter.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md), [00-prerequisites/07-oop-and-solid-primer](../../00-prerequisites/07-oop-and-solid-primer.md)

---

## 1. Context & Purpose

In complex enterprise domain models, attempting to use the same database entities, repositories, and services for both read and write operations creates tangled code. Read operations demand denormalized, fast, joined aggregations for user interfaces, while write operations demand strict validation, domain invariants, aggregate roots, and atomic transactions.

**Command Query Responsibility Segregation (CQRS)** separates read operations (**Queries**) from write operations (**Commands**). Through **`@nestjs/cqrs`**, NestJS provides dedicated buses for Commands (`CommandBus`), Queries (`QueryBus`), and Events (`EventBus` / `EventPublisher`), creating clean hexagonal domain architectures and enabling Event Sourcing.

---

## 2. Theoretical Foundations

### 2.1 The CQRS Separation
- **Commands**: Requests to mutate state (`CreateOrderCommand`, `CancelSubscriptionCommand`). Commands represent intent, are named in the imperative, and execute via a single `CommandHandler`.
- **Queries**: Requests to read state (`GetOrderByIdQuery`, `ListUsersQuery`). Queries return DTOs without mutating data and execute via a single `QueryHandler`.
- **Events**: Notifications that a domain state mutation occurred (`OrderCreatedEvent`, `OrderCancelledEvent`). Named in past tense, broadcast to multiple `EventHandlers`.
- **Sagas**: Long-running business transactions coordinating multiple commands in response to event streams (using RxJS reactive streams).

---

## 3. Architecture & Internal Mechanics

```
                  ┌────────────────────────────────────────┐
                  │          Controller Layer              │
                  └───────────┬────────────────┬───────────┘
                              │                │
            execute(command)  │                │ execute(query)
                              ▼                ▼
┌───────────────────────────────────┐ ┌───────────────────────────────────┐
│            CommandBus             │ │             QueryBus              │
│    CreateOrderCommandHandler      │ │      GetOrderByIdQueryHandler     │
├───────────────────────────────────┤ ├───────────────────────────────────┤
│ - Enforces Domain Invariants      │ │ - Read-Only SQL / ElasticSearch   │
│ - Mutates Write DB (Relational)   │ │ - Fast, Denormalized JSON DTO     │
│ - Emits OrderCreatedEvent         │ └───────────────────────────────────┘
└─────────────────┬─────────────────┘
                  │
                  ▼
┌───────────────────────────────────┐
│             EventBus              │
├───────────────────────────────────┤
│ EventHandler 1: Update Read Cache │
│ EventHandler 2: Send Email Alert  │
│ Saga: Trigger Shipping Workflow   │
└───────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Installing `@nestjs/cqrs`

```bash
npm install --save @nestjs/cqrs
```

### 4.2 Registering `CqrsModule`

```typescript
// src/orders/orders.module.ts
import { Module } from '@nestjs/common';
import { CqrsModule } from '@nestjs/cqrs';
import { OrdersController } from './orders.controller';
import { CreateOrderHandler } from './commands/create-order.handler';
import { GetOrderHandler } from './queries/get-order.handler';
import { OrderCreatedHandler } from './events/order-created.handler';
import { OrdersSaga } from './sagas/orders.saga';

@Module({
  imports: [CqrsModule],
  controllers: [OrdersController],
  providers: [
    CreateOrderHandler,
    GetOrderHandler,
    OrderCreatedHandler,
    OrdersSaga,
  ],
})
export class OrdersModule {}
```

---

### 4.3 Implementing a Command and CommandHandler

```typescript
// src/orders/commands/create-order.command.ts
export class CreateOrderCommand {
  constructor(
    public readonly customerId: string,
    public readonly amount: number,
  ) {}
}
```

```typescript
// src/orders/commands/create-order.handler.ts
import { CommandHandler, ICommandHandler, EventPublisher } from '@nestjs/cqrs';
import { CreateOrderCommand } from './create-order.command';
import { OrderAggregate } from '../aggregates/order.aggregate';

@CommandHandler(CreateOrderCommand)
export class CreateOrderHandler implements ICommandHandler<CreateOrderCommand> {
  constructor(private readonly publisher: EventPublisher) {}

  async execute(command: CreateOrderCommand): Promise<string> {
    const orderId = 'ord_' + Date.now();

    // Instantiate and merge domain aggregate with EventPublisher
    const order = this.publisher.mergeObjectContext(
      new OrderAggregate(orderId),
    );

    // Execute domain logic on aggregate
    order.createOrder(command.customerId, command.amount);

    // Commit publishes all uncommitted events to the EventBus automatically!
    order.commit();

    return orderId;
  }
}
```

---

### 4.4 The Domain Aggregate Root

```typescript
// src/orders/aggregates/order.aggregate.ts
import { AggregateRoot } from '@nestjs/cqrs';
import { OrderCreatedEvent } from '../events/order-created.event';

export class OrderAggregate extends AggregateRoot {
  constructor(private readonly id: string) {
    super();
  }

  createOrder(customerId: string, amount: number) {
    if (amount <= 0) {
      throw new Error('Order amount must be positive');
    }

    // Apply event internally and stage for publishing
    this.apply(new OrderCreatedEvent(this.id, customerId, amount));
  }
}
```

---

### 4.5 Implementing an Event and EventHandler

```typescript
// src/orders/events/order-created.event.ts
export class OrderCreatedEvent {
  constructor(
    public readonly orderId: string,
    public readonly customerId: string,
    public readonly amount: number,
  ) {}
}
```

```typescript
// src/orders/events/order-created.handler.ts
import { EventsHandler, IEventHandler } from '@nestjs/cqrs';
import { OrderCreatedEvent } from './order-created.event';
import { Logger } from '@nestjs/common';

@EventsHandler(OrderCreatedEvent)
export class OrderCreatedHandler implements IEventHandler<OrderCreatedEvent> {
  private readonly logger = new Logger(OrderCreatedHandler.name);

  handle(event: OrderCreatedEvent) {
    this.logger.log(
      `[Event Log] Order ${event.orderId} created for Customer ${event.customerId}. Updating read-models...`,
    );
    // Update read-model projection or trigger side-effects
  }
}
```

---

### 4.6 Implementing Sagas (Long-Running Workflows with RxJS)

A Saga listens for incoming events on the event stream and returns an Observable that emits a new Command:

```typescript
// src/orders/sagas/orders.saga.ts
import { Injectable, Logger } from '@nestjs/common';
import { ICommand, ofType, Saga } from '@nestjs/cqrs';
import { Observable } from 'rxjs';
import { map, delay } from 'rxjs/operators';
import { OrderCreatedEvent } from '../events/order-created.event';

class SendShippingNotificationCommand implements ICommand {
  constructor(public readonly orderId: string) {}
}

@Injectable()
export class OrdersSaga {
  private readonly logger = new Logger(OrdersSaga.name);

  @Saga()
  orderCreated = (events$: Observable<any>): Observable<ICommand> => {
    return events$.pipe(
      ofType(OrderCreatedEvent),
      delay(1000), // Simulate asynchronous delay before next action
      map((event) => {
        this.logger.log(`[Saga Trigger] Scheduling shipping for order: ${event.orderId}`);
        // Transforms the event into a new command!
        return new SendShippingNotificationCommand(event.orderId);
      }),
    );
  };
}
```

---

### 4.7 Consuming Commands and Queries in Controller

```typescript
// src/orders/orders.controller.ts
import { Controller, Post, Get, Body, Param } from '@nestjs/common';
import { CommandBus, QueryBus } from '@nestjs/cqrs';
import { CreateOrderCommand } from './commands/create-order.command';
import { GetOrderQuery } from './queries/get-order.query';

@Controller('orders')
export class OrdersController {
  constructor(
    private readonly commandBus: CommandBus,
    private readonly queryBus: QueryBus,
  ) {}

  @Post()
  async create(@Body() body: { customerId: string; amount: number }) {
    // Dispatch to CommandBus
    const orderId = await this.commandBus.execute(
      new CreateOrderCommand(body.customerId, body.amount),
    );
    return { orderId, status: 'ORDER_PLACED' };
  }

  @Get(':id')
  async findOne(@Param('id') id: string) {
    // Dispatch to QueryBus
    return this.queryBus.execute(new GetOrderQuery(id));
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Multiple Handlers for One Command
Commands represent a strict one-to-one mapping. Registering two `@CommandHandler` classes for the same command throws an unhandled exception at bootstrap.
- **Rule**: A Command can have exactly **one** handler. An Event can have **many** handlers.

### 5.2 Eventual Consistency Read Latency
If the QueryBus queries a denormalized read database (e.g., Elasticsearch or MongoDB read replica) that updates asynchronously via `OrderCreatedEvent`, a client calling `GET /orders/:id` immediately after `POST /orders` might receive a 404.
- **Handling**: Return the generated aggregate ID immediately and configure client UI to handle eventual consistency, or read recently created entities directly from the primary DB.

---

## 6. Performance & Memory Implications

- **Decoupled Read Scaling**: Query handlers can target optimized read-replica databases, caching tiers, or materialized views without imposing locks on the transactional write database.
- **In-Memory Dispatch**: The CommandBus and QueryBus dispatch calls via internal class token maps in microseconds ($< 0.05\text{ms}$).

---

## 7. Anti-Patterns & Common Mistakes

- **Applying CQRS to Trivial CRUD Apps**: Introducing CommandBus, QueryBus, and Sagas for a simple 3-table CRUD app. CQRS adds architectural overhead and should be reserved for complex domain models.
- **Mutating Data Inside Query Handlers**: Running SQL `UPDATE` statements inside a `QueryHandler`. Queries must be pure, idempotent, read-only operations.
- **Executing Side-Effects in Aggregate Roots**: Calling payment APIs directly inside `OrderAggregate.createOrder()`. Aggregate roots must encapsulate pure domain business logic and state transitions; external I/O belongs in handlers or Sagas.

---

## 8. Production Checklist & Operational Readiness

- [ ] Clear naming conventions enforced: Imperative for Commands (`CreateOrder`), Past-Tense for Events (`OrderCreated`).
- [ ] Aggregate roots contain pure domain invariants (no external I/O).
- [ ] Sagas manage cross-boundary distributed transactions.
- [ ] Read projections handle out-of-order events idempotently.

---

## 9. Migration & Version History

- **NestJS 7/8/9**: Standardized CQRS module and `EventPublisher.mergeObjectContext()`.
- **NestJS 11.x**: Full RxJS 7.8 compatibility for Sagas and native SWC constructor metadata reflection.

---

## 10. Verification & Testing

Unit testing a CommandHandler:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { CreateOrderHandler } from './commands/create-order.handler';
import { EventPublisher } from '@nestjs/cqrs';
import { CreateOrderCommand } from './commands/create-order.command';

describe('CreateOrderHandler', () => {
  let handler: CreateOrderHandler;
  let mockPublisher: any;

  beforeEach(async () => {
    mockPublisher = {
      mergeObjectContext: vi.fn((obj) => {
        obj.commit = vi.fn();
        return obj;
      }),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        CreateOrderHandler,
        { provide: EventPublisher, useValue: mockPublisher },
      ],
    }).compile();

    handler = module.get<CreateOrderHandler>(CreateOrderHandler);
  });

  it('executes command, applies domain rules and commits events', async () => {
    const cmd = new CreateOrderCommand('cust_101', 500);
    const orderId = await handler.execute(cmd);

    expect(orderId).toBeDefined();
    expect(mockPublisher.mergeObjectContext).toHaveBeenCalled();
  });
});
```

---

## 11. Comprehensive Examples

### Implementing a Query and QueryHandler

```typescript
// src/orders/queries/get-order.query.ts
export class GetOrderQuery {
  constructor(public readonly orderId: string) {}
}
```

```typescript
// src/orders/queries/get-order.handler.ts
import { IQueryHandler, QueryHandler } from '@nestjs/cqrs';
import { GetOrderQuery } from './get-order.query';
import { NotFoundException } from '@nestjs/common';
import { DataSource } from 'typeorm';

export interface OrderViewDto {
  id: string;
  customerName: string;
  totalAmount: number;
}

@QueryHandler(GetOrderQuery)
export class GetOrderHandler implements IQueryHandler<GetOrderQuery, OrderViewDto> {
  constructor(private readonly dataSource: DataSource) {}

  async execute(query: GetOrderQuery): Promise<OrderViewDto> {
    // Pure, optimized read query with fast join
    const [result] = await this.dataSource.query(
      `SELECT o.id, u.name as "customerName", o.total_amount as "totalAmount"
       FROM orders o
       JOIN users u ON u.id = o.customer_id
       WHERE o.id = $1`,
      [query.orderId],
    );

    if (!result) {
      throw new NotFoundException(`Order #${query.orderId} not found`);
    }

    return result;
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-event-emitter.md](./01-event-emitter.md) - In-process event bus
- [02-messaging-fundamentals.md](./02-messaging-fundamentals.md) - Distributed messaging
- [08-architecture-and-patterns/01-architecture/03-clean-architecture.md](../../08-architecture-and-patterns/01-architecture/03-clean-architecture.md) - Hexagonal and Clean Architecture
- [NestJS CQRS Documentation](https://docs.nestjs.com/recipes/cqrs)
