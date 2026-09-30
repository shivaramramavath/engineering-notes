# Filtering & Sorting

Letting clients narrow and order a collection through the query string, without opening the door to injection, slow queries, or a chaotic API.

## The goal

```
GET /api/v1/posts?status=published&authorId=7&sort=-createdAt&limit=20
```

Give clients flexible access to data, with:

- a **consistent, documented syntax** across every collection endpoint,
- **allow-lists** for every field a client can filter or sort by,
- **indexes** that back the queries you permit,
- **validation** so bad input gets a clear `400`/`422`, not a 500.

This file builds on `02-versioning-and-pagination.md`: filtering and sorting decide *which* rows; pagination decides *how many at a time*.

---

## Query string basics

Everything in `req.query` arrives as a **string** (or an array or object of strings). Type conversion is your job.

```js
// GET /posts?published=true&page=2&tags=node&tags=api
req.query.published   // "true"   (a string, not a boolean)
req.query.page        // "2"      (a string, not a number)
req.query.tags        // ["node", "api"]   (repeating a key gives an array)
```

Another gotcha: Express's default query parser (`qs`) turns `?filter[status]=x` into a nested object — and in Express 5 the default is the simple parser, which doesn't. Whatever you use, **validate the shape** instead of trusting it (`04-validation.md`).

---

## Filtering conventions

### Simple equality

```
GET /posts?status=published
GET /posts?authorId=7&status=published        (multiple filters = AND)
```

### Multiple values (OR within a field)

```
GET /posts?status=draft,published             (comma-separated — common and compact)
GET /posts?status=draft&status=published      (repeated key — also valid)
```

### Comparison operators

Three popular syntaxes — choose one and use it everywhere:

```
# Bracket style (LHS brackets)
GET /posts?createdAt[gte]=2026-01-01&createdAt[lt]=2026-07-01
GET /products?price[gte]=1000&price[lte]=5000

# Suffix style
GET /posts?createdAt_gte=2026-01-01&createdAt_lt=2026-07-01

# Dedicated range params (simplest, often enough)
GET /posts?createdAfter=2026-01-01&createdBefore=2026-07-01
```

Typical operators:

| Operator | Meaning | SQL |
|---|---|---|
| `eq` (default) | equals | `=` |
| `ne` | not equal | `<>` |
| `gt` / `gte` | greater than (or equal) | `>` / `>=` |
| `lt` / `lte` | less than (or equal) | `<` / `<=` |
| `in` | one of a list | `IN (...)` |

### Text search

```
GET /posts?q=express+middleware               (free-text search)
GET /posts?title[contains]=express            (field-specific)
```

For anything beyond trivial, don't build search from `LIKE '%term%'` — leading wildcards can't use an ordinary index and become full table scans. Use PostgreSQL full-text search (`tsvector`), MongoDB text indexes, or a search engine (Elasticsearch/OpenSearch, Meilisearch) and expose it through `q`.

### Keep it simple

Every operator you support is something you must implement, document, index, test, and maintain forever. Start with equality, date ranges, and a search `q`. Add more only when a client has a real need.

---

## Sorting conventions

```
GET /posts?sort=createdAt            ascending
GET /posts?sort=-createdAt           descending (leading minus)
GET /posts?sort=-createdAt,title     multiple: newest first, then title A→Z
```

The leading `-` convention is compact and widely used (JSON:API uses it). The alternative is two parameters: `?sortBy=createdAt&order=desc`.

**Always define a default sort** (e.g. `-createdAt`) and **always add a unique tie-breaker** (`id`) at the end, or pagination will return inconsistent results for rows with equal sort values.

---

## Implementation: allow-lists are everything

The dangerous version:

```js
// ❌ passes arbitrary client input straight into the query
app.get("/posts", async (req, res) => {
  const posts = await Post.find(req.query).sort(req.query.sort);
  res.json(posts);
});
```

Problems:

- **NoSQL injection:** `?authorId[$ne]=x` becomes `{ authorId: { $ne: "x" } }`; and `$where` or `$regex` payloads can run expensive or malicious operations (`08-authentication-security/05-common-vulnerabilities.md`).
- **Data leakage:** `?passwordHash=...` or `?isAdmin=true` lets clients probe internal fields.
- **Performance:** any field can be filtered/sorted on, including unindexed ones — slow queries become a denial-of-service vector.

### The safe approach: a declarative config per resource

```js
// schemas/posts.query.js
import { z } from "zod";

export const postListQuery = z.object({
  status: z
    .string()
    .transform((s) => s.split(","))
    .pipe(z.array(z.enum(["draft", "published", "archived"])))
    .optional(),
  authorId: z.string().uuid().optional(),
  createdAfter: z.coerce.date().optional(),
  createdBefore: z.coerce.date().optional(),
  q: z.string().trim().min(2).max(100).optional(),
  sort: z.string().default("-createdAt"),
  limit: z.coerce.number().int().min(1).max(100).default(20),
  cursor: z.string().optional(),
});

// fields that are safe and indexed
export const SORTABLE_FIELDS = new Set(["createdAt", "title", "updatedAt"]);
```

```js
// utils/query.js
import { AppError } from "./AppError.js";

// "-createdAt,title" → [{ field: "createdAt", dir: -1 }, { field: "title", dir: 1 }]
export function parseSort(sortParam, allowed) {
  return sortParam.split(",").filter(Boolean).map((part) => {
    const dir = part.startsWith("-") ? -1 : 1;
    const field = part.replace(/^[-+]/, "");

    if (!allowed.has(field)) {
      throw new AppError(400, "invalid_sort", `Cannot sort by "${field}"`, {
        allowed: [...allowed],
      });
    }
    return { field, dir };
  });
}
```

```js
// controllers/posts.js (MongoDB / Mongoose)
export async function list(req, res) {
  const q = postListQuery.parse(req.query);      // validated + typed
  const sort = parseSort(q.sort, SORTABLE_FIELDS);

  const filter = {};
  if (q.status) filter.status = { $in: q.status };
  if (q.authorId) filter.authorId = q.authorId;
  if (q.createdAfter || q.createdBefore) {
    filter.createdAt = {};
    if (q.createdAfter) filter.createdAt.$gte = q.createdAfter;
    if (q.createdBefore) filter.createdAt.$lt = q.createdBefore;
  }
  if (q.q) filter.$text = { $search: q.q };

  const sortSpec = Object.fromEntries(sort.map((s) => [s.field, s.dir]));
  sortSpec._id = -1;                              // deterministic tie-breaker

  const items = await Post.find(filter).sort(sortSpec).limit(q.limit + 1);
  // ... cursor pagination response (see 02-versioning-and-pagination.md)
}
```

Key principles:

1. **Build the query object yourself** from validated values. Never spread `req.query` into the database call.
2. **Allow-list** filterable and sortable fields, and operators.
3. **Convert types** (dates, numbers, booleans) at the edge.
4. **Return a helpful 400** naming the allowed fields when a client asks for something unsupported.

---

## SQL version: dynamic queries without injection

Values are **always** parameters. Column names can't be parameters, so they come from an allow-list map:

```js
const SORT_COLUMNS = {
  createdAt: "created_at",
  title: "title",
  updatedAt: "updated_at",
};

export async function listPosts(q) {
  const where = [];
  const params = [];

  if (q.status) {
    params.push(q.status);
    where.push(`status = ANY($${params.length})`);
  }
  if (q.authorId) {
    params.push(q.authorId);
    where.push(`author_id = $${params.length}`);
  }
  if (q.createdAfter) {
    params.push(q.createdAfter);
    where.push(`created_at >= $${params.length}`);
  }
  if (q.q) {
    params.push(q.q);
    where.push(`search_vector @@ plainto_tsquery('english', $${params.length})`);
  }

  // column names come from OUR map, never from user input
  const orderBy = q.sort
    .map(({ field, dir }) => `${SORT_COLUMNS[field]} ${dir === -1 ? "DESC" : "ASC"}`)
    .concat("id DESC")
    .join(", ");

  params.push(q.limit + 1);

  const sql = `
    SELECT * FROM posts
    ${where.length ? "WHERE " + where.join(" AND ") : ""}
    ORDER BY ${orderBy}
    LIMIT $${params.length}
  `;
  return pool.query(sql, params);
}
```

Only **values** are interpolated via `$1, $2, ...` placeholders. The only text concatenated into the SQL comes from your own constant objects. For more complex cases, use a query builder (Knex, Kysely, Drizzle) or an ORM rather than hand-building strings — but the allow-list principle still applies.

---

## Performance: indexes must match what you allow

Every filter or sort you permit must be backed by an index, or one request can trigger a full table scan.

```sql
-- supports: WHERE status = ? ORDER BY created_at DESC, id DESC
CREATE INDEX idx_posts_status_created ON posts (status, created_at DESC, id DESC);

-- supports: WHERE author_id = ? ORDER BY created_at DESC
CREATE INDEX idx_posts_author_created ON posts (author_id, created_at DESC);
```

```js
// MongoDB
postSchema.index({ status: 1, createdAt: -1, _id: -1 });
postSchema.index({ authorId: 1, createdAt: -1 });
```

Guidelines:

- **Equality filters first, sort field last** in a composite index.
- Check actual plans with `EXPLAIN ANALYZE` (PostgreSQL) or `.explain("executionStats")` (MongoDB).
- **Limit what combinations you support.** If clients can combine 8 filters and 5 sort fields arbitrarily, you can't index for all of them. Support the combinations that matter.
- **Cap everything:** max `limit`, max number of values in an `in` list, max search string length.

Details in `07-databases/postgresql/02-transactions-and-indexing.md`, `07-databases/mongodb/02-indexes-and-aggregation.md`, and `15-performance/03-database-optimization.md`.

---

## Field selection and expansion (optional extras)

### Sparse fieldsets: return only what's needed

```
GET /posts?fields=id,title,createdAt
```

```js
const FIELDS = new Set(["id", "title", "body", "status", "createdAt", "updatedAt"]);

function parseFields(param) {
  if (!param) return null;                             // null = default full shape
  const requested = param.split(",");
  const invalid = requested.filter((f) => !FIELDS.has(f));
  if (invalid.length) {
    throw new AppError(400, "invalid_fields", `Unknown fields: ${invalid.join(", ")}`);
  }
  return requested;
}
```

Useful for mobile clients and large resources. Again: **allow-list**, so clients can't request `passwordHash`.

### Expanding relations

```
GET /posts?include=author,comments
```

Whitelist the allowed `include` values and watch for **N+1 queries** and unbounded payloads — cap the depth and size of what can be included.

---

## Documenting it

Clients can't use what they can't discover. For each collection endpoint, document:

| Item | Example |
|---|---|
| Filterable fields and operators | `status` (eq, in), `createdAt` (gte, lt), `authorId` (eq) |
| Sortable fields | `createdAt`, `title`, `updatedAt` |
| Default sort | `-createdAt` |
| Default and max limit | 20 / 100 |
| Search behavior | `q` searches `title` and `body` |
| Error when invalid | `400` with `invalid_sort` / `invalid_filter` codes |

An OpenAPI spec is the natural home for this, and the same zod schemas can generate it.

---

## Common mistakes

```js
// ❌ spreading raw query into the DB call (injection + leakage + slow queries)
Post.find({ ...req.query });

// ❌ sort field interpolated into SQL
`ORDER BY ${req.query.sort}`

// ❌ no default sort or tie-breaker → unstable pagination
Post.find(filter).limit(20);

// ❌ filtering on unindexed fields → one request = full scan
// ❌ unbounded `in` lists or search strings
// ❌ different syntaxes on different endpoints (?sort=-date here, ?orderBy=date:desc there)
// ❌ silently ignoring unknown params — the client thinks the filter worked and gets wrong data
```

The last one matters: prefer **rejecting unknown filter and sort parameters with a 400**, so a typo like `?statsu=published` doesn't quietly return everything.

```js
const schema = postListQuery.strict();     // zod: unknown keys are an error
```

## Next

**`04-validation.md`** goes deep on the validation layer used throughout this file: schemas for bodies, params, and query strings, a reusable middleware, and clean error output.
