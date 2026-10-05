# Setup and First Query

LlamaIndex.TS is a data framework for LLM apps. You give it your data, it splits and embeds that data into an index, and at question time it retrieves the relevant pieces and passes them to an LLM. This note covers the minimum setup to get from zero to a working query, plus the configuration surface (`Settings`) that every later note relies on.

> **Version note:** Written against the `llamaindex` 0.12.x line, which uses the split-package layout (core package plus `@llamaindex/*` provider packages). Pin your own version and check the [official docs](https://developers.llamaindex.ai/typescript/framework/) if an import fails. Many older blog posts and tutorials predate this layout.

## Prerequisites

- Node.js 20 or newer (the `llamaindex` npm page lists Node >= 20).
- TypeScript, ideally with a runner like `tsx`.
- An API key for your LLM and embedding provider. The examples use OpenAI.

## Install

The core package does not bundle model providers. You install those separately.

```bash
npm init -y
npm i llamaindex @llamaindex/openai
npm i -D typescript tsx @types/node
```

Package map for now:

| Package | What it gives you |
|---|---|
| `llamaindex` | `Document`, `VectorStoreIndex`, `Settings`, node parsers, query and chat engines |
| `@llamaindex/openai` | `OpenAI` (LLM), `OpenAIEmbedding` |
| `@llamaindex/ollama` | Local LLM and embeddings via Ollama |
| `@llamaindex/huggingface` | Local embeddings (`HuggingFaceEmbedding`) |
| `@llamaindex/workflow` | Workflows (used in the agents section) |

If a tutorial does `import { OpenAI } from "llamaindex"`, it is from the older layout.

`tsconfig.json`, using the settings the docs call out:

```json
{
  "compilerOptions": {
    "moduleResolution": "bundler",
    "lib": ["DOM.AsyncIterable"],
    "target": "es2020",
    "module": "esnext"
  }
}
```

`moduleResolution` can also be `nodenext`, `node16` or `node`. The `DOM.AsyncIterable` lib is needed for Web Stream types, which matter once you stream responses.

Set your key in the environment:

```bash
export OPENAI_API_KEY=your-api-key
# or put it in .env and run: node --env-file .env ...
```

## Core idea: `Settings`

`Settings` is a global configuration object. Every component (index, query engine, node parser) reads its defaults from it unless you pass something explicitly.

```ts
import { Settings } from "llamaindex";
import { OpenAI, OpenAIEmbedding } from "@llamaindex/openai";

Settings.llm = new OpenAI({ model: "gpt-4o-mini", temperature: 0 });
Settings.embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });
Settings.chunkSize = 512;
Settings.chunkOverlap = 50;
```

| Setting | Used for |
|---|---|
| `Settings.llm` | Answer synthesis, agents, query transforms |
| `Settings.embedModel` | Embedding nodes at index time and queries at search time |
| `Settings.nodeParser` | How documents are split into nodes (default `SentenceSplitter`) |
| `Settings.chunkSize` / `chunkOverlap` | Shortcut that configures the default node parser |

The model names above are examples. Use whichever your account has access to.

If you don't set `Settings.embedModel`, the docs say the default is OpenAI's `text-embedding-ada-002`. Don't rely on that: set the embedding model explicitly (see "Common mistakes").

## First index and query

```ts
// main.ts
import fs from "node:fs/promises";
import { Document, Settings, VectorStoreIndex } from "llamaindex";
import { OpenAI, OpenAIEmbedding } from "@llamaindex/openai";

Settings.llm = new OpenAI({ model: "gpt-4o-mini" });
Settings.embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });

async function main() {
  const text = await fs.readFile("./data/essay.txt", "utf-8");
  const document = new Document({ text, id_: "essay" });

  // split -> embed -> store in an in-memory vector index
  const index = await VectorStoreIndex.fromDocuments([document]);

  const queryEngine = index.asQueryEngine();
  const response = await queryEngine.query({
    query: "What did the author do growing up?",
  });

  console.log(response.toString());
}

main();
```

```bash
npx tsx main.ts
```

What each step does:

1. `new Document({ text, id_ })` wraps raw text. Give it a stable `id_` so you can identify it later (this matters for updates and deduplication).
2. `VectorStoreIndex.fromDocuments` runs the whole indexing path: it splits the document into nodes with `Settings.nodeParser`, embeds each node with `Settings.embedModel`, and stores the vectors. By default the store is in memory.
3. `asQueryEngine()` wraps the index in a retrieve-then-synthesize pipeline.
4. `query({ query })` takes an object, not a bare string.

## How it works

```text
Document(s)
   │  node parser (SentenceSplitter, chunkSize/chunkOverlap)
   ▼
Nodes (chunks + inherited metadata)
   │  embedModel
   ▼
VectorStoreIndex  ◄──────────── in-memory by default

query("...")
   │  embedModel  (same model as above)
   ▼
Retriever: top-k most similar nodes
   │
   ▼
Response synthesizer: prompt = question + retrieved nodes → llm
   │
   ▼
Response (text + source nodes)
```

A normal RAG run makes two kinds of embedding calls: one per chunk at index time and one per query at question time. Both must use the same embedding model, or the similarity scores are meaningless.

To see what grounded an answer, inspect the source nodes on the response:

```ts
for (const n of response.sourceNodes ?? []) {
  console.log(n.score, n.node.id_);
}
```

## Using local models

The LLM and the embedding model are configured independently:

```bash
npm i @llamaindex/ollama @llamaindex/huggingface
```

```ts
import { Settings } from "llamaindex";
import { ollama } from "@llamaindex/ollama";
import { HuggingFaceEmbedding } from "@llamaindex/huggingface";

Settings.llm = ollama({ model: "<a-model-you-pulled-in-ollama>" });
Settings.embedModel = new HuggingFaceEmbedding({
  modelType: "BAAI/bge-small-en-v1.5",
});
```

The docs recommend a fairly capable model for agentic work, since small models handle tool calling poorly. For plain RAG a smaller model is usually fine. Check the provider pages for exact options.

## Common mistakes

**Swapping the LLM but not the embedding model.** Setting `Settings.llm = ollama(...)` does not change embeddings. Without an explicit `Settings.embedModel`, LlamaIndex still calls OpenAI to embed your data. That means a surprise API key error, or a "local-only" setup quietly sending data to a remote API. Set both.

**Changing the embedding model without reindexing.** Vectors from different models (or with different dimensions) are not comparable. If you change `embedModel`, rebuild the index. This matters a lot once vectors are persisted (see [03-indexes-and-storage](./03-indexes-and-storage.md)).

**Copying old tutorials.** Symptoms: `OpenAI` not exported from `llamaindex`, `ServiceContext` imports, `serviceContextFromDefaults`. Current code configures models through `Settings` and imports providers from `@llamaindex/*`.

**Passing a string to `query`.** The call shape is `queryEngine.query({ query: "..." })`.

**Reading the key too late.** `OPENAI_API_KEY` must be in the environment before the first call. With `node` or `tsx`, use `--env-file .env` (or load it yourself) rather than assuming `.env` is read automatically.

**Non-Node runtimes.** On Vercel Edge or Cloudflare Workers, some classes (certain vector stores and readers) are not exported from the top-level entry and must be imported by file path. If a class is "missing" only in the edge runtime, this is why. Check the package README for the current list.

## Debugging

- **Turn on library debug logging:** `Settings.debug = true`, or set the `DEBUG=llamaindex` environment variable.
- **Weak or off-topic answer:** print `response.sourceNodes` first. If the retrieved chunks are wrong, the problem is chunking, retrieval or embeddings, not the LLM.
- **Empty or irrelevant retrieval:** check that the text actually loaded (log `document.text.length`) and that index-time and query-time embeddings come from the same model.
- **Auth or 401 errors:** confirm which provider is really being called. The default embedding model is OpenAI even if your LLM isn't.
- **Slow first run:** indexing makes one embedding call per chunk, and smaller chunks mean more calls. Persist the index so you don't pay this on every start (see [03-indexes-and-storage](./03-indexes-and-storage.md)).

## Quick Summary

- Install `llamaindex` plus a provider package (`@llamaindex/openai`, `@llamaindex/ollama`, ...). Providers are not bundled.
- `Settings` is global config: `llm`, `embedModel`, `nodeParser`, `chunkSize`, `chunkOverlap`.
- `VectorStoreIndex.fromDocuments` does split → embed → store; `asQueryEngine().query({ query })` does retrieve → synthesize.
- Always set `Settings.embedModel` explicitly, and reindex whenever you change it.
- When an answer is bad, look at `response.sourceNodes` before blaming the LLM.
- Treat tutorials that import `OpenAI` from `llamaindex` or use `ServiceContext` as outdated.

## Next

[02-documents-and-nodes.md](./02-documents-and-nodes.md): what `fromDocuments` is doing to your text, how chunking works, and how metadata travels with nodes.
