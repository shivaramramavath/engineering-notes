# Embedding Strategies

## Concept
Beyond picking a model, *what* you embed and *how* you embed it strongly affects retrieval. This file covers practical strategies and best practices.

## Prerequisites
- [embeddings-basics.md](embeddings-basics.md)
- [embedding-models.md](embedding-models.md)

## Strategy 1: Decide What Text Gets Embedded
LlamaIndex.TS lets you control what is embedded versus what is only stored.

```ts
import { Document } from "llamaindex";

const doc = new Document({
  text: "Employees accrue 1.5 vacation days per month.",
  metadata: { source: "hr-policy.pdf", department: "HR", internalId: "x-9921" },
});

doc.excludedEmbedMetadataKeys = ["internalId"];   // not embedded
doc.excludedLlmMetadataKeys = ["internalId"];     // not shown to the LLM
// Verify: some versions may also accept these as constructor options.
```

- Metadata that helps meaning (title, section, product name) can improve retrieval when included in the embedded text.
- Noisy metadata (IDs, hashes, timestamps) dilutes the embedding. Exclude it.

## Strategy 2: Embed a Representation, Return the Original
Sometimes the best text to *search* is not the best text to *answer from*.

| Approach | Embed | Return to LLM |
|---|---|---|
| Summary embedding | Short summary of a chunk or document | Full original text |
| Hypothetical questions | Questions the chunk could answer | The chunk |
| Sentence window | A single sentence | The sentence plus its neighbors |
| Parent-child | Small child chunks | Larger parent chunk |

Sentence window is supported with `SentenceWindowNodeParser` plus `MetadataReplacementPostProcessor`. The other three patterns have no ready-made class in LlamaIndex.TS (the Python framework has recursive and auto-merging retrievers; the TS docs do not). You can build them by hand with a metadata `parentId` and a `Map` lookup. These feed the retrievers in `07-retrievers/`.

## Strategy 3: Match Chunk Size to the Model
- Stay within the model's max input tokens.
- Smaller chunks give sharper, more specific embeddings; larger chunks carry more context but blur topics.
- Tune chunk size using evaluation, not guesswork (see `03-data-ingestion/chunking.md`).

## Strategy 4: Batch and Cache Embedding Calls
Ingestion embeds every chunk, so cost and speed matter.

```ts
import { IngestionPipeline, IngestionCache, SentenceSplitter } from "llamaindex";
import { OpenAIEmbedding } from "@llamaindex/openai";

const embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });

const pipeline = new IngestionPipeline({
  transformations: [new SentenceSplitter({ chunkSize: 512 }), embedModel],
  cache: new IngestionCache("embeddings"),   // unchanged chunks are not re-embedded (verify constructor)
});
const nodes = await pipeline.run({ documents });
```

- The cache is on by default and in-memory by default, keyed by a hash of the input nodes plus the transformation config. In-memory means it is lost when the process exits; verify how to persist it in your version.
- Send texts in batches rather than one at a time.
- Respect provider rate limits; add retries with backoff (see `16-production/rate-limits-and-retries.md`).

## Strategy 5: Combine Dense Embeddings with Keyword Search
Dense embeddings miss exact terms such as product codes or error IDs. Pair them with sparse or BM25 retrieval, for example `Bm25Retriever` from `@llamaindex/bm25-retriever` fused with the vector retriever (see `07-retrievers/hybrid-retrieval.md`).

## Best Practices
- Use one embedding model per index; record its name and dimension.
- Normalize text consistently before embedding (see `03-data-ingestion/cleaning.md`).
- Version your embedding setup so you can tell which index was built how.
- Evaluate with real questions before and after any change.
- Plan for re-embedding. A model change means a full rebuild, so keep the raw documents.

## Common Mistakes
- Embedding metadata that adds noise.
- Re-embedding the entire corpus on every small update.
- Embedding boilerplate (headers, footers, navigation) that matches everything and nothing.
- Changing chunking and model at the same time, so you can't tell which helped.

## Related / Next
- `03-data-ingestion/chunking.md`
- `07-retrievers/auto-merging-retriever.md`
- `07-retrievers/hybrid-retrieval.md`
- `14-evaluation/retrieval-metrics.md`
