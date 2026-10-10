# Custom Retriever

## Concept
When no built-in retriever fits, subclass `BaseRetriever` and implement one method that takes a query and returns scored nodes. A custom retriever plugs into query engines, routers, fusion and everything else, exactly like a built-in one.

## When to Use
- Combine sources with your own logic (vector AND keyword, fallbacks, business rules).
- Enforce tenant filters in one unavoidable place.
- Wrap an external search system (Elasticsearch, a SQL database, an internal API).
- Add routing, boosting, or recency weighting.

## When Not to Use
Do not write one when filters or a postprocessor already do the job. Less custom code means less to maintain. (LlamaIndex.TS has no `QueryFusionRetriever`, so multi-retriever fusion is hand-rolled; see [hybrid-retrieval.md](hybrid-retrieval.md) and [query-fusion-retriever.md](query-fusion-retriever.md).)

## Prerequisites
- [retrieval-basics.md](retrieval-basics.md)
- [vector-retriever.md](vector-retriever.md)

> **Accuracy note.** `BaseRetriever` is the documented extension point in LlamaIndex.TS, but its exact abstract method signature and import path vary by version. The code below was NOT compiled or run. Everything marked "verify" must be checked against your installed version.

## The Interface
```ts
import { BaseRetriever } from "@llamaindex/core/retriever";            // verify path
import { TextNode } from "@llamaindex/core/schema";                     // verify path
import type { NodeWithScore, QueryBundle } from "@llamaindex/core";   // verify path

class MyRetriever extends BaseRetriever {
  constructor(/* ... */) {
    super();
    // ...
  }

  // verify: abstract method name and parameter type in your version
  async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
    const query = typeof params === "string" ? params : params.query;
    // ...
    return [{ node, score }];
  }
}
```
Callers use `.retrieve({ query })`, which wraps `_retrieve` with tracing and callbacks. In TypeScript everything is already `async`, so there is no separate sync and async pair. (Verify constructor details such as callback manager arguments on your version.)

### If subclassing does not fit your version
Everything that consumes a retriever only needs the `retrieve` contract. A plain class is a valid fallback for your own code (it will **not** type-check where a `BaseRetriever` is required, for example in `new RetrieverQueryEngine({ retriever })`; verify whether that constructor accepts a structural type):
```ts
interface SimpleRetriever {
  retrieve(params: { query: string }): Promise<NodeWithScore[]>;
}

class MyPlainRetriever implements SimpleRetriever {
  async retrieve({ query }: { query: string }): Promise<NodeWithScore[]> {
    // ...
    return [];
  }
}
```
The examples below use the `BaseRetriever` form.

## Example 1: AND / OR of Vector and Keyword
```ts
class HybridAndOrRetriever extends BaseRetriever {
  constructor(
    private vector: BaseRetriever,
    private keyword: BaseRetriever,
    private mode: "AND" | "OR" = "OR",
  ) {
    super();
  }

  async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
    const query = typeof params === "string" ? params : params.query;
    const [v, k] = await Promise.all([
      this.vector.retrieve({ query }),
      this.keyword.retrieve({ query }),
    ]);
    const vIds = new Set(v.map((n) => n.node.id_));
    const kIds = new Set(k.map((n) => n.node.id_));

    const keep =
      this.mode === "AND"
        ? [...vIds].filter((id) => kIds.has(id))
        : [...new Set([...vIds, ...kIds])];

    const merged = new Map<string, NodeWithScore>();
    for (const n of [...v, ...k]) merged.set(n.node.id_, n);   // later copy wins, as in a dict merge
    return keep.map((id) => merged.get(id)!);
  }
}
```
AND favors precision (both retrievers agree); OR favors recall.

## Example 2: Enforced Tenant Filter
```ts
import type { VectorStoreIndex } from "llamaindex";

class TenantRetriever extends BaseRetriever {
  private retriever: BaseRetriever;

  constructor(index: VectorStoreIndex, tenantId: string, topK = 8) {
    super();
    this.retriever = index.asRetriever({
      similarityTopK: topK,
      filters: { filters: [{ key: "tenant_id", value: tenantId, operator: "==" }] },   // MetadataFilters shape; verify
    });
  }

  async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
    const query = typeof params === "string" ? params : params.query;
    return this.retriever.retrieve({ query });
  }
}
```
The tenant comes from authenticated server-side context, and callers cannot bypass the filter. For stronger isolation use a namespace per tenant (`04-vector-databases/partitioning-and-filtering.md`); with `@llamaindex/pinecone` the namespace is fixed on the `PineconeVectorStore` object, so build one store and index per tenant ([vector-retriever.md](vector-retriever.md)).

## Example 3: Fallback When Results Are Weak
```ts
class FallbackRetriever extends BaseRetriever {
  constructor(
    private primary: BaseRetriever,
    private fallback: BaseRetriever,
    private minScore = 0.4,
  ) {
    super();
  }

  async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
    const query = typeof params === "string" ? params : params.query;
    const nodes = await this.primary.retrieve({ query });
    if (nodes.length === 0 || (nodes[0].score ?? 0) < this.minScore) {
      return this.fallback.retrieve({ query });
    }
    return nodes;
  }
}
```

## Example 4: Recency Boost
```ts
class RecencyRetriever extends BaseRetriever {
  constructor(private base: BaseRetriever, private weight = 0.1) {
    super();
  }

  async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
    const query = typeof params === "string" ? params : params.query;
    const nodes = await this.base.retrieve({ query });
    const now = new Date().getUTCFullYear();
    for (const n of nodes) {
      const year = Number(n.node.metadata["year"] ?? now);
      n.score = (n.score ?? 0) + this.weight * (1 - Math.min(now - year, 10) / 10);
    }
    return nodes.sort((a, b) => (b.score ?? 0) - (a.score ?? 0));
  }
}
```
Score arithmetic like this only makes sense when the base scores are on a known scale; validate against an evaluation set.

## Wrapping an External System
Convert each hit into a `TextNode` and wrap it:
```ts
import { TextNode } from "@llamaindex/core/schema";   // verify path

async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
  const query = typeof params === "string" ? params : params.query;
  const hits = await this.client.search(query, { size: 8 });         // your system
  return hits.map((h: { id: string; text: string; url: string; score: number }) => ({
    node: new TextNode({ id_: h.id, text: h.text, metadata: { source: h.url } }),   // stable id: see Common Mistakes
    score: h.score,
  }));
}
```

## Testing
```ts
import { test } from "node:test";
import assert from "node:assert/strict";

test("tenant isolation", async () => {
  const r = new TenantRetriever(index, "acme");
  const nodes = await r.retrieve({ query: "anything" });
  assert.ok(nodes.length > 0);                                       // an empty result would pass vacuously
  assert.ok(nodes.every((n) => n.node.metadata["tenant_id"] === "acme"));
});
```
Retrievers are plain classes, so they are easy to unit test with a small fixture index or with stub retrievers that return fixed `NodeWithScore` arrays. (These tests were not run.)

## Important Rules
- Return `NodeWithScore` objects; preserve node IDs and metadata.
- Keep retrievers deterministic and side-effect free.
- Run independent sub-retrievals concurrently (`Promise.all`) so fusion and workflows stay fast.
- Enforce security rules inside the retriever, not in the caller.

## Common Mistakes
- Re-implementing what a filter or postprocessor already does.
- Creating new nodes without stable IDs, breaking citations and deduplication.
- Mixing score scales when combining sources.
- Skipping tests for isolation and fallback logic.
- Forgetting `await` on a sub-retriever call and returning a `Promise` where an array is expected.

## Related / Next
- [query-fusion-retriever.md](query-fusion-retriever.md)
- `16-production/security.md`
- `15-debugging/debugging-rag.md`
