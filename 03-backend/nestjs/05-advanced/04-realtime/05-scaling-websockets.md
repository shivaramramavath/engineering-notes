# Scaling WebSockets and the Redis Streams Adapter

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 04-realtime
- **Prerequisites**: [01-gateways-and-events](./01-gateways-and-events.md), [02-socketio](./02-socketio.md), [05-advanced/01-caching/03-redis](../01-caching/03-redis.md)

---

## 1. Context & Purpose

A single Node.js process can comfortably maintain 20,000–50,000 concurrent idle WebSocket connections. However, enterprise systems with hundreds of thousands of concurrent users require horizontal scaling across multiple pods behind a load balancer.

In a multi-pod cluster, a fundamental challenge emerges: **The Partitioning Dilemma**. If User A is connected to Pod 1 and User B is connected to Pod 2, calling `server.to('chat').emit('message')` on Pod 1 reaches only the sockets physically connected to Pod 1! User B never receives the message.

To scale WebSockets horizontally, all pods must share a distributed Pub/Sub backplane. In Socket.IO and NestJS, this is solved via the **`@socket.io/redis-adapter`** or modern **`@socket.io/redis-streams-adapter`**.

---

## 2. Theoretical Foundations

### 2.1 The Distributed Real-Time Mesh
When Pod 1 emits an event to a room:
1. Pod 1 broadcasts the frame locally to its directly connected sockets.
2. Pod 1 publishes the event payload, target room, and namespace to a Redis Pub/Sub channel or Redis Stream.
3. Pod 2, Pod 3, and Pod 4 subscribe to this Redis channel.
4. Upon receiving the payload from Redis, sibling pods broadcast the frame to their locally connected sockets.

### 2.2 Pub/Sub vs Redis Streams Adapter
- **Classic Redis Pub/Sub (`@socket.io/redis-adapter`)**: Fire-and-forget. If Pod 2 experiences a momentary 100ms network blip, published packets are dropped and never recovered.
- **Redis Streams Adapter (`@socket.io/redis-streams-adapter`)**: Uses Redis Streams (`XADD`, `XREAD`). Provides message persistence and automatic packet catch-up when pods momentarily disconnect.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   Kubernetes Cluster                   │
│                                                        │
│  User A (Pod 1)                      User B (Pod 2)    │
│         │                                   ▲          │
│         ▼                                   │          │
│   ┌───────────┐                       ┌─────┴─────┐    │
│   │   Pod 1   │                       │   Pod 2   │    │
│   └─────┬─────┘                       └─────▲─────┘    │
└─────────┼───────────────────────────────────┼──────────┘
          │                                   │
          │ Publish: "room-42: Hello"         │ Consume & Broadcast
          │                                   │
          ▼                                   │
┌─────────────────────────────────────────────┴──────────┐
│                   Redis Pub/Sub / Stream               │
│                  Channel: `socket.io#chat#`            │
└────────────────────────────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @socket.io/redis-adapter ioredis
```

---

### 4.2 Building the Redis-Powered `RedisIoAdapter`

```typescript
// src/common/adapters/redis-io.adapter.ts
import { IoAdapter } from '@nestjs/platform-socket.io';
import { ServerOptions } from 'socket.io';
import { createAdapter } from '@socket.io/redis-adapter';
import Redis from 'ioredis';
import { INestApplication, Logger } from '@nestjs/common';

export class RedisIoAdapter extends IoAdapter {
  private adapterConstructor!: ReturnType<typeof createAdapter>;
  private readonly logger = new Logger(RedisIoAdapter.name);

  constructor(app: INestApplication) {
    super(app);
  }

  async connectToRedis(redisUrl: string): Promise<void> {
    // Redis adapter requires two dedicated connections: pubClient and subClient
    const pubClient = new Redis(redisUrl);
    const subClient = pubClient.duplicate();

    pubClient.on('error', (err) => this.logger.error('Redis PubClient Error', err.message));
    subClient.on('error', (err) => this.logger.error('Redis SubClient Error', err.message));

    await Promise.all([
      new Promise((resolve) => pubClient.once('ready', resolve)),
      new Promise((resolve) => subClient.once('ready', resolve)),
    ]);

    this.adapterConstructor = createAdapter(pubClient, subClient);
    this.logger.log('Redis WebSocket Adapter connected and initialized successfully.');
  }

  createIOServer(port: number, options?: ServerOptions): any {
    const server = super.createIOServer(port, {
      ...options,
      cors: { origin: '*', methods: ['GET', 'POST'] },
    });

    server.adapter(this.adapterConstructor);
    return server;
  }
}
```

---

### 4.3 Initializing the Adapter in `main.ts`

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';
import { RedisIoAdapter } from './common/adapters/redis-io.adapter';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // Initialize and connect Redis adapter
  const redisIoAdapter = new RedisIoAdapter(app);
  await redisIoAdapter.connectToRedis(process.env.REDIS_URL || 'redis://localhost:6379');

  app.useWebSocketAdapter(redisIoAdapter);

  await app.listen(3000);
}
bootstrap();
```

---

### 4.4 Load Balancer Configuration (Sticky Sessions)

When running behind Nginx, configure IP hashing or cookie affinity:

```nginx
# /etc/nginx/conf.d/websocket.conf
upstream nestjs_nodes {
    ip_hash; # Sticky sessions based on client IP
    server 10.0.0.1:3000;
    server 10.0.0.2:3000;
    server 10.0.0.3:3000;
}

server {
    listen 80;
    server_name realtime.example.com;

    location /socket.io/ {
        proxy_pass http://nestjs_nodes;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        # Disable proxy buffering for real-time traffic
        proxy_buffering off;
        proxy_read_timeout 86400s;
        proxy_send_timeout 86400s;
    }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Redis Node Outage
If the shared Redis broker restarts, the Pub/Sub connections break.
- **Handling**: `ioredis` automatically reconnects with exponential backoff. The Redis adapter resubscribes to active rooms automatically once the connection recovers.

### 5.2 Message Amplification Overhead
If a room contains 100,000 users distributed across 10 pods, broadcasting one message to that room causes Redis to replicate that message 10 times (once to each pod).
- **Rule**: Avoid mega-broadcasts over WebSockets for public broadcast data; use HTTP SSE behind a CDN or Kafka event streaming.

---

## 6. Performance & Memory Implications

- **Cluster Throughput**: With `@socket.io/redis-adapter`, a 10-pod cluster can support 250,000+ simultaneous connections, broadcasting events across nodes in $< 2\text{ms}$.
- **Socket Memory**: In-memory room tracking on each pod is scoped only to its own connected sockets, avoiding memory bloat from inactive remote sockets.

---

## 7. Anti-Patterns & Common Mistakes

- **Reusing the SubClient for Standard Redis Commands**: Attempting to call `subClient.get('key')`. Once a Redis connection enters subscriber mode via `SUBSCRIBE`, the server rejects all normal read/write commands.
- **Deploying Multi-Pod Socket.io Without Redis Adapter**: Deploying to Kubernetes with multiple replicas and wondering why users in the same room cannot see each other's messages.
- **Ignoring Ephemeral Port Exhaustion**: Scaling to 50,000 connections on a single Linux instance without tuning `sysctl` TCP limits (`net.ipv4.ip_local_port_range`, `net.core.somaxconn`, `fs.file-max`).

---

## 8. Production Checklist & Operational Readiness

- [ ] `@socket.io/redis-adapter` configured with separate `pubClient` and `subClient`.
- [ ] Load balancer configured with WebSocket upgrade headers and sticky sessions.
- [ ] OS file descriptor limit increased (`ulimit -n 65535`) in Docker containers.
- [ ] Redis cluster monitored for Pub/Sub memory buffer overruns.

---

## 9. Migration & Version History

- **Socket.io 4.x**: Introduced `@socket.io/redis-streams-adapter` for guaranteed packet catchup.

---

## 10. Verification & Testing

Unit testing adapter initialization:

```typescript
import { RedisIoAdapter } from './redis-io.adapter';

describe('RedisIoAdapter', () => {
  it('instantiates adapter cleanly', () => {
    const mockApp: any = { getHttpServer: () => ({}) };
    const adapter = new RedisIoAdapter(mockApp);
    expect(adapter).toBeDefined();
  });
});
```

---

## 11. Comprehensive Examples

### Linux OS Tuning for 100,000+ Real-Time Connections

```ini
# /etc/sysctl.d/99-websockets.conf
# Increase system file descriptor limit
fs.file-max = 2097152

# Maximize socket listen backlog
net.core.somaxconn = 65535

# Expand local port range for outbound connections
net.ipv4.ip_local_port_range = 1024 65535

# Enable TCP keepalive tuning
net.ipv4.tcp_keepalive_time = 300
net.ipv4.tcp_keepalive_intvl = 15
net.ipv4.tcp_keepalive_probes = 5
```

---

## 12. Cross-References & Further Reading

- [02-socketio.md](./02-socketio.md) - Socket.io features
- [04-rooms-and-namespaces.md](./04-rooms-and-namespaces.md) - Multi-tenancy and rooms
- [05-advanced/01-caching/03-redis](../01-caching/03-redis.md) - Redis infrastructure
- [Socket.IO Redis Adapter Documentation](https://socket.io/docs/v4/redis-adapter/)
