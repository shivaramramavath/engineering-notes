# Retrievers

> **Accuracy note.** TypeScript edition. API details come from LlamaIndex.TS and Pinecone documentation pages read on 2026-10-10 plus knowledge of the libraries. The code was NOT compiled or run (the sandbox could not install npm packages). Import paths in particular vary between versions and doc pages; verify against your installed version. Several retrievers that exist in the Python framework are not in LlamaIndex.TS; the "In LlamaIndex.TS?" column below says which.

## Concept
A **retriever** takes a query and returns the most relevant nodes. It is the single biggest determinant of RAG quality: if the right text is not retrieved, nothing downstream can fix it.

```ts
const nodes = await retriever.retrieve({ query: "What is the refund policy?" });   // NodeWithScore[] (docs also show a plain string; verify)
for (const n of nodes) {
  console.log(n.score, n.node.metadata, n.node.getContent(MetadataMode.NONE).slice(0, 80));
}
```

A retriever is **not** a query engine. It stops at "which nodes"; the query engine adds postprocessing and the LLM answer (`06-llamaindex/query-engine.md`). Any retriever below plugs into `new RetrieverQueryEngine({ retriever })`.

## Prerequisites
- `06-llamaindex/indexes.md`
- `06-llamaindex/query-engine.md`

## Retriever Chooser

"In LlamaIndex.TS?" values: **Yes** = a documented class or option; **Hand-rolled** = not in the library, this repo shows how to build it; **Not available** = no library support, concept plus build-it-yourself pointer only.

| Problem you have | Retriever | In LlamaIndex.TS? | File |
|---|---|---|---|
| Default semantic search | Vector retriever | Yes (`index.asRetriever()`) | [vector-retriever.md](vector-retriever.md) |
| Exact terms, codes, names are missed | BM25 | Yes (`Bm25Retriever` from `@llamaindex/bm25-retriever`) | [bm25-retriever.md](bm25-retriever.md) |
| Both meaning and exact terms matter | Hybrid | Partly: store-dependent `VectorStoreQueryMode.HYBRID` (verify); robust path is hand-rolled RRF over BM25 + vector | [hybrid-retrieval.md](hybrid-retrieval.md) |
| One phrasing of the query misses relevant text | Query fusion (multi-query) | Hand-rolled in this repo | [query-fusion-retriever.md](query-fusion-retriever.md) |
| Small chunks match well but lack context | Auto-merging | Hand-rolled in this repo | [auto-merging-retriever.md](auto-merging-retriever.md) |
| Search summaries or tables, then follow links to full data | Recursive | Hand-rolled in this repo | [recursive-retriever.md](recursive-retriever.md) |
| Users filter by implied metadata ("2024 reports") | Auto-retriever | Hand-rolled in this repo | [auto-retriever.md](auto-retriever.md) |
| Several data sources; choose per query | Router | Yes (`RouterQueryEngine` + `LLMSingleSelector`; no retriever-level router documented) | [router-retriever.md](router-retriever.md) |
| Whole-document or whole-corpus summaries | Summary retriever | Yes (`SummaryIndex`, returns all nodes); `DocumentSummaryIndex` not documented for TS | [summary-retriever.md](summary-retriever.md) |
| Questions about entity relationships | Property graph | Not available | [property-graph-retriever.md](property-graph-retriever.md) |
| Small exact-keyword lookups, learning | Keyword table | Yes (`KeywordTableIndex`, mode `rake` / `simple`) | [keyword-table-retriever.md](keyword-table-retriever.md) |
| None of these fit | Custom | Yes (extend `BaseRetriever`) | [custom-retriever.md](custom-retriever.md) |

Shared concepts (top-k, cutoffs, filters, scores): [retrieval-basics.md](retrieval-basics.md).

## Escalation Path
Do not start with the most advanced retriever. Escalate when evaluation shows a specific failure.

```
1. Vector retriever, tuned similarityTopK and chunking
2. + Reranker (09-reranking) for precision
3. + Hybrid (BM25 or sparse) when exact terms are missed
4. + Query fusion when phrasing variation hurts recall
5. + Auto-merging / sentence-window when chunk context is the problem
6. + Router / recursive / graph for multi-source or relational data
```
Measure each step with `14-evaluation/retrieval-metrics.md`. If a step does not move the metrics on your data, drop it. Note that steps 4 to 6 mostly mean writing your own code in TypeScript.

## Composition
Retrievers wrap other retrievers:

```
Router ─► chooses among ─► [Hybrid( Vector + BM25 ) , Summary, Graph]
Fusion ─► runs many queries over ─► [Vector, BM25]
Auto-merging ─► wraps ─► Vector retriever over leaf nodes
Recursive ─► follows parent links into ─► other retrievers or query engines
```
In LlamaIndex.TS the router operates on query engines (`RouterQueryEngine`), not retrievers. Rerankers and cutoffs are **postprocessors**, applied after any retriever (`09-reranking/`).

## Comparison

| Retriever | Needs LLM at query time | Extra ingestion cost | Extra storage | Main benefit | In LlamaIndex.TS? |
|---|---|---|---|---|---|
| Vector | No | None | None | Semantic match | Yes |
| BM25 | No | None (builds in-memory index) | Nodes in memory | Exact terms | Yes (separate package) |
| Hybrid | No | Sparse encoding or BM25 build | Sparse vectors or BM25 index | Both strengths | Hand-rolled (RRF) |
| Query fusion | Yes (query generation) | None | None | Recall across phrasings | Hand-rolled |
| Auto-merging | No | Hierarchical parsing (by hand) | Docstore / Map with all nodes | Context without large chunks | Hand-rolled |
| Recursive | Maybe | Build parent links (summaries cost LLM calls) | Docstore / node Map | Hierarchies, tables | Hand-rolled |
| Auto-retriever | Yes | Metadata schema | None | Natural-language filters | Hand-rolled |
| Router | Yes (selection) | None | None | Multi-source choice | Yes (`RouterQueryEngine`) |
| Summary | Maybe | None | None | Whole-document coverage | Yes (`SummaryIndex`) |
| Property graph | Yes (extraction + maybe query) | High (entity extraction) | Graph store | Relationships | Not available |

## Interview Angle
"How would you improve retrieval?" A strong answer names a failure first (missed exact terms, lost context, ambiguous query), then picks the matching retriever, and says how it would be measured. Bonus: knowing which of these your framework ships and which you must build.

## Related / Next
- [retrieval-basics.md](retrieval-basics.md)
- `08-query-transformation/README.md`
- `09-reranking/reranking-fundamentals.md`
- `15-debugging/retrieval-failure-modes.md`
