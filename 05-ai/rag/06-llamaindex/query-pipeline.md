# Query Pipelines and Workflows

## Concept
Sometimes the standard retrieve-then-synthesize flow is not enough: you need branching, loops, parallel steps, or several models chained together. The Python framework has offered two tools for custom flows:

- **Query pipelines** (`QueryPipeline`): declare a graph of components and links. **Not available in LlamaIndex.TS** (see below).
- **Workflows**: event-driven, async steps. LlamaIndex.TS provides these in the separate package `@llamaindex/workflow`. Python has been moving toward Workflows as the recommended approach too.

> **Verify before choosing.** `QueryPipeline` was being deprecated in Python in favor of Workflows and has no documented TypeScript counterpart. Check the current docs and release notes for your version. In TS, use Workflows or plain TypeScript. The Workflow API has changed between releases; the code below is unverified and not compiled.

## Prerequisites
- [query-engine.md](query-engine.md)
- [response-synthesizer.md](response-synthesizer.md)

## First, Do You Need This?
Most RAG apps do not. Use the simpler tools first:

| Need | Simplest tool |
|---|---|
| Retrieve, rerank, answer | `RetrieverQueryEngine` with postprocessors |
| Rewrite the query first | A small wrapper function (`TransformQueryEngine` is not documented for TS; see `08-query-transformation/`) |
| Pick among data sources | `RouterQueryEngine` (`07-retrievers/router-retriever.md`) |
| Split a question into parts | `SubQuestionQueryEngine` |
| Multi-turn | Chat engine (`12-chat/`) |

Reach for workflows when you need custom branching, loops (retry retrieval with a rewritten query), parallel fan-out, or human approval steps.

## Plain TypeScript Is Often Enough
```ts
async function answer(question: string): Promise<string> {
  const rewritten = await rewriteQuery(question);                         // your own function
  let nodes = await retriever.retrieve({ query: rewritten });
  if (nodes.length === 0 || (nodes[0].score ?? 0) < 0.3) {
    nodes = await fallbackRetriever.retrieve({ query: question });
  }
  const reranked = await reranker.postprocessNodes(nodes, question);      // verify postprocessor method signature
  const response = await synthesizer.synthesize({ query: question, nodes: reranked });   // verify argument shape
  return response.toString();
}
```
Explicit code is easy to read, test and debug. Add a framework only when its structure pays for itself.

## Query Pipeline (not available in LlamaIndex.TS)
Python's `QueryPipeline` (modules, `add_link`, `chain=[...]`, `InputComponent`) has no documented TS equivalent, and the Python version is itself legacy. Do not search the TS package for it. The same ideas map onto plain code:

| Python `QueryPipeline` idea | TypeScript equivalent |
|---|---|
| Modules (retriever, prompt, LLM, synthesizer) | Plain objects you already have |
| Links between modules | Function calls passing outputs as inputs |
| Chain `[a, b, c]` | `await c(await b(await a(x)))` |
| Parallel branches | `Promise.all([...])` |
| Verbose tracing | Log each step's output (see Debugging) |

A tiny hand-rolled step runner, if you want a declared chain (unverified, not compiled):
```ts
type Step<I, O> = (input: I) => Promise<O>;

async function runChain(steps: Step<any, any>[], input: unknown, verbose = false) {
  let value = input;
  for (const [i, step] of steps.entries()) {
    value = await step(value);
    if (verbose) console.log(`step ${i}:`, value);
  }
  return value;
}
```

## Workflow (current direction; verify imports)
The Workflow package is separate (`npm i @llamaindex/workflow`). The newer API defines events with `workflowEvent` and registers handlers on a workflow created by `createWorkflow`; older versions used a `Workflow` class with `addStep`. Verify which your version provides.

```ts
import { createWorkflow, workflowEvent } from "@llamaindex/workflow";   // verify package and export names
import type { NodeWithScore } from "@llamaindex/core/schema";

const startEvent = workflowEvent<{ query: string }>();
const retrievedEvent = workflowEvent<{ query: string; nodes: NodeWithScore[] }>();
const stopEvent = workflowEvent<string>();

const workflow = createWorkflow();

workflow.handle([startEvent], async (context, start) => {
  const { query } = start.data;
  const nodes = await retriever.retrieve({ query });
  return retrievedEvent.with({ query, nodes });
});

workflow.handle([retrievedEvent], async (context, ev) => {
  const response = await synthesizer.synthesize({ query: ev.data.query, nodes: ev.data.nodes });   // verify argument shape
  return stopEvent.with(response.toString());
});

// Run it (the runner API differs by version; verify)
const { stream, sendEvent } = workflow.createContext();
sendEvent(startEvent.with({ query: "What is the refund policy?" }));
for await (const ev of stream) {
  if (stopEvent.include(ev)) {
    console.log(ev.data);
    break;
  }
}
```
How it works: each handler consumes an event type and emits another; the event types define the flow. Branching, loops and parallel steps come from emitting different events. Handlers are async by design. Python's decorator syntax (`@step`) and the `timeout` constructor argument shown in Python tutorials do not apply; if you need a timeout in TS, race the run against a timer yourself (verify built-in support).

## Typical Advanced Flows
- **Corrective retrieval**: retrieve → grade relevance → if poor, rewrite the query and retrieve again.
- **Multi-source**: fan out to several retrievers in parallel, merge, rerank, answer.
- **Human in the loop**: pause for approval before an expensive or risky step.
- **Multi-step RAG**: see `13-workflows/multi-step-rag.md` for worked designs.

## Debugging
- Log the output of every step (query, nodes, scores, final prompt).
- Test each step in isolation with fixed inputs.
- Set timeouts and a maximum number of loop iterations to avoid runaway flows.

## Important Rules
- Start with plain TypeScript or a standard query engine; add structure only when needed.
- Keep steps small, pure and individually testable.
- Always bound loops and set timeouts.
- Check which Workflow API your installed version exposes before building on it, and do not look for `QueryPipeline` in TS.

## Common Mistakes
- Building a workflow for a flow that a query engine already handles.
- Unbounded retry loops that burn LLM tokens.
- Hiding failures inside steps; log and surface errors.
- Copying pipeline or workflow code from Python tutorials, or from older TS tutorials, without checking the API still exists.

## Related / Next
- `13-workflows/multi-step-rag.md`
- `13-workflows/advanced-rag.md`
- `08-query-transformation/README.md`
