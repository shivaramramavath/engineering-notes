# Hybrid Applications: HTTP and Microservices Combined

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 02-fundamentals
- **Prerequisites**: [01-architecture-overview](./01-architecture-overview.md), [02-modules](./02-modules.md), [00-prerequisites/03-nodejs-async-and-event-loop](../00-prerequisites/03-nodejs-async-and-event-loop.md)

---

## 1. Context & Purpose

In modern cloud architectures, services rarely serve only one communication channel. A single backend service often needs to serve **HTTP REST/GraphQL APIs** to web and mobile clients while simultaneously listening for messages on a **Message Broker** (e.g., RabbitMQ, Kafka, Redis Pub/Sub, NATS, or gRPC) for asynchronous inter-service coordination.

In traditional frameworks, developers are forced to run two separate processes or wire complex custom event listeners. NestJS provides native **Hybrid Applications**. A hybrid application binds an HTTP listener (Express or Fastify) and one or more microservice transporters to the same unified DI container, sharing database connections, configuration, and domain services with zero duplication.

---

## 2. Theoretical Foundations

Communication patterns fall broadly into two paradigms:
1. **Synchronous Request-Response (HTTP/REST)**: Client blocks waiting for an immediate answer. Ideal for user interfaces, mobile apps, and public APIs.
2. **Asynchronous Event-Driven (Message Brokers)**: Decoupled producer-consumer messages. Ideal for background workloads, distributed workflows, audit logs, and cross-service synchronization.

A NestJS **Hybrid Application** merges both paradigms into a single unified runtime process.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│               NestJS Unified Application Context       │
│         (Shared Services, Database, Config, DI)        │
└──────────────┬──────────────────────────┬──────────────┘
               │                          │
               ▼                          ▼
 ┌───────────────────────────┐ ┌───────────────────────────┐
 │   HTTP Server Listener    │ │   Microservice Transporter│
 │      (Express 5 / Port)   │ │    (Redis PubSub / TCP)   │
 ├───────────────────────────┤ ├───────────────────────────┤
 │ @Controller('orders')     │ │ @MessagePattern('order.*')│
 │ @Get(':id')               │ │ @EventPattern('order.pay')│
 └───────────────────────────┘ └───────────────────────────┘
               │                          │
               ▼                          ▼
         HTTP Clients               Message Broker
     (Web / Mobile Apps)          (RabbitMQ / Kafka)
```

Both listeners share the same singleton instances: when a microservice event updates a record via `OrdersService`, an HTTP request hitting `OrdersController` immediately sees the updated state.

---

## 4. Concrete Implementation

### 4.1 Installing Microservice Dependencies

Install `@nestjs/microservices`:
```bash
npm install --save @nestjs/microservices
```

---

### 4.2 Bootstrapping a Hybrid Application in `main.ts`

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import { AppModule } from './app.module';

async function bootstrap() {
  // Step 1: Create standard HTTP application
  const app = await NestFactory.create(AppModule);

  // Step 2: Connect a microservice transporter to the HTTP instance
  app.connectMicroservice<MicroserviceOptions>({
    transport: Transport.REDIS,
    options: {
      host: process.env.REDIS_HOST || 'localhost',
      port: Number(process.env.REDIS_PORT) || 6379,
    },
  });

  // Step 3: Start all connected microservice listeners
  await app.startAllMicroservices();

  // Step 4: Listen for incoming HTTP traffic
  await app.listen(3000);
  console.log('Hybrid Application listening on HTTP port 3000 and Redis transporter.');
}
bootstrap();
```

---

### 4.3 Handling Both HTTP and Message Patterns in One Controller

```typescript
// src/orders/orders.controller.ts
import { Controller, Get, Post, Body, Param } from '@nestjs/common';
import { MessagePattern, EventPattern, Payload } from '@nestjs/microservices';
import { OrdersService } from './orders.service';

@Controller('orders')
export class OrdersController {
  constructor(private readonly ordersService: OrdersService) {}

  // 1. HTTP Endpoint: Read order details
  @Get(':id')
  async getOrder(@Param('id') id: string) {
    return this.ordersService.findById(id);
  }

  // 2. HTTP Endpoint: Create order
  @Post()
  async createOrder(@Body() dto: any) {
    return this.ordersService.create(dto);
  }

  // 3. Microservice RPC: Request-Response message pattern
  @MessagePattern('orders.calculate_tax')
  calculateTax(@Payload() data: { amount: number; state: string }) {
    return this.ordersService.computeTax(data.amount, data.state);
  }

  // 4. Microservice Event: Fire-and-forget event pattern
  @EventPattern('payment.completed')
  async handlePaymentCompleted(@Payload() event: { orderId: string; amount: number }) {
    await this.ordersService.markAsPaid(event.orderId);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Forgetting `startAllMicroservices()`
Calling `app.connectMicroservice(...)` without calling `await app.startAllMicroservices()` registers the transporter configuration but never initiates connection to the broker. The HTTP server runs normally, but messages from the broker are never consumed.

### 5.2 Transporter Connection Outages at Startup
If the message broker (e.g. RabbitMQ or Redis) is temporarily down during application startup, `app.startAllMicroservices()` can throw an unhandled connection error, terminating the entire process and preventing HTTP traffic from serving health checks.
- **Handling**: Configure retry reconnect options or verify broker connectivity before launching.

---

## 6. Performance & Memory Implications

- **Resource Efficiency**: Running HTTP and broker consumers in the same Node.js process saves 30–50% memory compared to maintaining separate container deployments, sharing database connection pools and cache clients.
- **Event Loop Congestion**: If an incoming microservice batch processes millions of events synchronously, it can block the event loop, causing HTTP API latency spikes. For heavy event pipelines, separate worker pods are recommended.

---

## 7. Anti-Patterns & Common Mistakes

- **Starting Separate Processes for Minor Event Subscriptions**: Deploying an entirely new microservice pod and repository just to listen to one Redis event, duplicating ORM configurations and schemas.
- **Long-Running Blocking Operations in Shared Handlers**: Running CPU-intensive image processing or data exports directly inside hybrid microservice handlers, starving HTTP endpoints.
- **Mixing Exception Handlers**: Assuming HTTP exception filters handle microservice transporter errors. Microservice handlers require `RpcException` and `@UseFilters(new ExceptionFilter())` tailored for RPC contexts.

---

## 8. Production Checklist & Operational Readiness

- [ ] `await app.startAllMicroservices()` called before `app.listen()`.
- [ ] Transporter reconnect options and dead-letter queues (DLQ) configured on message brokers.
- [ ] Both HTTP and Microservice shutdown hooks enabled (`app.enableShutdownHooks()`).
- [ ] Health checks (`/health`) report connectivity for both HTTP and connected microservices.
- [ ] Correlation IDs propagated across both HTTP headers and message metadata.

---

## 9. Migration & Version History

- **NestJS 5.x**: Introduced the hybrid application architecture.
- **NestJS 9.x**: Enhanced multiple transporter bindings on a single hybrid app.
- **NestJS 11.x**: Added client transporter `.unwrap()` support and unified reverse shutdown hooks across both HTTP and RPC listeners.

---

## 10. Verification & Testing

Unit testing a hybrid controller:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { OrdersController } from './orders.controller';
import { OrdersService } from './orders.service';

describe('OrdersController (Hybrid)', () => {
  let controller: OrdersController;
  let service: OrdersService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      controllers: [OrdersController],
      providers: [
        {
          provide: OrdersService,
          useValue: {
            computeTax: vi.fn().mockReturnValue(15),
            markAsPaid: vi.fn().mockResolvedValue(true),
          },
        },
      ],
    }).compile();

    controller = module.get<OrdersController>(OrdersController);
    service = module.get<OrdersService>(OrdersService);
  });

  it('handles microservice message pattern orders.calculate_tax', () => {
    const result = controller.calculateTax({ amount: 100, state: 'CA' });
    expect(result).toBe(15);
  });
});
```

---

## 11. Comprehensive Examples

### Multi-Transporter Hybrid Setup (HTTP + TCP + Redis)

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // Transporter 1: Internal TCP for low-latency RPC
  app.connectMicroservice<MicroserviceOptions>({
    transport: Transport.TCP,
    options: { port: 3001 },
  });

  // Transporter 2: Redis for asynchronous events
  app.connectMicroservice<MicroserviceOptions>({
    transport: Transport.REDIS,
    options: { host: 'localhost', port: 6379 },
  });

  await app.startAllMicroservices();
  await app.listen(3000);
}
bootstrap();
```

---

## 12. Cross-References & Further Reading

- [01-architecture-overview.md](./01-architecture-overview.md) - Application architecture
- [05-advanced/06-microservices/01-microservices-overview.md](../05-advanced/06-microservices/01-microservices-overview.md) - Deep dive into microservices
- [05-advanced/03-events-and-messaging/01-event-emitter.md](../05-advanced/03-events-and-messaging/01-event-emitter.md) - In-process event bus
- [NestJS Hybrid Application Documentation](https://docs.nestjs.com/faq/hybrid-application)
