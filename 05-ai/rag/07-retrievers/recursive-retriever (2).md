# Recursive Retriever

> **Not available in LlamaIndex.TS - this file shows a hand-rolled TypeScript implementation.** The Python framework has `RecursiveRetriever` and `IndexNode`; the LlamaIndex.TS documentation read on 2026-10-10 does not. The patterns below use a `parentId` in node metadata plus a `Map` lookup, built only on `BaseRetriever`, `TextNode` and `NodeWithScore`. Import paths vary by version (verify).

## Concept
A **recursive retriever** follows references between nodes. It retrieves a node that *points to* something else (another node or retriever), then returns or searches the target. This decouples **what you search** (a small, well-matched representation) from **what you return** (the full, rich content).

```
Search over:   small summary / sentence / table description
                         │ points to (metadata.parentId)
Return:        full document / parent section / actual table / sub-index
```

## When to Use
- **Small-to-big**: match on small chunks or summaries, return the larger context.
- **Document hierarchies**: search document summaries, then search inside the chosen document.
- **Tables and structured data**: embed a table description, return the table itself.
- **Heterogeneous sources**: a reference points to a specific retriever for that source.

## Related Choice
For fixed parent/child chunk hierarchies with merge logic, see [auto-merging-retriever.md](auto-merging-retriever.md). Use recursive retrieval for flexible, custom reference structures.

## Prerequisites
- `06-llamaindex/nodes-and-parsers.md`
- [vector-retriever.md](vector-retriever.md)

## Key Concept: A Reference in Metadata
Python uses an `IndexNode` with an `index_id`. In TypeScript, store the target ID in metadata on an ordinary `TextNode`:

```ts
import { TextNode } from "@llamaindex/core/schema"; // verify path

const summaryNode = new TextNode({
  text: "Summary: Q3 financial results, revenue and costs by region.",
  metadata: { parentId: "q3-report" }, // points to something registered under this ID
});
```
The node's text is what gets embedded; `metadata.parentId` is the pointer you resolve after retrieval.

## Pattern 1: Small-to-Big (chunk to parent)
```ts
import { Document, SentenceSplitter, VectorStoreIndex } from "llamaindex";
import { BaseRetriever } from "@llamaindex/core/retriever"; // verify path
import type { QueryBundle, NodeWithScore } from "@llamaindex/core"; // verify path
import type { TextNode } from "@llamaindex/core/schema";

const bigSplitter = new SentenceSplitter({ chunkSize: 1024, chunkOverlap: 100 });
const smallSplitter = new SentenceSplitter({ chunkSize: 128, chunkOverlap: 0 });

const bigNodes = (await bigSplitter.transform(documents)) as TextNode[]; // verify: transform returns nodes
const parents = new Map<string, TextNode>(); // target lookup
const smallNodes: TextNode[] = [];

for (const big of bigNodes) {
  parents.set(big.id_, big);
  const pieces = (await smallSplitter.transform([new Document({ text: big.text })])) as TextNode[];
  for (const small of pieces) {
    small.metadata = { ...small.metadata, parentId: big.id_ };
    smallNodes.push(small);
  }
}

const index = await VectorStoreIndex.init({ nodes: smallNodes }); // embed the SMALL nodes (verify init/fromNodes)
const vectorRetriever = index.asRetriever({ similarityTopK: 6 });

class SmallToBigRetriever extends BaseRetriever {
  constructor(
    private base: BaseRetriever,
    private parents: Map<string, TextNode>,
  ) {
    super();
  }

  async _retrieve(query: QueryBundle): Promise<NodeWithScore[]> {
    const hits = await this.base.retrieve(query);
    const best = new Map<string, NodeWithScore>(); // dedupe parents, keep best child score
    for (const hit of hits) {
      const parentId = hit.node.metadata.parentId as string | undefined;
      const parent = parentId ? this.parents.get(parentId) : undefined;
      const target = parent ?? hit.node; // fall back to the small node if unresolved
      const prev = best.get(target.id_);
      if (!prev || (hit.score ?? 0) > (prev.score ?? 0)) {
        best.set(target.id_, { node: target, score: hit.score });
      }
    }
    return [...best.values()].sort((a, b) => (b.score ?? 0) - (a.score ?? 0));
  }
}

const retriever = new SmallToBigRetriever(vectorRetriever, parents);
const nodes = await retriever.retrieve({ query: "What are the refund conditions?" }); // returns BIG nodes
```
Small chunks give precise matches; the returned result is the big chunk, carrying context. Several small hits on the same parent collapse into one result.

## Pattern 2: Document Summaries to Per-Document Retrievers
`DocumentSummaryIndex` is not documented for LlamaIndex.TS, so create the summaries yourself (for example one LLM call per document at ingestion time).

```ts
const docRetrievers = new Map<string, BaseRetriever>();
for (const [docId, docIndex] of docIndexes) {
  docRetrievers.set(docId, docIndex.asRetriever({ similarityTopK: 3 }));
}

// Top level: one summary node per document, pointing at the document ID
const topNodes = [...docIndexes.keys()].map(
  (docId) => new TextNode({ text: summaries[docId], metadata: { parentId: docId } }),
);
const topIndex = await VectorStoreIndex.init({ nodes: topNodes }); // verify
const topRetriever = topIndex.asRetriever({ similarityTopK: 2 });

class DocumentRouterRetriever extends BaseRetriever {
  constructor(
    private top: BaseRetriever,
    private perDoc: Map<string, BaseRetriever>,
  ) {
    super();
  }

  async _retrieve(query: QueryBundle): Promise<NodeWithScore[]> {
    const topHits = await this.top.retrieve(query); // step 1: pick documents from summaries
    const lists = await Promise.all(
      topHits.map((hit) => this.perDoc.get(hit.node.metadata.parentId as string)?.retrieve(query) ?? []),
    ); // step 2: search inside only those
    return lists.flat().sort((a, b) => (b.score ?? 0) - (a.score ?? 0));
  }
}
```
Step 1 picks the relevant documents from summaries; step 2 searches inside only those. This scales to many documents while keeping each search focused. Scores from different per-document indexes are comparable only if they use the same embedding model and metric.

## Pattern 3: Tables
Embed a short table description as a node whose `parentId` is a table ID; resolve that ID to the table itself (a `Map` of table nodes) or to a function that queries it (your own SQL or dataframe code). The retriever dispatches to whatever the ID maps to.

## Pattern 4: Heterogeneous Sources
Different IDs map to different retrievers (a vector retriever for docs, a wrapper around a SQL service for data). This is the same `Map<string, BaseRetriever>` as Pattern 2. It overlaps with routing ([router-retriever.md](router-retriever.md)); here the match on content decides the route, not an LLM selector.

## Use in a Query Engine
```ts
import { RetrieverQueryEngine } from "llamaindex/engines";

const engine = new RetrieverQueryEngine({ retriever });
const response = await engine.query({ query: "What are the refund conditions?" });
```

## Storage Requirements
The `parents` Map must resolve every target ID. In production the target nodes must live in a **durable store** (a docstore, JSON file or database) available to the app, not just in the vector store (`06-llamaindex/storage.md`). With Pinecone, remember that vectors-only storage does not give you parent nodes automatically: either keep the parent text in a database keyed by `parentId`, or accept a size-limited copy in metadata (verify Pinecone metadata size limits). Rebuild the Map at startup from that store.

## Debugging
- Log the small-node hits and the resolved parents for a few queries.
- Check that every `parentId` exists in the Map; a missing one silently falls back to the small node in the code above (or returns nothing in Pattern 2).
- Compare small-node hits with returned big nodes to confirm the mapping.

## Important Rules
- Embed the small representation; return the big one.
- Keep a complete, durable ID mapping from small to big.
- Make sure big nodes fit your context budget; many large parents can blow it.

## Common Mistakes
- Forgetting Map entries, so references cannot resolve.
- Returning many large parents and overflowing the context window.
- Losing the mapping on restart (in-memory Map not persisted).
- Using it where auto-merging or sentence-window would be simpler.

## Related / Next
- [auto-merging-retriever.md](auto-merging-retriever.md)
- [router-retriever.md](router-retriever.md)
- `10-context-and-generation/context-window-and-budget.md`
