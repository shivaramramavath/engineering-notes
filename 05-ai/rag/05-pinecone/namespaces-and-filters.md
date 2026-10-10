# Namespaces and Metadata Filters

## Concept
Pinecone gives you two ways to scope a search: **namespaces** (hard partitions) and **metadata filters** (conditions on fields). Concepts and the trade-offs are in `04-vector-databases/partitioning-and-filtering.md`. This file is the Pinecone syntax and strategy.

## Prerequisites
- [query.md](query.md)
- `04-vector-databases/partitioning-and-filtering.md`

## Namespaces

```ts
// Write
await index.upsert({ records: vecs, namespace: "tenant-acme" });

// Read
await index.query({ vector: q, topK: 5, namespace: "tenant-acme" });

// Inspect
const stats = await index.describeIndexStats();
console.log(stats.namespaces);    // { "tenant-acme": { recordCount: 1200 }, ... }  (field name varies by version; verify)

// Remove all data in one
await index.deleteAll({ namespace: "tenant-acme" });   // older SDKs: index.namespace("tenant-acme").deleteAll()
```
Older SDKs scope by `index.namespace("tenant-acme")` on the handle instead of passing `namespace` per call. Verify against your installed version.

Rules:
- A namespace is created implicitly on first upsert.
- Each query targets **one** namespace. There is no cross-namespace query in a single call; to search several, query each and merge results.
- Omitting `namespace` uses the default namespace.
- There are limits on namespaces per index that depend on plan (verify before designing around very large counts).

## Namespace Strategy

| Use case | Namespace |
|---|---|
| Multi-tenant SaaS | `tenant-<id>` |
| Chat with uploaded files | `chat-<sessionId>` (delete at session end) |
| Environment separation inside one index | `dev`, `staging` (prefer separate indexes for prod) |
| Collections of documents | `kb-hr`, `kb-engineering` |
| Embedding-version experiments | `v1`, `v2` (same dimension only) |

Naming conventions:
- Lowercase, prefix by purpose, include a stable ID.
- Generate names in code, never from raw user input (avoid collisions and injection-style surprises).

## Metadata Filters

```ts
await index.query({
  vector: q,
  topK: 10,
  namespace: "tenant-acme",
  filter: {
    $and: [
      { department: { $eq: "HR" } },
      { year: { $gte: 2024 } },
      { docType: { $in: ["policy", "faq"] } },
    ],
  },
  includeMetadata: true,
});
```

Common operators (verify the full current list): `$eq`, `$ne`, `$gt`, `$gte`, `$lt`, `$lte`, `$in`, `$nin`, `$exists`, `$and`, `$or`.

### Type pitfalls
- `{ year: { $gte: 2024 } }` works only if `year` was stored as a **number**. If it was stored as the string `"2024"`, the filter silently matches nothing. In TypeScript, check that your ingestion code does not pass `String(year)` or read the year from a regex capture without `Number(...)`.
- Lists of strings in metadata match with `$in` against their elements (verify exact semantics for your use case).
- Nested metadata objects are not supported; flatten them.

### Filtering through LlamaIndex.TS
```ts
import { MetadataFilters } from "llamaindex"; // some doc pages import it from "@llamaindex/core/vector-store"; verify

const filters = new MetadataFilters({
  filters: [{ key: "department", value: "HR", operator: "==" }],
});
const retriever = liIndex.asRetriever({ similarityTopK: 5, filters });
```
LlamaIndex.TS translates these into Pinecone filter syntax. Operator strings such as `"=="` and `">="` are documented; the full list is not, so check operator support (verify) if you use less common ones.

## Choosing Between Them

| Need | Use |
|---|---|
| Tenant or session isolation, easy bulk delete | Namespace |
| Narrow by date, type, department, tags | Filter |
| Both | Namespace per tenant, filters for everything else |

## Security Note
A filter is only as safe as the code that applies it. For tenant isolation, derive the namespace on the **server** from the authenticated user, never from a request parameter. The Pinecone key must never reach the browser. See `16-production/security.md`.

## Important Rules
- Plan filter fields at ingestion; adding one later means re-upserting metadata.
- Use consistent types for each field across all records.
- Handle fewer-than-`topK` and zero results.
- Keep write and read namespaces identical.

## Common Mistakes
- Querying the default namespace after writing to a named one.
- String-versus-number mismatches in filters.
- Relying on client-supplied tenant IDs.
- Creating high-cardinality metadata (unique values per record) and filtering on it heavily; use IDs or namespaces for that.

## Related / Next
- [sparse-and-hybrid.md](sparse-and-hybrid.md)
- `07-retrievers/auto-retriever.md`
- `12-chat/loading-data-in-chat.md`
