# Query Transforms and Routing

Users don't write good search queries. Questions are vague ("how did that change go?"), compound ("compare Q1 and Q2 revenue and explain the drop"), or aimed at a data source your single index doesn't cover. Two families of techniques fix this *before* retrieval:

- **Query transforms** rewrite the question: hypothetical answers (HyDE), decomposition into sub-questions, step-by-step multi-hop queries.
- **Routing** picks which index, retriever or query engine should handle the question.

> Prerequisites: [03-retrievers-and-postprocessors](./03-retrievers-and-postprocessors.md), [../01-core/04-query-and-chat-engines](../01-core/04-query-and-chat-engines.md).

> **Status in TypeScript:** `RouterQueryEngine` and `SubQuestionQueryEngine` are built in. I did not find built-in HyDE or multi-step query transform classes in the LlamaIndex.TS API reference (Python has them), so those two are shown as small hand-written pieces. Check the reference for your version in case that has changed.

## HyDE (hypothetical document embeddings)

**Problem:** a short question and a long answer passage embed differently, so similarity between them is mediocre.

**Idea:** ask the LLM to write a *plausible answer* (it may be wrong in details), embed **that** as the search query, and retrieve real chunks similar to it. The hypothetical text sits closer in embedding space to real answer passages than the question does.

```ts
import { Settings, type NodeWithScore } from "llamaindex";

async function hydeRetrieve(
  retriever: { retrieve(p: { query: string }): Promise<NodeWithScore[]> },
  question: string,
): Promise<NodeWithScore[]> {
  const hypo = await Settings.llm.complete({
    prompt:
      `Write a short, factual-sounding passage that would answer the question below. ` +
      `It's fine if details are made up; match the style of technical documentation.\n\n` +
      `Question: ${question}`,
  });

  // search with the hypothetical passage instead of (or in addition to) the question
  return retriever.retrieve({ query: hypo.text });
}
```

When it helps: short or abstract questions over technical or long-form corpora, where plain query embeddings retrieve poorly.

Caveats:

- One extra LLM call per query.
- If the model has no idea about your domain, the hypothetical passage drifts and retrieval gets *worse*. Test it.
- Don't show the hypothetical text to users or feed it to the synthesizer as a source. It's only a search key, and it can contain invented facts.
- A safe variant: retrieve with both the question and the hypothetical passage and fuse the lists (see [05-query-fusion](./05-query-fusion.md)).

## Sub-questions

For compound questions, split into focused sub-questions, answer each against the right source, then synthesize. LlamaIndex.TS provides `SubQuestionQueryEngine`, which takes **tools**: each wraps a query engine with a name and description the LLM uses to decide which sub-question goes where.

```ts
import {
  QueryEngineTool,
  SubQuestionQueryEngine,
  VectorStoreIndex,
} from "llamaindex";

const lyftTool = new QueryEngineTool({
  queryEngine: lyftIndex.asQueryEngine(),
  metadata: {
    name: "lyft_10k",
    description: "Lyft annual report financials for 2021",
  },
});

const uberTool = new QueryEngineTool({
  queryEngine: uberIndex.asQueryEngine(),
  metadata: {
    name: "uber_10k",
    description: "Uber annual report financials for 2021",
  },
});

const engine = SubQuestionQueryEngine.fromDefaults({
  queryEngineTools: [lyftTool, uberTool],
});

const res = await engine.query({
  query: "Compare Uber's and Lyft's 2021 revenue growth.",
});
console.log(res.toString());
```

Under the hood: an LLM generates sub-questions tagged with a tool name; each runs on that tool's engine; a synthesizer combines the answers.

Use it when:

- the question genuinely spans several documents or indexes ("compare A and B"),
- or it has independent parts that can be answered separately.

Skip it when a single retrieval would do. It multiplies LLM calls (question generation, N sub-answers, synthesis), so latency and cost rise quickly. Good tool descriptions matter more than anything else here: the LLM routes sub-questions based on them.

## Multi-step queries

Some questions are **dependent**: the second search needs the first answer ("who founded the company that acquired X, and where did they study?"). Decomposition into independent sub-questions can't express that.

I did not find a dedicated multi-step query engine in the TypeScript package. The reliable approach is an agent loop (see `03-tools-and-agents/02-agents.md`) that calls your retrieval tools repeatedly, each call informed by earlier results. A minimal manual version:

```ts
async function multiHop(question: string, engine: { query(p: { query: string }): Promise<{ toString(): string }> }, maxSteps = 3) {
  let notes = "";
  for (let step = 0; step < maxSteps; step++) {
    const plan = await Settings.llm.complete({
      prompt:
        `Question: ${question}\nWhat we know so far:\n${notes || "(nothing)"}\n\n` +
        `If we can fully answer, reply "DONE". Otherwise reply with the single next search query.`,
    });
    const next = plan.text.trim();
    if (next.toUpperCase().startsWith("DONE")) break;

    const found = await engine.query({ query: next });
    notes += `Q: ${next}\nA: ${found.toString()}\n\n`;
  }
  const final = await Settings.llm.complete({
    prompt: `Using these notes, answer the question.\n\n${notes}\nQuestion: ${question}`,
  });
  return final.text;
}
```

Always cap the steps. Unbounded loops are a cost and reliability hazard.

## Routing

A router sends each question to the best of several engines. Typical splits: summary questions versus specific-fact questions, per-department indexes, or "documents" versus "SQL database".

```ts
import { RouterQueryEngine, SummaryIndex, VectorStoreIndex } from "llamaindex";

const vectorIndex = await VectorStoreIndex.fromDocuments(documents);
const summaryIndex = await SummaryIndex.fromDocuments(documents);

const router = RouterQueryEngine.fromDefaults({
  queryEngineTools: [
    {
      queryEngine: summaryIndex.asQueryEngine(),
      description: "Useful for summarization questions about the whole document",
    },
    {
      queryEngine: vectorIndex.asQueryEngine(),
      description: "Useful for retrieving specific facts and details",
    },
  ],
});

const res = await router.query({ query: "Give me a high-level summary." });
console.log(res.metadata?.selectorResult); // which engine was chosen, and why
```

How it works: a **selector** (by default LLM-based, `LLMSingleSelector` selects exactly one choice) reads each tool's `description` and picks one. The tool descriptions are effectively your routing prompt, so write them as distinguishing "use this when..." statements, and make them mutually exclusive. `selectorResult` in the response metadata shows the decision; log it.

Trade-offs:

- One extra LLM call per query.
- Misroutes fail silently: the user gets a confident answer from the wrong source. Evaluate routing accuracy separately from answer quality.
- If your choice rule is deterministic (the user picked a workspace, the URL names a product), route in plain code instead of paying an LLM to guess.

## Choosing a technique

| Symptom | Try |
|---|---|
| Short or vague questions retrieve poorly | HyDE, or multi-query + fusion |
| One question hides several asks | `SubQuestionQueryEngine` |
| Each step depends on the previous answer | Agent loop / multi-step |
| Multiple sources with different purposes | `RouterQueryEngine` (or deterministic routing) |
| Follow-ups ("what about last year?") in chat | A chat engine that condenses the question (see `../01-core/04-query-and-chat-engines.md`) |

## Common mistakes

**Stacking all of them by default.** Each adds LLM calls and failure modes. Add one only when you can show the failure it fixes.

**Vague tool descriptions.** Routers and sub-question engines are only as good as the descriptions.

**Overlapping descriptions** that make routing a coin flip.

**Treating HyDE output as evidence.** It is a search key, never a source.

**No step cap** in multi-step loops.

**Ignoring added latency.** Several sequential LLM calls before retrieval even starts can double response time.

## Debugging

- Log the transformed query, generated sub-questions, and router choice for every request.
- Compare retrieval results with and without the transform for the same questions.
- For routers, keep a small labelled set of questions with the correct engine and measure accuracy.
- If sub-questions look off, improve tool names and descriptions first, then check the LLM's capability.

## Quick Summary

- Query transforms rewrite the question; routing chooses where it goes.
- `SubQuestionQueryEngine` (with `QueryEngineTool`) and `RouterQueryEngine` are built in; HyDE and multi-step are small hand-written helpers in TypeScript.
- HyDE retrieves with a hypothetical answer; never present it as a source.
- Sub-questions suit independent parts; dependent hops need an agent or bounded loop.
- Routing quality is entirely in the tool descriptions; log `selectorResult`.
- Add transforms only when evaluation shows a specific failure they fix.

## Next

[07-advanced-retrieval-patterns.md](./07-advanced-retrieval-patterns.md): sentence-window, small-to-big and auto-merging, recursive retrieval, and graph approaches.
