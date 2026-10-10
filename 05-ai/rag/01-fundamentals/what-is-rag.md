# What is RAG?

## Concept
**Retrieval-Augmented Generation (RAG)** is a pattern where an LLM answers a question using relevant information *retrieved at query time* from an external knowledge source, instead of relying only on what it memorized during training.

```
Question ──► Retrieve relevant chunks ──► Put them in the prompt ──► LLM answers
```

## Prerequisites
- Basic idea of what an LLM is and what a prompt is.

## Why RAG?
LLMs on their own have real limits:

| Problem | What happens | How RAG helps |
|---|---|---|
| Knowledge cutoff | Model doesn't know recent or newly added info | Retrieve from a live, updatable source |
| Private data | Model never saw your documents | Index your own files, DBs, wikis |
| Hallucination | Model invents plausible-sounding answers | Ground answers in retrieved text; cite sources |
| Context limits | You can't paste 10,000 pages into a prompt | Retrieve only the few relevant chunks |
| Cost of updates | Retraining for every data change is impractical | Update the index, not the model |
| Access control | Model can't tell who may see what | Filter retrieval by user, tenant or metadata |

## How It Works (high level)
1. **Ingest (offline):** load documents → split into chunks → embed → store in a vector database.
2. **Retrieve (online):** embed the question → find the most similar chunks.
3. **Generate (online):** insert the chunks into a prompt → the LLM writes an answer grounded in them.

Details of each stage: see [rag-pipeline.md](rag-pipeline.md).

## Minimal Example (LlamaIndex.TS)
Setup (Node 20+, verify exact minimum; ESM project with `"type": "module"` in `package.json`):

```bash
npm i llamaindex @llamaindex/openai @llamaindex/readers
npm i -D typescript tsx @types/node
```

```ts
// rag-hello.ts  (run: npx tsx --env-file=.env rag-hello.ts, with OPENAI_API_KEY in .env)
import { VectorStoreIndex } from "llamaindex";
import { SimpleDirectoryReader } from "@llamaindex/readers/directory";

// Import paths vary between LlamaIndex.TS versions; verify against your installed version.
const documents = await new SimpleDirectoryReader().loadData({ directoryPath: "./data" });
const index = await VectorStoreIndex.fromDocuments(documents);

const queryEngine = index.asQueryEngine();
const response = await queryEngine.query({ query: "What does the refund policy say?" });
console.log(response.toString());
```

With no `Settings` configured, LlamaIndex.TS defaults to OpenAI models, so `OPENAI_API_KEY` must be set (verify the defaults for your version).

## When RAG Is a Good Fit
- Q&A over documentation, policies, contracts, support tickets
- Data that changes often
- Answers must be traceable to a source
- Different users may see different data

## When RAG Is *Not* Enough
- The task is about **style or behavior**, not knowledge (see [rag-vs-fine-tuning.md](rag-vs-fine-tuning.md))
- The answer needs reasoning across *the whole* corpus (e.g. "summarize all 5,000 reviews"). Plain top-k retrieval will miss most of it; use summary indexes or other strategies.
- The data is small enough to fit directly in the context window.

## Common Mistakes
- Assuming RAG removes hallucination. It reduces it, but a bad retrieval still produces a confident wrong answer.
- Blaming the LLM when the real problem is chunking or retrieval.
- Skipping evaluation, so quality is never measured.

## Related / Next
- [rag-vs-fine-tuning.md](rag-vs-fine-tuning.md)
- [rag-pipeline.md](rag-pipeline.md)
