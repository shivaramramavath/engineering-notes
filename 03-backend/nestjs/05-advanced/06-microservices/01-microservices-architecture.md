# Microservices Architecture Overview in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 06-microservices
- **Prerequisites**: [02-fundamentals/01-architecture-overview](../../02-fundamentals/01-architecture-overview.md), [05-advanced/03-events-and-messaging/02-messaging-fundamentals](../03-events-and-messaging/02-messaging-fundamentals.md)

---

## 1. Context & Purpose

In large organizations, monolithic backends encounter scaling limits: independent teams collide on shared codebases, deployment pipelines slow to hours, and scaling up requires replicating the entire application even if only one feature is bottlenecked.

**Microservices Architecture** decomposes a monolithic application into a suite of small, autonomous, independently deployable services organized around business capabilities (e.g. Users Service, Payment Service, Inventory Service). NestJS provides native microservice primitives via `@nestjs/microservices`, abstracting underlying communication protocols behind unified patterns while preserving full access to dependency injection, pipes, guards, and interceptors.

---

## 2. Theoretical Foundations

### 2.1 Core Microservice Tenets
- **Single Responsibility**: Each service models a bounded context (Domain-Driven Design).
- **Decentralized Data**: Each microservice strictly owns its own database. Service A must **never** query Service B's database directly.
- **Polyglot Communication**: High-speed internal communication (gRPC, TCP, RabbitMQ, Kafka) paired with external public gateways (HTTP REST, GraphQL).

### 2.2 Synchronous vs Asynchronous Communication
- **Synchronous (HTTP, gRPC, TCP)**: The caller waits for an immediate response. High coupling; network timeouts propagate across the call chain.
- **Asynchronous (Event-Driven Broker)**: The caller publishes an event and continues immediately. Loose coupling; high resilience against temporary service outages.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   Public Internet                      │
│                  Web & Mobile Clients                  │
└───────────────────────────┬────────────────────────────┘
                            │ (Public HTTPS / REST / GraphQL)
                            ▼
┌────────────────────────────────────────────────────────┐
│             API Gateway Service (NestJS)               │
│     - Authentication, Rate Limiting, SSL Termination   │
│     - Aggregates downstream microservice calls         │
└───────┬───────────────────┬────────────────────┬───────┘
        │ (gRPC)            │ (TCP / RPC)        │ (Kafka Events)
        ▼                   ▼                    ▼
┌──────────────┐    ┌──────────────┐     ┌──────────────┐
│ Auth Service │    │ Order Service│     │Analytics Svc │
│ (PostgreSQL) │    │ (PostgreSQL) │     │  (ClickHouse)│
└──────────────┘    └──────────────┘     └──────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Standalone Microservice Initialization

A microservice is initialized without an HTTP server listener via `NestFactory.createMicroservice()`:

```typescript
// src/main.ts (Standalone Orders Microservice)
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.createMicroservice<MicroserviceOptions>(AppModule, {
    transport: Transport.TCP,
    options: {
      host: '0.0.0.0',
      port: 3001,
    },
  });

  app.enableShutdownHooks(); // Ensure clean socket/connection closure
  await app.listen();
  console.log('Orders Microservice listening on TCP port 3001');
}
bootstrap();
```

---

### 4.2 Handling Messages with Patterns

```typescript
// src/orders/orders.controller.ts
import { Controller } from '@nestjs/common';
import { MessagePattern, EventPattern, Payload } from '@nestjs/microservices';
import { OrdersService } from './orders.service';

@Controller()
export class OrdersController {
  constructor(private readonly ordersService: OrdersService) {}

  // 1. Request-Response pattern (RPC): Caller expects a return value
  @MessagePattern({ cmd: 'get_order' })
  async getOrder(@Payload() data: { id: string }) {
    return this.ordersService.findById(data.id);
  }

  // 2. Event-driven pattern (Pub/Sub): Fire-and-forget
  @EventPattern('order_status_updated')
  async handleStatusUpdate(@Payload() data: { orderId: string; status: string }) {
    await this.ordersService.updateStatus(data.orderId, data.status);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Cascading Failures
If Service A synchronously calls Service B, which calls Service C, an outage in Service C cascades backward, exhausting connection pools across the entire chain.
- **Handling**: Implement **Circuit Breakers** (e.g. Cockatiel / Baresoil) and fallback responses.

### 5.2 Distributed Transaction Failures (Partial Success)
If Order Service creates an order, but Payment Service fails, the system enters an inconsistent state.
- **Handling**: Use the **Saga Pattern** (orchestrated or choreographed compensating transactions) instead of distributed 2-Phase Commit (2PC).

---

## 6. Performance & Memory Implications

- **Transport Overhead**: Internal TCP/gRPC communication consumes significantly less CPU and bandwidth than HTTP/1.1 REST, bypassing headers, cookies, and text encoding overhead.
- **Memory Footprint**: Microservices stripped of HTTP engines boot in $< 100\text{ms}$ and consume only 30–50 MB of RAM.

---

## 7. Anti-Patterns & Common Mistakes

- **Creating a "Distributed Monolith"**: Deploying 20 microservices that share the exact same PostgreSQL database instance. Schema changes require coordinated redeployments, defeating the entire purpose of microservices.
- **Synchronous Chaining for Everything**: Calling 6 microservices synchronously via HTTP in response to one user request.
- **Premature Decomposition**: Splitting a small 2-person project into 15 microservices before domain boundaries are understood.

---

## 8. Production Checklist & Operational Readiness

- [ ] Each microservice owns its private database schema.
- [ ] Correlation IDs forwarded across all inter-service headers.
- [ ] Centralized distributed tracing (OpenTelemetry / Jaeger) implemented.
- [ ] Circuit breakers applied to all synchronous RPC calls.
- [ ] Health checks reporting transporter status.

---

## 9. Migration & Version History

- **NestJS 11.x**: Added transporter `.unwrap()` support and unified reverse-topological shutdown lifecycle order.

---

## 10. Verification & Testing

E2E testing a TCP microservice:

```typescript
import { Test } from '@nestjs/testing';
import { ClientProxy, ClientsModule, Transport } from '@nestjs/microservices';
import { firstValueFrom } from 'rxjs';
import { AppModule } from './app.module';

describe('Microservice E2E', () => {
  let client: ClientProxy;

  beforeAll(async () => {
    const app = await Test.createTestingModule({
      imports: [AppModule],
    }).compile();

    const microservice = app.createNestMicroservice({
      transport: Transport.TCP,
      options: { port: 3002 },
    });
    await microservice.listen();

    const clientModule = await Test.createTestingModule({
      imports: [
        ClientsModule.register([
          { name: 'TEST_CLIENT', transport: Transport.TCP, options: { port: 3002 } },
        ]),
      ],
    }).compile();

    client = clientModule.get('TEST_CLIENT');
    await client.connect();
  });

  it('receives RPC message response', async () => {
    const res = await firstValueFrom(client.send({ cmd: 'get_order' }, { id: '1' }));
    expect(res).toBeDefined();
  });
});
```

---

## 11. Comprehensive Examples

### Circuit Breaker Wrapper Pattern

```typescript
// src/common/resilience/circuit-breaker.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { ClientProxy } from '@nestjs/microservices';
import { firstValueFrom, timeout } from 'rxjs';

@Injectable()
export class ResilientClientService {
  private readonly logger = new Logger(ResilientClientService.name);
  private failures = 0;
  private openUntil = 0;

  async sendWithBreaker<T>(client: ClientProxy, pattern: any, data: any): Promise<T> {
    if (Date.now() < this.openUntil) {
      throw new Error('Circuit Breaker is OPEN. Downstream service unavailable.');
    }

    try {
      const result = await firstValueFrom(client.send<T>(pattern, data).pipe(timeout(3000)));
      this.failures = 0;
      return result;
    } catch (err) {
      this.failures++;
      if (this.failures >= 5) {
        this.openUntil = Date.now() + 30000; // Open circuit for 30s
        this.logger.error('Circuit breaker tripped to OPEN state for 30s');
      }
      throw err;
    }
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-transporters.md](./02-transporters.md) - Deep dive into transporters
- [03-message-patterns.md](./03-message-patterns.md) - Message patterns
- [05-hybrid-apps-and-api-gateway.md](./05-hybrid-apps-and-api-gateway.md) - API Gateway architecture
- [Martin Fowler on Microservices](https://martinfowler.com/articles/microservices.html)
