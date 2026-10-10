# LlamaIndex Architecture

## Concept
LlamaIndex.TS is a set of composable abstractions. Understanding the **seven core abstractions** and how data flows between them makes every other file in this chapter easy to place.

## Prerequisites
- [README.md](README.md)
- `01-fundamentals/rag-pipeline.md`

## The Seven Abstractions

| Abstraction | Role | Key classes (examples) |
|---|---|---|
| **Reader** | Source → `Document`s | `SimpleDirectoryReader`, `PDFReader`, `MarkdownReader` (`@llamaindex/readers/*`) |
| **Transformation** | `Node`s in → `Node`s out | Node parsers, embedding models, custom `TransformComponent` |
| **Node** | The retrievable unit with metadata and relationships | `TextNode`, `Document` (`IndexNode`, `ImageNode` exist; verify) |
| **Index** | Organizes nodes for retrieval | `VectorStoreIndex`, `SummaryIndex`, `KeywordTableIndex` |
| **Storage** | Persists nodes, index metadata, vectors | `StorageContext`, docstore, index store, vector store |
| **Retriever** | Query → relevant nodes | `index.asRetriever()`, `Bm25Retriever`, custom `BaseRetriever` subclasses |
| **Query engine** | Retriever + postprocessors + synthesizer → answer | `RetrieverQueryEngine`, `index.asQueryEngine()` |

## Data Flow

```
INGESTION
SimpleDirectoryReader ─► Document[]
   └─► Transformations: NodeParser → (custom steps) → EmbedModel
        └─► TextNode[] with embeddings
             └─► index.insert / insertNodes / VectorStoreIndex.fromDocuments
                  └─► StorageContext: vector store + docstore + index store

QUERY
question ─► Retriever ─► NodeWithScore[]
   └─► Node postprocessors (similarity cutoff, rerank, window replace)
        └─► Response synthesizer (prompt + LLM)
             └─► Response (text + sourceNodes)
```

## Components Share a Common Shape
- Almost everything is **swappable** behind a small interface: `BaseRetriever`, query engine, node postprocessor, response synthesizer, vector store, embedding model, `LLM`.
- Pieces compose: a query engine can wrap any retriever; a tool can wrap a query engine.

## High-Level vs Low-Level API

**High level** (fast start):
```ts
const index = await VectorStoreIndex.fromDocuments(documents);
const engine = index.asQueryEngine();
```

**Low level** (full control; what the high-level API does underneath). Import paths vary by version; verify:
```ts
import { SentenceSplitter, VectorStoreIndex } from "llamaindex";
import { RetrieverQueryEngine } from "llamaindex/engines";
import { SimilarityPostprocessor } from "llamaindex/postprocessors";

const nodes = await new SentenceSplitter({ chunkSize: 512 }).transform(documents);
const index = await VectorStoreIndex.init({ nodes });          // verify; or fromDocuments(documents)

const retriever = index.asRetriever({ similarityTopK: 10 });
const engine = new RetrieverQueryEngine({
  retriever,
  nodePostprocessors: [new SimilarityPostprocessor({ similarityCutoff: 0.5 })],
  // responseSynthesizer: new ResponseSynthesizer({ responseMode: "compact" }),  // see response-synthesizer.md
});
```
Start high level; drop to low level when you need to swap a stage, insert a postprocessor, or debug.

## Where to Plug In

| Goal | Plug in at |
|---|---|
| Better chunks | Node parser (`03-data-ingestion/chunking.md`) |
| Extra metadata | Custom `TransformComponent` ([transformations.md](transformations.md)) |
| Different search | Retriever (`07-retrievers/`) |
| Rewrite the question | Query transform (`08-query-transformation/`) |
| Filter or rerank candidates | Node postprocessor (`09-reranking/`) |
| Change the answer style | Prompt or synthesizer (`10-context-and-generation/`) |
| Multi-turn | Chat engine (`12-chat/`) |
| Custom multi-step logic | Plain async functions or Workflows ([query-pipeline.md](query-pipeline.md)) |

## Observability Hooks
- LlamaIndex.TS has a callback/event mechanism (`Settings.callbackManager`; verify) and integrates with tracing tools. Details in `16-production/observability.md` (verify current API).
- Quick debugging: log `response.sourceNodes` and the retrieved nodes' text and scores.

## Important Rules
- Keep ingestion and query configuration consistent (embedding model, namespace, metadata keys).
- Prefer composition: add or swap one component at a time.
- Debug by inspecting each stage's output, retrieval first.

## Common Mistakes
- Treating the framework as a black box and never looking at retrieved nodes.
- Customizing the LLM prompt when the retriever is the problem.
- Rebuilding the index on every app start instead of connecting to the stored one.

## Related / Next
- [nodes-and-parsers.md](nodes-and-parsers.md)
- [transformations.md](transformations.md)
- `15-debugging/debugging-rag.md`
