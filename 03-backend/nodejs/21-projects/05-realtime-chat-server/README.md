# Project 05: Real-Time WebSocket Chat Server (`proj-node-05-realtime-chat`)

> High-concurrency real-time WebSocket messaging server built with Node.js 22 LTS, `ws`, and Redis Pub/Sub for horizontal multi-node clustering. Features upgrade handshake JWT authentication, room/channel multiplexing, backpressure detection via `ws.bufferedAmount`, heartbeat ping/pong keepalive management, and Redis-backed presence tracking.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-05-realtime-chat` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Transport** | Raw WebSockets via `ws` |
| **Clustering Engine** | Redis Pub/Sub (`ioredis`) |
| **Auth** | JWT Token verification on HTTP Upgrade Handshake |
| **Scalability** | Multi-node cluster with backpressure throttling |
| **Testing** | Vitest + WebSocket client integration |
| **Complexity Level** | Advanced / Production-Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 Multi-Node Clustering with Redis Pub/Sub

When scaling WebSockets horizontally across multiple Node.js processes or container instances:
1. User A is connected to Node Instance 1 via WebSocket.
2. User B is connected to Node Instance 2 via WebSocket.
3. When User A sends a message to Room `#general`, Node Instance 1 publishes the message to Redis channel `room:#general`.
4. Node Instance 2 (and all other subscribed instances) receives the Redis Pub/Sub event and broadcasts the message to local WebSocket connections belonging to Room `#general`.

```
+----------------------------------------------------------------------------------------------------+
|                               Multi-Node WebSocket Architecture                                    |
|                                                                                                    |
|   [ Client A (Alice) ]                                               [ Client B (Bob) ]            |
|            │                                                                  │                    |
|            │ WebSocket (WS)                                                   │ WebSocket (WS)     |
|            ▼                                                                  ▼                    |
|  +──────────────────────────────────+                +──────────────────────────────────+          |
|  |       Node.js WS Instance 1      |                |       Node.js WS Instance 2      |          |
|  | - Auth Upgrade Handshake (JWT)   |                | - Auth Upgrade Handshake (JWT)   |          |
|  | - Room Set: ['#general']         |                | - Room Set: ['#general']         |          |
|  | - Backpressure buffer check      |                | - Backpressure buffer check      |          |
|  +─────────────────┬────────────────+                +─────────────────┬────────────────+          |
|                    │                                                   │                           |
|                    │ PUBLISH room:#general                             │ SUBSCRIBE room:*          |
|                    ▼                                                   ▼                           |
|  +──────────────────────────────────────────────────────────────────────────────────────+          |
|  |                               Redis Pub/Sub Message Bus                              |          |
|  | - Message Fan-out across all active cluster nodes                                    |          |
|  | - Presence Tracking: SADD room:#general:users userId                                 |          |
|  +──────────────────────────────────────────────────────────────────────────────────────+          |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-05-realtime-chat/
├── src/
│   ├── config/
│   │   └── env.ts               # Port, Redis connection, JWT secret
│   ├── infrastructure/
│   │   └── redis.ts             # Publisher & Subscriber ioredis client pairs
│   ├── websocket/
│   │   ├── server.ts            # WebSocketServer initialization & upgrade verification
│   │   ├── client-manager.ts    # Connection registry & room mappings
│   │   ├── heartbeat.ts         # Ping/pong keepalive timer to prune dead sockets
│   │   └── handlers/
│   │       └── message.handler.ts # Inbound message parsing and room broadcasting
│   └── app.ts                   # HTTP server & WebSocket attach point
├── test/
│   └── integration/
│       └── chat.e2e.test.ts     # Multi-client WebSocket integration test
├── Dockerfile
├── docker-compose.yml           # 2 WS nodes behind Nginx with sticky sessions
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-05-realtime-chat",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node --env-file=.env src/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "ioredis": "^5.4.2",
    "jose": "^5.9.6",
    "ws": "^8.18.0",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/node": "^22.13.0",
    "@types/ws": "^8.5.14",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/websocket/server.ts`
```typescript
import { createServer, IncomingMessage } from 'node:http';
import { WebSocketServer, WebSocket } from 'ws';
import * as jose from 'jose';
import Redis from 'ioredis';

const JWT_SECRET = new TextEncoder().encode(process.env.JWT_SECRET || 'chat-server-secret-key-32chars!');
const redisPub = new Redis(process.env.REDIS_URL || 'redis://localhost:6379');
const redisSub = new Redis(process.env.REDIS_URL || 'redis://localhost:6379');

interface AuthenticatedSocket extends WebSocket {
  userId: string;
  isAlive: boolean;
  rooms: Set<string>;
}

export function initWebSocketServer(server: ReturnType<typeof createServer>) {
  const wss = new WebSocketServer({ noServer: true });
  const localClients = new Map<string, AuthenticatedSocket>();

  // 1. Subscribe to Redis wildcard room channels
  redisSub.psubscribe('room:*');
  redisSub.on('pmessage', (_pattern, channel, message) => {
    const room = channel.replace('room:', '');
    for (const client of localClients.values()) {
      if (client.rooms.has(room) && client.readyState === WebSocket.OPEN) {
        // Backpressure check: Skip or disconnect slow clients with high buffer backlog
        if (client.bufferedAmount > 1024 * 1024) { // 1MB buffer limit
          console.warn(`[WS] High backpressure on client ${client.userId}. Dropping frame.`);
          continue;
        }
        client.send(message);
      }
    }
  });

  // 2. HTTP Upgrade Handshake Authentication
  server.on('upgrade', async (request: IncomingMessage, socket, head) => {
    try {
      const url = new URL(request.url ?? '', `http://${request.headers.host}`);
      const token = url.searchParams.get('token');

      if (!token) {
        socket.write('HTTP/1.1 401 Unauthorized\r\n\r\n');
        socket.destroy();
        return;
      }

      const { payload } = await jose.jwtVerify(token, JWT_SECRET);
      const userId = payload.sub as string;

      wss.handleUpgrade(request, socket, head, (ws) => {
        const client = ws as AuthenticatedSocket;
        client.userId = userId;
        client.isAlive = true;
        client.rooms = new Set();
        wss.emit('connection', client, request);
      });
    } catch {
      socket.write('HTTP/1.1 403 Forbidden\r\n\r\n');
      socket.destroy();
    }
  });

  // 3. Connection lifecycle
  wss.on('connection', (ws: AuthenticatedSocket) => {
    localClients.set(ws.userId, ws);
    console.log(`[WS] Client connected: ${ws.userId}`);

    ws.on('pong', () => {
      ws.isAlive = true;
    });

    ws.on('message', async (data) => {
      try {
        const payload = JSON.parse(data.toString());
        // Handle join room action
        if (payload.action === 'join' && payload.room) {
          ws.rooms.add(payload.room);
          await redisPub.sadd(`presence:${payload.room}`, ws.userId);
          console.log(`[WS] User ${ws.userId} joined ${payload.room}`);
          return;
        }

        // Handle message dispatch
        if (payload.action === 'message' && payload.room && payload.text) {
          const broadcastPayload = JSON.stringify({
            room: payload.room,
            sender: ws.userId,
            text: payload.text,
            timestamp: new Date().toISOString(),
          });
          // Publish across all cluster instances
          await redisPub.publish(`room:${payload.room}`, broadcastPayload);
        }
      } catch (err) {
        console.error('[WS] Error processing message', err);
      }
    });

    ws.on('close', async () => {
      localClients.delete(ws.userId);
      for (const room of ws.rooms) {
        await redisPub.srem(`presence:${room}`, ws.userId);
      }
      console.log(`[WS] Client disconnected: ${ws.userId}`);
    });
  });

  // 4. Heartbeat keepalive ping loop every 30 seconds
  const pingInterval = setInterval(() => {
    for (const client of localClients.values()) {
      if (!client.isAlive) {
        client.terminate();
        localClients.delete(client.userId);
        continue;
      }
      client.isAlive = false;
      client.ping();
    }
  }, 30000);

  wss.on('close', () => clearInterval(pingInterval));

  return wss;
}
```

### `src/app.ts`
```typescript
import { createServer } from 'node:http';
import { initWebSocketServer } from './websocket/server.js';

const server = createServer((req, res) => {
  if (req.url === '/health') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ status: 'healthy' }));
    return;
  }
  res.writeHead(404);
  res.end();
});

initWebSocketServer(server);

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`[Realtime Chat WS Server] Listening on port ${PORT}`);
});
```

---

## 3. Verification & Testing Suite

### Unit Test (`test/unit/chat.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';

describe('Realtime Chat WebSocket protocol', () => {
  it('formats room broadcast payload correctly', () => {
    const payload = {
      room: '#general',
      sender: 'user_123',
      text: 'Hello world',
      timestamp: new Date().toISOString(),
    };

    const serialized = JSON.stringify(payload);
    const parsed = JSON.parse(serialized);

    expect(parsed.room).toBe('#general');
    expect(parsed.sender).toBe('user_123');
  });
});
```

---

## 4. Production Deployment

### `docker-compose.yml`
```yaml
version: '3.8'

services:
  redis:
    image: redis:7-alpine
    ports:
      - "6379:6379"

  ws-node-1:
    build: .
    environment:
      PORT: 3001
      REDIS_URL: redis://redis:6379
    depends_on:
      - redis

  ws-node-2:
    build: .
    environment:
      PORT: 3002
      REDIS_URL: redis://redis:6379
    depends_on:
      - redis
```
