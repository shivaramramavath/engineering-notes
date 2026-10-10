# Auto-Merging Retriever

> **Not available in LlamaIndex.TS - this file shows a hand-rolled TypeScript implementation.** The Python framework has `HierarchicalNodeParser` and `AutoMergingRetriever`; the LlamaIndex.TS documentation read on 2026-10-10 has neither. The code below builds the hierarchy with two `SentenceSplitter` sizes and implements the merge rule yourself. Import paths vary by version (verify).

## Concept
Chunking forces a trade-off: small chunks match precisely but lack context; large chunks carry context but blur matching. The **auto-merging retriever** gets both:

1. Documents are parsed into a **hierarchy** (here two levels, for example 512 and 128 tokens).
2. Only the smallest **leaf** chunks are embedded and searched.
3. If *enough sibling leaves* of the same parent are retrieved, they are **merged** into the parent chunk, which is returned instead.

```
Parent (512 tokens)
 ├── leaf A  ← retrieved
 ├── leaf B  ← retrieved
 ├── leaf C  ← retrieved      → 3 of 4 hit → return the PARENT
 └── leaf D
```
When a question needs a whole section, the section comes back; when it needs one fact, the leaf does.

## When to Use
- Long, structured documents (manuals, reports, legal text) where answers span adjacent passages.
- Retrieval finds the right area but returns fragments that miss the surrounding explanation.

## When Not to Use
- Very short documents, or Q&A where every answer sits in one sentence.
- Setups where you cannot maintain a durable store of all parent nodes.

## Prerequisites
- `03-data-ingestion/chunking.md` (hierarchical splitting)
- `06-llamaindex/nodes-and-parsers.md`
- `06-llamaindex/storage.md`

## Step 1: Build the Hierarchy
```ts
import { Document, SentenceSplitter, VectorStoreIndex } from "llamaindex";
import { TextNode } from "@llamaindex/core/schema"; // verify path

const parentSplitter = new SentenceSplitter({ chunkSize: 512, chunkOverlap: 0 });
const leafSplitter = new SentenceSplitter({ chunkSize: 128, chunkOverlap: 0 });

const parents = new Map<string, TextNode>();            // ALL parents, keyed by id
const childCount = new Map<string, number>();           // leaves per parent
const leaves: TextNode[] = [];

const parentNodes = (await parentSplitter.transform(documents)) as TextNode[]; // verify: transform returns nodes
for (const parent of parentNodes) {
  parents.set(parent.id_, parent);
  const pieces = (await leafSplitter.transform([new Document({ text: parent.text })])) as TextNode[];
  for (const leaf of pieces) {
    leaf.metadata = { ...leaf.metadata, ...parent.metadata, parentId: parent.id_ };
    leaves.push(leaf);
  }
  childCount.set(parent.id_, pieces.length);
}
```
Two levels are enough for most corpora. For three levels (2048, 512, 128) repeat the idea: each node stores its own `parentId`, and the merge step can be applied repeatedly. You can also set a `NodeRelationship.PARENT` relationship on each leaf (verify), but metadata is simpler and survives vector-store round trips.

## Step 2: Index Leaves Only
```ts
const index = await VectorStoreIndex.init({ nodes: leaves }); // verify init/fromNodes; or use fromDocuments with a pipeline
const baseRetriever = index.asRetriever({ similarityTopK: 12 });
```

## Step 3: The Merging Retriever
```ts
import { BaseRetriever } from "@llamaindex/core/retriever"; // verify path
import type { NodeWithScore, QueryBundle } from "@llamaindex/core"; // verify path

class AutoMergingRetriever extends BaseRetriever {
  constructor(
    private base: BaseRetriever,
    private parents: Map<string, TextNode>,
    private childCount: Map<string, number>,
    private threshold = 0.5, // merge when hit ratio is strictly greater than this
  ) {
    super();
  }

  async _retrieve(query: QueryBundle): Promise<NodeWithScore[]> {
    const hits = await this.base.retrieve(query);

    // Group retrieved leaves by parent
    const byParent = new Map<string, NodeWithScore[]>();
    const orphans: NodeWithScore[] = [];
    for (const hit of hits) {
      const pid = hit.node.metadata.parentId as string | undefined;
      if (!pid || !this.parents.has(pid)) {
        orphans.push(hit);
        continue;
      }
      byParent.set(pid, [...(byParent.get(pid) ?? []), hit]);
    }

    const out: NodeWithScore[] = [...orphans];
    for (const [pid, group] of byParent) {
      const ratio = group.length / (this.childCount.get(pid) ?? Infinity);
      if (ratio > this.threshold) {
        const score = Math.max(...group.map((h) => h.score ?? 0));
        out.push({ node: this.parents.get(pid)!, score }); // merged parent
      } else {
        out.push(...group); // keep the leaves
      }
    }
    return out.sort((a, b) => (b.score ?? 0) - (a.score ?? 0));
  }
}

const retriever = new AutoMergingRetriever(baseRetriever, parents, childCount, 0.5);
const nodes = await retriever.retrieve({ query: "How do I reset the device?" });
```

## Use in a Query Engine
```ts
import { RetrieverQueryEngine } from "llamaindex/engines";

const engine = new RetrieverQueryEngine({ retriever });
```

## With Pinecone
```ts
import { PineconeVectorStore } from "@llamaindex/pinecone";

const vectorStore = new PineconeVectorStore({
  indexName: process.env.PINECONE_INDEX_NAME,
  namespace: "manuals",
});
const index = await VectorStoreIndex.fromVectorStore(vectorStore); // connect; leaves must already be upserted
```
Pinecone holds only the **leaf** vectors (upsert them with `VectorStoreIndex.fromDocuments`/an ingestion pipeline against this store, or the raw Pinecone SDK). The parents live in your `parents` Map, which must be **persisted and available at query time**. This is the main operational cost of this retriever. In production write `parents` and `childCount` to a docstore, a JSON file, or a database (Postgres, Redis, DynamoDB) keyed by node ID, and rebuild the Maps on startup. `SimpleDocumentStore` with `persist(path)` is a documented option for small setups (verify how to add nodes). A Map lost on restart means merging silently stops working. Details: `06-llamaindex/storage.md`.

Leaf text stored in Pinecone metadata (`textKey`) is how leaf content is returned; parents are not there, so do not expect the vector store to hand them back.

## Parameters

| Parameter | Meaning |
|---|---|
| `threshold` | Merge when `childrenHit / childrenTotal` is greater than this (0.5 here; Python's default is around 0.5) |
| `similarityTopK` of base retriever | Retrieve generously (10 to 20) so merging has siblings to combine |
| Splitter sizes | Parent and leaf chunk sizes, largest to smallest |

## Tuning
- If merging never happens: raise top-k or lower the threshold.
- If it merges too aggressively and returns huge parents: raise the threshold or reduce the parent size.
- Choose chunk sizes by evaluating answer quality (`14-evaluation/`). 512 and 128 are a starting point, not a rule.
- Watch the context budget: merged parents are large (`10-context-and-generation/context-window-and-budget.md`).

## Updating Documents
Both the leaf vectors and the stored parent nodes must be updated together, or merging can return stale parents. Handle this in your sync process (`11-document-management/refresh-and-sync.md`). Deleting a document means removing its leaves from Pinecone and all of its parents from your parent store.

## Comparison

| Technique | Search over | Returns | Notes |
|---|---|---|---|
| Plain vector | Chunks | Same chunks | Simple |
| Sentence-window | Single sentences | Sentence plus neighbors | `SentenceWindowNodeParser` + `MetadataReplacementPostProcessor` are documented |
| Recursive (small-to-big) | Small nodes | Mapped big nodes | Custom mapping ([recursive-retriever.md](recursive-retriever.md)) |
| **Auto-merging** | Leaf nodes | Leaves, or merged parents | Adaptive; needs a parent store |

## Important Rules
- Embed leaf nodes only; keep all parents in a durable store.
- Keep the parent store in sync with the vector store.
- Retrieve enough leaves for merging to trigger.
- Budget context for merged parents.

## Common Mistakes
- Embedding all hierarchy levels, which duplicates content in results.
- In-memory Map lost on restart, so parents cannot be found.
- Top-k so small that merging never occurs.
- Not updating the parent store when documents change.
- Forgetting that `childCount` must be recomputed when a parent is re-split.

## Related / Next
- [recursive-retriever.md](recursive-retriever.md)
- `03-data-ingestion/chunking.md`
- `11-document-management/update-documents.md`
