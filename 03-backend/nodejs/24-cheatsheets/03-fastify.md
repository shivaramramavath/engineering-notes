# Cheatsheet 03: Fastify 5.0 Reference Card

> Production syntax reference for Fastify 5.0, JSON Schema validation, lifecycle hooks, plugins, and decorator scoping.

---

### 1. Minimal Fastify 5 Server
```javascript
import Fastify from 'fastify';

const fastify = Fastify({
  logger: { level: 'info' },
});

fastify.get('/health', async () => ({ status: 'healthy' }));

// In Fastify 5, listen requires an options object (variadic arguments removed)
await fastify.listen({ port: 3000, host: '0.0.0.0' });
```

---

### 2. JSON Schema Validation & Fast Serialization
Fastify compiles schemas via Ajv for validation and `fast-json-stringify` for response serialization:
```javascript
const userSchema = {
  params: {
    type: 'object',
    required: ['id'],
    properties: {
      id: { type: 'string', pattern: '^[0-9]+$' },
    },
  },
  body: {
    type: 'object',
    required: ['email', 'name'],
    properties: {
      email: { type: 'string', format: 'email' },
      name: { type: 'string', minLength: 2 },
    },
  },
  response: {
    201: {
      type: 'object',
      properties: {
        id: { type: 'string' },
        email: { type: 'string' },
      },
    },
  },
};

fastify.post('/users/:id', { schema: userSchema }, async (request, reply) => {
  const { id } = request.params;
  const { email, name } = request.body;
  return reply.status(201).send({ id, email });
});
```

---

### 3. Lifecycle Hooks Execution Order
```
onRequest ──> preParsing ──> preValidation ──> preHandler ──> [ Route Handler ] ──> preSerialization ──> onSend ──> onResponse
```

```javascript
// Global Authentication Hook:
fastify.addHook('preHandler', async (request, reply) => {
  if (request.url === '/public') return;
  const token = request.headers.authorization;
  if (!token) {
    return reply.status(401).send({ error: 'Unauthorized' });
  }
});
```

---

### 4. Plugins & Encapsulation (`fastify-plugin`)
By default, plugins create a new enclosed context. Use `fastify-plugin` (`fp`) to register singletons (like DB pools) accessible everywhere:
```javascript
import fp from 'fastify-plugin';
import pg from 'pg';

export const dbPlugin = fp(async (fastify, opts) => {
  const pool = new pg.Pool({ connectionString: opts.url });
  fastify.decorate('db', pool); // Accessible as fastify.db

  fastify.addHook('onClose', async (instance) => {
    await instance.db.end();
  });
});

// In server.js:
await fastify.register(dbPlugin, { url: process.env.DATABASE_URL });
```

---

### 5. Testing with `fastify.inject()`
Fastify enables unit testing without binding a real network port:
```javascript
import { test, expect } from 'vitest';
import { app } from './app.js';

test('GET /health returns 200', async () => {
  const res = await app.inject({ method: 'GET', url: '/health' });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ status: 'healthy' });
});
```
