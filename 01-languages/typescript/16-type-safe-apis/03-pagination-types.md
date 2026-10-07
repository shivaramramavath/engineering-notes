# Pagination Types

Any endpoint that returns a list eventually needs pagination. The request and response shapes look similar across endpoints, which makes them a good fit for **generic types** that you define once and reuse. The more important decision is which kind of pagination to use, because offset and cursor pagination behave very differently under load and under change.

**Prerequisites:**
- [Request and response types](./01-request-response-types.md)
- [Generic types](../06-generics/01-generic-types.md)
- [Iterators and generators](../12-async-and-iteration/04-iterators-and-generators.md) (for the "iterate every page" helper)

---

## Two styles

| | Offset (page-based) | Cursor (keyset) |
|---|---|---|
| Request | `page` and `pageSize` (or `offset` and `limit`) | `limit` and an opaque `cursor` |
| "Where am I?" | a number | a token pointing at the last item seen |
| Jump to page N | **yes** | no |
| Consistent while data changes | **no**: inserts and deletes shift pages, causing duplicates or skipped items | **yes**: continues from a specific item |
| Cost for deep pages | grows with the offset (the database skips rows) | stays flat (uses an index) |
| Total count | easy to include, but can be expensive | usually omitted |
| Good for | admin tables, small datasets, "page 3 of 12" UIs | feeds, infinite scroll, large or fast-changing data, APIs consumed by programs |

Rule of thumb: use **offset** when users need page numbers and the data is small and fairly static, and **cursor** for everything that scales or changes.

## Offset pagination types

```ts
interface OffsetPageQuery {
  page: number;        // 1-based
  pageSize: number;
}

interface OffsetPage<T> {
  items: T[];
  page: number;
  pageSize: number;
  total: number;       // total matching items
  totalPages: number;
}
```

A generic `OffsetPage<T>` works for every list endpoint:

```ts
type ListUsersResponse = OffsetPage<UserDto>;
type ListPostsResponse = OffsetPage<PostDto>;
```

Server side:

```ts
async function listUsers({ page, pageSize }: OffsetPageQuery): Promise<OffsetPage<UserDto>> {
  const offset = (page - 1) * pageSize;
  const [rows, total] = await Promise.all([
    db.users.findMany({ skip: offset, take: pageSize, orderBy: { id: "asc" } }),
    db.users.count(),
  ]);
  return {
    items: rows.map(toUserDto),
    page,
    pageSize,
    total,
    totalPages: Math.ceil(total / pageSize),
  };
}
```

Problems to know about:

- With an offset, the database often still **reads and discards** the skipped rows, so `page=10000` is slow.
- If a row is inserted or deleted between two requests, items **shift** across pages: one appears twice, another is skipped.
- `total` needs a separate `COUNT` query, which can be slow on large tables with filters. Consider omitting it, or returning an approximate count.

## Cursor pagination types

```ts
interface CursorPageQuery {
  limit: number;
  cursor?: string;     // absent for the first page
}

interface CursorPage<T> {
  items: T[];
  nextCursor: string | null;   // null means there are no more items
}
```

A client loops until `nextCursor` is `null`. The cursor is **opaque**: clients must treat it as a string to pass back, not parse it. That leaves you free to change what it encodes.

### How the server implements it (keyset)

Instead of "skip N rows", ask for rows **after** the last one seen, using the sort key:

```sql
-- sorted newest first; (created_at, id) is a unique, stable sort key
SELECT * FROM posts
WHERE (created_at, id) < ($1, $2)       -- values decoded from the cursor
ORDER BY created_at DESC, id DESC
LIMIT $3;                                -- limit + 1, to detect whether there is a next page
```

Fetch `limit + 1` rows. If you get `limit + 1`, there is another page: drop the extra row and build `nextCursor` from the last row you return.

Requirements:

- The sort must be **total**: add a unique tiebreaker such as `id` to the order, or rows with equal sort values will be skipped or repeated.
- The sort columns should be **indexed**.
- The cursor encodes the sort key of the last item. It is often base64-encoded JSON.

### Typed cursor encoding

Because the cursor comes back from the client, it is **untrusted input**. Decode and validate it:

```ts
import { z } from "zod";

const CursorSchema = z.object({
  createdAt: z.string().datetime(),
  id: z.string(),
});

type Cursor = z.output<typeof CursorSchema>;

function encodeCursor(cursor: Cursor): string {
  return Buffer.from(JSON.stringify(cursor)).toString("base64url");
}

function decodeCursor(raw: string): Cursor | null {
  try {
    const json = JSON.parse(Buffer.from(raw, "base64url").toString("utf8"));
    const parsed = CursorSchema.safeParse(json);
    return parsed.success ? parsed.data : null;
  } catch {
    return null;                       // malformed cursor -> treat as invalid input (400)
  }
}
```

(`Buffer` is Node's. In browsers use `btoa`/`atob` with care for non-ASCII.) A bad cursor should produce a `400` validation error, not a server error ([error response types](./04-error-response-types.md)). If the data must be tamper-proof, sign the cursor, or keep sensitive details out of it.

## A generic page type for both

You can unify them with one generic response, or keep two. A discriminated union is useful when an API supports both styles:

```ts
type Page<T> =
  | ({ kind: "offset" } & OffsetPage<T>)
  | ({ kind: "cursor" } & CursorPage<T>);
```

Most APIs pick one style per endpoint, so separate `OffsetPage<T>` and `CursorPage<T>` types are simpler.

### A reusable schema factory

When using schemas, build the page schema from the item schema:

```ts
import { z } from "zod";

const cursorPageSchema = <S extends z.ZodType>(item: S) =>
  z.object({
    items: z.array(item),
    nextCursor: z.string().nullable(),
  });

const UsersPage = cursorPageSchema(UserDtoSchema);
type UsersPage = z.output<typeof UsersPage>;      // { items: UserDto[]; nextCursor: string | null }
```

This is a generic function over schemas, the runtime counterpart to the generic type.

## Request parameters

Treat query parameters as untrusted strings, then validate and bound them:

```ts
const PageQuerySchema = z.object({
  limit: z.coerce.number().int().min(1).max(100).default(20),
  cursor: z.string().optional(),
});
```

- **Always set a maximum** for `limit` or `pageSize`, or a client can ask for a million rows.
- **Set a sensible default** so omitted values behave predictably.
- **Whitelist sort fields** with a union type (`sort: "createdAt" | "name"`) rather than passing a raw column name into a query ([input validation](../23-security/01-input-validation.md)).

Typed sorting and filtering:

```ts
type UserSort = "createdAt" | "name";

interface ListUsersQuery extends CursorPageQuery {
  sort?: UserSort;
  order?: "asc" | "desc";
  role?: "admin" | "member";
}
```

Keep the **same sort** for the whole traversal: the cursor is only valid for the sort it was created under. Tying the sort into the cursor, or rejecting a mismatch, avoids confusing results.

## Consuming pages in the client

An async generator turns "call until `nextCursor` is null" into a plain loop:

```ts
async function* iterateAll<T>(
  fetchPage: (cursor: string | undefined) => Promise<CursorPage<T>>,
): AsyncGenerator<T> {
  let cursor: string | undefined;
  do {
    const page = await fetchPage(cursor);
    yield* page.items;
    cursor = page.nextCursor ?? undefined;
  } while (cursor !== undefined);
}

for await (const user of iterateAll((c) => api.listUsers({ limit: 100, cursor: c }))) {
  console.log(user.email);
}
```

Pages are fetched lazily as the loop runs, and breaking out of the loop stops further requests ([iterators and generators](../12-async-and-iteration/04-iterators-and-generators.md)).

For UIs, data-fetching libraries provide infinite-query helpers that manage the list of pages and the "load more" state ([server state with TanStack Query](../19-react-and-frontend/07-server-state-tanstack-query.md)).

## Other conventions

- **`hasMore`** can be included next to `nextCursor` for convenience, but `nextCursor === null` already carries the information.
- **`Link` headers** (`rel="next"`) are an HTTP-native way to expose the next page URL. They are less convenient to type than a body field.
- **Previous cursors** (`prevCursor`) support bidirectional navigation and need keyset queries in both directions.
- **Totals in cursor APIs** are sometimes offered as a separate endpoint or an optional, approximate field.
- Document the page size limits and ordering guarantees as part of the [contract](./00-api-contracts.md).

## Important rules and misconceptions

- **Offset pagination is not wrong,** just limited. For small, stable data it is the simplest choice.
- **A cursor is not an id.** It encodes a position in a particular ordering.
- **Pagination without a stable order is meaningless.** Always order explicitly, with a unique tiebreaker.
- **`total` is not free.** Counting can cost more than fetching the page.
- **Clients must treat cursors as opaque.** Parsing them couples them to your internals.
- **Page size limits belong on the server,** whatever the client asks for.

## Common mistakes

- Offset pagination over a large, frequently changing table.
- Ordering by a non-unique column, so keyset pagination skips or repeats rows.
- Not capping `limit` / `pageSize`.
- Trusting a decoded cursor without validating it.
- Passing a client-provided sort field straight into SQL.
- Returning `total` on every request when nobody uses it.
- Changing the sort between pages of one traversal.
- Using `nextCursor: undefined` to mean "no more", which JSON drops, instead of `null`.

## Debugging

- **Duplicates or missing items across pages:** the sort is not total, or you are using offset on changing data.
- **Slow deep pages:** switch to keyset, and check the index matches the sort columns.
- **Infinite loop in a client:** the server returns a non-null `nextCursor` on the last page, or ignores the cursor. Check the `limit + 1` logic.
- **400 on a valid-looking cursor:** the cursor was created under a different sort, or has been URL-mangled (base64url avoids `+`, `/`, and `=`).
- Log the SQL and parameters for a failing page request.

## Quick summary

- Choose **offset** for small, stable data with page numbers and **cursor** for large or changing data.
- Define generic `OffsetPage<T>` and `CursorPage<T>` types, and a schema factory for the runtime side.
- Cursor pagination uses a total sort order (add a unique tiebreaker), fetches `limit + 1`, and returns `nextCursor: string | null`.
- Cursors are opaque to clients and untrusted on the server: validate them.
- Cap page sizes, whitelist sort fields, and consume pages lazily with an async generator or a data-fetching library.

**Next:** [Error response types](./04-error-response-types.md)