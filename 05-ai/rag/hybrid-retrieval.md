# Summary Retrievers

> **Accuracy note.** TypeScript edition; code was NOT compiled or run. `SummaryIndex` exists in LlamaIndex.TS and returns all nodes by default. The embedding and LLM retriever modes of Python's `SummaryIndex` are not documented for TypeScript, and `DocumentSummaryIndex` is not documented for TypeScript. Those parts are flagged below with hand-rolled, unverified equivalents.

## Concept
Top-k vector retrieval sees only a few chunks, so it cannot answer questions that need the **whole** document or corpus ("summarize this report", "list every deadline mentioned"). Summary-based retrievers address that in two ways:

1. **`SummaryIndex` retrievers**: return all nodes, or a ranked subset (all nodes is the documented TypeScript behavior).
2. **`DocumentSummaryIndex` retrievers**: use per-document summaries to pick the relevant *documents* first, then return those documents' nodes (Python; hand-rolled below for TypeScript).

## When to Use
- Whole-document summarization or extraction across all of a document.
- Bounded corpora (one report, one contract) where reading everything is acceptable.
- "Which documents are relevant?" at corpus scale, before looking inside them.

## When Not to Use
- Large corpora where sending all nodes to an LLM is too slow or costly.
- Pinpoint fact lookup. A vector retriever is better and cheaper.

## Prerequisites
- `06-llamaindex/indexes.md`
- `06-llamaindex/response-synthesizer.md`

## SummaryIndex Retrievers
```ts
import { SummaryIndex } from "llamaindex";

const summaryIndex = await SummaryIndex.fromDocuments(documents);

const allNodes = summaryIndex.asRetriever();                 // default: ALL nodes
const nodes = await allNodes.retrieve({ query: "Summarize the key obligations." });
```

Python's `retriever_mode="embedding"` and `retriever_mode="llm"` (with `choice_batch_size`) are **not documented for LlamaIndex.TS**. Some TypeScript versions may include an LLM-based summary retriever in source; whether and how it is exposed through `asRetriever` is unknown here (verify in your installed package). Do not pass Python option names such as `retriever_mode` or `choice_batch_size`.

| Mode | Behavior | Cost | In LlamaIndex.TS? |
|---|---|---|---|
| default | Returns every node | No retrieval cost; heavy synthesis cost | Yes (documented) |
| `embedding` | Ranks nodes by embedding similarity | Embedding calls | Not documented; use a `VectorStoreIndex` retriever, which is the same idea (`index.asRetriever({ similarityTopK: 5 })`) |
| `llm` | LLM scores each batch of nodes for relevance | LLM calls per batch | Not documented (verify); sketch below |

### Hand-Rolled LLM Relevance Selection (unverified sketch)
A minimal stand-in for the Python `llm` mode: ask the LLM which numbered nodes in a batch are relevant. Not a library feature.
```ts
import { Settings, MetadataMode } from "llamaindex";
import type { NodeWithScore } from "@llamaindex/core";   // path varies by version; verify

async function llmSelectNodes(
  query: string,
  all: NodeWithScore[],        // e.g. await summaryIndex.asRetriever().retrieve({ query })
  batchSize = 10,
): Promise<NodeWithScore[]> {
  const picked: NodeWithScore[] = [];
  for (let start = 0; start < all.length; start += batchSize) {
    const batch = all.slice(start, start + batchSize);
    const listing = batch
      .map((n, i) => `${i}: ${n.node.getContent(MetadataMode.NONE).slice(0, 500)}`)
      .join("\n\n");
    const res = await Settings.llm.complete({                           // verify: complete() params and result shape
      prompt:
        `Question: ${query}\n\nPassages:\n${listing}\n\n` +
        `Reply with ONLY a comma-separated list of the passage numbers relevant to the question, or "none".`,
    });
    for (const m of res.text.matchAll(/\d+/g)) {
      const idx = Number(m[0]);
      if (idx < batch.length) picked.push(batch[idx]);
    }
  }
  return picked;
}
```
One LLM call per batch, so cost grows with corpus size, exactly as in the table above.

Pair the default mode with a summarizing response mode:
```ts
import { getResponseSynthesizer } from "llamaindex";   // verify export and option names

const engine = summaryIndex.asQueryEngine({
  responseSynthesizer: getResponseSynthesizer("tree_summarize"),   // verify: how the mode is selected in your version
});
const response = await engine.query({ query: "Summarize the key obligations in this contract." });
console.log(response.toString());
```
`tree_summarize` summarizes groups of chunks hierarchically. Some versions may also accept a response-mode option directly on `asQueryEngine` (verify). See `06-llamaindex/response-synthesizer.md`.

## DocumentSummaryIndex
**Not documented for LlamaIndex.TS.** Python's `DocumentSummaryIndex` generates a summary for each document at build time, then retrieves **documents** by summary relevance. In TypeScript, build the same idea yourself: summarize each document, index the summaries, and map hits back to the documents' nodes. This is a hand-rolled, unverified sketch.

```ts
import { Document, Settings, SentenceSplitter, VectorStoreIndex } from "llamaindex";
import { BaseRetriever } from "@llamaindex/core/retriever";            // verify path
import type { NodeWithScore, QueryBundle } from "@llamaindex/core";   // verify path

// Build: one summary per document (cost: one LLM call per document)
const splitter = new SentenceSplitter({ chunkSize: 512, chunkOverlap: 50 });
const nodesByDoc = new Map<string, NodeWithScore["node"][]>();
const summaryDocs: Document[] = [];

for (const doc of documents) {
  const res = await Settings.llm.complete({                             // verify: complete() params and result shape
    prompt: `Summarize this document in 5 sentences, naming its main topics:\n\n${doc.text}`,
  });
  summaryDocs.push(new Document({ text: res.text, metadata: { docId: doc.id_ } }));
  nodesByDoc.set(doc.id_, (await splitter.transform([doc])) as NodeWithScore["node"][]);   // verify: transform returns nodes
}

// Index the summaries only
const summaryVectorIndex = await VectorStoreIndex.fromDocuments(summaryDocs);
const summaryRetriever = summaryVectorIndex.asRetriever({ similarityTopK: 3 });

class DocumentSummaryRetriever extends BaseRetriever {
  async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
    const query = typeof params === "string" ? params : params.query;
    const hitSummaries = await summaryRetriever.retrieve({ query });
    return hitSummaries.flatMap((h) =>
      (nodesByDoc.get(h.node.metadata["docId"] as string) ?? []).map((node) => ({ node, score: h.score })),
    );
  }
}

const retriever = new DocumentSummaryRetriever();
const nodes = await retriever.retrieve({ query: "Which policy covers remote work?" });
```
Build cost is one summarization per document. The summaries and the `nodesByDoc` map live in memory here; persist them (a docstore, JSON file or database) or you will pay the summarization cost on every app start. Whether `DocumentSummaryIndex` appears in a newer LlamaIndex.TS release is something to check (verify) before you use this sketch.

Two-stage pattern for large corpora:
1. Retrieve the best **documents** using summaries.
2. Run a vector retriever **inside** those documents (for example with a metadata filter on a document-ID field on the chunks, verify as in [vector-retriever.md](vector-retriever.md)).
This is the idea behind recursive retrieval ([recursive-retriever.md](recursive-retriever.md)) and sub-question engines.

## Cost Awareness
A default summary retrieval sends the entire corpus through the LLM. Estimate tokens before running:
```
cost ≈ total document tokens × (1 + summarization overhead) × price per token
```
Cache results for documents that rarely change.

## Important Rules
- Match the tool to the question type: pinpoint facts → vector; whole-document questions → summary.
- Use `tree_summarize` for summarization queries.
- Bound the corpus size or use the two-stage pattern.
- Do not pass Python-only options (`retriever_mode`, `choice_batch_size`) to the TypeScript `SummaryIndex`.

## Common Mistakes
- Using vector top-k for "summarize everything" and getting a partial summary that sounds complete.
- Running default `SummaryIndex` retrieval over a huge corpus.
- Rebuilding document summaries on every app start.
- Assuming `DocumentSummaryIndex` exists in LlamaIndex.TS because it exists in Python.

## Related / Next
- [router-retriever.md](router-retriever.md) (route summary questions to this retriever)
- [recursive-retriever.md](recursive-retriever.md)
- `08-query-transformation/sub-question-query-engine.md`
