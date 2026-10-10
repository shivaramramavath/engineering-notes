# Transformations

## Concept
A **transformation** takes a list of nodes and returns a list of nodes. Parsers and embedding models are transformations, so they chain into a single ingestion flow. You can add your own steps by implementing `TransformComponent`.

```
Document[] ─► SentenceSplitter ─► (custom step) ─► EmbedModel ─► TextNode[] with embeddings
```

## Prerequisites
- [nodes-and-parsers.md](nodes-and-parsers.md)
- `03-data-ingestion/ingestion-pipeline.md`

## Built-in Kinds

| Kind | Examples | Effect |
|---|---|---|
| Node parsers | `SentenceSplitter`, `MarkdownNodeParser` | Split documents into nodes |
| Embedding models | `OpenAIEmbedding` (`@llamaindex/openai`) | Fill `node.embedding` |
| Custom | Your `TransformComponent` | Anything else |

**Metadata extractors** (`TitleExtractor`, `KeywordExtractor`, `QuestionsAnsweredExtractor`, `SummaryExtractor`) exist in Python. They are not documented for LlamaIndex.TS. Verify whether your installed version exports them; if not, write an LLM-based `TransformComponent` (below).

## Where Transformations Are Applied

### 1. Global default
```ts
import { Settings, SentenceSplitter } from "llamaindex";
Settings.nodeParser = new SentenceSplitter({ chunkSize: 512 });
```

### 2. Ingestion pipeline (recommended for real projects)
```ts
import { IngestionPipeline, SentenceSplitter, DocStoreStrategy } from "llamaindex";
import { SimpleDocumentStore } from "llamaindex/storage";    // verify path
import { OpenAIEmbedding } from "@llamaindex/openai";

const embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });

const pipeline = new IngestionPipeline({
  transformations: [new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 }), embedModel],
  vectorStore,                                   // e.g. PineconeVectorStore (storage.md)
  docStore: new SimpleDocumentStore(),
  docStoreStrategy: DocStoreStrategy.UPSERTS,
});
const nodes = await pipeline.run({ documents });
```
Gives caching, docstore-based deduplication, and writes directly to the vector store. Strategies: `UPSERTS` (default), `DUPLICATES_ONLY`, `UPSERTS_AND_DELETE`, `NONE`. Details in `03-data-ingestion/ingestion-pipeline.md`.

## Order Matters
1. Parse (split) first.
2. Add metadata on the resulting nodes.
3. Embed last, so that added metadata is included in what gets embedded (unless excluded).

## Cost of LLM-Based Metadata Steps
Any step that calls an LLM per node makes **one call per chunk**. On 100,000 chunks that is 100,000 calls per step.
- Run on a sample first and compare retrieval metrics.
- Use a cheaper LLM for extraction.
- Keep the cache enabled so reruns don't repeat the cost.

## Writing a Custom Transformation
Implement `TransformComponent` (exact constructor and `transform` signature: verify against your version; the documented pattern is a class whose async transform receives nodes and returns nodes):

```ts
import { TransformComponent } from "llamaindex";
import type { BaseNode } from "@llamaindex/core/schema";

class AddWordCount extends TransformComponent {
  constructor() {
    super(async (nodes: BaseNode[]): Promise<BaseNode[]> => {
      for (const node of nodes) {
        node.metadata["word_count"] = node.getContent().split(/\s+/).filter(Boolean).length;
      }
      return nodes;
    });
  }
}

const pipeline = new IngestionPipeline({
  transformations: [new SentenceSplitter(), new AddWordCount(), embedModel],
});
```
Typical custom steps: removing boilerplate, tagging tenant IDs, dropping near-empty nodes, normalizing text.

```ts
class DropShortNodes extends TransformComponent {
  constructor(minChars = 40) {
    super(async (nodes: BaseNode[]) => nodes.filter((n) => n.getContent().trim().length >= minChars));
  }
}
```

### Hand-rolled LLM metadata (replacement for TitleExtractor and friends)
```ts
import { Settings } from "llamaindex";

class SummaryMetadata extends TransformComponent {
  constructor() {
    super(async (nodes: BaseNode[]): Promise<BaseNode[]> => {
      for (const node of nodes) {
        const res = await Settings.llm.complete({
          prompt: `Summarize in one sentence:\n\n${node.getContent()}`,
        });                                              // verify complete() signature and result field
        node.metadata["summary"] = res.text;
      }
      return nodes;
    });
  }
}
```
Run it with limited concurrency in real use, and keep the prompt and model fixed so results are cacheable.

## Caching
```ts
import { IngestionCache } from "llamaindex";
const pipeline = new IngestionPipeline({ transformations: [/* ... */], cache: new IngestionCache("my-cache") });
```
The cache is on by default and in-memory by default; it is keyed by a hash of the input nodes plus the transformation config (`disableCache` turns it off). Changing a parser's settings invalidates the relevant cached results.

## Important Rules
- Transformations must be deterministic, or caching and change detection misbehave.
- Embedding must come after parsing and metadata steps.
- Do not enrich metadata with an LLM unless evaluation shows it helps.

## Common Mistakes
- Running an expensive LLM step over the full corpus before testing.
- Putting the embedding model before the parser.
- Non-deterministic custom steps (timestamps, random IDs) that make every run look like new data.
- Forgetting that `Settings.nodeParser` silently applies to every index.

## Related / Next
- [indexes.md](indexes.md)
- `03-data-ingestion/metadata.md`
- `11-document-management/refresh-and-sync.md`
