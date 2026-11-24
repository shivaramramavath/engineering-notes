---
title: "Custom Microservice Transporters in NestJS"
description: "Authoring custom microservice servers and client proxies extending Server and ClientProxy in @nestjs/microservices"
tags: [nestjs, internals, microservices, custom-transporter, client-proxy, server]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Custom Microservice Transporters in NestJS

## Conceptual Overview
While `@nestjs/microservices` provides built-in transporters for Redis, NATS, RabbitMQ, Kafka, gRPC, and TCP, enterprise architectures frequently encounter proprietary messaging brokers (e.g., Apache Pulsar, AWS SQS/SNS, Google Cloud Pub/Sub, Azure Service Bus, or ZeroMQ).

NestJS is architected so that any messaging transport can be seamlessly plugged into the framework by implementing two foundational classes:
1. **Custom Server** (extends `Server`): Listens for incoming messages/events from the transport, deserializes payloads, matches them against registered message patterns (`@MessagePattern()`, `@EventPattern()`), and sends responses back.
2. **Custom ClientProxy** (extends `ClientProxy`): Publishes outbound request-response messages and one-way events, coordinates response correlation IDs, and manages connection lifecycles.

```
+-------------------------------------------------------------------------------+
|                    Custom Transporter Architecture                            |
|                                                                               |
|  [ Inbound Flow: Custom Server ]                                              |
|  Broker/Wire -> [ CustomServer.listen() ]                                     |
|                        |                                                      |
|                        v                                                      |
|  [ this.messageHandlers.get(pattern) ]                                        |
|                        |                                                      |
|                        v                                                      |
|  Execute Handler -> Return Observable / Promise -> Send Ack/Reply to Broker   |
|                                                                               |
|  [ Outbound Flow: Custom ClientProxy ]                                        |
|  client.send('pattern', data)                                                 |
|        |                                                                      |
|        v                                                                      |
|  [ CustomClientProxy.publish() ]                                              |
|  - Attach correlationId                                                       |
|  - Register response callback                                                 |
|  - Publish to Wire/Broker                                                     |
+-------------------------------------------------------------------------------+
```

---

## Core Mechanisms & Architecture

### 1. The `Server` Contract
Every transporter server inherits from `Server` (implementing `CustomTransportStrategy`):
- `listen(callback: () => void)`: Binds to the broker/socket and invokes `callback()` once ready.
- `close()`: Gracefully disconnects from the broker and closes open sockets.
- `this.messageHandlers`: A `Map<string, MessageHandler>` containing all `@MessagePattern()` and `@EventPattern()` handlers discovered by the scanner.
- `this.transformToObservable()`: Converts handler return values into RxJS Observables.

### 2. The `ClientProxy` Contract
Outbound communication inherits from `ClientProxy`:
- `connect()`: Establishes the socket/broker connection.
- `publish(packet: ReadPacket, callback: (packet: WritePacket) => void)`: Dispatches a request-response message, waiting for a correlated reply.
- `dispatchEvent(packet: ReadPacket)`: Dispatches a fire-and-forget event with no response expectation.
- `close()`: Closes client connections.

---

## Implementation Details & NestJS APIs

### 1. Implementing a Custom Server (Example: In-Memory / IPC Transporter)

```typescript
// src/transporters/in-memory.server.ts
import { CustomTransportStrategy, Server } from '@nestjs/microservices';
import { EventEmitter } from 'node:events';

export class InMemoryServer extends Server implements CustomTransportStrategy {
  constructor(private readonly bus: EventEmitter) {
    super();
  }

  listen(callback: () => void) {
    this.bus.on('message', async (packet: { pattern: string; data: any; id: string }) => {
      const handler = this.getHandlerByPattern(packet.pattern);
      if (!handler) {
        this.bus.emit(`reply:${packet.id}`, { err: `No handler for ${packet.pattern}` });
        return;
      }

      // Convert handler execution into an Observable stream
      const response$ = this.transformToObservable(await handler(packet.data));
      this.send(response$, (publishData) => {
        this.bus.emit(`reply:${packet.id}`, publishData);
      });
    });

    callback();
  }

  close() {
    this.bus.removeAllListeners();
  }
}
```

### 2. Implementing a Custom ClientProxy

```typescript
// src/transporters/in-memory.client.ts
import { ClientProxy, ReadPacket, WritePacket } from '@nestjs/microservices';
import { EventEmitter } from 'node:events';
import { randomUUID } from 'node:crypto';

export class InMemoryClient extends ClientProxy {
  constructor(private readonly bus: EventEmitter) {
    super();
  }

  async connect(): Promise<any> {
    return Promise.resolve();
  }

  async close() {
    // Teardown connections
  }

  protected publish(
    packet: ReadPacket,
    callback: (packet: WritePacket) => void,
  ): () => void {
    const correlationId = randomUUID();
    const replyEvent = `reply:${correlationId}`;

    const responseHandler = (responsePacket: WritePacket) => {
      this.bus.removeListener(replyEvent, responseHandler);
      callback(responsePacket);
    };

    this.bus.on(replyEvent, responseHandler);

    this.bus.emit('message', {
      pattern: packet.pattern,
      data: packet.data,
      id: correlationId,
    });

    // Return cleanup/teardown function for cancellation
    return () => this.bus.removeListener(replyEvent, responseHandler);
  }

  protected async dispatchEvent(packet: ReadPacket): Promise<any> {
    this.bus.emit('event', packet);
  }
}
```

### 3. Registering the Custom Transporter in NestJS

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';
import { InMemoryServer } from './transporters/in-memory.server';
import { sharedBus } from './transporters/bus';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // Connect microservice using custom strategy
  app.connectMicroservice({
    strategy: new InMemoryServer(sharedBus),
  });

  await app.startAllMicroservices();
  await app.listen(3000);
}
bootstrap();
```

---

## Production Patterns & Best Practices

### 1. Transporter Unwrap Helper (NestJS 11 Feature)
In NestJS 11, `ClientProxy` and microservice contexts expose `.unwrap<T>()` to access the raw underlying driver instance (e.g., the raw `ioredis`, `kafkajs`, or `@grpc/grpc-js` client):

```typescript
const rawKafkaClient = client.unwrap<Kafka>();
```

### 2. Packet Serialization and Error Propagation
Custom transporters must structure response packets conforming to `WritePacket`:
```typescript
interface WritePacket<T = any> {
  err?: any;
  response?: T;
  isDisposed?: boolean;
}
```
Setting `isDisposed: true` signals to the calling client that the stream has terminated, completing the RxJS Observable.

---

## Performance & Scale Considerations

- **Backpressure Handling**: Ensure custom servers apply backpressure when consuming from message queues (e.g., AWS SQS or Pulsar). Do not pull more messages from the broker than worker concurrency limits allow.
- **Heartbeats & Keep-Alive**: Implement socket keep-alives and ping-pong intervals to detect broker network partitions before requests timeout.

---

## Security & Failure Modes

- **Poison Pill Messages**: If a message cannot be deserialized or crashes JSON parsing, the server must acknowledge/dead-letter the message rather than crashing the process in an infinite redelivery loop.
- **Correlation ID Leaks**: Ensure response listeners in `ClientProxy.publish()` have strict timeouts (e.g., 10s). If a remote worker never replies, unregister the listener to prevent memory leaks.

---

## Anti-Patterns & Common Pitfalls

- **Neglecting Teardown Cleanup in `publish()`**: Failing to return an unsubscription cleanup function from `publish()` leaves listeners hanging in memory when consumers unsubscribe or timeout.

---

## Testing Strategies

```typescript
// test/custom-transporter.spec.ts
import { EventEmitter } from 'node:events';
import { InMemoryServer } from '../src/transporters/in-memory.server';
import { InMemoryClient } from '../src/transporters/in-memory.client';

describe('Custom Transporter (In-Memory)', () => {
  let bus: EventEmitter;
  let server: InMemoryServer;
  let client: InMemoryClient;

  beforeEach((done) => {
    bus = new EventEmitter();
    server = new InMemoryServer(bus);
    client = new InMemoryClient(bus);

    server.addHandler('ping', () => 'pong');
    server.listen(done);
  });

  afterEach(() => {
    server.close();
  });

  it('should send and receive correlated request-response', (done) => {
    client.send('ping', {}).subscribe((result) => {
      expect(result).toBe('pong');
      done();
    });
  });
});
```

---

## Migration & Version Evolution

- **NestJS 11 Transporter Unwrapping**: The `.unwrap()` method added in NestJS 11 simplifies custom transporter testing by enabling direct access to underlying client mocks.

---

## Key Takeaways & Cheat Sheet

- Extend `Server` and implement `CustomTransportStrategy` to build custom microservice servers.
- Extend `ClientProxy` to build custom microservice publishing clients.
- Always use `this.transformToObservable()` to handle async and sync handler returns uniformly.
- Return a cleanup function from `publish()` to prevent memory leaks on client unsubscription.
- Concurrence and backpressure must be governed at the custom server level.

---

## Cross-References
- [02-transporters.md](../05-advanced/06-microservices/02-transporters.md) - Built-in microservice transporters
- [05-platform-adapters.md](file:///03-backend/nestjs/06-internals/05-platform-adapters.md) - Platform adapter mechanics
