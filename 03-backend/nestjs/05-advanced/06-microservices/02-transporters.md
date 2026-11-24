# Microservice Transporters in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 06-microservices
- **Prerequisites**: [01-microservices-architecture](./01-microservices-architecture.md), [05-advanced/03-events-and-messaging/02-messaging-fundamentals](../03-events-and-messaging/02-messaging-fundamentals.md)

---

## 1. Context & Purpose

In NestJS, a **Transporter** is the communication driver responsible for transmitting messages between microservices. NestJS abstracts communication logic completely: whether messages travel over raw TCP sockets, Redis Pub/Sub, RabbitMQ queues, Kafka commit logs, NATS channels, or gRPC HTTP/2 streams, the application-level controller code (`@MessagePattern()`, `@EventPattern()`) remains virtually identical.

Choosing the right transporter depends on delivery guarantees, latency requirements, message ordering constraints, and broker operational overhead.

---

## 2. Theoretical Foundations

### 2.1 Transporter Comparison Matrix

| Transporter | Protocol | Primary Strengths | Best Use Cases |
|---|---|---|---|
| **TCP** | Raw TCP socket | Zero external broker required, minimal latency | Internal cluster microservices, local dev |
| **Redis** | Redis Pub/Sub | Lightweight, uses existing Redis infrastructure | Ephemeral broadcasts, cache invalidation |
| **MQTT** | MQTT (TCP/WebSockets) | Extremely low bandwidth, small footprint | IoT devices, embedded systems, mobile telemetry |
| **NATS** | NATS protocol | Microsecond latency, millions of msgs/sec | High-throughput cloud-native microservices |
| **RabbitMQ** | AMQP 0-9-1 | Complex routing, durable queues, manual ACKs | Enterprise transactional workflows, billing |
| **Kafka** | Kafka binary protocol | Partitioned commit log, event replayability | Event streaming, high-volume clickstream/audit |
| **gRPC** | HTTP/2 + Protobuf | Strict typing, bi-directional streaming | Cross-language RPC, high-performance polyglot |

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   NestJS Controller                    │
│               @MessagePattern('get_user')              │
└───────────────────────────┬────────────────────────────┘
                            │
               Abstract Server / ClientProxy
                            │
 ┌──────────────┬───────────┼───────────┬──────────────┐
 ▼              ▼           ▼           ▼              ▼
TCP           Redis       NATS       RabbitMQ        gRPC
(Net Socket)  (PubSub)   (Core)      (amqplib)     (@grpc/grpc-js)
```

In NestJS 11, transporters implement the `.unwrap<T>()` method, allowing developers to break through the abstraction layer and access the underlying native connection handle (e.g. native NATS client, native Kafka consumer).

---

## 4. Concrete Implementation

### 4.1 Configuring Multiple Transporters in One Client

```typescript
// src/gateway/gateway-clients.module.ts
import { Module } from '@nestjs/common';
import { ClientsModule, Transport } from '@nestjs/microservices';

export const USER_TCP_SERVICE = 'USER_TCP_SERVICE';
export const EVENT_NATS_SERVICE = 'EVENT_NATS_SERVICE';

@Module({
  imports: [
    ClientsModule.register([
      // Client 1: High-speed internal TCP
      {
        name: USER_TCP_SERVICE,
        transport: Transport.TCP,
        options: { host: '127.0.0.1', port: 4001 },
      },
      // Client 2: High-throughput NATS event bus
      {
        name: EVENT_NATS_SERVICE,
        transport: Transport.NATS,
        options: {
          servers: ['nats://localhost:4222'],
        },
      },
    ]),
  ],
  exports: [ClientsModule],
})
export class GatewayClientsModule {}
```

---

### 4.2 Using `.unwrap()` in NestJS 11 for Native Driver Access

When standard Nest abstractions do not expose a driver-specific feature (e.g. manual NATS flush, custom Kafka partitioner):

```typescript
// src/orders/orders-transporter.service.ts
import { Injectable, Inject, OnModuleInit } from '@nestjs/common';
import { ClientProxy } from '@nestjs/microservices';
import { NatsConnection } from 'nats';
import { EVENT_NATS_SERVICE } from '../gateway/gateway-clients.module';

@Injectable()
export class OrdersTransporterService implements OnModuleInit {
  constructor(
    @Inject(EVENT_NATS_SERVICE) private readonly natsClient: ClientProxy,
  ) {}

  async onModuleInit() {
    await this.natsClient.connect();

    // Access underlying native NATS connection using unwrap() in NestJS 11
    const nativeNats = this.natsClient.unwrap<NatsConnection>();

    console.log(`Connected to NATS server: ${nativeNats.getServer()}`);
  }
}
```

---

### 4.3 Custom Transporter Implementation

NestJS allows creating entirely custom transporters (e.g. AWS SQS or Google Cloud Pub/Sub) by extending `Server` and `ClientProxy`:

```typescript
// src/common/transporters/custom-client.ts
import { ClientProxy, ReadPacket, WritePacket } from '@nestjs/microservices';

export class CustomSqsClient extends ClientProxy {
  async connect(): Promise<any> {
    // Initialize custom AWS SQS client
  }

  async close() {
    // Terminate connection
  }

  protected publish(packet: ReadPacket, callback: (packet: WritePacket) => void): () => void {
    // Send message to custom broker
    callback({ response: 'Processed' });
    return () => {}; // Teardown subscription
  }

  protected async dispatchEvent(packet: ReadPacket): Promise<any> {
    // Broadcast event to custom broker
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Reconnection Handling During Transporter Outages
If the message broker restarts, `ClientProxy` can drop in-flight messages unless buffer/queue options are configured.
- **Handling**: Ensure `maxRetriesPerRequest` and reconnect intervals are configured in transporter options.

### 5.2 Deserialization Mismatches
When switching between TCP (raw JSON buffers) and gRPC (strictly typed Protobuf binary), payloads containing extra fields are permitted in TCP but strictly rejected in Protobuf.

---

## 6. Performance & Memory Implications

- **TCP vs Brokers**: TCP eliminates the middleman broker hop ($0.5\text{ms}$ latency vs $2–5\text{ms}$ through RabbitMQ), but lacks message persistence if the destination pod is down.
- **Zero Memory Leaks with `.unwrap()`**: Native handles unwrapped via `.unwrap()` share the lifecycle of the parent `ClientProxy` and terminate cleanly on shutdown.

---

## 7. Anti-Patterns & Common Mistakes

- **Using TCP Across Public Internet**: Running unencrypted raw TCP microservice ports across untrusted networks. Internal TCP must be isolated within private VPC subnets.
- **Treating Redis Pub/Sub as a Persistent Message Queue**: Assuming Redis Pub/Sub retains messages when consumers are offline. If no consumer is listening, published messages vanish instantly.

---

## 8. Production Checklist & Operational Readiness

- [ ] Transporter selected based on durability requirements (RabbitMQ/Kafka for persistence; TCP/NATS for ephemeral speed).
- [ ] TLS encryption enabled for cloud broker connections.
- [ ] Network firewall restricts internal microservice ports to private subnets.
- [ ] Native drivers cleanly drained during `app.close()`.

---

## 9. Migration & Version History

- **NestJS 11.x**: Added `.unwrap()` method across all built-in transporters to access native driver connections directly.

---

## 10. Verification & Testing

Unit testing transporter unwrapping:

```typescript
import { OrdersTransporterService } from './orders-transporter.service';

describe('OrdersTransporterService', () => {
  it('unwraps native connection cleanly', async () => {
    const mockNative = { getServer: () => 'nats://localhost:4222' };
    const mockClient: any = {
      connect: vi.fn().mockResolvedValue(undefined),
      unwrap: vi.fn().mockReturnValue(mockNative),
    };

    const service = new OrdersTransporterService(mockClient);
    await service.onModuleInit();

    expect(mockClient.unwrap).toHaveBeenCalled();
  });
});
```

---

## 11. Comprehensive Examples

### Resilient NATS Transporter Setup

```typescript
// src/main.ts (NATS Microservice)
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.createMicroservice<MicroserviceOptions>(AppModule, {
    transport: Transport.NATS,
    options: {
      servers: ['nats://localhost:4222'],
      maxReconnectAttempts: 10,
      reconnectTimeWait: 2000,
    },
  });

  await app.listen();
}
bootstrap();
```

---

## 12. Cross-References & Further Reading

- [01-microservices-architecture.md](./01-microservices-architecture.md) - Microservices architecture
- [03-message-patterns.md](./03-message-patterns.md) - Message patterns
- [06-grpc-fundamentals-and-protobuf.md](./06-grpc-fundamentals-and-protobuf.md) - gRPC transporter
- [NestJS Transporters Documentation](https://docs.nestjs.com/microservices/basics#transporters)
