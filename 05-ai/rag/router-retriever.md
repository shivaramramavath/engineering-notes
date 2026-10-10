# Retrieval Basics

## Concept
Concepts shared by every retriever: how many nodes to fetch (**top-k**), how to drop weak matches (**score cutoff**), how to restrict the search (**metadata filters**), and how to read **scores**.

## Prerequisites
- [README.md](README.md)
- `02-embeddings/similarity-metrics.md`

## The Retrieval Contract
```ts
import { VectorStoreIndex } from "llamaindex";

const retriever = index.asRetriever({ similarityTopK: 5 });

const nodes = await retriever.retrieve({ query: "vacation policy" });   // object form
// Docs also show retriever.retrieve("vacation policy") with a plain string; verify for your version.
```
Returns `NodeWithScore[]`: `.node` (the node), `.score` (number or `undefined`). `retrieve` is async; there is no separate `aretrieve`.

## Top-k
`similarityTopK` is how many nodes the retriever returns.

```ts
const retriever = index.asRetriever({ similarityTopK: 10 });
```
- The library default is small (verify the exact number in your version). Defaults are rarely right for real data.
- Too low: the answer's source chunk is cut off (low recall).
- Too high: noise, higher cost and latency, worse "lost in the middle" behavior.

### Choosing top-k
| Strategy | Typical numbers |
|---|---|
| Retrieve for the LLM directly | 3 to 8 |
| Retrieve wide, then rerank down | Retrieve 20 to 50, keep 3 to 8 |
| Query fusion / hybrid | Each sub-retriever 10 to 20, fused list trimmed |

Treat these as starting points; tune on your evaluation set. The best top-k depends on chunk size: smaller chunks need larger k.

## asRetriever Options
```ts
import { VectorStoreQueryMode } from "@llamaindex/core/vector-store";   // path varies by version; verify

const retriever = index.asRetriever({
  similarityTopK: 8,
  mode: VectorStoreQueryMode.DEFAULT,   // DEFAULT | HYBRID | MMR | SPARSE | SEMANTIC_HYBRID
  filters,                              // MetadataFilters, see below
  customParams: { alpha: 0.5 },         // store-specific; alpha for HYBRID, mmrThreshold for MMR
});
```

| Option | Meaning |
|---|---|
| `similarityTopK` | Number of nodes to return |
| `mode` | A `VectorStoreQueryMode`. Only `DEFAULT` is safe everywhere; the others depend on the vector store (verify) |
| `filters` | `MetadataFilters` applied in the vector store |
| `customParams` | Free-form parameters passed to the store (`alpha`, `mmrThreshold`) |

## Retrieve Wide, Then Narrow
The most reliable pattern:

```ts
import { SimilarityPostprocessor } from "llamaindex/postprocessors";
import { CohereRerank } from "@llamaindex/cohere";

const engine = index.asQueryEngine({
  similarityTopK: 30,
  nodePostprocessors: [
    new SimilarityPostprocessor({ similarityCutoff: 0.3 }),
    new CohereRerank({ apiKey: process.env.COHERE_API_KEY!, topN: 5 }),
  ],
});
```
High recall first (cheap, vector search), then precision (rerank). Postprocessors run in array order. `COHERE_API_KEY` is the conventional variable name, not one defined by this repo's `.env`. See `09-reranking/`.

## Score Cutoff (Similarity Threshold)
Drop nodes below a score so weak matches never reach the LLM.

```ts
import { SimilarityPostprocessor } from "llamaindex/postprocessors";

const cutoff = new SimilarityPostprocessor({ similarityCutoff: 0.5 });
const filtered = await cutoff.postprocessNodes(nodes);   // method name and sync/async: verify
```

**Scores are not comparable across setups.**
- Cosine scores depend on the embedding model and the data; 0.78 does not mean "78% relevant".
- BM25 scores are unbounded and depend on corpus statistics.
- Fusion (reciprocal rank) scores are rank-derived, not similarities.
- Rerankers output their own relevance scores.

How to set a cutoff:
1. Run 30 to 100 real queries.
2. Print the scores of the correct chunks versus the irrelevant ones.
3. Choose a cutoff that separates them, then validate on held-out queries.

Never copy a threshold from a tutorial. A cutoff that is too high yields empty context and "I don't know" answers; a cutoff that is too low does nothing.

## Metadata Filters
```ts
import { MetadataFilters } from "@llamaindex/core/vector-store";   // one doc page imports it from "llamaindex"; verify

const filters = new MetadataFilters({
  filters: [
    { key: "department", value: "HR", operator: "==" },
    { key: "year", value: 2024, operator: ">=" },
  ],
});
const retriever = index.asRetriever({ similarityTopK: 5, filters });
```
- Operator strings such as `"=="` and `">="` are documented; the full list is not (verify).
- Supported operators vary by vector store (verify for yours).
- Filtering shrinks the candidate pool and can return fewer than `similarityTopK` results. Handle empty and short lists.
- Types must match what was stored (`2024` number versus `"2024"` string).
- For tenant isolation, use namespaces and server-side enforcement, not client-chosen filters (`04-vector-databases/partitioning-and-filtering.md`).
- To let the LLM infer filters from the question, see [auto-retriever.md](auto-retriever.md) (hand-rolled in TypeScript).

## Diversity: MMR
**Maximal marginal relevance** trades relevance for diversity, avoiding five near-duplicate chunks.
```ts
const retriever = index.asRetriever({
  similarityTopK: 8,
  mode: VectorStoreQueryMode.MMR,
  customParams: { mmrThreshold: 0.5 },   // verify support in your vector store, including Pinecone
});
```
Support depends on the vector store; when unavailable, rerank or dedupe in a postprocessor.

## Debugging Retrieval Quickly
```ts
const nodes = await retriever.retrieve({ query: question });
for (const n of nodes) {
  const text = n.node.getContent(MetadataMode.NONE).slice(0, 120).replace(/\n/g, " ");
  console.log((n.score ?? 0).toFixed(3), n.node.metadata.source, "|", text);
}
```
Printing retrieved nodes costs no LLM calls, so do it before touching prompts. (`MetadataMode` comes from `@llamaindex/core/schema`.)

## Important Rules
- Retrieve for recall, then rerank for precision.
- Tune `similarityTopK` and cutoffs with data, not intuition.
- Different retrievers produce different score scales; do not compare scores across them.
- Handle zero and short result lists.

## Common Mistakes
- Leaving default top-k and never testing it.
- Copying a similarity threshold from a blog post.
- Comparing BM25 and cosine scores directly.
- Assuming a filter always leaves `similarityTopK` results.
- Judging answers while never reading the retrieved nodes.

## Related / Next
- [vector-retriever.md](vector-retriever.md)
- `09-reranking/reranking-fundamentals.md`
- `14-evaluation/retrieval-metrics.md`
- `15-debugging/retrieval-failure-modes.md`
