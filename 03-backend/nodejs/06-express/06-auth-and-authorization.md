# Authentication & Authorization

Two related but distinct concerns: **authentication** confirms who is making a request; **authorization** decides what that identified person is allowed to do. This file covers implementing both as Express middleware — the deeper mechanics (JWTs, password hashing, sessions, OAuth) live in `08-authentication-security/`.

## Authentication vs authorization, precisely

```
Authentication:  "Who are you?"           → 401 if this fails
Authorization:    "Are you allowed to do this?" → 403 if this fails
```

This maps directly onto the `401` vs `403` distinction from `05-http-web/01-http-methods-and-status-codes.md` — a request with no valid credentials at all gets `401`; a request from a known, authenticated user attempting something they're not permitted to do gets `403`.

---

## Authentication middleware: verifying a JWT

```js
import jwt from "jsonwebtoken";

function requireAuth(req, res, next) {
  const authHeader = req.headers.authorization;
  const token = authHeader?.startsWith("Bearer ") ? authHeader.slice(7) : null;

  if (!token) {
    return res.status(401).json({ error: "Missing token" });
  }

  try {
    req.user = jwt.verify(token, process.env.JWT_SECRET);
    next();
  } catch {
    return res.status(401).json({ error: "Invalid or expired token" });
  }
}
```

```js
app.get("/profile", requireAuth, (req, res) => {
  res.json({ userId: req.user.sub });
});
```

`requireAuth` attaches `req.user` once verification succeeds — every route after it in the chain can now assume `req.user` exists and is trustworthy, without re-verifying anything itself. Full JWT mechanics (access/refresh tokens, signing, expiry) are covered in `08-authentication-security/02-jwt-and-tokens.md`.

### Session-based authentication, as an alternative

```js
function requireAuth(req, res, next) {
  if (!req.session?.userId) {
    return res.status(401).json({ error: "Not authenticated" });
  }
  next();
}
```

Same shape, different mechanism underneath — whether identity comes from a verified JWT or a server-side session (`08-authentication-security/03-sessions.md`), the middleware pattern of "verify, then attach identity to `req`, then `next()`" stays the same.

---

## Authorization middleware: role-based access

```js
function requireRole(...allowedRoles) {
  return function (req, res, next) {
    if (!allowedRoles.includes(req.user.role)) {
      return res.status(403).json({ error: "Forbidden" });
    }
    next();
  };
}

app.delete("/users/:id", requireAuth, requireRole("admin"), (req, res) => {
  // only reachable by an authenticated admin
});

app.patch(
  "/posts/:id",
  requireAuth,
  requireRole("admin", "editor"),
  (req, res) => {
    // reachable by an authenticated admin OR editor
  },
);
```

Notice `requireAuth` always runs **before** `requireRole` — authorization is meaningless without first knowing who the user is; `req.user` must already exist by the time `requireRole` checks `req.user.role`.

---

## Resource-level authorization: beyond just roles

Role checks answer "can this _kind_ of user do this _kind_ of thing" — but many real permission checks are about a specific resource: "can _this_ user edit _this specific_ post?"

```js
async function requireOwnership(req, res, next) {
  const post = await Post.findById(req.params.id);

  if (!post) {
    return res.status(404).json({ error: "Post not found" });
  }

  if (post.authorId !== req.user.sub && req.user.role !== "admin") {
    return res.status(403).json({ error: "You don't own this post" });
  }

  req.post = post; // pass the already-fetched resource forward, avoid a duplicate query
  next();
}

app.put("/posts/:id", requireAuth, requireOwnership, (req, res) => {
  // req.post is already loaded and confirmed to belong to req.user (or user is an admin)
});
```

This kind of check inherently needs a database lookup, unlike a pure role check — worth being deliberate about, since it adds a query to every request on a protected route.

---

## Optional authentication

Some routes behave differently for logged-in vs anonymous users, without strictly _requiring_ login:

```js
function optionalAuth(req, res, next) {
  const authHeader = req.headers.authorization;
  const token = authHeader?.startsWith("Bearer ") ? authHeader.slice(7) : null;

  if (token) {
    try {
      req.user = jwt.verify(token, process.env.JWT_SECRET);
    } catch {
      // invalid token — proceed as anonymous rather than rejecting outright
    }
  }
  next(); // always continues, regardless of whether auth succeeded
}

app.get("/posts/:id", optionalAuth, (req, res) => {
  const post = getPost(req.params.id);
  const canEdit = req.user?.id === post.authorId;
  res.json({ ...post, canEdit });
});
```

Unlike `requireAuth`, this never responds with `401` — it just populates `req.user` when possible and always calls `next()`.

---

## Combining validation, auth, and authorization in one route

```js
app.put(
  "/posts/:id",
  requireAuth, // 401 if not logged in
  requireOwnership, // 403 if logged in but doesn't own the resource
  validate(updatePostSchema), // 400 if the request body is malformed
  updatePostController, // finally, the actual logic
);
```

The order here is deliberate: cheaply reject unauthenticated requests first, before doing a database lookup for ownership, before bothering to validate a body that might get rejected anyway if the user isn't even allowed to make this request at all.

## Common mistakes

- **Checking authorization before authentication** — `req.user` doesn't exist yet if `requireRole` runs before `requireAuth`, causing a confusing crash rather than a clean `401`.
- **Returning `401` for an authorization failure, or `403` for an authentication failure** — mixing these up sends the wrong signal to a client about whether re-authenticating would even help.
- **Trusting a role/permission claim embedded in a JWT that's since become stale** — if roles can change, consider how long-lived tokens should be, or re-check against the database for highly sensitive actions rather than trusting an old token's claims blindly.
- **Re-fetching a resource multiple times across ownership checks and the actual handler** — pass the already-loaded resource forward via `req` (as `requireOwnership` does above) instead.

## Quick summary

- Authentication confirms identity (`401` on failure); authorization confirms permission (`403` on failure) — always run authentication first
- `requireAuth` middleware attaches `req.user`; everything after it can trust that identity without re-verifying
- Role-based authorization (`requireRole`) is a closure-returning middleware, same pattern as configurable validation middleware
- Resource-level ("do you own this") authorization needs its own middleware and typically a database lookup
- Order route middleware to reject cheaply first: auth, then authorization, then validation, then the actual logic

## Next

**`07-file-upload.md`** covers handling file uploads — another kind of request that typically needs both auth and validation applied to it.
