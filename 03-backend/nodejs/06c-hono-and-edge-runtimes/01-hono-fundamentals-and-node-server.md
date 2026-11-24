# Hono Fundamentals & Running on Node.js via `@hono/node-server`

> Building modern Web Standards-based HTTP APIs with Hono and executing on Node.js using `@hono/node-server`.

| Metadata | Details |
| :--- | :--- |
| **Framework Baseline** | Hono v4+ |
| **Adapter** | `@hono/node-server` |
| **Standards** | Web Standards `Request`, `Response`, `fetch()` |

---

## 1. What is Hono?

Hono is an ultra-fast, lightweight web framework built on standard Web APIs (`fetch`, `Request`, `Response`, `Headers`).
- **Zero-Dependency Core**: Extremely small bundle footprint (< 20kB).
- **Run Anywhere**: Runs seamlessly on Cloudflare Workers, Deno, Bun, and Node.js without rewriting route handlers.
- **Type-Safe RPC**: Provides end-to-end type safety between client and server via `hc` (Hono Client).

---

## 2. Bootstrapping Hono on Node.js

Install Hono and the official Node.js server adapter:
```bash
npm install hono @hono/node-server
```

Create `src/server.ts`:
```typescript
import { Hono } from 'hono';
import { serve } from '@hono/node-server';
import { logger } from 'hono/logger';
import { cors } from 'hono/cors';

const app = new Hono();

// Built-in middleware
app.use('*', logger());
app.use('/api/*', cors());

// Routing with Web Standards Context `c`
app.get('/api/health', (c) => {
  return c.json({ status: 'healthy', timestamp: new Date().toISOString() });
});

app.get('/api/users/:id', (c) => {
  const id = c.req.param('id');
  return c.json({ userId: id, name: 'Elena Rostova' });
});

// Start the Node.js HTTP server
serve({
  fetch: app.fetch,
  port: 3000,
}, (info) => {
  console.log(`Hono server listening on http://localhost:${info.port}`);
});
```

---

## 3. Streaming Responses with Hono

Because Hono operates natively on Web Streams, streaming SSE or LLM tokens requires minimal code:

```typescript
import { streamSSE } from 'hono/streaming';

app.get('/api/stream', (c) => {
  return streamSSE(c, async (stream) => {
    let id = 0;
    while (id < 5) {
      id++;
      await stream.writeSSE({
        data: JSON.stringify({ message: `Token stream ${id}` }),
        event: 'chunk',
        id: String(id),
      });
      await stream.sleep(1000);
    }
  });
});
```
