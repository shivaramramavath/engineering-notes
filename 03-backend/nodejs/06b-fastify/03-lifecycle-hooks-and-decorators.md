# Fastify Lifecycle Hooks & Decorators

> Mastering request lifecycle hooks (`onRequest`, `preHandler`, `onResponse`), custom error handlers, and instance decoration.

| Metadata | Details |
| :--- | :--- |
| **Framework Baseline** | Fastify v5.0+ |
| **Concepts** | Request Lifecycle Pipeline, Decorators |

---

## 1. The Fastify Request Lifecycle

Fastify processes incoming requests through a deterministic pipeline of hooks:

```
Incoming Request
      │
      ▼
[ onRequest ]           <--- Earliest point; inspect IP, assign correlation IDs
      │
      ▼
[ preParsing ]         <--- Modify raw payload stream before body parsing
      │
      ▼
[ preValidation ]      <--- Pre-schema normalization
      │
      ▼
[ Validation (AJV) ]   <--- Automatic schema validation
      │
      ▼
[ preHandler ]         <--- Authentication & permission guards
      │
      ▼
[ Route Handler ]      <--- Controller business logic
      │
      ▼
[ preSerialization ]   <--- Transform return value before JSON stringification
      │
      ▼
[ onSend ]             <--- Modify outgoing headers / payload
      │
      ▼
[ onResponse ]         <--- Logging, APM telemetry, latency tracking
```

---

## 2. Implementing an Authentication Hook (`preHandler`)

```javascript
fastify.addHook('preHandler', async (request, reply) => {
  // Skip auth for public routes
  if (request.routeOptions.config.isPublic) return;

  const authHeader = request.headers.authorization;
  if (!authHeader?.startsWith('Bearer ')) {
    reply.code(401);
    throw new Error('Missing or invalid Authorization header');
  }

  const token = authHeader.split(' ')[1];
  try {
    const user = await verifyToken(token);
    request.user = user; // Attach authenticated identity to request
  } catch {
    reply.code(401);
    throw new Error('Token expired or invalid');
  }
});
```

---

## 3. Decorating Instances & Requests

Use `fastify.decorate()` and `fastify.decorateRequest()` to safely extend properties:
```javascript
// Extend the Fastify server instance
fastify.decorate('cacheClient', redisClient);

// Extend the Request object
fastify.decorateRequest('user', null);
```
