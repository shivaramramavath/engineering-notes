# Testing

Graphs are easier to test than they look, because almost everything in them is a plain function: nodes take state and return an update, routers take state and return a name. The hard part is the model, which is slow, costs money and isn't deterministic. The strategy is to keep the model out of most tests and test its *quality* separately.

Prerequisites: [Nodes and Edges](../01-core/03-nodes-and-edges.md), [Routing and Parallelism](../01-core/04-routing-and-parallelism.md).

Examples use [Vitest](https://vitest.dev); Jest works the same way.

## A layered strategy

| Layer | What it checks | Model involved? | Runs |
|---|---|---|---|
| Unit | One node, router or tool as a function | No | Every commit |
| Graph | Wiring, routing, loops, reducers, interrupts | Faked | Every commit |
| Integration | The real model and tools still work together | Yes, a few cases | Before release / nightly |
| Evals | Answer quality across a dataset | Yes | On a schedule, on prompt/model changes |

The first two layers are fast and deterministic and should carry most of your coverage.

## Design for testability: inject the model

Don't construct models inside nodes. Build the graph from a factory that receives its dependencies, so a test can pass a stub.

```ts
// support.ts
import { StateGraph, Annotation, START, END } from "@langchain/langgraph";

export const State = Annotation.Root({
  text: Annotation<string>(),
  category: Annotation<"billing" | "tech">(),
  reply: Annotation<string>(),
});

export const billing = (s: typeof State.State) => ({
  reply: `Routing "${s.text}" to billing`,
});
export const tech = (s: typeof State.State) => ({
  reply: `Routing "${s.text}" to tech support`,
});
export const routeByCategory = (s: typeof State.State) => s.category;

export function buildGraph(deps: {
  classify: (text: string) => Promise<"billing" | "tech">;
}) {
  return new StateGraph(State)
    .addNode("classify", async (s: typeof State.State) => ({
      category: await deps.classify(s.text),
    }))
    .addNode("billing", billing)
    .addNode("tech", tech)
    .addEdge(START, "classify")
    .addConditionalEdges("classify", routeByCategory, ["billing", "tech"])
    .addEdge("billing", END)
    .addEdge("tech", END)
    .compile();
}
```

In production `classify` calls a model; in tests it's an async function returning a fixed value. The same trick works for a whole `callModel` function: pass `(messages) => Promise<AIMessage>` rather than a chat model object.

## Unit tests: nodes, routers, tools

Nodes and routers are called directly with a hand-built state:

```ts
import { describe, it, expect } from "vitest";
import { billing, routeByCategory, State } from "./support";

type S = typeof State.State;
const base: S = { text: "refund please", category: "billing", reply: "" };

describe("nodes and routers", () => {
  it("billing writes a reply", () => {
    expect(billing(base).reply).toContain("billing");
  });

  it("router returns the category as the next node", () => {
    expect(routeByCategory({ ...base, category: "tech" })).toBe("tech");
  });
});
```

Tools are runnables, so you can call them with `.invoke(args)`:

```ts
const out = await getWeather.invoke({ city: "Pune" });
expect(out).toContain("Pune");
```

## Graph tests: assert the path, not just the result

Stream `updates` and record which nodes ran. That checks routing and ordering, which `invoke` alone hides.

```ts
import { buildGraph } from "./support";

it("routes billing questions to billing", async () => {
  const graph = buildGraph({ classify: async () => "billing" });

  const steps: string[] = [];
  for await (const chunk of await graph.stream(
    { text: "I was charged twice" },
    { streamMode: "updates" },
  )) {
    steps.push(...Object.keys(chunk));
  }

  expect(steps).toEqual(["classify", "billing"]);
});

it("routes everything else to tech", async () => {
  const graph = buildGraph({ classify: async () => "tech" });
  const out = await graph.invoke({ text: "app crashes" });
  expect(out.reply).toContain("tech support");
});
```

Cases worth covering at this layer:

- every branch of every router
- loop exits, including the case where the exit condition is never met (see below)
- reducers merging parallel branches
- the empty/odd inputs your nodes must tolerate

### Loops that must terminate

For graphs with cycles, lower the recursion limit and assert the runaway case fails loudly instead of hanging:

```ts
import { GraphRecursionError } from "@langchain/langgraph";

it("gives up instead of looping forever", async () => {
  const graph = buildStubbornGraph(); // a stub that never satisfies the exit condition
  await expect(
    graph.invoke({ text: "x" }, { recursionLimit: 5 }),
  ).rejects.toBeInstanceOf(GraphRecursionError);
});
```

## Testing interrupts and resumes

A graph that pauses for a human ([Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md)) needs a checkpointer and a thread id. Use a fresh thread per test:

```ts
import { randomUUID } from "node:crypto";
import { Command } from "@langchain/langgraph";
import { graph } from "./approval-graph"; // compiled with a MemorySaver

it("pauses for approval, then rejects", async () => {
  const cfg = { configurable: { thread_id: randomUUID() } };

  await graph.invoke({ topic: "Q3 launch" }, cfg);
  expect((await graph.getState(cfg)).next).toEqual(["approval"]);

  const done = await graph.invoke(new Command({ resume: "no" }), cfg);
  expect(done.status).toBe("rejected");
});
```

Make one test per resume answer. Also test an *invalid* answer if the node validates it.

## Tests that need a checkpointer

Compile with `new MemorySaver()` in tests; never share one saver or one `thread_id` between tests, or they'll see each other's state. A new `thread_id` per test is enough.

## Integration tests and evals

**Integration**: a handful of end-to-end runs against the real model and tools to catch provider or prompt breakage. Assert on *properties*, not exact text: a tool was called, the answer contains an expected fact, the output parses against its schema.

**Evals** measure quality over a dataset of inputs with expected properties:

- Keep a curated set of real and tricky inputs, with the expected route, tool use, or key facts.
- Score with deterministic checks where you can, and a model-as-judge where you can't, spot-checking the judge against human labels.
- Track scores over time; run on prompt, model and graph changes, not on every commit.
- Evaluate **components separately** (routing accuracy, retrieval hit rate, final answer) so a regression points at a step. For retrieval-heavy graphs see [Agentic RAG](../04-patterns/03-agentic-rag.md).

Don't try to make model output deterministic with a low temperature and then assert exact strings. It's brittle and breaks on every provider update.

## Common mistakes

- **Calling the real model in unit tests.** Slow, flaky, costly.
- **Asserting exact LLM text.** Assert structure and properties.
- **Sharing a checkpointer or `thread_id` across tests.** Leaks state between them.
- **Only checking the final state.** A wrong path can still land on a plausible-looking result; assert the steps.
- **Forgetting `await` before `graph.stream(...)`.** The loop then throws because a promise isn't iterable.
- **No test for the failure path.** Cover model errors, empty tool results and retry exhaustion ([Reliability](./02-reliability.md)).

## Debugging a failing test

- Print the streamed `updates` for the failing input; the first surprising chunk is usually the bug.
- Run the offending node alone with the exact state from the stream.
- Reproduce a bad production run by copying the thread's state at the step before it went wrong into a unit test ([Time Travel](../03-stateful/04-time-travel.md) helps find that step).

## Quick summary

- Nodes, routers and tools are plain functions; test them directly with hand-built state.
- Build graphs from a factory that takes the model as a dependency; stub it in tests.
- Stream `updates` to assert the path; lower `recursionLimit` to test that loops terminate.
- Test interrupts with a `MemorySaver`, a fresh `thread_id` per test, and a `Command({ resume })`.
- Keep model quality in evals, scored on properties, not exact text.

**Next:** [Reliability](./02-reliability.md)
