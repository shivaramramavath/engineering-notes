# Pagination

Any endpoint that returns a collection will eventually return too many items. Pagination bounds response size, protects your database, and gives clients a way to walk through results. The two designs, **offset** and **cursor**, have very different behavior under load and under change, so choose deliberately and decide **before** the first client integrates.

Prerequisites: [REST and resource design](./01-rest-and-resource-design.md), [indexing and query basics](../02-database-foundations/06-indexing-and-query-basics.md) (why deep offsets are slow).

## Always paginate lists

An unbounded `GET /orders` works in development and takes down production once the table grows. Every collection endpoint needs:

- a **default** page size,
- a **maximum** page size (enforced server-side),
- a **stable, deterministic ordering**.

## Offset (page/limit) pagination

```text
GET /orders?page=3&limit=20
GET /orders?offset=40&limit=20          ← same idea, different parameters
```

```sql
SELECT * FROM orders ORDER BY created_at DESC, id DESC LIMIT 20 OFFSET 40;
```

| Pros | Cons |
|------|------|
| Simple to implement and understand | Slow on deep pages: the database walks and discards `OFFSET` rows |
| Supports jumping to arbitrary pages ("page 7 of 30") | **Unstable under change:** inserts/deletes between requests cause duplicates or skipped items |
| Easy to show total counts | `COUNT(*)` on large tables is expensive |

Fine for **small or admin datasets**, back-office tables, and when users truly need "go to page N".

## Cursor (keyset) pagination

The client passes an opaque **cursor** pointing at where the previous page ended; the server seeks directly from there.

```text
GET /orders?limit=20
→ { "items": [...], "nextCursor": "eyJjIjoiMjAyNS0wMS0xNVQxMDozMDowMFoiLCJpIjoiNDIifQ" }

GET /orders?limit=20&cursor=eyJjIjoi...
```

```sql
SELECT * FROM orders
WHERE (created_at, id) < ($1, $2)          -- values decoded from the cursor
ORDER BY created_at DESC, id DESC
LIMIT 21;                                   -- fetch limit+1 to know if there's a next page
```

| Pros | Cons |
|------|------|
| **Constant-time** regardless of depth (with a proper index) | No jumping to arbitrary pages |
| **Stable** while data changes (no duplicates/skips for items before the cursor) | Needs a unique, indexed sort key (with a tiebreaker) |
| Scales to huge datasets and infinite scroll | Total counts aren't free; changing sort order means a different cursor format |

Best for **feeds, large collections, public APIs, and anything append-heavy**.

### Choosing

| Need | Choose |
|------|--------|
| Small dataset, admin UI with page numbers | Offset |
| Large/growing data, infinite scroll, API for third parties | **Cursor** |
| Both | Offer cursor as the default; offset only where it's cheap |

## Implementing in Nest

### Query DTO (validate and bound the inputs)

```ts
// common/dto/pagination-query.dto.ts
import { Type } from 'class-transformer';
import { IsInt, IsOptional, Max, Min } from 'class-validator';

export class PaginationQueryDto {
  @IsOptional() @Type(() => Number) @IsInt() @Min(1)
  page: number = 1;

  @IsOptional() @Type(() => Number) @IsInt() @Min(1) @Max(100)     // enforce a maximum page size
  limit: number = 20;
}
```

Query values are strings, so `@Type(() => Number)` (or `transform` with implicit conversion) is required ([class-transformer](../../03-core-concepts/02-validation-and-serialization/04-class-transformer.md)). The `@Max` is what stops `?limit=1000000`.

### Offset response

```ts
async list({ page, limit }: PaginationQueryDto) {
  const [items, total] = await this.repo.findAndCount({
    order: { createdAt: 'DESC', id: 'DESC' },      // deterministic: always include a unique tiebreaker
    take: limit,
    skip: (page - 1) * limit,
  });
  return {
    items,
    meta: { page, limit, total, totalPages: Math.ceil(total / limit) },
  };
}
```

Same shape with Prisma (`findMany` + `count` in a [`$transaction`](../04-prisma/07-transactions.md)) or Mongoose (`find().skip().limit()` + `countDocuments`).

### Cursor implementation

Encode the sort key(s) into an **opaque** token (base64url of JSON is common) so clients don't depend on its structure:

```ts
const encode = (v: { c: string; i: string }) => Buffer.from(JSON.stringify(v)).toString('base64url');
const decode = (cursor: string) => {
  try {
    return JSON.parse(Buffer.from(cursor, 'base64url').toString());
  } catch {
    throw new BadRequestException('Invalid cursor');
  }
};

async list(limit: number, cursor?: string) {
  const after = cursor ? decode(cursor) : null;

  const qb = this.repo.createQueryBuilder('o')
    .orderBy('o.createdAt', 'DESC').addOrderBy('o.id', 'DESC')
    .take(limit + 1);                                           // one extra row to detect "has more"

  if (after) {
    qb.where('(o.createdAt, o.id) < (:createdAt, :id)', { createdAt: after.c, id: after.i });
  }

  const rows = await qb.getMany();
  const hasMore = rows.length > limit;
  const items = hasMore ? rows.slice(0, limit) : rows;
  const last = items[items.length - 1];

  return { items, nextCursor: hasMore && last ? encode({ c: last.createdAt.toISOString(), i: last.id }) : null };
}
```

Row-value comparison `(a, b) < (x, y)` works in PostgreSQL and MySQL; adapt for other databases (an equivalent `a < x OR (a = x AND b < y)` is portable). **Validate the decoded cursor's shape** (types, UUID/date formats) rather than trusting it; treat it as untrusted input, and never put anything sensitive in it. If you must prevent tampering, sign it (HMAC). With Prisma, use its native `cursor` + `skip: 1` + `take` ([Prisma Client](../04-prisma/03-prisma-client.md)).

**Index** the sort key(s): `(created_at DESC, id DESC)` makes keyset queries seek instead of scan ([indexing](../02-database-foundations/06-indexing-and-query-basics.md)).

## Ordering must be deterministic

If many rows share the same `created_at`, ordering by it alone is ambiguous, and pages can repeat or skip rows. **Always add a unique tiebreaker** (`id`) as the last sort key, in both offset and cursor pagination. Allow client-chosen sorting only on a **whitelist** of indexed fields ([filtering and sorting](./04-filtering-sorting-and-search.md)); a cursor is only valid for the sort order it was created under.

## Response shape

Return an **object**, not a bare array, so you can add metadata without breaking clients:

```json
// offset
{ "items": [ ... ], "meta": { "page": 3, "limit": 20, "total": 412, "totalPages": 21 } }

// cursor
{ "items": [ ... ], "nextCursor": "eyJ...", "hasMore": true }
```

Optionally add navigation links, in the body or as `Link` headers (RFC 8288):

```http
Link: </orders?cursor=eyJ...&limit=20>; rel="next"
```

Pick **one** convention per API and stay consistent across endpoints. Document it ([Swagger responses](../09-openapi-and-swagger/05-responses-and-examples.md)). A generic `Paginated<T>` type/DTO avoids repeating the shape (with Swagger, generic DTOs need a custom decorator to document the item type).

## Total counts: useful but not free

`COUNT(*)` over a large filtered table can be slower than the page query itself.

- Make totals **optional** (`?includeTotal=true`) or omit them for cursor APIs.
- Return an **approximate** count when exactness isn't needed.
- Cache counts for expensive, slow-changing queries.
- UIs that only need "next page" can use `hasMore` (the `limit + 1` trick) instead of a total.

## Edge cases

- **Empty results:** `200` with `items: []`, not `404`.
- **Out-of-range page** (offset): return an empty page (or `400` if you prefer strictness), but be consistent.
- **Invalid cursor:** `400` with a clear message.
- **Changing filters or sort mid-pagination:** a cursor is tied to the original query; clients must restart when they change filters.
- **Deleted/updated rows** between pages: cursors tolerate it; offsets may skip/duplicate.
- **Authorization:** apply per-user filters **in the query** so counts and pages never include data the caller can't see ([data-level authorization](../07-authorization/03-abac-and-policies.md)).
- **Rate limiting** scraping through deep pagination ([rate limiting](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)).

## Testing

- Create more rows than one page and walk all pages: collect ids and assert **no duplicates, no gaps**, correct order.
- Insert/delete rows between page requests (for cursor APIs) and assert stability.
- Boundary tests: `limit=1`, `limit=max`, `limit=max+1` (rejected or clamped, per your policy), rows with identical sort keys, empty result sets, invalid cursors.
- Check the query plan uses the index on large data ([indexing](../02-database-foundations/06-indexing-and-query-basics.md)); see [integration testing](../01-testing/05-integration-testing.md).

## Common mistakes

- **No maximum page size.**
- **Unstable ordering** (no tiebreaker, or no `ORDER BY` at all).
- **Offset pagination on huge or frequently changing tables.**
- **Cursor contents exposed as a public structure** (clients parse or craft them), or unvalidated cursors passed into queries.
- **Always computing totals**, making every list request slow.
- **Returning a bare array**, leaving no room for metadata.
- **Reusing a cursor after changing sort or filter.**
- **Index missing for the sort key.**
- **Inconsistent parameter names/shapes across endpoints** (`page`/`limit` here, `offset`/`size` there).
- **Filtering for authorization in memory** after fetching a page, producing short or leaky pages.

## Debugging

- Duplicates or missing rows between pages: non-deterministic ordering, or data changing under offset pagination. Add a unique tiebreaker or move to cursors.
- Deep pages very slow: offset cost; switch to keyset and check indexes with `EXPLAIN`.
- `limit` ignored or strings in arithmetic (`'2' * 20`): missing `@Type(() => Number)`/transform.
- Cursor "invalid" after a deploy: you changed the cursor format; version the format or tolerate old ones temporarily.
- Total count timing out: make it optional, approximate, or cached.

## Quick Summary

- Always paginate with a default and a **hard maximum** page size and a **deterministic order** (unique tiebreaker).
- **Offset** is simple and supports page jumping but is slow and unstable at scale; **cursor/keyset** is fast and stable but sequential-only.
- Validate query params with a DTO (`@Type(() => Number)`, `@Max`); treat cursors as opaque, validated, optionally signed input.
- Return an object (`items` + `meta`/`nextCursor`), use `limit + 1` for `hasMore`, and make totals optional.
- Index sort keys, apply authorization in the query, and test by walking all pages for duplicates and gaps.

## Next

[Filtering, sorting, and search →](./04-filtering-sorting-and-search.md)
