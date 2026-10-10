# Pinecone Indexes

## Concept
An **index** stores vectors of one fixed dimension and searches them with one fixed metric. Everything else (namespaces, metadata, filtering) lives inside an index.

## Prerequisites
- [setup.md](setup.md)
- `02-embeddings/similarity-metrics.md`

## Serverless Indexes
Serverless is the standard choice for new projects: Pinecone manages capacity, and you pay for storage and operations rather than provisioning machines. Older tutorials describe **pod-based** indexes (you pick pod type and size). Prefer serverless unless you have a specific reason and have confirmed pods are still offered for your plan (verify).

## Create an Index
```ts
import { Pinecone } from "@pinecone-database/pinecone";

const pc = new Pinecone({ apiKey: process.env.PINECONE_API_KEY! });

const existing = await pc.listIndexes(); // verify result shape: { indexes?: [{ name }] }
const exists = existing.indexes?.some((i) => i.name === "docs") ?? false;

if (!exists) {
  await pc.createIndex({
    name: "docs",
    dimension: 1536,                 // must match your embedding model
    metric: "cosine",                // "cosine" | "dotproduct" | "euclidean"
    spec: { serverless: { cloud: "aws", region: "us-east-1" } },
    deletionProtection: "disabled",  // use "enabled" in production
    waitUntilReady: true,            // verify option name per SDK version
  });
}
```
Option names (`deletionProtection`, `waitUntilReady`) differ between SDK versions; verify against yours.

| Setting | Guidance |
|---|---|
| `name` | Lowercase letters, numbers, hyphens. Include the embedding model or version if you may rebuild (for example `docs-te3s-v1`). |
| `dimension` | Must equal the embedding model's output size. **Fixed for the index's life.** |
| `metric` | Use what the embedding model recommends (usually `cosine`). Use `dotproduct` for hybrid dense-sparse indexes. **Fixed for the index's life.** |
| `spec` | Cloud and region. Choose a region close to your application and compliant with your data rules. |
| `deletionProtection` | Turn on in production to prevent accidental deletes. |

## Wait Until Ready
Creation is asynchronous. Use the SDK's wait option if your version supports it (verify), or poll:
```ts
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

while (!(await pc.describeIndex("docs")).status?.ready) {
  await sleep(1000);
}
```

## Inspect, List, Delete
```ts
await pc.listIndexes();
const desc = await pc.describeIndex("docs");  // dimension, metric, host, status
const index = pc.index({ host: desc.host });  // newer API; older: pc.index("docs")
await index.describeIndexStats();

await pc.deleteIndex("docs");                 // irreversible; fails if protection is on
```

## Integrated Embedding Indexes
Pinecone also offers indexes where Pinecone hosts the embedding model and embeds text for you (verify current API and model list in the TypeScript SDK). Convenient for prototypes. In this repository we embed in our own pipeline, using LlamaIndex.TS, so that embedding model, chunking and updates stay under our control.

## Dense vs Sparse Indexes
- **Dense index**: embedding vectors (this chapter's default).
- **Sparse index** or sparse values: keyword-style vectors. See [sparse-and-hybrid.md](sparse-and-hybrid.md).

## Designing Index Layout

| Question | Recommendation |
|---|---|
| One index or many? | One index per embedding model/dimension. Use namespaces for tenants and collections |
| Dev vs prod? | Separate indexes (or projects) |
| Changing embedding model? | Create a new index, re-ingest, switch traffic, delete the old one |
| Many small tenants? | Namespaces, not indexes (check current limits) |

## Important Rules
- Dimension and metric cannot be changed after creation.
- One index must contain vectors from only one embedding model.
- Always enable deletion protection on production indexes.
- Keep the raw documents so rebuilding an index is possible.

## Common Mistakes
- Dimension mismatch (`1536` index, `3072` model), causing upsert errors. `PineconeVectorStore` in LlamaIndex.TS does not create the index for you; its dimension must match your `embedModel`.
- Using `euclidean` or `dotproduct` because a tutorial did, when the model expects `cosine`.
- Creating an index per tenant or per user.
- Forgetting that `deleteIndex` destroys all data immediately.

## Related / Next
- [upsert.md](upsert.md)
- [namespaces-and-filters.md](namespaces-and-filters.md)
- `16-production/cost-and-scaling.md`
