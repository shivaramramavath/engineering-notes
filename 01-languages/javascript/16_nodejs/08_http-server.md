# HTTP Server

The `node:http` module is enough to build a complete web server. Frameworks (Express, Fastify, Hono) wrap these same primitives. Understanding the raw API explains what they do. For protocol basics see [HTTP Fundamentals](../15_networking/01_http-fundamentals.md).

## Hello server

```js
import http from 'node:http';

const server = http.createServer((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
  res.end('Hello, world\n');
});

server.listen(3000, () => {
  console.log('http://localhost:3000');
});
```

The handler runs **once per request**. `req` is an `IncomingMessage` (a Readable stream); `res` is a `ServerResponse` (a Writable stream).

## The request

```js
http.createServer((req, res) => {
  req.method;                          // 'GET', 'POST', ...
  req.url;                             // '/users?id=5' (path + query, no host)
  req.headers;                         // lowercase header names
  req.headers['content-type'];
  req.socket.remoteAddress;            // client IP (the proxy's IP behind a load balancer)
  req.httpVersion;                     // '1.1'

  const url = new URL(req.url, `http://${req.headers.host}`);
  url.pathname;                        // '/users'
  url.searchParams.get('id');          // '5'
});
```

## The response

```js
res.statusCode = 201;
res.statusMessage = 'Created';                  // optional
res.setHeader('Content-Type', 'application/json');
res.setHeader('Set-Cookie', ['a=1; HttpOnly', 'b=2']);   // arrays for repeated headers
res.getHeader('content-type');
res.removeHeader('X-Powered-By');

res.write('part 1');            // send a chunk (chunked transfer encoding)
res.end('last part');           // finish; must be called exactly once

// Or in one call:
res.writeHead(200, { 'Content-Type': 'text/html' });
res.end('<h1>Hi</h1>');
```

Headers must be set **before** the first `write()` / `end()`. Afterwards, `ERR_HTTP_HEADERS_SENT` is thrown.

Send JSON:

```js
function sendJson(res, status, data) {
  const body = JSON.stringify(data);
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(body),
  });
  res.end(body);
}
```

## Common status codes

| Code | Meaning | Use |
|------|---------|-----|
| `200` | OK | Successful GET |
| `201` | Created | Successful POST that created something (add `Location`) |
| `204` | No Content | Success with empty body (DELETE) |
| `301` / `302` / `307` / `308` | Redirects | Set `Location` header |
| `304` | Not Modified | Conditional GET with cache validators |
| `400` | Bad Request | Malformed input |
| `401` / `403` | Unauthenticated / Forbidden | Auth failures |
| `404` | Not Found | Unknown route or resource |
| `405` | Method Not Allowed | Add an `Allow` header |
| `413` | Payload Too Large | Body exceeded your limit |
| `429` | Too Many Requests | Rate limiting |
| `500` | Internal Server Error | Unexpected failure |
| `503` | Service Unavailable | Overloaded or shutting down |

## Reading the body

The body arrives as a stream. Collect it, enforce a size limit, then parse:

```js
async function readBody(req, limit = 1_000_000) {
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    size += chunk.length;
    if (size > limit) {
      const err = new Error('Payload too large');
      err.status = 413;
      throw err;
    }
    chunks.push(chunk);
  }
  return Buffer.concat(chunks).toString('utf8');
}

async function readJson(req) {
  const text = await readBody(req);
  try {
    return JSON.parse(text);
  } catch {
    const err = new Error('Invalid JSON');
    err.status = 400;
    throw err;
  }
}
```

Without a size limit, a client can exhaust your memory.

## Routing

A minimal router: match method + path, dispatch to handlers.

```js
const routes = [];
const route = (method, pattern, handler) => routes.push({ method, pattern, handler });

route('GET',  /^\/users$/,           async () => ({ status: 200, body: users }));
route('GET',  /^\/users\/(\d+)$/,    async (req, [id]) => {
  const user = users.find((u) => u.id === Number(id));
  return user ? { status: 200, body: user } : { status: 404, body: { error: 'Not found' } };
});
route('POST', /^\/users$/,           async (req) => {
  const data = await readJson(req);
  const user = { id: users.length + 1, ...data };
  users.push(user);
  return { status: 201, body: user };
});

const users = [];

const server = http.createServer(async (req, res) => {
  const { pathname } = new URL(req.url, 'http://localhost');

  try {
    const pathMatches = routes.filter((r) => r.pattern.test(pathname));
    if (pathMatches.length === 0) return sendJson(res, 404, { error: 'Not found' });

    const match = pathMatches.find((r) => r.method === req.method);
    if (!match) {
      res.setHeader('Allow', [...new Set(pathMatches.map((r) => r.method))].join(', '));
      return sendJson(res, 405, { error: 'Method not allowed' });
    }

    const params = pathname.match(match.pattern).slice(1);
    const { status, body } = await match.handler(req, params);
    sendJson(res, status, body);
  } catch (err) {
    const status = err.status ?? 500;
    if (status === 500) console.error(err);
    sendJson(res, status, { error: status === 500 ? 'Internal Server Error' : err.message });
  }
});
```

`URLPattern` is also available in recent Node versions for cleaner path matching.

## Middleware idea

Frameworks compose functions that wrap the handler:

```js
const logger = (next) => async (req, res) => {
  const start = performance.now();
  res.on('finish', () => {
    console.log(`${req.method} ${req.url} ${res.statusCode} ${(performance.now() - start).toFixed(1)}ms`);
  });
  return next(req, res);
};

const withErrors = (next) => async (req, res) => {
  try { await next(req, res); }
  catch (err) {
    if (!res.headersSent) sendJson(res, err.status ?? 500, { error: 'Internal Server Error' });
    else res.destroy();
  }
};

const handler = logger(withErrors(app));
http.createServer(handler).listen(3000);
```

See [Composition and Pipe](../07_functional-programming/04_composition-and-pipe.md).

## Serving static files

```js
import { createReadStream } from 'node:fs';
import { stat } from 'node:fs/promises';
import { pipeline } from 'node:stream/promises';
import path from 'node:path';

const PUBLIC = path.resolve('public');
const TYPES = { '.html': 'text/html', '.css': 'text/css', '.js': 'text/javascript', '.json': 'application/json', '.png': 'image/png' };

async function serveStatic(req, res, pathname) {
  const file = path.resolve(PUBLIC, '.' + pathname);
  if (!file.startsWith(PUBLIC + path.sep)) return sendJson(res, 403, { error: 'Forbidden' });   // path traversal

  try {
    const s = await stat(file);
    if (!s.isFile()) throw Object.assign(new Error(), { code: 'ENOENT' });
    res.writeHead(200, {
      'Content-Type': TYPES[path.extname(file)] ?? 'application/octet-stream',
      'Content-Length': s.size,
    });
    await pipeline(createReadStream(file), res);
  } catch (err) {
    if (err.code === 'ENOENT') return sendJson(res, 404, { error: 'Not found' });
    if (!res.headersSent) sendJson(res, 500, { error: 'Internal Server Error' });
    else res.destroy();
  }
}
```

## Streaming and Server-Sent Events

```js
http.createServer((req, res) => {
  if (req.url === '/events') {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      Connection: 'keep-alive',
    });

    let n = 0;
    const timer = setInterval(() => res.write(`data: ${++n}\n\n`), 1000);
    req.on('close', () => clearInterval(timer));     // client disconnected
  }
}).listen(3000);
```

See [WebSockets and SSE](../15_networking/05_websockets-and-sse.md).

## Timeouts and limits

| Setting | Default (recent Node) | Purpose |
|---------|-----------------------|---------|
| `server.headersTimeout` | 60 s | Max time to receive request headers |
| `server.requestTimeout` | 300 s | Max time to receive the entire request |
| `server.keepAliveTimeout` | 5 s | Idle time to keep a connection open |
| `server.maxHeadersCount` | 2000 | Max number of headers |
| `server.maxRequestsPerSocket` | unlimited | Requests per keep-alive connection |
| `--max-http-header-size` | 16 KiB | CLI flag for header size |

```js
server.requestTimeout = 30_000;
server.keepAliveTimeout = 65_000;   // longer than a typical load balancer idle timeout (e.g. 60 s)
server.headersTimeout = 66_000;     // keep above keepAliveTimeout
```

Tune these for your deployment, especially behind a reverse proxy. Defaults can change between Node releases, so check the docs for yours.

## Client disconnects

```js
req.on('close', () => { /* connection ended: stop expensive work */ });
res.on('finish', () => { /* response fully sent */ });

const ac = new AbortController();
res.on('close', () => ac.abort());          // propagate to fetch / DB calls
```

## Graceful shutdown

```js
function shutdown(signal) {
  console.log(`${signal}: closing server`);

  server.close((err) => {                    // stops accepting; callback fires when all connections end
    process.exit(err ? 1 : 0);
  });

  server.closeIdleConnections();             // drop idle keep-alive sockets now
  setTimeout(() => server.closeAllConnections(), 10_000).unref();   // force after a grace period
}

process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('SIGINT', () => shutdown('SIGINT'));
```

`server.close()` waits for **all** connections to finish, and keep-alive connections can hold it open for a long time. That is why `closeIdleConnections()` and `closeAllConnections()` exist.

## Errors on the server

```js
server.on('error', (err) => {
  if (err.code === 'EADDRINUSE') console.error('Port already in use');
  else console.error(err);
  process.exit(1);
});

server.on('clientError', (err, socket) => {
  socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');   // malformed request from the client
});
```

Always catch errors inside async handlers. An unhandled rejection in a handler leaves the client hanging and can crash the process.

## HTTPS

```js
import https from 'node:https';
import { readFileSync } from 'node:fs';

https.createServer({
  key: readFileSync('key.pem'),
  cert: readFileSync('cert.pem'),
}, handler).listen(443);
```

In production TLS is usually terminated by a reverse proxy (nginx, a cloud load balancer), and Node serves plain HTTP behind it. In that setup read the client address from `X-Forwarded-For` only from proxies you trust.

## HTTP as a client

```js
// fetch is built in
const res = await fetch('http://localhost:3000/users', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ name: 'Ada' }),
  signal: AbortSignal.timeout(5000),
});
console.log(res.status, await res.json());
```

See [Fetch](../15_networking/02_fetch.md). For low-level control use `http.request`.

## Putting it together

```js
import http from 'node:http';

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);

    if (req.method === 'GET' && url.pathname === '/health') {
      return sendJson(res, 200, { ok: true, uptime: process.uptime() });
    }
    if (req.method === 'POST' && url.pathname === '/echo') {
      const data = await readJson(req);
      return sendJson(res, 200, { received: data });
    }
    sendJson(res, 404, { error: 'Not found' });
  } catch (err) {
    sendJson(res, err.status ?? 500, { error: err.status ? err.message : 'Internal Server Error' });
  }
});

server.listen(process.env.PORT ?? 3000);
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| No body size limit | Memory exhaustion | Enforce a limit, return `413` |
| Setting headers after writing | `ERR_HTTP_HEADERS_SENT` | Set headers first; check `res.headersSent` |
| Calling `res.end()` twice or never | Errors, or a hung client | End once on every code path |
| Unhandled errors in async handlers | Hung requests, crashes | `try/catch` around the whole handler |
| Joining `req.url` onto a directory | Path traversal | Resolve and verify the prefix |
| `*Sync` APIs in handlers | Blocks all clients | Async APIs and streams |
| Trusting `X-Forwarded-For` blindly | IP spoofing | Only behind a trusted proxy |
| `server.close()` alone on shutdown | Keep-alive connections hold it open | `closeIdleConnections()` plus a timeout |
| Leaking error details in `500` responses | Information disclosure | Log internally, return a generic message |
| `keepAliveTimeout` below the proxy's timeout | Intermittent `502` errors | Make it longer than the load balancer's idle timeout |
| Ignoring `req.on('close')` for long work | Wasted work for gone clients | Abort via `AbortController` |

## Key takeaways

- `http.createServer` gives you `req` (readable stream) and `res` (writable stream)
- Set status and headers before writing the body; call `res.end()` exactly once
- Read request bodies with a size limit; parse JSON defensively
- Stream large responses with `pipeline`
- Handle errors in every async path; return generic messages for `500`
- Shut down gracefully on `SIGTERM`: stop accepting, close idle connections, force after a timeout
- Frameworks add routing and middleware on top of this same API

**Next:** [Child Process and Cluster](./09_child-process-and-cluster.md)
