# Apache Kafka Integration in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 03-events-and-messaging
- **Prerequisites**: [02-messaging-fundamentals](./02-messaging-fundamentals.md), [00-prerequisites/03-nodejs-async-and-event-loop](../../00-prerequisites/03-nodejs-async-and-event-loop.md)

---

## 1. Context & Purpose

When building high-throughput event streaming platforms—processing millions of events per second for financial transactions, telemetry streaming, clickstream analysis, or event-sourced architectures—traditional message brokers hit throughput ceilings.

**Apache Kafka** is a distributed, horizontally partitioned, replicated commit log. Kafka decouples producers and consumers using partitioned topics, consumer groups, and offset management. NestJS provides first-class Kafka support via `@nestjs/microservices` and `kafkajs`, supporting both RPC request-response patterns (via reply topics) and massive event stream consumers.

---

## 2. Theoretical Foundations

Kafka's core architectural primitives:
- **Topics & Partitions**: A topic is divided into partitions. Partitions are the unit of parallelism; messages within a partition are strictly ordered by sequential offsets.
- **Consumer Groups**: Multiple consumers share a `groupId`. Kafka assigns partitions evenly across consumers in the group. If consumers outnumber partitions, excess consumers sit idle.
- **Message Keys & Partitioning**: Messages with the same key (e.g. `userId`) always route to the exact same partition, guaranteeing strict chronological ordering for that user.
- **Offsets & Commits**: Consumers track their position in the partition log. Offsets can be committed automatically or manually after successful database processing.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   NestJS Kafka Producer                │
│    client.emit('orders.created', { key: 'u1', ... })   │
└───────────────────────────┬────────────────────────────┘
                            │
               Hash(Key) % Partitions (e.g., Partition 1)
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                 Kafka Cluster (Topic: orders)          │
│   Partition 0: [msg1] [msg2]                           │
│   Partition 1: [msg1 (u1)] [msg2 (u1)] [msg3 (u1)]     │
│   Partition 2: [msg1]                                  │
└───────────────────────────┬────────────────────────────┘
                            │
              Consumer Group: `order-processing-group`
                            │
 ┌──────────────────────────┴───────────────────────────┐
 ▼                                                      ▼
NestJS Consumer Pod 1                               NestJS Consumer Pod 2
(Consumes Partition 0 & 1)                          (Consumes Partition 2)
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @nestjs/microservices kafkajs
```

---

### 4.2 Configuring Kafka Microservice in `main.ts`

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.createMicroservice<MicroserviceOptions>(AppModule, {
    transport: Transport.KAFKA,
    options: {
      client: {
        brokers: [process.env.KAFKA_BROKER || 'localhost:9092'],
        clientId: 'orders-service',
      },
      consumer: {
        groupId: 'orders-consumer-group',
        allowAutoTopicCreation: true,
      },
    },
  });

  await app.listen();
  console.log('Kafka Consumer microservice is listening.');
}
bootstrap();
```

---

### 4.3 Kafka Producer Setup with `ClientsModule`

```typescript
// src/orders/orders-producer.module.ts
import { Module } from '@nestjs/common';
import { ClientsModule, Transport } from '@nestjs/microservices';
import { OrdersProducerService } from './orders-producer.service';

export const KAFKA_SERVICE = 'KAFKA_SERVICE';

@Module({
  imports: [
    ClientsModule.register([
      {
        name: KAFKA_SERVICE,
        transport: Transport.KAFKA,
        options: {
          client: {
            brokers: ['localhost:9092'],
            clientId: 'orders-producer',
          },
          producer: {
            allowAutoTopicCreation: true,
          },
        },
      },
    ]),
  ],
  providers: [OrdersProducerService],
  exports: [OrdersProducerService],
})
export class OrdersProducerModule {}
```

---

### 4.4 Publishing Partitioned Events in Producer

```typescript
// src/orders/orders-producer.service.ts
import { Injectable, Inject, OnModuleInit, Logger } from '@nestjs/common';
import { ClientKafka } from '@nestjs/microservices';
import { KAFKA_SERVICE } from './orders-producer.module';

@Injectable()
export class OrdersProducerService implements OnModuleInit {
  private readonly logger = new Logger(OrdersProducerService.name);

  constructor(@Inject(KAFKA_SERVICE) private readonly kafkaClient: ClientKafka) {}

  async onModuleInit() {
    // Connect to Kafka brokers at startup
    await this.kafkaClient.connect();
    this.logger.log('Kafka Producer connected.');
  }

  async publishOrderCreated(orderId: string, customerId: string, totalAmount: number) {
    this.logger.log(`Publishing order.created for order ${orderId} (Customer: ${customerId})`);

    // In Kafka, passing a 'key' guarantees all events for this customer
    // land on the exact same partition in chronological order
    this.kafkaClient.emit('order.created', {
      key: customerId,
      value: {
        orderId,
        customerId,
        totalAmount,
        timestamp: new Date().toISOString(),
      },
    });
  }
}
```

---

### 4.5 Subscribing to Kafka Events in Consumer Controller

```typescript
// src/orders/orders.controller.ts
import { Controller } from '@nestjs/common';
import { EventPattern, Payload, Ctx, KafkaContext } from '@nestjs/microservices';

@Controller()
export class OrdersController {
  @EventPattern('order.created')
  async handleOrderCreated(
    @Payload() message: any,
    @Ctx() context: KafkaContext,
  ) {
    const originalMessage = context.getMessage();
    const partition = context.getPartition();
    const topic = context.getTopic();
    const offset = originalMessage.offset;

    console.log(
      `[Kafka Event Received] Topic: ${topic} | Partition: ${partition} | Offset: ${offset}`,
    );
    console.log('Payload:', message);

    // In NestJS 11, unwrap() provides access to the underlying kafkajs consumer
    const nativeConsumer = context.getConsumer();
    // Manual offset commit can be triggered if autoCommit: false
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Consumer Rebalance Storms
If a consumer pod pauses for too long during garbage collection or blocking I/O, Kafka assumes the consumer died (`session.timeout.ms` or `max.poll.interval.ms` expired) and triggers a **Consumer Group Rebalance**. All consumers pause reading, causing severe lag spikes.
- **Handling**: Ensure message processing completes within `max.poll.interval.ms` (default 5 minutes), or tune settings.

### 5.2 Out-of-Order Processing Across Partitions
Kafka guarantees strict FIFO ordering **only within a single partition**, never across different partitions. If order events for Customer A land on Partition 1 and Partition 2, they may be processed out of sequence.
- **Rule**: Always assign a deterministic `key` (e.g. `customerId` or `orderId`) when emitting messages.

---

## 6. Performance & Memory Implications

- **Batching & Compression**: Kafka producers buffer messages into batches before transmitting over TCP. Enabling compression (`gzip` or `snappy`) reduces network bandwidth by up to 70%.
- **Zero-Copy Disk I/O**: Kafka writes to the OS page cache and reads directly via the Linux `sendfile` system call, bypassing JVM/Node user memory space.

---

## 7. Anti-Patterns & Common Mistakes

- **Using Kafka as a Traditional Work Queue**: Expecting individual message acknowledgments and dynamic task re-assignment. For point-to-point task queues, use RabbitMQ or BullMQ instead.
- **Omitting Message Keys**: Publishing all events without keys. Kafka distributes messages via round-robin, destroying ordering guarantees.
- **Blocking the Consumer Thread**: Executing long synchronous CPU loops inside the Kafka consumer handler, triggering session timeout disconnects.

---

## 8. Production Checklist & Operational Readiness

- [ ] Message keys configured for entities requiring chronological order.
- [ ] Number of topic partitions $\ge$ maximum expected consumer pod count.
- [ ] Compression enabled on producers (`compression: CompressionTypes.GZIP`).
- [ ] Consumer `groupId` uniquely identified per microservice domain.
- [ ] Consumer lag monitored via Prometheus / Datadog.

---

## 9. Migration & Version History

- **NestJS 7/8**: Upgraded to `kafkajs` 2.x as the underlying engine.
- **NestJS 11.x**: Added `.unwrap()` to `KafkaContext` to expose the native `kafkajs` Consumer and Producer instances.

---

## 10. Verification & Testing

Unit testing a Kafka producer:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { OrdersProducerService } from './orders-producer.service';
import { KAFKA_SERVICE } from './orders-producer.module';

describe('OrdersProducerService', () => {
  let service: OrdersProducerService;
  let mockKafka: any;

  beforeEach(async () => {
    mockKafka = {
      connect: vi.fn().mockResolvedValue(undefined),
      emit: vi.fn(),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        OrdersProducerService,
        { provide: KAFKA_SERVICE, useValue: mockKafka },
      ],
    }).compile();

    service = module.get<OrdersProducerService>(OrdersProducerService);
  });

  it('publishes event with customerId key for partitioning', async () => {
    await service.publishOrderCreated('ord_1', 'cust_100', 250);

    expect(mockKafka.emit).toHaveBeenCalledWith(
      'order.created',
      expect.objectContaining({
        key: 'cust_100',
        value: expect.objectContaining({ orderId: 'ord_1', totalAmount: 250 }),
      }),
    );
  });
});
```

---

## 11. Comprehensive Examples

### Unwrapping Native KafkaJS Consumer for Manual Offsets

```typescript
// src/orders/manual-commit.controller.ts
import { Controller } from '@nestjs/common';
import { EventPattern, Payload, Ctx, KafkaContext } from '@nestjs/microservices';

@Controller()
export class ManualCommitController {
  @EventPattern('financial.transactions')
  async handleTransaction(@Payload() data: any, @Ctx() context: KafkaContext) {
    const originalMessage = context.getMessage();
    const topic = context.getTopic();
    const partition = context.getPartition();
    const consumer = context.getConsumer(); // Native KafkaJS Consumer instance!

    // 1. Process business logic (e.g. database write)
    await this.processPayment(data);

    // 2. Commit offset manually after guaranteed processing
    await consumer.commitOffsets([
      {
        topic,
        partition,
        offset: (Number(originalMessage.offset) + 1).toString(),
      },
    ]);
  }

  private async processPayment(data: any) {
    // DB transaction...
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-messaging-fundamentals.md](./02-messaging-fundamentals.md) - Messaging concepts
- [04-rabbitmq.md](./04-rabbitmq.md) - RabbitMQ comparison
- [05-cqrs-module.md](./05-cqrs-module.md) - Event sourcing with Kafka
- [KafkaJS Documentation](https://kafka.js.org)
