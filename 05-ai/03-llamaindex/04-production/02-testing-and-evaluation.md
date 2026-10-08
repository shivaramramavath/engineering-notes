# Testing and Evaluation

LLM apps are non-deterministic and "correct" is fuzzy, so people skip testing and tune by vibes: change chunk size, try three questions, ship. Then a later change quietly breaks half the answers. The fix is a small, stable **evaluation set** and a script that scores every change against it. Evaluation is the thing that lets you say "reranking improved hit rate from 71% to 84%" instead of "it feels better".

> Prerequisites: [../02-rag/03-retrievers-and-postprocessors](../02-rag/03-retrievers-and-postprocessors.md), [01-tracing-and-debugging](./01-tracing-and-debugging.md).

## What to test, at which layer

| Layer | What breaks | How to test |
|---|---|---|
| Plain code (parsers, custom transformations, postprocessors, tool `execute`) | Ordinary bugs | Normal unit tests, no LLM |
| **Retrieval** | Right chunk not returned or ranked low | Labelled questions → expected source IDs; hit rate, MRR |
| **Generation** | Hallucination, off-topic, wrong facts | LLM-judged faithfulness, relevancy, correctness |
| Agents / workflows | Wrong tool, loops, wrong order | Assertions on tool-call sequences and step counts |
| End to end | Regressions across all of the above | Golden question set run in CI, thresholds |

Test retrieval separately from generation. Retrieval is deterministic, cheap (no LLM), and where most failures start. If retrieval hit rate is bad, generation scores are meaningless.

## Build a golden set

Start with **30 to 100 real questions**. Quality beats quantity.

```ts
type GoldenCase = {
  id: string;
  question: string;
  expectedSources: string[];     // doc/node ids (or metadata.source) that contain the answer
  referenceAnswer?: string;      // optional, enables correctness scoring
  tags?: string[];               // "identifier", "multi-hop", "should-refuse"
};
```

Where the questions come from, best first: real user questions from logs (anonymized), questions from domain experts, then LLM-generated questions over your chunks. Generated questions are useful for volume but are biased toward easy, chunk-shaped queries, so mix them with real ones. I did not verify a built-in question-generation helper in the TypeScript package; generating questions with `Settings.llm.complete` and a prompt is simple enough to do yourself.

Include deliberately hard cases:

- Exact-token queries (error codes, SKUs): vectors tend to fail here.
- Paraphrased queries with no shared words.
- Multi-document questions.
- **Unanswerable** questions where the right behavior is "I don't know".
- Questions about similar-but-different things (2023 vs 2024 policy).

Keep the set versioned in git next to the code. Every production failure you debug gets added to it.

## Retrieval metrics (no LLM needed)

Two metrics cover most needs:

- **Hit rate @k**: fraction of questions where at least one expected source appears in the top k.
- **MRR** (mean reciprocal rank): average of `1 / rank_of_first_correct_hit`. Rewards putting the right chunk first.

```ts
async function evalRetrieval(retriever: { retrieve(p: { query: string }): Promise<NodeWithScore[]> },
                             cases: GoldenCase[], k = 5) {
  let hits = 0, rrSum = 0;
  const misses: string[] = [];

  for (const c of cases.filter((x) => x.expectedSources.length)) {
    const results = (await retriever.retrieve({ query: c.question })).slice(0, k);
    const ids = results.map((r) => r.node.metadata.source ?? r.node.id_);
    const firstRank = ids.findIndex((id) => c.expectedSources.includes(id));

    if (firstRank >= 0) { hits++; rrSum += 1 / (firstRank + 1); }
    else misses.push(c.id);
  }

  const n = cases.filter((x) => x.expectedSources.length).length;
  return { hitRate: hits / n, mrr: rrSum / n, misses };
}
```

Match on a stable identifier (a `source` metadata field or a document ID), not on chunk text, so the test survives re-chunking.

Use it to compare configurations side by side:

```ts
for (const topK of [3, 5, 10]) {
  console.log(topK, await evalRetrieval(index.asRetriever({ similarityTopK: topK }), cases, topK));
}
```

This is how you decide chunk size, top-k, hybrid search, rerankers, and embedding models with evidence.

## Generation metrics (LLM as judge)

LlamaIndex.TS ships LLM-based evaluators:

| Evaluator | Question it answers | Needs reference? |
|---|---|---|
| `FaithfulnessEvaluator` | Is the answer supported by the retrieved context (no hallucination)? | No |
| `RelevancyEvaluator` | Do the response and sources actually address the query? | No |
| `CorrectnessEvaluator` | Does the answer match a reference answer? Scores 0 to 5 per the docs | Yes |

```ts
import { FaithfulnessEvaluator, RelevancyEvaluator, Settings } from "llamaindex";
import { OpenAI } from "@llamaindex/openai";

// judge model: use a strong one, temperature 0
Settings.llm = new OpenAI({ model: "gpt-4o", temperature: 0 });

const faithfulness = new FaithfulnessEvaluator();
const relevancy = new RelevancyEvaluator();

const queryEngine = index.asQueryEngine({ similarityTopK: 5 });

let faithful = 0, relevant = 0;
for (const c of cases) {
  const response = await queryEngine.query({ query: c.question });

  const f = await faithfulness.evaluateResponse({ query: c.question, response });
  const r = await relevancy.evaluateResponse({ query: c.question, response });

  if (f.passing) faithful++;
  if (r.passing) relevant++;
}
console.log({ faithfulness: faithful / cases.length, relevancy: relevant / cases.length });
```

`evaluateResponse` takes the original `query` and the engine's `response` object (which carries the source nodes). The result has a `passing` boolean; evaluators may also return a score and feedback text that explain the verdict, which is worth logging for failures. `CorrectnessEvaluator` additionally needs the reference answer; check its parameter type in your version.

Cautions about LLM judges:

- **They are models too.** They are noisy and can be lenient or biased toward their own style. Use a strong judge at temperature 0, and **spot check** a sample of judgments by hand, especially the failures and surprising passes.
- **Don't judge with the same weak model that generated the answer.**
- **Costs add up**: each evaluation is one or more LLM calls per question per metric. Run the cheap retrieval metrics on every commit and the judge-based ones nightly or before releases.
- Look at **distributions and trends**, not single-number thresholds from one run. Run twice to see the noise floor before alerting on small differences.

Faithfulness is the most valuable generation metric for RAG: it catches confident answers that the sources don't support. Check "should refuse" cases separately: the right outcome is an "I don't know" answer, so assert on that behavior (string match or a small judge prompt), not on faithfulness.

## Testing agents and workflows

Agents are the least deterministic part. Test the *behavior you care about* rather than exact text:

```ts
import { agentToolCallEvent } from "@llamaindex/workflow";

async function toolsUsed(question: string) {
  const used: string[] = [];
  for await (const event of myAgent.runStream(question)) {
    if (agentToolCallEvent.include(event)) used.push(event.data.toolName);
  }
  return used;
}

test("budget question uses the budget tool, not weather", async () => {
  const used = await toolsUsed("What is 2% of the SF budget?");
  expect(used).toContain("sf_budget");
  expect(used).not.toContain("get_weather");
  expect(used.length).toBeLessThanOrEqual(4);   // loop guard
});
```

Assert on: which tools were called, how many steps, that forbidden tools were not called, that the final answer contains required facts. Workflows are easier because each handler is a function: test handlers and branching decisions in isolation with fixed inputs, and drive the whole workflow with the event stream.

## Unit tests that don't need an LLM

Most of your code is ordinary and deserves ordinary tests:

- Custom postprocessors (`postprocessNodes` on hand-made nodes).
- Custom readers and metadata mapping (stable `id_`s, excluded keys).
- Tool `execute` functions with malicious or malformed arguments.
- Fusion and parent-expansion logic (pure functions).
- Authorization filters: given a user, the filter built is exactly the allowed set.

Design for testability by **injecting dependencies** (a retriever interface, an LLM-like object) instead of reaching for globals. I'd avoid mocking LlamaIndex internals; mock at your own boundary.

## Running it in CI

- **Every commit:** unit tests plus retrieval metrics on a small fixture corpus (fast, cheap, deterministic). Fail if hit rate drops more than an agreed margin.
- **Nightly / pre-release:** the judge-based suite over the full golden set; store results per run and diff against the last release.
- **Pin what affects scores:** embedding model, chunking config, index version, judge model. Record them with every result so a score change is attributable.
- Re-run the same suite when you change the **embedding model** (requires re-indexing) or the **LLM** version; providers update models.

## Production feedback loop

Evaluation sets rot. Close the loop: collect thumbs up/down, sample low-rated and "I don't know" answers weekly, triage them with traces ([01-tracing-and-debugging](./01-tracing-and-debugging.md)), and promote the real failures into the golden set.

## Common mistakes

**Evaluating on questions the system was tuned on.** Keep a held-out slice you never tune against.

**One aggregate score.** Break results down by tag (identifier queries, multi-hop, unanswerable) to see what a change helps and hurts.

**Skipping retrieval metrics** and relying only on LLM-judged answers.

**Using the LLM judge as ground truth** without human spot checks.

**Golden set made only of easy generated questions.**

**Not versioning the data and config** that produced a score.

**Testing the agent by running it once.**

## Quick Summary

- Test layers separately: plain code, retrieval, generation, agent behavior, end to end.
- Build a versioned golden set of 30 to 100 real questions with expected sources; include hard, unanswerable and exact-token cases.
- Retrieval hit rate@k and MRR need no LLM and drive most decisions (chunking, k, hybrid, rerank).
- `FaithfulnessEvaluator`, `RelevancyEvaluator`, `CorrectnessEvaluator` provide LLM-judged scores via `evaluateResponse`; use a strong judge, spot check, and watch noise.
- Test agents by tool-call sequences and step counts, not exact text.
- Run cheap checks on every commit, expensive judging on a schedule, and feed production failures back into the set.

## Next

[03-reliability-and-cost.md](./03-reliability-and-cost.md): keeping latency, failures and the bill under control.
