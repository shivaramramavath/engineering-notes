# Node.js Technical Interview: Express, Fastify & HTTP Architecture Q&A

> Technical questions and architectural answers on web framework internals, middleware execution mechanics, Express 5.0 migration, Fastify optimization, and HTTP protocol performance.

---

### Q1: Explain the Express middleware pipeline execution model. What happens when `next()`, `next('route')`, and `next(error)` are invoked?

#### In-Depth Answer
In Express, request handling is structured as a linear array of middleware functions evaluated sequentially:
- **`next()`**: Passes control to the immediately following middleware in the current stack.
- **`next('route')`**: Bypasses all remaining middleware functions in the **current route sub-stack** and jumps immediately to the **next matching route** definition. Only valid inside `app.METHOD()` or `router.METHOD()`.
- **`next(err)`**: When `next()` is called with *any* argument (except the string `'route'`), Express treats the argument as an error. It skips all remaining standard middleware in the pipeline and jumps directly to the nearest **Error-Handling Middleware** (defined with 4 parameters: `(err, req, res, next)`).

#### The 4-Argument Error Middleware Trap:
Express inspects `fn.length` using JavaScript reflection at route registration time.
If your error middleware has only 3 arguments `(err, req, res)`, Express treats it as a standard middleware with an argument named `err` (mistaking it for `req`) and will **never execute it during error propagation**! Error middleware **must always have 4 arguments**: `(err, req, res, next)`.

---

### Q2: What are the key architectural improvements and breaking changes introduced in Express 5.0?

#### In-Depth Answer
Released as General Availability in October 2024, Express 5.0 modernized Express for Node.js 22 LTS:

1. **Automatic Async Error Forwarding (No more `express-async-errors`)**:
   - In Express 4, if an `async` route handler rejected a Promise (`throw new Error()` or DB rejection), the error was unhandled and caused the process to crash unless manually wrapped in `try/catch` and passed to `next(err)`.
   - In Express 5, all rejected Promises returned by route handlers are **automatically caught and forwarded to the error-handling middleware**:
   ```javascript
   // Express 5 native async safety:
   app.get('/users/:id', async (req, res) => {
     const user = await db.getUser(req.params.id); // If this rejects, Express 5 catches it cleanly!
     res.json(user);
   });
   ```
2. **`path-to-regexp` v8 Upgrade**:
   - Raw RegExp in route patterns is removed.
   - Wildcards now require explicit names: `app.get('/files/*name', ...)` instead of `app.get('/files/*', ...)`.
   - Optional parameters require curly braces: `/users{/:id}` instead of `/users/:id?`.
3. **Removal of Deprecated Methods**:
   - `app.del()` is removed (use `app.delete()`).
   - `req.param(name)` is removed (access `req.params`, `req.query`, or `req.body` explicitly).
   - `res.send(status)` with a number is removed (use `res.sendStatus(status)` or `res.status(code).send()`).
4. **`res.redirect()` Status Enforcement**:
   - Stricter compliance: throws error if an invalid HTTP status code is supplied.

---

### Q3: Why is Fastify substantially faster than Express? Detail its internal architecture.

#### In-Depth Answer
Fastify delivers 2x to 5x higher request throughput than Express due to three primary architectural innovations:

1. **`find-my-way` Radix Tree Router**:
   - Express uses a linear array of regex matchers. For $N$ registered routes, Express performs $O(N)$ regex evaluations per request.
   - Fastify uses a Radix Tree (Prefix Tree). Route lookup time is $O(K)$ where $K$ is the length of the URL path, completely independent of how many thousands of routes are registered.
2. **`fast-json-stringify` Serialization**:
   - When calling `res.json(data)` in Express, V8 invokes native `JSON.stringify()`, which must inspect object shapes dynamically at runtime.
   - Fastify compiles JSON Schema definitions into specialized C++ / V8 JIT-optimized serialization functions at startup. Pre-compiled serializers bypass dynamic object reflection and serialize payloads up to 2x faster.
3. **Zero-Allocation Context (`Object.create(null)`)**:
   - Fastify minimizes memory churn and GC pressure by recycling internal request/reply objects and using null-prototype dictionaries for parameters to prevent prototype pollution checks.

---

### Q4: How does HTTP Keep-Alive work in Node.js? What is socket exhaustion, and how is it prevented?

#### In-Depth Answer
In HTTP/1.1, opening a new TCP connection requires a 3-way handshake (`SYN`, `SYN-ACK`, `ACK`), plus an additional TLS 1.3 handshake (1-2 round trips).
**HTTP Keep-Alive** allows a single persistent TCP socket to be reused for hundreds of sequential HTTP requests between the same client and server, saving CPU cycles and latency.

#### Socket Exhaustion Vulnerability:
When a Node.js microservice makes frequent outbound HTTP calls using standard `fetch()` or `http.request()` without a configured agent:
- Connections close after each request.
- The OS puts closed sockets into the `TIME_WAIT` state for 60 to 120 seconds to guarantee duplicate packets in transit are safely discarded.
- Under high load, the server exhausts all available ephemeral port numbers (typically ~65,000), throwing `EADDRNOTAVAIL` or `ECONNRESET`.

#### Production Mitigation:
Configure an explicit `http.Agent` with `keepAlive: true` and a bounded `maxSockets`:

```javascript
import http from 'node:http';
import https from 'node:https';

export const httpAgent = new http.Agent({
  keepAlive: true,
  maxSockets: 100,           // Max concurrent sockets per host
  maxFreeSockets: 10,        // Max idle sockets kept open
  timeout: 30000,            // Active socket timeout
});
```
