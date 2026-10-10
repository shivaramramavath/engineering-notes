# Indexes

## Concept
An **index** is a data structure over nodes that makes them retrievable in a particular way. Every index can produce a retriever (`asRetriever`) and a query engine (`asQueryEngine`). The index type decides *how* nodes are found.

## Prerequisites
- [nodes-and-parsers.md](nodes-and-parsers.md)
- `04-vector-databases/vector-db-concepts.md`

## Index Types

"In LlamaIndex.TS?" follows the convention of `07-retrievers/README.md`: **Yes** = a documented class, **Not documented** = exists in Python; verify your installed version before relying on it, **Not available** = no library support.

| Index | How it finds nodes | Best for | In LlamaIndex.TS? |
|---|---|---|---|
| **`VectorStoreIndex`** | Embedding similarity | Default for almost all RAG | Yes |
| **`SummaryIndex`** | Returns all nodes | Summarizing an entire document or small corpus | Yes (basic form; the Python LLM-ranked and embedding-ranked retriever variants are not documented for TS; verify) |
| **`KeywordTableIndex`** | Keyword extraction and lookup | Exact-keyword needs on small corpora | Yes (mode `rake` / `simple`; verify) |
| **`TreeIndex`** | Hierarchy of summaries | Top-down summarization; less common | Not documented (verify) |
| **`DocumentSummaryIndex`** | Per-document summaries guide retrieval | Choosing which documents to look in first | Not documented (verify); hand-roll below |
| **`PropertyGraphIndex`** | Entities and relationships (knowledge graph) plus vectors | Questions about connections between entities | Not available |

Start with `VectorStoreIndex`. Reach for others only for a specific need.

## VectorStoreIndex

### Build from documents
```ts
import { VectorStoreIndex } from "llamaindex";

const index = await VectorStoreIndex.fromDocuments(documents);
// Python's show_progress flag has no documented equivalent; log around the call instead (verify)
```

### Build from nodes
```ts
const index = await VectorStoreIndex.init({ nodes });     // verify; fromDocuments(documents) is the documented path
```

### Connect to an existing vector store (no re-embedding)
```ts
import { VectorStoreIndex } from "llamaindex";
import { PineconeVectorStore } from "@llamaindex/pinecone";

const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME!,
  namespace: "docs",
});
const index = await VectorStoreIndex.fromVectorStore(vectorStore);   // verify
```
This is the usual app-startup path: ingestion happened earlier, the app only connects. `PineconeVectorStore` creates its own Pinecone client and reads `PINECONE_API_KEY` (see [storage.md](storage.md) and `05-pinecone/upsert.md`).

### Use it
```ts
const retriever = index.asRetriever({ similarityTopK: 5 });
const engine = index.asQueryEngine({ similarityTopK: 5 });
```

## SummaryIndex
```ts
import { SummaryIndex } from "llamaindex";

const summaryIndex = await SummaryIndex.fromDocuments(documents);
const engine = summaryIndex.asQueryEngine();     // see response-synthesizer.md for choosing tree_summarize
const response = await engine.query({ query: "Summarize the whole document." });
console.log(response.toString());
```
Top-k vector retrieval cannot answer "summarize everything" well, since it sees only a few chunks. A summary index feeds all nodes through a summarizing synthesizer. It uses many LLM tokens, so use it for bounded corpora. Passing a `tree_summarize` synthesizer to this engine is the Python recipe; in TS construct the engine with an explicit synthesizer (verify the option name; see [response-synthesizer.md](response-synthesizer.md)).

## KeywordTableIndex
Extracts keywords per node (LLM or simple extraction) and looks nodes up by query keywords. LlamaIndex.TS has a `KeywordTableIndex` with `rake` and `simple` modes (verify); the LLM-based extraction variant is not documented for TS. Mostly useful as a learning tool or for small exact-match use; see `07-retrievers/keyword-table-retriever.md`. For production keyword search, BM25 is usually a better choice (`07-retrievers/bm25-retriever.md`, `Bm25Retriever` from `@llamaindex/bm25-retriever`).

## DocumentSummaryIndex (not documented in LlamaIndex.TS)
Python builds a summary per document and uses the summaries to pick which documents to retrieve from. Do not expect `DocumentSummaryIndex` in the TS package (verify). A small hand-rolled equivalent (unverified, not compiled): summarize each document with the LLM, embed the summaries in their own `VectorStoreIndex`, and keep one per-document vector index in a `Map`.

```ts
import { Document, Settings, VectorStoreIndex } from "llamaindex";

async function buildDocSummaryRouter(documents: Document[]) {
  const summaryDocs: Document[] = [];
  const perDocIndex = new Map<string, VectorStoreIndex>();

  for (const doc of documents) {
    const res = await Settings.llm.complete({
      prompt: `Summarize in 3 sentences:\n\n${doc.getText()}`,
    });                                                   // verify complete() signature and result field
    summaryDocs.push(new Document({ text: res.text, metadata: { docId: doc.id_ } }));
    perDocIndex.set(doc.id_, await VectorStoreIndex.fromDocuments([doc]));
  }
  const summaryIndex = await VectorStoreIndex.fromDocuments(summaryDocs);
  return { summaryIndex, perDocIndex };
}

// Query time: 1) retrieve top summaries, 2) read metadata.docId, 3) query that document's index.
```
One LLM call per document at ingestion. For large corpora prefer `RouterQueryEngine` over per-document engines (see `07-retrievers/router-retriever.md`).

## PropertyGraphIndex (not available in LlamaIndex.TS)
Python builds a graph of entities and relations from text (and can also keep vector embeddings), then supports graph-aware retrieval. There is no `PropertyGraphIndex` in LlamaIndex.TS, so there is no TS code to show here. If you truly need a graph, options are a separate graph database with its own client plus your own entity extraction step (an LLM-based `TransformComponent`, see [transformations.md](transformations.md)), or running the Python framework as a separate service. Worth it only when questions are about relationships ("which suppliers affect product X?"); it adds significant ingestion cost and complexity, so confirm that vector retrieval is not enough first. Concept notes in `07-retrievers/property-graph-retriever.md`.

## Choosing

| Question type | Index |
|---|---|
| "What does the doc say about X?" | `VectorStoreIndex` |
| "Summarize this document" | `SummaryIndex` (or a hand-rolled document-summary setup) |
| "Which document is relevant?" then drill down | Hand-rolled document summaries + per-document vector indexes (or `RouterQueryEngine`) |
| "How are A and B connected?" | Not available in TS (graph database or Python) |
| Exact tokens like error codes | BM25 / hybrid in addition to vector |

## Multiple Indexes
Real systems often combine indexes, each wrapped as a query engine tool, with a router or sub-question engine choosing among them (`07-retrievers/router-retriever.md`, `08-query-transformation/sub-question-query-engine.md`).

## Index Operations (preview)
```ts
await index.insert(newDocument);                                       // add a document
await index.deleteRefDoc("doc-id", true);                              // second arg: delete from docstore (verify signature)
await index.refreshRefDocs(updatedDocuments);                          // update changed, insert new (verify it exists in your version)
```
Behavior depends on your vector store and on whether a docstore is kept. These are covered in `11-document-management/`; read that chapter before relying on them with Pinecone. If `refreshRefDocs` is missing, use `IngestionPipeline` with a docstore (see [transformations.md](transformations.md)).

## Important Rules
- One `VectorStoreIndex` per embedding model and collection of data.
- App startup should **connect** to an existing store, not rebuild.
- Use specialized indexes for the question type they serve, not by default.
- Do not assume a Python index class exists in TS; check the table above.

## Common Mistakes
- Rebuilding the vector index at every launch (cost and time).
- Using top-k vector retrieval for whole-corpus summarization.
- Adopting a graph index without a clear relational question (and then discovering TS has none).
- Assuming `deleteRefDoc` works identically on every vector store.

## Related / Next
- [storage.md](storage.md)
- [query-engine.md](query-engine.md)
- `07-retrievers/README.md`
