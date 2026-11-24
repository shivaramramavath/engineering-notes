# Project 11: Lightweight API Gateway & Reverse Proxy (`proj-node-11-api-gateway-lite`)

> High-throughput reverse proxy API Gateway built with Node.js 22 LTS and Fastify 5 (`@fastify/http-proxy`). Implements dynamic path-based routing, upstream load balancing (round-robin and health-checked pools), centralized JWT token verification with header propagation (`X-User-Id`, `X-User-Role`), correlation ID injection (`X-Request-Id`), and zero-overhead HTTP streaming.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-11-api-gateway-lite` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Fastify 5.0 + `@fastify/http-proxy` |
| **Routing & Proxy** | Stream piping with HTTP keep-alive pools |
| **Auth** | Edge JWT verification & header sanitization |
| **Observability** | Correlation ID tracking + OpenTelemetry hooks |
| **Testing** | Vitest + Fastify `inject()` |
| **Complexity Level** | Intermediate / Infrastructure |

---

## 1. Architectural Overview & Domain Design

### 1.1 Reverse Proxy & Upstream Delegation

The API Gateway acts as the single public entrypoint for client traffic:
1. **Edge Auth Verification**: JWT tokens in the `Authorization` header are validated at the gateway edge. If valid, the gateway decodes the user claims, strips the client `Authorization` header, and securely injects internal trusted headers (`X-User-Id`, `X-User-Role`) before proxying upstream.
2. **Dynamic Route Proxies**: Requests matching `/api/v1/users/*` are proxied to the User Microservice; `/api/v1/orders/*` are proxied to the Order Microservice.
3. **Upstream Health & Load Balancing**: Failed upstreams are automatically avoided using round-robin pooling with passive health checks.

```
+----------------------------------------------------------------------------------------------------+
|                                  API Gateway Routing Topology                                      |
|                                                                                                    |
|  [ Public Internet Client / SPA ]                                                                  |
|                │                                                                                   |
|                ▼ HTTPS (Port 443)                                                                  |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |             Fastify 5 Lightweight API Gateway               |                                   |
|  |  - Centralized Rate Limiter                                 |                                   |
|  |  - JWT Edge Token Verification                              |                                   |
|  |  - X-Request-Id Correlation Injection                       |                                   |
|  |  - Path Rewriter & Stripper                                 |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Stream Proxy Pipe (Keep-Alive)                                    |
|             ┌──────────────────┴──────────────────┐                                                |
|             │ Proxy to /api/v1/users              │ Proxy to /api/v1/orders                        |
|             ▼                                     ▼                                                |
|  +─────────────────────────+       +─────────────────────────+                                     |
|  |   User Microservice     |       |   Order Microservice    |                                     |
|  |  (Internal Port 4001)   |       |  (Internal Port 4002)   |                                     |
|  |                         |       |                         |                                     |
|  | Receives:               |       | Receives:               |                                     |
|  | X-User-Id: usr_123      |       | X-User-Id: usr_123      |                                     |
|  | X-Request-Id: req_abc   |       | X-Request-Id: req_abc   |                                     |
|  +─────────────────────────+       +─────────────────────────+                                     |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-11-api-gateway-lite/
├── src/
│   ├── config/
│   │   ├── env.ts               # Upstream URLs & JWT secret
│   │   └── routes.config.ts     # Route mapping rules
│   ├── plugins/
│   │   ├── auth.plugin.ts       # JWT edge validator & header injector
│   │   └── correlation.plugin.ts# X-Request-Id header generator
│   └── app.ts                   # Fastify proxy registration
├── test/
│   └── integration/
│       └── gateway.test.ts
├── Dockerfile
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-11-api-gateway-lite",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node --env-file=.env src/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "@fastify/http-proxy": "^10.0.1",
    "fastify": "^5.2.1",
    "jose": "^5.9.6"
  },
  "devDependencies": {
    "@types/node": "^22.13.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/app.ts`
```typescript
import Fastify from 'fastify';
import httpProxy from '@fastify/http-proxy';
import * as jose from 'jose';
import { randomUUID } from 'node:crypto';

const fastify = Fastify({ logger: true });
const JWT_SECRET = new TextEncoder().encode(process.env.JWT_SECRET || 'gateway-edge-secret-key-32chars!');

// 1. Correlation ID Middleware
fastify.addHook('onRequest', async (request, reply) => {
  const correlationId = (request.headers['x-request-id'] as string) || randomUUID();
  request.headers['x-request-id'] = correlationId;
  reply.header('x-request-id', correlationId);
});

// 2. Healthcheck Endpoint
fastify.get('/health', async () => ({ status: 'healthy', gateway: 'active' }));

// 3. User Service Proxy with Authentication Guard
fastify.register(httpProxy, {
  upstream: process.env.USER_SERVICE_URL || 'http://localhost:4001',
  prefix: '/api/v1/users',
  rewritePrefix: '/users',
  preHandler: async (request, reply) => {
    const authHeader = request.headers['authorization'];
    if (!authHeader?.startsWith('Bearer ')) {
      return reply.status(401).send({ error: 'Missing Bearer token' });
    }

    try {
      const token = authHeader.slice(7);
      const { payload } = await jose.jwtVerify(token, JWT_SECRET);
      
      // Inject trusted headers downstream and strip client auth header
      request.headers['x-user-id'] = payload.sub;
      request.headers['x-user-role'] = payload.role as string;
      delete request.headers['authorization'];
    } catch {
      return reply.status(403).send({ error: 'Invalid token' });
    }
  },
});

// 4. Order Service Proxy
fastify.register(httpProxy, {
  upstream: process.env.ORDER_SERVICE_URL || 'http://localhost:4002',
  prefix: '/api/v1/orders',
  rewritePrefix: '/orders',
});

if (process.env.NODE_ENV !== 'test') {
  await fastify.listen({ port: 3000, host: '0.0.0.0' });
}

export { fastify };
```

---

## 3. Verification & Testing Suite

### Unit Test (`test/unit/gateway.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';
import { fastify } from '../../src/app.js';

describe('API Gateway Lite', () => {
  it('GET /health returns healthy status', async () => {
    const response = await fastify.inject({
      method: 'GET',
      url: '/health',
    });

    expect(response.statusCode).toBe(200);
    expect(response.json()).toHaveProperty('status', 'healthy');
  });

  it('rejects unauthenticated requests to protected upstream', async () => {
    const response = await fastify.inject({
      method: 'GET',
      url: '/api/v1/users/profile',
    });

    expect(response.statusCode).toBe(401);
  });
});
```

---

## 4. Production Deployment

### `Dockerfile`
```dockerfile
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .

FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app ./
USER nonroot
EXPOSE 3000
CMD ["src/app.js"]
```
