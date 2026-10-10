# BM25 Retriever

## Concept
**BM25** is a classic keyword ranking function. It scores a document by how often query terms appear in it, how rare those terms are across the corpus, and how long the document is. It does **not** understand meaning; it matches words.

```
query: "ERR_CONN_RESET 4012"
→ ranks chunks that literally contain ERR_CONN_RESET and 4012
```

## When to Use
- Exact tokens: error codes, SKUs, function names, legal clause numbers, person or product names.
- Domain jargon that embedding models handle poorly.
- As one half of hybrid retrieval ([hybrid-retrieval.md](hybrid-retrieval.md)).
- A cheap, explainable baseline.

## When It Falls Short
- Synonyms and paraphrase ("car" versus "automobile").
- Questions phrased very differently from the source text.
- Multilingual corpora without language-specific tokenization.

## Prerequisites
- [retrieval-basics.md](retrieval-basics.md)
- `03-data-ingestion/chunking.md`

## Install and Use
```bash
npm i @llamaindex/bm25-retriever
```
```ts
import { Bm25Retriever } from "@llamaindex/bm25-retriever";

// From an index's docstore
const bm25 = new Bm25Retriever({ docStore: index.docStore, topK: 5 });

const results = await bm25.retrieve({ query: "ERR_CONN_RESET 4012" });
```
Only the `docStore` and `topK` options are documented. Whether stemming, language or custom tokenization options exist is not documented; check the package source for your version (verify). The package is separate from `llamaindex`, so check it is compatible with your installed core version.

## Persisting the BM25 Index
The documented constructor builds from a docstore, and no persistence API for the BM25 index itself is documented for TypeScript (the Python version has `persist`; do not assume it here). Persist the **docstore** instead and rebuild the retriever at startup:
```ts
import { SimpleDocumentStore } from "llamaindex/storage";

const docStore = SimpleDocumentStore.fromPersistPath("./storage/doc_store.json");
// ...build or load the index with this docStore, then:
const bm25 = new Bm25Retriever({ docStore, topK: 5 });
```
Whether `Bm25Retriever` indexes lazily or at construction (and so how costly the rebuild is) is not documented (verify). Measure startup time on your corpus.

## Important: BM25 Needs the Text Locally
BM25 builds its own in-memory index from node text in the docstore. With a Pinecone-only setup, LlamaIndex.TS does not keep nodes locally: Pinecone stores the vectors and text in metadata, while `index.docStore` can be empty (see `06-llamaindex/storage.md`). If the docstore is empty, BM25 returns nothing. Choose one:

| Option | Notes |
|---|---|
| Keep nodes in a docstore and build BM25 from it | Straightforward; needs a shared, persisted docstore in production (`SimpleDocumentStore` is file-based and single-process) |
| Rebuild from source documents at app start | Fine for small corpora; slow for large ones |
| Use Pinecone **sparse vectors** via the raw SDK | Keeps everything in one store (`05-pinecone/sparse-and-hybrid.md`); wrap in a custom retriever |
| Use a search engine with built-in BM25 (Elasticsearch, OpenSearch) | Adds infrastructure; wrap in a custom retriever |

Always check `nodes.length > 0` in a smoke test so an empty docstore does not look like "no matches".

## Updates and Multi-Tenancy
- The BM25 index does not update itself when you add or delete documents; rebuild or re-create the retriever in your sync process (`11-document-management/refresh-and-sync.md`).
- BM25 over a shared corpus has no namespace concept. For multi-tenant apps, build one docstore and retriever per tenant, or filter results afterward and over-fetch.

## Scores
BM25 scores are unbounded and depend on the corpus. Do not compare them with cosine scores, and do not reuse a cosine cutoff. Combine with vector results by **rank** (reciprocal rank fusion), not raw score ([hybrid-retrieval.md](hybrid-retrieval.md) has a TypeScript helper).

## Important Rules
- Tokenize and (optionally) stem consistently for index and query.
- Keep the BM25 index synchronized with the vector store.
- Combine, don't replace: BM25 plus vector is usually stronger than either.

## Common Mistakes
- Building BM25 from a different node set than the vector index.
- Passing an empty docstore (typical with Pinecone-only setups) and getting zero results.
- Letting it go stale after document updates.
- Averaging BM25 and cosine scores directly.
- Expecting it to match synonyms.

## Related / Next
- [hybrid-retrieval.md](hybrid-retrieval.md)
- [query-fusion-retriever.md](query-fusion-retriever.md)
- `05-pinecone/sparse-and-hybrid.md`
