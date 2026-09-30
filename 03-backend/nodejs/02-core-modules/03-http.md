# `http` — Building Servers Without a Framework

Node's built-in module for creating HTTP servers and making HTTP requests. Express, Fastify, and virtually every Node web framework are built directly on top of this module — understanding it demystifies what those frameworks are actually doing underneath.

```js
import http from "node:http";
```

## A minimal server

```js
import http from "node:http";

const server = http.createServer((req, res) => {
  res.writeHead(200, { "Content-Type": "text/plain" });
  res.end("Hello, world!");
});

server.listen(3000, () => {
  console.log("Server running on port 3000");
});
```

This is, functionally, what `app.listen()` in Express eventually calls under the hood.

---

## The request object (`req`)

```js
http.createServer((req, res) => {
  console.log(req.method); // "GET", "POST", etc.
  console.log(req.url); // "/users/123?active=true"
  console.log(req.headers); // { host: "...", "user-agent": "...", ... }
});
```

`req` is a **readable stream** (see `05-streams.md`) — the request body isn't available as a ready-made property, you have to read it from the stream yourself:

```js
http.createServer((req, res) => {
  let body = "";
  req.on("data", (chunk) => {
    body += chunk;
  });
  req.on("end", () => {
    const parsed = JSON.parse(body);
    res.end(`Received: ${parsed.name}`);
  });
});
```

This manual body-parsing is exactly what `express.json()` middleware does for you automatically — it's a big part of why frameworks exist at all.

---

## The response object (`res`)

```js
res.writeHead(200, { "Content-Type": "application/json" });
res.write("partial content"); // can call multiple times before end()
res.end(JSON.stringify({ ok: true }));
```

- `res.writeHead(statusCode, headers)` — sets the status and headers (must happen before any `write`/`end` calls)
- `res.write(chunk)` — sends a chunk of the response body (optional, can be called multiple times)
- `res.end([data])` — finishes the response, optionally sending a final chunk

### Shortcuts

```js
res.statusCode = 404;
res.setHeader("Content-Type", "application/json");
res.end(JSON.stringify({ error: "Not found" }));
```

---

## Basic routing (what a framework replaces)

```js
const server = http.createServer((req, res) => {
  if (req.method === "GET" && req.url === "/") {
    res.end("Home page");
  } else if (req.method === "GET" && req.url === "/about") {
    res.end("About page");
  } else if (req.method === "POST" && req.url === "/users") {
    // ... handle creating a user
  } else {
    res.statusCode = 404;
    res.end("Not found");
  }
});
```

This is manageable for a handful of routes, and becomes unwieldy fast — matching URL patterns, extracting params (`/users/:id`), parsing query strings, and organizing dozens of routes is exactly what Express's routing layer (`express/03-routing.md`) exists to handle for you.

---

## Parsing the URL and query string

```js
import { URL } from "node:url";

http.createServer((req, res) => {
  const parsedUrl = new URL(req.url, `http://${req.headers.host}`);
  console.log(parsedUrl.pathname); // "/search"
  console.log(parsedUrl.searchParams.get("q")); // "nodejs"
});
```

`req.url` is only the path + query string (not a full URL) — the `URL` constructor needs a base to resolve it against, which is why `req.headers.host` is passed in as the second argument.

---

## Making HTTP requests

```js
import http from "node:http";

http.get("http://example.com/api/data", (res) => {
  let data = "";
  res.on("data", (chunk) => (data += chunk));
  res.on("end", () => console.log(JSON.parse(data)));
});
```

### In practice, use `fetch` instead

Since Node 18, the global `fetch` API (the same one browsers have) is built in and is almost always preferable for making outgoing requests:

```js
const res = await fetch("http://example.com/api/data");
const data = await res.json();
```

`http.request`/`http.get` are mostly relevant now for understanding what's happening underneath, or for very low-level streaming use cases `fetch` doesn't cover as naturally.

---

## `https` — the same API, over TLS

```js
import https from "node:https";
import fs from "node:fs";

const server = https.createServer(
  {
    key: fs.readFileSync("key.pem"),
    cert: fs.readFileSync("cert.pem"),
  },
  (req, res) => {
    res.end("Secure!");
  },
);

server.listen(443);
```

Identical API to `http`, just requiring a TLS certificate and key. In most real deployments, TLS termination happens at a reverse proxy or load balancer (Nginx, a cloud load balancer) in front of the Node process, rather than in Node itself — so plain `http` inside the app is often correct even in production.

---

## Why frameworks exist

Given everything above, a framework like Express is mostly a thin, well-designed layer over `http.createServer` that provides:

- Pattern-based routing (`/users/:id`) instead of manual `if` chains on `req.url`
- Middleware — composable functions that run before your route handler (auth, logging, body parsing)
- Automatic body/query parsing
- A cleaner `res.json()`/`res.status()` API instead of manual `writeHead`/`end`

None of this is magic — it's exactly the kind of code shown in this file, generalized and packaged. See `express/01-express-basics.md`.

## Quick summary

- `http.createServer((req, res) => {...})` is the foundation every Node web framework builds on
- `req` is a readable stream — reading a request body means listening for `data`/`end` events yourself, unless a framework handles it
- `res.writeHead`/`write`/`end` control the response; frameworks wrap these in nicer APIs like `res.json()`
- Use the global `fetch` for outgoing requests in modern Node rather than `http.request`/`http.get`
- `https` mirrors `http`'s API but requires a certificate — often handled by a reverse proxy instead, in production

## Next

**`04-events.md`** covers the `EventEmitter` pattern — the same `req.on("data", ...)` style seen above is used throughout Node's core modules and most third-party libraries.
