# Indexes and Storage

An index is the structure LlamaIndex queries. `VectorStoreIndex` is the default and by far the most common: it stores each node's embedding and finds the nearest ones at query time. By default everything lives in memory and disappears when the process exits, so the practical questions are where to persist it and how to reload it without paying for embeddings again.

> Prerequisites: [01-setup-and-first-query](./01-setup-and-first-query.md), [02-documents-and-nodes](./02-documents-and-nodes.md).

## What is stored where

A `StorageContext` bundles the stores an index uses:

| Store | Holds |
|---|---|
| Vector store | Embeddings (and, for stores that support it, the node text and metadata) |
| Document store | The node/document objects |
| Index store | Index structure metadata |

With no configuration, all three are in-memory. `storageContextFromDefaults({ persistDir })` makes them file-backed in a local directory. You can also swap in an external vector store, which is the usual production choice.

Other index types exist (for example `SummaryIndex`), but vector search is what almost all RAG apps use, so this note focuses on it.

## Persisting to local disk

```ts
import {
  Document,
  VectorStoreIndex,
  storageContextFromDefaults,
} from "llamaindex";

const storageContext = await storageContextFromDefaults({
  persistDir: "./storage",
});

const index = await VectorStoreIndex.fromDocuments(
  [new Document({ text: "Test Text" })],
  { storageContext },
);
```

Per the docs, storage works automatically once the `StorageContext` is configured: building the index writes to `persistDir`.

### Load instead of rebuild

On the next start, point a storage context at the same directory and initialize the index from it:

```ts
import { existsSync } from "node:fs";

async function getIndex(docs: Document[]) {
  const storageContext = await storageContextFromDefaults({
    persistDir: "./storage",
  });

  if (existsSync("./storage")) {
    return VectorStoreIndex.init({ storageContext }); // reuses stored vectors
  }
  return VectorStoreIndex.fromDocuments(docs, { storageContext });
}
```

Check the signature against your installed version; `init({ storageContext })` is the load path used in the docs and maintainer examples, but option names have moved between releases.

Things to know:

- On a first run you may see a message like "No valid data found at path ... vector_store.json starting new store". That is informational, not an error.
- **Do not share one `persistDir` between different index types.** A maintainer-confirmed gotcha: use a separate directory per index, for example `./storage_vector` and `./storage_summary`.
- The default file-backed vector store is simple and good for prototypes and small corpora. It is not meant for large datasets or multi-process writers.

## External vector stores

For anything shared, large or long-lived, use a real vector database. Each is a separate package, for example `@llamaindex/postgres`, `@llamaindex/qdrant`, `@llamaindex/milvus`, `@llamaindex/chroma`, `@llamaindex/upstash`. The usage pattern is the same for all:

```ts
// build the index and write vectors into the store
const index = await VectorStoreIndex.fromDocuments(docs, {
  storageContext: { vectorStore },
});

// later (another process, another deploy): attach to what is already there
const index = await VectorStoreIndex.fromVectorStore(vectorStore);
```

`fromVectorStore` does **not** re-embed anything. It just wraps existing vectors. It still needs `Settings.embedModel` to embed incoming *queries*, and that must be the same model used at index time.

### pgvector (PostgreSQL)

Good fit if you already run Postgres: vectors, metadata and app data live together.

```bash
npm i @llamaindex/postgres pg pgvector
```

```sql
CREATE EXTENSION IF NOT EXISTS vector;
```

```ts
import { PGVectorStore } from "@llamaindex/postgres";
import { Settings, VectorStoreIndex } from "llamaindex";
import { OpenAIEmbedding } from "@llamaindex/openai";

Settings.embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });

const vectorStore = new PGVectorStore({
  clientConfig: {
    host: process.env.POSTGRES_HOST,
    port: 5432,
    database: "llamaindex",
    user: process.env.POSTGRES_USER,
    password: process.env.POSTGRES_PASSWORD,
  },
  dimensions: 1536, // must match the embedding model's output size
});

// first time: ingest
const index = await VectorStoreIndex.fromDocuments(docs, {
  storageContext: { vectorStore },
});

// afterwards: reuse
const sameIndex = await VectorStoreIndex.fromVectorStore(vectorStore);
```

Notes on `PGVectorStore`:

- **`dimensions` must equal the embedding model's vector size.** `text-embedding-3-small` produces 1536 by default; if you pass a smaller `dimensions` to the embedding model, pass the same value here.
- Constructor options include `clientConfig` (a `pg` client config), `client` (an existing client, with `shouldConnect: false` if it's already connected), `schemaName`, `tableName`, `dimensions` and `performSetup`. The store can create its schema and table on first use. Check the API reference for exact defaults before relying on them.
- It stores node text in the table, so a separate document store is not needed for retrieval.
- `setCollection("name")` lets several corpora share one table.
- **Data written by the Python `PGVectorStore` is not compatible** with the TypeScript one (stated in the API reference). Don't point the two at the same table.
- Production: use a connection pool, and add an HNSW or IVFFlat index on the embeddings column once the table is large. Create it with plain SQL; pgvector 0.5.0+ is needed for HNSW.

### Updating and deleting

```ts
await index.insert(newDoc); // add a document to an existing index
```

Deleting by source document and refreshing changed files without duplicates is handled by the ingestion pipeline's docstore and deduplication; see `02-rag/02-ingestion-pipelines.md`. If you edit a document and just call `fromDocuments` again against the same store, you add chunks rather than replace them.

## Choosing a store

| Situation | Reasonable choice |
|---|---|
| Learning, tests, small corpora | In-memory or `persistDir` |
| Already on Postgres, moderate scale | pgvector |
| Large scale, managed service, advanced filtering | A dedicated vector DB (Qdrant, Milvus, etc.) |
| Edge runtimes | Check which stores are importable there (see note 01) |

## Common mistakes

**Embedding model drift.** Index built with one model, queried with another (or a different `dimensions`). Symptoms: dimension errors from the database, or silently poor retrieval. Pin the model in config and treat a model change as a full re-index.

**Rebuilding on every start.** If startup is slow and your bill has line items for embeddings, you are re-embedding. Persist, then load.

**Re-ingesting without IDs.** Running `fromDocuments` on the same files twice into a persistent store duplicates chunks. Use stable `id_`s and the ingestion pipeline.

**Assuming `fromVectorStore` loads documents.** It attaches to vectors; whether node text comes back depends on whether the store keeps text (`PGVectorStore` does).

**Sharing a persist directory across index types or concurrent writers.**

## Debugging

- Count rows or vectors in the store after ingest; compare with the node count you expect.
- If every query returns nothing relevant after a restart, confirm you attached to the same directory, table or collection you wrote to, and that `Settings.embedModel` matches.
- Dimension mismatch errors: compare the embedding model's output size with the store's configured `dimensions`.
- For pgvector, `EXPLAIN` your similarity query to confirm it uses the vector index once you create one.

## Quick Summary

- Default storage is in-memory. Persist with `storageContextFromDefaults({ persistDir })` or an external vector store.
- Build once with `fromDocuments`, then reload with `VectorStoreIndex.init({ storageContext })` (local) or `VectorStoreIndex.fromVectorStore(vectorStore)` (external).
- Query-time embeddings must come from the same model as index-time embeddings; vector dimensions must match.
- One `persistDir` per index; the local store is for prototypes.
- `PGVectorStore` (`@llamaindex/postgres`) needs the `vector` extension, matching `dimensions`, and is not Python-compatible.
- Re-running ingestion without stable IDs duplicates data.

## Next

[04-query-and-chat-engines.md](./04-query-and-chat-engines.md): turning an index into answers, controlling synthesis, streaming, and multi-turn chat.
