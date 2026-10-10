# Vector Database Comparison

## Concept
There is no single best vector database. The right choice depends on scale, operations capacity, existing infrastructure, filtering needs and budget.

> Features, limits and pricing change often. This file compares **categories and typical trade-offs**. Verify specifics against each vendor's current documentation before deciding. Also check that a maintained TypeScript client and a LlamaIndex.TS integration exist for your candidate; the Python ecosystem has more integrations.

## Prerequisites
- [vector-db-concepts.md](vector-db-concepts.md)
- [partitioning-and-filtering.md](partitioning-and-filtering.md)

## Categories

| Category | Examples | Idea |
|---|---|---|
| **Managed vector service** | Pinecone | Fully hosted; you don't run servers |
| **Open-source vector database** | Qdrant, Weaviate, Milvus | Self-host or use the vendor's cloud |
| **Extension to an existing database** | pgvector (PostgreSQL), Elasticsearch/OpenSearch vector search | Add vectors to a database you already run |
| **Embedded / local** | Chroma, LanceDB | Runs in-process or on a laptop; great for prototypes |
| **Library** | FAISS | Fast index code, not a database; you handle persistence and serving |

## Typical Trade-offs

| Option | Strengths | Watch out for |
|---|---|---|
| **Pinecone** | No infrastructure to run; serverless scaling; namespaces; sparse-dense support | Vendor lock-in; ongoing usage cost; data lives in their cloud |
| **Qdrant / Weaviate / Milvus** | Control, open source, rich filtering and features; self-host option | You operate and scale it (unless using their cloud) |
| **pgvector** | One database for relational data and vectors; transactions; familiar tooling | Large-scale, high-QPS workloads may need tuning or a dedicated system |
| **Elasticsearch / OpenSearch** | Strong keyword plus vector hybrid search if already deployed | Heavier to run; vectors are an add-on |
| **Chroma / LanceDB** | Very quick start; local development | Less suited to large multi-tenant production without extra work |
| **FAISS** | Fast, flexible, free | No built-in metadata filtering, persistence or server layer; Node support is limited (verify) |

## Decision Guide

| Situation | Lean toward |
|---|---|
| Prototype or learning project | Chroma, or an in-memory LlamaIndex.TS index (`VectorStoreIndex` with the default store) |
| Small team, want production quickly, no ops | Managed service (Pinecone) |
| Already run PostgreSQL, moderate scale | pgvector |
| Need full control, data residency, or lower cost at large scale | Self-hosted open source |
| Already run Elasticsearch/OpenSearch with strong keyword needs | Its vector search |
| Heavy hybrid (keyword + vector) requirements | Check hybrid support in each candidate |

## Evaluation Criteria

| Criterion | Question to ask |
|---|---|
| Scale | How many vectors now and in a year? |
| Latency | What p95 query time do you need? |
| Filtering | Which filter operators and metadata types are required? |
| Multi-tenancy | Namespaces/partitions? How many tenants are supported? |
| Hybrid search | Native sparse + dense support? |
| Operations | Who runs it, patches it, scales it, backs it up? |
| Cost model | Per-query, per-storage, per-node? Estimate real monthly cost |
| Data location and compliance | Region, residency, encryption, certifications |
| Ecosystem | Is there a maintained LlamaIndex.TS integration and TypeScript SDK? |
| Exit cost | How hard is it to move your data elsewhere? |

## Practical Advice
- **Keep your raw documents and embedding config.** With them, switching vector databases is a re-ingestion, not a rewrite.
- Prototype on the simplest option; choose production infrastructure when you know your scale and filtering needs.
- Test with **your** data volume and query patterns, not vendor benchmarks.
- LlamaIndex.TS abstracts the store behind a vector store interface, so swapping later is mostly a configuration change plus re-ingestion.

```ts
// Swapping stores in LlamaIndex.TS touches only this part
import { VectorStoreIndex } from "llamaindex";
import { PineconeVectorStore } from "@llamaindex/pinecone";

const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME,
  namespace: process.env.PINECONE_NAMESPACE ?? "",
});

const index = await VectorStoreIndex.fromDocuments(documents, {
  storageContext: { vectorStore },     // docs also show: await StorageContext.fromDefaults({ vectorStore }); verify
});

// Later, connect to the same data without re-embedding:
const existing = await VectorStoreIndex.fromVectorStore(vectorStore);
```
Another store means another package (`@llamaindex/<name>`) and its own constructor options; the rest of your code stays the same.

## This Repository's Choice
This repository uses **Pinecone** for hands-on chapters because it removes infrastructure work and lets you focus on RAG concepts. Everything in `04-vector-databases/` applies to any vector store.

## Common Mistakes
- Choosing by benchmark headlines instead of your own workload.
- Adopting a self-hosted system without anyone owning operations.
- Adding a dedicated vector database when pgvector would have been enough.
- Picking a store with no solid TypeScript client or LlamaIndex.TS integration.
- Ignoring exit cost until the bill or the limits arrive.

## Related / Next
- `05-pinecone/README.md`
- `16-production/cost-and-scaling.md`
