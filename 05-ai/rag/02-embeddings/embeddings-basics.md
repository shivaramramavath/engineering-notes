# Embeddings Basics

## Concept
An **embedding** is a list of numbers (a vector) that represents the *meaning* of a piece of text. Texts with similar meaning get vectors that are close together; unrelated texts get vectors that are far apart.

```
"How do I reset my password?"      → [0.12, -0.48, 0.91, ...]
"I forgot my login credentials"    → [0.10, -0.45, 0.88, ...]   (close)
"Best pizza in town"               → [-0.70, 0.33, 0.05, ...]   (far)
```

This is what lets RAG find relevant text even when the question and the document share no keywords.

## Prerequisites
- [Documents and nodes](../01-fundamentals/documents-and-nodes.md)

## How It Works
1. Text goes into an **embedding model** (a neural network).
2. The model outputs a fixed-length vector, for example 384, 768, 1536 or 3072 numbers (in TypeScript, a `number[]`).
3. Vectors are stored in a vector database.
4. At query time the question is embedded by the **same model**, and the nearest vectors are returned.

## Dimensions
The **dimension** is the length of the vector. It is fixed by the model.

| Dimension size | Effect |
|---|---|
| Higher | Can capture more nuance; more storage, memory and search cost |
| Lower | Cheaper and faster; may lose some detail |

Important consequences:
- A vector index is created with a fixed dimension. Every vector inserted must match it.
- Changing the embedding model usually changes the dimension, so you must create a new index and re-embed everything.
- Some models support shortened ("truncated") output dimensions. Check the model's documentation (for OpenAI's `text-embedding-3-*` models this is a `dimensions` option; verify whether your `OpenAIEmbedding` version exposes it).

## Example (LlamaIndex.TS)
```ts
import { OpenAIEmbedding } from "@llamaindex/openai";   // verify import path for your version

const embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });

const vec: number[] = await embedModel.getTextEmbedding("How do I reset my password?");  // verify method name
console.log(vec.length);        // dimension of this model's output (1536 for this model)
console.log(vec.slice(0, 5));
```

Set it globally so indexes and queries use the same model:
```ts
import { Settings } from "llamaindex";

Settings.embedModel = embedModel;
```

`OpenAIEmbedding` reads `OPENAI_API_KEY` from the environment. Run with `npx tsx --env-file=.env embed.ts`.

## What Embeddings Capture and Miss
**Good at:** paraphrases, synonyms, topical similarity, cross-phrasing matches.

**Weak at:**
- Exact identifiers (error codes, SKUs, names). Use keyword or hybrid search.
- Negation ("allowed" vs "not allowed" can look similar).
- Precise numbers and dates.
- Text in a language or domain the model wasn't trained on.

## Important Rules
- Use the **same model** for documents and queries.
- Never mix vectors from different models in one index.
- Embedding quality sets a ceiling on retrieval quality; a better LLM cannot recover chunks that were never retrieved.

## Common Mistakes
- Changing the model but keeping the old index.
- Embedding huge chunks that exceed the model's input limit. The text is truncated silently or the call fails.
- Expecting embeddings to match exact keywords.

## Related / Next
- [embedding-models.md](embedding-models.md)
- [similarity-metrics.md](similarity-metrics.md)
