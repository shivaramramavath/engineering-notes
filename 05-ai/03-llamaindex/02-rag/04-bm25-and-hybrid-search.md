# BM25 and Hybrid Search

Vector search matches *meaning*. It is weak at exact tokens: error codes (`ERR_5012`), SKUs, function names, people's names, acronyms, version numbers. **BM25** is the classic keyword ranking function: it scores documents by how often query terms appear (with saturation), how rare those terms are across the corpus, and document length. It is fast, needs no embeddings, and is excellent when the query contains exactly the words the answer contains.

**Hybrid search** runs both and merges the results, so you get semantic matches and exact-term matches.

> Prerequisites: [03-retrievers-and-postprocessors](./03-retrievers-and-postprocessors.md).

## When BM25 beats vectors (and when it doesn't)

| Query | Better with |
|---|---|
| `error code E4021 meaning` | BM25 (the token E4021 is the signal) |
| `how do I get my money back` (doc says "refund policy") | Vectors (no shared words) |
| `invoice INV-2291 status` | BM25 |
| Mixed: `why does ERR_5012 happen on login` | Hybrid |

BM25 has no notion of synonyms or paraphrase, and it depends on tokenization (stemming, stopwords), which is tuned for English by default in most implementations. For other languages check what your implementation does.

## `Bm25Retriever` in LlamaIndex.TS

The TypeScript package ships a BM25 retriever as a separate package:

```bash
npm i @llamaindex/bm25-retriever
```

It reads nodes from a **document store** (not from your vector store). Its options, per the API reference, are a `docStore`, an optional `topK`, and an optional list of `docIds` to restrict the search.

```ts
import { Bm25Retriever } from "@llamaindex/bm25-retriever";
import { SimpleDocumentStore, SentenceSplitter, Document } from "llamaindex";

const nodes = new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 })
  .getNodesFromDocuments(documents);

const docStore = new SimpleDocumentStore();
await docStore.addDocuments(nodes, true); // true = allow overwrite

const bm25 = new Bm25Retriever({ docStore, topK: 5 });

const results = await bm25.retrieve({ query: "ERR_5012 login failure" });
for (const r of results) console.log(r.score, r.node.id_);
```

I have not verified the exact signature of `addDocuments` across versions (some versions take an optional second boolean argument). Check your installed type definitions.

Key facts to internalize:

- **BM25 needs the node text somewhere it can read.** In this package that means a docstore holding your nodes. If you ingest through a vector store that stores text itself (for example `PGVectorStore`), the library's docstore may be empty. Count what the docstore holds before trusting BM25 results.
- **The BM25 index is built in memory** from those nodes, so big corpora cost startup time and RAM. Rebuild or refresh it when your nodes change; stale BM25 data is a classic hybrid bug.
- **Scores are not on the same scale as vector similarity.** You cannot add or compare them directly. See below and [05-query-fusion](./05-query-fusion.md).
- Query-time filters on metadata are not automatic here. If you need tenant or access isolation, restrict with `docIds` or filter results afterwards, and test that BM25 cannot leak other tenants' nodes.

## Hybrid: two ways

### 1. Let the database do it

Some vector stores support hybrid or sparse queries natively. The package exports a `VectorStoreQueryMode` enum for choosing a query mode on stores that implement more than plain vector search, and the API reference includes hybrid-capable stores (for example Azure AI Search variants). If your store supports hybrid, that is usually the best option: one query, one index, native filtering. The way you select the mode on a retriever has changed between releases and is store-specific, so check your store's page rather than copying a snippet from elsewhere.

### 2. Run two retrievers and merge

Works with any store. The LlamaIndex TypeScript package does not include a ready-made fusion retriever that I could find (Python's `QueryFusionRetriever` has no TS equivalent), so you merge results yourself. A minimal and robust merge is reciprocal rank fusion, which only uses ranks and therefore ignores the incompatible score scales:

```ts
import type { NodeWithScore } from "llamaindex";

function rrf(lists: NodeWithScore[][], k = 60, topN = 5): NodeWithScore[] {
  const scores = new Map<string, { node: NodeWithScore; score: number }>();

  for (const list of lists) {
    list.forEach((item, rank) => {
      const id = item.node.id_;
      const entry = scores.get(id) ?? { node: item, score: 0 };
      entry.score += 1 / (k + rank + 1);
      scores.set(id, entry);
    });
  }

  return [...scores.values()]
    .sort((a, b) => b.score - a.score)
    .slice(0, topN)
    .map(({ node, score }) => ({ ...node, score }));
}

const query = "why does ERR_5012 happen on login";
const [vec, kw] = await Promise.all([
  vectorRetriever.retrieve({ query }),
  bm25.retrieve({ query }),
]);

const fused = rrf([vec, kw]);
```

To use the fused list inside a query engine, wrap it in a retriever (the next note shows a `BaseRetriever` subclass) or call your synthesizer on the fused nodes yourself.

Weighting, normalization-based fusion, and multi-query variants are in [05-query-fusion](./05-query-fusion.md).

## Practical guidance

- **Same chunks for both retrievers.** Node IDs must match across the vector and BM25 sides or fusion can't recognize duplicates. Build both from the same nodes.
- **Retrieve more than you need from each** (for example 20 each), fuse, then keep 5 to 8 and optionally rerank.
- **Measure.** Hybrid is not free: two retrievals, more code, an in-memory index to keep fresh. Test on queries that contain identifiers and on conceptual queries; if hybrid doesn't move your evaluation numbers, don't ship the complexity (see `04-production/02-testing-and-evaluation.md`).
- Run the two retrievals concurrently with `Promise.all`; latency should be roughly the slower of the two, plus fusion.

## Common mistakes

**Empty BM25 results because the docstore is empty.** Check the docstore actually contains nodes.

**Adding raw scores together.** BM25 and cosine scores are different quantities.

**Stale BM25 index.** New documents reach the vector store but not the in-memory BM25 corpus.

**Different chunking on each side.** Fusion deduplicates by node ID, so mismatched nodes just double up.

**Assuming hybrid fixes bad chunking.** It doesn't.

## Debugging

- Retrieve with each retriever separately for a failing query and print both ranked lists. Which side had the right node and at what rank?
- Verify exact-term queries: the BM25 side should find the identifier even when vectors don't.
- Log fused ranks alongside the originals to see whether fusion helps or hurts.
- If BM25 returns nothing at all, count nodes in the docstore and confirm the query isn't entirely stopwords.

## Quick Summary

- BM25 is lexical: great for identifiers and exact terms, blind to paraphrase.
- LlamaIndex.TS has `Bm25Retriever` (`@llamaindex/bm25-retriever`) that reads nodes from a docstore; options are `docStore`, `topK`, `docIds`.
- Hybrid = BM25 + vectors, either natively in a store that supports it or by merging two retrievers.
- Never combine raw scores; use rank-based fusion (RRF) or normalize first.
- Keep both indexes built from the same nodes and keep BM25 fresh.
- Prove the gain with evaluation before keeping the extra complexity.

## Next

[05-query-fusion.md](./05-query-fusion.md): multi-query retrieval and the main fusion algorithms (reciprocal rank, relative score, distribution-based), with weights.
