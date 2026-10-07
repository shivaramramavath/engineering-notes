# Workflows

A **workflow** is an event-driven program: you define typed **events**, attach **handlers** that react to them, and each handler emits new events. The engine routes events to handlers. There is no graph DSL; control flow (branching, loops, parallel fan-out) is just "which event does this handler emit next".

Use workflows when you know the steps but need real control: retries, loops with limits, approval pauses, parallel work, or mixing LLM calls with ordinary code. An agent ([02-agents](./02-agents.md)) lets the model choose the path; a workflow lets *you* choose it.

> Prerequisites: [02-agents](./02-agents.md), [../01-core/04-query-and-chat-engines](../01-core/04-query-and-chat-engines.md).

> **Version note:** Workflows moved into their own package, `@llamaindex/workflow-core`, and `@llamaindex/workflow` re-exports it for the older import paths. Handler signatures differ between docs pages. Newer ones pass the context first, `handle([event], (context, event) => ...)`, while the framework tutorial still shows `handle([event], (event) => ...)` and reads state via `getContext()`. This note uses the context-first form. Check your installed version's types.

```bash
npm i @llamaindex/workflow-core
# or, if you also want the agent helpers:
npm i @llamaindex/workflow
```

## Core ideas

| Piece | Role |
|---|---|
| `workflowEvent<T>()` | Defines an event type carrying data of type `T` |
| `workflow.handle([events], handler)` | Registers a handler that runs when the listed event(s) occur |
| `event.with(data)` | Creates an event instance to emit or return |
| `workflow.createContext()` | Starts one run; returns `{ sendEvent, stream, ... }` |
| `stream` | Async stream of every event in that run; has `until`, `filter`, `toArray` |

A handler can **return** an event (emitting it), or call `sendEvent(...)` to emit one or several.

## A minimal RAG workflow

```ts
import { createWorkflow, workflowEvent } from "@llamaindex/workflow-core";

const startEvent = workflowEvent<string>();                       // the question
const retrievedEvent = workflowEvent<{ question: string; context: string }>();
const answerEvent = workflowEvent<string>();                      // final result

const workflow = createWorkflow();

workflow.handle([startEvent], async (_ctx, start) => {
  const hits = await retriever.retrieve({ query: start.data });
  const context = hits.map((h) => h.node.getContent(MetadataMode.NONE)).join("\n---\n");
  return retrievedEvent.with({ question: start.data, context });
});

workflow.handle([retrievedEvent], async (_ctx, ev) => {
  const res = await Settings.llm.complete({
    prompt: `Answer using only this context.\n\nContext:\n${ev.data.context}\n\nQuestion: ${ev.data.question}`,
  });
  return answerEvent.with(res.text);
});

// run it
const { stream, sendEvent } = workflow.createContext();
sendEvent(startEvent.with("What is the refund window?"));

const events = await stream.until(answerEvent).toArray();
console.log(events.at(-1)!.data);
```

How it runs: you send `startEvent`; the first handler retrieves and returns `retrievedEvent`; the second handler synthesizes and returns `answerEvent`; `until(answerEvent)` ends the stream when that event appears, and the last event is the result.

You can also consume events as they happen:

```ts
for await (const event of stream) {
  if (answerEvent.include(event)) { /* done */ break; }
}
```

`include(event)` is the type guard that narrows an event to a specific type.

## Why bother when you could write `async` functions?

For a linear pipeline, plain functions are simpler. Workflows pay off when you need:

- **Loops and retries** where the path depends on LLM output.
- **Fan-out / fan-in** across many items with shared accounting.
- **Pausing** for human input and resuming later (even in another request).
- **Observability**: every step is an event you can log, trace and replay.
- **Composing** agents and RAG steps behind one entry point.

If none of these apply, don't use a workflow.

## Branching

Branching is choosing which event to emit. Add one handler per branch:

```ts
const questionEvent = workflowEvent<string>();
const simpleEvent = workflowEvent<string>();
const complexEvent = workflowEvent<string>();
const resultEvent = workflowEvent<string>();

workflow.handle([questionEvent], async (_ctx, ev) => {
  const verdict = await Settings.llm.complete({
    prompt: `Reply with only SIMPLE or COMPLEX. Is this question a single fact lookup?\n${ev.data}`,
  });
  return verdict.text.includes("SIMPLE")
    ? simpleEvent.with(ev.data)
    : complexEvent.with(ev.data);
});

workflow.handle([simpleEvent], async (_ctx, ev) => {
  const r = await queryEngine.query({ query: ev.data });
  return resultEvent.with(r.toString());
});

workflow.handle([complexEvent], async (_ctx, ev) => {
  const r = await subQuestionEngine.query({ query: ev.data });
  return resultEvent.with(r.toString());
});
```

This is the same idea as `RouterQueryEngine`, but you control the rule: it can be an LLM call, a regex, a user setting, or a database lookup. Prefer a deterministic check when one exists.

## Loops (with a cap)

A loop is a handler that emits an event another handler (or itself) will process again. **Always carry a counter**. The framework tutorial's joke example uses shared state for this:

```ts
import { createWorkflow, workflowEvent } from "@llamaindex/workflow-core";
import { createStatefulMiddleware } from "@llamaindex/workflow-core/middleware/state";

const startEvent = workflowEvent<string>();
const draftEvent = workflowEvent<{ text: string }>();
const critiqueEvent = workflowEvent<{ text: string; critique: string }>();
const resultEvent = workflowEvent<{ text: string }>();

const { withState } = createStatefulMiddleware(() => ({
  iterations: 0,
  maxIterations: 3,
}));
const flow = withState(createWorkflow());

flow.handle([startEvent], async (_ctx, ev) => {
  const r = await Settings.llm.complete({ prompt: `Write a short product blurb for: ${ev.data}` });
  return draftEvent.with({ text: r.text });
});

flow.handle([draftEvent], async (_ctx, ev) => {
  const r = await Settings.llm.complete({
    prompt: `Critique this blurb. If it needs improvement, include the word IMPROVE.\n\n${ev.data.text}`,
  });
  return r.text.includes("IMPROVE")
    ? critiqueEvent.with({ text: ev.data.text, critique: r.text })
    : resultEvent.with({ text: ev.data.text });
});

flow.handle([critiqueEvent], async ({ state }, ev) => {
  state.iterations++;
  const r = await Settings.llm.complete({
    prompt: `Rewrite the blurb using this critique.\n\nBlurb: ${ev.data.text}\n\nCritique: ${ev.data.critique}`,
  });
  return state.iterations < state.maxIterations
    ? draftEvent.with({ text: r.text })     // loop back
    : resultEvent.with({ text: r.text });   // stop at the cap
});
```

State from `withState` is shared across handlers of one run. In the older tutorial style you read it with `getContext().state` instead of destructuring from the context argument.

Never let an LLM judgement ("is it good enough?") be the only exit. The counter guarantees termination and bounds cost.

## Fan-out and fan-in

To process items in parallel, emit one event per item from one handler and aggregate in another. The pattern from the docs: a start handler calls `sendEvent` once per item; a worker handler processes each item and emits a result event; a collecting handler counts results in state and emits a completion event when the count matches.

```ts
const itemEvent = workflowEvent<number>();
const itemDoneEvent = workflowEvent<string>();
const allDoneEvent = workflowEvent<string[]>();

const { withState } = createStatefulMiddleware(() => ({ total: 0, results: [] as string[] }));
const flow = withState(createWorkflow());

flow.handle([startEvent], async ({ sendEvent, state }, ev) => {
  const items = [1, 2, 3, 4];
  state.total = items.length;
  state.results = [];
  for (const n of items) sendEvent(itemEvent.with(n));
});

flow.handle([itemEvent], async (_ctx, ev) => {
  const out = await processOne(ev.data);        // runs concurrently across items
  return itemDoneEvent.with(out);
});

flow.handle([itemDoneEvent], async ({ state }, ev) => {
  state.results.push(ev.data);
  if (state.results.length === state.total) {
    return allDoneEvent.with(state.results);
  }
});
```

Things to watch:

- Handlers for different items run concurrently, so keep shared-state updates simple and don't depend on arrival order.
- Make `total` part of state per run; a module-level counter would be shared between runs.
- Add a concurrency limit if each item calls an LLM or rate-limited API.

## Multi-agent workflows

For agents that hand off to each other you don't need to wire events by hand. `@llamaindex/workflow` provides `multiAgent`:

```ts
import { agent, multiAgent } from "@llamaindex/workflow";
import { openai } from "@llamaindex/openai";

const writerAgent = agent({
  name: "WriterAgent",
  description: "Writes the final answer for the user from research notes",
  tools: [],
  llm: openai({ model: "gpt-4o-mini" }),
});

const researchAgent = agent({
  name: "ResearchAgent",
  description: "Finds facts in the knowledge base",
  tools: [kbTool],
  llm: openai({ model: "gpt-4o-mini" }),
  canHandoffTo: [writerAgent], // may delegate to the writer
});

const team = multiAgent({
  agents: [researchAgent, writerAgent],
  rootAgent: researchAgent, // the run starts here
});

const result = await team.run("Summarize our remote-work policy for new hires");
console.log(result.data.result);
```

Facts from the docs: each agent needs a unique `name` and a `description` (the description is used for task routing), `tools`, and optionally `canHandoffTo` (agent names or instances it may delegate to). `rootAgent` is where the run starts. Because `canHandoffTo` takes agent instances, define the agents that are handed *to* first. For two-way handoffs, pass agent names instead of instances and check your version's types.

Multi-agent systems are still agents: each handoff is more LLM calls and more places to go wrong. Start with a single agent and several tools, and only split into agents when prompts or tool sets become too large for one.

For a lower-level alternative, a workflow where each handler calls one specialised agent or query engine gives you explicit, testable control over routing.

## Human in the loop

A workflow can pause for input and resume later, even in a different request. The documented pattern uses `snapshot` and `resume`:

```ts
const { sendEvent, snapshot, stream } = workflow.createContext();
sendEvent(startEvent.with("begin"));

// workflow emits humanRequestEvent when it needs approval
await stream.until(humanRequestEvent).toArray();
const saved = await snapshot();          // persist this (DB, Redis) and return to the user

// ... later, in another request, after the human responds:
const resumed = workflow.resume(saved);
resumed.sendEvent(humanResponseEvent.with("approved"));
const events = await resumed.stream.until(stopEvent).toArray();
```

This is the right shape for "agent proposes a refund; a person approves it". The snapshot is data you must store and protect (it may contain user content), and you decide how long it stays valid.

## Observability and cancellation

- **Tracing:** wrap the workflow with the trace middleware to emit per-handler trace data, optionally to OpenTelemetry:

  ```ts
  import { withTraceEvents } from "@llamaindex/workflow-core/middleware/trace-events";
  const workflow = withTraceEvents(createWorkflow());
  ```

  Details in `../04-production/01-tracing-and-debugging.md`.
- **Cancellation:** the context exposes an `AbortSignal` (`signal`). Check `signal.aborted` in long handlers and pass it to `fetch` or SDK calls so a cancelled request stops spending money.
- **Logging events:** during development, iterate the stream and `console.log` every event; the sequence tells you exactly where a run diverged.

## Serving

A run is just an async stream, so it maps naturally onto server-sent events or a streamed HTTP response. `@llamaindex/workflow-core` also ships helpers for server frameworks such as Hono. See `../04-production/05-serving-and-integration.md`.

## Common mistakes

**No handler for an emitted event.** The run silently stalls and `until(resultEvent)` never resolves. Every event type that can be emitted needs a handler, or must be the one you wait for.

**Loops with no hard cap.** Use a counter in state, not just an LLM verdict.

**Module-level mutable state** shared by concurrent runs. Keep per-run data in workflow state.

**Awaiting the wrong event.** `until(x)` waits for event type `x`; if a branch emits a different terminal event, you hang. Use one shared result event or `until` the right one per branch.

**Forgetting timeouts.** Put a timeout around runs (or check `signal`) so one stuck LLM call doesn't hold a request forever.

**Calling `getContext()` from the wrong place.** In the older style, call it directly in the handler body, not from detached callbacks; the library docs mark some usage as unsupported.

**Using workflows for linear pipelines.** Plain async functions are simpler.

## Debugging

1. Log the full event stream for a failing run. What was the last event, and was there a handler for it?
2. Add a catch-all logger on the stream while developing.
3. If a run hangs, list all event types your handlers emit and check each has a consumer.
4. For loops, log the counter and exit condition on every pass.
5. Use the trace middleware to see per-handler timing and errors.

## Quick Summary

- Workflows are typed events plus handlers; handlers return or `sendEvent` new events.
- Run with `createContext()`, `sendEvent(startEvent.with(...))`, and read the `stream` (`until`, `filter`, `toArray`, or `for await`).
- Branching = emit different events; loops = re-emit with a counter in `withState` state; fan-out = `sendEvent` per item plus an aggregating handler.
- `multiAgent` composes agents with `canHandoffTo`; descriptions drive routing.
- `snapshot` / `resume` give human-in-the-loop pauses across requests.
- Every emitted event needs a handler, every loop needs a cap, and every run needs a timeout or cancellation path.
- Prefer workflows over agents when you know the steps; prefer plain functions when the flow is linear.

## Next

`../04-production/01-tracing-and-debugging.md`: seeing what your retrieval, agents and workflows actually did in production.