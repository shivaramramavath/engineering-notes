# 05-04 — Real-Time Systems, WebSockets, and Streaming

Real-time architectures enable live collaborative applications, trading platforms, multiplayer gaming, chat, and streaming AI responses.

This module provides a comprehensive guide to real-time engineering in NestJS: from bidirectional WebSockets with **Socket.IO** and **ws** to horizontal scaling with **Redis adapters**, handshake authentication, room multi-tenancy, and unidirectional **Server-Sent Events (SSE)**.

---

## Module Index

| # | File | Core Topics |
|---|---|---|
| 01 | [Gateways & Events](./01-gateways-and-events.md) | `@WebSocketGateway`, `@SubscribeMessage`, lifecycle hooks, custom exception filters |
| 02 | [Socket.IO Integration](./02-socketio.md) | Engine.IO transport fallbacks, client acknowledgments, custom `IoAdapter` |
| 03 | [WebSocket Authentication](./03-websocket-authentication.md) | Handshake token verification, `socket.data.user`, `WsJwtGuard`, `@WsUser` |
| 04 | [Rooms & Namespaces](./04-rooms-and-namespaces.md) | Namespace multiplexing, multi-tenant room isolation, dynamic join/leave |
| 05 | [Scaling WebSockets](./05-scaling-websockets.md) | `@socket.io/redis-adapter`, multi-pod cluster broadcasting, sticky sessions, OS limits |
| 06 | [SSE & HTTP Streaming](./06-server-sent-events-and-streaming.md) | `@Sse()`, `EventSource`, HTTP/2 multiplexing, LLM token streaming |

---

## Key Learning Outcomes

1. Structure real-time gateways using idiomatic NestJS dependency injection and guards.
2. Secure persistent real-time connections via handshake JWT verification.
3. Isolate multi-tenant customer data using cryptographically partitioned room namespaces.
4. Scale WebSockets across dozens of Kubernetes pods using Redis Pub/Sub adapters.
5. Choose appropriately between WebSockets and Server-Sent Events based on directionality.
