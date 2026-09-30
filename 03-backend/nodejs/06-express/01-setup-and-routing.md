# Setup & Routing

Getting an Express app running, and defining the routes it responds to.

## Install and a minimal app

```bash
npm install express
```

```js
import express from "express";

const app = express();

app.get("/", (req, res) => {
  res.send("Hello, world!");
});

app.listen(3000, () => {
  console.log("Server running on port 3000");
});
```

This is, functionally, the same server shown in `02-core-modules/03-http.md`'s minimal example, but with Express handling routing, body parsing, and response helpers for you.

---

## Essential middleware every app needs

```js
app.use(express.json()); // parse JSON request bodies
app.use(express.urlencoded({ extended: true })); // parse form-encoded bodies
```

Without `express.json()`, `req.body` is `undefined` for JSON requests — this single line replaces the manual stream-reading shown in `02-core-modules/03-http.md`'s raw `http` example.

---

## Basic routing

```js
app.get("/users", (req, res) => {
  /* list users */
});
app.post("/users", (req, res) => {
  /* create a user */
});
app.put("/users/:id", (req, res) => {
  /* replace a user */
});
app.patch("/users/:id", (req, res) => {
  /* update a user */
});
app.delete("/users/:id", (req, res) => {
  /* delete a user */
});
```

Each corresponds directly to the HTTP methods covered in `05-http-web/01-http-methods-and-status-codes.md`.

---

## Route parameters

```js
app.get("/users/:id", (req, res) => {
  console.log(req.params.id); // e.g. "42" from GET /users/42
  res.json({ id: req.params.id });
});
```

### Multiple params

```js
app.get("/users/:userId/posts/:postId", (req, res) => {
  const { userId, postId } = req.params;
  res.json({ userId, postId });
});
```

### Query strings

```js
// GET /users?active=true&page=2
app.get("/users", (req, res) => {
  console.log(req.query.active); // "true" — a string, same caveat as process.env
  console.log(req.query.page); // "2"
});
```

Route params (`:id`) are for identifying a specific resource; query strings (`?active=true`) are for filtering, sorting, and pagination — covered in more depth in `09-api-development/02-versioning-and-pagination.md` and `09-api-development/03-filtering-and-sorting.md`.

---

## Sending responses

```js
res.send("plain text or HTML");
res.json({ key: "value" }); // sets Content-Type: application/json automatically
res.status(201).json({ id: 1 }); // chain a status code before sending
res.sendStatus(204); // send just a status code, no body
```

`res.json()` is almost always what you want for an API — it handles `Content-Type` and serialization for you, rather than manually calling `JSON.stringify` and setting headers as shown in the raw `http` example.

---

## Organizing routes with `express.Router()`

As an app grows, keeping every route in one file becomes unmanageable. `Router` lets you group related routes into their own module:

```js
// routes/users.js
import { Router } from "express";

const router = Router();

router.get("/", (req, res) => {
  /* list users */
});
router.get("/:id", (req, res) => {
  /* get one user */
});
router.post("/", (req, res) => {
  /* create a user */
});

export default router;
```

```js
// app.js
import usersRouter from "./routes/users.js";

app.use("/users", usersRouter);
```

Requests to `/users` and `/users/:id` are handled by `usersRouter`, but the router itself doesn't need to know its own prefix — `app.use("/users", ...)` supplies it, which means the same router could be mounted at a different path elsewhere without any changes inside it.

### A typical folder structure

```
src/
├── routes/
│   ├── users.js
│   ├── posts.js
│   └── index.js
├── controllers/        (see 03-controllers.md)
└── app.js
```

```js
// routes/index.js — combine all routers into one
import { Router } from "express";
import usersRouter from "./users.js";
import postsRouter from "./posts.js";

const router = Router();
router.use("/users", usersRouter);
router.use("/posts", postsRouter);

export default router;
```

```js
// app.js
import routes from "./routes/index.js";
app.use("/api", routes); // everything now lives under /api/users, /api/posts, etc.
```

---

## Route matching order matters

```js
// ❌ /users/me never matches — /users/:id catches it first, treating "me" as an id
app.get("/users/:id", handler);
app.get("/users/me", handler);
```

```js
// ✅ specific routes before parameterized ones
app.get("/users/me", handler);
app.get("/users/:id", handler);
```

Express matches routes in the order they're registered, top to bottom — a common bug is registering a catch-all parameterized route before a more specific literal one that should take priority.

---

## Route-level middleware

```js
app.get("/admin/dashboard", requireAuth, requireAdmin, (req, res) => {
  res.json({ secret: "data" });
});
```

Multiple functions can be passed to a route — each one runs in order, and must call `next()` to pass control to the next (covered fully in `02-middleware.md`).

## Common mistakes

- **Forgetting `express.json()`** — `req.body` is `undefined` for any JSON request without it.
- **Registering a parameterized route before a more specific literal one** — the parameterized route matches first and the literal one is never reached.
- **Not using `Router()` as an app grows** — a single giant route file quickly becomes hard to navigate and merge-conflict-prone in a team.
- **Confusing route params and query strings** — `:id` is part of the URL path structure; `?key=value` is for optional filters/options.

## Quick summary

- `express.json()`/`express.urlencoded()` are needed before `req.body` is populated
- Route params (`:id`) identify a specific resource; query strings (`?key=value`) filter/modify a request
- `express.Router()` groups related routes into their own module, mounted at a prefix via `app.use()`
- Route order matters — Express matches top to bottom, so specific routes need to come before catch-all parameterized ones

## Next

**`02-middleware.md`** covers what's actually happening between a request arriving and your route handler running.
