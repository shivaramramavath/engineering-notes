# LlamaIndex (LlamaIndex.TS)

> **Accuracy note.** TypeScript edition. API details come from LlamaIndex.TS and Pinecone documentation pages read on 2026-10-10 plus knowledge of the libraries. The code was NOT compiled or run (the sandbox could not install npm packages). Import paths in particular vary between versions and doc pages; verify against your installed version. Where something is likely to drift it is marked "verify". Docs: <https://ts.llamaindex.ai>.

## What LlamaIndex Is
LlamaIndex is a framework for building LLM applications over your data. For RAG it provides ready-made building blocks for each pipeline stage, so you assemble a system instead of writing each stage by hand. This chapter covers **LlamaIndex.TS** (package `llamaindex` plus `@llamaindex/*`). The Python framework is larger; where LlamaIndex.TS lacks a feature, the relevant file says so and shows how to build the equivalent.

```
Documents ─► Transformations ─► Nodes ─► Index (+ Storage) ─► Retriever ─► Postprocessors ─► Synthesizer ─► Response
                                                              └────────────── Query Engine ─────────────────────┘
```

## Installation and Packages
The core lives in `llamaindex`; integrations are separate `@llamaindex/<name>` packages. Use Node 20+ (verify the exact minimum) and ESM (`"type": "module"` in `package.json`).

```bash
npm i llamaindex @llamaindex/openai @llamaindex/pinecone @pinecone-database/pinecone
npm i -D typescript tsx @types/node
# optional
npm i @llamaindex/readers @llamaindex/bm25-retriever @llamaindex/cohere @llamaindex/workflow zod gpt-tokenizer
```
Run a script with env vars loaded and no dotenv:
```bash
npx tsx --env-file=.env src/main.ts
```
Expected `.env` (add it to `.gitignore`): `OPENAI_API_KEY`, `PINECONE_API_KEY`, `PINECONE_INDEX_NAME`, `PINECONE_NAMESPACE`.

Install only `llamaindex` and the `OpenAI` or Pinecone imports will fail: they live in `@llamaindex/openai` and `@llamaindex/pinecone`. Import paths differ between versions and doc pages (for example `llamaindex/engines`, `llamaindex/postprocessors`, `llamaindex/storage`); verify against what your installed version exports.

## Global Settings
```ts
import { Settings, SentenceSplitter } from "llamaindex";
import { OpenAI, OpenAIEmbedding } from "@llamaindex/openai";

Settings.llm = new OpenAI({ model: "gpt-4o-mini" });                       // verify current model names
Settings.embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });
Settings.nodeParser = new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 });
```
`Settings` supplies defaults to everything built afterward. Pass components explicitly when you need per-index control. Rule: set the embedding model once and use it for ingestion **and** querying.

## Minimal End-to-End Example
```ts
import { VectorStoreIndex } from "llamaindex";
import { SimpleDirectoryReader } from "@llamaindex/readers/directory";   // verify path

const documents = await new SimpleDirectoryReader().loadData({ directoryPath: "./data" });
const index = await VectorStoreIndex.fromDocuments(documents);
const queryEngine = index.asQueryEngine({ similarityTopK: 4 });

const response = await queryEngine.query({ query: "What is the refund policy?" });
console.log(response.toString());
for (const n of response.sourceNodes ?? []) {                            // verify property name
  console.log(n.score, n.node.metadata["source"]);
}
```

## Chapter Map

| File | Read when you need to |
|---|---|
| [architecture.md](architecture.md) | See how all the pieces fit and where to plug in |
| [nodes-and-parsers.md](nodes-and-parsers.md) | Work with the Node API and relationships |
| [transformations.md](transformations.md) | Chain parsers and embeddings; write custom steps |
| [indexes.md](indexes.md) | Pick an index type |
| [storage.md](storage.md) | Persist data; connect Pinecone; manage docstores |
| [query-engine.md](query-engine.md) | Turn an index into question answering |
| [response-synthesizer.md](response-synthesizer.md) | Control how retrieved text becomes an answer |
| [query-pipeline.md](query-pipeline.md) | Compose custom multi-step flows (Workflows and plain TypeScript) |

Retrievers, query transformation, reranking, chat and document management have their own chapters (`07` to `12`), because they are large topics that cut across the framework.

## Where Each Concern Lives

| Concern | Chapter |
|---|---|
| Loading, chunking, metadata | `03-data-ingestion/` |
| Retrievers | `07-retrievers/` |
| HyDE, rewriting, sub-questions | `08-query-transformation/` |
| Rerankers | `09-reranking/` |
| Prompts, context, streaming | `10-context-and-generation/` |
| Insert, update, delete documents | `11-document-management/` |
| Chat engines and memory | `12-chat/` |

## Common Mistakes
- Copying old tutorials or Python snippets: names, import paths and available features differ in LlamaIndex.TS.
- Mixing embedding models between ingestion and query.
- Relying on defaults (chunk size, `similarityTopK`, response mode) without checking them.
- Installing only `llamaindex` and then wondering why OpenAI or Pinecone imports fail.
- Exposing API keys in browser code. The libraries are Node-oriented; call them from a server.

## Related / Next
- [architecture.md](architecture.md)
- `05-pinecone/README.md`
