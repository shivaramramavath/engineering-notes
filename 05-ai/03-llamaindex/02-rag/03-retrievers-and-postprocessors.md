# Retrievers and Postprocessors

Retrieval decides what the LLM gets to see. If the right chunk isn't retrieved, no prompt or model can recover. The standard shape is: **retrieve wide, then narrow**. A retriever fetches candidate nodes; **node postprocessors** filter, re-rank or rewrite them; only then does the synthesizer call the LLM.

> Prerequisites: [../01-core/04-query-and-chat-engines](../01-core/04-query-and-chat-engines.md).

```text
query ─► retriever (top-k candidates)
          └► postprocessor 1 (e.g. similarity cutoff)
              └► postprocessor 2 (e.g. reranker, keep top N)
                  └► response synthesizer ─► LLM
```

## Retrievers directly

A query engine hides the retriever. Using it directly is the best way to see what retrieval actually returns:

```ts
const retriever = index.asRetriever({ similarityTopK: 5 });

const results = await retriever.retrieve({ query: "What is the refund window?" });

for (const r of results) {
  console.log(r.score, r.node.id_);
  console.log(r.node.getContent(MetadataMode.NONE).slice(0, 200));
}
```

Each result is a `NodeWithScore`: a node plus a score. For vector retrieval the score is a similarity (higher is better).

Retriever types in the TypeScript package include:

| Retriever | Behavior |
|---|---|
| `VectorIndexRetriever` (from `index.asRetriever()`) | Top-k most similar nodes |
| `SummaryIndexRetriever` | Returns all nodes regardless of query |
| `SummaryIndexLLMRetriever` | LLM scores and filters nodes |
| `KeywordTable*Retriever` (LLM, simple, RAKE) | Keyword-based retrieval over a `KeywordTableIndex` |
| `Bm25Retriever` | BM25 lexical retrieval (see [04-bm25-and-hybrid-search](./04-bm25-and-hybrid-search.md)) |

## Top-k

`similarityTopK` is the number of candidates fetched. The library default is small (2), which is rarely right for real use. A reasonable pattern:

- Retrieve generously (say 10 to 20) when a reranker follows.
- Retrieve a modest number (3 to 6) when nothing follows.

```ts
const queryEngine = index.asQueryEngine({ similarityTopK: 8 });
```

More context is not automatically better. Extra weakly-relevant chunks dilute the prompt, increase cost and latency, and can push the model toward wrong details. The fix is a cutoff or reranker, not just a bigger `k`.

## Metadata filters

Filters restrict the search space *before* ranking, using node metadata. On a query engine the option is `preFilters`; filters are a plain object:

```ts
const queryEngine = index.asQueryEngine({
  similarityTopK: 5,
  preFilters: {
    filters: [
      { key: "department", value: "hr", operator: "==" },
      { key: "year", value: 2024, operator: ">=" },
    ],
    condition: "and",
  },
});
```

Notes:

- Operators include `==`, `!=`, `>`, `>=`, `<`, `<=`, `in`, `nin`, `contains` and a few more, but **which operators work depends on the vector store**. The in-memory store supports fewer than databases like Postgres or Chroma. Test your exact store.
- Value types must match what you stored: `"2024"` is not `2024`.
- Filter keys are the keys you put in `metadata` at ingestion, so decide the filterable fields up front (source, department, tenant, language, date). Changing them later means re-ingesting.
- `FilterOperator` and `FilterCondition` enums are exported if you prefer them to string literals.
- On the retriever itself, the option name has differed between docs pages (`preFilters` versus `filters`). Verify it against your installed version's types.

Filters are also your multi-tenancy tool: a mandatory `tenantId` filter applied server-side on every query. Never let user input choose it. See `04-production/04-security.md`.

## Similarity cutoff

`SimilarityPostprocessor` drops nodes scoring below a threshold:

```ts
import { SimilarityPostprocessor } from "llamaindex";

const queryEngine = index.asQueryEngine({
  similarityTopK: 10,
  nodePostprocessors: [new SimilarityPostprocessor({ similarityCutoff: 0.7 })],
});
```

Caveats:

- **Scores are not comparable across embedding models.** A good cutoff for one model is wrong for another. Pick the number by looking at scores for known-good and known-bad queries.
- It only makes sense for similarity-scored retrieval. BM25 and fused scores live on different scales.
- If everything is filtered out, the synthesizer gets no context. Decide what your app does then (say "I don't know" rather than letting the model improvise).

## Rerankers

Embedding similarity is a fast approximation. A **reranker** is a model that reads the query and each candidate together and scores them more accurately, but slowly, so you run it only on the few candidates the retriever returns.

```bash
npm i @llamaindex/cohere
```

```ts
import { CohereRerank } from "@llamaindex/cohere";

const reranker = new CohereRerank({
  apiKey: process.env.COHERE_API_KEY!,
  topN: 4,
});

const queryEngine = index.asQueryEngine({
  similarityTopK: 20,                 // wide
  nodePostprocessors: [reranker],     // narrow to 4
});
```

Other options exist, such as `JinaAIReranker`. Check each provider's package page for current option names.

Trade-offs:

- Adds a network call (latency, cost, and your text leaving your infrastructure for hosted rerankers).
- Usually the single biggest quality gain after fixing chunking.
- Order of postprocessors matters. Apply a similarity cutoff first to cheaply drop junk, then the reranker on what is left.

## Other built-in and custom postprocessors

- `MetadataReplacementPostProcessor("window")` swaps each retrieved node's text for text stored in metadata (used by sentence-window retrieval; see [07-advanced-retrieval-patterns](./07-advanced-retrieval-patterns.md)).
- A postprocessor is any object with `postprocessNodes(nodes: NodeWithScore[]): Promise<NodeWithScore[]>`. Writing your own is simple:

```ts
import type { NodeWithScore } from "llamaindex";

class DropStaleDocs {
  constructor(private minYear: number) {}

  async postprocessNodes(nodes: NodeWithScore[]): Promise<NodeWithScore[]> {
    return nodes.filter((n) => (n.node.metadata.year ?? 0) >= this.minYear);
  }
}

const queryEngine = index.asQueryEngine({
  nodePostprocessors: [new DropStaleDocs(2023)],
});
```

Use custom postprocessors for business rules (freshness, access control checks, deduplicating near-identical chunks) that don't belong in embeddings.

## Putting it together

```ts
const queryEngine = index.asQueryEngine({
  similarityTopK: 20,
  preFilters: {
    filters: [{ key: "tenantId", value: tenantId, operator: "==" }],
  },
  nodePostprocessors: [
    new SimilarityPostprocessor({ similarityCutoff: 0.5 }),
    reranker,
  ],
});
```

Tenant filter first (correctness and security), then cheap cutoff, then reranker.

## Common mistakes

**Leaving `similarityTopK` at the default.** Too few candidates means the right chunk never reaches the LLM.

**Large `k` with no reranker.** Noise rises with k.

**Hard-coding a similarity cutoff copied from a blog post.** It is model-specific.

**Filtering on metadata you didn't store (or stored with a different type).** You get empty results, not an error.

**Reranking everything.** Rerank tens of candidates, not hundreds.

**Applying postprocessors in the wrong order.** Cutoffs and filters before rerankers.

## Debugging

- Always call `retriever.retrieve` on the failing query and print scores and text. Is the right chunk present? At what rank?
- Right chunk present but low rank → add a reranker or improve chunking.
- Right chunk absent → check chunking, embedding model, filters, and whether the data was ingested at all.
- Everything filtered out → loosen the cutoff or check filter key names and types.
- Compare retrieval with and without postprocessors to see what each stage removes.

## Quick Summary

- Retrieve wide, then narrow with postprocessors, then synthesize.
- Use `retriever.retrieve({ query })` to inspect candidates and scores directly.
- Set `similarityTopK` deliberately; the default is low.
- `preFilters` restrict by metadata before ranking; operator support depends on the store, and types must match.
- `SimilarityPostprocessor` cutoffs are embedding-model-specific; calibrate them.
- Rerankers (for example `CohereRerank`) give the best precision gain but add a call and latency.
- Custom postprocessors are plain objects with `postprocessNodes`.

## Next

[04-bm25-and-hybrid-search.md](./04-bm25-and-hybrid-search.md): keyword retrieval for what vectors miss, and how to combine the two.
