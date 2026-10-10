# Similarity Metrics

## Concept
A similarity (or distance) metric turns two vectors into a single number saying how alike they are. Vector search uses it to rank stored vectors against the query vector.

## Prerequisites
- [embeddings-basics.md](embeddings-basics.md)

## The Three Common Metrics

| Metric | Measures | Range | Higher means |
|---|---|---|---|
| **Cosine similarity** | Angle between vectors (direction only) | -1 to 1 | More similar |
| **Dot product** | Angle and magnitude | Unbounded | More similar |
| **Euclidean distance** | Straight-line distance | 0 to infinity | **Less** similar |

### Cosine similarity
```
cos(a, b) = (a · b) / (|a| × |b|)
```
Ignores vector length, so a short and a long text about the same topic still match. This is the usual default for text embeddings.

### Dot product
```
a · b = Σ aᵢ × bᵢ
```
If vectors are **normalized** (length 1), dot product gives the same ranking as cosine, and is slightly cheaper to compute. Magnitude carries information in some models, so dot product can behave differently on unnormalized vectors.

### Euclidean distance
```
d(a, b) = √ Σ (aᵢ − bᵢ)²
```
Smaller is closer. Less common for text, but used by some models.

## Example
Plain TypeScript number-array math, no libraries:

```ts
const dot = (a: number[], b: number[]): number =>
  a.reduce((sum, ai, i) => sum + ai * b[i], 0);

const norm = (a: number[]): number => Math.sqrt(dot(a, a));

const cosine = (a: number[], b: number[]): number =>
  dot(a, b) / (norm(a) * norm(b));

const euclidean = (a: number[], b: number[]): number =>
  Math.sqrt(a.reduce((sum, ai, i) => sum + (ai - b[i]) ** 2, 0));

const a = [1.0, 2.0, 3.0];
const b = [2.0, 4.0, 6.5];

console.log(cosine(a, b), dot(a, b), euclidean(a, b));
```

Run with `npx tsx similarity.ts`. For real embeddings, `a` and `b` would be the `number[]` returned by the embedding model (see [embeddings-basics.md](embeddings-basics.md)); both must have the same length.

## Which One to Use
- **Use the metric the embedding model was trained for.** Its documentation says which. Most text models are designed for cosine (or dot product on normalized vectors).
- If the model outputs normalized vectors, cosine and dot product rank identically.
- If you use sparse and dense vectors together in hybrid search, dot product is typically required by the vector store. Check the store's docs.

## Setting the Metric
The metric is chosen **when the index is created** and generally cannot be changed afterward.

```ts
// Pinecone TypeScript SDK example (verify option names per SDK version)
import { Pinecone } from "@pinecone-database/pinecone";

const pc = new Pinecone({ apiKey: process.env.PINECONE_API_KEY! });

await pc.createIndex({
  name: "docs",
  dimension: 1536,
  metric: "cosine",     // or "dotproduct", "euclidean"
  spec: { serverless: { cloud: "aws", region: "us-east-1" } },
  deletionProtection: "enabled",
  waitUntilReady: true,
});
```

## Scores Are Relative, Not Absolute
A cosine score of 0.78 does not mean "78% relevant". Score ranges depend on the model and the data. Do not hard-code a universal threshold; calibrate on your own queries (see `07-retrievers/retrieval-basics.md`).

## Common Mistakes
- Choosing a metric different from the one the model was trained for.
- Treating Euclidean distance as a similarity (lower is better, not higher).
- Setting a fixed similarity cutoff copied from a tutorial.
- Forgetting the metric can't be changed later without rebuilding the index.

## Related / Next
- [embedding-strategies.md](embedding-strategies.md)
- `04-vector-databases/vector-db-concepts.md`
- `05-pinecone/indexes.md`
