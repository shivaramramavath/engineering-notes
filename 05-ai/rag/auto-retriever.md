# Query Fusion Retriever

> **Not available in LlamaIndex.TS - this file shows a hand-rolled TypeScript implementation.** The Python framework ships `QueryFusionRetriever`; the LlamaIndex.TS documentation read on 2026-10-10 has no equivalent. Everything below is built from `BaseRetriever`, `NodeWithScore`, `QueryBundle` and `Settings.llm`. Import paths vary by version (verify).

## Concept
A query fusion retriever does two related things:

1. **Multi-query**: an LLM rewrites the user's question into several variants; each variant is searched.
2. **Fusion**: results from all queries (and from all underlying retrievers) are merged into one ranked list.

```
"vacation days"
   ├─ LLM generates ─► "annual leave policy", "PTO accrual rules", "time off entitlement"
   └─ each query ─► [vector retriever, BM25 retriever] ─► fuse by rank ─► top-k
```

With `numQueries = 1` it is a pure **fusion of retrievers** (no LLM), the basis of hybrid retrieval.

## When to Use
- Users phrase questions differently from the documents' wording.
- Short or ambiguous queries.
- You want hybrid (vector + BM25) fusion ([hybrid-retrieval.md](hybrid-retrieval.md)).
- Recall matters more than latency.

## When Not to Use
- Latency or LLM cost is tight (multi-query adds an LLM call and several searches).
- Queries are already precise and dense retrieval performs well.

## Prerequisites
- [vector-retriever.md](vector-retriever.md)
- `08-query-transformation/query-rewriting.md`

## Reciprocal Rank Fusion (RRF)
Each result list contributes `1 / (k + rank)` to a node's score, with `k = 60` (the constant from the original RRF paper). Ranks start at 1. Nodes that appear high in several lists win. Only ranks matter, so scores from different retrievers (cosine versus BM25) never need to be put on one scale.

## The Class
```ts
import { BaseRetriever } from "@llamaindex/core/retriever";        // verify path
import type { NodeWithScore, QueryBundle } from "@llamaindex/core"; // verify path
import { Settings } from "llamaindex";

interface QueryFusionOptions {
  retrievers: BaseRetriever[];
  numQueries?: number; // total queries including the original; 1 disables generation
  topK?: number;       // size of the final fused list; omit to return everything
  rrfK?: number;       // RRF constant, default 60
  queryGenPrompt?: (query: string, n: number) => string;
  verbose?: boolean;
}

const defaultPrompt = (query: string, n: number): string =>
  `Generate ${n} different search queries related to the question below. ` +
  `Return one query per line, with no numbering and no extra text.\n\nQuestion: ${query}\nQueries:`;

export class QueryFusionRetriever extends BaseRetriever {
  private retrievers: BaseRetriever[];
  private numQueries: number;
  private topK?: number;
  private rrfK: number;
  private queryGenPrompt: (query: string, n: number) => string;
  private verbose: boolean;

  constructor(opts: QueryFusionOptions) {
    super();
    this.retrievers = opts.retrievers;
    this.numQueries = opts.numQueries ?? 4;
    this.topK = opts.topK;
    this.rrfK = opts.rrfK ?? 60;
    this.queryGenPrompt = opts.queryGenPrompt ?? defaultPrompt;
    this.verbose = opts.verbose ?? false;
  }

  private async generateQueries(original: string): Promise<string[]> {
    if (this.numQueries <= 1) return [original];
    const res = await Settings.llm.complete({
      prompt: this.queryGenPrompt(original, this.numQueries - 1),
    }); // verify: complete({ prompt }) returning { text }
    const variants = res.text
      .split("\n")
      .map((l) => l.replace(/^[\s\-*\d.)]+/, "").trim())
      .filter((l) => l.length > 0 && l !== original)
      .slice(0, this.numQueries - 1);
    const queries = [original, ...variants];
    if (this.verbose) console.log("fusion queries:", queries);
    return queries;
  }

  async _retrieve(query: QueryBundle): Promise<NodeWithScore[]> {
    const queries = await this.generateQueries(query.query); // verify: QueryBundle field name

    // Run every (query, retriever) pair concurrently.
    const lists = await Promise.all(
      queries.flatMap((q) => this.retrievers.map((r) => r.retrieve({ query: q }))),
    );

    // RRF: score(node) = sum over lists of 1 / (k + rank)
    const scores = new Map<string, number>();
    const nodes = new Map<string, NodeWithScore>();
    for (const list of lists) {
      list.forEach((nws, i) => {
        const id = nws.node.id_;
        scores.set(id, (scores.get(id) ?? 0) + 1 / (this.rrfK + i + 1));
        if (!nodes.has(id)) nodes.set(id, nws);
      });
    }

    const fused = [...nodes.entries()]
      .map(([id, nws]) => ({ node: nws.node, score: scores.get(id)! }))
      .sort((a, b) => b.score - a.score);

    return this.topK ? fused.slice(0, this.topK) : fused;
  }
}
```
Notes:
- `retrieve({ query })` returns scored nodes sorted best-first for built-in retrievers, so the array index is the rank.
- The returned `score` is the RRF score (small numbers such as 0.03), not a similarity. Do not apply a `SimilarityPostprocessor` cutoff to it.
- Nodes are deduplicated by `node.id_`, so retrievers must return stable IDs.
- If one retriever fails, `Promise.all` rejects everything. Use `Promise.allSettled` if you prefer partial results.

## Usage
```ts
const fusion = new QueryFusionRetriever({
  retrievers: [vectorRetriever, bm25Retriever],
  numQueries: 4, // original + 3 generated variants
  topK: 10,
  verbose: true,
});
const nodes = await fusion.retrieve({ query: "vacation days" });
```

## Parameters

| Parameter | Meaning |
|---|---|
| `retrievers` | Retrievers to run for each query |
| `topK` | Final number of nodes returned |
| `numQueries` | Total queries including the original; `1` means no query generation |
| `rrfK` | RRF constant (60) |
| `queryGenPrompt` | Custom prompt builder for generating variants |

These are the names of this hand-rolled class, not a library API.

## Weighted or Score-Based Fusion
The Python class also offers score-normalizing modes (`relative_score`, `dist_based_score`). The class above implements only RRF. If you need weights, multiply each list's contribution by a per-retriever weight inside the loop (for example `weight / (k + rank)`). Start with plain RRF.

## In a Query Engine
```ts
import { RetrieverQueryEngine } from "llamaindex/engines";

const engine = new RetrieverQueryEngine({ retriever: fusion });
const response = await engine.query({ query: "vacation days" });
console.log(response.toString());
```

## Customizing Query Generation
```ts
const fusion = new QueryFusionRetriever({
  retrievers: [vectorRetriever],
  numQueries: 4,
  queryGenPrompt: (query, n) =>
    `You are helping search an HR knowledge base. Generate ${n} different search queries ` +
    `related to: ${query}\nQueries, one per line:\n`,
});
```
Domain-specific prompts produce better variants than a generic one.

## Cost and Latency
- Generation: one LLM call per question (this class makes a single call that returns all variants).
- Retrieval: `numQueries x number of retrievers` searches, run concurrently with `Promise.all`.
- Use a small, fast LLM for generation.
- Cache results for repeated questions (`16-production/caching.md`).

## Evaluating It
Compare recall@k with and without multi-query on questions that are phrased unlike the source text. If recall does not improve, remove it.

## Important Rules
- Use `numQueries: 1` when you only want to fuse retrievers.
- Retrieve generously per sub-retriever, trim with `topK` or a reranker.
- Bound `numQueries` (3 to 5) to control cost.

## Common Mistakes
- Using multi-query to patch bad chunking.
- Generating many near-identical variants with a weak prompt.
- Running searches in a sequential `for` loop with `await`, so latency spikes.
- Skipping a reranker after fusion, leaving noisy merged results.
- Treating the RRF score as a similarity score.

## Related / Next
- [hybrid-retrieval.md](hybrid-retrieval.md)
- [custom-retriever.md](custom-retriever.md)
- `08-query-transformation/query-rewriting.md`
- `09-reranking/reranking-fundamentals.md`
