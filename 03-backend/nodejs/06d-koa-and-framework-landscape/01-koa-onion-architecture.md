# Koa.js & The Onion Middleware Architecture

> Understanding Koa's downstream-upstream middleware cascade (the "onion model") and context-based request lifecycle.

| Metadata | Details |
| :--- | :--- |
| **Framework Baseline** | Koa v2.15+ |
| **Core Architecture** | Cascading Middleware ("Onion Model") |
| **Level** | Framework Internals |

---

## 1. The Onion Model Explained

In Express, middleware executes sequentially in one direction. To measure response time, an Express middleware must hook into the response `'finish'` event.

In Koa, middleware cascades **downstream** until reaching the route handler, and then flows back **upstream**:

```
Request In
   │
   ▼
┌────────────────── Middleware 1 (e.g. Logger) ──────────────────┐
│  console.log('--> Request started');                            │
│  ┌─────────────── Middleware 2 (e.g. Auth) ──────────────────┐  │
│  │  await verifySession(ctx);                                │  │
│  │  ┌──────────── Route Handler (Business Logic) ──────────┐ │  │
│  │  │  ctx.body = { data: 'Success' };                     │ │  │
│  │  └──────────────────────────────────────────────────────┘ │  │
│  │  console.log('<-- Auth finished');                        │  │
│  └───────────────────────────────────────────────────────────┘  │
│  const ms = Date.now() - start;                                 │
│  console.log(`<-- Request finished in ${ms}ms`);               │
└─────────────────────────────────────────────────────────────────┘
   │
   ▼
Response Out
```

---

## 2. Implementing the Onion Cascade in Koa

```javascript
import Koa from 'koa';

const app = new Koa();

// 1. Outermost layer: Response time calculation
app.use(async (ctx, next) => {
  const start = Date.now();
  await next(); // Flow downstream!
  // Flow back upstream after all downstream handlers finish:
  const ms = Date.now() - start;
  ctx.set('X-Response-Time', `${ms}ms`);
});

// 2. Middle layer: Error handling
app.use(async (ctx, next) => {
  try {
    await next();
  } catch (err) {
    ctx.status = err.status || 500;
    ctx.body = { error: err.message };
    ctx.app.emit('error', err, ctx);
  }
});

// 3. Innermost layer: Business Logic
app.use(async (ctx) => {
  ctx.body = { message: 'Hello from Koa Onion Architecture!' };
});

app.listen(3000);
```
