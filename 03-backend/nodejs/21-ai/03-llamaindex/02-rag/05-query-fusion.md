# Query Fusion

**Query fusion** means retrieving with several searches and merging the ranked lists into one. The searches can differ in *retriever* (vector + BM25) or in *query* (the original question plus LLM-generated rewrites), or both. Merging is called **fusion**, and the choice of fusion algorithm matters because different retrievers produce scores on incompatible scales.

> Prerequisites: [03-retrievers-and-postprocessors](./03-retrievers-and-postprocessors.md), [04-bm25-and-hybrid-search](./04-bm25-and-hybrid-search.md).

> **Status in TypeScript:** Python LlamaIndex has a built-in `QueryFusionRetriever` (multi-query generation plus `reciprocal_rerank`, `relative_score` and `dist_based_score` modes). I could not find an equivalent class in the LlamaIndex.TS API reference. This note therefore explains the algorithms and gives small, self-contained implementations you can drop in. Re-check the reference for your version in case a built-in has since been added.

## Why fuse at all

- **Different retrievers fail differently.** Vectors miss exact tokens; BM25 misses paraphrase. A union with sensible ranking beats either alone.
- **One phrasing is a single point of failure.** A user's question may use different words than the document. Generating a few rewrites and retrieving for each raises the odds that one phrasing hits.
- **Cost:** N retrievals instead of one, plus an LLM call if you generate queries. Worth it only if evaluation shows gains.

## The fusion algorithms

You have several ranked lists, each a list of `(node, score)`. You want one list.

### Reciprocal rank fusion (RRF)

Ignore scores; use only ranks. Each list gives a document `1 / (k + rank)` points; sum across lists.

```text
score(d) = Σ over lists  1 / (k + rank_in_list(d))        k ≈ 60 by convention
```

- Pros: no normalization, robust to wildly different score scales, trivial to implement, a good default.
- Cons: throws away score magnitude (a near-perfect match and a barely-relevant one at adjacent ranks look similar), and a document missing from a list simply gets nothing from it.
- `k` softens the influence of top ranks. Larger `k` flattens differences.

### Relative score fusion

Normalize each list's scores to a common range (min-max to 0..1), then take a weighted sum.

```text
norm_i(d) = (score_i(d) - min_i) / (max_i - min_i)
score(d)  = Σ w_i · norm_i(d)
```

- Pros: keeps score magnitude; weights give direct control ("trust BM25 less").
- Cons: min-max is sensitive to outliers. One extreme top score squashes everyone else toward zero. With few results per list, the min and max are noisy.

### Distribution-based score fusion

Like relative score, but the normalization bounds come from the score distribution rather than the observed min and max, typically mean ± 3 standard deviations, so a single outlier doesn't dominate.

```text
lo = mean - 3σ,  hi = mean + 3σ
norm(d) = clamp((score - lo) / (hi - lo), 0, 1)
```

- Pros: more stable than min-max when scores are skewed.
- Cons: needs enough results per list for the mean and deviation to mean something (retrieve a generous number from each retriever).

### Choosing

| Situation | Pick |
|---|---|
| First implementation, mixed retrievers | RRF |
| You want to weight sources explicitly and have enough results per list | Relative or distribution-based |
| Very few results per retriever | RRF (normalization is noisy) |

Whatever you choose, validate with an evaluation set. Rank-based and score-based fusion can disagree by several points depending on your data.

## Implementations

```ts
import type { NodeWithScore } from "llamaindex";

type Ranked = NodeWithScore[];

// 1) Reciprocal rank fusion, with optional per-list weights
export function rrf(lists: Ranked[], opts: { k?: number; weights?: number[] } = {}): Ranked {
  const k = opts.k ?? 60;
  const acc = new Map<string, NodeWithScore>();

  lists.forEach((list, i) => {
    const w = opts.weights?.[i] ?? 1;
    list.forEach((item, rank) => {
      const id = item.node.id_;
      const prev = acc.get(id);
      const add = w / (k + rank + 1);
      acc.set(id, { ...item, score: (prev?.score ?? 0) + add });
    });
  });

  return [...acc.values()].sort((a, b) => (b.score ?? 0) - (a.score ?? 0));
}

// 2) Score normalization, then weighted sum
type Norm = "minmax" | "dist";

function normalize(list: Ranked, mode: Norm): Map<string, number> {
  const scores = list.map((x) => x.score ?? 0);
  let lo: number, hi: number;

  if (mode === "minmax") {
    lo = Math.min(...scores);
    hi = Math.max(...scores);
  } else {
    const mean = scores.reduce((a, b) => a + b, 0) / scores.length;
    const sd = Math.sqrt(scores.reduce((a, b) => a + (b - mean) ** 2, 0) / scores.length);
    lo = mean - 3 * sd;
    hi = mean + 3 * sd;
  }

  const range = hi - lo || 1; // avoid divide-by-zero when all scores are equal
  return new Map(
    list.map((x) => [
      x.node.id_,
      Math.min(1, Math.max(0, ((x.score ?? 0) - lo) / range)),
    ]),
  );
}

export function scoreFusion(
  lists: Ranked[],
  opts: { mode?: Norm; weights?: number[] } = {},
): Ranked {
  const mode = opts.mode ?? "minmax";
  const acc = new Map<string, NodeWithScore>();

  lists.forEach((list, i) => {
    const w = opts.weights?.[i] ?? 1 / lists.length;
    const norm = normalize(list, mode);
    for (const item of list) {
      const id = item.node.id_;
      const prev = acc.get(id);
      acc.set(id, { ...item, score: (prev?.score ?? 0) + w * (norm.get(id) ?? 0) });
    }
  });

  return [...acc.values()].sort((a, b) => (b.score ?? 0) - (a.score ?? 0));
}
```

A document absent from a list contributes zero from that list. This is the standard behavior and is reasonable, but it means documents found by only one retriever rank below those found by both. That is usually what you want.

### Weights

`weights[i]` scales list `i`. Example: `rrf([vec, kw], { weights: [1, 0.5] })` favors vectors. Start equal; adjust only with evaluation evidence, because weights tuned on a handful of queries rarely generalize.

## Multi-query: generate rewrites

```ts
import { Settings } from "llamaindex";

async function generateQueries(question: string, n = 3): Promise<string[]> {
  const prompt =
    `Generate ${n} different search queries that would help answer the question below. ` +
    `Use different wording and cover different angles. Output one query per line, no numbering.\n\n` +
    `Question: ${question}`;

  const res = await Settings.llm.complete({ prompt });
  const extra = res.text
    .split("\n")
    .map((s) => s.trim())
    .filter(Boolean)
    .slice(0, n);

  return [question, ...extra]; // always keep the original
}
```

Keep the original question in the set. Rewrites help recall but can drift from the user's intent.

## A fusion retriever you can plug into a query engine

```ts
import { BaseRetriever, RetrieverQueryEngine, type NodeWithScore } from "llamaindex";

class FusionRetriever extends BaseRetriever {
  constructor(
    private retrievers: BaseRetriever[],
    private opts: { topN?: number; numQueries?: number } = {},
  ) {
    super();
  }

  async _retrieve(params: { query: unknown }): Promise<NodeWithScore[]> {
    const question = String(params.query); // assumes text queries
    const queries = await generateQueries(question, this.opts.numQueries ?? 3);

    const lists = await Promise.all(
      queries.flatMap((q) => this.retrievers.map((r) => r.retrieve({ query: q }))),
    );

    return rrf(lists).slice(0, this.opts.topN ?? 8);
  }
}

const retriever = new FusionRetriever([vectorRetriever, bm25]);
const queryEngine = new RetrieverQueryEngine(retriever);
```

The `BaseRetriever` abstract method and its parameter type have the shape `_retrieve(params: QueryBundle)`; the loose `{ query: unknown }` above is for readability. Use the real `QueryBundle` type from your version. The constructor argument order of `RetrieverQueryEngine` is `(retriever, responseSynthesizer?, nodePostprocessors?)` in the version I checked.

Fusion output does not carry meaningful similarity scores (RRF scores are not similarities), so **don't put a `SimilarityPostprocessor` cutoff after fusion**. Put a reranker after it instead.

## Cost and latency

For `Q` queries and `R` retrievers you run `Q × R` retrievals plus one LLM call for generation. All retrievals can run in parallel, so latency is roughly one LLM call plus the slowest retrieval, plus fusion. Cap `numQueries` (2 to 4 is typical) and cache generated queries for repeated questions.

## Common mistakes

**Fusing raw scores from different retrievers.** Use RRF or normalize first.

**Dropping the original query** from the multi-query set.

**Keeping a similarity cutoff after fusion.** Fused scores aren't similarities.

**Too few results per list for distribution-based normalization.** Retrieve more, or use RRF.

**Mismatched node IDs across retrievers.** Fusion dedups by `node.id_`; if the two retrievers return different node objects for the same text, you'll double count.

**Tuning weights on three examples.**

## Debugging

- Print each input list (ids and scores) and the fused list for a failing query.
- Check that the expected node appears in at least one input list. If not, fusion cannot help.
- If fusion lowers quality, compare against the best single retriever on your evaluation set; keep the simpler option if it wins.
- Log generated queries; poor rewrites are the usual cause of multi-query regressions.

## Quick Summary

- Fusion merges ranked lists from multiple retrievers and/or multiple generated queries.
- RRF (rank-based, `1/(k+rank)`, `k≈60`) is the robust default. Relative and distribution-based fusion normalize scores and allow weights, but need enough results per list.
- LlamaIndex.TS has no built-in fusion retriever that I could find; the implementations above are small and testable.
- Always keep the original query, run retrievals in parallel, and put a reranker (not a similarity cutoff) after fusion.
- Validate every fusion choice and weight on an evaluation set.

## Next

[06-query-transforms-and-routing.md](./06-query-transforms-and-routing.md): rewriting queries (HyDE, sub-questions, multi-step) and routing them to the right engine.
