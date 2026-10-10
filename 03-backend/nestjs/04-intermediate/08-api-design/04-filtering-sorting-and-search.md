# Filtering, Sorting, and Search

List endpoints almost always grow query parameters: "only paid orders", "newest first", "matching 'laptop'". Done carelessly these become your biggest sources of **slow queries, injection bugs, and inconsistent APIs**. Done well, they're a small, predictable vocabulary backed by whitelists and indexes.

Prerequisites: [REST and resource design](./01-rest-and-resource-design.md), [pagination](./03-pagination.md), [ValidationPipe](../../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md).

## Conventions

```text
GET /orders?status=paid&customerId=42              filter: equality on fields
GET /orders?createdFrom=2025-01-01&createdTo=2025-01-31   filter: ranges
GET /orders?status=paid,shipped                    filter: any of several values (comma list)
GET /orders?sort=-createdAt,total                  sort: field list, "-" prefix = descending
GET /orders?q=laptop                               search: free text
GET /orders?fields=id,status,total                 sparse fieldsets (optional)
```

Rules of thumb:

- **Filter by resource attributes with query parameters**, not path segments (`/orders?status=paid`, not `/orders/paid`).
- Prefer **simple, flat, documented parameters** for common filters. Add structure (operators) only when you need it.
- **Name parameters consistently** across endpoints (`createdFrom`/`createdTo` everywhere, one sort syntax everywhere).
- Combine freely with [pagination](./03-pagination.md); a cursor is only valid for the filter and sort it was created with.
- **Unknown parameters:** ignore or reject (`forbidNonWhitelisted`). Rejecting catches typos (`?statuss=paid` silently returning everything); decide and document.

## Query DTO: validate, convert, whitelist

```ts
// orders/dto/list-orders-query.dto.ts
import { Transform, Type } from 'class-transformer';
import { IsArray, IsDate, IsEnum, IsIn, IsOptional, IsString, MaxLength } from 'class-validator';

const SORTABLE = ['createdAt', 'total', 'status'] as const;

export class ListOrdersQueryDto extends PaginationQueryDto {
  @IsOptional()
  @Transform(({ value }) => (typeof value === 'string' ? value.split(',') : value))
  @IsArray()
  @IsEnum(OrderStatus, { each: true })
  status?: OrderStatus[];                              // ?status=paid,shipped

  @IsOptional() @Type(() => Date) @IsDate()
  createdFrom?: Date;

  @IsOptional() @Type(() => Date) @IsDate()
  createdTo?: Date;

  @IsOptional() @IsString() @MaxLength(100)
  q?: string;

  @IsOptional()
  @Transform(({ value }) => (typeof value === 'string' ? value.split(',') : value))
  @IsArray()
  @IsIn([...SORTABLE, ...SORTABLE.map((f) => `-${f}`)], { each: true })    // whitelist, both directions
  sort?: string[] = ['-createdAt'];
}
```

```ts
@Get()
list(@Query() query: ListOrdersQueryDto, @CurrentUser() user: AuthUser) {
  return this.orders.list(query, user);
}
```

Important details:

- Query values are **strings**: convert with `@Type(() => Number/Date)` and `@Transform` ([class-transformer](../../03-core-concepts/02-validation-and-serialization/04-class-transformer.md)). Guard transforms on `typeof`; clients can send arrays (`?status=a&status=b`) or objects.
- **Whitelist** filterable and sortable fields in the DTO (`@IsIn`, `@IsEnum`) so clients can't reference arbitrary columns.
- Bound everything: list lengths (`@ArrayMaxSize`), string lengths (`@MaxLength`), date ranges. Each filter is a way to make your database work.
- Express 5 (the default in **NestJS 11**) changed the default query-string parser to `simple`, so nested bracket syntax like `?filter[status]=paid` is **not** parsed into objects unless you opt back in (`app.set('query parser', 'extended')` on Express). Flat parameters work everywhere; if you rely on nested syntax, configure and test the parser explicitly, and check the migration notes for your version.

## Applying filters safely

### Build conditions from the validated DTO, never from raw input

```ts
// TypeORM QueryBuilder
const qb = this.repo.createQueryBuilder('o').where('o.userId = :userId', { userId: user.id });   // authorization scope FIRST

if (q.status?.length) qb.andWhere('o.status IN (:...status)', { status: q.status });
if (q.createdFrom) qb.andWhere('o.createdAt >= :from', { from: q.createdFrom });
if (q.createdTo) qb.andWhere('o.createdAt < :to', { to: q.createdTo });
```

```ts
// Prisma
const where: Prisma.OrderWhereInput = {
  userId: user.id,                                            // authorization scope FIRST
  ...(q.status?.length ? { status: { in: q.status } } : {}),
  ...(q.createdFrom || q.createdTo
    ? { createdAt: { gte: q.createdFrom, lt: q.createdTo } }  // undefined bounds are ignored, which is intended here
    : {}),
};
```

Rules:

- **Authorization/tenant scoping is applied unconditionally**, then user-provided filters narrow further. A filter must never be able to widen what the caller may see ([data-level authorization](../07-authorization/03-abac-and-policies.md)).
- **Values go in parameters**; never interpolate into SQL strings ([QueryBuilder safety](../03-typeorm/05-query-builder.md)).
- **`undefined` filters mean "no filter"** in TypeORM and Prisma: fine when intentional (optional parameters), dangerous when a value that should be present is missing ([the undefined trap](../03-typeorm/04-repositories.md)).
- For MongoDB, coerce user values to the expected type (`String(...)`) so operator objects (`{"$ne": null}`) can't slip in ([NoSQL injection](../05-mongoose/03-repositories.md)).

### Sorting: whitelist and map

Column and direction **cannot be parameterized**, so injection risk is real if you interpolate them.

```ts
const SORT_COLUMNS: Record<string, string> = {
  createdAt: 'o.createdAt',
  total: 'o.total',
  status: 'o.status',
};

for (const key of q.sort ?? ['-createdAt']) {
  const desc = key.startsWith('-');
  const column = SORT_COLUMNS[desc ? key.slice(1) : key];       // only mapped names pass
  if (column) qb.addOrderBy(column, desc ? 'DESC' : 'ASC');
}
qb.addOrderBy('o.id', 'DESC');                                  // unique tiebreaker for stable pagination
```

Always end with a **unique tiebreaker**, or [pages become unstable](./03-pagination.md). Sort only on **indexed** fields; sorting a big table by an unindexed column can be slow or hit memory limits ([indexing](../02-database-foundations/06-indexing-and-query-basics.md)).

## Operators and richer filters

When equality and ranges aren't enough, choose a convention and document it:

```text
?total[gte]=100&total[lte]=500             bracket operators (needs the extended query parser)
?total_gte=100&total_lte=500               flat suffixes (works with any parser)
?filter=status:paid,total:>100             compact DSL (you write the parser)
POST /orders/search  { "filter": {...} }   body-based search for complex queries
```

- **Flat parameters** (`createdFrom`/`createdTo`, `minTotal`) are simplest to validate and document, and work with any query parser.
- For **complex, deeply nested** filtering, a `POST /resource/search` endpoint with a JSON body avoids URL length limits and parsing quirks. It's not RESTfully "pure" but is a widespread, pragmatic choice (just don't treat it as cacheable `GET`).
- **Never** accept a client-supplied query language that maps straight onto your database (raw SQL fragments, Mongo query objects, ORM `where` objects). That's an injection and authorization bypass waiting to happen. Translate from a small, validated vocabulary.
- GraphQL-style flexibility is available via [GraphQL](../../05-advanced/05-graphql/README.md) if clients genuinely need to shape queries.

## Search

| Approach | Good for | Limits |
|----------|----------|--------|
| `ILIKE '%term%'` / regex | Small tables, admin tools | Can't use normal indexes with a leading wildcard; slow at scale |
| **Database full-text search** (PostgreSQL `tsvector`/GIN, MySQL FULLTEXT, Mongo text/Atlas Search) | Medium scale, stemming and ranking in the DB | Language handling and relevance tuning are limited |
| **Dedicated search engine** (Elasticsearch/OpenSearch, Meilisearch, Typesense) | Large scale, typo tolerance, facets, relevance tuning | Another system to run and keep in sync |

```ts
// escape user input before using it in LIKE patterns (so % and _ are literals)
const escapeLike = (s: string) => s.replace(/[\\%_]/g, '\\$&');
qb.andWhere('o.reference ILIKE :q', { q: `%${escapeLike(q.q)}%` });
```

Search guidance:

- Treat `q` as **untrusted**: bound its length, escape wildcards, never build regexes from it unescaped (regex DoS), and rate-limit expensive search endpoints ([rate limiting](../../07-production/01-security/04-rate-limiting-and-brute-force-protection.md)).
- Require a **minimum length** (for example 2-3 characters) to avoid "match everything" queries.
- Decide which fields are searched and document it; searching many columns with `OR` and wildcards is expensive.
- When using a separate search engine, **filter authorization inside the search query too**, since results from the engine bypass your database-level scoping unless you replicate permissions ([full-text search](../../08-architecture-and-patterns/04-real-world-patterns/07-full-text-search.md)).

## Sparse fieldsets and includes (optional)

```text
GET /orders?fields=id,status,total
GET /orders/42?include=items,customer
```

They reduce payloads and over-fetching. Map them through a **whitelist** (to `select`/relations), never straight to column names, and watch authorization (a field list can't expose columns the caller may not see). They add combinatorial test surface; add them only if clients need them. Typed response DTOs are usually enough.

## Performance and cost control

- **Index** the fields you allow filtering and sorting on, in combinations that match real queries (equality first, then sort, then range) ([indexing](../02-database-foundations/06-indexing-and-query-basics.md)).
- Whitelisting filters is also **capacity planning**: every filterable/sortable field is a commitment to keep fast.
- **Cap** list sizes, date ranges, and `IN` list lengths; reject absurd combinations rather than letting them run.
- Add **statement timeouts** so one bad query can't hold connections ([connections](../02-database-foundations/02-database-connection.md)).
- **Cache** hot, repeated queries cautiously, keyed on the normalized parameters and the user's scope ([caching](../../05-advanced/01-caching/01-caching-fundamentals.md)).
- Normalize parameters (sort order, list order, defaults) before using them in cache keys or logs.

## Documenting it

List every supported filter, its type, allowed values, and defaults in OpenAPI so clients (and generated SDKs) get it right: `@ApiQuery` or, better, an annotated query DTO ([documenting endpoints](../09-openapi-and-swagger/02-documenting-endpoints.md)). Include the **sort syntax**, **max page size**, and the **semantics of combining filters** (AND across different parameters, OR within a comma list).

## Testing

- Each filter alone and in combination; empty results; boundaries (inclusive/exclusive range ends).
- **Sort stability:** many rows with identical sort values still paginate without duplicates.
- Invalid values (`status=bogus`, `sort=password`, `limit=999999`) get `400`, not a database error.
- **Authorization:** a filter can't reveal another user's/tenant's records (for example `?userId=<someone else>` must not widen scope).
- Injection attempts in `q`/`sort` (`'; DROP TABLE`, `%`, regex metacharacters, `{"$ne":null}`) behave as literals or are rejected ([E2E testing](../01-testing/06-e2e-testing.md)).
- Query plans on realistic data volumes ([integration testing](../01-testing/05-integration-testing.md)).

## Common mistakes

- **Interpolating `sort`/field names** into queries (SQL injection through identifiers).
- **No whitelist**: clients sort/filter on any column, including unindexed or sensitive ones.
- **Forgetting the tiebreaker** in sorting, breaking pagination.
- **Filters that can widen authorization scope** (a client-supplied `userId`/`tenantId` overriding the caller's).
- **Not converting query strings** (`'20'`, `'true'`, dates) before using them.
- **Leading-wildcard `LIKE`/regex search** on big tables, and unescaped wildcards/regex from users.
- **Passing client JSON straight into ORM `where` objects** or Mongo queries.
- **Silent acceptance of unknown parameters**, hiding typos.
- **Relying on nested bracket syntax** without checking the Express 5 query parser behavior.
- **Unbounded `IN` lists, date ranges, or search terms.**

## Debugging

- Filter seems ignored: the parameter was misspelled and unknown params are ignored, or conversion produced `undefined` (a failed `@Transform`), which means "no filter".
- `?status=a&status=b` behaves differently from `?status=a,b`: the DTO must normalize string vs array input.
- Nested `filter[...]` not parsed on Nest 11: Express 5's `simple` query parser; switch to flat params or enable the extended parser deliberately.
- Slow list endpoint: log the generated SQL and run `EXPLAIN ANALYZE`; look for a missing index for the filter/sort combination.
- Totals or pages leak other tenants' data: authorization scope isn't applied in the base query.

## Quick Summary

- Use flat, consistent, documented query parameters; filters narrow, **authorization scope always applies first**.
- Validate and convert with a query DTO; **whitelist** filterable and sortable fields; bound list sizes and string lengths.
- Never interpolate identifiers (sort/field names) or accept raw query objects; map from a small vocabulary; always add a unique sort tiebreaker.
- Search: escape wildcards, require minimum length, and use full-text search or a dedicated engine at scale; rate-limit expensive searches.
- Index what you expose, cap costs, document the syntax in OpenAPI, and test combinations, boundaries, injection attempts, and authorization.

## Next

[Response and error format →](./05-response-and-error-format.md)
