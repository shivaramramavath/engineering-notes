# Rooms, Namespaces, and Multi-Tenancy in WebSockets

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 04-realtime
- **Prerequisites**: [01-gateways-and-events](./01-gateways-and-events.md), [02-socketio](./02-socketio.md), [03-websocket-authentication](./03-websocket-authentication.md)

---

## 1. Context & Purpose

In large-scale real-time applications, broadcasting messages to every connected client is inefficient and breaches security. Different domains—such as customer support chats, real-time analytics dashboards, private direct messages, and multi-tenant SaaS organizations—require distinct boundaries.

Socket.IO provides two hierarchical organizational primitives:
1. **Namespaces (`/orders`, `/chat`, `/admin`)**: Multiplex distinct communication channels over the same underlying TCP connection, each with its own authentication middleware, event routing, and connection pool.
2. **Rooms (`room:org-100`, `room:chat-42`)**: Dynamic server-side groupings within a namespace that allow targeted broadcasting to specific subsets of connected sockets.

---

## 2. Theoretical Foundations

### 2.1 Namespaces vs Rooms
| Dimension | Namespaces | Rooms |
|---|---|---|
| Scope | Macro communication channel (`/admin` vs `/customer`) | Dynamic subgroup within a namespace |
| Connection | Separate connection lifecycle & handshake | Internal server-side socket set |
| Client Control | Client explicitly connects: `io('/admin')` | Server manages membership: `socket.join(room)` |
| Middleware & Auth | Handshake authentication isolated per namespace | Managed within the namespace scope |

### 2.2 Dynamic Multi-Tenant Rooms
In multi-tenant SaaS architectures, tenants must never receive each other's updates. When a user connects, the server inspects their authenticated tenant ID and automatically joins their socket to `tenant:${tenantId}`. Broadcasts scoped to this room remain completely isolated from other tenants.

---

## 3. Architecture & Internal Mechanics

```
Single TCP Connection (Client Browser)
               │
   ┌───────────┴───────────┐
   ▼                       ▼
Namespace: `/public`    Namespace: `/enterprise`
   │                       │
   │                ┌──────┴──────┐
   │                ▼             ▼
   │          Room: `org-101`  Room: `org-102`
   │                │             │
   ▼                ▼             ▼
Broadcast        Broadcast     Broadcast
All Public       Only Org 101  Only Org 102
```

---

## 4. Concrete Implementation

### 4.1 Multiplexing Namespaces with Multiple Gateways

NestJS allows declaring separate Gateway classes for each namespace:

```typescript
// src/chat/public-chat.gateway.ts
import { WebSocketGateway, SubscribeMessage, MessageBody } from '@nestjs/websockets';

@WebSocketGateway({ namespace: 'public' })
export class PublicChatGateway {
  @SubscribeMessage('ping')
  handlePing() {
    return 'pong-from-public';
  }
}
```

```typescript
// src/admin/admin.gateway.ts
import { WebSocketGateway, SubscribeMessage, UseGuards } from '@nestjs/websockets';
import { WsAdminGuard } from './guards/ws-admin.guard';

// Distinct namespace with dedicated Admin guard
@WebSocketGateway({ namespace: 'admin' })
@UseGuards(WsAdminGuard)
export class AdminGateway {
  @SubscribeMessage('getSystemStats')
  handleStats() {
    return { memory: process.memoryUsage(), cpu: process.cpuUsage() };
  }
}
```

---

### 4.2 Multi-Tenant Room Isolation Pattern

```typescript
// src/tenants/tenant-events.gateway.ts
import {
  WebSocketGateway,
  WebSocketServer,
  OnGatewayConnection,
  SubscribeMessage,
  MessageBody,
  ConnectedSocket,
} from '@nestjs/websockets';
import { Logger } from '@nestjs/common';
import { Server, Socket } from 'socket.io';

@WebSocketGateway({ namespace: 'tenants' })
export class TenantEventsGateway implements OnGatewayConnection {
  @WebSocketServer()
  server!: Server;

  private readonly logger = new Logger(TenantEventsGateway.name);

  handleConnection(client: Socket) {
    const tenantId = client.handshake.auth?.tenantId;
    const userId = client.handshake.auth?.userId;

    if (!tenantId) {
      client.disconnect(true);
      return;
    }

    // 1. Join tenant-wide broadcast room
    client.join(`tenant:${tenantId}`);

    // 2. Join private user room for direct user notifications
    client.join(`user:${userId}`);

    this.logger.log(`Client ${client.id} joined rooms: tenant:${tenantId} & user:${userId}`);
  }

  // Broadcast to all users within the same tenant
  broadcastToTenant(tenantId: string, event: string, payload: any) {
    this.server.to(`tenant:${tenantId}`).emit(event, payload);
  }

  // Send private notification to a specific user across all their devices
  sendToUser(userId: string, event: string, payload: any) {
    this.server.to(`user:${userId}`).emit(event, payload);
  }

  // Client requests joining a specific document room
  @SubscribeMessage('joinDocument')
  handleJoinDocument(
    @ConnectedSocket() client: Socket,
    @MessageBody('documentId') docId: string,
  ) {
    const roomName = `doc:${docId}`;
    client.join(roomName);

    // Notify other collaborators in the room
    client.to(roomName).emit('collaboratorJoined', {
      clientId: client.id,
      docId,
    });

    return { status: 'joined', room: roomName };
  }

  @SubscribeMessage('leaveDocument')
  handleLeaveDocument(
    @ConnectedSocket() client: Socket,
    @MessageBody('documentId') docId: string,
  ) {
    const roomName = `doc:${docId}`;
    client.leave(roomName);
    client.to(roomName).emit('collaboratorLeft', { clientId: client.id, docId });
    return { status: 'left', room: roomName };
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Stale Rooms on Disconnect
When a socket disconnects, Socket.io automatically purges the socket from all rooms it was a member of. However, if you maintain custom in-memory tracking maps (e.g. `Map<room, users[]>`), you must remove the user explicitly inside `handleDisconnect()`.

### 5.2 Room Name Collision Across Tenants
If Tenant A and Tenant B both create documents with ID `123`, joining the room `'123'` without namespacing causes cross-tenant data leakage.
- **Rule**: Always prefix room names with the tenant ID:
  `room:tenant:${tenantId}:doc:${docId}`.

---

## 6. Performance & Memory Implications

- **Room Data Structure**: In Socket.io, rooms are implemented using native JavaScript `Set` structures inside the adapter. Adding and removing sockets from rooms occurs in $O(1)$ time.
- **Broadcasting Efficiency**: Calling `server.to(room).emit()` iterates only over the sockets present in that specific room Set, avoiding $O(N)$ full-server broadcasts.

---

## 7. Anti-Patterns & Common Mistakes

- **Creating Dynamic Namespaces per Tenant**: Using dynamic regex namespaces (`/tenant-.*`) for thousands of tenants. Namespaces are expensive and hold distinct multiplexing state. Use **Rooms** within a single shared namespace instead.
- **Client-Controlled Room Joining Without Verification**: Allowing clients to join arbitrary room names (`admin_room`) without verifying user roles. Always enforce authorization guards before calling `client.join()`.

---

## 8. Production Checklist & Operational Readiness

- [ ] Room names prefixed with tenant IDs to guarantee cross-tenant isolation.
- [ ] Room join events protected with authentication and authorization checks.
- [ ] Automatic socket cleanup verified upon client disconnect.
- [ ] Redis adapter configured if rooms span multiple server pods.

---

## 9. Migration & Version History

- **Socket.io 4.x**: Improved room membership sets and support for arbitrary room sets.

---

## 10. Verification & Testing

Unit testing room joining logic:

```typescript
import { TenantEventsGateway } from './tenant-events.gateway';

describe('TenantEventsGateway (Rooms)', () => {
  let gateway: TenantEventsGateway;

  beforeEach(() => {
    gateway = new TenantEventsGateway();
  });

  it('joins document room and notifies existing collaborators', () => {
    const mockClientTo = { emit: vi.fn() };
    const mockClient: any = {
      id: 'sock_1',
      join: vi.fn(),
      to: vi.fn().mockReturnValue(mockClientTo),
    };

    const res = gateway.handleJoinDocument(mockClient, 'doc_999');

    expect(res.status).toBe('joined');
    expect(mockClient.join).toHaveBeenCalledWith('doc:doc_999');
    expect(mockClient.to).toHaveBeenCalledWith('doc:doc_999');
    expect(mockClientTo.emit).toHaveBeenCalledWith('collaboratorJoined', {
      clientId: 'sock_1',
      docId: 'doc_999',
    });
  });
});
```

---

## 11. Comprehensive Examples

### Inspecting Sockets in a Room Programmatically

```typescript
// src/chat/room-inspector.service.ts
import { Injectable } from '@nestjs/common';
import { Server } from 'socket.io';

@Injectable()
export class RoomInspectorService {
  async getConnectedUserCount(server: Server, roomName: string): Promise<number> {
    // Fetches all socket IDs in the room across the entire adapter
    const sockets = await server.in(roomName).allSockets();
    return sockets.size;
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-gateways-and-events.md](./01-gateways-and-events.md) - Gateway basics
- [03-websocket-authentication.md](./03-websocket-authentication.md) - Tenant authentication
- [05-scaling-websockets.md](./05-scaling-websockets.md) - Scaling rooms across clusters
- [Socket.IO Rooms Guide](https://socket.io/docs/v4/rooms/)
