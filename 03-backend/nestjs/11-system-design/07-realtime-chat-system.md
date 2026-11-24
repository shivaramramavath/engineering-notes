---
title: "System Design: Realtime Chat and Messaging Platform"
description: "Designing a Slack/Discord-style realtime messaging platform supporting 1M concurrent connections with NestJS and WebSockets"
tags: [nestjs, system-design, chat, websockets, socketio, redis-adapter]
---

# System Design: Realtime Chat and Messaging Platform

## Architecture
- **WebSocket Gateway**: NestJS `WebSocketGateway` with Socket.IO.
- **Horizontal Scaling**: Multi-pod cluster connected via `@socket.io/redis-adapter` for cross-node message routing.
- **Message Storage**: Cassandra or ScyllaDB for high-velocity chat history; PostgreSQL for user relationships.
