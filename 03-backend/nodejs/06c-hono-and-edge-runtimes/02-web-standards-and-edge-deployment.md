# Web Standards & Edge Runtimes with Hono

> Building cross-runtime portable APIs across Node.js, Cloudflare Workers, and AWS Lambda using standard Web Fetch interfaces.

| Metadata | Details |
| :--- | :--- |
| **Framework Baseline** | Hono v4+ |
| **Runtimes** | Node.js, Cloudflare Workers, Deno, AWS Lambda |
| **Protocols** | WinterCG Web Standards |

---

## 1. The WinterCG Web Standards Paradigm

Historically, server-side JavaScript frameworks were tightly coupled to Node.js proprietary APIs (`http.IncomingMessage`, `http.ServerResponse`, `req.connection`).
The WinterCG (Web-interoperable Runtimes Community Group) standardized on:
- Global `Request` and `Response`
- Global `fetch()`
- Global `Headers` and `URL`
- Web Streams (`ReadableStream`, `WritableStream`)

By programming against Hono's `Context`, your backend routes have **zero Node.js proprietary dependencies**:

```typescript
// Pure portable route handler
export const getProductHandler = async (c: Context) => {
  const authHeader = c.req.header('Authorization');
  const query = c.req.query('q');

  return c.json({ results: [], query });
};
```

---

## 2. Deploying Across Different Runtimes

### Node.js Production Target
```typescript
import { serve } from '@hono/node-server';
import { app } from './app';

serve({ fetch: app.fetch, port: 3000 });
```

### Cloudflare Workers Target
```typescript
import { app } from './app';

// Cloudflare Workers natively exports the fetch handler!
export default app;
```

### AWS Lambda Target
```typescript
import { handle } from 'hono/aws-lambda';
import { app } from './app';

export const handler = handle(app);
```
One single application codebase deploys seamlessly to containers, serverless Lambdas, and global edge CDNs without code refactoring.
