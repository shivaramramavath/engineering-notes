# Fastify Schemas: Validation with AJV & Serialization

> Compiling JSON schemas for request validation, prototype pollution protection in `request.params`, and response serialization with `fast-json-stringify`.

| Metadata | Details |
| :--- | :--- |
| **Framework Baseline** | Fastify v5.0+ |
| **Engines** | AJV v8 (Validation), `fast-json-stringify` (Serialization) |
| **Level** | High-Performance Frameworks |

---

## 1. Fastify v5 Strict Schema Requirement

In Fastify v5, shorthand schema notations are removed. Full JSON Schema structures are required for `body`, `querystring`, `params`, and `headers`:

```javascript
const createUserSchema = {
  body: {
    type: 'object',
    required: ['email', 'name'],
    properties: {
      email: { type: 'string', format: 'email' },
      name: { type: 'string', minLength: 2 },
      age: { type: 'integer', minimum: 18 },
    },
    additionalProperties: false, // Prevents unintended fields
  },
  response: {
    201: {
      type: 'object',
      properties: {
        id: { type: 'string' },
        email: { type: 'string' },
        createdAt: { type: 'string' },
      },
    },
  },
};

fastify.post('/users', { schema: createUserSchema }, async (request, reply) => {
  // request.body is strictly validated against the schema by AJV!
  const newUser = await db.users.create(request.body);
  reply.code(201);
  return newUser; // Serialized with fast-json-stringify!
});
```

---

## 2. Why Response Serialization Improves Performance

Standard Express responses execute `JSON.stringify(object)`.
In JavaScript, `JSON.stringify()` recursively inspects the entire object and its prototype chain dynamically.

**Fastify's `response` schema compiles a static serialization function ahead of time**:
- Because the exact keys and types are known, Fastify generates custom JavaScript strings using string concatenation, skipping type inspection.
- **Security Bonus**: Any fields on the return object NOT declared in the `response` schema (such as `passwordHash` or internal flags) are automatically stripped from the outgoing JSON payload.

---

## 3. Prototype Pollution Protection in Fastify v5

In Fastify v5, `request.params` is created with `Object.create(null)` to eliminate prototype pollution attack vectors.
```javascript
// ❌ FAILS in Fastify v5:
request.params.hasOwnProperty('id');

// ✅ CORRECT:
Object.hasOwn(request.params, 'id');
```
