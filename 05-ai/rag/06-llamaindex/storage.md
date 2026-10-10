# Storage

## Concept
LlamaIndex keeps data in up to **three stores**, bundled by a `StorageContext`:

| Store | Holds | Default |
|---|---|---|
| **Vector store** | Embeddings (and often node text and metadata) | In-memory `SimpleVectorStore` |
| **Docstore** | The full `Node`/`Document` objects, keyed by ID, plus hashes | In-memory `SimpleDocumentStore` |
| **Index store** | Index structure metadata (which nodes belong to which index) | In-memory `SimpleIndexStore` |

```
StorageContext
  ├── vectorStore   (embeddings)         ← Pinecone, Chroma, Qdrant, ...
  ├── docStore      (nodes, doc hashes)  ← local; database-backed options are limited in TS (see below)
  └── indexStore    (index metadata)
```

## Prerequisites
- [indexes.md](indexes.md)
- `04-vector-databases/vector-db-concepts.md`

## Default: Local Persistence
```ts
import { VectorStoreIndex, storageContextFromDefaults } from "llamaindex";

// first run: build and persist
const storageContext = await storageContextFromDefaults({ persistDir: "./storage" });   // verify name and path
const index = await VectorStoreIndex.fromDocuments(documents, { storageContext });

// later: load from the same directory
const loadedContext = await storageContextFromDefaults({ persistDir: "./storage" });
const loaded = await VectorStoreIndex.init({ storageContext: loadedContext });          // verify
```
The persist and load calls differ from Python (there is no `load_index_from_storage` documented for TS; verify). Newer versions may also expose `StorageContext.fromDefaults`-style helpers; check the docs for your version. Good for prototypes. Everything lives in JSON files in the directory.

## Remote Vector Store (Pinecone)
```ts
import { VectorStoreIndex } from "llamaindex";
import { PineconeVectorStore } from "@llamaindex/pinecone";

const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME!,
  namespace: "tenant-acme",
});

// Build (first time)
const index = await VectorStoreIndex.fromDocuments(documents, {
  storageContext: { vectorStore },   // docs also show a storageContextFromDefaults({ vectorStore }) helper; verify
});

// Reconnect (every later start)
const reconnected = await VectorStoreIndex.fromVectorStore(vectorStore);   // verify
```
`PineconeVectorStore` builds its own client (reads `PINECONE_API_KEY` unless you pass `apiKey`); you do not pass a Pinecone `Index` object as in Python. Options such as `chunkSize` (upsert batch size) and `textKey` are covered in `05-pinecone/upsert.md`. Use the same `namespace` on write and read.

## What Pinecone Stores vs What Docstore Stores
Vector stores like Pinecone **store the node text and metadata** alongside vectors (`storesText`). In that case LlamaIndex by default does **not** populate the docstore or index store for a plain `VectorStoreIndex`, since the vector store can return the text itself (verify this default for your TS version).

Consequences:
- Simple RAG on Pinecone works without any docstore.
- Features that need node lookup by ID (auto-merging with parent nodes, recursive retrieval, some update flows) need a **docstore**, and you must create and populate one yourself. Python can force node storage with `store_nodes_override=True`; a documented TS equivalent is not known (verify), so populate the docstore or a `Map` explicitly as shown below.
- `IngestionPipeline` with a docstore is the clean way to get change detection (`03-data-ingestion/ingestion-pipeline.md`).

## Docstore Options
`SimpleDocumentStore` is in memory and file-persisted. **Database-backed docstores (Redis, MongoDB, Postgres and similar) are well known from Python; they are not documented for LlamaIndex.TS** (verify your installed version and the `@llamaindex/*` package list before assuming one exists). If you need a shared durable docstore in TS, either check for an integration package, or implement a small store yourself (unverified sketch, not compiled; adapt to the real docstore interface of your version, or simply use it directly in your own retriever code):

```ts
import type { BaseNode } from "@llamaindex/core/schema";
import { SimpleDocumentStore } from "llamaindex/storage";   // verify path

const docStore = new SimpleDocumentStore();
await docStore.addDocuments(allNodes, true);                // e.g. parents and leaves for auto-merging; verify signature

const storageContext = { vectorStore, docStore };           // verify; helper form: storageContextFromDefaults({ vectorStore, docStore })
```

Hand-rolled shared node lookup (what auto-merging needs), backed by any key-value store you already run:
```ts
interface NodeStore {
  get(id: string): Promise<BaseNode | undefined>;
  put(nodes: BaseNode[]): Promise<void>;
}

class MapNodeStore implements NodeStore {                    // swap the Map for Redis/Postgres calls in production
  private nodes = new Map<string, BaseNode>();
  async get(id: string) { return this.nodes.get(id); }
  async put(nodes: BaseNode[]) { for (const n of nodes) this.nodes.set(n.id_, n); }
}
```

| Need | Docstore |
|---|---|
| Prototype, single process | `SimpleDocumentStore` + file persist |
| Auto-merging or recursive retrieval in production | Shared database-backed store; in TS likely hand-rolled (see above) |
| Ingestion change detection across runs | Persisted docstore |
| Multiple app servers | Shared docstore (not local files) |

## Keeping Stores in Sync
The vector store and docstore are separate systems. If you delete a document from one but not the other, they drift apart:
- Orphan vectors still return results whose docstore entry is gone.
- Docstore entries linger for vectors that were deleted.

Always change both inside one code path, ideally the pipeline or `refreshRefDocs` (verify it exists in your version). See `11-document-management/refresh-and-sync.md`.

## Chat Stores (preview)
Conversation history uses a separate **chat store**, not these three. See `12-chat/chat-sessions.md`.

## Important Rules
- Reconnect to a remote vector store at startup; do not rebuild.
- Use the same `namespace` for write and read.
- If you need parent/child lookup, you need a docstore (or node `Map`) with *all* nodes.
- Persist docstore state somewhere shared and durable in production.

## Common Mistakes
- Using auto-merging retrieval on Pinecone without a populated docstore.
- In-memory docstore lost on restart; the next ingestion duplicates everything.
- Updating the vector store but not the docstore (or the reverse).
- Persisting to a local directory in a stateless container.
- Looking for Python docstore integrations (Redis, MongoDB, Postgres) in the TS packages.

## Related / Next
- [query-engine.md](query-engine.md)
- `11-document-management/doc-ids-and-tracking.md`
- `07-retrievers/auto-merging-retriever.md`
