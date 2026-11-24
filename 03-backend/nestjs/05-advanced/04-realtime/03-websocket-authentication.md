# WebSocket Authentication and Authorization

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 04-realtime
- **Prerequisites**: [01-gateways-and-events](./01-gateways-and-events.md), [04-intermediate/06-authentication/02-jwt-authentication](../../04-intermediate/06-authentication/02-jwt-authentication.md)

---

## 1. Context & Purpose

Authenticating WebSocket connections is fundamentally different from authenticating stateless HTTP requests:
1. **HTTP**: Every request passes an `Authorization: Bearer <token>` header, evaluated independently by guards.
2. **WebSocket**: The browser `WebSocket` constructor does not permit setting custom HTTP request headers. Furthermore, authenticating every single real-time frame via database lookup introduces immense CPU overhead.

The industry-standard architecture performs **Handshake Authentication**: the client passes an authentication token (JWT) during the initial connection handshake (via `auth` payload, query parameters, or `HttpOnly` cookies). The gateway verifies the token once during connection, attaching the authenticated user context to `socket.data.user` for the remainder of the persistent session.

---

## 2. Theoretical Foundations

### 2.1 The Authentication Lifecycle
1. **Handshake Verification**: Client connects and presents credentials (`client.handshake.auth.token`).
2. **Validation**: Server verifies JWT signature and expiry.
   - If invalid: Reject connection immediately via `client.disconnect(true)` or throw `WsException`.
   - If valid: Attach user payload to `socket.data.user`.
3. **Session-Long Identity**: Subsequent message events (`@SubscribeMessage`) retrieve user identity directly from `client.data.user` without re-verifying signatures.
4. **Token Expiration during Connection**: If a connection remains open for 8 hours, long-lived JWTs expire. Workers must listen to refresh events or schedule disconnections when tokens expire.

---

## 3. Architecture & Internal Mechanics

```
Browser Client (Socket.IO)
io('http://localhost:3000', { auth: { token: 'jwt.token.here' } })
               │
               ▼
   handleConnection(socket)
               │
               ├─> Extracts token from socket.handshake.auth
               │
               ▼
       JwtService.verify(token)
               │
       ┌───────┴───────┐
       ▼               ▼
   [Valid]         [Invalid]
       │               │
       │               ▼
       │      socket.disconnect(true)
       │      (Terminate TCP Socket)
       ▼
socket.data.user = userPayload
       │
       ▼
@SubscribeMessage('sendMessage')
Handler reads socket.data.user (Zero DB Lookups!)
```

---

## 4. Concrete Implementation

### 4.1 Handshake Authentication in Gateway

```typescript
// src/chat/authenticated-chat.gateway.ts
import {
  WebSocketGateway,
  OnGatewayConnection,
  OnGatewayDisconnect,
  WebSocketServer,
  SubscribeMessage,
  MessageBody,
  ConnectedSocket,
  WsException,
} from '@nestjs/websockets';
import { Logger, UseGuards } from '@nestjs/common';
import { Server, Socket } from 'socket.io';
import { JwtService } from '@nestjs/jwt';
import { WsJwtGuard } from './guards/ws-jwt.guard';

@WebSocketGateway({ namespace: 'chat' })
export class AuthenticatedChatGateway
  implements OnGatewayConnection, OnGatewayDisconnect
{
  @WebSocketServer()
  server!: Server;

  private readonly logger = new Logger(AuthenticatedChatGateway.name);

  constructor(private readonly jwtService: JwtService) {}

  // 1. Authenticate during initial connection handshake
  async handleConnection(socket: Socket) {
    try {
      // Extract token from auth object, query, or headers
      const token =
        socket.handshake.auth?.token ||
        socket.handshake.headers['authorization']?.split(' ')[1] ||
        socket.handshake.query?.token;

      if (!token || typeof token !== 'string') {
        this.logger.warn(`Connection rejected: Missing token for socket ${socket.id}`);
        socket.disconnect(true);
        return;
      }

      // Verify JWT cryptographic signature
      const payload = await this.jwtService.verifyAsync(token, {
        secret: process.env.JWT_SECRET || 'jwt-secret',
      });

      // Attach user object to socket session data
      socket.data.user = {
        id: payload.sub,
        email: payload.email,
        roles: payload.roles || [],
      };

      this.logger.log(`User ${payload.email} connected on socket ${socket.id}`);
    } catch (err: any) {
      this.logger.warn(`Connection rejected: Invalid token (${err.message})`);
      socket.disconnect(true);
    }
  }

  handleDisconnect(socket: Socket) {
    if (socket.data?.user) {
      this.logger.log(`User ${socket.data.user.email} disconnected`);
    }
  }

  // 2. Protected event handler using custom WsJwtGuard
  @UseGuards(WsJwtGuard)
  @SubscribeMessage('postMessage')
  handlePostMessage(
    @ConnectedSocket() socket: Socket,
    @MessageBody('content') content: string,
  ) {
    const user = socket.data.user;
    this.logger.log(`Message received from user ${user.email}: ${content}`);

    return { status: 'sent', sender: user.email };
  }
}
```

---

### 4.2 Building a Type-Safe `WsJwtGuard`

```typescript
// src/chat/guards/ws-jwt.guard.ts
import { CanActivate, ExecutionContext, Injectable } from '@nestjs/common';
import { WsException } from '@nestjs/websockets';
import { Socket } from 'socket.io';

@Injectable()
export class WsJwtGuard implements CanActivate {
  canActivate(context: ExecutionContext): boolean {
    const client = context.switchToWs().getClient<Socket>();
    const user = client.data?.user;

    if (!user) {
      throw new WsException('Unauthorized: Missing valid session context');
    }

    return true;
  }
}
```

---

### 4.3 Custom Param Decorator: `@WsUser()`

Extract authenticated user data cleanly in handler signatures:

```typescript
// src/common/decorators/ws-user.decorator.ts
import { createParamDecorator, ExecutionContext } from '@nestjs/common';
import { Socket } from 'socket.io';

export const WsUser = createParamDecorator(
  (data: string | undefined, ctx: ExecutionContext) => {
    const socket = ctx.switchToWs().getClient<Socket>();
    const user = socket.data?.user;
    return data ? user?.[data] : user;
  },
);
```

Usage in gateway:
```typescript
@SubscribeMessage('editProfile')
handleEditProfile(
  @WsUser() user: any,
  @MessageBody() updateDto: any,
) {
  console.log(`Editing profile for user ID: ${user.id}`);
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Passing Tokens in Query Parameters
If clients pass `?token=eyJ...` in the URL, tokens appear in plain text in reverse proxy access logs (Nginx / Cloudflare / AWS CloudWatch).
- **Remedy**: Always pass tokens inside the `auth` payload:
  `io({ auth: { token: '...' } })`. Socket.io transmits this in the HTTP POST body or initial WebSocket header, avoiding query parameter URL leakage.

### 5.2 Token Revocation and User Banning
If an admin bans a user or revokes their session in the database, existing WebSocket connections remain active unless explicitly disconnected.
- **Handling**: Listen to Redis Pub/Sub `user.revoked` events and disconnect the target socket across all cluster nodes.

---

## 6. Performance & Memory Implications

- **Handshake Verification**: Cryptographic JWT verification runs **once** per connection, not per message. This allows a single gateway node to process tens of thousands of messages per second with minimal CPU load.
- **Memory Footprint**: Attaching minimal user properties (`id`, `roles`) to `socket.data` consumes $< 1\text{KB}$ of RAM per connection.

---

## 7. Anti-Patterns & Common Mistakes

- **Re-Verifying JWT on Every Event**: Calling `jwtService.verify(token)` inside every `@SubscribeMessage()`. This degrades performance under high message throughput.
- **Using HTTP Guards in WebSockets**: Attaching `@UseGuards(AuthGuard('jwt'))` designed for Express HTTP. It attempts to read `req.headers` and crashes with `TypeError: req.get is not a function`.
- **Leaving Unauthenticated Sockets Connected**: Forgetting to call `socket.disconnect(true)` when validation fails. The client remains connected to the server and can consume resources.

---

## 8. Production Checklist & Operational Readiness

- [ ] Tokens passed via Socket.io `auth: { token }` payload, avoiding plain query params.
- [ ] Failed handshake authentication triggers `socket.disconnect(true)`.
- [ ] Role-based authorization verified on sensitive message events.
- [ ] Socket disconnection mechanism implemented for banned or logged-out users.

---

## 9. Migration & Version History

- **Socket.io 3+**: Introduced `socket.handshake.auth` as the official authentication channel.
- **NestJS 11.x**: Validated with `@nestjs/jwt` v11 and asynchronous token verification.

---

## 10. Verification & Testing

Unit testing `WsJwtGuard`:

```typescript
import { WsJwtGuard } from './ws-jwt.guard';
import { WsException } from '@nestjs/websockets';

describe('WsJwtGuard', () => {
  let guard: WsJwtGuard;

  beforeEach(() => {
    guard = new WsJwtGuard();
  });

  it('allows access if socket.data.user exists', () => {
    const mockContext: any = {
      switchToWs: () => ({
        getClient: () => ({ data: { user: { id: 'u1' } } }),
      }),
    };

    expect(guard.canActivate(mockContext)).toBe(true);
  });

  it('throws WsException if user is missing', () => {
    const mockContext: any = {
      switchToWs: () => ({
        getClient: () => ({ data: {} }),
      }),
    };

    expect(() => guard.canActivate(mockContext)).toThrow(WsException);
  });
});
```

---

## 11. Comprehensive Examples

### Client-Side Connection Example (Socket.io Client)

```javascript
// client/app.js
import { io } from 'socket.io-client';

const token = localStorage.getItem('access_token');

const socket = io('https://api.example.com/chat', {
  auth: {
    token: token, // Sent securely in the handshake
  },
  transports: ['websocket'],
});

socket.on('connect', () => {
  console.log('Connected and authenticated with socket ID:', socket.id);
});

socket.on('connect_error', (err) => {
  console.error('Authentication failed:', err.message);
  // Redirect to login page
});
```

---

## 12. Cross-References & Further Reading

- [01-gateways-and-events.md](./01-gateways-and-events.md) - Gateway basics
- [04-intermediate/06-authentication/02-jwt-authentication.md](../../04-intermediate/06-authentication/02-jwt-authentication.md) - JWT token issuance
- [Socket.IO Middlewares and Auth Guide](https://socket.io/docs/v4/middlewares/)
