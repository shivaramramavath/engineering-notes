# Auto-Retriever (LLM-Inferred Metadata Filters)

> **Not available in LlamaIndex.TS - this file shows a hand-rolled TypeScript implementation.** The Python framework has `VectorIndexAutoRetriever`; the LlamaIndex.TS documentation read on 2026-10-10 does not. The implementation below asks the LLM for JSON, validates it with `zod`, and passes the result to `index.asRetriever({ filters })`.

## Concept
Users ask in natural language: *"What did the 2024 HR policy say about remote work?"* That sentence contains a **semantic part** ("remote work") and **filter parts** (`year = 2024`, `department = HR`). The **auto-retriever** uses an LLM to split the question into a query string and metadata filters, then runs a filtered vector search.

```
"2024 HR policy on remote work"
        │ LLM
        ▼
query: "remote work policy"      filters: {year: 2024, department: "HR"}
        │
        ▼ filtered vector search
```

## When to Use
- Your documents carry rich, well-defined metadata (year, author, product, category).
- Users naturally mention those attributes in questions.
- Pure semantic search keeps returning the right topic from the wrong year or category.

## When Not to Use
- Little metadata, or users never reference it.
- Strict access control: **never** rely on LLM-inferred filters for security (see below).
- Latency or cost is tight; this adds an LLM call to every query.

## Prerequisites
- `03-data-ingestion/metadata.md`
- [retrieval-basics.md](retrieval-basics.md)
- A vector store that supports metadata filtering.

## Implementation
```ts
import { z } from "zod";
import { Settings, MetadataFilters } from "llamaindex";
import { BaseRetriever } from "@llamaindex/core/retriever"; // verify path
import type { NodeWithScore, QueryBundle } from "@llamaindex/core"; // verify path

// 1. The schema is the allow-list: only these fields and operators are ever used.
const fieldSpecs = {
  year: { type: "int", description: "Year the policy was published", operators: ["==", ">=", "<="] },
  department: {
    type: "str",
    description: "Owning department. One of: HR, Finance, Engineering",
    operators: ["=="],
    allowed: ["HR", "Finance", "Engineering"],
  },
} as const;

const inferredSchema = z.object({
  query: z.string().min(1),
  filters: z
    .array(
      z.object({
        key: z.enum(["year", "department"]),
        operator: z.enum(["==", ">=", "<="]),
        value: z.union([z.string(), z.number()]),
      }),
    )
    .max(4),
});
type Inferred = z.infer<typeof inferredSchema>;

function buildPrompt(question: string): string {
  const fields = Object.entries(fieldSpecs)
    .map(([name, s]) => `- ${name} (${s.type}): ${s.description}. Operators: ${s.operators.join(", ")}`)
    .join("\n");
  return (
    `Split the user question into a semantic search query and metadata filters.\n` +
    `Content: Company HR policy documents.\nAvailable metadata fields:\n${fields}\n\n` +
    `Return ONLY JSON: {"query": string, "filters": [{"key": string, "operator": string, "value": string|number}]}.\n` +
    `Use an empty filters array when the question names no filterable attribute. Never invent fields.\n\n` +
    `Question: ${question}\nJSON:`
  );
}

// 2. Validate against the schema AND the allow-list; drop anything suspicious.
function sanitize(raw: Inferred, original: string): Inferred {
  const filters = raw.filters.filter((f) => {
    const spec = fieldSpecs[f.key];
    if (!(spec.operators as readonly string[]).includes(f.operator)) return false;
    if (spec.type === "int") return typeof f.value === "number" && Number.isInteger(f.value);
    return typeof f.value === "string" && "allowed" in spec && (spec.allowed as readonly string[]).includes(f.value);
  });
  return { query: raw.query || original, filters };
}

async function inferQueryAndFilters(question: string): Promise<Inferred> {
  try {
    const res = await Settings.llm.complete({ prompt: buildPrompt(question) }); // verify: complete({ prompt }) -> { text }
    const json = res.text.slice(res.text.indexOf("{"), res.text.lastIndexOf("}") + 1);
    return sanitize(inferredSchema.parse(JSON.parse(json)), question);
  } catch {
    return { query: question, filters: [] }; // unparseable output: plain search
  }
}

// 3. The retriever: infer, filter, search, fall back when empty.
class AutoRetriever extends BaseRetriever {
  constructor(
    private index: { asRetriever(opts: Record<string, unknown>): BaseRetriever }, // VectorStoreIndex
    private topK = 5,
    private verbose = false,
  ) {
    super();
  }

  async _retrieve(query: QueryBundle): Promise<NodeWithScore[]> {
    const { query: q, filters } = await inferQueryAndFilters(query.query); // verify: QueryBundle field name
    if (this.verbose) console.log("inferred:", q, filters);

    if (filters.length > 0) {
      const retriever = this.index.asRetriever({
        similarityTopK: this.topK,
        filters: new MetadataFilters({ filters: filters.map((f) => ({ key: f.key, value: f.value, operator: f.operator })) }),
      }); // verify operator strings and filter shape for your version
      const nodes = await retriever.retrieve({ query: q });
      if (nodes.length > 0) return nodes;
    }
    // No filters, or the filtered search came back empty: fall back to unfiltered search.
    return this.index.asRetriever({ similarityTopK: this.topK }).retrieve({ query: q });
  }
}

const autoRetriever = new AutoRetriever(index, 5, true);
const nodes = await autoRetriever.retrieve({ query: "2024 HR policy on remote work" });
```
In production, type `index` as `VectorStoreIndex` instead of the structural type above.

## The Schema Is the Prompt
The LLM only knows what your field list tells it. Quality depends on:
- **Accurate field names** that match stored metadata exactly.
- **Types** (`int`, `str`, `float`).
- **Clear descriptions**, including the allowed values for categorical fields.

Vague descriptions produce wrong or invented filters. The `zod` schema plus the allow-list check is what keeps invented fields and operators out of the vector store query.

## Failure Modes

| Problem | Effect | Mitigation |
|---|---|---|
| LLM infers a filter the user did not intend | Relevant documents excluded | Log inferred output; test on real queries |
| Field values spelled differently from stored values | Zero results | Document exact allowed values; normalize metadata at ingestion |
| Wrong type (string versus number) | Filter matches nothing | Consistent metadata types; the sanitizer checks types |
| Over-filtering | Empty or tiny result sets | Fall back to unfiltered search when results are empty (built into the class above) |
| Malformed JSON | Exception | `try/catch` returns an unfiltered search |

## Security Warning
LLM-inferred filters are **not** an access-control mechanism. A user could phrase a question so the model produces no filter, or a different filter, and see data they should not (including through prompt injection such as "ignore the filters"). Enforce tenant and permission boundaries with **namespaces and server-side filters applied outside the LLM** (`04-vector-databases/partitioning-and-filtering.md`, `16-production/security.md`). Use auto-retrieval only for convenience filters inside data the user is already allowed to see. Note that the fallback above widens the search, so it must only ever run on top of an already-scoped index or namespace.

## Testing
Create a table of questions with the expected query and filters and compare against `inferQueryAndFilters`. This is a good unit-test target (`15-debugging/debugging-rag.md`). Mock `Settings.llm` to test the sanitizer deterministically.

## Important Rules
- Describe metadata fields precisely, including allowed values.
- Validate LLM output with a schema and an allow-list before it reaches the store.
- Always handle empty results with a fallback.
- Never use inferred filters for authorization.
- Keep metadata types and spellings consistent.

## Common Mistakes
- Underspecified field descriptions.
- Passing the raw LLM JSON straight into `asRetriever({ filters })`.
- No fallback, so a wrong filter yields "I don't know".
- Treating the inferred filter as a security boundary.
- Metadata values that differ in case or format from what the LLM produces.

## Related / Next
- [router-retriever.md](router-retriever.md)
- `04-vector-databases/partitioning-and-filtering.md`
- `16-production/security.md`
