# The RAG Pipeline

## Concept
A RAG system has two phases: an **offline ingestion** phase that prepares your data, and an **online query** phase that retrieves context and generates an answer.

```
OFFLINE (ingestion)
  Load ──► Clean ──► Chunk ──► Embed ──► Store (vector DB)

ONLINE (query)
  Question ──► Embed ──► Retrieve ──► (Rerank) ──► Build prompt ──► LLM ──► Answer
```

## Prerequisites
- [what-is-rag.md](what-is-rag.md)

## Components

| Component | Job | Covered in |
|---|---|---|
| Loader | Read raw files, web pages, DBs into documents | `03-data-ingestion/loaders.md` |
| Chunker / node parser | Split documents into retrievable pieces | `03-data-ingestion/chunking.md` |
| Embedding model | Turn text into vectors | `02-embeddings/` |
| Vector store | Store vectors and metadata; run similarity search | `04-vector-databases/`, `05-pinecone/` |
| Index | The LlamaIndex.TS structure that organizes nodes | `06-llamaindex/indexes.md` |
| Retriever | Fetch the most relevant nodes for a query | `07-retrievers/` |
| Query transformer | Rewrite or decompose the question first | `08-query-transformation/` |
| Reranker | Reorder retrieved nodes by true relevance | `09-reranking/` |
| Response synthesizer | Combine nodes and question into a prompt, call the LLM | `06-llamaindex/response-synthesizer.md` |
| LLM | Write the final answer | `10-context-and-generation/` |

## Stage by Stage

**1. Ingestion (offline)**
- Load raw sources into `Document` objects.
- Clean noise (headers, footers, boilerplate).
- Chunk into nodes (`TextNode`s), each carrying metadata.
- Embed each node; upsert vectors plus metadata into the vector store.
- Run again (incrementally) whenever documents change. See `11-document-management/`.

**2. Retrieval (online)**
- Embed the user's question with the *same* embedding model used at ingestion.
- Search for the top-k nearest vectors, optionally applying metadata filters.
- Optionally combine with keyword search (hybrid) and rerank the results.

**3. Generation (online)**
- Assemble a prompt: instructions + retrieved context + question.
- The LLM produces an answer grounded in that context.
- Optionally return source citations.

## The Whole Pipeline in LlamaIndex.TS
Ingestion and querying are usually two separate scripts or services. Package names and import paths vary by version; verify.

```ts
// ingest.ts
import { Settings, SentenceSplitter, VectorStoreIndex } from "llamaindex";
import { OpenAI, OpenAIEmbedding } from "@llamaindex/openai";
import { PineconeVectorStore } from "@llamaindex/pinecone";
import { SimpleDirectoryReader } from "@llamaindex/readers/directory";

Settings.llm = new OpenAI({ model: "gpt-4o-mini" });                  // verify model names
Settings.embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });
Settings.nodeParser = new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 });

const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME,
  namespace: process.env.PINECONE_NAMESPACE,
});
const documents = await new SimpleDirectoryReader().loadData({ directoryPath: "./data" });
await VectorStoreIndex.fromDocuments(documents, { storageContext: { vectorStore } }); // verify storageContext form
```

```ts
// query.ts  (same Settings.embedModel as ingestion!)
const index = await VectorStoreIndex.fromVectorStore(vectorStore);   // connects, no re-embedding
const retriever = index.asRetriever({ similarityTopK: 5 });
const nodes = await retriever.retrieve({ query: "What is the refund window?" });   // check retrieval first
const engine = index.asQueryEngine({ retriever });
const response = await engine.query({ query: "What is the refund window?" });
console.log(response.toString());
```

Run with `npx tsx --env-file=.env ingest.ts`. Keys come from `OPENAI_API_KEY`, `PINECONE_API_KEY`, `PINECONE_INDEX_NAME`, `PINECONE_NAMESPACE`.

## Retrieval vs Generation
Treat these as two separately debuggable systems:

| | Retrieval | Generation |
|---|---|---|
| Question it answers | "Did we find the right text?" | "Did the model use it correctly?" |
| Typical failures | Wrong chunks, missing chunks, bad chunk boundaries | Ignores context, hallucinates, bad formatting |
| Metrics | Precision, recall, MRR, NDCG | Faithfulness, answer relevance |

If retrieval is wrong, no prompt will fix generation. **Always check retrieval first** (call `retriever.retrieve` and print the nodes before blaming the LLM).

## Important Rules
- Use the **same embedding model** for documents and queries.
- Keep chunk metadata (source, page, doc ID); you need it for filtering, citations and updates.
- Ingestion and query run on different schedules and should be built as separate pipelines.

## Common Mistakes
- Debugging the prompt when the right chunk was never retrieved.
- Changing the embedding model without re-embedding the whole index.
- Dropping metadata during chunking, which makes updates and filtering impossible later.

## Related / Next
- [documents-and-nodes.md](documents-and-nodes.md)
- `02-embeddings/embeddings-basics.md`
