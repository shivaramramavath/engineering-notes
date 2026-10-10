# Router Retriever (Query Routing)

## Concept
A **router** chooses, per query, which retriever (or query engine) to use. Different questions need different tools: factual lookups want a vector retriever, "summarize this document" wants a summary retriever, "how are these entities related" wants a graph retriever.

```
                 ┌─► VectorRetriever      (specific facts)
question ─► Selector (LLM) ─► SummaryRetriever   (whole-document)
                 └─► GraphRetriever       (relationships)
```

In LlamaIndex.TS the documented router works on **query engines** (`RouterQueryEngine` with `LLMSingleSelector`). A retriever-level `RouterRetriever` is not documented, so this file shows the query-engine router and then a small hand-rolled retriever router. In this repo's chooser the router is "Yes" only in the query-engine form ([README.md](README.md)).

This file also covers **query routing** in general (the topic previously in `09-query-transformation/query-routing.md`).

> **Accuracy note.** Code here was NOT compiled or run. Import paths and constructor shapes vary between LlamaIndex.TS versions (verify). The property graph route in the diagram is not available in LlamaIndex.TS ([property-graph-retriever.md](property-graph-retriever.md)).

## When to Use
- Several data sources or indexes with different strengths.
- Mixed question types (lookup, summary, comparison).
- Separate collections per department or product, and the question implies which.

## When Not to Use
- One index answers everything well.
- The route can be decided by simple rules (cheaper and more predictable than an LLM selector).

## Prerequisites
- [retrieval-basics.md](retrieval-basics.md)
- [summary-retriever.md](summary-retriever.md)

## Router Retriever
**Not documented in LlamaIndex.TS.** Python's `RouterRetriever` (selector plus `RetrieverTool`s) has no documented TypeScript counterpart. Do not assume `RouterRetriever` or `RetrieverTool` can be imported. Two options: route whole query engines (next section), or hand-roll the router as a custom retriever ([custom-retriever.md](custom-retriever.md)):

```ts
import { Settings } from "llamaindex";
import { BaseRetriever } from "@llamaindex/core/retriever";            // verify path
import type { NodeWithScore, QueryBundle } from "@llamaindex/core";   // verify path

interface RetrieverRoute {
  retriever: BaseRetriever;
  description: string;   // the only thing the LLM sees
}

// Hand-rolled and unverified: not a LlamaIndex.TS feature
class LLMRouterRetriever extends BaseRetriever {
  constructor(private routes: RetrieverRoute[], private defaultRoute = 0) {
    super();
  }

  private async choose(query: string): Promise<number> {
    const menu = this.routes.map((r, i) => `${i}: ${r.description}`).join("\n");
    const prompt =
      `Pick the single best tool for the question. Reply with ONLY the number.\n\n` +
      `Tools:\n${menu}\n\nQuestion: ${query}\nAnswer:`;
    const res = await Settings.llm.complete({ prompt });                // verify: complete() params and result shape
    const idx = Number.parseInt(res.text.trim(), 10);
    const valid = Number.isInteger(idx) && idx >= 0 && idx < this.routes.length;
    console.log(JSON.stringify({ query, raw: res.text, chosen: valid ? idx : this.defaultRoute }));   // log every decision
    return valid ? idx : this.defaultRoute;                              // default route on unparseable output
  }

  async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
    const query = typeof params === "string" ? params : params.query;
    const idx = await this.choose(query);
    return this.routes[idx].retriever.retrieve({ query });
  }
}

const router = new LLMRouterRetriever(
  [
    {
      retriever: vectorIndex.asRetriever({ similarityTopK: 5 }),
      description: "Useful for answering specific factual questions about policy details.",
    },
    {
      retriever: summaryIndex.asRetriever(),
      description: "Useful for summarizing an entire document or getting a broad overview.",
    },
  ],
  0,
);
const nodes = await router.retrieve({ query: "Give me a high-level overview of the handbook." });
```
This selects one route with plain prompting. It has no structured-output guarantees and no multi-select; treat it as a teaching sketch.

## Router Query Engine (route to whole engines)
This is the documented LlamaIndex.TS router. Names and options below are from the docs and memory (verify against your version).
```ts
import { RouterQueryEngine, LLMSingleSelector } from "llamaindex";   // verify exports

const engine = RouterQueryEngine.fromDefaults({                        // verify: fromDefaults vs constructor
  selector: new LLMSingleSelector({ llm: Settings.llm }),              // verify constructor options
  queryEngineTools: [
    { queryEngine: vectorEngine, description: "Specific facts." },
    { queryEngine: summaryEngine, description: "Whole-document summaries." },
  ],
});

const response = await engine.query({ query: "Give me a high-level overview of the handbook." });
console.log(response.toString());
```
Here `vectorEngine` is typically `vectorIndex.asQueryEngine()` and `summaryEngine` is `summaryIndex.asQueryEngine(...)` ([summary-retriever.md](summary-retriever.md)). Use the hand-rolled retriever form when you want only retrieval; use the query-engine form when each route should also decide how to synthesize the answer.

## Selectors

| Selector | Behavior | In LlamaIndex.TS? |
|---|---|---|
| `LLMSingleSelector` | LLM picks one route | Yes (documented with `RouterQueryEngine`) |
| `LLMMultiSelector` | LLM may pick several; results are combined | Not documented; check your version's exports (verify) |
| `PydanticSingleSelector` / `PydanticMultiSelector` | Structured function-calling output in Python | Not documented for TypeScript |

Selector names beyond `LLMSingleSelector` are from memory (verify). For structured selection in your own code, validate the LLM output with `zod` rather than parsing free text.

## The Descriptions Are the Router
The LLM sees only each tool's `description`. Good descriptions:
- State what the route is good at **and** what it is not for.
- Are distinct from each other (overlap causes flip-flopping).
- Mention data scope ("2024 financial reports", "API documentation").

## Non-LLM Routing
Often a rule does the job and is cheaper and deterministic:

```ts
function choose(question: string): BaseRetriever {
  const q = question.toLowerCase();
  if (["summarize", "overview", "summary"].some((w) => q.includes(w))) {
    return summaryRetriever;
  }
  return vectorRetriever;
}
```
Or route by user context (the user's department decides the namespace). For access control, always route from authenticated context, never from an LLM guess.

## Cost and Latency
Each routed query adds an LLM call for selection. Use a small, fast model, and cache routing decisions for repeated questions (a `Map` keyed by normalized question is enough to start).

## Debugging
Log the chosen route and the selector's reason for every query (the hand-rolled router above logs the raw LLM reply; for `RouterQueryEngine`, check what your version exposes on the response, verify). Most "wrong answer" cases in routed systems are wrong routes.

## Important Rules
- Write distinct, specific tool descriptions.
- Log every routing decision.
- Provide a sensible default or fallback route.
- Never use an LLM router for authorization.

## Common Mistakes
- Overlapping descriptions that make routing unstable.
- No default route, so ambiguous questions fail.
- Adding a router when one hybrid retriever would do.
- Not evaluating routing accuracy separately from answer quality.
- Looking for `RouterRetriever` or `RetrieverTool` in LlamaIndex.TS; only the query-engine router is documented.

## Related / Next
- [summary-retriever.md](summary-retriever.md)
- [property-graph-retriever.md](property-graph-retriever.md)
- [custom-retriever.md](custom-retriever.md)
- `08-query-transformation/sub-question-query-engine.md`
