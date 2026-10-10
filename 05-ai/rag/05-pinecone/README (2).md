# Pinecone

> **Accuracy note.** TypeScript edition. API details come from LlamaIndex.TS and Pinecone documentation pages read on 2026-10-10 plus knowledge of the libraries. The code was NOT compiled or run (the sandbox could not install npm packages). Import paths in particular vary between versions and doc pages; verify against your installed version. Pinecone's SDK and limits change often, so every limit, default and model name below is marked "verify" where it is likely to drift. Before relying on a number, confirm it at <https://docs.pinecone.io>.

## What Pinecone Is
Pinecone is a managed vector database. You create an **index**, **upsert** vectors with metadata into **namespaces**, and **query** by vector with optional metadata filters. You do not run servers.

```
Your app ──► Pinecone client ──► Index
                                   ├── namespace "tenant-a": [records...]
                                   ├── namespace "tenant-b": [records...]
                                   └── namespace "": [records...]    (default)
```

## Architecture in One Page

| Concept | Meaning |
|---|---|
| **Project** | Container for indexes and API keys |
| **Index** | A collection of vectors with a fixed dimension and metric |
| **Serverless index** | Pay for storage and usage; Pinecone manages capacity. The standard choice for new projects |
| **Namespace** | Isolated partition inside an index |
| **Record** | `id` + `values` (vector) + optional `metadata` (+ optional `sparseValues`) |
| **Host** | The URL of one index; used to connect for data operations |

Two API surfaces:
- **Control plane**: create, describe, list, delete indexes (the `Pinecone` client).
- **Data plane**: upsert, query, fetch, delete, update records (the `Index` object returned by `pc.index(...)`).

The TypeScript SDK is Node-oriented. Never use it in browser code with a real API key; call it from a server and let the browser talk to your server.

## How Pinecone Fits This Repository

```
03-data-ingestion  ──►  embeddings  ──►  Pinecone index/namespace
                                              │
LlamaIndex.TS retriever  ◄── query + filters ─┘
```

LlamaIndex.TS talks to Pinecone through `PineconeVectorStore` (package `@llamaindex/pinecone`). This chapter shows the **raw Pinecone API** so you understand what LlamaIndex does underneath, and so you can debug it.

## Chapter Map

| File | Read when you need to |
|---|---|
| [setup.md](setup.md) | Install, authenticate, connect |
| [indexes.md](indexes.md) | Create and choose index settings |
| [upsert.md](upsert.md) | Load vectors efficiently |
| [query.md](query.md) | Search and fetch records |
| [delete-and-update.md](delete-and-update.md) | Remove or change data |
| [namespaces-and-filters.md](namespaces-and-filters.md) | Isolate tenants, filter by metadata |
| [sparse-and-hybrid.md](sparse-and-hybrid.md) | Combine keyword and semantic search |
| [scaling-and-cost.md](scaling-and-cost.md) | Control cost and plan for growth |

Cross-cutting production topics (security, observability, caching, retries) live in `16-production/`, not here.

## Quick Start
```ts
import { Pinecone } from "@pinecone-database/pinecone";

const pc = new Pinecone({ apiKey: process.env.PINECONE_API_KEY! });

await pc.createIndex({
  name: "docs",
  dimension: 1536,
  metric: "cosine",
  spec: { serverless: { cloud: "aws", region: "us-east-1" } },
  waitUntilReady: true, // verify option name per SDK version
});

const model = await pc.describeIndex("docs");
const index = pc.index({ host: model.host }); // newer API; older versions: pc.index("docs")

const vec = new Array(1536).fill(0.1);
await index.upsert({
  records: [{ id: "id-1", values: vec, metadata: { source: "a.pdf" } }],
  namespace: "demo",
}); // newer API; verify against your installed version (see upsert.md)

const res = await index.query({ vector: vec, topK: 3, namespace: "demo", includeMetadata: true });
console.log(res.matches);
```
Run with `npx tsx --env-file=.env quickstart.ts`.

## Related / Next
- [setup.md](setup.md)
- `06-llamaindex/storage.md`
- `13-workflows/llamaindex-pinecone-rag.md`
