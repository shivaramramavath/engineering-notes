# Partitioning and Filtering

## Concept
Two tools restrict which vectors a query searches:

- **Partitioning** (namespaces, collections, tenants): splits an index into separate, isolated groups. A query targets one group.
- **Metadata filtering**: a condition on record metadata applied during a query within a group.

Both answer "search only *these* records", but they behave very differently.

## Prerequisites
- [vector-db-concepts.md](vector-db-concepts.md)
- [Metadata](../03-data-ingestion/metadata.md)

## Namespaces (Partitions)
A namespace is a logical partition inside one index. Records in different namespaces never appear in each other's query results.

```ts
// Conceptual (Pinecone TS SDK shape; verify against your SDK version)
await index.upsert({ records, namespace: "tenant-acme" });
await index.query({ vector: q, topK: 5, namespace: "tenant-acme" });   // sees only acme
```

With LlamaIndex.TS, the namespace is a `PineconeVectorStore` option (default `""`):
```ts
const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME,
  namespace: `tenant-${tenantId}`,
});
```

Properties:
- Queries target **exactly one** namespace.
- Strong logical isolation between groups.
- Cheap to create; deleting a whole namespace removes all its data at once.
- Search only scans that partition, which helps speed.

Typical uses: one namespace per tenant, per user session, per environment, or per document collection.

## Metadata Filtering
A filter limits results by metadata fields during the query.

```ts
// Raw Pinecone SDK: Mongo-style operators
await index.query({
  vector: q,
  topK: 5,
  includeMetadata: true,
  filter: { department: { $eq: "HR" }, year: { $gte: 2024 } },
});

// LlamaIndex.TS: MetadataFilters (operator strings; full list not documented, verify)
const filters = new MetadataFilters({
  filters: [
    { key: "department", value: "HR", operator: "==" },
    { key: "year", value: 2024, operator: ">=" },
  ],
});
const retriever = vectorIndex.asRetriever({ similarityTopK: 5, filters });
```

Properties:
- Works across the whole index (or namespace).
- Flexible: combine conditions, ranges and sets.
- Filter syntax and supported operators differ by vendor.
- Fields should be planned at ingestion time.

## Namespace vs Filter

| | Namespace | Metadata filter |
|---|---|---|
| Granularity | Coarse (whole groups) | Fine (any field) |
| Isolation | Strong | Depends on your code applying it every time |
| Query scope | One namespace per query | Any combination of conditions |
| Bulk delete | Delete the namespace | Delete by filter (not supported on Pinecone serverless; verify) |
| Performance | Searches only the partition | Filters during or around the ANN search |
| Best for | Tenants, sessions, environments | Categories, dates, doc types, access tags |

**Rule of thumb:** use namespaces for *who owns the data* (tenant, session), and filters for *what kind of data it is* (type, date, department).

## How Filtering Interacts with ANN Search

| Approach | What happens | Risk |
|---|---|---|
| **Pre-filter** | Restrict to matching records, then search | Can be slower or less accurate with very selective filters on some indexes |
| **Post-filter** | Search top-k first, then drop non-matching | May return fewer than k results, even none |
| **Filtered ANN** (integrated) | Filter applied during graph traversal | Best behavior; depends on the engine |

Practical effect: with a very restrictive filter, you may get **fewer results than requested**. Do not assume you always get `topK` items. Handle short or empty result lists.

```ts
const nodes = await retriever.retrieve({ query });
if (nodes.length === 0) return "I could not find anything matching those filters.";
```

## Multi-Tenant Design

| Pattern | Isolation | Notes |
|---|---|---|
| Namespace per tenant | Strong | Simple; works well up to many tenants (check vendor limits) |
| Shared namespace + `tenantId` filter | Weaker | One missed filter leaks data across tenants |
| Index per tenant | Strongest | Highest cost and management overhead |

For most apps, **namespace per tenant** is the best default. If you must use a filter-based design, enforce the tenant filter in a single server-side code path that callers cannot bypass:

```ts
function retrieverFor(tenantId: string) {            // the only place retrievers are built
  const filters = new MetadataFilters({
    filters: [{ key: "tenantId", value: tenantId, operator: "==" }],
  });
  return index.asRetriever({ similarityTopK: 5, filters });
}
```
Take `tenantId` from the authenticated session, never from the request body. See `16-production/security.md`.

## Per-Session or Per-Chat Data
A common pattern for chat with uploaded files: one namespace per chat session, deleted when the session ends. See `12-chat/loading-data-in-chat.md` and `12-chat/unloading-data-in-chat.md`.

## Design Checklist
- Which field decides ownership? Make it a namespace (or an enforced filter).
- Which fields will users filter on? Add them as metadata now.
- Are values consistently typed (numbers as numbers, ISO dates)?
- What happens when a filter matches nothing?
- How will you delete a tenant's or session's data?

## Important Rules
- Namespaces are for isolation; filters are for narrowing.
- Never rely on the client to send the tenant filter. Apply it on the server.
- Inconsistent metadata types cause filters to silently match nothing.
- Always handle fewer-than-k results.

## Common Mistakes
- Using filters for tenant isolation, then forgetting one on a query path.
- Querying the wrong namespace and concluding "the data isn't indexed".
- Creating a namespace per tiny entity without checking vendor limits.
- Filtering on fields that were never added to metadata.

## Related / Next
- `05-pinecone/namespaces-and-filters.md`
- `07-retrievers/retrieval-basics.md`
- `07-retrievers/auto-retriever.md`
- `12-chat/chat-sessions.md`
