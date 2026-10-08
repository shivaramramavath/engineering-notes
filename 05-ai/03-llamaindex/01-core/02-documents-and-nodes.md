# Documents and Nodes

Everything LlamaIndex retrieves is a **node**: a chunk of text with metadata and links to its neighbours. Everything you feed in is a **document**. Between the two sits a **node parser** that does the chunking. Most RAG quality problems (answers that miss context, retrieval that returns half a thought) start here, not in the LLM.

> Prerequisite: [01-setup-and-first-query](./01-setup-and-first-query.md).

## The two objects

```text
Document  ──(node parser)──►  TextNode, TextNode, TextNode, ...
 whole source                  chunks that inherit the document's metadata
```

- `Document` is a `TextNode` subclass representing a whole source (a file, a page, a DB row).
- `TextNode` is the chunk that actually gets embedded, stored and retrieved.
- Other node types exist (`IndexNode` for recursive retrieval, `ImageNode`), but `TextNode` is what you will use 95% of the time.

Both share the same base fields:

| Field | Meaning |
|---|---|
| `id_` | Unique ID of the node or document |
| `text` | The content |
| `metadata` | Free-form key/value object |
| `excludedEmbedMetadataKeys` | Metadata keys hidden from the **embedding** text |
| `excludedLlmMetadataKeys` | Metadata keys hidden from the **LLM** prompt text |
| `relationships` | Links to source, previous, next, parent, child nodes |

## Creating documents

```ts
import { Document } from "llamaindex";

const doc = new Document({
  id_: "handbook-v3",
  text: "Employees accrue 1.5 vacation days per month...",
  metadata: { source: "hr/handbook.pdf", version: 3, department: "hr" },
});
```

Readers (file, web, database, LlamaParse) produce `Document`s for you; see `02-rag/01-readers-and-llamaparse.md`. When you build documents by hand, always set `id_` deliberately. A stable ID is what lets you update or delete a document later instead of duplicating it.

## Chunking: the node parser

By default `Settings.nodeParser` is a `SentenceSplitter`, which splits text with a preference for complete sentences. `Settings.chunkSize` and `Settings.chunkOverlap` configure it. To control it directly:

```ts
import { Settings, SentenceSplitter } from "llamaindex";

Settings.nodeParser = new SentenceSplitter({
  chunkSize: 512,
  chunkOverlap: 50,
});
```

Run a parser by hand to see what it produces:

```ts
const nodes = Settings.nodeParser.getNodesFromDocuments([doc]);
console.log(nodes.length);
console.log(nodes[0].text);
console.log(nodes[0].metadata);
```

`VectorStoreIndex.fromDocuments` does exactly this internally, then embeds the nodes.

To override chunking for one operation without touching global state, `Settings.withChunkSize(512, () => ...)` and `Settings.withChunkOverlap(...)` run a callback with a temporary value.

### Choosing chunk size

| Smaller chunks (e.g. 256-512) | Larger chunks (e.g. 1024+) |
|---|---|
| More precise matches, less noise per hit | More context per hit |
| More embedding calls, bigger index | Fewer calls, smaller index |
| Risk: answer split across chunks | Risk: embedding "blurs" several topics |

There is no universal best value; it depends on your content and question style. Measure it (see `04-production/02-testing-and-evaluation.md`) rather than copying a number. Overlap exists so a sentence cut at a boundary still appears whole somewhere; keep it a modest fraction of chunk size.

### Other parsers

- `MarkdownNodeParser` splits on heading structure and records the header path in metadata (for example `{ 'Header 1': 'Main Header' }`). Good for docs and READMEs.
- `CodeSplitter` (from `@llamaindex/node-parser/code`, built on tree-sitter) splits source code along syntax boundaries.
- Sentence-window and hierarchical parsers are covered in `02-rag/07-advanced-retrieval-patterns.md`.

Pick the parser that matches the document's structure. Splitting Markdown or code with a plain sentence splitter throws away information that the structure already gives you for free.

## Metadata

Documents pass their metadata to every node they produce, so each chunk knows where it came from.

```ts
const nodes = Settings.nodeParser.getNodesFromDocuments([doc]);
console.log(nodes[0].metadata);
// { source: "hr/handbook.pdf", version: 3, department: "hr" }
```

Metadata does two jobs:

1. **Filtering** at retrieval time ("only `department: hr`"), covered in `02-rag/03-retrievers-and-postprocessors.md`.
2. **Context** for the model and the embedding: by default metadata is rendered into the text that is embedded and into the text sent to the LLM.

The second point surprises people. If you stash a long `raw_json` or an internal database ID in metadata, it is embedded and shown to the LLM too, which skews similarity and burns tokens. Use the exclusion lists:

```ts
const doc = new Document({
  id_: "ticket-4812",
  text: "Customer reports login failures after the 2.4 update...",
  metadata: {
    ticketId: "4812",       // useful for filtering, noise for the model
    title: "Login failures after 2.4",
  },
  excludedEmbedMetadataKeys: ["ticketId"],
  excludedLlmMetadataKeys: ["ticketId"],
});
```

You can see the text each consumer receives with `getContent` and `MetadataMode`:

```ts
import { MetadataMode } from "llamaindex";

console.log(node.getContent(MetadataMode.EMBED)); // what the embedder sees
console.log(node.getContent(MetadataMode.LLM));   // what the LLM sees
console.log(node.getContent(MetadataMode.NONE));  // text only
```

Printing these two is the fastest way to debug "why does this chunk retrieve for the wrong query".

Keep metadata values simple (strings, numbers, booleans, arrays of those). Vector stores serialize metadata, and filter support varies by store.

## Relationships

Nodes carry links created by the parser:

| Relationship | Points to |
|---|---|
| `SOURCE` | The originating document |
| `PREVIOUS` / `NEXT` | Adjacent chunks |
| `PARENT` / `CHILD` | Hierarchy (hierarchical parsers) |

```ts
import { NodeRelationship } from "llamaindex";

const source = node.relationships[NodeRelationship.SOURCE];
console.log(source?.nodeId);
```

You rarely read these directly. They matter because retrieval patterns such as auto-merging and sentence-window use them to expand a hit into its neighbours or parent, and because a vector store uses the source document ID to delete all chunks of one document.

## Adding nodes you built yourself

If you split text with your own logic, wrap each piece in a `TextNode` and insert it into an existing index:

```ts
import { TextNode } from "llamaindex";

const custom = new TextNode({
  text: "Refunds are processed within 5 business days.",
  metadata: { source: "faq" },
});

await index.insertNodes([custom]);
```

For whole documents on an existing index use `await index.insert(doc)`.

## Common mistakes

**Not setting `id_`.** IDs are generated when you omit them, so re-ingesting the same file creates duplicates instead of replacing. Use deterministic IDs (path, URL, primary key).

**Metadata as a dumping ground.** Anything not in the exclusion lists is embedded and sent to the LLM. Exclude identifiers and bulky fields.

**Changing chunking and expecting old vectors to follow.** Chunk boundaries are fixed at index time. Change `chunkSize` or the parser, then re-index.

**One-size-fits-all splitting.** Tables, code and Markdown often need different parsers than prose.

**Tuning chunk size by feel.** Inspect actual nodes, then run a small evaluation set.

## Debugging checklist

1. Run the parser by hand and read `nodes[i].text`. Are sentences cut mid-thought? Are tables shredded?
2. Print `getContent(MetadataMode.EMBED)` for a node that retrieves badly.
3. Check node count against expectation (a 40-page PDF producing 3 nodes means the reader or parser misbehaved).
4. If filters return nothing, print `node.metadata` and compare key names and types exactly (`"2023"` is not `2023`).

## Quick Summary

- Documents are sources; `TextNode`s are the chunks that get embedded and retrieved.
- The default parser is `SentenceSplitter`, configured by `Settings.chunkSize` / `chunkOverlap` or by assigning `Settings.nodeParser`.
- Match the parser to the content: Markdown, code and prose want different splitters.
- Metadata is inherited by nodes and, unless excluded, is part of both the embedded text and the LLM prompt. Use `excludedEmbedMetadataKeys` and `excludedLlmMetadataKeys`.
- Stable `id_` values make updates and deletes possible.
- Debug chunking by printing nodes and `getContent(MetadataMode.EMBED | LLM)`.

## Next

[03-indexes-and-storage.md](./03-indexes-and-storage.md): where nodes and their vectors live, and how to persist them.
