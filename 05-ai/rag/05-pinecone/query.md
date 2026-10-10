# Querying and Fetching

## Concept
- **Query**: find the top-k records most similar to a vector.
- **Fetch**: retrieve specific records by ID.

## Prerequisites
- [upsert.md](upsert.md)
- `02-embeddings/similarity-metrics.md`

Call shapes below use the newer options-object style. On older SDKs the namespace is set on the handle (`index.namespace("ns").query(...)`); verify against your installed version (see the note in [upsert.md](upsert.md)).

## Query by Vector
```ts
const res = await index.query({
  vector: queryVec,              // same embedding model as the stored vectors
  topK: 5,
  namespace: "tenant-acme",
  filter: { department: { $eq: "HR" } },
  includeMetadata: true,
  includeValues: false,          // vectors are large; usually leave off
});

for (const m of res.matches) {
  console.log(m.id, m.score, m.metadata?.source);
}
```

| Parameter | Notes |
|---|---|
| `vector` | Query embedding; dimension must match the index |
| `topK` | Number of results. A maximum applies (verify; the cap is lower when values or metadata are included) |
| `namespace` | Searches exactly one namespace |
| `filter` | Metadata filter; see [namespaces-and-filters.md](namespaces-and-filters.md) |
| `includeMetadata` | Needed to get chunk text and source back |
| `includeValues` | Returns stored vectors; adds payload size |

## Query by ID
Find records similar to an existing record:
```ts
await index.query({ id: "handbook.pdf#0042", topK: 5, namespace: "tenant-acme" });
```

## Reading Scores
- With `cosine`, higher is more similar.
- With `euclidean`, **lower** distance is more similar.
- Scores are relative to your model and data. Do not copy cutoffs from tutorials. Calibrate on your own queries (see `07-retrievers/retrieval-basics.md`).

## Fetch by ID
```ts
const res = await index.fetch({
  ids: ["handbook.pdf#0042", "handbook.pdf#0043"],
  namespace: "tenant-acme",
}); // older SDKs: index.namespace("tenant-acme").fetch([...]); verify
console.log(res.records["handbook.pdf#0042"]?.metadata); // property name `records` vs `vectors` varies by version; verify
```
Fetch is exact and cheap. Use it to verify that a record exists and to inspect what was stored when debugging.

## List IDs by Prefix
```ts
let paginationToken: string | undefined;
do {
  const page = await index.listPaginated({
    prefix: "handbook.pdf#",
    limit: 100,
    paginationToken,
    namespace: "tenant-acme",
  });
  console.log(page.vectors?.map((v) => v.id)); // a page of matching IDs
  paginationToken = page.pagination?.next;
} while (paginationToken);
```
Works well with the `<docId>#<chunkNo>` ID pattern. This is how you find every chunk of a document before an update or delete. (Verify: listing support and behavior on your index type, and the exact result shape in your version.)

## Query Text, Not Just Vectors
Pinecone queries take **vectors**. Embed the question first with the same model used at ingestion. LlamaIndex.TS does this automatically:

```ts
import { VectorStoreIndex } from "llamaindex";
import { PineconeVectorStore } from "@llamaindex/pinecone";

const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME!,
  namespace: "tenant-acme",
});
const liIndex = await VectorStoreIndex.fromVectorStore(vectorStore); // no re-embedding

const retriever = liIndex.asRetriever({ similarityTopK: 5 });
const nodes = await retriever.retrieve({ query: "What is the vacation policy?" }); // docs sometimes show a plain string; verify
```

## Handling Results
- A filter or an empty namespace can return **fewer than `topK`** matches, or none. Handle that.
- Matches come back in score order.
- Treat results as candidates; consider reranking (`09-reranking/`).

## Debug Checklist When Results Look Wrong
1. `describeIndexStats()`: does the namespace have vectors?
2. Same namespace for write and query?
3. Same embedding model (and prefixes) for documents and query?
4. Filter field names and types match stored metadata?
5. `fetch` a known ID: is its metadata what you expect?
6. Try the query with no filter, then add the filter back.
7. Recently upserted? Wait briefly and retry.

## Important Rules
- Embed queries with the exact model used for the documents.
- Query one namespace at a time.
- Request metadata only when needed; ask for values almost never.
- Do not assume `topK` results will always come back.

## Common Mistakes
- Embedding queries with a different model than documents.
- Wrong namespace, hence "empty" results.
- Asking for `includeValues: true` and paying for payload you never use.
- Treating a low score threshold from a blog post as universal.

## Related / Next
- [namespaces-and-filters.md](namespaces-and-filters.md)
- `07-retrievers/vector-retriever.md`
- `15-debugging/debugging-rag.md`
