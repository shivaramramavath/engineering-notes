# Advanced Retrieval Patterns

Most retrieval problems come from one tension: **small chunks embed and match precisely, but large chunks give the LLM enough context to answer.** The patterns here all try to have both: match on something small, then hand the model something bigger. Plus two related ideas: recursive retrieval (route through a hierarchy) and graph-based retrieval (follow relationships).

> Prerequisites: [../01-core/02-documents-and-nodes](../01-core/02-documents-and-nodes.md), [03-retrievers-and-postprocessors](./03-retrievers-and-postprocessors.md).

> **Status in TypeScript:** `SentenceWindowNodeParser` and `MetadataReplacementPostProcessor` are built in. I did not find built-in equivalents of Python's `HierarchicalNodeParser`, `AutoMergingRetriever`, a recursive retriever, or a graph/property-graph index in the LlamaIndex.TS API reference. For those, this note shows the idea and a small hand-built version. Check the reference for your version, since this part of the ecosystem moves quickly.

Do these only after the basics are solid (good parsing, sensible chunk size, reranking). They add complexity and index size, and they are easy to get subtly wrong.

## Pattern 1: sentence-window retrieval

**Idea:** index each *sentence* as its own node (very precise embeddings), but store the surrounding sentences (the "window") in metadata. At query time, retrieve the matching sentence, then swap its text for the window before generation.

```ts
import {
  Settings,
  SentenceWindowNodeParser,
  MetadataReplacementPostProcessor,
  VectorStoreIndex,
} from "llamaindex";

Settings.nodeParser = SentenceWindowNodeParser.fromDefaults({
  windowSize: 3, // sentences on each side of the matched sentence
});

const index = await VectorStoreIndex.fromDocuments(documents);

const queryEngine = index.asQueryEngine({
  similarityTopK: 6,
  nodePostprocessors: [new MetadataReplacementPostProcessor("window")],
});
```

Details:

- The parser stores the window under the metadata key `window` and the original sentence under `original_text` by default, which is why the postprocessor is told to use `"window"`.
- Chunk-size settings don't apply to this parser; the window size does.
- The window must **not** be part of what gets embedded, or you lose the precision that motivated the pattern. Verify it: print `node.getContent(MetadataMode.EMBED)` for a node and confirm the window text is absent. If it isn't, add the key to `excludedEmbedMetadataKeys` on the nodes.
- The LLM then sees larger context than the matching sentence, so answers are more coherent while retrieval stays sharp.

Trade-offs: many more nodes (one per sentence), so a bigger index and more embedding calls. Overlapping windows from adjacent hits can repeat the same text in the prompt; deduplicate or raise the postprocessor's awareness (a custom postprocessor can merge overlapping windows).

## Pattern 2: small-to-big (parent retrieval)

**Idea:** split documents into large **parent** chunks, split each parent into small **child** chunks, embed only the children, and at query time return the *parent* of each matching child.

This is the manual version of what Python calls hierarchical parsing, and it is easy to build with plain ingredients: two splitters, an ID link in metadata, and a postprocessor.

```ts
import {
  Document,
  SentenceSplitter,
  TextNode,
  type NodeWithScore,
} from "llamaindex";

const parentSplitter = new SentenceSplitter({ chunkSize: 1536, chunkOverlap: 0 });
const childSplitter = new SentenceSplitter({ chunkSize: 256, chunkOverlap: 20 });

const parents = parentSplitter.getNodesFromDocuments(documents);
const parentStore = new Map<string, TextNode>(); // id -> parent node
const children: TextNode[] = [];

for (const parent of parents) {
  parentStore.set(parent.id_, parent as TextNode);

  const childDocs = [new Document({ text: parent.getContent(), metadata: parent.metadata })];
  for (const child of childSplitter.getNodesFromDocuments(childDocs)) {
    child.metadata = { ...child.metadata, parentId: parent.id_ };
    child.excludedEmbedMetadataKeys = [...child.excludedEmbedMetadataKeys, "parentId"];
    child.excludedLlmMetadataKeys = [...child.excludedLlmMetadataKeys, "parentId"];
    children.push(child as TextNode);
  }
}
// index only `children` (embed + store them)
```

Then expand at query time:

```ts
class ParentExpander {
  constructor(private parents: Map<string, TextNode>) {}

  async postprocessNodes(nodes: NodeWithScore[]): Promise<NodeWithScore[]> {
    const seen = new Set<string>();
    const out: NodeWithScore[] = [];
    for (const n of nodes) {
      const parent = this.parents.get(n.node.metadata.parentId);
      if (!parent || seen.has(parent.id_)) continue; // dedupe: many children, one parent
      seen.add(parent.id_);
      out.push({ node: parent, score: n.score });
    }
    return out;
  }
}
```

How to insert the children: use whichever index-building path fits your setup (for example your own ingestion flow that embeds and writes the child nodes to the vector store, then attach with `VectorStoreIndex.fromVectorStore`). I'm keeping that step abstract because the API for building an index straight from pre-made nodes has differed across versions.

Production note: the in-memory `parentStore` map above must survive restarts and be shared across instances. Store parents in a docstore or a database table keyed by `parentId`.

**Auto-merging** is a refinement: only promote to the parent when *several* children of the same parent were retrieved (a majority), otherwise keep the small child. You can add that to `ParentExpander` by counting hits per `parentId` and expanding only above a threshold. It's useful when a single stray child hit shouldn't drag in a big, mostly-irrelevant parent.

## Pattern 3: recursive retrieval

**Idea:** index *summaries or pointers* at the top level; each points to a more detailed retriever or query engine. Retrieval recurses: match a summary, then retrieve inside what it points to. Good for corpora made of many large documents, or mixed content (documents plus tables).

The package has an `IndexNode` node type designed as such a pointer. I did not verify a ready-made recursive retriever that consumes it. A robust alternative, built from verified parts:

1. Create one index (or query engine) per large document or table.
2. Wrap each as a tool with a descriptive summary.
3. Use `RouterQueryEngine` or `SubQuestionQueryEngine` over those tools (see [06-query-transforms-and-routing](./06-query-transforms-and-routing.md)).

That gives the same two-level behavior (pick the right document, then retrieve inside it) with components that are documented and stable. Use a deterministic first stage instead (metadata filter on `source`) whenever you can; it's cheaper and more predictable than an LLM picking the document.

## Pattern 4: graph-based retrieval

**Idea:** extract entities and relations into a graph and retrieve by traversing relationships ("which teams depend on service X?"). It shines on multi-hop, relationship-heavy questions that vector similarity answers poorly.

In LlamaIndex Python there are knowledge-graph and property-graph indexes. I did not find one in the TypeScript package; its listed index types are `VectorStoreIndex`, `SummaryIndex` and `KeywordTableIndex`. Realistic options if you need graph retrieval in a TS stack:

- Use a graph database (for example Neo4j) directly and wrap your query in a custom retriever that returns `NodeWithScore[]`.
- Run the graph part in a separate Python service and call it from your TS app as a tool.
- Approximate with metadata: store entity IDs and relation fields on nodes and filter or expand with `preFilters`.

Graph pipelines are heavy: entity extraction costs an LLM call per chunk, quality depends on prompt and schema, and the graph needs maintenance as data changes. Prove that vector + hybrid retrieval fails on your question set before taking this on.

## Choosing

| Symptom | Pattern |
|---|---|
| Matches are right but the LLM lacks surrounding context | Sentence-window or small-to-big |
| Retrieved chunks are fragments of the same section | Small-to-big with auto-merge threshold |
| Hundreds of large documents; wrong document gets picked | Per-document indexes + router, or metadata filter |
| Questions about relationships across entities | Graph approach (outside the TS core) |
| None of the above, just mediocre results | Revisit parsing, chunking, hybrid search and reranking first |

## Common mistakes

**Embedding the window.** Defeats sentence-window retrieval. Check `MetadataMode.EMBED` output.

**Returning duplicate parents.** Several children map to one parent; dedupe before synthesis.

**Losing the parent store on restart.** Hand-built parent lookups need persistent storage.

**Adding patterns before measuring.** Each one multiplies index size and ingestion cost.

**Mixing chunking regimes in one index** without recording which is which in metadata.

## Debugging

- Print what the LLM actually receives: after postprocessors, are the texts the expanded windows/parents, or still the tiny matches?
- Count nodes per document before and after; sentence-window and child nodes inflate counts by design.
- For parent expansion, log `parentId` hit counts per query to see whether merging triggers.
- Compare answers and retrieval with the pattern on and off on the same evaluation questions.

## Quick Summary

- The goal: match on small units, answer from larger context.
- Sentence-window is built in (`SentenceWindowNodeParser` + `MetadataReplacementPostProcessor("window")`); keep the window out of the embedding text.
- Small-to-big and auto-merging are easy to hand-build with two splitters, a `parentId` in metadata, and a postprocessor; persist the parent store.
- Recursive retrieval can be approximated with per-document indexes plus routing; prefer deterministic filters when possible.
- Graph retrieval isn't in the TS core; use a graph database or a Python service when relationships really matter.
- Earn each pattern with evaluation results; otherwise stick with good chunking, hybrid search and reranking.

## Next

`03-tools-and-agents/01-tools.md`: turning query engines and functions into tools an agent can call.
