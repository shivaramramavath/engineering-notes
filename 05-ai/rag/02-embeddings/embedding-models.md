# Embedding Models

## Concept
Choosing an embedding model is one of the highest-impact decisions in a RAG system. It determines retrieval quality, cost, latency, vector dimension, and whether your data leaves your infrastructure.

> Model names, prices and rankings change quickly. Treat the examples below as categories, and verify current options against the provider docs and a benchmark such as MTEB before deciding.

## Prerequisites
- [embeddings-basics.md](embeddings-basics.md)

## Two Families

| | Hosted API models | Open-source / local models |
|---|---|---|
| Examples | OpenAI `text-embedding-3-small` / `-large`, Cohere Embed, Voyage | Open models such as `all-MiniLM-L6-v2`, BGE, E5, GTE (run via an ONNX/Transformers.js-style runtime in Node) |
| Setup | API key | Download weights; run on CPU or GPU |
| Cost | Per token | Compute and hosting cost |
| Privacy | Data sent to provider | Data stays local |
| Ops burden | Low | You manage serving and scaling |
| Typical dimensions | 1024 to 3072 | 384 to 1024 |

## What to Compare

| Factor | Why it matters |
|---|---|
| Retrieval quality | Check MTEB retrieval scores, then test on **your** data |
| Dimension | Drives storage, memory and query cost |
| Max input tokens | Chunks longer than this are truncated |
| Language support | Multilingual corpora need a multilingual model |
| Domain fit | Code, legal, medical and similar text may need specialized models |
| Latency and throughput | Matters for query-time embedding and for bulk ingestion |
| Price | Ingestion embeds your whole corpus; queries embed every question |
| Instruction/prefix needs | Some models expect prefixes such as `query:` and `passage:` |

## Using Models in LlamaIndex.TS
```ts
import { Settings } from "llamaindex";
import { OpenAIEmbedding } from "@llamaindex/openai";

// Hosted (documented)
Settings.embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });
```

Local options (verify before relying on them):
```ts
// Option A (verify): a LlamaIndex.TS HuggingFace embedding integration, which I believe exists as
// the separate package @llamaindex/huggingface and runs models locally. Check the package name,
// class name (HuggingFaceEmbedding) and constructor options in the current docs.
//
// Option B (verify): use a Transformers.js-style library (e.g. @xenova/transformers or
// @huggingface/transformers) directly in Node, and wrap it in your own embedding class by
// extending BaseEmbedding from "@llamaindex/core/embeddings" (check the abstract methods
// your version requires, typically getTextEmbedding / getQueryEmbedding).
```

I could not confirm either option's exact API without installing the packages, so treat both as starting points, not working code.

Other hosted providers (Cohere, Voyage, Gemini, and so on) generally ship as separate `@llamaindex/<name>` packages; verify availability for the one you want. The Python framework has a very large embeddings catalog; the TypeScript catalog is smaller.

## How to Choose
1. Start with a solid, inexpensive default to get a baseline.
2. Build a small evaluation set of real questions with the correct source chunks (see `14-evaluation/`).
3. Compare two or three candidate models on retrieval metrics such as recall@k and MRR.
4. Pick the cheapest model that meets your quality bar.

Do not choose by benchmark rank alone. A model that tops a general leaderboard can lose to a smaller one on your domain.

## Important Rules
- The model you pick at ingestion is the model you must use at query time.
- Record the model name and version alongside the index (for example in the index name or metadata).
- Match the index dimension to the model's output dimension.
- Respect the model's input limit when setting chunk size.

## Common Mistakes
- Picking the biggest model by default, then paying for storage you don't need.
- Re-embedding the corpus repeatedly because the model was never settled.
- Using an English-only model on multilingual data.
- Ignoring required query and passage prefixes for models that need them.

## Related / Next
- [similarity-metrics.md](similarity-metrics.md)
- [embedding-strategies.md](embedding-strategies.md)
- `14-evaluation/retrieval-metrics.md`
