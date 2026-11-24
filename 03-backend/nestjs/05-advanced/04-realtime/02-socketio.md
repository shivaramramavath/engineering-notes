# Socket.IO Architecture and Advanced Features

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 04-realtime
- **Prerequisites**: [01-gateways-and-events](./01-gateways-and-events.md), [00-prerequisites/04-http-and-rest-basics](../../00-prerequisites/04-http-and-rest-basics.md)

---

## 1. Context & Purpose

While the raw WebSocket protocol (RFC 6455) provides basic frame transmission, enterprise production applications require higher-level transport resiliency:
- Automatic reconnection with exponential backoff.
- HTTP Long-Polling fallback when firewalls or corporate proxies block WebSocket handshakes.
- Acknowledgments (request-response callbacks over real-time connections).
- Packet multiplexing over distinct namespaces.

**Socket.IO** is the most mature real-time library in the JavaScript ecosystem. Through `@nestjs/platform-socket.io`, NestJS provides seamless integration with Socket.IO v4, exposing its full suite of acknowledgments, broadcasting semantics, and custom transport adapters.

---

## 2. Theoretical Foundations

### 2.1 The Engine.IO Layer (Fallback Architecture)
Socket.IO is built on top of **Engine.IO**, which manages the underlying transport:
1. **HTTP Long-Polling Handshake**: The client initiates an HTTP GET request to establish a session ID (`sid`).
2. **WebSocket Upgrade**: Engine.IO attempts to upgrade the connection to a pure WebSocket connection.
3. **Graceful Fallback**: If the WebSocket handshake fails (e.g. restrictive corporate proxy), the connection falls back cleanly to HTTP Long-Polling without disconnecting the user.

### 2.2 Client Acknowledgments (ACK Callbacks)
Unlike pure raw WebSockets where messages are fire-and-forget, Socket.IO allows clients to pass a callback function as the final argument. The server executes this callback, returning values directly back to the specific caller.

---

## 3. Architecture & Internal Mechanics

```
Browser Client (Socket.IO Client v4)
              │
              ├─> 1. GET /socket.io/?EIO=4&transport=polling (Obtains sid)
              │
              ├─> 2. GET /socket.io/?EIO=4&transport=websocket (Upgrades to WS)
              │
              ▼
NestJS IoAdapter (@nestjs/platform-socket.io)
              │
              ▼
Socket.io Server (Server Instance)
              │
   ┌──────────┴──────────┐
   ▼                     ▼
Namespace: `/admin`   Namespace: `/orders`
Room: `ops`           Room: `order-100`
   │                     │
   ▼                     ▼
AdminGateway          OrdersGateway
```

---

## 4. Concrete Implementation

### 4.1 Configuring the Custom `IoAdapter`

To configure global Socket.io options (such as CORS, max payload size, and transports), implement a custom adapter:

```typescript
// src/common/adapters/socket-io.adapter.ts
import { IoAdapter } from '@nestjs/platform-socket.io';
import { ServerOptions } from 'socket.io';
import { INestApplication } from '@nestjs/common';

export class CustomSocketIoAdapter extends IoAdapter {
  constructor(app: INestApplication) {
    super(app);
  }

  createIOServer(port: number, options?: ServerOptions): any {
    const server = super.createIOServer(port, {
      ...options,
      cors: {
        origin: ['https://app.example.com', 'http://localhost:3000'],
        methods: ['GET', 'POST'],
        credentials: true,
      },
      transports: ['websocket', 'polling'], // Prefer WebSocket first
      pingInterval: 10000,                  // Ping every 10s
      pingTimeout: 5000,                    // Timeout after 5s without response
      maxHttpBufferSize: 1e6,               // Max payload: 1MB
    });

    return server;
  }
}
```

Register in `main.ts`:
```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';
import { CustomSocketIoAdapter } from './common/adapters/socket-io.adapter';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // Set the custom WebSocket adapter globally
  app.useWebSocketAdapter(new CustomSocketIoAdapter(app));

  await app.listen(3000);
}
bootstrap();
```

---

### 4.2 Handling Acknowledgments (RPC-Style Responses)

In NestJS gateways, returning an object from a message handler automatically delivers it to the client's acknowledgment callback:

```typescript
// src/orders/orders.gateway.ts
import {
  WebSocketGateway,
  SubscribeMessage,
  MessageBody,
  ConnectedSocket,
} from '@nestjs/websockets';
import { Socket } from 'socket.io';

@WebSocketGateway({ namespace: 'orders' })
export class OrdersGateway {
  @SubscribeMessage('submitOrder')
  handleSubmitOrder(
    @ConnectedSocket() client: Socket,
    @MessageBody() orderData: { items: string[]; total: number },
  ) {
    console.log(`Received order from client: ${client.id}`);

    // Returning an object sends it directly back as the ACK callback payload!
    return {
      status: 'SUCCESS',
      orderId: 'ord_' + Date.now(),
      confirmedAt: new Date().toISOString(),
    };
  }
}
```

#### Frontend Consumer (Client-Side ACK)
```javascript
// Browser client
socket.emit('submitOrder', { items: ['Item A'], total: 45 }, (response) => {
  console.log('Server acknowledged order:', response.orderId);
});
```

---

### 4.3 Advanced Broadcasting Patterns

```typescript
// src/chat/chat.gateway.ts (Broadcasting examples)
import { WebSocketGateway, WebSocketServer } from '@nestjs/websockets';
import { Server, Socket } from 'socket.io';

@WebSocketGateway({ namespace: 'chat' })
export class ChatGateway {
  @WebSocketServer()
  server!: Server;

  broadcastToRoom(room: string, event: string, data: any) {
    // 1. Broadcast to everyone in the room
    this.server.to(room).emit(event, data);
  }

  broadcastToRoomExceptSender(sender: Socket, room: string, event: string, data: any) {
    // 2. Broadcast to everyone in the room EXCEPT the sender
    sender.to(room).emit(event, data);
  }

  broadcastToAll(event: string, data: any) {
    // 3. Broadcast globally to all connected sockets in this namespace
    this.server.emit(event, data);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Sticky Sessions Required for Long-Polling
When deploying Socket.io behind a multi-pod load balancer (Nginx / AWS ALB), if a client connects via HTTP Long-Polling, subsequent polling HTTP requests must route to the **same pod** that issued the session ID (`sid`). Failing to configure sticky sessions causes `Session ID unknown (400 Bad Request)` errors.
- **Handling**: Either enable cookie-based **Sticky Sessions** on your load balancer or force clients to use pure WebSockets: `transports: ['websocket']`.

### 5.2 Payload Buffer Overflow
If a malicious client sends a 50MB string over the WebSocket frame, Node.js will parse the packet in RAM, potentially crashing the process.
- **Remedy**: Always configure `maxHttpBufferSize: 1e6` (1MB limit).

---

## 6. Performance & Memory Implications

- **Socket.io Framing Overhead**: Socket.IO adds a few bytes of packet wrapping (e.g. `42["chat", {...}]`) compared to raw WebSockets. For 99% of web applications, the productivity and fallback benefits far outweigh this overhead.
- **Compression**: Socket.io supports `perMessageDeflate` compression, but compressing small JSON packets (< 500 bytes) increases CPU usage without saving bandwidth. Disable compression for high-frequency small message streams.

---

## 7. Anti-Patterns & Common Mistakes

- **Assuming WebSocket Transport Without Verifying Client**: Using Socket.io on the server while the frontend uses native browser `new WebSocket('ws://...')`. Native WebSocket clients cannot speak Socket.IO protocol! Use `socket.io-client` on the frontend or switch the NestJS adapter to `WsAdapter`.
- **Neglecting CORS Headers**: Forgetting to configure CORS in `IoAdapter`, causing browser clients on different domains to reject connection attempts.

---

## 8. Production Checklist & Operational Readiness

- [ ] Custom `IoAdapter` registered with explicit CORS and buffer limits.
- [ ] Sticky sessions configured on load balancer if HTTP polling fallback is enabled.
- [ ] Ping interval and timeout tuned for mobile network resilience.
- [ ] Rooms and namespaces sanitized against injection attacks.

---

## 9. Migration & Version History

- **Socket.io 3 to 4**: Modernized CORS defaults and introduced typed events.
- **NestJS 11.x**: Validated with Socket.io 4.8+ and Express 5 server adapters.

---

## 10. Verification & Testing

Unit testing acknowledgment handlers:

```typescript
import { OrdersGateway } from './orders.gateway';

describe('OrdersGateway', () => {
  let gateway: OrdersGateway;

  beforeEach(() => {
    gateway = new OrdersGateway();
  });

  it('returns valid acknowledgment payload', () => {
    const mockSocket: any = { id: 'sock_123' };
    const res = gateway.handleSubmitOrder(mockSocket, { items: ['item_1'], total: 20 });

    expect(res.status).toBe('SUCCESS');
    expect(res.orderId).toBeDefined();
  });
});
```

---

## 11. Comprehensive Examples

### Disconnecting Idle / Stale Sockets Dynamically

```typescript
// src/chat/socket-manager.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { Server } from 'socket.io';

@Injectable()
export class SocketManagerService {
  private readonly logger = new Logger(SocketManagerService.name);

  disconnectUserAcrossAllSockets(server: Server, userId: string) {
    // Iterate all connected sockets in namespace
    const sockets = server.sockets.sockets;

    sockets.forEach((socket) => {
      if (socket.data?.user?.id === userId) {
        this.logger.warn(`Forcefully disconnecting socket ${socket.id} for user ${userId}`);
        socket.emit('force_logout', { reason: 'Account suspended or logged in elsewhere' });
        socket.disconnect(true);
      }
    });
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-gateways-and-events.md](./01-gateways-and-events.md) - Gateway fundamentals
- [03-websocket-authentication.md](./03-websocket-authentication.md) - Handshake authentication
- [05-scaling-websockets.md](./05-scaling-websockets.md) - Redis Streams adapter
- [Socket.IO Official Documentation](https://socket.io/docs/v4/)
