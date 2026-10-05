# Query and Chat Engines

An index stores data; an **engine** turns it into answers. A *query engine* answers one question at a time. A *chat engine* keeps conversation history and retrieves context for each turn. Both are thin pipelines over the same two parts: a **retriever** (find nodes) and a **response synthesizer** (ask the LLM using those nodes).

> Prerequisites: [01-setup-and-first-query](./01-setup-and-first-query.md), [03-indexes-and-storage](./03-indexes-and-storage.md).

## Query engine

```ts
const queryEngine = index.asQueryEngine();
const response = await queryEngine.query({ query: "What is our refund window?" });

console.log(response.toString());
```

A query engine wraps a `Retriever` and a `ResponseSynthesizer`: it uses the query to fetch nodes, then sends them to the LLM. It is stateless. Every call is independent, so "and what about for digital goods?" means nothing to it.

```text
query ─► retriever (top-k nodes) ─► [postprocessors] ─► synthesizer ─► LLM ─► response
                                                                          └► sourceNodes
```

### Configuring it

`asQueryEngine` accepts options for the common cases:

```ts
const queryEngine = index.asQueryEngine({
  similarityTopK: 5, // only used when you don't pass your own retriever
});
```

Other options on `VectorStoreIndex.asQueryEngine` include `retriever`, `responseSynthesizer`, `preFilters`, `nodePostprocessors` and `customParams`. Filters and postprocessors (similarity cutoff, rerankers) are covered in `02-rag/03-retrievers-and-postprocessors.md`.

For full control, compose the engine yourself:

```ts
import { RetrieverQueryEngine, getResponseSynthesizer } from "llamaindex";

const retriever = index.asRetriever({ similarityTopK: 8 });
const synthesizer = getResponseSynthesizer("tree_summarize");

const queryEngine = new RetrieverQueryEngine(retriever, synthesizer);
```

Constructor argument shapes have changed across versions, so check the API reference for yours.

### The response object

```ts
const response = await queryEngine.query({ query });

response.toString();      // the answer text
response.sourceNodes;     // retrieved nodes with scores (when available)
```

Always look at the source nodes when an answer is wrong: they tell you whether retrieval or generation failed.

## Response synthesis

The synthesizer decides how retrieved chunks are fed to the LLM.

| Mode | How it works | Use when |
|---|---|---|
| `compact` (default; `CompactAndRefine`) | Stuffs as many chunks as fit into each prompt, refines across prompts if needed | Most cases: fewer LLM calls |
| `refine` | One LLM call per node, refining the answer each time | Maximum detail, accepts higher cost and latency |
| `tree_summarize` | Builds an answer recursively over chunks | Summaries over many chunks |

```ts
import { getResponseSynthesizer } from "llamaindex";

const queryEngine = index.asQueryEngine({
  responseSynthesizer: getResponseSynthesizer("compact"),
});
```

More retrieved nodes means more tokens and, in `refine`, more calls. Raising `similarityTopK` is not free.

## Streaming query responses

Add `stream: true` and consume an async iterable:

```ts
const stream = await queryEngine.query({
  query: "Summarize the onboarding policy",
  stream: true,
});

for await (const chunk of stream) {
  process.stdout.write(chunk.response);
}
```

Notes:

- Each chunk is a piece of generated text in `chunk.response`.
- You need `lib: ["DOM.AsyncIterable"]` in `tsconfig.json` (see note 01) or TypeScript will complain about iterating the stream.
- If the engine makes several LLM calls (for example `refine`), you effectively see output as the final answer is generated; streaming helps latency, not total cost.
- If you need the full text afterwards, accumulate the chunks yourself.

## Chat engine

```ts
const chatEngine = index.asChatEngine();

const first = await chatEngine.chat({ message: "What is our refund window?" });
console.log(first.response);

const followUp = await chatEngine.chat({ message: "Does that apply to digital goods?" });
console.log(followUp.response);
```

`index.asChatEngine()` returns a `ContextChatEngine`: for each message it retrieves relevant context and passes that, plus the conversation history, to the LLM. Options can be passed in:

```ts
const chatEngine = index.asChatEngine({
  similarityTopK: 5,
  systemPrompt: "You answer questions about company policy. Say when the context doesn't cover it.",
});
```

Streaming works the same way:

```ts
const stream = await chatEngine.chat({ message: "Summarize that in two lines", stream: true });
for await (const chunk of stream) {
  process.stdout.write(chunk.response);
}
```

You can construct a context chat engine from any retriever:

```ts
import { ContextChatEngine } from "llamaindex";

const chatEngine = new ContextChatEngine({ retriever: index.asRetriever() });
```

### Chat engine types

| Engine | Behavior |
|---|---|
| `ContextChatEngine` | Retrieves context for each message, then chats with history. Default for `asChatEngine()` and the usual RAG choice |
| `CondenseQuestionChatEngine` | Rewrites each message plus history into a standalone question, then runs it through a query engine |
| `SimpleChatEngine` | Plain LLM chat with history, no retrieval |

Use `ContextChatEngine` unless you have a reason not to. `CondenseQuestionChatEngine` is useful when follow-ups are heavily pronoun-based ("what about the second one?") and your retriever does poorly on them, at the cost of an extra LLM call per turn.

### History and state

The conversation history lives **inside the engine instance** and is available as `chatEngine.chatHistory`. Two consequences:

- **One engine instance per conversation.** Sharing a single engine across users in a server leaks history between them. Create an engine per session, or persist history yourself and pass it back in (`chat` accepts a `chatHistory` option).
- History grows every turn, and it is sent to the LLM each time. Long chats cost more and eventually hit context limits, so cap or summarize history in long-running sessions.

## Query engine or chat engine?

| You need | Use |
|---|---|
| Single question, API endpoint, batch evaluation | Query engine |
| Follow-up questions that refer to earlier turns | Chat engine |
| Tools, multiple data sources, multi-step reasoning | An agent (see `03-tools-and-agents/02-agents.md`) |

You can also expose a query engine as a tool for an agent; see `03-tools-and-agents/01-tools.md`.

## Common mistakes

**Expecting memory from a query engine.** It has none. Follow-ups need a chat engine or your own question rewriting.

**Reusing one chat engine for every user.** History is shared state.

**Blaming the LLM for retrieval failures.** If `sourceNodes` don't contain the answer, no synthesis mode will fix it. Look at chunking, `similarityTopK`, filters and embeddings first.

**Cranking `similarityTopK` to "be safe".** More context dilutes the prompt, raises cost, and with `refine` multiplies LLM calls. Add a similarity cutoff or reranker instead.

**Using `refine` by default.** It makes one call per node. Start with `compact`.

**Passing a string to `query` or `chat`.** Both take an object: `{ query }` and `{ message }`.

**Streaming types failing to compile.** Add `DOM.AsyncIterable` to `lib`.

## Debugging

- Print `response.sourceNodes` (scores and text) for the failing question. If the right chunk is missing or low-scored, work upstream.
- Enable `Settings.debug = true` to see what is sent to the LLM.
- For chat, print `chatEngine.chatHistory` to confirm what the model actually sees.
- If answers ignore your system prompt, confirm you passed it to the engine you are actually calling.
- Stream output cuts off or never arrives: check that your LLM provider supports streaming and that your HTTP layer (proxy, serverless runtime) isn't buffering the response.

## Quick Summary

- A query engine = retriever + synthesizer; stateless; `query({ query })`.
- A chat engine adds history and per-turn retrieval; `chat({ message })`; `index.asChatEngine()` gives a `ContextChatEngine`.
- Synthesis modes: `compact` (default), `refine` (one call per node), `tree_summarize`.
- Streaming: pass `stream: true` and iterate `chunk.response` with `for await`.
- Keep one chat engine per conversation and manage history growth.
- When answers are wrong, inspect `sourceNodes` before touching the prompt or model.

## Next

`02-rag/01-readers-and-llamaparse.md`: getting real files, web pages and database rows into `Document`s, including LlamaParse for complex PDFs.
