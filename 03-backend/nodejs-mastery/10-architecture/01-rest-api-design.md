# REST API Design

Modeling your domain as resources, naming URLs consistently, and using HTTP methods and status codes the way clients expect.

## What REST actually means (in practice)

REST is an architectural style built on a few ideas. In day-to-day API work, it boils down to:

- **Resources, not actions.** The URL identifies a *thing* (`/posts/42`); the HTTP method says what to do with it.
- **Uniform interface.** Standard methods (`GET`, `POST`, ...) with standard meanings.
- **Stateless.** Every request carries everything the server needs (credentials, parameters). The server doesn't remember the previous request.
- **Representations.** The client gets a representation of a resource (usually JSON), not the resource itself.

Most "REST APIs" in the wild are really "JSON over HTTP with resource-style URLs." That's fine — the conventions below are what matter.

---

## Think in resources

A resource is a noun in your domain: a user, a post, a comment, an invoice.

```
❌ Verbs in URLs (RPC style)
POST /createPost
POST /getPostById
POST /deletePost?id=42

✅ Nouns in URLs, verbs in HTTP methods
POST   /posts
GET    /posts/42
DELETE /posts/42
```

### Naming rules

| Rule | Example |
|---|---|
| **Plural nouns** for collections | `/posts`, not `/post` |
| **Lowercase, hyphenated** words | `/blog-posts`, not `/blogPosts` or `/blog_posts` |
| **No trailing slash** | `/posts`, not `/posts/` |
| **No file extensions** | `/posts/42`, not `/posts/42.json` (use `Accept` header) |
| **IDs identify a single resource** | `/posts/42` |
| **Nest to show ownership, but only 1–2 levels** | `/users/7/posts` |

### Nesting: don't go too deep

```
✅ /users/7/posts                  posts belonging to user 7
✅ /posts/42/comments              comments on post 42

❌ /users/7/posts/42/comments/9/likes/3    brittle and hard to use
```

Once a resource has its own globally unique ID, give it a top-level URL too:

```
GET /comments/9          ← usable without knowing the user and post
```

### Actions that don't fit CRUD

Some operations genuinely aren't "create/read/update/delete." Two common patterns:

```
# 1. Model the action as a sub-resource (preferred)
POST /orders/42/cancellation
POST /users/7/password-reset
POST /posts/42/publication

# 2. Use a verb as a last resort, on a sub-path of the resource
POST /orders/42/cancel
POST /reports/generate
```

Use `POST` for these. Pick one style and stay consistent across the whole API.

---

## HTTP methods

| Method | Meaning | Safe? | Idempotent? | Has body? |
|---|---|---|---|---|
| `GET` | Read | ✅ | ✅ | No |
| `POST` | Create / trigger an action | ❌ | ❌ | Yes |
| `PUT` | Replace the entire resource | ❌ | ✅ | Yes |
| `PATCH` | Partially update | ❌ | Not guaranteed | Yes |
| `DELETE` | Remove | ❌ | ✅ | Rarely |

- **Safe** = doesn't change server state. Browsers, crawlers, and proxies assume `GET` is safe, so never change data on a `GET`.
- **Idempotent** = calling it N times has the same effect as calling it once. This is why retries of `PUT` and `DELETE` are harmless, but retries of `POST` are not (see `06-idempotency.md`).

### `PUT` vs `PATCH`

```js
// Existing resource: { id: 42, title: "Hello", body: "World", published: false }

// PUT replaces the WHOLE resource — omitted fields are reset/removed
PUT /posts/42   { "title": "New title", "body": "New body", "published": true }

// PATCH changes only the fields sent — everything else stays as it was
PATCH /posts/42 { "published": true }
```

`PATCH` is what most clients want for edits, and most APIs implement only `PATCH` and skip `PUT`. Pick what makes sense, but don't make `PUT` behave like `PATCH`.

---

## The full CRUD example

```js
// routes/posts.js
import { Router } from "express";
import * as posts from "../controllers/posts.js";

const router = Router();

router.get("/", posts.list);
router.post("/", posts.create);
router.get("/:id", posts.get);
router.patch("/:id", posts.update);
router.delete("/:id", posts.remove);

export default router;
```

```js
// controllers/posts.js
import { Post } from "../models/Post.js";
import { AppError } from "../utils/AppError.js";

export async function create(req, res) {
  const post = await Post.create({ ...req.body, authorId: req.user.id });

  res
    .status(201)
    .location(`/api/v1/posts/${post.id}`)     // where the new resource lives
    .json({ data: toPostDto(post) });
}

export async function get(req, res) {
  const post = await Post.findById(req.params.id);
  if (!post) throw new AppError(404, "not_found", "Post not found");

  res.json({ data: toPostDto(post) });
}

export async function update(req, res) {
  const post = await Post.findOneAndUpdate(
    { _id: req.params.id, authorId: req.user.id },    // ownership check (IDOR)
    req.body,
    { new: true }
  );
  if (!post) throw new AppError(404, "not_found", "Post not found");

  res.json({ data: toPostDto(post) });
}

export async function remove(req, res) {
  await Post.deleteOne({ _id: req.params.id, authorId: req.user.id });
  res.sendStatus(204);          // no body
}
```

Notes:

- `async` handlers that `throw` work out of the box in Express 5. In Express 4 you need a wrapper or a package like `express-async-errors` (see `06-express/04-error-handling.md`).
- Ownership checks live in the query — see `08-authentication-security/05-common-vulnerabilities.md` (IDOR).
- `toPostDto` maps a database record to the public shape (next section).

---

## Status codes: pick the most specific correct one

### Success (2xx)

| Code | Use for |
|---|---|
| `200 OK` | Successful `GET`, `PATCH`, `PUT` returning a body |
| `201 Created` | Resource created; include a `Location` header |
| `202 Accepted` | Request accepted for **asynchronous** processing (queued job) — see `11-async-processing/` |
| `204 No Content` | Success with nothing to return (typical for `DELETE`) |

### Client errors (4xx) — the caller did something wrong

| Code | Use for |
|---|---|
| `400 Bad Request` | Malformed request (invalid JSON, bad query syntax) |
| `401 Unauthorized` | Not authenticated (missing/invalid credentials) |
| `403 Forbidden` | Authenticated, but not allowed |
| `404 Not Found` | Resource doesn't exist (or you're hiding that it does) |
| `405 Method Not Allowed` | URL exists, but not for this method |
| `409 Conflict` | State conflict (duplicate email, edit conflict) |
| `410 Gone` | Resource existed and was permanently removed (also good for sunset versions) |
| `413 Payload Too Large` | Body exceeds limit |
| `415 Unsupported Media Type` | Wrong `Content-Type` |
| `422 Unprocessable Content` | Well-formed JSON, but fails validation |
| `429 Too Many Requests` | Rate limited — see `08-authentication-security/06-rate-limiting.md` |

### Server errors (5xx) — the server did something wrong

| Code | Use for |
|---|---|
| `500 Internal Server Error` | Unexpected failure |
| `502 Bad Gateway` | Upstream service returned garbage |
| `503 Service Unavailable` | Overloaded or down for maintenance (add `Retry-After`) |
| `504 Gateway Timeout` | Upstream service timed out |

### The `400` vs `422` decision

Pick one convention and keep it. A common, sensible rule:

- `400` → the request can't even be *parsed* (broken JSON, wrong types of query params).
- `422` → it parsed fine, but the **values** break business or validation rules (email invalid, title too short).

Many teams use `400` for both, which is also fine — consistency is what counts.

### The `401` vs `403` vs `404` decision

```
No/invalid token                        → 401
Valid token, but role lacks permission  → 403
Valid token, resource belongs to someone else → 404 (don't confirm it exists)
```

---

## Response shape

Decide on one shape and use it for **every** endpoint.

### Envelope or bare?

```jsonc
// Option A: bare — the resource is the body
{ "id": 42, "title": "Hello" }

// Option B: envelope — data separate from metadata
{
  "data": { "id": 42, "title": "Hello" }
}

// Collections in an envelope leave room for pagination metadata
{
  "data": [ { "id": 42 }, { "id": 43 } ],
  "meta": { "total": 120, "limit": 20 },
  "links": { "next": "/api/v1/posts?cursor=abc" }
}
```

An envelope costs a few bytes but lets you add `meta` and `links` later **without breaking clients**. That's usually worth it. Bare arrays at the top level are a trap — you can never add pagination info to `[ ... ]` without changing its type.

### Use DTOs — don't leak your database

Never return raw database records. Map them to a public shape:

```js
function toPostDto(post) {
  return {
    id: post.id,
    title: post.title,
    body: post.body,
    published: post.published,
    author: { id: post.authorId },
    createdAt: post.createdAt.toISOString(),
    updatedAt: post.updatedAt.toISOString(),
  };
}
```

This protects against:

- leaking internal fields (password hashes, `__v`, soft-delete flags),
- coupling clients to your schema — you can rename columns without breaking the API,
- inconsistent formats (`_id` vs `id`, `Date` objects vs strings).

### Field conventions

| Convention | Recommendation |
|---|---|
| **Naming** | `camelCase` (natural in JSON/JS) — or `snake_case` — **pick one** |
| **Dates** | ISO 8601 in UTC: `"2026-09-30T10:15:00.000Z"` |
| **IDs** | Strings (even if numeric internally) — safer across languages and future migrations |
| **Money** | Integer minor units (`1999` = $19.99) plus a currency code, **never floats** |
| **Booleans** | Real booleans, not `"true"` or `1` |
| **Nulls vs missing** | Be consistent: a known-empty value is `null`, an unset optional field may be omitted |
| **Enums** | Lowercase strings: `"draft"`, `"published"` |

```js
// ❌ floating-point money
{ "price": 19.99 }     // 0.1 + 0.2 !== 0.3 — rounding bugs

// ✅ integer minor units + currency
{ "price": { "amount": 1999, "currency": "USD" } }
```

---

## Headers that matter

```js
// Content negotiation — always say what you're sending
res.type("application/json");               // res.json() does this automatically

// Location of a newly created resource
res.status(201).location(`/api/v1/posts/${post.id}`);

// Caching of GET responses (see 05-http-web/05-caching-and-compression.md)
res.set("Cache-Control", "public, max-age=60");
res.set("ETag", etag);

// Request tracing (see 14-logging-observability/02-correlation-id.md)
res.set("X-Request-Id", req.id);
```

### Conditional requests save bandwidth

Express generates weak ETags for `res.json()` automatically. A client that sends `If-None-Match` gets a bodyless `304 Not Modified` when nothing changed:

```
GET /posts/42
→ 200 OK, ETag: W/"abc123"

GET /posts/42        (If-None-Match: W/"abc123")
→ 304 Not Modified   (no body)
```

For write safety, the mirror image is `If-Match` on `PUT`/`PATCH` for **optimistic concurrency** — the server rejects an update with `412 Precondition Failed` if the resource changed since the client read it, preventing lost updates.

---

## Relationships and expanding data

Clients often want a post *and* its author. Three common approaches:

```jsonc
// 1. Just the ID — the client makes a second request
{ "id": 42, "authorId": "7" }

// 2. A link
{ "id": 42, "author": { "id": "7", "href": "/api/v1/users/7" } }

// 3. Opt-in expansion — the client asks for it
GET /posts/42?include=author
{ "id": 42, "author": { "id": "7", "name": "Sam" } }
```

The opt-in `include` (or `expand`) approach gives clients control without bloating every response. Whitelist the allowed values — never pass `include` straight into a database query.

---

## Designing for the long term

**Additive changes are safe. Removing or changing things is not.**

| Change | Breaking? |
|---|---|
| Add a new endpoint | No |
| Add an optional request field | No |
| Add a new field to a response | No (clients must ignore unknown fields) |
| Remove or rename a field | **Yes** |
| Change a field's type | **Yes** |
| Make an optional field required | **Yes** |
| Change a status code or error format | **Yes** |
| Tighten validation | Potentially |

This is exactly the problem `02-versioning-and-pagination.md` addresses.

### Other design habits

- **Return the created/updated resource**, so clients don't need a follow-up `GET`.
- **Make `DELETE` idempotent** — deleting something already gone should still succeed (`204`) or return `404` consistently. Pick one.
- **Use `HEAD` and `OPTIONS`** correctly. Express answers both automatically for routes you've defined.
- **Always set a body size limit**: `express.json({ limit: "100kb" })`.
- **Require `Content-Type: application/json`** for bodies and return `415` otherwise.
- **Document with OpenAPI (Swagger).** A machine-readable spec powers documentation, client generation, and contract tests.

---

## A quick design checklist for any endpoint

- [ ] URL is a plural noun; method expresses the action
- [ ] Correct status codes for success **and** each failure case
- [ ] Input validated (`04-validation.md`)
- [ ] Authenticated and authorized, with ownership checks (`08-authentication-security/`)
- [ ] Response is a DTO in the standard shape — no internal fields
- [ ] Collections are paginated (`02-versioning-and-pagination.md`)
- [ ] Errors use the standard format (`05-error-responses.md`)
- [ ] Unsafe retries considered (`06-idempotency.md`)

## Next

**`02-versioning-and-pagination.md`** covers how to change your API without breaking existing clients, and how to return large collections in manageable pages.
