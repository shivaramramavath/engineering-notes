# RAG vs Fine-Tuning

## Concept
Both techniques adapt an LLM to your use case, but they change different things:

- **RAG** changes what the model *sees* at query time (its input).
- **Fine-tuning** changes what the model *is* (its weights).

This chapter is conceptual and language-neutral; the comparison is the same whether your app is written in TypeScript or anything else.

## Rule of Thumb
> **RAG for knowledge, fine-tuning for behavior.**

If the model needs to *know* something, retrieve it. If the model needs to *act* differently (tone, format, task skill), fine-tune it.

## Comparison

| Aspect | RAG | Fine-tuning |
|---|---|---|
| Best for | Facts, documents, changing data | Style, format, domain language, task skill |
| Updating knowledge | Re-index the changed documents | Retrain |
| Source citations | Natural (you know which chunks were used) | Not possible |
| Hallucination control | Better (answers grounded in context) | Does not fix it |
| Access control | Filter at retrieval time | Cannot restrict what the model "knows" |
| Upfront cost | Low to medium (build an ingestion pipeline) | Medium to high (data prep, training, evaluation) |
| Per-query cost | Higher (longer prompts, retrieval step) | Lower prompts |
| Latency | Extra retrieval hop | None added |
| Failure mode | Bad retrieval, bad chunking | Overfitting, forgetting, stale knowledge |

## Decision Guide
- Need answers from private or frequently changing documents → **RAG**
- Need the model to always reply in a specific JSON schema or brand voice → **fine-tune** (or try prompting first; structured output features of your LLM SDK may be enough)
- Need both fresh knowledge and a specialized style → **combine them**: fine-tune for behavior, RAG for facts
- Small, static knowledge that fits in the prompt → **just put it in the prompt**

## Common Mistakes
- Fine-tuning to "teach" the model facts. It memorizes unreliably and can't be updated cheaply.
- Using RAG to fix a style or format problem. A better prompt or fine-tune is the right tool.
- Skipping the simplest option: good prompting with the data in context.

## Interview Angle
A typical question is "When would you choose RAG over fine-tuning?" A good answer covers data freshness, citations, access control, and cost of updates, and mentions that the two can be combined.

## Related / Next
- [what-is-rag.md](what-is-rag.md)
- [rag-pipeline.md](rag-pipeline.md)
