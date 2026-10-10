# Property Graph Retriever

> **Not available in LlamaIndex.TS.** The Python framework has `PropertyGraphIndex`, path extractors, graph stores and graph sub-retrievers. The LlamaIndex.TS documentation read on 2026-10-10 documents none of them, and this repo does not invent equivalents. The Python names are listed below only so you recognize them in tutorials. A small hand-rolled TypeScript sketch (unverified, not compiled or run) follows, along with the realistic alternatives.

## Concept
A **property graph** stores *entities* (nodes) and *relationships* (edges) extracted from text, each with properties. Graph retrieval answers questions about **how things connect**, which similarity search over isolated chunks handles poorly.

```
(Acme Corp) -[SUPPLIES]-> (Widget X) -[USED_IN]-> (Product Y) -[RECALLED_IN]-> (2023)
Question: "Which products were affected by Acme's supply problems?"
```

## When to Use
- Questions about relationships, dependencies, chains ("who reports to whom", "which suppliers affect product X").
- Multi-hop reasoning across documents.
- Domains with clear entities: org charts, supply chains, research citations, regulations.

## When Not to Use
- Questions answerable from a few passages. Vector retrieval is far cheaper.
- Ingestion budget is limited. Entity extraction runs an LLM over every chunk.
- You have not yet shown that vector (plus hybrid and reranking) fails on your questions.
- You want an off-the-shelf solution in TypeScript. There is none in LlamaIndex.TS; you build and maintain the graph layer yourself.

## Prerequisites
- `06-llamaindex/indexes.md`
- [vector-retriever.md](vector-retriever.md)
- [custom-retriever.md](custom-retriever.md)

> The Python area of LlamaIndex changes fast. Python class names below are from memory and **must be verified** against current Python docs if you rely on them. None of them exist in LlamaIndex.TS.

## What Python Provides (for orientation only)

| Python concept | What it does | In LlamaIndex.TS? |
|---|---|---|
| `PropertyGraphIndex` | Builds and queries a graph from documents | Not available |
| `SimpleLLMPathExtractor` | Free-form LLM triplet extraction | Not available |
| `SchemaLLMPathExtractor` | Extraction restricted to an entity and relation schema you define (more consistent) | Not available |
| `DynamicLLMPathExtractor` | Lets the LLM expand the schema as it goes | Not available |
| `ImplicitPathExtractor` | Adds edges from existing node relationships (no LLM) | Not available |
| `SimplePropertyGraphStore`, Neo4j store | In-memory and database-backed graph stores | Not available (use a database driver directly) |
| `LLMSynonymRetriever` | LLM generates keywords and synonyms from the query, matched to graph entities | Not available |
| `VectorContextRetriever` | Embeds the query, finds similar graph nodes, then expands their neighbors | Not available |
| `TextToCypherRetriever` | LLM writes a graph database query (Cypher); powerful and riskier | Not available |
| `CypherTemplateRetriever` | Fills fixed query templates from the question; safer and predictable | Not available |

A defined **schema** (allowed entity and relation types) greatly improves consistency; free-form extraction produces duplicates and variants ("Acme", "Acme Corp.", "ACME").

## Closest Approaches in TypeScript

| Option | Notes |
|---|---|
| Plain RAG (vector + hybrid + rerank) | Try this first; many "relational" questions are answered by good chunks |
| Hand-rolled triple store (sketch below) | Fine for learning and small corpora; you own extraction quality, entity resolution and updates |
| A graph database (for example Neo4j) via its own driver, wrapped in a custom retriever | Real graph storage and query power; you write extraction, ingestion and the retriever ([custom-retriever.md](custom-retriever.md)) |
| Run the Python framework as a separate service | Reasonable if graph retrieval is central and your team can operate two stacks |

## Hand-Rolled Sketch: In-Memory Triple Store (unverified)
This is a deliberately small illustration, not a library feature and not production code. It uses the global LLM to extract triples per chunk, keeps them in memory, and retrieves by matching query terms to entity names and expanding one hop.

```ts
import { Settings } from "llamaindex";
import { TextNode } from "@llamaindex/core/schema";                    // verify path
import { BaseRetriever } from "@llamaindex/core/retriever";            // verify path
import type { NodeWithScore, QueryBundle } from "@llamaindex/core";   // verify path

type Triple = { subject: string; relation: string; object: string; sourceNodeId: string };

const normalize = (s: string) => s.toLowerCase().replace(/[^a-z0-9 ]/g, "").trim();   // crude entity resolution

async function extractTriples(chunk: TextNode, maxPaths = 10): Promise<Triple[]> {
  const prompt =
    `Extract up to ${maxPaths} (subject, relation, object) triples from the text. ` +
    `Reply with ONLY a JSON array of objects with keys "subject", "relation", "object".\n\nText:\n${chunk.text}`;
  const res = await Settings.llm.complete({ prompt });                 // verify: complete() params and result shape
  try {
    const parsed = JSON.parse(res.text) as Array<{ subject: string; relation: string; object: string }>;
    return parsed.map((t) => ({ ...t, sourceNodeId: chunk.id_ }));
  } catch {
    return [];   // LLM returned invalid JSON; log and retry in real code
  }
}

class TripleGraphRetriever extends BaseRetriever {
  constructor(
    private triples: Triple[],
    private chunks: Map<string, TextNode>,    // sourceNodeId -> chunk (the "include text" idea)
    private topK = 8,
  ) {
    super();
  }

  async _retrieve(params: QueryBundle | string): Promise<NodeWithScore[]> {
    const query = normalize(typeof params === "string" ? params : params.query);

    // 1. Entities whose name appears in the query
    const seeds = new Set<string>();
    for (const t of this.triples) {
      for (const e of [t.subject, t.object]) {
        if (query.includes(normalize(e))) seeds.add(normalize(e));
      }
    }

    // 2. One-hop expansion: any triple touching a seed entity
    const hits = this.triples.filter(
      (t) => seeds.has(normalize(t.subject)) || seeds.has(normalize(t.object)),
    );

    // 3. Return the source chunks of those triples, scored by how many triples point at them
    const counts = new Map<string, number>();
    for (const t of hits) counts.set(t.sourceNodeId, (counts.get(t.sourceNodeId) ?? 0) + 1);

    return [...counts.entries()]
      .sort((a, b) => b[1] - a[1])
      .slice(0, this.topK)
      .filter(([id]) => this.chunks.has(id))
      .map(([id, c]) => ({ node: this.chunks.get(id)!, score: c }));
  }
}
```
Limits of the sketch: string matching only (no synonyms, no embeddings), one hop, no persistence, no deletion handling, and score is a hit count that is not comparable to cosine scores. Real use needs a schema-constrained extraction prompt, entity resolution, and a store.

Combine graph retrieval with vector retrieval over chunks for best coverage. LlamaIndex.TS has no `QueryFusionRetriever`; merge by rank with the hand-rolled `reciprocalRankFusion` helper from [hybrid-retrieval.md](hybrid-retrieval.md):
```ts
const [graphHits, vectorHits] = await Promise.all([
  graphRetriever.retrieve({ query }),
  vectorRetriever.retrieve({ query }),
]);
const mixed = reciprocalRankFusion([graphHits, vectorHits]).slice(0, 8);
```

## Using a Graph Database Directly (unverified)
If you move to a real graph database, call it through its own driver inside a custom retriever. With Neo4j the official `neo4j-driver` package is the usual route (`npm i neo4j-driver`; verify current API):
```ts
import neo4j from "neo4j-driver";

const driver = neo4j.driver(process.env.NEO4J_URI!, neo4j.auth.basic(process.env.NEO4J_READONLY_USER!, process.env.NEO4J_READONLY_PASSWORD!));
const session = driver.session({ defaultAccessMode: neo4j.session.READ });   // read access mode; also use a read-only DB user

// Fixed template, parameters only: the LLM never writes the query text
const result = await session.run(
  "MATCH (s:Supplier {name: $name})-[:SUPPLIES]->(:Part)-[:USED_IN]->(p:Product) RETURN DISTINCT p.name AS product LIMIT 25",
  { name: extractedSupplierName },
);
await session.close();
```
Entity names pulled from the question by an LLM are still untrusted input: pass them as query **parameters**, never concatenate them into Cypher.

## Safety: Text-to-Cypher
Letting an LLM write database queries executed against your graph can read more than intended or run costly queries. Use a **read-only database user** (a driver-level read access mode is not a substitute for database permissions), restrict the schema, and prefer template-based retrieval where possible. Never expose it with a privileged account. Since LlamaIndex.TS has no `TextToCypherRetriever`, any text-to-Cypher code in a TypeScript project is code you wrote, so it needs your own query validation, timeouts and result limits.

## Cost and Maintenance
- Extraction cost scales with corpus size (LLM call per chunk).
- Re-extract when documents change; remove graph facts for deleted documents (`11-document-management/`).
- Entity resolution (merging duplicates) is an ongoing quality issue.

## Evaluate Before Committing
1. Collect 20+ real relational questions.
2. Measure answer quality with vector + hybrid + rerank.
3. Build the graph on a small subset and compare.
Adopt the graph only if the gain justifies the cost, which in TypeScript includes the cost of building the layer yourself.

## Important Rules
- Define an entity and relation schema for consistent graphs.
- Pair graph retrieval with chunk-text retrieval.
- Use read-only credentials for any query-generating retriever.
- Prove the need on a subset before indexing the whole corpus.
- Do not look for `PropertyGraphIndex` in LlamaIndex.TS; it is not documented there.

## Common Mistakes
- Building a graph for questions plain RAG already answers.
- Free-form extraction with no schema, yielding messy duplicates.
- No plan for updates and deletions.
- Running LLM-generated database queries with a privileged account.
- Copying Python `PropertyGraphIndex` snippets into a TypeScript project.

## Related / Next
- [router-retriever.md](router-retriever.md)
- [custom-retriever.md](custom-retriever.md)
- `13-workflows/advanced-rag.md`
- `16-production/security.md`
