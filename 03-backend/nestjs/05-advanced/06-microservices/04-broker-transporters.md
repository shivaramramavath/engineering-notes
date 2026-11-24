# Broker Transporters: RabbitMQ, Kafka, NATS, and Redis

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 06-microservices
- **Prerequisites**: [02-transporters](./02-transporters.md), [03-message-patterns](./03-message-patterns.md)

---

## 1. Context & Purpose

While point-to-point transporters like TCP require producers to know the IP address and port of every consumer, modern cloud-native architectures require decoupled **Message Brokers**. Message brokers act as a resilient intermediary: producers publish messages to the broker, and the broker handles buffering, routing, consumer load balancing, and persistence.

NestJS supports the four major industry brokers: **RabbitMQ**, **Apache Kafka**, **NATS**, and **Redis Pub/Sub**. This guide provides an in-depth comparison of their architectural trade-offs, configuration requirements, and failover mechanics in NestJS 11.

---

## 2. Theoretical Foundations

### 2.1 The Architectural Spectrum of Message Brokers

```
◄── High Latency / Strong Persistence               Low Latency / Ephemeral Speed ──►
[ Apache Kafka ]       [ RabbitMQ ]              [ NATS Core ]       [ Redis Pub/Sub ]
Commit Log             AMQP Exchange/Queue       In-Memory Mesh      In-Memory RAM
Millions msgs/sec      Transactional Workflows   Sub-millisecond     Ultra-lightweight
Multi-day Replay       Dead-Letter Exchanges     Zero persistence    Zero persistence
```

### 2.2 Transporter Selection Decision Framework
- Choose **Apache Kafka** if: You need event sourcing, event replayability, audit streaming, or partitioned chronological ordering for high volumes ($> 100\text{k}\text{ msgs/s}$).
- Choose **RabbitMQ** if: You need complex routing (topic/fanout/direct), guaranteed message persistence, dead-letter exchanges (DLX), and worker load balancing.
- Choose **NATS** if: You need ultra-fast, low-latency ($< 1\text{ms}$) microservice RPC and pub/sub with zero disk overhead.
- Choose **Redis Pub/Sub** if: You already have Redis and need ephemeral broadcasts (e.g., cache invalidation, real-time presence) where lost packets are acceptable.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   ClientsModule.register()             │
└──────────────┬──────────────────────────┬──────────────┘
               │                          │
        Transport.RMQ              Transport.KAFKA
               │                          │
               ▼                          ▼
┌───────────────────────────┐ ┌───────────────────────────┐
│     RabbitMQ Broker       │ │       Kafka Cluster       │
│  - amqp://localhost:5672  │ │  - localhost:9092         │
│  - durable: true          │ │  - consumer groupId       │
│  - noAck: false           │ │  - allowAutoTopicCreation │
└───────────────────────────┘ └───────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Configuring NATS Transporter in NestJS

Install NATS dependencies:
```bash
npm install --save nats
```

```typescript
// src/nats-microservice.ts
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.createMicroservice<MicroserviceOptions>(AppModule, {
    transport: Transport.NATS,
    options: {
      servers: ['nats://localhost:4222'],
      queue: 'orders_service_queue', // Queue group: load balances messages across pods!
    },
  });

  await app.listen();
}
bootstrap();
```

---

### 4.2 Broker Client Factory Configuration

```typescript
// src/common/brokers/broker-client.module.ts
import { Module } from '@nestjs/common';
import { ClientsModule, Transport } from '@nestjs/microservices';

@Module({
  imports: [
    ClientsModule.register([
      // 1. RabbitMQ Client for Billing Transactions
      {
        name: 'BILLING_RMQ',
        transport: Transport.RMQ,
        options: {
          urls: ['amqp://localhost:5672'],
          queue: 'billing_transactions',
          queueOptions: { durable: true },
        },
      },
      // 2. Kafka Client for User Telemetry Stream
      {
        name: 'TELEMETRY_KAFKA',
        transport: Transport.KAFKA,
        options: {
          client: { brokers: ['localhost:9092'] },
          consumer: { groupId: 'telemetry-consumer' },
        },
      },
    ]),
  ],
  exports: [ClientsModule],
})
export class BrokerClientModule {}
```

---

### 4.3 Polyglot Broker Dispatcher

```typescript
// src/orders/orders-dispatcher.service.ts
import { Injectable, Inject, Logger } from '@nestjs/common';
import { ClientProxy, ClientKafka } from '@nestjs/microservices';

@Injectable()
export class OrdersDispatcherService {
  private readonly logger = new Logger(OrdersDispatcherService.name);

  constructor(
    @Inject('BILLING_RMQ') private readonly rmqClient: ClientProxy,
    @Inject('TELEMETRY_KAFKA') private readonly kafkaClient: ClientKafka,
  ) {}

  async processOrder(order: { id: string; amount: number; userId: string }) {
    // 1. Transactional command routed through RabbitMQ (guaranteed delivery)
    this.rmqClient.emit('order.charge', {
      orderId: order.id,
      amount: order.amount,
    });

    // 2. High-throughput telemetry event streamed through Kafka
    this.kafkaClient.emit('user.telemetry', {
      key: order.userId,
      value: { event: 'ORDER_PLACED', orderId: order.id, timestamp: Date.now() },
    });

    this.logger.log(`Order ${order.id} dispatched across RabbitMQ and Kafka.`);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 NATS Queue Groups vs Individual Subscribers
If `queue: 'my_queue'` is omitted in NATS options, **every single connected pod** receives a copy of every message (broadcast).
- **Rule**: For worker load balancing (where only one pod should process a task), always specify a `queue` group string.

### 5.2 Network Partitioning in Kafka vs RabbitMQ
- In RabbitMQ, network splits can cause partition desynchronization (network partitions must be resolved via Mnesia clustering configurations).
- In Kafka, partitions are managed by Kraft / Zookeeper metadata quorums, automatically failing over to in-sync replicas (ISR).

---

## 6. Performance & Memory Implications

- **NATS Throughput**: Can process over 5,000,000 messages/sec with $< 0.1\text{ms}$ latency due to its zero-allocation memory design.
- **Kafka Batching**: High throughput achieved by streaming sequential disk chunks via the OS page cache.

---

## 7. Anti-Patterns & Common Mistakes

- **Using Redis Pub/Sub for Financial Ledgers**: Using Redis Pub/Sub where dropped packets can cause lost transactions.
- **Forgetting Reconnection Options on Cloud Brokers**: Hardcoding single IP addresses without retry backoff when connecting to Amazon MSK or CloudAMQP.

---

## 8. Production Checklist & Operational Readiness

- [ ] Transporter selected according to persistence and latency requirements.
- [ ] Queue groups configured in NATS for worker load balancing.
- [ ] TLS certificates and credentials loaded securely via environment variables.
- [ ] Dead-letter mechanisms configured for persistent brokers.

---

## 9. Migration & Version History

- **NestJS 9/10/11**: Unified `ClientProxy` and `Server` interfaces across all 4 major message brokers.

---

## 10. Verification & Testing

Unit testing multi-broker dispatch:

```typescript
import { OrdersDispatcherService } from './orders-dispatcher.service';

describe('OrdersDispatcherService', () => {
  it('dispatches to both RMQ and Kafka', async () => {
    const mockRmq = { emit: vi.fn() };
    const mockKafka = { emit: vi.fn() };

    const service = new OrdersDispatcherService(mockRmq as any, mockKafka as any);
    await service.processOrder({ id: '1', amount: 50, userId: 'u1' });

    expect(mockRmq.emit).toHaveBeenCalledWith('order.charge', expect.any(Object));
    expect(mockKafka.emit).toHaveBeenCalledWith('user.telemetry', expect.any(Object));
  });
});
```

---

## 11. Comprehensive Examples

### Health Check for Message Brokers via `@nestjs/terminus`

```typescript
// src/health/broker-health.controller.ts
import { Controller, Get } from '@nestjs/common';
import { HealthCheck, HealthCheckService, MicroserviceHealthIndicator } from '@nestjs/terminus';
import { Transport } from '@nestjs/microservices';

@Controller('health')
export class BrokerHealthController {
  constructor(
    private readonly health: HealthCheckService,
    private readonly microservice: MicroserviceHealthIndicator,
  ) {}

  @Get()
  @HealthCheck()
  checkBrokers() {
    return this.health.check([
      () =>
        this.microservice.pingCheck('rabbitmq', {
          transport: Transport.RMQ,
          options: { urls: ['amqp://localhost:5672'] },
        }),
    ]);
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-transporters.md](./02-transporters.md) - Transporter fundamentals
- [05-advanced/03-events-and-messaging/03-kafka.md](../03-events-and-messaging/03-kafka.md) - Dedicated Kafka guide
- [05-advanced/03-events-and-messaging/04-rabbitmq.md](../03-events-and-messaging/04-rabbitmq.md) - Dedicated RabbitMQ guide
- [NATS Official Documentation](https://docs.nats.io)
