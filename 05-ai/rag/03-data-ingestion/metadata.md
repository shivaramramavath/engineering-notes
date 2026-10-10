# Metadata

## Concept
**Metadata** is structured information attached to documents and nodes: source, page, author, date, section, category, access level. It does not need to be semantically searched; it is used to **filter, cite, update and debug**.

## Prerequisites
- [Documents and nodes](../01-fundamentals/documents-and-nodes.md)
- [chunking.md](chunking.md)

## What Metadata Is For

| Purpose | Example |
|---|---|
| Filtering | Only search `department == "HR"` or `year >= 2024` |
| Citations | Show file name and page next to the answer |
| Updates and deletes | Find every chunk of `handbook.pdf` via the document ID / source relationship |
| Access control | Restrict results to `tenantId == "acme"` |
| Debugging | Trace a bad answer back to its source chunk |
| Better embeddings | Include a title or section so a chunk is understood in context |

## Attaching Metadata
```ts
import { Document } from "llamaindex";

const doc = new Document({
  text: "Employees accrue 1.5 vacation days per month.",
  id_: "hr-policy.pdf#p4",
  metadata: {
    source: "hr-policy.pdf",
    page: 4,
    department: "HR",
    year: 2025,
    tenantId: "acme",
  },
});
```
Or after loading, by mutating `doc.metadata`. See [loaders.md](loaders.md). Keep metadata values flat (strings, numbers, booleans).

## Controlling What Gets Embedded vs Shown
```ts
doc.excludedEmbedMetadataKeys = ["tenantId", "year"];  // not part of the embedding text
doc.excludedLlmMetadataKeys = ["tenantId"];            // not shown to the LLM
```
The text that is embedded or shown is built with `node.getContent(metadataMode)` (`MetadataMode` from `@llamaindex/core/schema`), which includes or omits metadata according to these lists.

| Key type | Embed? | Show to LLM? |
|---|---|---|
| Descriptive (title, section) | Yes | Yes |
| Filter-only (tenantId, year, access level) | No | Usually no |
| Citation-only (file path, URL) | No | Sometimes |

## Auto-Extracted Metadata
The Python framework ships LLM-based extractors (`TitleExtractor`, `QuestionsAnsweredExtractor`, `KeywordExtractor`). I did not find equivalents documented for LlamaIndex.TS (verify your version). The pipeline step is just a `TransformComponent`, so you can hand-roll one: call `Settings.llm` per chunk (for example "list 3 questions this chunk answers"; verify the LLM method name) and write the result into `node.metadata`.

- Can improve retrieval (for example "questions this chunk answers").
- Costs LLM calls for every chunk. Use only when evaluation shows a gain.
- Put the extractor before the embedding step, and let `IngestionCache` avoid repeating the calls (see [ingestion-pipeline.md](ingestion-pipeline.md)).

## Filtering at Query Time
```ts
import { MetadataFilters } from "llamaindex";

const filters = new MetadataFilters({
  filters: [{ key: "department", value: "HR", operator: "==" }],
});
const retriever = index.asRetriever({ similarityTopK: 5, filters });
```
`MetadataFilters` is also exported from `"@llamaindex/core/vector-store"`; the import path varies by version. The full operator list is not documented in the pages I read; verify. Filter support and syntax vary by vector store. See `05-pinecone/namespaces-and-filters.md` and `07-retrievers/retrieval-basics.md`.

## Storage Limits
Vector stores cap metadata size per vector (Pinecone, for example, has a per-record metadata limit, about 40KB at the time of writing; verify, and supports a limited set of value types such as strings, numbers, booleans and lists of strings). Check your store's current limits. Keep values short and flat; do not stuff long text into metadata.

## Design Tips
- Decide your filter fields **before** ingestion. Adding a new filter field later means re-ingesting.
- Use consistent keys and value formats (`2025` always as a number, dates always ISO format).
- Always include a stable document ID or source path.
- Include a tenant or owner field if different users may see different data.

## Important Rules
- Metadata is for filtering and tracing, not for storing content.
- Never rely on metadata alone for security. Enforce access control in the retrieval layer too (see `16-production/security.md`).
- Keep key names stable across the whole corpus.

## Common Mistakes
- Embedding noisy metadata (IDs, timestamps) and diluting the vector.
- Long metadata that overflows chunk size or the store's limit.
- Inconsistent types (`"2025"` in one file, `2025` in another), which breaks filters silently.
- Forgetting to add the filter field until the app needs it.

## Related / Next
- [ingestion-pipeline.md](ingestion-pipeline.md)
- `04-vector-databases/partitioning-and-filtering.md`
- `11-document-management/doc-ids-and-tracking.md`
