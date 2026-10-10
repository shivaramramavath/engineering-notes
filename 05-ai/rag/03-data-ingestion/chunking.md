# Chunking

## Concept
**Chunking** splits documents into pieces (nodes) small enough to embed and retrieve precisely, yet large enough to carry a complete idea. It is one of the biggest levers on RAG quality.

```
Document ──► Node Parser ──► [Node, Node, Node, ...]
```

> TypeScript edition. Import paths vary between LlamaIndex.TS versions; verify against your installed version.

## Prerequisites
- [Documents and nodes](../01-fundamentals/documents-and-nodes.md)
- [cleaning.md](cleaning.md)

## The Core Trade-off

| Chunks too small | Chunks too large |
|---|---|
| Lose context; ideas split across chunks | Embedding blends many topics; blurry matches |
| More chunks to store and search | Wasted prompt tokens on irrelevant text |
| Good precision, poor completeness | Good completeness, poor precision |

No universal best size exists. Start with a reasonable default (a few hundred tokens), then tune with evaluation.

## Chunk Size and Overlap
- **Chunk size**: maximum tokens per chunk.
- **Overlap**: tokens shared between consecutive chunks, so a sentence cut at a boundary still appears whole in one chunk.

Overlap guidance:
- A modest overlap (roughly 10 to 20 percent of chunk size) is a common starting point.
- Too much overlap inflates storage and returns near-duplicate chunks.
- Zero overlap is fine for structure-based splitting (headings, sections) where boundaries are already meaningful.

## What LlamaIndex.TS Documents

| Parser | Documented in LlamaIndex.TS? |
|---|---|
| `SentenceSplitter` | Yes |
| `TokenTextSplitter` | Yes |
| `MarkdownNodeParser` | Yes |
| `SentenceWindowNodeParser` | Yes |
| `CodeSplitter` | Yes (needs tree-sitter) |
| `SimpleNodeParser` | Deprecated; use `SentenceSplitter` |
| `HierarchicalNodeParser` | **Not documented** (Python has it) |
| `SemanticSplitterNodeParser` | **Not documented** (Python has it) |
| `HTMLNodeParser` | Not documented |

## Strategies

### 1. Fixed-size / token splitting
Splits by token count, ignoring meaning. Simple and predictable; may cut mid-sentence.
```ts
import { TokenTextSplitter } from "llamaindex";

const parser = new TokenTextSplitter({ chunkSize: 512, chunkOverlap: 50, separator: " " });
```

### 2. Sentence-aware splitting (default choice)
Respects sentence and paragraph boundaries while targeting a size. This is `SentenceSplitter`, and a good default for prose.
```ts
import { SentenceSplitter, Settings } from "llamaindex";

const splitter = new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 });
const nodes = await splitter.transform(documents);          // takes an array of nodes/documents

// Or set it globally:
Settings.nodeParser = splitter;
```
Other documented options: `paragraphSeparator`, `secondaryChunkingRegex`, `separator`, `extraAbbreviations`.

### 3. Recursive (separator-hierarchy) splitting
Tries large separators first (paragraphs), then falls back to smaller ones (sentences, then words) until chunks fit. This is the idea behind LangChain's `RecursiveCharacterTextSplitter`; `SentenceSplitter` follows a similar paragraph → sentence → word fallback, so it covers most of this need.

### 4. Structure-based splitting
Split on the document's own structure.
```ts
import { MarkdownNodeParser } from "llamaindex";
import { CodeSplitter } from "@llamaindex/node-parser/code";   // needs tree-sitter; verify setup

const mdParser = new MarkdownNodeParser();   // splits by header, keeps header hierarchy in metadata
// CodeSplitter takes { getParser, maxChars }; the tree-sitter getParser wiring is version-specific (verify)
```
Best when documents have reliable headings or sections. For HTML, convert to text or Markdown first (see [loaders.md](loaders.md)).

### 5. Semantic splitting (not documented for LlamaIndex.TS)
The Python framework has `SemanticSplitterNodeParser`, which embeds sentences and splits where the topic shifts. **LlamaIndex.TS does not document an equivalent.** If you want to try the idea, a hand-rolled version is short but unverified against your data:
1. Split into sentences (for example with `SentenceSplitter` at a tiny `chunkSize`).
2. Embed each sentence with `Settings.embedModel`.
3. Compute cosine distance between neighbouring sentences (optionally with a small window of context).
4. Start a new chunk where the distance exceeds a high percentile (for example the 95th).

It costs embedding calls at ingestion time and gains are not guaranteed, so compare against `SentenceSplitter` on your data first.

### 6. Sentence-window splitting
Embeds single sentences for precision, but stores surrounding sentences in metadata so the LLM sees context.
```ts
import { SentenceWindowNodeParser } from "llamaindex";

const parser = new SentenceWindowNodeParser({
  windowSize: 3,
  windowMetadataKey: "window",
  originalTextMetadataKey: "original_text",
});
```
Needs a post-processor at query time to swap in the window: `MetadataReplacementPostProcessor({ targetMetadataKey: "window" })` from `"llamaindex/postprocessors"`. See `06-llamaindex/` and `10-context-and-generation/`.

### 7. Hierarchical splitting (not documented for LlamaIndex.TS)
Python offers `HierarchicalNodeParser` with an auto-merging retriever: retrieve small chunks, then merge up to the parent when enough children match. **Neither is documented for LlamaIndex.TS.** A hand-rolled alternative:
1. Split each document with a `SentenceSplitter` at a large size (parents, for example 2048).
2. Split each parent with a smaller `SentenceSplitter` (children, for example 512).
3. Store `parentId` in each child's metadata (and set `NodeRelationship` if you want; verify).
4. Index only the children; keep parents in a `Map<string, TextNode>`.
5. At query time, swap or merge children for their parent (see `07-retrievers/auto-merging-retriever.md`).

## Choosing a Strategy

| Document type | Start with |
|---|---|
| General prose | `SentenceSplitter` |
| Markdown / docs with headings | `MarkdownNodeParser`, then size-limit large sections |
| HTML pages | Clean to text or Markdown first |
| Code | `CodeSplitter` |
| Long documents needing context at answer time | `SentenceWindowNodeParser`, or hand-rolled parent/child |
| Topic-mixed text without structure | Compare `SentenceSplitter` against a hand-rolled semantic splitter |

## Chunk Metadata
Each node inherits its document's metadata. Add chunk-level fields (section title, page, position) where useful. See [metadata.md](metadata.md).

## Gotcha: Metadata Counts Toward Chunk Size
`SentenceSplitter` counts metadata length when fitting chunks. If metadata is longer than the chunk size, parsing fails or produces degenerate chunks. Keep metadata short, or exclude noisy keys from the embedded text (see [metadata.md](metadata.md)).

## Tuning Process
1. Build an evaluation set of real questions with known source passages.
2. Try 2 or 3 chunk sizes (for example 256, 512, 1024) and one or two strategies.
3. Compare retrieval metrics (recall@k, MRR) and answer quality.
4. Keep the simplest configuration that performs well.

## Important Rules
- Chunk size must fit the embedding model's input limit.
- Preserve source links (the node's `ref_doc_id` / source relationship) and metadata on every node.
- Change one variable at a time when tuning.
- If you change chunking, re-ingest the affected documents.

## Common Mistakes
- Using one chunk size for every document type.
- Splitting tables, code blocks or lists in the middle.
- Huge overlap that returns the same text three times.
- Judging chunking by eye instead of with retrieval metrics.
- Assuming a Python-only parser (hierarchical, semantic) exists in TypeScript and writing imports that do not resolve.

## Related / Next
- [metadata.md](metadata.md)
- [ingestion-pipeline.md](ingestion-pipeline.md)
- `06-llamaindex/nodes-and-parsers.md`
- `07-retrievers/auto-merging-retriever.md`
- `14-evaluation/retrieval-metrics.md`
