# Pinecone Scaling and Cost

## Concept
Pinecone-specific behavior for growth and spend. General production guidance is in `16-production/cost-and-scaling.md`.

> Pricing, plan limits and quotas change. This file explains **what drives cost** and how to control it. It does not state prices. Check <https://www.pinecone.io/pricing> and the current docs for numbers.

## Prerequisites
- [indexes.md](indexes.md)
- [namespaces-and-filters.md](namespaces-and-filters.md)

## How Serverless Is Billed (conceptually)

| Driver | What increases it |
|---|---|
| **Storage** | Number of vectors, dimension, size of metadata |
| **Write units** | Upserts, updates, deletes (more data per request, more units) |
| **Read units** | Queries, fetches, lists; roughly scale with how much data a query must search (for example the size of the namespace queried) |
| **Plan minimums** | Paid plans typically have a minimum monthly commitment (verify) |
| **Extras** | Inference (embedding/rerank), backups, import, support tiers |

Because read cost tends to follow the size of the namespace being searched, **many small namespaces are generally cheaper to query than one huge namespace** that you always filter down (verify against the current cost documentation).

## Storage Estimate
```
raw size ≈ vectors × dimension × 4 bytes   (+ metadata + overhead)
```
Example: 5 million vectors at 1536 dimensions ≈ 5,000,000 × 1536 × 4 ≈ 30.7 GB raw, before metadata and overhead.

```ts
const rawGb = (vectors: number, dimension: number) => (vectors * dimension * 4) / 1e9;
console.log(rawGb(5_000_000, 1536).toFixed(1)); // 30.7
```

## Ways to Cut Cost

| Lever | How |
|---|---|
| Lower dimension | Use a smaller embedding model, or a model that supports shortened vectors |
| Smaller metadata | Do not store large text blobs you don't need; keep fields short |
| Fewer, better chunks | Larger chunks or smarter chunking reduce vector count |
| Namespace per tenant/session | Queries scan less data; whole namespaces can be deleted cheaply |
| Delete stale data | Expire old sessions and superseded document versions |
| Don't re-embed unchanged content | Docstore + cache (`03-data-ingestion/ingestion-pipeline.md`) |
| Cache frequent queries | Skip identical lookups (`16-production/caching.md`) |
| Return less | `includeValues: false`, reasonable `topK` |
| Avoid chatty patterns | Batch upserts; do not fetch one ID at a time |
| Use a reranker on fewer candidates | Fetch top 20-50 and rerank, rather than top 500 |

## Scaling Behavior
- Serverless scales storage and throughput without provisioning, but **rate limits** exist per project and operation. Handle `429` responses with exponential backoff (`16-production/rate-limits-and-retries.md`).
- Bulk ingestion: batch, parallelize with bounded concurrency (see [upsert.md](upsert.md)), and consider bulk import for very large loads (verify availability).
- Query latency depends on namespace size, filters, `topK` and region. Measure p95 from your application's region.
- In a Node service, create one `Pinecone` client and one `Index` handle per process and reuse them, rather than constructing them per request.

## Capacity Planning Checklist
- [ ] Estimated vector count now and in 12 months
- [ ] Dimension and metadata size per record
- [ ] Namespace layout and expected count
- [ ] Peak query rate and acceptable p95 latency
- [ ] Ingestion volume and update frequency
- [ ] Data retention rules (when does data get deleted?)
- [ ] Monthly cost estimate from the pricing page, with a margin

## Monitoring Spend
- Watch usage and cost in the Pinecone console.
- Alert on sudden growth in vector count or read units. A runaway ingestion loop or a missing deletion job shows up here first.
- Tag or name indexes by environment so test traffic is not billed as production mystery spend.

## Important Rules
- Estimate cost before bulk-loading a large corpus.
- Delete data you no longer need; stale sessions and old versions accumulate.
- Test limits and latency with realistic volumes, not toy data.
- Rebuilding an index (new model) temporarily doubles storage; plan for that.

## Common Mistakes
- Using a very high-dimension model by default and paying for storage that adds little quality.
- Keeping every chat session's namespace forever.
- Putting everything in one giant namespace, then filtering heavily.
- No retry/backoff, so bursts of 429s become failures.
- Assuming the free tier's limits apply to production.

## Related / Next
- `16-production/cost-and-scaling.md`
- `16-production/observability.md`
- `12-chat/unloading-data-in-chat.md`
