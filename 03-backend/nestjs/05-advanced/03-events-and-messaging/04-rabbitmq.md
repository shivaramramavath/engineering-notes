# RabbitMQ Integration in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 03-events-and-messaging
- **Prerequisites**: [02-messaging-fundamentals](./02-messaging-fundamentals.md), [05-advanced/02-background-processing/01-queues-fundamentals](../02-background-processing/01-queues-fundamentals.md)

---

## 1. Context & Purpose

**RabbitMQ** is the world's most widely deployed open-source message broker implementing the **AMQP 0-9-1** (Advanced Message Queuing Protocol) standard. Unlike Kafka's dumb-broker/partition-log architecture, RabbitMQ is a **smart broker** offering complex routing topologies, flexible exchanges (Direct, Fanout, Topic, Headers), message acknowledgments, dead-letter exchanges (DLX), and transactional queuing.

In NestJS, `@nestjs/microservices` natively supports RabbitMQ (`Transport.RMQ`) via `amqplib` and `amqp-connection-manager`. It provides seamless RPC request-response semantics, asynchronous event distribution, and resilient auto-reconnecting AMQP channels.

---

## 2. Theoretical Foundations

RabbitMQ decouples message publishers from queues using **Exchanges**:
1. **Producer**: Publishes messages to an **Exchange** with a **Routing Key**.
2. **Exchange Types**:
   - **Direct**: Routes to queues with matching routing keys (`orders.process`).
   - **Fanout**: Broadcasts to all bound queues, ignoring routing keys (Pub/Sub).
   - **Topic**: Routes using wildcard patterns (`orders.*`, `notifications.#`).
   - **Headers**: Routes based on message header attributes.
3. **Queue**: Buffers messages and delivers them to connected consumers.
4. **Manual Acknowledgment (ACK / NACK)**: Consumers must acknowledge successful processing (`channel.ack(msg)`). If rejected (`channel.nack(msg, false, false)`), messages are routed to a Dead-Letter Exchange (DLX).

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   NestJS RMQ Producer                  │
│       client.emit('order.created', orderPayload)       │
└───────────────────────────┬────────────────────────────┘
                            │
               AMQP Protocol (Port 5672)
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                   RabbitMQ Server                      │
│                                                        │
│   Exchange: `amq.topic`                                │
│       │                                                │
│       ├─ Routing Key: `order.created`                  │
│       │                                                │
│       ▼                                                │
│   Queue: `orders_queue`                                │
│   [ msg1 ] [ msg2 ] [ msg3 ]                           │
└───────────────────────────┬────────────────────────────┘
                            │
               channel.consume() via TCP
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                   NestJS RMQ Consumer                  │
│               @EventPattern('order.created')           │
│         channel.ack(originalMessage)                   │
└────────────────────────────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @nestjs/microservices amqplib amqp-connection-manager
npm install --save-dev @types/amqplib
```

---

### 4.2 Bootstrapping a RabbitMQ Microservice in `main.ts`

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.createMicroservice<MicroserviceOptions>(AppModule, {
    transport: Transport.RMQ,
    options: {
      urls: [process.env.RABBITMQ_URL || 'amqp://guest:guest@localhost:5672'],
      queue: 'orders_queue',
      queueOptions: {
        durable: true, // Queue survives broker restarts
      },
      noAck: false, // Enforce manual acknowledgments (ACK/NACK)
      prefetchCount: 10, // Preload up to 10 messages per worker
    },
  });

  await app.listen();
  console.log('RabbitMQ Consumer microservice is listening on orders_queue.');
}
bootstrap();
```

---

### 4.3 Configuring the Producer Client with `ClientsModule`

```typescript
// src/orders/orders-rmq.module.ts
import { Module } from '@nestjs/common';
import { ClientsModule, Transport } from '@nestjs/microservices';
import { OrdersRmqService } from './orders-rmq.service';

export const RMQ_SERVICE = 'RMQ_SERVICE';

@Module({
  imports: [
    ClientsModule.register([
      {
        name: RMQ_SERVICE,
        transport: Transport.RMQ,
        options: {
          urls: ['amqp://localhost:5672'],
          queue: 'orders_queue',
          queueOptions: {
            durable: true,
          },
        },
      },
    ]),
  ],
  providers: [OrdersRmqService],
  exports: [OrdersRmqService],
})
export class OrdersRmqModule {}
```

---

### 4.4 Publishing Events in Producer

```typescript
// src/orders/orders-rmq.service.ts
import { Injectable, Inject, Logger } from '@nestjs/common';
import { ClientProxy } from '@nestjs/microservices';
import { RMQ_SERVICE } from './orders-rmq.module';

@Injectable()
export class OrdersRmqService {
  private readonly logger = new Logger(OrdersRmqService.name);

  constructor(@Inject(RMQ_SERVICE) private readonly client: ClientProxy) {}

  publishOrderCreated(orderId: string, amount: number) {
    this.logger.log(`Emitting order.created event to RabbitMQ for order ${orderId}`);

    this.client.emit('order.created', {
      orderId,
      amount,
      timestamp: new Date().toISOString(),
    });
  }
}
```

---

### 4.5 Consuming with Manual Acknowledgments (`RmqContext`)

When `noAck: false` is configured, consumers must manually acknowledge messages:

```typescript
// src/orders/orders-rmq.controller.ts
import { Controller } from '@nestjs/common';
import { EventPattern, Payload, Ctx, RmqContext } from '@nestjs/microservices';

@Controller()
export class OrdersRmqController {
  @EventPattern('order.created')
  async handleOrderCreated(
    @Payload() data: { orderId: string; amount: number },
    @Ctx() context: RmqContext,
  ) {
    const channel = context.getChannelRef();
    const originalMsg = context.getMessage();

    try {
      console.log(`Processing order: ${data.orderId}`);
      // Process business logic / database write
      await new Promise((r) => setTimeout(r, 200));

      // 1. Manually Acknowledge successful processing
      channel.ack(originalMsg);
    } catch (error) {
      console.error(`Failed to process order ${data.orderId}. NACKing message.`);

      // 2. Reject message: requeue = false routes message to DLX (if configured)
      channel.nack(originalMsg, false, false);
    }
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Poison Pill Messages Crashing Consumers
If a malformed message causes an unhandled exception and is rejected with `requeue: true`, RabbitMQ immediately re-delivers it to the same worker, causing an infinite crash loop.
- **Handling**: Always reject poison messages with `channel.nack(msg, false, false)` so they route to a Dead-Letter Exchange (DLX).

### 5.2 Unacknowledged Message Accumulation
If a consumer forgets to call `channel.ack(msg)` or `channel.nack()`, messages remain in an unacknowledged (`unacked`) state in RabbitMQ RAM until the TCP connection drops.
- **Rule**: Every code branch (including error handlers) must terminate with either `channel.ack()` or `channel.nack()`.

---

## 6. Performance & Memory Implications

- **Prefetch Count (`prefetchCount`)**: By default, RabbitMQ sends all queued messages to the connected consumer at once. Setting `prefetchCount: 10` limits the unacknowledged buffer per worker, balancing load across multiple worker pods.
- **Durable Queues**: Marking queues and messages as `durable: true` and `persistent: true` forces writes to disk, ensuring data survival across broker restarts at a minor I/O latency cost.

---

## 7. Anti-Patterns & Common Mistakes

- **Defaulting to `noAck: true` in Production**: Using auto-acknowledgments. If the worker pod crashes midway through processing, the message was already deleted from RabbitMQ and is lost forever.
- **Creating One Queue Per Message Type**: Creating 50 different queues for 50 event types instead of using Topic Exchanges and binding routing keys to shared domain queues.
- **Blocking the Main Event Loop**: Running heavy synchronous CPU work inside AMQP handlers, delaying `channel.ack()` calls and causing AMQP heartbeat timeouts.

---

## 8. Production Checklist & Operational Readiness

- [ ] `durable: true` enabled on queues and exchanges.
- [ ] `noAck: false` configured and manual `channel.ack()` verified on all paths.
- [ ] `prefetchCount` configured (e.g. 10–50) to prevent worker buffer starvation.
- [ ] Dead-Letter Exchange (DLX) configured on all production queues.
- [ ] AMQP connection pooling managed via `amqp-connection-manager`.

---

## 9. Migration & Version History

- **NestJS 7/8**: Standardized `RmqContext` and manual acknowledgment patterns.
- **NestJS 11.x**: Support for direct native channel access via `context.getChannelRef()`.

---

## 10. Verification & Testing

Unit testing manual acknowledgment handling:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { OrdersRmqController } from './orders-rmq.controller';

describe('OrdersRmqController', () => {
  let controller: OrdersRmqController;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      controllers: [OrdersRmqController],
    }).compile();

    controller = module.get<OrdersRmqController>(OrdersRmqController);
  });

  it('acknowledges message on successful processing', async () => {
    const mockChannel = { ack: vi.fn(), nack: vi.fn() };
    const mockMsg = { content: 'test' };

    const mockContext: any = {
      getChannelRef: () => mockChannel,
      getMessage: () => mockMsg,
    };

    await controller.handleOrderCreated({ orderId: 'o1', amount: 100 }, mockContext);

    expect(mockChannel.ack).toHaveBeenCalledWith(mockMsg);
    expect(mockChannel.nack).not.toHaveBeenCalled();
  });
});
```

---

## 11. Comprehensive Examples

### Dead-Letter Exchange (DLX) Topology Setup

```typescript
// src/rabbitmq/dlx-setup.service.ts
import { Injectable, OnModuleInit } from '@nestjs/common';
import * as amqp from 'amqplib';

@Injectable()
export class DlxSetupService implements OnModuleInit {
  async onModuleInit() {
    const conn = await amqp.connect(process.env.RABBITMQ_URL || 'amqp://localhost:5672');
    const channel = await conn.createChannel();

    // 1. Declare Dead-Letter Exchange and Queue
    await channel.assertExchange('dlx.orders', 'direct', { durable: true });
    await channel.assertQueue('orders_dlq', { durable: true });
    await channel.bindQueue('orders_dlq', 'dlx.orders', 'orders.dead');

    // 2. Declare Main Queue with DLX configuration
    await channel.assertQueue('orders_queue', {
      durable: true,
      arguments: {
        'x-dead-letter-exchange': 'dlx.orders',
        'x-dead-letter-routing-key': 'orders.dead',
        'x-message-ttl': 86400000, // 24h message expiration
      },
    });

    await channel.close();
    await conn.close();
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-messaging-fundamentals.md](./02-messaging-fundamentals.md) - Messaging paradigms
- [03-kafka.md](./03-kafka.md) - Apache Kafka comparison
- [05-advanced/02-background-processing/01-queues-fundamentals](../02-background-processing/01-queues-fundamentals.md) - BullMQ queues
- [RabbitMQ Official Tutorials](https://www.rabbitmq.com/getstarted.html)
