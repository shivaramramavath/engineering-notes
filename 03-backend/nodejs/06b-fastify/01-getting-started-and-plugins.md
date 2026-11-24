# Fastify v5: Getting Started & The Plugin Architecture

> Introduction to Fastify v5, asynchronous bootstrapping, encapsulation contexts, and the plugin ecosystem via `fastify-plugin`.

| Metadata | Details |
| :--- | :--- |
| **Framework Baseline** | Fastify v5.0+ |
| **Node.js Baseline** | Node.js 20+ LTS |
| **Core Concept** | Encapsulated Plugin Tree |

---

## 1. Why Fastify? Architectural Philosophy

Fastify is built for high throughput and low latency:
- **Low Overhead**: Handles up to $2\times$ the requests per second of Express under identical workloads.
- **JSON Schema Compilation**: Uses `fast-json-stringify` and `ajv` to precompile schemas into optimized C++-like serializer functions.
- **Encapsulation Model**: Plugins and decorators create scoped child contexts rather than polluting a global application instance.

---

## 2. Fastify v5 Bootstrapping & Listen Signature

In Fastify v5, the variadic `.listen(port, host, cb)` signature has been removed. You must pass an options object:

```javascript
import Fastify from 'fastify';

const fastify = Fastify({
  logger: {
    level: 'info',
    transport: {
      target: 'pino-pretty',
    },
  },
});

fastify.get('/ping', async (request, reply) => {
  return { status: 'ok', timestamp: Date.now() };
});

try {
  // Fastify v5 required options object signature:
  await fastify.listen({ port: 3000, host: '0.0.0.0' });
} catch (err) {
  fastify.log.error(err);
  process.exit(1);
}
```

---

## 3. Plugin Encapsulation & `fastify-plugin` (`fp`)

In Fastify, `fastify.register()` creates a **new isolated child context**. Decorators or hooks registered inside a plugin do NOT leak to parent or sibling scopes:

```javascript
import fp from 'fastify-plugin';

// 1. Encapsulated Sub-Route Plugin
async function userRoutes(fastify, options) {
  fastify.get('/users', async (req, reply) => {
    return [{ id: 1, name: 'Alice' }];
  });
}

// 2. Global Plugin (Breaking encapsulation via fastify-plugin)
const dbPlugin = fp(async (fastify, options) => {
  const db = { query: async () => 'DB Connected' };
  // Decorate the fastify instance globally
  fastify.decorate('db', db);
});

// App registration
await fastify.register(dbPlugin);
await fastify.register(userRoutes, { prefix: '/api/v1' });
```
