# Documents and Nodes

## Concept
LlamaIndex.TS represents your data with two core units:

- **Document**: a whole source item (a PDF, a web page, a database row). It is the *input* to ingestion.
- **Node** (also called a chunk): a slice of a document. It is the *unit that gets embedded, stored and retrieved*.

```
Document (report.pdf)
   ├── Node 1  (page 1, paragraph 1-3)
   ├── Node 2  (page 1, paragraph 4-6)
   └── Node 3  (page 2, paragraph 1-3)
```

## Prerequisites
- [rag-pipeline.md](rag-pipeline.md)

## Why Nodes Exist
- Embedding models and LLM contexts have size limits.
- A small, focused chunk gives a sharper embedding than a whole document.
- Retrieval returns only the relevant piece instead of everything.

## What Each Carries

| Field | Document | Node |
|---|---|---|
| Text | Full content | Chunk content |
| `metadata` | File name, author, date, etc. | Inherited from the document, plus chunk-specific fields |
| ID | `id_` | `id_` |
| Relationships | None | Links to source document, previous and next nodes, parent and children |
| Embedding | Not usually | Yes, stored with the node (`embedding`) |

`Document` extends `TextNode`, which extends `BaseNode`. `BaseNode` fields include `id_`, `embedding`, `metadata`, `excludedEmbedMetadataKeys`, `excludedLlmMetadataKeys`, `relationships` and `hash`, plus `getContent(metadataMode)`.

## Example
```ts
import { Document, SentenceSplitter } from "llamaindex";
import { NodeRelationship } from "@llamaindex/core/schema";   // verify import path

const doc = new Document({
  text: "RAG retrieves context before generating an answer. ...",
  metadata: { source: "handbook.pdf", page: 3 },
});

const splitter = new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 });
const nodes = await splitter.transform([doc]);

console.log(nodes[0].metadata);                               // inherits source and page
console.log(nodes[0].relationships[NodeRelationship.SOURCE]); // points back to the original document (verify exact shape)
console.log(nodes[0].relationships);                          // SOURCE, PREVIOUS, NEXT links
```

## Node Relationships
Nodes keep links to their neighbors and origin (`NodeRelationship`):
- **SOURCE**: the parent document (its `nodeId` is the document id)
- **PREVIOUS / NEXT**: adjacent chunks
- **PARENT / CHILD**: for hierarchical structures. LlamaIndex.TS has no documented `HierarchicalNodeParser`; you can set these relationships yourself (verify), as described in the retriever chapters.

These links power features covered later: updating or deleting a document's chunks (`index.deleteRef(docId)`), and small-to-big retrieval patterns.

## Important Rules
- Retrieval works on **nodes**, never on whole documents.
- The SOURCE relationship (the document id) is what lets you delete or update every chunk of a document. Keep it intact.
- Metadata set on the document is copied onto its nodes, so attach useful metadata early.

## Common Mistakes
- Putting huge or irrelevant metadata on documents. It is copied to every node, can pollute the embedded text, and inflates storage.
- Assuming one document equals one vector.
- Losing the source link, which breaks citations and document updates.

## Related / Next
- `03-data-ingestion/chunking.md`
- `03-data-ingestion/metadata.md`
- `06-llamaindex/nodes-and-parsers.md`
- `11-document-management/doc-ids-and-tracking.md`
