# Embeddings and Vector Stores

An **embedding** turns text into a list of numbers so that texts with similar meaning end up close together. A **vector store** saves those vectors and answers "which stored chunks are closest to this query?" Together they give you semantic search: finding text by meaning, not by matching keywords.

> Checked against the LangChain JS 1.x docs. `MemoryVectorStore` is imported from `@langchain/classic` in 1.x.

**Prerequisites:** [Loading and Splitting](./01-loading-and-splitting.md)

---

## The idea

```text
"When was Nike incorporated?"  ──embed──►  [0.012, -0.34, ...]   (query vector)
                                                  │
                         nearest vectors ◄────────┘
                                │
chunk A [..]   chunk B [..]   chunk C [..]   ...   (stored chunk vectors)
```

Distance between vectors approximates difference in meaning. A query like "company founding date" can match a chunk saying "incorporated in 1964" even though they share no words. That is also the weakness: exact strings, IDs and rare terms are not what embeddings are good at (more on this below).

---

## Embedding models

```bash
npm i @langchain/openai
```

```ts
import { OpenAIEmbeddings } from "@langchain/openai";

const embeddings = new OpenAIEmbeddings({ model: "text-embedding-3-large" });

const v = await embeddings.embedQuery("Dogs are loyal companions.");
console.log(v.length); // a fixed length per model
```

The interface has two main methods:

| Method | Use for |
|---|---|
| `embedQuery(text)` | One string, typically the user's question |
| `embedDocuments(texts)` | A batch of strings, typically chunks being indexed |

Rules that matter:

- **Vector length is fixed per model** (the docs' OpenAI example prints 1536). Your vector store index must be created with the matching dimension.
- **Use the same model for indexing and querying.** Vectors from different models are not comparable.
- **Changing the model means re-embedding everything.**
- Other providers ship their own embedding classes in their packages; the call shape is the same.

Embedding costs money and time proportional to text volume. Embed once at ingest, not on every request. Only the query gets embedded at request time.

---

## Vector stores

All stores implement the same `VectorStore` interface, so switching is mostly a constructor change.

| Store | Package | Persistence |
|---|---|---|
| Memory | `@langchain/classic` | In-process only (lost on restart) |
| PGVector | `@langchain/pgvector` | Persistent |
| Pinecone | `@langchain/pinecone` | Persistent |
| Qdrant | `@langchain/qdrant` | Persistent |
| MongoDB Atlas | `@langchain/mongodb` | Persistent |
| Redis | `@langchain/redis` | Persistent |
| Weaviate | `@langchain/weaviate` | Persistent |

This is a subset; the integrations page has the full list and each store's setup.

### In-memory store (dev, tests, small data)

```ts
import { MemoryVectorStore } from "@langchain/classic/vectorstores/memory";

const vectorStore = new MemoryVectorStore(embeddings);
await vectorStore.addDocuments(chunks); // embeds each chunk and stores it
```

`addDocuments` calls the embedding model for you. Nothing is saved to disk, so rebuild on every process start. Great for learning and unit tests; not for production.

### Choosing a persistent store

If you already run Postgres, PGVector avoids a new piece of infrastructure. A managed service (Pinecone, Qdrant Cloud, etc.) trades operational work for cost and a new dependency. Decide on: data size, filtering needs, hybrid search support, ops capacity, and where your data is allowed to live. Setup and constructor options differ per store, so follow that store's integration page.

---

## Searching

```ts
const results = await vectorStore.similaritySearch("When was Nike incorporated?", 4);

for (const doc of results) {
  console.log(doc.metadata.source, doc.pageContent.slice(0, 120));
}
```

- Second argument `k` is how many chunks to return.
- Results are `Document`s, so your metadata (source, page) comes back with them.

### With scores

```ts
const scored = await vectorStore.similaritySearchWithScore("Nike revenue 2023", 4);
// [[Document, score], ...]
```

**Be careful interpreting scores.** Depending on the store and metric, a score may be a similarity (higher is better) or a distance (lower is better), and the scale differs between stores. Don't hard-code a threshold copied from another setup. Print scores for known good and bad queries and pick your cutoff from that.

### Filtering by metadata

The interface supports metadata filters on search, but **the filter syntax depends on the store**. Use it for tenant isolation, language, date ranges, or access control. Check your store's page for the format.

> If different users must not see each other's data, enforce that with a filter (or separate indexes) on **every** query, in server code, not in the prompt.

### Deleting and updating

`delete` removes stored documents by id. To keep an index fresh, give chunks stable ids at ingest, delete the old ones for a changed source, and add the new ones. Re-adding without deleting creates duplicates.

---

## Retrievers: a vector store as a Runnable

```ts
const retriever = vectorStore.asRetriever({
  searchType: "mmr",
  searchKwargs: { fetchK: 20 },
});

const docs = await retriever.invoke("What was Nike's revenue in 2023?");
```

A retriever takes a query string and returns `Document[]`. It is a Runnable, so `invoke` and `batch` work and it composes in chains ([Runnables and LCEL](../01-core/04-runnables-and-lcel.md)). The next note puts it to use.

`searchType: "mmr"` (maximal marginal relevance) trades a bit of raw similarity for **diversity**, avoiding five near-identical chunks. Option names inside `searchKwargs` can vary by store, so confirm on your store's page. The docs example sets `fetchK`.

---

## Where pure vector search falls short

- **Exact matches:** product codes, error codes, names and IDs may not be retrieved reliably. Hybrid search (vector plus keyword/BM25) helps; support depends on the store.
- **Questions needing several documents or aggregation** ("how many customers churned last quarter") are not retrieval problems. Use SQL or tools.
- **Short or vague queries** embed poorly. Query rewriting can help.
- **Chunks missing context:** if the chunk doesn't say what it is about, neither will its vector. Add titles or section headers into chunk text or metadata.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Different embedding models for index and query | Use one model for both; re-embed if you change it |
| Wrong vector dimension in the store schema | Match the embedding model's output length |
| Using `MemoryVectorStore` in production | It vanishes on restart; use a persistent store |
| Re-embedding the corpus on every request or server start | Embed once at ingest and persist |
| Treating scores as universal | Meaning and scale vary by store; calibrate on your data |
| Re-adding changed docs without deleting old chunks | Use stable ids; delete then add |
| No metadata filter on multi-tenant data | Filter every query by tenant in code |
| Assuming top-k contains the answer | Test retrieval separately from generation |

### Debugging retrieval

1. Run `similaritySearch` directly with your real questions and read the top chunks.
2. If the right chunk is absent: look at chunking, metadata, and whether the content was even indexed.
3. If it is present but ranked low: try a larger `k` plus re-ranking, hybrid search, or better chunk context.
4. Only when retrieval looks right, debug the prompt and model.

---

## Quick Summary

- Embeddings map text to vectors; nearby vectors mean similar meaning.
- `embedQuery` for questions, `embedDocuments` for chunks; same model on both sides; dimension is fixed per model.
- `vectorStore.addDocuments(chunks)`, then `similaritySearch(query, k)`; results are `Document`s with metadata.
- `MemoryVectorStore` (`@langchain/classic`) is for dev only; use a persistent store in production.
- `asRetriever()` turns the store into a Runnable; `"mmr"` adds diversity.
- Scores, filters and option names are store-specific. Check your store's docs and calibrate on real data.

**Next:** [Retrieval and RAG](./03-retrieval-and-rag.md)