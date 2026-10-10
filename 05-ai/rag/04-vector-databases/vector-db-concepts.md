# Vector Database Concepts

## Concept
A **vector database** stores embedding vectors together with their metadata and answers one core question fast: *"which stored vectors are closest to this query vector?"*

It is the storage and search layer of a RAG system. This file covers vendor-independent concepts; Pinecone specifics are in `05-pinecone/`.

## Prerequisites
- [Embeddings basics](../02-embeddings/embeddings-basics.md)
- [Similarity metrics](../02-embeddings/similarity-metrics.md)

## What a Record Contains

```ts
interface VectorRecord {
  id: string;                              // unique key, e.g. "handbook.pdf-0042"
  values: number[];                        // the embedding vector, e.g. [0.12, -0.48, 0.91, ...]
  metadata: {                              // flat key/value pairs
    source: string;                        // "handbook.pdf"
    page: number;                          // 7
    tenantId: string;                      // "acme"
    text?: string;                         // the chunk text
  };
}
```

- **ID**: unique key, used for fetch, update and delete.
- **Vector**: must match the index dimension.
- **Metadata**: used for filtering, citations and tracing. Often also holds the chunk text, so retrieval can return it without a second lookup.

## Why Not a Normal Database?
Regular databases index exact values (`WHERE id = 5`). Similarity search over high-dimensional vectors has no such exact shortcut. Comparing the query to every stored vector (brute force) is accurate but slow at scale. Vector databases use specialized indexes.

## Exact vs Approximate Search

| | Exact (brute force / flat) | Approximate (ANN) |
|---|---|---|
| Accuracy | Perfect | Very high, not perfect |
| Speed at scale | Slow (scans everything) | Fast |
| Use when | Small data (thousands to low hundreds of thousands), or you need exact results | Large data, low latency needed |

**ANN** (approximate nearest neighbor) trades a small amount of recall for large speed and cost gains.

## Common Index Types

| Index | Idea | Trade-off |
|---|---|---|
| **Flat** | Compare against every vector | Exact, slow at scale |
| **HNSW** | Layered graph; navigate from coarse to fine | Very fast and accurate; high memory use |
| **IVF** | Cluster vectors; search only the nearest clusters | Less memory; accuracy depends on clusters searched |
| **PQ / quantization** | Compress vectors | Much smaller; some accuracy loss |

Managed services such as Pinecone choose and tune the index internally, so you rarely select these yourself. Self-hosted systems expose the settings.

## Core Operations

| Operation | Meaning |
|---|---|
| **Upsert** | Insert a record, or replace it if the ID exists |
| **Query** | Return the top-k most similar records to a query vector |
| **Fetch** | Get records by ID |
| **Delete** | Remove by ID, by filter, or by namespace/partition |
| **Update** | Change metadata or vector of an existing record |

```ts
// Conceptual pseudo-API (most vendors have this shape; names differ)
await index.upsert({ records: [{ id: "a1", values: vec, metadata: { source: "x.pdf" } }] });
const results = await index.query({ vector: queryVec, topK: 5, filter: { source: "x.pdf" } });
```
The real Pinecone TypeScript SDK signatures (and their version differences) are in `05-pinecone/`.

## Key Parameters
- **topK**: how many results to return.
- **metric**: cosine, dot product or Euclidean. Set at index creation.
- **dimension**: fixed at index creation.
- **filter**: metadata conditions applied during search.

## Consistency and Freshness
Many vector databases are **eventually consistent**: a just-upserted record may not appear in queries for a short time. Tests that upsert and immediately query can fail intermittently. Allow a brief delay or poll until the record is visible.

## Scale Considerations
- **Memory**: vectors dominate cost. Rough size is `number of vectors × dimension × 4 bytes` for 32-bit floats, before index overhead.
- **Dimension**: lower dimension means cheaper storage and faster search.
- **Metadata**: storing full chunk text in metadata increases storage and per-record limits.
- **Recall**: ANN indexes can miss true neighbors. Measure recall if it matters.

Example: 1 million vectors at 1536 dimensions is about 6 GB of raw vector data (1,000,000 × 1536 × 4 bytes), before overhead.

## Important Rules
- Dimension and metric are fixed per index; changing either means a new index.
- Vectors in one index must all come from the same embedding model.
- Store enough metadata to filter, cite and delete later.
- Treat query results as candidates, not truth; reranking can improve them (see `09-reranking/`).
- Keep vector database API keys on the server. The TypeScript SDKs are Node-oriented; if a browser app needs search, put a server endpoint in between.

## Common Mistakes
- Creating an index with the wrong dimension or metric and discovering it after ingesting.
- Assuming an upsert is instantly queryable.
- Storing everything (full documents) in metadata and hitting size limits.
- Using a vector database when a few thousand vectors in memory would do.

## Related / Next
- [partitioning-and-filtering.md](partitioning-and-filtering.md)
- [vector-db-comparison.md](vector-db-comparison.md)
- `05-pinecone/indexes.md`
