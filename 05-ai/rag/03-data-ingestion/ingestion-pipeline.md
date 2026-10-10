# Ingestion Pipeline

## Concept
The **ingestion pipeline** chains every offline step (load, clean, chunk, extract metadata, embed, store) into one repeatable, restartable process. It should be safe to run again and again as your documents change.

```
Load ──► Clean ──► Chunk ──► (Extract metadata) ──► Embed ──► Upsert to vector store
```

> TypeScript edition. Import paths vary between LlamaIndex.TS versions; verify against your installed version.

## Prerequisites
- [loaders.md](loaders.md), [cleaning.md](cleaning.md), [chunking.md](chunking.md), [metadata.md](metadata.md)
- `02-embeddings/`

## LlamaIndex.TS `IngestionPipeline`
```ts
import { IngestionPipeline, SentenceSplitter } from "llamaindex";
import { OpenAIEmbedding } from "@llamaindex/openai";
import { PineconeVectorStore } from "@llamaindex/pinecone";

const embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });
const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME,
  namespace: "docs",
});

const pipeline = new IngestionPipeline({
  transformations: [
    new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 }),
    embedModel,                       // embedding is the last transformation
  ],
  vectorStore,
});

const nodes = await pipeline.run({ documents });
```
`pipeline.run` returns the nodes it processed and writes them to the vector store. You can also pass `{ nodes }` instead of `{ documents }`. Custom steps (cleaning, metadata extraction) implement `TransformComponent` and slot into `transformations`.

## Making It Incremental
Without extra setup, rerunning the pipeline re-embeds everything. Add a **docstore** to track what has already been processed.

```ts
import { DocStoreStrategy, IngestionPipeline, SentenceSplitter } from "llamaindex";
import { SimpleDocumentStore } from "llamaindex/storage";

const docStore = new SimpleDocumentStore();

const pipeline = new IngestionPipeline({
  transformations: [new SentenceSplitter({ chunkSize: 512 }), embedModel],
  docStore,
  vectorStore,
  docStoreStrategy: DocStoreStrategy.UPSERTS,
});

await pipeline.run({ documents });           // first run: ingests all
await pipeline.run({ documents });           // second run: skips unchanged documents
```

How it works: each document has an ID and a content hash. On a rerun:
- Same ID, same hash → skipped.
- Same ID, changed hash → old nodes replaced (upsert).
- New ID → ingested.

| Strategy | Behavior |
|---|---|
| `UPSERTS` | Add new, replace changed (the default) |
| `DUPLICATES_ONLY` | Skip documents already seen; no replacement |
| `UPSERTS_AND_DELETE` | Also remove documents missing from the input batch |
| `NONE` | No docstore-based change detection |

Be careful with `UPSERTS_AND_DELETE`: if your input batch is incomplete, it deletes data you meant to keep.

**Stable IDs are required.** Set `id_` on each `Document` yourself (for example from the file path; see [loaders.md](loaders.md)). Random IDs make every run look like new data.

`SimpleDocumentStore` is in memory. Persist it so state survives restarts:
```ts
docStore.persist("./pipeline_storage/docstore.json");                 // file name: verify
const restored = SimpleDocumentStore.fromPersistPath("./pipeline_storage/docstore.json");
```
For multi-process or production use, back the docstore with a database by implementing `BaseDocumentStore` / `BaseKVStore`; ready-made Postgres/Mongo/Redis docstores are mentioned only generically in the docs (verify). See `06-llamaindex/storage.md`.

## Caching Transformations
```ts
import { IngestionCache } from "llamaindex";

const pipeline = new IngestionPipeline({
  transformations: [/* ... */],
  cache: new IngestionCache("ingestion-cache"),   // name argument; verify
});
```
The cache is on by default and in memory by default. It is keyed by a hash of the input nodes plus the transformation config, so unchanged chunks aren't re-embedded or re-processed by an LLM extractor. Set `disableCache: true` on the pipeline to turn it off. An in-memory cache is lost on restart, so for repeated runs across processes you need a persisted cache (verify what your version supports).

## Parallelism
The Python framework offers `num_workers` for multiprocessing. I did not find an equivalent documented for LlamaIndex.TS. In Node, embedding is I/O-bound, so the practical lever is batching and bounded concurrency in your own code (for example processing documents in groups of N with `Promise.all`). Embedding calls are usually limited by API rate limits, so batch and back off (see `16-production/rate-limits-and-retries.md`).

## Pipeline Design Checklist
- [ ] Stable document IDs
- [ ] Deterministic cleaning (same input gives same text)
- [ ] Source and filter metadata attached
- [ ] Docstore for change detection
- [ ] Embedding model and chunking settings recorded
- [ ] Failures logged per document, not silently skipped
- [ ] Idempotent: running twice leaves the same final state
- [ ] A path to delete documents (see `11-document-management/`)

## Handling Failures
- Process in batches so one bad file does not stop everything.
- Log the document ID and error for each failure.
- Retry transient errors (network, rate limits); do not retry parse errors blindly.
- Keep a list of failed documents to reprocess.

```ts
const failed: { id: string; error: unknown }[] = [];
for (const batch of chunkArray(documents, 20)) {
  try {
    await pipeline.run({ documents: batch });
  } catch (error) {
    failed.push({ id: batch.map((d) => d.id_).join(","), error });
  }
}
```
(`chunkArray` is a small helper you write: split an array into groups of N.)

## Important Rules
- Use the same embedding model and settings for every document in an index.
- Version your pipeline config. Changing chunking or the model means re-ingesting.
- Keep raw documents so you can rebuild the index.
- Test on a small sample before ingesting the full corpus; embedding thousands of documents has a real cost.

## Common Mistakes
- No stable IDs, so reruns duplicate everything.
- Re-embedding the whole corpus for a one-file change.
- In-memory docstore lost on restart, so the next run duplicates data.
- Running `UPSERTS_AND_DELETE` on a partial document list.
- Ingesting everything before checking a sample for quality.

## Related / Next
- `05-pinecone/upsert.md`
- `11-document-management/refresh-and-sync.md`
- `16-production/architecture.md`
- `13-workflows/document-lifecycle.md`
