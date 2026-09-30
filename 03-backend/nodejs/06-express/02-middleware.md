# Middleware

The core mechanism Express is built around: functions that run in sequence between a request arriving and a response being sent, each able to inspect, modify, or short-circuit the request.

## The basic shape

```js
function myMiddleware(req, res, next) {
  console.log(`${req.method} ${req.url}`);
  next(); // pass control to the next middleware/route handler
}

app.use(myMiddleware);
```

Every middleware function receives `req`, `res`, and `next`. Calling `next()` passes control forward; **not** calling it (without also sending a response) leaves the request hanging forever — one of the most common Express bugs.

---

## The request pipeline

```js
app.use(logger);
app.use(express.json());
app.use(authenticate);

app.get("/users", (req, res) => {
  res.json(users);
});

app.use(errorHandler);
```

```
Request
  ↓
logger        → console.log, then next()
  ↓
express.json() → parses body, then next()
  ↓
authenticate   → checks auth, then next() (or sends 401 and stops)
  ↓
route handler   → sends the actual response
  ↓
(errorHandler only runs if something threw/called next(err))
```

Middleware runs **in the order it's registered** — this is why `express.json()` needs to come before any route that reads `req.body`, and why auth middleware needs to come before the routes it's meant to protect.

---

## Application-level vs route-level middleware

```js
// application-level — runs for EVERY request
app.use(logger);

// route-level — runs only for this specific route
app.get("/admin", requireAuth, (req, res) => {
  res.json({ secret: true });
});

// path-scoped — runs for every request under /api
app.use("/api", apiLogger);
```

---

## Middleware that modifies `req`/`res`

```js
app.use((req, res, next) => {
  req.requestTime = Date.now();
  next();
});

app.get("/", (req, res) => {
  res.json({ requestTime: req.requestTime });
});
```

Attaching custom properties to `req` is the standard way middleware passes information forward to whatever runs after it — `authenticate` middleware attaching `req.user`, for example, is the pattern `06-auth-and-authorization.md` relies on heavily.

---

## Common built-in and third-party middleware

```js
app.use(express.json()); // parse JSON bodies
app.use(express.urlencoded({ extended: true })); // parse form bodies
app.use(express.static("public")); // serve static files from a folder
```

```bash
npm install cors helmet morgan cookie-parser compression
```

```js
import cors from "cors";
import helmet from "helmet";
import morgan from "morgan";
import cookieParser from "cookie-parser";
import compression from "compression";

app.use(helmet()); // security headers — see 08-authentication-security/07-helmet.md
app.use(cors()); // CORS — see 05-http-web/04-cors.md
app.use(morgan("dev")); // request logging
app.use(cookieParser()); // parse Cookie header into req.cookies — see 05-http-web/03-cookies.md
app.use(compression()); // gzip responses — see 05-http-web/05-caching-and-compression.md
```

---

## Error-handling middleware: a special shape

```js
app.use((err, req, res, next) => {
  console.error(err);
  res.status(err.statusCode || 500).json({ error: err.message });
});
```

Express recognizes error-handling middleware specifically by its **four** parameters (`err, req, res, next`) — this is not optional, and must be registered **after** all other routes/middleware. Full coverage in `04-error-handling.md`.

---

## Writing configurable middleware (a closure, in practice)

```js
function requireRole(role) {
  return function (req, res, next) {
    if (req.user?.role !== role) {
      return res.status(403).json({ error: "Forbidden" });
    }
    next();
  };
}

app.get("/admin", requireAuth, requireRole("admin"), (req, res) => {
  res.json({ secret: true });
});
```

`requireRole("admin")` runs once and returns the actual middleware function, which closes over `role` — the exact closure pattern from `03-javascript-for-node/03-closures.md`, applied directly to building configurable Express middleware.

---

## Handling async errors in middleware/route handlers

```js
// ❌ a rejected promise here is NOT automatically caught by Express (pre-Express 5)
app.get("/users/:id", async (req, res) => {
  const user = await db.findUser(req.params.id); // if this throws, it becomes an unhandled rejection
  res.json(user);
});
```

```js
// ✅ wrap it, or use a helper that does this for you
app.get("/users/:id", async (req, res, next) => {
  try {
    const user = await db.findUser(req.params.id);
    res.json(user);
  } catch (err) {
    next(err); // hands off to error-handling middleware
  }
});
```

A common solution is a small wrapper to avoid repeating `try/catch` in every async route:

```js
const asyncHandler = (fn) => (req, res, next) => {
  Promise.resolve(fn(req, res, next)).catch(next);
};

app.get(
  "/users/:id",
  asyncHandler(async (req, res) => {
    const user = await db.findUser(req.params.id);
    res.json(user);
  }),
);
```

(Express 5 handles this automatically for `async` route handlers — worth checking which major version a given project uses, since the manual wrapper is only necessary on Express 4 and earlier.) Full coverage in `04-error-handling.md`.

## Common mistakes

- **Forgetting to call `next()`** — the request hangs forever, with no response and no error, since Express is waiting for something that never happens.
- **Calling `next()` AND sending a response in the same middleware** — leads to a "headers already sent" error, since Express then tries to move on to the next handler which also tries to respond.
- **Registering error-handling middleware before other routes** — it must come last, or it won't be positioned to catch errors from everything before it.
- **Not catching errors from `async` route handlers on Express 4** — they become unhandled rejections rather than reaching your error handler, unless wrapped.
- **Registering `express.json()` after routes that need `req.body`** — order matters; middleware only affects requests that reach it _after_ it's registered.

## Quick summary

- Middleware runs in registration order; each one must call `next()` to continue the chain, or send a response to end it
- Error-handling middleware is identified by its four-argument signature `(err, req, res, next)` and must be registered last
- Middleware commonly attaches data to `req` (like `req.user`) for later middleware/handlers to use
- A function returning a middleware function (a closure) is the standard way to build configurable middleware like `requireRole(role)`
- Async route handlers need explicit `try/catch` + `next(err)` (or a wrapper) on Express 4; Express 5 handles this automatically

## Next

**`03-controllers.md`** covers organizing the actual business logic behind a route, separate from the routing/middleware layer.
