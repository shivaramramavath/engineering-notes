# WebSockets & Socket.IO Cheat Sheet

```typescript
@WebSocketGateway({ cors: { origin: '*' } })
export class ChatGateway {
  @WebSocketServer() server: Server;

  @SubscribeMessage('msg')
  handleMsg(@ConnectedSocket() client: Socket, @MessageBody() data: string) {
    client.broadcast.emit('msg', data);
  }
}
```
