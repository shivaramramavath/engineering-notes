# Query Engine

## Concept
A **query engine** takes a natural-language question and returns an answer. It wires together a retriever, optional node postprocessors, and a response synthesizer.

```
question ─► Retriever ─► Node postprocessors ─► Response synthesizer (LLM) ─► Response
```

Query engines are single-shot (one question, one answer). For multi-turn conversation use chat engines (`12-chat/chat-engines.md`).

## Prerequisites
- [indexes.md](indexes.md)
- `07-retrievers/README.md`

## Quick Start
```ts
const engine = index.asQueryEngine({ similarityTopK: 5 });
const response = await engine.query({ query: "What is the vacation policy?" });

console.log(response.toString());                         // answer text
for (const n of response.sourceNodes ?? []) {             // evidence; verify property name
  console.log(n.score, n.node.metadata, n.node.getContent(MetadataMode.NONE).slice(0, 100));
}
```
`MetadataMode` comes from `@llamaindex/core/schema` (see [nodes-and-parsers.md](nodes-and-parsers.md)). The query argument is an object `{ query }`; older docs show a plain string (verify).

## The Response Object
| Field | Contents |
|---|---|
| `response.toString()` | Generated answer (docs also show `response.message.content`; verify) |
| `response.sourceNodes` | Retrieved `NodeWithScore` objects used as context (verify name) |
| `response.metadata` | Extra info (for example node IDs and metadata by ID; verify) |

Always look at `sourceNodes` when judging quality. Wrong answers are most often wrong retrieval.

## Common Options on `asQueryEngine`
```ts
const engine = index.asQueryEngine({
  similarityTopK: 10,                          // candidates retrieved
  responseSynthesizer,                         // see response-synthesizer.md (response mode lives here in TS)
  preFilters: metadataFilters,                 // restrict by metadata (verify option name and filter shape)
  nodePostprocessors: [/* ... */],             // filter / rerank / transform nodes
});
```
Unlike Python, there is no `response_mode` or `streaming` option on `asQueryEngine` that this repo has verified: choose the mode by building a synthesizer, and stream per call (see below). Options are passed through to the retriever, synthesizer or engine; exact option names vary by index type and version (verify).

## Building Explicitly
Import paths vary by version; verify:
```ts
import { RetrieverQueryEngine } from "llamaindex/engines";
import { SimilarityPostprocessor } from "llamaindex/postprocessors";
import { getResponseSynthesizer } from "llamaindex";             // verify; architecture.md shows a ResponseSynthesizer class form

const retriever = index.asRetriever({ similarityTopK: 10 });
const engine = new RetrieverQueryEngine({
  retriever,
  responseSynthesizer: getResponseSynthesizer("compact"),       // verify
  nodePostprocessors: [new SimilarityPostprocessor({ similarityCutoff: 0.4 })],
});
```
Swap in any retriever from `07-retrievers/` here (BM25, fusion, auto-merging) without changing the rest. Several of those are hand-rolled in this repo; a hand-rolled retriever extends `BaseRetriever` and plugs in the same way. Routers work at the query-engine level in TS (`RouterQueryEngine`).

## Node Postprocessors
Run after retrieval and before synthesis; can filter, reorder or rewrite nodes.

| Postprocessor | Does | In LlamaIndex.TS? |
|---|---|---|
| `SimilarityPostprocessor` | Drops nodes below a score cutoff | Yes |
| `KeywordNodePostprocessor` | Requires or excludes keywords | Not documented (verify); filter nodes in a small custom postprocessor instead |
| `MetadataReplacementPostProcessor` | Swaps in a sentence window | Yes (verify exact class name casing) |
| `SentenceTransformerRerank` / `CohereRerank` | Rerank by relevance (`09-reranking/`) | `CohereRerank` yes (`@llamaindex/cohere`); `SentenceTransformerRerank` not available (local Python models) |
| `LongContextReorder` | Reorders nodes to reduce "lost in the middle" (`10-context-and-generation/context-ordering.md`) | Not documented; hand-roll (see below) |
| `SentenceEmbeddingOptimizer` | Prunes sentences within nodes (`10-context-and-generation/context-compression.md`) | Not documented (verify) |

Typical pattern: retrieve a wide set (top 20 to 50), rerank, keep the best few.

A custom postprocessor is any object with a `postprocessNodes` method (verify the interface in your version). Unverified sketch of a hand-rolled long-context reorder, which puts the best nodes at the edges:
```ts
import type { NodeWithScore } from "@llamaindex/core/schema";

class LongContextReorderLite {
  async postprocessNodes(nodes: NodeWithScore[]): Promise<NodeWithScore[]> {
    const sorted = [...nodes].sort((a, b) => (b.score ?? 0) - (a.score ?? 0));
    const front: NodeWithScore[] = [];
    const back: NodeWithScore[] = [];
    sorted.forEach((n, i) => (i % 2 === 0 ? front : back).push(n));
    return [...front, ...back.reverse()];
  }
}
```

## Async and Streaming
All TS query calls are already async (`await engine.query(...)`); there is no separate `aquery`.

```ts
const stream = await engine.query({ query: "...", stream: true });    // verify option
for await (const chunk of stream) {
  process.stdout.write(chunk.toString());                              // verify chunk shape (chunk.message.content or chunk.response)
}
```
See `10-context-and-generation/streaming.md`.

## Special Query Engines
Built on the same interface, so they plug in where a normal engine does:

| Engine | Purpose | In LlamaIndex.TS? |
|---|---|---|
| `SubQuestionQueryEngine` | Break a complex question into sub-questions across tools (`08-query-transformation/sub-question-query-engine.md`) | Yes |
| `RouterQueryEngine` | Choose among engines per query | Yes (with selectors such as `LLMSingleSelector`) |
| `TransformQueryEngine` | Apply a query transform (HyDE) before retrieval (`08-query-transformation/hyde.md`) | Not documented (verify); hand-roll by transforming the query yourself, then calling the engine |
| `CitationQueryEngine` | Answers with inline source citations (`10-context-and-generation/grounded-generation.md`) | Not documented (verify); hand-roll with a prompt that numbers the sources |

Hand-rolled transform wrapper (unverified, not compiled):
```ts
import { Settings } from "llamaindex";

async function queryWithRewrite(engine: ReturnType<typeof index.asQueryEngine>, question: string) {
  const res = await Settings.llm.complete({
    prompt: `Rewrite this question as a standalone search query:\n${question}`,
  });                                                                  // verify complete() signature and result field
  return engine.query({ query: res.text });
}
```

## Customizing Prompts
```ts
import { PromptTemplate } from "llamaindex";

const qaPrompt = new PromptTemplate({
  templateVars: ["context", "query"],
  template:
    "Answer using ONLY the context below. If the context is insufficient, say you don't know.\n" +
    "Context:\n{context}\n\nQuestion: {query}\nAnswer: ",
});                                                                    // verify variable names; Python uses {context_str} and {query_str}

engine.updatePrompts({ "responseSynthesizer:textQATemplate": qaPrompt });   // verify key format
console.log(Object.keys(engine.getPrompts()));                              // discover the prompt names
```
Some TS versions take the QA template as a function `({ context, query }) => string` instead of a `PromptTemplate`; check the type of the built-in default in your version. Prompt design is covered in `10-context-and-generation/prompt-design.md`.

## Important Rules
- Inspect `sourceNodes` for every quality investigation.
- Set `similarityTopK` deliberately. The default is small (verify; commonly 2).
- Use the same engine configuration in tests and production, or evaluation results mean little.

## Common Mistakes
- Judging only the answer text and never the retrieved nodes.
- Leaving default `topK` and wondering why answers miss information.
- Using a query engine for chat; there is no memory.
- Adding postprocessors that silently remove everything (a high cutoff), producing "I don't know".
- Passing a Python-style `responseMode` or `streaming` option to `asQueryEngine` and having it silently ignored.

## Related / Next
- [response-synthesizer.md](response-synthesizer.md)
- `07-retrievers/README.md`
- `12-chat/chat-engines.md`
