# Sparse Vectors and Hybrid Search

## Concept
- **Dense vectors** (embeddings) capture meaning. Good at paraphrase, weak on exact terms.
- **Sparse vectors** capture which words appear and how important they are (BM25/TF-IDF style). Good at exact terms, codes, names; blind to synonyms.
- **Hybrid search** combines both, so you get semantic matches *and* exact-term matches.

General concepts: `07-retrievers/hybrid-retrieval.md`. This file covers how Pinecone represents and queries them.

## Prerequisites
- [query.md](query.md)
- `07-retrievers/bm25-retriever.md`

## Dense vs Sparse

| | Dense | Sparse |
|---|---|---|
| Representation | Fixed-length list of floats (e.g. 1536) | Indices of words plus weights; mostly zeros |
| Finds | Similar meaning | Matching terms |
| Fails on | Product codes, error IDs, rare names | Synonyms, paraphrases |
| Produced by | Embedding model | BM25/TF-IDF code or learned sparse model |

## Two Ways to Do Hybrid in Pinecone

### A. One index holding both (sparse-dense vectors)
Each record carries a dense vector and sparse values. The index uses the `dotproduct` metric.

```ts
await pc.createIndex({
  name: "docs-hybrid",
  dimension: 1536,
  metric: "dotproduct",                      // required for sparse-dense queries
  spec: { serverless: { cloud: "aws", region: "us-east-1" } },
});

await index.upsert({
  records: [
    {
      id: "a#0",
      values: denseVec,
      sparseValues: { indices: [10, 45, 3000], values: [0.5, 0.3, 0.8] }, // verify exact type in your SDK version
      metadata: { text: "..." },
    },
  ],
});
```

Query with both, weighting by an `alpha` you apply yourself:
```ts
interface SparseVec { indices: number[]; values: number[] }

function weight(dense: number[], sparse: SparseVec, alpha: number) {
  // alpha = 1.0 -> all dense; alpha = 0.0 -> all sparse
  return {
    vector: dense.map((v) => v * alpha),
    sparseVector: { indices: sparse.indices, values: sparse.values.map((v) => v * (1 - alpha)) },
  };
}

const { vector, sparseVector } = weight(queryDense, querySparse, 0.6);
const res = await index.query({ vector, sparseVector, topK: 10, includeMetadata: true });
```

### B. Separate dense and sparse indexes, merge results
Query a dense index and a sparse index independently, merge the two ranked lists (for example with reciprocal rank fusion), then **rerank**. Pinecone has been steering toward this pattern, since it is flexible and allows a reranker to settle the final order (verify current recommendation).

| | A. Single sparse-dense index | B. Separate indexes + rerank |
|---|---|---|
| Simplicity | Simpler | More parts |
| Tuning | `alpha` weighting | Fusion + reranker |
| Flexibility | Limited | High |
| Quality ceiling | Good | Often better with a reranker |

## Producing Sparse Vectors
The popular BM25 encoder, `pinecone-text`, is a **Python library**. There is no TypeScript port to rely on. Two TypeScript options:

**Option 1: compute sparse vectors yourself.** A small tokenizer plus TF-IDF/BM25-style weights. Hash each token to an integer index (so no vocabulary file is needed), and keep corpus statistics for IDF.

```ts
const STOP = new Set(["the", "a", "an", "and", "or", "of", "to", "in", "is", "for"]);

function tokenize(text: string): string[] {
  return text.toLowerCase().match(/[a-z0-9]+/g)?.filter((t) => !STOP.has(t)) ?? [];
}

// 32-bit FNV-1a hash -> sparse index. Collisions are possible but rare enough for a start.
function hashToken(token: string): number {
  let h = 0x811c9dc5;
  for (let i = 0; i < token.length; i++) {
    h ^= token.charCodeAt(i);
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  return h;
}

class Bm25Encoder {
  private df = new Map<number, number>();
  private n = 0;
  private avgLen = 0;
  constructor(private k1 = 1.2, private b = 0.75) {}

  fit(corpus: string[]): void {            // fit on the SAME corpus you index
    let total = 0;
    for (const doc of corpus) {
      const toks = tokenize(doc);
      total += toks.length;
      for (const id of new Set(toks.map(hashToken))) this.df.set(id, (this.df.get(id) ?? 0) + 1);
    }
    this.n = corpus.length;
    this.avgLen = total / Math.max(1, this.n);
  }

  encodeDocument(text: string): SparseVec {
    const toks = tokenize(text);
    const tf = new Map<number, number>();
    for (const t of toks) tf.set(hashToken(t), (tf.get(hashToken(t)) ?? 0) + 1);
    const norm = this.k1 * (1 - this.b + (this.b * toks.length) / this.avgLen);
    const indices: number[] = [];
    const values: number[] = [];
    for (const [id, f] of tf) {
      indices.push(id);
      values.push((f * (this.k1 + 1)) / (f + norm));   // saturated term frequency
    }
    return { indices, values };
  }

  encodeQuery(text: string): SparseVec {   // IDF weight goes on the query side
    const tf = new Set(tokenize(text).map(hashToken));
    const indices: number[] = [];
    const values: number[] = [];
    for (const id of tf) {
      const df = this.df.get(id) ?? 0;
      indices.push(id);
      values.push(Math.log(1 + (this.n - df + 0.5) / (df + 0.5)));
    }
    return { indices, values };
  }
}
```
Sparse indices must be unique per vector and within the allowed integer range (verify the limits; a 32-bit unsigned hash fits the documented range as far as I know, but check). This is a teaching-grade encoder, not a tuned one; evaluate it on your data.

**Option 2: Pinecone's hosted sparse embedding.** Pinecone's inference API can produce learned sparse vectors, exposed in the TypeScript SDK under `pc.inference`:
```ts
const out = await pc.inference.embed(   // verify: method name, signature and model names per SDK version
  "pinecone-sparse-english-v0",         // verify: model names change
  ["first passage", "second passage"],
  { inputType: "passage" },
);
// each item carries sparse indices and values; verify the field names (e.g. sparseIndices/sparseValues)
```
Use `inputType: "query"` for queries. Mark every detail here as verify.

Whatever you choose, encode documents and queries with the **same** sparse encoder, fitted on the same corpus.

## LlamaIndex.TS
The Pinecone integration in LlamaIndex.TS has no documented sparse-vector or hybrid support. `VectorStoreQueryMode.HYBRID` exists, but whether it does anything depends on the vector store (verify for `PineconeVectorStore`).

```ts
import { VectorStoreQueryMode } from "@llamaindex/core/vector-store";

const retriever = liIndex.asRetriever({
  similarityTopK: 5,
  mode: VectorStoreQueryMode.HYBRID,   // verify that PineconeVectorStore honors this
  customParams: { alpha: 0.5 },        // verify
});
```
The Python framework has a documented Pinecone hybrid setup (sparse encoder configured on the store). LlamaIndex.TS does not document the equivalent. The robust paths are:
1. Use the raw Pinecone SDK as shown above (option A), wrapped in a custom `BaseRetriever` if you need it inside a LlamaIndex.TS query engine.
2. Use `Bm25Retriever` (`@llamaindex/bm25-retriever`) plus the Pinecone vector retriever, fused with a hand-rolled RRF retriever (option B).

Hybrid retrieval is covered conceptually in `07-retrievers/hybrid-retrieval.md`.

## Tuning `alpha`
- Start around 0.5, then evaluate.
- Lean sparse (lower alpha) for corpora full of identifiers, part numbers, code.
- Lean dense (higher alpha) for natural-language questions over prose.
- Tune per corpus using retrieval metrics, not intuition (`14-evaluation/`).

## Important Rules
- Sparse-dense queries on one index require `dotproduct`.
- Dense and sparse parts must each come from consistent encoders.
- Hybrid raises recall; reranking raises precision. They work well together.

## Common Mistakes
- Creating a `cosine` index, then trying sparse-dense queries.
- Fitting the BM25 encoder on a different corpus than the one indexed (or changing the tokenizer between indexing and querying).
- Looking for `pinecone-text` on npm. It is Python-only.
- Reaching for hybrid before checking whether plain dense retrieval already meets your quality bar.
- Never tuning `alpha`.

## Related / Next
- `07-retrievers/hybrid-retrieval.md`
- `09-reranking/reranking-fundamentals.md`
- `15-debugging/retrieval-failure-modes.md`
