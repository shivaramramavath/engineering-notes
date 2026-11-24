# WebSocket Gateways and Event Handling in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 04-realtime
- **Prerequisites**: [02-fundamentals/01-architecture-overview](../../02-fundamentals/01-architecture-overview.md), [02-fundamentals/09-request-lifecycle](../../02-fundamentals/09-request-lifecycle.md)

---

## 1. Context & Purpose

Traditional HTTP interactions follow a client-pull model (Request-Response). However, collaborative tools (Google Docs), financial trading desks, multiplayer gaming, and live chat applications require bidirectional, full-duplex, low-latency communication where the server can push state updates to connected clients instantaneously without polling overhead.

**WebSockets** provide persistent, bidirectional TCP connections established via an HTTP Upgrade handshake. In NestJS, real-time communication is encapsulated within **Gateways** (`@WebSocketGateway()`). Gateways mirror standard controllers, providing declarative event decorators (`@SubscribeMessage()`), dependency injection, guards, interceptors, and pipes.

---

## 2. Theoretical Foundations

### 2.1 The WebSocket Protocol Handshake (RFC 6455)
A WebSocket connection begins as a standard HTTP/1.1 request:
```http
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13
```
If accepted, the server responds with `HTTP/1.1 101 Switching Protocols`, transforming the TCP socket into a bidirectional binary/text framing channel.

### 2.2 Gateways vs Controllers
| Feature | HTTP Controller | WebSocket Gateway |
|---|---|---|
| Protocol | Stateless HTTP Request/Response | Stateful Persistent TCP Socket |
| Execution Model | One execution per HTTP call | Continuous session with multiple event frames |
| Routing Decorators | `@Get()`, `@Post()`, `@Param()` | `@SubscribeMessage()`, `@ConnectedSocket()`, `@MessageBody()` |
| Lifecycle | Request scoped or Singleton | Socket lifecycle (`OnGatewayConnection`, `OnGatewayDisconnect`) |

---

## 3. Architecture & Internal Mechanics

```
Browser Client (WebSocket)
          │
          ├─> Connects: ws://localhost:3000/events
          │
          ▼
NestJS WebSocket Gateway Layer
          │
          ├─> handleConnection(clientSocket)
          │
          ▼
Event Router (@SubscribeMessage('chat:message'))
          │
          ├─> Pipes (Validation: DTO)
          ├─> Guards (Authentication & Permissions)
          │
          ▼
Gateway Handler Method
          │
          ├─> Updates Database
          ├─> Broadcasts to Room / Clients via server.to('room').emit(...)
          │
          ▼
Connected Clients receive push frame
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

NestJS supports two primary WebSocket adapters: **Socket.io** (default, feature-rich) and **ws** (lightweight, pure RFC 6455):
```bash
npm install --save @nestjs/websockets @nestjs/platform-socket.io
npm install --save-dev @types/socket.io
```

---

### 4.2 Building a Real-Time Chat Gateway

```typescript
// src/chat/chat.gateway.ts
import {
  WebSocketGateway,
  SubscribeMessage,
  MessageBody,
  ConnectedSocket,
  WebSocketServer,
  OnGatewayInit,
  OnGatewayConnection,
  OnGatewayDisconnect,
} from '@nestjs/websockets';
import { Logger, UsePipes, ValidationPipe } from '@nestjs/common';
import { Server, Socket } from 'socket.io';

export interface ChatMessageDto {
  room: string;
  sender: string;
  text: string;
}

@WebSocketGateway(3001, {
  cors: {
    origin: '*',
  },
  namespace: 'chat',
})
export class ChatGateway
  implements OnGatewayInit, OnGatewayConnection, OnGatewayDisconnect
{
  @WebSocketServer()
  server!: Server;

  private readonly logger = new Logger(ChatGateway.name);

  afterInit(server: Server) {
    this.logger.log('Chat WebSocket Gateway initialized on port 3001');
  }

  handleConnection(client: Socket) {
    this.logger.log(`Client connected: ${client.id}`);
  }

  handleDisconnect(client: Socket) {
    this.logger.log(`Client disconnected: ${client.id}`);
  }

  // 1. Subscribe to join-room events
  @SubscribeMessage('joinRoom')
  handleJoinRoom(
    @ConnectedSocket() client: Socket,
    @MessageBody('room') room: string,
  ) {
    client.join(room);
    this.logger.log(`Client ${client.id} joined room: ${room}`);

    // Notify room of new member
    client.to(room).emit('userJoined', { clientId: client.id, room });
    return { status: 'joined', room };
  }

  // 2. Subscribe to incoming messages
  @UsePipes(new ValidationPipe())
  @SubscribeMessage('sendMessage')
  handleMessage(
    @ConnectedSocket() client: Socket,
    @MessageBody() payload: ChatMessageDto,
  ) {
    this.logger.log(`Message from ${payload.sender} to ${payload.room}: ${payload.text}`);

    // Broadcast message to everyone in the room (including sender)
    this.server.to(payload.room).emit('newMessage', {
      ...payload,
      timestamp: new Date().toISOString(),
    });

    // Return acknowledgment back to sender
    return { delivered: true };
  }
}
```

---

### 4.3 Registering the Gateway in a Module

```typescript
// src/chat/chat.module.ts
import { Module } from '@nestjs/common';
import { ChatGateway } from './chat.gateway';

@Module({
  providers: [ChatGateway],
  exports: [ChatGateway],
})
export class ChatModule {}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Port Sharing vs Dedicated Port
Passing a port number to `@WebSocketGateway(3001)` creates a separate raw HTTP server listening on port 3001. Omitting the port (`@WebSocketGateway()`) binds the WebSocket engine directly to the primary HTTP application server (e.g. port 3000), sharing TLS certificates and port configurations cleanly.

### 5.2 Exception Filters in WebSocket Gateways
Standard `HttpException` filters do not catch WebSocket errors properly because `host.switchToHttp()` fails.
- **Handling**: Use `@Catch(WsException)` and access `host.switchToWs()`.

---

## 6. Performance & Memory Implications

- **Connection Density**: Each active WebSocket connection maintains an open TCP socket and file descriptor. In Node.js, 10,000 idle connections consume ~150–200 MB of RAM.
- **Heartbeats & Ping/Pong**: Socket.io maintains ping/pong intervals (default 25s) to detect dropped mobile connections and prevent stale sockets from leaking memory.

---

## 7. Anti-Patterns & Common Mistakes

- **Blocking Handlers with Heavy Computations**: Running synchronous hashing or heavy database loops inside `@SubscribeMessage()`, freezing real-time packets for all other connected users.
- **Emitting Events to Non-Existent Rooms**: Emitting `server.to(room)` without validating if the room string is sanitized.
- **Bypassing Nest Dependency Injection**: Instantiating raw Socket.io `io(server)` manually in `main.ts` instead of using declarative NestJS `@WebSocketGateway()` classes.

---

## 8. Production Checklist & Operational Readiness

- [ ] CORS policies configured strictly to authorized client domains.
- [ ] Authentication executed during the initial connection handshake.
- [ ] Ping interval and ping timeout tuned for mobile network resilience.
- [ ] Gateway bound to shared HTTP port to simplify reverse proxy / load balancer routing.

---

## 9. Migration & Version History

- **NestJS 8/9**: Standardized `@WebSocketServer()` and updated to Socket.io 4.x.
- **NestJS 11.x**: Validated with Express 5 HTTP upgrade server handlers.

---

## 10. Verification & Testing

Testing gateway event emission using a Socket.io client:

```typescript
import { Test } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import { io, Socket as ClientSocket } from 'socket.io-client';
import { ChatGateway } from './chat.gateway';

describe('ChatGateway (e2e)', () => {
  let app: INestApplication;
  let clientSocket: ClientSocket;

  beforeAll(async () => {
    const module = await Test.createTestingModule({
      providers: [ChatGateway],
    }).compile();

    app = module.createNestApplication();
    await app.listen(3001);
  });

  afterAll(async () => {
    await app.close();
  });

  beforeEach((done) => {
    clientSocket = io('http://localhost:3001/chat', { transports: ['websocket'] });
    clientSocket.on('connect', done);
  });

  afterEach(() => {
    if (clientSocket.connected) {
      clientSocket.disconnect();
    }
  });

  it('joins room and receives acknowledgment', (done) => {
    clientSocket.emit('joinRoom', { room: 'general' }, (response: any) => {
      expect(response.status).toBe('joined');
      expect(response.room).toBe('general');
      done();
    });
  });
});
```

---

## 11. Comprehensive Examples

### Custom WebSocket Exception Filter

```typescript
// src/common/filters/ws-exception.filter.ts
import { Catch, ArgumentsHost } from '@nestjs/common';
import { BaseWsExceptionFilter, WsException } from '@nestjs/websockets';
import { Socket } from 'socket.io';

@Catch(WsException)
export class CustomWsExceptionFilter extends BaseWsExceptionFilter {
  catch(exception: WsException, host: ArgumentsHost) {
    const client = host.switchToWs().getClient<Socket>();
    const error = exception.getError();
    const details = typeof error === 'string' ? { message: error } : error;

    // Send error event back to the specific emitting client
    client.emit('exception', {
      status: 'error',
      ...details,
      timestamp: new Date().toISOString(),
    });
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-socketio.md](./02-socketio.md) - Deep dive into Socket.io
- [03-websocket-authentication.md](./03-websocket-authentication.md) - Authenticating real-time connections
- [05-scaling-websockets.md](./05-scaling-websockets.md) - Redis adapter for cluster scaling
- [NestJS WebSockets Guide](https://docs.nestjs.com/websockets/gateways)
