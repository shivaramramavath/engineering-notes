# Keyword Table Retriever

## Concept
`KeywordTableIndex` extracts keywords from each node and builds a lookup table `keyword → nodes`. At query time the retriever extracts keywords from the question and returns nodes that share them.

```
Node 1 → {refund, policy, 30-day}
Node 2 → {vacation, accrual}
query "refund window" → keywords {refund, window} → Node 1
```

## When to Use
- Small corpora where simple keyword lookup is enough.
- Teaching: it shows the keyword-matching idea without embeddings.
- Situations needing a transparent, explainable match (you can see which keywords matched).

## When Not to Use
For production keyword search, prefer **BM25** ([bm25-retriever.md](bm25-retriever.md)) or sparse vectors. They rank by term importance and scale better. The keyword table has no frequency weighting, and some modes need LLM calls.

## Prerequisites
- `06-llamaindex/indexes.md`
- [bm25-retriever.md](bm25-retriever.md)

## Usage
`KeywordTableIndex` is documented for LlamaIndex.TS, with a query engine `mode` of `"rake"` or `"simple"`.

```ts
import { KeywordTableIndex } from "llamaindex";

const index = await KeywordTableIndex.fromDocuments(documents);

const engine = index.asQueryEngine({ mode: "simple" });   // or "rake"; see Modes
const response = await engine.query({ query: "refund window" });
console.log(response.toString());
```
The option name (`mode`), its placement, and whether a retriever form (`index.asRetriever(...)`) accepts the same option are from the docs for the query engine only (verify for your version). The Python option names (`retriever_mode`, `GPT` default mode) do not carry over.

## Modes

| Mode | Extraction | Notes |
|---|---|---|
| `simple` | Regex-style keyword extraction | No LLM involved per the Python design; verify in TS |
| `rake` | RAKE algorithm | Python needs `rake-nltk`; whether the TS build bundles its own implementation is undocumented (verify) |

Whether `KeywordTableIndex.fromDocuments` itself calls the LLM to extract keywords at build time (as the Python default does) is not documented for TypeScript. Test on 5 documents and watch your LLM usage dashboard before indexing a large corpus.

## Trade-offs
| | Keyword table | BM25 | Vector |
|---|---|---|---|
| Understands meaning | No | No | Yes |
| Ranks by term importance | No | Yes | n/a |
| Ingestion cost | Possibly high (verify) | Low | Embedding cost |
| Scales to large corpora | Poorly | Well | Well |
| Exact match | Yes | Yes | Weak |

## Important Rules
- Treat this as a niche tool. Start with vector plus BM25.
- Use `simple` or `rake` modes to avoid LLM cost at query time.

## Common Mistakes
- Building over thousands of nodes before measuring the LLM cost of index creation.
- Choosing it for production keyword search over BM25.
- Expecting ranking quality comparable to BM25.
- Copying Python option names (`retriever_mode`) into TypeScript.

## Related / Next
- [bm25-retriever.md](bm25-retriever.md)
- [hybrid-retrieval.md](hybrid-retrieval.md)
