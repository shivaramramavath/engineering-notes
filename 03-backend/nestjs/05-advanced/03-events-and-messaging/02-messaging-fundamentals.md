# Distributed Messaging Fundamentals

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 03-events-and-messaging
- **Prerequisites**: [01-event-emitter](./01-event-emitter.md), [05-advanced/02-background-processing/01-queues-fundamentals](../02-background-processing/01-queues-fundamentals.md)

---

## 1. Context & Purpose

As backend architectures evolve from monolithic codebases to distributed microservices, services must communicate across physical networks and server boundaries. Direct synchronous HTTP/REST communication between microservices creates tight coupling, cascading failures, latency bloat, and poor fault tolerance.

**Distributed Messaging** enables microservices to communicate asynchronously via intermediary message brokers (RabbitMQ, Apache Kafka, NATS, Redis Pub/Sub, AWS SQS/SNS). Through `@nestjs/microservices`, NestJS provides a unified abstraction layer supporting multiple transporters, standardizing request-response patterns and event-driven architectures across enterprise systems.

---

## 2. Theoretical Foundations

Distributed messaging operates across two fundamental paradigms:

### 2.1 Request-Response (RPC) vs Event-Driven (Pub/Sub)
1. **Request-Response (Point-to-Point / RPC)**:
   - Client sends a command (`orders.calculate_tax`) and expects a specific return payload.
   - Handled in NestJS via `@MessagePattern('pattern')`.
2. **Event-Driven (Publish-Subscribe)**:
   - Producer emits a broadcast notification (`order.created`) with no expectation of an immediate reply.
   - Handled in NestJS via `@EventPattern('pattern')`.

### 2.2 Broker Paradigms: Smart Broker vs Dumb Broker
- **Smart Broker / Dumb Consumer (e.g. RabbitMQ)**: The broker manages message routing (exchanges, queues), delivery state, acknowledgments, and worker load balancing. Messages are deleted once acknowledged.
- **Dumb Broker / Smart Consumer (e.g. Apache Kafka)**: The broker is an immutable append-only distributed commit log. Consumers track their own read position (offset). Messages are retained for days or weeks, enabling replayability and event streaming.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│              Order Microservice (Producer)             │
│   client.send('get_order', { id: 1 })   (RPC Pattern)  │
│   client.emit('order_created', payload) (Event Pattern)│
└───────────────────────────┬────────────────────────────┘
                            │
               Message Broker (RabbitMQ / Kafka)
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
┌───────────────────────────┐ ┌───────────────────────────┐
│  Payment Service (RPC)    │ │  Shipping Service (Event) │
│  @MessagePattern(...)     │ │  @EventPattern(...)       │
│  Returns: { status: 'OK' }│ │  Fire-and-forget          │
└───────────────────────────┘ └───────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Installing Microservices Package

```bash
npm install --save @nestjs/microservices
```

---

### 4.2 Building a Transport-Agnostic Client Proxy

NestJS abstracts brokers behind `ClientProxy`:

```typescript
// src/orders/orders-client.module.ts
import { Module } from '@nestjs/common';
import { ClientsModule, Transport } from '@nestjs/microservices';

export const BILLING_SERVICE = 'BILLING_SERVICE';

@Module({
  imports: [
    ClientsModule.register([
      {
        name: BILLING_SERVICE,
        transport: Transport.TCP, // Easily switchable to Transport.RMQ or Transport.KAFKA!
        options: {
          host: '127.0.0.1',
          port: 3001,
        },
      },
    ]),
  ],
  exports: [ClientsModule],
})
export class OrdersClientModule {}
```

---

### 4.3 Using `client.send()` (Request-Response) vs `client.emit()` (Event)

```typescript
// src/orders/orders.service.ts
import { Injectable, Inject, Logger } from '@nestjs/common';
import { ClientProxy } from '@nestjs/microservices';
import { firstValueFrom } from 'rxjs';
import { BILLING_SERVICE } from './orders-client.module';

@Injectable()
export class OrdersService {
  private readonly logger = new Logger(OrdersService.name);

  constructor(@Inject(BILLING_SERVICE) private readonly billingClient: ClientProxy) {}

  // 1. Request-Response (RPC Pattern)
  async calculateTax(amount: number, state: string): Promise<number> {
    this.logger.log(`Calling remote billing service for tax calculation...`);

    // client.send() returns an RxJS Observable expecting a response
    const tax$ = this.billingClient.send<number>('billing.calculate_tax', {
      amount,
      state,
    });

    const taxAmount = await firstValueFrom(tax$);
    return taxAmount;
  }

  // 2. Publish-Subscribe (Event-Driven Pattern)
  async notifyOrderCreated(orderId: string, amount: number) {
    this.logger.log(`Broadcasting order.created event...`);

    // client.emit() is fire-and-forget; does not wait for a response
    this.billingClient.emit('order.created', {
      orderId,
      amount,
      createdAt: new Date().toISOString(),
    });
  }
}
```

---

### 4.4 Handling Patterns in the Remote Service Controller

```typescript
// src/billing/billing.controller.ts (Running on port 3001)
import { Controller } from '@nestjs/common';
import { MessagePattern, EventPattern, Payload } from '@nestjs/microservices';

@Controller()
export class BillingController {
  // Handles RPC request: MUST return a response
  @MessagePattern('billing.calculate_tax')
  calculateTax(@Payload() data: { amount: number; state: string }): number {
    const taxRate = data.state === 'CA' ? 0.0825 : 0.05;
    return Number((data.amount * taxRate).toFixed(2));
  }

  // Handles Event broadcast: No return response expected
  @EventPattern('order.created')
  handleOrderCreated(@Payload() data: any) {
    console.log(`[Event Received] Creating billing invoice for order: ${data.orderId}`);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Remote Service Timeout on `client.send()`
If the remote microservice hangs or crashes while processing `client.send()`, the calling service hangs indefinitely unless a timeout is applied.
- **Handling**: Always pipe RxJS `timeout` operator:
```typescript
import { timeout } from 'rxjs/operators';
const res = await firstValueFrom(this.client.send('pattern', data).pipe(timeout(5000)));
```

### 5.2 Network Serializer Mismatch
Different transporters serialize data differently (JSON, Buffers, Protobuf). Passing circular objects or un-serializable symbols results in corrupted packets.

---

## 6. Performance & Memory Implications

- **Connection Reuse**: `ClientProxy` maintains persistent TCP / broker connections, multiplexing requests across open channels without renegotiating TCP handshakes.
- **Throughput Comparison**: Kafka handles millions of events/sec via disk batching; RabbitMQ handles tens of thousands of messages/sec with complex routing; TCP provides the lowest raw latency for internal RPC.

---

## 7. Anti-Patterns & Common Mistakes

- **Using `client.send()` When No Response is Needed**: Forcing request-response roundtrips for fire-and-forget telemetry or notifications. Always use `client.emit()`.
- **Awaiting `client.emit()`**: Writing `await this.client.emit(...)` and expecting a return payload. `emit()` returns an Observable that completes with `void`.
- **Leaking RpcExceptions**: Throwing raw HTTP `BadRequestException` in microservice handlers. Microservices require `RpcException` to serialize errors cleanly across transporters.

---

## 8. Production Checklist & Operational Readiness

- [ ] RPC calls enforce strict client-side timeouts via RxJS `timeout()`.
- [ ] Correlation IDs injected into message headers to track requests across services.
- [ ] Microservice handlers throw `RpcException` rather than `HttpException`.
- [ ] Connection retry options configured on `ClientsModule`.

---

## 9. Migration & Version History

- **NestJS 8/9**: Standardized `ClientProxy` with modern RxJS 7.x operators.
- **NestJS 11.x**: Added `.unwrap()` method on microservice transporters to access native driver connections directly (e.g. raw Kafka or RabbitMQ channel).

---

## 10. Verification & Testing

Unit testing microservice client calls:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { OrdersService } from './orders.service';
import { BILLING_SERVICE } from './orders-client.module';
import { of } from 'rxjs';

describe('OrdersService (Microservice Client)', () => {
  let service: OrdersService;
  let mockClient: any;

  beforeEach(async () => {
    mockClient = {
      send: vi.fn().mockReturnValue(of(8.25)),
      emit: vi.fn(),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        OrdersService,
        { provide: BILLING_SERVICE, useValue: mockClient },
      ],
    }).compile();

    service = module.get<OrdersService>(OrdersService);
  });

  it('sends RPC message and resolves returned value', async () => {
    const tax = await service.calculateTax(100, 'CA');
    expect(tax).toBe(8.25);
    expect(mockClient.send).toHaveBeenCalledWith('billing.calculate_tax', {
      amount: 100,
      state: 'CA',
    });
  });
});
```

---

## 11. Comprehensive Examples

### Centralized Transporter Error Filter

```typescript
// src/common/filters/rpc-exception.filter.ts
import { Catch, RpcExceptionFilter, ArgumentsHost } from '@nestjs/common';
import { Observable, throwError } from 'rxjs';
import { RpcException } from '@nestjs/microservices';

@Catch(RpcException)
export class CustomRpcExceptionFilter implements RpcExceptionFilter<RpcException> {
  catch(exception: RpcException, host: ArgumentsHost): Observable<any> {
    const error = exception.getError();
    console.error('[RPC Error Encountered]', error);

    // Return standardized error payload across transporter
    return throwError(() => ({
      status: 'error',
      message: typeof error === 'string' ? error : (error as any).message,
      timestamp: new Date().toISOString(),
    }));
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-event-emitter.md](./01-event-emitter.md) - In-process events
- [03-kafka.md](./03-kafka.md) - Apache Kafka deep dive
- [04-rabbitmq.md](./04-rabbitmq.md) - RabbitMQ deep dive
- [NestJS Microservices Overview](https://docs.nestjs.com/microservices/basics)
