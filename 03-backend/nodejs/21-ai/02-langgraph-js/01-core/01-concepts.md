# LangGraph.js: Core Concepts

LangGraph.js is a library for building **stateful, multi-step programs as graphs**. You declare a shared state, write plain functions (*nodes*) that read it and return updates, and connect them with *edges*. LangGraph runs the graph, merges the updates into the state, and gives you streaming, persistence, pausing and resuming on top.

It is mostly used for LLM agents, but nothing in the core is LLM-specific. Nodes are just functions, so every example in this folder runs without an API key.

> Targets `@langchain/langgraph` 1.x on Node 20+. Examples are ESM TypeScript.

## Why a graph instead of a loop

A `while` loop around a model call is fine until you need to:

- branch on intermediate results
- run steps in parallel
- pause for a human and resume hours later
- survive a crash without redoing finished work
- see exactly what happened at each step

All of these need the same thing: **named steps with explicit state between them**. A graph gives LangGraph step boundaries where it can checkpoint, interrupt, stream and retry. That is the real value, more than the graph shape itself.

## The pieces

| Piece | What it is |
|---|---|
| **State** | The shared data every node reads. A schema plus an update rule (reducer) per key. |
| **Node** | A function `(state, config) => update`. Does the work. |
| **Edge** | Decides which node(s) run next. |
| **Compiled graph** | What `.compile()` returns: a runnable with `invoke`, `stream`, `batch`. |

`StateGraph` is the *builder*. You add nodes and edges to it, then call `compile()` once. You run the compiled graph, not the builder.

## Setup

```bash
npm install @langchain/langgraph @langchain/core
```

`@langchain/core` supplies message classes and the `RunnableConfig` type; LangGraph itself builds on it.

## A minimal graph

```ts
import { StateGraph, Annotation, START, END } from "@langchain/langgraph";

const State = Annotation.Root({
  name: Annotation<string>(),
  greeting: Annotation<string>(),
});

const greet = (state: typeof State.State) => ({
  greeting: `Hello, ${state.name}!`,
});

const shout = (state: typeof State.State) => ({
  greeting: state.greeting.toUpperCase(),
});

const graph = new StateGraph(State)
  .addNode("greet", greet)
  .addNode("shout", shout)
  .addEdge(START, "greet")
  .addEdge("greet", "shout")
  .addEdge("shout", END)
  .compile();

const result = await graph.invoke({ name: "Ada" });
// { name: "Ada", greeting: "HELLO, ADA!" }
```

Three things to notice:

1. Nodes **return updates**, they don't return "the next input". `shout` reads `greeting` from state because `greet` wrote it there.
2. A node returns only the keys it changed. The rest of the state is untouched.
3. `START` and `END` are markers, not nodes you write.

To watch it run, stream the per-node updates:

```ts
for await (const chunk of await graph.stream(
  { name: "Ada" },
  { streamMode: "updates" },
)) {
  console.log(chunk);
}
// { greet: { greeting: "Hello, Ada!" } }
// { shout: { greeting: "HELLO, ADA!" } }
```

## How execution works

LangGraph runs in **supersteps**, an idea inspired by Google's Pregel. Each superstep is one round:

```
invoke(input)
   │
   ▼
 write input into state
   │
   ▼
┌─► pick the nodes triggered by the previous step's updates
│     │
│     ▼
│   run them (concurrently, if there are several)
│     │        └─ none of them can see each other's writes
│     ▼
│   apply all their updates through the reducers
│     │
│     ▼
│   checkpoint (only if a checkpointer is configured)
└─────┘  repeat until nothing is left to run
   │
   ▼
 return final state
```

The consequences matter more than the diagram:

- A node sees the state **as it was at the start of its superstep**. Nodes running in parallel are blind to each other.
- Updates are merged **at the end of the step**. If two parallel nodes write the same key, the key needs a reducer to say how to combine them (see [State and Reducers](./02-state-and-reducers.md)).
- Checkpoints, interrupts and retries happen at **superstep boundaries**. This is what makes pause/resume possible.
- The **recursion limit** counts supersteps, not nodes. It is a safety net against infinite loops (default 25, raised per call with `{ recursionLimit: n }`). Hitting it throws `GraphRecursionError`.

## Common misconceptions

- **"LangGraph requires LangChain models."** It doesn't. Use any SDK, or no LLM at all. `@langchain/core` is a dependency, not a requirement to use `ChatOpenAI` and friends.
- **"Nodes pass data to each other."** They don't. Nodes read and write shared state; edges only control *order*.
- **"A node's return value replaces the state."** It's a partial update merged key by key, using each key's reducer.
- **"It's a DAG."** Cycles are allowed and are the point: agent loops are a cycle (model → tools → model).
- **"Edges mean sequential."** Two edges out of one node start two branches in the same superstep.

## Debugging first aid

- `streamMode: "updates"` shows which node ran and what it wrote. `"values"` shows the full state after each step.
- To see the wiring, print a diagram:

  ```ts
  console.log((await graph.getGraph()).drawMermaid());
  ```

  Paste the output into any Mermaid renderer.
- A node that "does nothing" almost always forgot to `return` its update. More in [Nodes and Edges](./03-nodes-and-edges.md).

## Quick summary

- A graph = **state + nodes + edges**, compiled into a runnable.
- Nodes return partial updates; they communicate through state, not return values.
- Execution is a sequence of **supersteps**; parallel nodes in one step can't see each other, and their updates merge at the end.
- Checkpointing, interrupts and the recursion limit all operate on supersteps.

**Next:** [State and Reducers](./02-state-and-reducers.md)
