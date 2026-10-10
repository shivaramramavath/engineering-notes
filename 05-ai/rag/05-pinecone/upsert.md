# Upserting Vectors

## Concept
**Upsert** inserts a record, or replaces it if the ID already exists. It is the only way to write vectors, so batching, IDs and metadata design all happen here.

## Prerequisites
- [indexes.md](indexes.md)
- `03-data-ingestion/metadata.md`

## Record Shape
```ts
import type { PineconeRecord } from "@pinecone-database/pinecone";

const record: PineconeRecord = {
  id: "handbook.pdf#0042",
  values: [0.12, -0.48 /* ... */],     // length === index dimension
  metadata: {
    docId: "handbook.pdf",
    page: 7,
    text: "Employees accrue ...",     // chunk text for retrieval
  },
};
```
`PineconeRecord` can also be generic over your metadata type (verify). A record may carry `sparseValues` too; see [sparse-and-hybrid.md](sparse-and-hybrid.md).

## Basic Upsert
```ts
await index.upsert({
  records: [
    { id: "a#0", values: vec0, metadata: { docId: "a" } },
    { id: "a#1", values: vec1, metadata: { docId: "a" } },
  ],
  namespace: "tenant-acme",
});
```
- Omit `namespace` to use the default namespace. A namespace is created automatically on first write.
- Re-upserting an existing ID **overwrites** the whole record, including metadata.

> **SDK version difference (verify against your installed version).** The newer SDK takes an options object: `index.upsert({ records, namespace })`. Older SDKs (roughly v5 and below) took an array and scoped the namespace on the handle: `index.namespace("tenant-acme").upsert([...records])`. Many tutorials show the older form. Both appear in the wild; check the type signatures in your `node_modules` or the docs for your version. The same applies to `query`, `fetch`, `update` and `deleteMany` in the other files of this chapter.

## Batching
Never upsert one vector per request. Send batches.

```ts
function* batches<T>(items: T[], size = 100): Generator<T[]> {
  for (let i = 0; i < items.length; i += size) {
    yield items.slice(i, i + size);
  }
}

for (const batch of batches(records, 100)) {
  await index.upsert({ records: batch, namespace: "tenant-acme" });
}
```

Limits to respect (verify current numbers in the Pinecone docs):
- A cap on **records per request** (on the order of 1,000) and on **request size** (on the order of 2 MB).
- A cap on **metadata size per record** (on the order of tens of KB; commonly cited as 40 KB).
- Smaller batches (around 100) are the usual practical choice for high-dimension vectors, since request size grows with dimension.

Throughput options:
- Parallel batch upserts with bounded concurrency (for example `Promise.all` over small groups of batches; do not fire thousands of requests at once).
- For very large loads, Pinecone offers bulk import from object storage (verify availability, plan, and TypeScript SDK support).

```ts
async function upsertParallel(records: PineconeRecord[], namespace: string, concurrency = 4) {
  const all = [...batches(records, 100)];
  for (let i = 0; i < all.length; i += concurrency) {
    await Promise.all(
      all.slice(i, i + concurrency).map((batch) => index.upsert({ records: batch, namespace })),
    );
  }
}
```

## Retries
```ts
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

async function upsertWithRetry(batch: PineconeRecord[], namespace: string, tries = 5) {
  for (let attempt = 0; attempt < tries; attempt++) {
    try {
      return await index.upsert({ records: batch, namespace });
    } catch (err) {
      if (attempt === tries - 1) throw err;
      await sleep(2 ** attempt * 1000);
    }
  }
}
```
In production code, inspect the error (rate limit and 5xx are retryable; a dimension mismatch or bad request is not) rather than retrying everything. See `16-production/rate-limits-and-retries.md`.

## ID Design
IDs decide whether you can update and delete later.

| Pattern | Example | Benefit |
|---|---|---|
| `<docId>#<chunkNo>` | `handbook.pdf#0042` | List and delete all chunks of one document by prefix |
| Content hash | `sha1(text)` | Natural dedup of identical chunks |
| Random UUID | `crypto.randomUUID()` | Convenient but cannot update or delete by document |

Prefer `<docId>#<chunkNo>`. It makes document-level updates practical (see [delete-and-update.md](delete-and-update.md) and `11-document-management/`). LlamaIndex generates its own node IDs (`id_`); to control them, set document and node IDs explicitly in your pipeline.

## Metadata Choices
- Store a stable `docId` and any fields you will filter on.
- Store the chunk text in metadata if you want retrieval to return it without another lookup. LlamaIndex's Pinecone store does this by default (the `textKey` option, default `"text"`), which means chunk size interacts with the per-record metadata cap.
- Keep values short, flat and consistently typed.
- Supported value types are limited (strings, numbers, booleans, lists of strings); nested objects are not supported (verify).

## Freshness
Upserts are **eventually consistent**: a record may not show up in queries for a short moment.
```ts
await index.upsert({ records: batch, namespace: "t" });
await index.describeIndexStats();      // counts can lag briefly
```
In tests, poll for the record (fetch by ID) instead of querying instantly.

## With LlamaIndex.TS
```ts
import { VectorStoreIndex, Document } from "llamaindex";
import { PineconeVectorStore } from "@llamaindex/pinecone";

const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME!,
  namespace: "tenant-acme",
  chunkSize: 100, // upsert batch size (default 100)
});

const liIndex = await VectorStoreIndex.fromDocuments(documents, {
  storageContext: { vectorStore }, // docs also show StorageContext.fromDefaults({ vectorStore }); verify
});
```
`PineconeVectorStore` creates its own Pinecone client (it reads `PINECONE_API_KEY` unless you pass `apiKey`). The same batching and metadata rules apply underneath. For incremental loads, use `IngestionPipeline` with a docstore (see `03-data-ingestion/ingestion-pipeline.md`).

## Important Rules
- Vector length must equal the index dimension.
- Always batch and retry.
- Upsert replaces the full record. To change one metadata field, use `update`.
- Use deterministic IDs so reruns overwrite instead of duplicating.

## Common Mistakes
- Random IDs on every run, creating duplicates.
- Oversized batches that exceed request limits.
- Putting entire documents in metadata and exceeding the per-record cap.
- Querying instantly after upsert and concluding the write failed.
- Wrong namespace on write versus read.
- Copying the old `index.namespace("ns").upsert([...])` form into a newer SDK (or the reverse) and getting type errors.

## Related / Next
- [query.md](query.md)
- `11-document-management/insert-documents.md`
- `03-data-ingestion/ingestion-pipeline.md`
