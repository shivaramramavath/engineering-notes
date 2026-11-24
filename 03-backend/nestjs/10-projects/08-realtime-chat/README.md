# Project 08: Realtime Collaborative Chat Platform (`08-realtime-chat`)

> Multi-room real-time chat service built with Socket.IO, `@socket.io/redis-adapter` for horizontal scaling across Kubernetes pods, and JWT handshake authentication.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-08-realtime-chat` |
| **WebSockets** | NestJS WebSocketGateway (Socket.IO) |
| **Scaling** | Redis Adapter (Pub/Sub across cluster instances) |
| **Database** | MongoDB 7 / PostgreSQL 16 (Chat History) |
| **Complexity Level** | Advanced Realtime Reference |

---

## 1. Clustered WebSocket Gateway

```typescript
// src/chat/chat.gateway.ts
import {
  WebSocketGateway,
  WebSocketServer,
  SubscribeMessage,
  MessageBody,
  ConnectedSocket,
  OnGatewayConnection,
} from '@nestjs/websockets';
import { Server, Socket } from 'socket.io';
import { UseGuards, Logger } from '@nestjs/common';
import { WsJwtGuard } from '../auth/ws-jwt.guard';

@WebSocketGateway({
  cors: { origin: '*' },
  namespace: 'chat',
})
export class ChatGateway implements OnGatewayConnection {
  @WebSocketServer()
  server: Server;

  private readonly logger = new Logger(ChatGateway.name);

  handleConnection(client: Socket) {
    this.logger.log(`Client connected: ${client.id}`);
  }

  @UseGuards(WsJwtGuard)
  @SubscribeMessage('joinRoom')
  handleJoinRoom(@ConnectedSocket() client: Socket, @MessageBody('roomId') roomId: string) {
    client.join(roomId);
    this.server.to(roomId).emit('userJoined', { clientId: client.id, roomId });
  }

  @UseGuards(WsJwtGuard)
  @SubscribeMessage('sendMessage')
  handleMessage(
    @ConnectedSocket() client: Socket,
    @MessageBody() payload: { roomId: string; message: string },
  ) {
    // Broadcasts across all instances via Redis Adapter
    this.server.to(payload.roomId).emit('newMessage', {
      senderId: (client as any).user?.id,
      text: payload.message,
      timestamp: new Date().toISOString(),
    });
  }
}
```
