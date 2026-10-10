# Vector Retriever

## Concept
The default retriever. It embeds the query and returns the nodes whose vectors are nearest (dense, semantic search) from a `VectorStoreIndex`.

## When to Use
- Almost always as your first retriever and your baseline.
- Natural-language questions over prose where meaning matters more than exact wording.

## When It Falls Short
- Exact identifiers, error codes, rare names (add BM25 or hybrid).
- Negation and precise numbers.
- Chunks that are too small to carry context (auto-merging, sentence-window).
- Vague or multi-part questions (query transformation, fusion).

## Prerequisites
- [retrieval-basics.md](retrieval-basics.md)
- `06-llamaindex/indexes.md`

## Usage
```ts
const retriever = index.asRetriever({ similarityTopK: 5 });
const nodes = await retriever.retrieve({ query: "How many vacation days do I get?" });
```
`asRetriever` is the documented way to get a vector retriever. Python's explicit `VectorIndexRetriever` class is not documented for TypeScript; use `asRetriever`.

## Parameters

| Parameter | Meaning |
|---|---|
| `similarityTopK` | Number of nodes to return |
| `filters` | `MetadataFilters` applied in the vector store |
| `mode` | `VectorStoreQueryMode.DEFAULT` (dense), `HYBRID`, `MMR`, `SPARSE`, `SEMANTIC_HYBRID`; non-default modes depend on the store (verify) |
| `customParams` | Store-specific options, e.g. `{ alpha: 0.5 }` for hybrid or `{ mmrThreshold: 0.5 }` for MMR ([hybrid-retrieval.md](hybrid-retrieval.md)) |

Restricting by node or document ID is not documented for TypeScript; use a metadata filter on a document-ID field instead (verify).

## With Pinecone
```ts
import { VectorStoreIndex } from "llamaindex";
import { PineconeVectorStore } from "@llamaindex/pinecone";

const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME,
  namespace: "tenant-acme",
});
const index = await VectorStoreIndex.fromVectorStore(vectorStore);   // no re-embedding
const retriever = index.asRetriever({ similarityTopK: 8 });
```
The namespace is fixed on the vector store object. To search another tenant, build another store and index object. `PineconeVectorStore` creates its own Pinecone client from `PINECONE_API_KEY` (or an `apiKey` option). See `05-pinecone/namespaces-and-filters.md`.

## How It Works
1. The query string is embedded with the index's embed model.
2. The vector store runs a nearest-neighbor query (top-k, filters).
3. Matches are converted to `NodeWithScore` (the vector store returns node text and metadata, or LlamaIndex looks nodes up in the docstore).

Because the query is embedded with `Settings.embedModel` (or the model passed to the index), a mismatch with the ingestion model silently ruins results. See `02-embeddings/embeddings-basics.md`.

## Providing a Precomputed Embedding
Python allows passing an embedding inside a `QueryBundle`. Whether LlamaIndex.TS honors `QueryBundle.embedding` in the vector retriever is not documented (verify). If you need cached query embeddings, cache at the embed-model layer (wrap `embedModel.getTextEmbedding` with your own memoization) or query the Pinecone SDK directly with a vector (`05-pinecone/`).

## Tuning Checklist
- Chunk size and overlap (`03-data-ingestion/chunking.md`)
- `similarityTopK` (retrieve wide, rerank)
- Embedding model (`02-embeddings/embedding-models.md`)
- Metadata included in embedded text (`03-data-ingestion/metadata.md`)
- Filters and namespaces

## Important Rules
- Use the same embedding model as at ingestion.
- Check the retrieved nodes and scores on real questions.
- Add other retrievers only after you can name the failure.

## Common Mistakes
- Never raising `similarityTopK` from the small default.
- Embedding model mismatch between ingestion and query.
- Searching the wrong namespace.
- Blaming the LLM for retrieval failures.

## Related / Next
- [bm25-retriever.md](bm25-retriever.md)
- [hybrid-retrieval.md](hybrid-retrieval.md)
- `09-reranking/reranking-fundamentals.md`
