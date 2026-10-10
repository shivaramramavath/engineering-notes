# Nodes and Node Parsers

## Concept
The conceptual intro is in `01-fundamentals/documents-and-nodes.md` and chunking strategies are in `03-data-ingestion/chunking.md`. This file is the **API reference**: the `Node` classes, relationships, and how parsers produce nodes.

## Prerequisites
- `01-fundamentals/documents-and-nodes.md`
- `03-data-ingestion/chunking.md`

## Node Types

| Class | Use |
|---|---|
| `TextNode` | A chunk of text (the default product of parsers) |
| `Document` | Extends `TextNode`; the source container |
| `IndexNode` | A node that points to another object (exists in the core schema; used for recursive retrieval in Python; verify TS support) |
| `ImageNode` | Image content, for multimodal use (verify) |

```ts
import { TextNode } from "@llamaindex/core/schema";       // verify path; also exported from "llamaindex" in some versions

const node = new TextNode({ text: "Employees accrue 1.5 days per month.", metadata: { source: "hr.pdf" } });
console.log(node.id_, node.metadata);
```

## What a Node Holds
- `text`, `metadata`
- `id_`
- `embedding` (set after embedding)
- `relationships` (links to other nodes)
- `excludedEmbedMetadataKeys`, `excludedLlmMetadataKeys`
- `hash` (used for change detection)
- `getContent(metadataMode)`

## What the Embedder and LLM Actually See
Metadata is included in the text sent to the embedder and the LLM unless excluded.

```ts
import { MetadataMode } from "@llamaindex/core/schema";

console.log(node.getContent(MetadataMode.EMBED));  // what gets embedded
console.log(node.getContent(MetadataMode.LLM));    // what the LLM sees
console.log(node.getContent(MetadataMode.NONE));   // text only

node.excludedEmbedMetadataKeys = ["source"];       // hide a key from the embedder
```
Use these calls when debugging "why did this chunk match?". See `03-data-ingestion/metadata.md`.

## Relationships

```ts
import { NodeRelationship } from "@llamaindex/core/schema";

node.relationships[NodeRelationship.SOURCE];      // RelatedNodeInfo → original Document (nodeId)
node.relationships[NodeRelationship.PREVIOUS];
node.relationships[NodeRelationship.NEXT];
node.relationships[NodeRelationship.PARENT];
node.relationships[NodeRelationship.CHILD];
node.sourceNode?.nodeId;                           // shortcut to the SOURCE document (verify)
```

| Relationship | Created by | Used by |
|---|---|---|
| SOURCE | Parsers | Document updates and deletes (`deleteRef`) |
| PREVIOUS / NEXT | Parsers (typically default) | Context expansion, sentence-window |
| PARENT / CHILD | Your own code (see below) | Hand-rolled auto-merging / small-to-big |

## Using a Parser
```ts
import { SentenceSplitter } from "llamaindex";

const parser = new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 });
const nodes = await parser.transform(documents);          // or: await Settings.nodeParser(documents) (verify)
```
Other documented options: `paragraphSeparator`, `secondaryChunkingRegex`, `separator`, `extraAbbreviations`.

Parsers are also **transformations**, so they work directly inside `IngestionPipeline`. See [transformations.md](transformations.md).

## Common Parsers (see chunking.md for when to use which)

| Parser | Notes |
|---|---|
| `SentenceSplitter` | Default; sentence-aware with size limit |
| `TokenTextSplitter` | Pure token count (`chunkSize`, `chunkOverlap`, `separator`) |
| `SentenceWindowNodeParser` | Single sentences plus window metadata (`windowSize`, `windowMetadataKey`, `originalTextMetadataKey`) |
| `MarkdownNodeParser` | Splits by header, keeps the header hierarchy in metadata |
| `CodeSplitter` (`@llamaindex/node-parser/code`) | Needs tree-sitter |

`SimpleNodeParser` is deprecated. Python's `SemanticSplitterNodeParser`, `HTMLNodeParser` and `JSONNodeParser` are not documented for TS (verify before relying on them).

## Hierarchical Nodes
**`HierarchicalNodeParser` and `get_leaf_nodes` are not in the LlamaIndex.TS documentation** (the Python framework has them). Build the parent/child structure yourself: split into large chunks, split each parent into small chunks, record the parent ID in metadata and `NodeRelationship.PARENT`/`CHILD`, and embed only the leaves. Keep **all** nodes in a docstore or `Map`, because merging needs to look parents up. The full hand-rolled approach is in `07-retrievers/auto-merging-retriever.md`.

## Controlling Node IDs
For stable IDs (important for updates and deduplication), give documents stable `id_`s and let child nodes follow from them. Python parsers accept an `id_func`; a TS equivalent is not documented (verify). If you need deterministic node IDs, set them in a custom transformation after splitting:

```ts
import { createHash } from "node:crypto";
import type { TextNode } from "@llamaindex/core/schema";

function stableId(docId: string, i: number): string {
  return createHash("sha1").update(`${docId}-${i}`).digest("hex");
}

// inside a TransformComponent, after splitting:
nodes.forEach((n: TextNode, i: number) => {
  n.id_ = stableId(n.sourceNode?.nodeId ?? "unknown", i);
});
```
See `11-document-management/doc-ids-and-tracking.md`.

## Important Rules
- Metadata and text together must fit within the chunk size.
- Keep `SOURCE` relationships; they power updates, deletes and citations.
- Embed leaf nodes only when using hierarchical parsing.

## Common Mistakes
- Putting only leaf nodes in the docstore, so auto-merging cannot find parents.
- Assuming the LLM sees the same text as the embedder; metadata exclusion keys differ.
- Random node IDs that change every ingestion run.
- Looking for Python-only classes (`HierarchicalNodeParser`, `SemanticSplitterNodeParser`) in the TS package.

## Related / Next
- [transformations.md](transformations.md)
- `07-retrievers/recursive-retriever.md`
- `07-retrievers/auto-merging-retriever.md`
