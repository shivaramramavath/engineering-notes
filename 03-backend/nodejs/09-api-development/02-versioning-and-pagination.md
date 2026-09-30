# Versioning & Pagination

Two problems every growing API runs into: how to change it without breaking existing clients, and how to return large collections without melting your server.

# Part 1 — Versioning

## Why version at all?

The moment a client depends on your API, its shape is a promise. Mobile apps can't be force-updated, partners integrate once and forget, and other teams deploy on their own schedule. Versioning lets you ship **breaking changes** while old clients keep working.

**First, avoid needing a new version.** Most changes can be made additively (see the table in `01-rest-api-design.md`): new endpoints, new optional fields, new response fields. Only truly breaking changes need a new version.

---

## Versioning strategies

### 1. URL path versioning (most common)

```
GET /api/v1/posts
GET /api/v2/posts
```

| Pros | Cons |
|---|---|
| Obvious and easy to see in logs, docs, and curl | The "same" resource has different URLs |
| Trivial to route, cache, and rate-limit separately | Encourages big-bang version bumps |
| Works in any browser or tool | |

### 2. Header versioning

```
GET /api/posts
Accept: application/vnd.myapp.v2+json
```

Or a custom header like `API-Version: 2`.

| Pros | Cons |
|---|---|
| Clean URLs; purist-friendly | Invisible in logs and browser address bars |
| | Harder to test and cache (needs `Vary: Accept` / `Vary: API-Version`) |

### 3. Query parameter

```
GET /api/posts?version=2
```

Easy, but mixes versioning into resource identity and is easy to forget. Rarely recommended.

### 4. Date-based versions

```
Stripe-Version: 2026-09-30
```

Clients pin the date they integrated; the server applies transformations to keep old behavior. Very powerful, but requires significant engineering (a chain of response transformers). Used by Stripe, worth knowing about.

**Recommendation:** start with **URL path versioning** (`/api/v1`). It's the simplest and what most teams — and most interview answers — expect.

---

## Implementing URL versioning in Express

Each version gets its own router, mounted under its prefix:

```js
// routes/v1/index.js
import { Router } from "express";
import postsV1 from "./posts.js";
import usersV1 from "./users.js";

const router = Router();
router.use("/posts", postsV1);
router.use("/users", usersV1);
export default router;
```

```js
// routes/v2/index.js
import { Router } from "express";
import postsV2 from "./posts.js";        // changed
import usersV1 from "../v1/users.js";    // reuse unchanged v1 routes

const router = Router();
router.use("/posts", postsV2);
router.use("/users", usersV1);
export default router;
```

```js
// app.js
import v1 from "./routes/v1/index.js";
import v2 from "./routes/v2/index.js";

app.use("/api/v1", v1);
app.use("/api/v2", v2);
```

```
src/
├── routes/
│   ├── v1/
│   └── v2/
├── controllers/        shared where behavior is unchanged
├── services/           ONE business-logic layer shared by all versions
└── dto/
    ├── v1/post.js      each version has its own response mapping
    └── v2/post.js
```

**Key idea:** versions should differ mostly at the **edges** (routes, validation, DTOs). Keep business logic in a shared service layer (`10-architecture/03-repository-and-service-pattern.md`) so you fix a bug once, not per version.

```js
// dto/v1/post.js
export const toPostDto = (p) => ({ id: p.id, title: p.title, author: p.authorName });

// dto/v2/post.js — author became an object (a breaking change)
export const toPostDto = (p) => ({
  id: p.id,
  title: p.title,
  author: { id: p.authorId, name: p.authorName },
});
```

---

## Deprecation and sunsetting

Don't remove a version overnight. Announce, warn, then retire.

```js
// middleware/deprecated.js
export function deprecated({ sunset, successor }) {
  return (req, res, next) => {
    res.set("Deprecation", "true");
    res.set("Sunset", new Date(sunset).toUTCString());          // when it will stop working
    res.set("Link", `<${successor}>; rel="successor-version"`);
    next();
  };
}

app.use("/api/v1", deprecated({
  sunset: "2027-06-30",
  successor: "/api/v2",
}), v1);
```

A sensible lifecycle:

1. **Announce** — changelog, email, docs banner.
2. **Deprecate** — add `Deprecation` and `Sunset` headers; keep it working.
3. **Monitor** — log which clients still call the old version (by API key or user agent) and contact them.
4. **Sunset** — after the date, return `410 Gone` with a message pointing to the new version.

---

## Versioning rules of thumb

- Version the **whole API**, not individual endpoints (easier for clients to reason about).
- Keep the number of live versions small — two is ideal, three is a burden.
- Never change the meaning of an existing field within a version.
- Give a migration guide for every breaking release.
- Clients must be told to **ignore unknown fields**, so you can add fields freely.

---

# Part 2 — Pagination

## Why paginate?

```js
// ❌ returns every row: 2 million posts → huge memory, slow response, angry client
app.get("/posts", async (req, res) => {
  res.json(await Post.find());
});
```

Unbounded lists cause slow queries, memory pressure (blocking the event loop while serializing huge JSON — see `15-performance/01-event-loop-performance.md`), and wasted bandwidth. **Every collection endpoint needs a maximum page size**, enforced server-side, even if the client doesn't ask for paging.

---

## Strategy 1: Offset pagination (`page` / `limit`)

```
GET /posts?page=3&limit=20
GET /posts?offset=40&limit=20        (equivalent)
```

```js
export async function list(req, res) {
  const page = Math.max(1, Number(req.query.page) || 1);
  const limit = Math.min(100, Math.max(1, Number(req.query.limit) || 20));   // cap at 100
  const offset = (page - 1) * limit;

  const [items, total] = await Promise.all([
    Post.find().sort({ createdAt: -1, _id: -1 }).skip(offset).limit(limit),
    Post.countDocuments(),
  ]);

  res.json({
    data: items.map(toPostDto),
    meta: {
      page,
      limit,
      total,
      totalPages: Math.ceil(total / limit),
    },
  });
}
```

SQL equivalent:

```sql
SELECT * FROM posts ORDER BY created_at DESC, id DESC LIMIT 20 OFFSET 40;
SELECT COUNT(*) FROM posts;
```

### Advantages

- Simple to implement and understand.
- Clients can jump to any page ("page 7 of 52") and show page numbers.

### Problems

1. **Slow on deep pages.** `OFFSET 1000000` makes the database walk and discard a million rows before returning 20.
2. **Unstable results.** If a row is inserted or deleted while a user pages through, items can be **skipped or shown twice**.
3. **`COUNT(*)` is expensive** on large tables (it may scan the whole table or index on every request).

Use offset pagination for **small to medium, slow-changing datasets** and admin UIs that need page numbers.

---

## Strategy 2: Cursor (keyset) pagination

Instead of "skip N rows," say "give me rows **after this one**." The cursor marks the last item the client saw.

```
GET /posts?limit=20
→ { data: [...20 items], meta: { nextCursor: "eyJjIjoiMjAyNi0wOS0zMF..." } }

GET /posts?limit=20&cursor=eyJjIjoiMjAyNi0wOS0zMF...
→ next 20 items
```

### The mechanics (SQL)

For a feed ordered by newest first, with `id` as the tie-breaker:

```sql
-- first page
SELECT * FROM posts
ORDER BY created_at DESC, id DESC
LIMIT 21;                                   -- fetch one extra to know if there's a next page

-- next page, given the last row's (created_at, id)
SELECT * FROM posts
WHERE (created_at, id) < ($1, $2)           -- row-value comparison
ORDER BY created_at DESC, id DESC
LIMIT 21;
```

With a composite index on `(created_at DESC, id DESC)`, this is fast **no matter how deep** the client goes — the database seeks straight to the position instead of skipping rows.

### The implementation

```js
// utils/cursor.js — opaque cursors
export const encodeCursor = (obj) =>
  Buffer.from(JSON.stringify(obj)).toString("base64url");

export function decodeCursor(cursor) {
  try {
    return JSON.parse(Buffer.from(cursor, "base64url").toString());
  } catch {
    return null;       // invalid cursor → caller returns 400
  }
}
```

```js
// controllers/posts.js (PostgreSQL with pg)
export async function list(req, res) {
  const limit = Math.min(100, Math.max(1, Number(req.query.limit) || 20));

  let rows;
  if (req.query.cursor) {
    const c = decodeCursor(req.query.cursor);
    if (!c) throw new AppError(400, "invalid_cursor", "Malformed cursor");

    ({ rows } = await pool.query(
      `SELECT * FROM posts
       WHERE (created_at, id) < ($1, $2)
       ORDER BY created_at DESC, id DESC
       LIMIT $3`,
      [c.createdAt, c.id, limit + 1]
    ));
  } else {
    ({ rows } = await pool.query(
      `SELECT * FROM posts ORDER BY created_at DESC, id DESC LIMIT $1`,
      [limit + 1]
    ));
  }

  const hasMore = rows.length > limit;
  const items = hasMore ? rows.slice(0, limit) : rows;
  const last = items.at(-1);

  res.json({
    data: items.map(toPostDto),
    meta: {
      limit,
      hasMore,
      nextCursor: hasMore
        ? encodeCursor({ createdAt: last.created_at, id: last.id })
        : null,
    },
  });
}
```

MongoDB version of the same idea:

```js
const filter = cursor
  ? { $or: [
      { createdAt: { $lt: c.createdAt } },
      { createdAt: c.createdAt, _id: { $lt: c.id } },
    ] }
  : {};

const docs = await Post.find(filter).sort({ createdAt: -1, _id: -1 }).limit(limit + 1);
```

### Advantages

- **Consistent performance** at any depth (given the right index).
- **Stable**: inserts and deletes don't cause skipped or duplicated items.
- Ideal for **infinite scroll**, feeds, logs, and large tables.

### Trade-offs

- No "jump to page 50" and no total page count.
- The sort order must be **deterministic** — always add a unique tie-breaker (`id`) to the sort, or rows with equal values will be dropped or repeated.
- Changing the sort means different cursors; include the sort in the cursor or reject mismatches.

### Design notes

- Make cursors **opaque** (base64 of JSON) so clients don't build their own and you're free to change the internals.
- **Validate and ideally sign** cursors if they contain anything sensitive; treat decoded values as untrusted input and still use parameterized queries.
- Cursor pagination must be combined with `ORDER BY` on indexed columns — see `07-databases/postgresql/02-transactions-and-indexing.md` and `07-databases/mongodb/02-indexes-and-aggregation.md`.

---

## Choosing between them

| | Offset | Cursor |
|---|---|---|
| Jump to arbitrary page | ✅ | ❌ |
| Total count / page numbers | ✅ | ❌ (or expensive) |
| Performance on deep pages | ❌ degrades | ✅ constant |
| Stable under inserts/deletes | ❌ | ✅ |
| Implementation complexity | Low | Medium |
| Best for | Admin tables, small datasets | Feeds, large/fast-changing data, public APIs |

**Default recommendation:** cursor pagination for public or high-volume APIs; offset for internal dashboards with modest data.

---

## Pagination response conventions

Include enough metadata for clients to navigate without guessing:

```jsonc
{
  "data": [ /* items */ ],
  "meta": {
    "limit": 20,
    "hasMore": true,
    "nextCursor": "eyJjcmVhdGVkQXQiOi..."
  },
  "links": {
    "self": "/api/v1/posts?limit=20",
    "next": "/api/v1/posts?limit=20&cursor=eyJjcmVhdGVkQXQiOi..."
  }
}
```

Some APIs use the standard `Link` header (RFC 8288) instead:

```js
res.set("Link", `</api/v1/posts?cursor=${next}>; rel="next"`);
```

### Rules

- **Always enforce a max `limit`** (e.g. 100) and a sensible default (e.g. 20) — reject or clamp abusive values.
- **Validate** `page`, `limit`, and `cursor` (see `04-validation.md`) and return `400` on garbage.
- **Always specify an explicit sort order.** Without `ORDER BY`, databases return rows in arbitrary order and pagination is meaningless.
- **Avoid returning exact totals** on huge tables. Offer `hasMore`, or an approximate count, or make the count optional (`?includeTotal=true`).
- **Don't paginate in application memory** — fetch everything and `slice()` defeats the whole purpose.

### A reusable helper

```js
// utils/pagination.js
import { z } from "zod";

const paginationSchema = z.object({
  limit: z.coerce.number().int().min(1).max(100).default(20),
  cursor: z.string().optional(),
});

export function parsePagination(query) {
  return paginationSchema.parse(query);       // throws on invalid → 400/422 via error handler
}
```

## Next

**`03-filtering-and-sorting.md`** shows how to let clients narrow and order collections (`?status=published&sort=-createdAt`) without opening the door to injection or slow queries.
