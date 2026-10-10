# Response Synthesizer

## Concept
The **response synthesizer** is the generation half of a query engine. It takes the question and the retrieved nodes, builds prompts, calls the LLM, and returns the answer. Its **response mode** decides how many LLM calls are made and how the context is combined.

## Prerequisites
- [query-engine.md](query-engine.md)
- `10-context-and-generation/context-window-and-budget.md`

## Usage
```ts
import { getResponseSynthesizer } from "llamaindex";             // verify; architecture.md shows a ResponseSynthesizer class form
import { RetrieverQueryEngine } from "llamaindex/engines";       // verify path

const synthesizer = getResponseSynthesizer("compact");           // verify signature

// Standalone
const nodes = await retriever.retrieve({ query: "What is the refund policy?" });
const response = await synthesizer.synthesize({ query: "What is the refund policy?", nodes });   // verify argument shape
console.log(response.toString());

// Inside a query engine (the mode is set through the synthesizer in TS)
const engine = new RetrieverQueryEngine({ retriever, responseSynthesizer: synthesizer });
```
Python's `index.as_query_engine(response_mode="compact")` shortcut is not a verified option in TS; build the synthesizer and pass it in.

## Response Modes

LlamaIndex.TS documents fewer modes than Python. The documented ones (verify against your version) are `compact`, `refine`, `tree_summarize` and `multi_modal` (image input; out of scope here). The rest exist in Python only; the last column says what to do in TS.

| Mode | How it works | LLM calls | Use when | In LlamaIndex.TS? |
|---|---|---|---|---|
| **`compact`** (default) | Packs as many chunks as fit into each prompt, then refines across prompts if needed | Few | General Q&A; best default | Yes |
| **`refine`** | Answers from the first chunk, then refines with each next chunk | One per chunk | Detailed answers where each chunk matters; slower and costlier | Yes |
| **`tree_summarize`** | Summarizes chunk groups, then summarizes the summaries, recursively | Several | Summarization over many chunks | Yes |
| **`simple_summarize`** | Truncates everything to fit one prompt | One | Quick and cheap; may drop content | Not documented (verify); see recipes below |
| **`accumulate`** | Answers the question separately per chunk and concatenates | One per chunk | When you want per-chunk answers | Not documented; hand-roll |
| **`compact_accumulate`** | Like `accumulate`, with chunks compacted | Fewer | Per-chunk answers, lower cost | Not documented; hand-roll |
| **`no_text`** | Retrieves only; no LLM call | None | Debugging retrieval (inspect `sourceNodes`) | Not documented; call `retriever.retrieve` directly |
| **`context_only`** | Returns the concatenated context | None | Pass the context to your own LLM call | Not documented; join node text yourself |
| **`generation`** | Ignores context; plain LLM answer | One | Comparison baseline | Not documented; call `Settings.llm.complete` directly |

Mode names and availability can change between versions (verify against your install).

### Hand-rolled equivalents (unverified, not compiled)
```ts
import { MetadataMode } from "@llamaindex/core/schema";
import { Settings } from "llamaindex";

// no_text: retrieval only
const retrieved = await retriever.retrieve({ query });
retrieved.forEach((n) => console.log(n.score, n.node.getContent(MetadataMode.NONE).slice(0, 80)));

// context_only: concatenated context for your own LLM call
const context = retrieved.map((n) => n.node.getContent(MetadataMode.LLM)).join("\n\n");

// generation: baseline with no context
const baseline = await Settings.llm.complete({ prompt: query });      // verify complete() signature and result field

// accumulate: one answer per chunk
const perChunk = await Promise.all(
  retrieved.map((n) =>
    Settings.llm.complete({
      prompt: `Context:\n${n.node.getContent(MetadataMode.LLM)}\n\nQuestion: ${query}\nAnswer:`,
    }),
  ),
);
console.log(perChunk.map((r, i) => `Response ${i + 1}: ${r.text}`).join("\n\n"));
```
For `accumulate` over many chunks, limit concurrency (see the note in [transformations.md](transformations.md)).

## Choosing a Mode

| Situation | Mode |
|---|---|
| Typical question answering | `compact` |
| "Summarize this document" | `tree_summarize` |
| Debug retrieval without LLM cost | `retriever.retrieve` directly (Python: `no_text`) |
| Custom prompting outside LlamaIndex | Join node text yourself (Python: `context_only`) |
| You must use every retrieved chunk faithfully | `refine` |

## Prompt Templates
Each mode uses templates you can inspect and replace.

```ts
import { PromptTemplate, getResponseSynthesizer } from "llamaindex";

const qaPrompt = new PromptTemplate({
  templateVars: ["context", "query"],
  template:
    "Context information is below.\n---------------------\n{context}\n---------------------\n" +
    "Using only the context above, answer the question. If the answer is not in the context, say so.\n" +
    "Question: {query}\nAnswer: ",
});                                                                    // verify variable names; Python uses {context_str} and {query_str}

const synthesizer = getResponseSynthesizer("compact", { textQATemplate: qaPrompt });   // verify option name and whether a function is expected
```

| Template | Used for |
|---|---|
| `textQATemplate` | First (or only) answer prompt |
| `refineTemplate` | Refining an existing answer with new context |
| `summaryTemplate` | Summarization modes (verify name) |

Python names these `text_qa_template`, `refine_template`, `summary_template`. Check the exact keys with `synthesizer.getPrompts()` (verify).

## How Context Gets Split
The synthesizer respects the LLM's context window. In `compact` mode, nodes are packed into as few prompts as fit. If total context exceeds one prompt, extra calls refine the answer. This is why a large `similarityTopK` increases latency and cost: more context may mean more LLM calls.

## Cost and Latency
- Calls scale with context size and mode.
- `refine` (and a hand-rolled `accumulate`) make one call per chunk.
- Rerank and keep fewer, better chunks to cut cost, instead of feeding everything.

## Structured and Cited Output
For structured outputs (JSON, schema-validated objects, for example with `zod`) and for citations, see `10-context-and-generation/grounded-generation.md`.

## Important Rules
- The synthesizer cannot recover information that was never retrieved.
- Retrieve without synthesis (`retriever.retrieve`) to separate "retrieval problem" from "generation problem."
- Instruct the model in the prompt to say "I don't know" when context is insufficient.

## Common Mistakes
- Switching to `refine` for quality when the real issue is retrieval.
- A huge `topK` with `refine`, creating dozens of LLM calls.
- Overriding the QA template and forgetting the context or query placeholder.
- Not grounding the prompt, so the model answers from general knowledge.
- Passing a Python-only mode name (`no_text`, `accumulate`, ...) and assuming TS supports it.

## Related / Next
- [query-pipeline.md](query-pipeline.md)
- `10-context-and-generation/prompt-design.md`
- `10-context-and-generation/grounded-generation.md`
