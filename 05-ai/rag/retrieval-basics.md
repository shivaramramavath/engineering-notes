# Hybrid Retrieval

## Concept
**Hybrid retrieval** combines dense (semantic) search with sparse (keyword) search so that you get both meaning-based and exact-term matches.

| | Dense | Sparse (BM25 / sparse vectors) |
|---|---|---|
| Finds | Similar meaning | Matching words |
| Misses | Rare terms, IDs, exact phrases | Synonyms, paraphrase |

Hybrid is the most common first upgrade over plain vector search, because real corpora contain both natural language and exact identifiers.

## When to Use
- Questions mix concepts and specifics ("how do I fix error E4012 in the billing module?").
- Product names, codes, acronyms and jargon matter.
- Evaluation shows relevant chunks missed by dense search that BM25 would catch.

## Prerequisites
- [vector-retriever.md](vector-retriever.md)
- [bm25-retriever.md](bm25-retriever.md)
- `05-pinecone/sparse-and-hybrid.md`

## Three Ways to Build It

### 1. Vector-store-native hybrid (store-dependent, verify)
`VectorStoreQueryMode.HYBRID` exists in LlamaIndex.TS, but the documentation says only that support depends on the vector store.
```ts
import { VectorStoreQueryMode } from "@llamaindex/core/vector-store";   // path varies; verify

const retriever = index.asRetriever({
  similarityTopK: 8,
  mode: VectorStoreQueryMode.HYBRID,
  customParams: { alpha: 0.5 },     // intended meaning: 1.0 = dense only, 0.0 = sparse only
});
```
**Pinecone: do not assume this works.** The `@llamaindex/pinecone` integration has no documented sparse-vector support, so the mode may silently fall back to dense-only or ignore `alpha`. Verify by reading the integration source for your installed version and by testing: run a query with an exact rare token and compare `DEFAULT` against `HYBRID` results. If you need Pinecone-native hybrid, use the raw Pinecone SDK with `sparseValues` on a `dotproduct` index (`05-pinecone/sparse-and-hybrid.md`) and wrap it in a custom retriever ([custom-retriever.md](custom-retriever.md)).

### 2. Two retrievers fused client-side (robust path)
Run a vector retriever and a `Bm25Retriever`, then merge by rank with reciprocal rank fusion. LlamaIndex.TS has no `QueryFusionRetriever`, so the fusion is a small function you own. The helper below is self-contained; [query-fusion-retriever.md](query-fusion-retriever.md) builds on the same idea.

```ts
import { Bm25Retriever } from "@llamaindex/bm25-retriever";
import type { NodeWithScore } from "@llamaindex/core";   // path varies by version; verify

/**
 * Reciprocal Rank Fusion. Each inner list must be ordered best-first.
 * Score of a node = sum over lists of 1 / (k + rank), rank starting at 1.
 */
export function reciprocalRankFusion(lists: NodeWithScore[][], k = 60): NodeWithScore[] {
  const scores = new Map<string, number>();
  const best = new Map<string, NodeWithScore>();

  for (const list of lists) {
    list.forEach((item, i) => {
      const id = item.node.id_;
      scores.set(id, (scores.get(id) ?? 0) + 1 / (k + i + 1));
      if (!best.has(id)) best.set(id, item);      // keep the first copy seen
    });
  }

  return [...scores.entries()]
    .sort((a, b) => b[1] - a[1])
    .map(([id, score]) => ({ node: best.get(id)!.node, score }));   // NodeWithScore shape; verify it is a plain interface
}

const vectorRetriever = index.asRetriever({ similarityTopK: 20 });
const bm25Retriever = new Bm25Retriever({ docStore: index.docStore, topK: 20 });

const query = "how do I fix error E4012 in the billing module?";
const [dense, sparse] = await Promise.all([
  vectorRetriever.retrieve({ query }),
  bm25Retriever.retrieve({ query }),
]);

const fused = reciprocalRankFusion([dense, sparse]).slice(0, 10);
```
Two caveats:
- `Bm25Retriever` needs the nodes in a local docstore. With Pinecone alone, `index.docStore` may be empty, so read the caveat in [bm25-retriever.md](bm25-retriever.md) first.
- Node IDs must match across both retrievers (same node set), otherwise duplicates will not merge.

To plug the fused result into a query engine, wrap the same logic in a `BaseRetriever` subclass ([custom-retriever.md](custom-retriever.md)) and pass it to `new RetrieverQueryEngine({ retriever })`.

### 3. Separate stores, merge, rerank
Dense results from Pinecone, keyword results from a search engine, merge by rank (the same helper works), then rerank with a cross-encoder or Cohere (`09-reranking/`). Most flexible and often highest quality, at the cost of more moving parts.

## Combining Scores: Why Rank-Based Fusion
Dense and sparse scores live on different scales, so averaging them is unreliable. **Reciprocal Rank Fusion (RRF)** uses only ranks:

```
RRF(d) = Σ over retrievers r of  1 / (k + rank_r(d))      (commonly k = 60)
```
A document ranked highly by either retriever scores well; one ranked highly by both scores best. No score normalization is needed, which is why RRF is a strong default.

`alpha` weighting (approach 1) does work, but needs scores to be comparable, so tune it with evaluation.

## Tuning

| Knob | Effect |
|---|---|
| `alpha` (native hybrid) | Lean sparse for identifier-heavy corpora; lean dense for prose |
| Per-retriever `similarityTopK` / `topK` | Retrieve generously (10 to 30 each) before fusing |
| Fused list length | Keep more if you will rerank afterward |
| RRF `k` | Larger `k` flattens the influence of rank differences; 60 is the usual default |
| Reranker | Often gives the largest quality gain on top of hybrid |

## Evaluating Whether Hybrid Helps
1. Build an evaluation set with questions containing specifics (IDs, names) and questions in plain language.
2. Compare recall@k for dense-only versus hybrid.
3. Keep hybrid only if it improves the metrics you care about on your data.
(`14-evaluation/retrieval-metrics.md`)

## Important Rules
- Combine by rank when scales differ.
- Keep the sparse index synchronized with the dense index on updates and deletes.
- Retrieve wide, fuse, then rerank.
- Encode queries and documents with the same sparse encoder.
- Do not trust `VectorStoreQueryMode.HYBRID` on a store until you have tested it.

## Common Mistakes
- Averaging raw BM25 and cosine scores.
- Assuming `HYBRID` mode does real hybrid search on Pinecone.
- Adding hybrid before checking that dense retrieval actually misses exact terms.
- Letting sparse and dense indexes drift apart after updates.
- Never tuning `alpha`.

## Related / Next
- [query-fusion-retriever.md](query-fusion-retriever.md)
- [custom-retriever.md](custom-retriever.md)
- `09-reranking/reranking-fundamentals.md`
- `05-pinecone/sparse-and-hybrid.md`
- `15-debugging/retrieval-failure-modes.md`
