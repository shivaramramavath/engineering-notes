# Tools

A **tool** is a function an LLM can ask your code to run. The model never executes anything itself. It sees a list of tool names, descriptions and argument schemas, decides one is useful, and emits a structured call (`{ name, arguments }`). Your code runs it and feeds the result back. Tools are how an agent searches your index, calls an API, or does arithmetic.

> Prerequisites: [../01-core/04-query-and-chat-engines](../01-core/04-query-and-chat-engines.md). Used by [02-agents](./02-agents.md).

```bash
npm i @llamaindex/workflow @llamaindex/openai llamaindex zod
```

## What the model actually sees

```text
name:        search_policies
description: Answers questions about HR policy documents (vacation, expenses, remote work).
parameters:  { query: string }
```

That is all. The model picks tools and writes arguments based **only** on the name, description and parameter schema. Tool quality is mostly description quality.

## Function tools: `tool()`

`tool` is exported from `llamaindex`, with a Zod schema for the arguments:

```ts
import { tool } from "llamaindex";
import { z } from "zod";

const weatherTool = tool({
  name: "get_weather",
  description: "Get the current weather for a city",
  parameters: z.object({
    city: z.string().describe("City name, e.g. 'Paris'"),
  }),
  execute: async ({ city }) => {
    const res = await fetch(`https://api.example.com/weather?city=${encodeURIComponent(city)}`);
    return await res.text();
  },
});
```

There is also a shorthand for tools with no arguments: `tool(() => "Baby Llama is called cria", { name: "joke", description: "Use this tool to get a joke" })`.

Guidelines:

- `.describe(...)` on each field. The argument descriptions are part of the prompt.
- Return **strings or small JSON**, not megabytes. Whatever you return goes back into the model's context.
- Validate and clamp arguments inside `execute` anyway (the schema helps, but treat model output as untrusted input).
- Throw or return a clear error message on failure. The model can often recover from "city not found, try a different spelling" but not from a silent `undefined`.

## Query-engine tools

To let an agent search your data, wrap a query engine as a tool. The shortcut on an index:

```ts
const kbTool = index.queryTool({
  metadata: {
    name: "hr_policies",
    description:
      "Answers questions about company HR policy: vacation, expenses, remote work. " +
      "Pass a self-contained question.",
  },
  options: { similarityTopK: 8 },
});
```

Or explicitly, from any query engine (including routers, sub-question engines, or one with filters and rerankers):

```ts
import { QueryEngineTool } from "llamaindex";

const kbTool = new QueryEngineTool({
  queryEngine: index.asQueryEngine({ similarityTopK: 8 }),
  metadata: {
    name: "hr_policies",
    description: "Answers questions about company HR policy...",
  },
});
```

Notes:

- The docs remind you that the default `similarityTopK` is low (2). Set it deliberately; see [../02-rag/03-retrievers-and-postprocessors](../02-rag/03-retrievers-and-postprocessors.md).
- Each tool call runs the **whole** query engine, including its own LLM call to synthesize an answer. An agent that calls it three times pays for three retrievals plus three syntheses plus its own reasoning calls.
- Because the tool returns a *synthesized* answer, the agent loses the raw chunks and can't cite page numbers unless your engine puts them in the answer.

### Retrieval tool instead

If you'd rather the agent's own LLM read the raw chunks (cheaper, and gives you control over what it sees), expose retrieval as a function tool:

```ts
const retriever = index.asRetriever({ similarityTopK: 6 });

const searchDocs = tool({
  name: "search_docs",
  description: "Search the knowledge base and return the most relevant passages with sources.",
  parameters: z.object({ query: z.string().describe("A self-contained search query") }),
  execute: async ({ query }) => {
    const hits = await retriever.retrieve({ query });
    return hits
      .map((h, i) => `[${i + 1}] (${h.node.metadata.source ?? "unknown"})\n${h.node.getContent(MetadataMode.NONE)}`)
      .join("\n\n");
  },
});
```

Rule of thumb: a **query-engine tool** is a "black box that answers", good when your RAG pipeline is already tuned. A **retrieval tool** is "give me evidence", good when the agent must compare, cite or combine sources.

## Writing descriptions that work

Compare:

| Weak | Strong |
|---|---|
| `"Search documents"` | `"Answers questions about the 2023 SF city budget (revenue, departments, capital projects). Not for other years."` |
| `"Calculator"` | `"Adds two numbers. Use for any arithmetic; do not compute by yourself."` |

- Say **what it covers and what it doesn't**. With several tools, overlapping descriptions make selection a coin flip.
- Say **what the input should look like** ("a self-contained question, not a pronoun").
- Use distinct, descriptive names (`hr_policies`, not `tool1`).

## Side effects and permissions

A tool that writes (sends email, refunds, deletes) is an action the model can trigger from text it read, including text in retrieved documents. Treat it accordingly:

- Prefer read-only tools. Gate write tools behind confirmation (see human-in-the-loop in [03-workflows](./03-workflows.md)).
- Make tools idempotent where possible.
- Scope credentials narrowly and enforce authorization **inside the tool** using the server-side user identity, never an identity the model supplies as an argument.
- See `../04-production/04-security.md`.

## Using LlamaIndex with LangChain / LangGraph

The two ecosystems interoperate at the **tool boundary**. A tool is just a name, description, schema and function, so either side can wrap the other. A common split: LlamaIndex does retrieval and indexing; LangChain/LangGraph does orchestration.

### LlamaIndex query engine as a LangChain tool

```ts
import { tool as lcTool } from "@langchain/core/tools";
import { z } from "zod";

const searchKb = lcTool(
  async ({ question }) => {
    const res = await queryEngine.query({ query: question });
    return res.toString();
  },
  {
    name: "search_kb",
    description: "Answers questions about the company knowledge base.",
    schema: z.object({ question: z.string() }),
  },
);
```

Then pass `searchKb` in the `tools` array of a LangChain or LangGraph agent. LangChain's own docs show the agent constructors: `createReactAgent` (from `@langchain/langgraph/prebuilt`) was the long-time prebuilt, and current LangChain docs point to `createAgent` (from `langchain`). Their APIs are moving, so follow the LangChain docs for the version you install rather than copying agent-construction code from here.

Inside a LangGraph graph you can also call the query engine directly from a node function; no tool is needed if the graph, not the model, decides when to retrieve.

### LangChain tool inside a LlamaIndex agent

```ts
import { tool } from "llamaindex";

const asLlamaTool = (lc: any, schema: z.ZodObject<any>) =>
  tool({
    name: lc.name,
    description: lc.description,
    parameters: schema,
    execute: async (args) => String(await lc.invoke(args)),
  });
```

Reuse the same Zod schema you gave the LangChain tool.

### Which orchestrator?

Pick one orchestrator per request path. Nesting a LangGraph agent inside a LlamaIndex agent, each with its own loop, doubles the LLM calls and makes failures hard to trace. If you already run LangGraph, keep it, and use LlamaIndex as the retrieval library behind a tool. If you're building around LlamaIndex data structures, use LlamaIndex's agent and workflow layer ([02-agents](./02-agents.md), [03-workflows](./03-workflows.md)).

## Common mistakes

**Vague or overlapping descriptions.** The most common reason an agent picks the wrong tool or never uses one.

**Returning huge payloads.** A tool that dumps a 20k-token JSON blob into context wastes money and drowns the answer.

**Using a model without tool-calling support.** Agents here depend on an LLM with function calling. Small local models often handle it poorly.

**Treating arguments as trusted.** Validate; don't build SQL or shell commands by string concatenation from them.

**Passing identity as an argument.** `userId` or `tenantId` must come from your server context, not the model.

**Wrapping a whole agent in a query-engine tool** without noticing the nested loop cost.

## Debugging

- Log every tool call: name, arguments, result size, duration (the agent event stream in [02-agents](./02-agents.md) gives you this).
- If a tool is never called, rewrite its description from the model's perspective: when would you use this?
- If it's called with bad arguments, tighten the schema and add `.describe()` hints and examples.
- Test the tool's `execute` directly, outside any agent, with the exact inputs the model produced.

## Quick Summary

- A tool = name + description + argument schema + function. The model only sees the first three.
- `tool({ name, description, parameters: z.object(...), execute })` for functions; `index.queryTool(...)` or `new QueryEngineTool(...)` for query engines.
- Query-engine tools return synthesized answers (extra LLM call); retrieval tools return evidence for the agent to reason over.
- Descriptions decide whether tools are used correctly. Be specific about scope and input format.
- Write tools are security-sensitive: least privilege, server-side identity, confirmation for risky actions.
- LangChain/LangGraph and LlamaIndex interoperate by wrapping each other's tools; keep one orchestrator per path.

## Next

[02-agents.md](./02-agents.md): the loop that decides when and how to call these tools, and when you shouldn't use one at all.