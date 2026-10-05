# Ingestion Pipelines

`VectorStoreIndex.fromDocuments` is fine for a first demo, but it does one thing: split, embed, store. A real system needs more: extra enrichment steps (titles, summaries), not paying twice to embed unchanged text, and re-running when only some documents changed. An `IngestionPipeline` is the building block for that. It takes documents in, applies an ordered list of **transformations**, and returns nodes (optionally writing them into a vector store).

> Prerequisites: [../01-core/02-documents-and-nodes](../01-core/02-documents-and-nodes.md), [../01-core/03-indexes-and-storage](../01-core/03-indexes-and-storage.md).

## Basic pipeline

```ts
import { Document, IngestionPipeline, SentenceSplitter } from "llamaindex";
import { OpenAIEmbedding } from "@llamaindex/openai";

const pipeline = new IngestionPipeline({
  transformations: [
    new SentenceSplitter({ chunkSize: 1024, chunkOverlap: 20 }),
    new OpenAIEmbedding(),
  ],
});

const nodes = await pipeline.run({
  documents: [new Document({ id_: "essay", text: "..." })],
});
```

The transformations run in order, each taking nodes and returning nodes. Here the splitter produces chunks and the embedding model attaches vectors. `run` returns the final nodes.

Components that work as transformations include node parsers (`SentenceSplitter`, `MarkdownNodeParser`, ...), metadata extractors (such as `TitleExtractor`, `QuestionsAnsweredExtractor`, `SummaryExtractor`, `KeywordExtractor`), and embedding models.

Transformations can also be used on their own:

```ts
let nodes = new SentenceSplitter().getNodesFromDocuments([doc]);
nodes = await new TitleExtractor().transform(nodes);
```

### Writing into a vector store

Pass a `vectorStore` and the pipeline inserts the resulting nodes for you:

```ts
const pipeline = new IngestionPipeline({
  transformations: [
    new SentenceSplitter({ chunkSize: 1024, chunkOverlap: 20 }),
    new OpenAIEmbedding(),
  ],
  vectorStore, // e.g. a PGVectorStore or QdrantVectorStore
});

await pipeline.run({ documents });
```

Then attach an index to the same store without re-embedding:

```ts
const index = await VectorStoreIndex.fromVectorStore(vectorStore);
```

Keep `Settings.embedModel` equal to the embedding model in the pipeline, or query vectors won't match stored vectors.

## Order matters

Put cheap, shrinking steps first and expensive per-node steps last, because everything after the splitter runs once per chunk.

```text
documents ─► splitter ─► extractor(s) (LLM call per node!) ─► embedding ─► nodes
```

Extractors like `TitleExtractor` or `QuestionsAnsweredExtractor` make LLM calls, often one or more per node. On a large corpus that is real money and time. This is exactly why caching exists.

## Custom transformations

A custom transformation is a class with an async `transform(nodes)` method that returns nodes:

```ts
import { TransformComponent, type TextNode } from "llamaindex";

class RemoveSpecialCharacters extends TransformComponent {
  constructor() {
    super(async (nodes: TextNode[]) => {
      for (const node of nodes) {
        node.text = node.text.replace(/[^\w\s]/gi, "");
      }
      return nodes;
    });
  }
}

const pipeline = new IngestionPipeline({
  transformations: [new RemoveSpecialCharacters()],
});
```

The constructor shape of `TransformComponent` has varied between releases; check the transformations page of the docs for your version, and use whatever pattern it shows. Typical uses: cleaning boilerplate, redacting PII before text reaches embeddings or the LLM, and adding computed metadata.

## Caching

The pipeline caches the output of each transformation, keyed by a hash of the input nodes plus the transformation's own configuration. Re-running the pipeline over unchanged input reuses cached results, so expensive steps (LLM extractors, embedding calls) are skipped for text you have already processed.

Practical implications:

- Changing a transformation's settings (chunk size, model, prompt) changes its hash, so affected steps re-run and downstream steps follow.
- The cache saves *transformation work*. It does not tell the pipeline which documents are new or changed. That is the docstore's job (next section).
- A cache held only in memory disappears on restart. If your pipeline runs as a short-lived job, the cache must live somewhere that persists, or you will pay again every run. Confirm what your version persists.

## Deduplication and incremental updates

Without a docstore, running the same documents through the pipeline twice inserts them twice. To avoid that, give the pipeline a `docStore`:

```ts
import { IngestionPipeline, SimpleDocumentStore } from "llamaindex";

const docStore = new SimpleDocumentStore();

const pipeline = new IngestionPipeline({
  transformations: [splitter, embedModel],
  docStore,
  vectorStore,
});

await pipeline.run({ documents });
```

How it behaves:

- The docstore tracks documents by **`id_`** together with a content hash.
- With a docstore attached, the default strategy is **upsert**: a document whose ID is unseen is ingested; one whose ID and hash match what was stored is skipped; one whose ID matches but whose content changed replaces the old version.
- With no docstore, the strategy is effectively "none", so there is no deduplication.

This is why stable IDs are such a recurring theme in these notes. If your reader assigns random IDs, every run looks like all-new data and the docstore can't help you.

The `DocStoreStrategy` enum also offers other strategies (for example ones that only drop duplicates, or that also delete documents which disappeared from the source). The enum is exported by the package, but I could not verify every member name for the TypeScript version, so check the enum in your installed version before setting `docStoreStrategy`.

### Making the state survive restarts

Dedup only works if the docstore remembers across runs. An in-memory `SimpleDocumentStore` forgets when the process exits. Options:

- A file-backed store from `storageContextFromDefaults({ persistDir })`, using its `docStore`. Verify, by checking that files appear in `persistDir` after a run, that your version actually writes docstore state after pipeline runs.
- A database-backed docstore. The API reference lists Postgres, Mongo and Cosmos-backed stores; pick the one matching your infrastructure and follow its page.

A sensible incremental flow:

```text
cron / webhook
   └─► reader loads current source documents (stable ids)
        └─► pipeline.run({ documents })
              ├─ unchanged docs  → skipped (docstore hash match)
              ├─ changed docs    → re-processed, old chunks replaced
              └─ new docs        → added
```

Removing documents that no longer exist in the source needs an explicit delete (for example `vectorStore.delete(refDocId)` on stores that support it) unless you use a strategy that handles deletions.

## Common mistakes

**Random document IDs.** Dedup and upserts silently never trigger.

**Putting LLM extractors before the splitter or on huge nodes.** Run them after splitting, and only if retrieval actually benefits.

**Forgetting the embedding step.** If your pipeline writes to a vector store, nodes need embeddings by the time they're inserted, so include the embedding model as the last transformation.

**Expecting the cache to deduplicate.** The cache avoids recomputation; the docstore decides what is new.

**Changing the embedding model mid-stream.** Old vectors stay in the store. Re-index fully when the model changes.

**Not persisting state in job-style runs.** A pipeline that "works" locally but reprocesses everything in CI or cron is almost always missing persistent cache and docstore state.

## Debugging

- Run the pipeline without `vectorStore` and inspect `nodes` first: chunk boundaries, metadata, extractor output.
- Print `node.getContent(MetadataMode.EMBED)` to see exactly what gets embedded.
- Run twice and compare counts. A second run on unchanged input should add nothing and finish fast.
- If every run re-embeds everything, check IDs, then docstore persistence, then cache persistence, in that order.
- Count vectors in the store after ingest and compare to the expected node count.

## Quick Summary

- `IngestionPipeline` = ordered transformations (parsers, extractors, embeddings) from documents to nodes, optionally writing to a vector store.
- Run cheap steps first; per-node LLM extractors are expensive.
- The cache skips recomputation of unchanged transformation steps.
- A `docStore` plus stable `id_`s gives upsert-style dedup and incremental updates; no docstore means no dedup.
- Dedup state and cache must persist across runs for scheduled ingestion to be incremental.
- Deletions from the source need explicit handling.

## Next

[03-retrievers-and-postprocessors.md](./03-retrievers-and-postprocessors.md): now that data is in, how to fetch the right nodes and clean them up before generation.
