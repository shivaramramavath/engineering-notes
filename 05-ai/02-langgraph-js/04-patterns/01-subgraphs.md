# Subgraphs

A subgraph is a compiled graph used as a node inside another graph. It lets you package a multi-step workflow behind one name, reuse it in several parents, test it on its own, and let different people own different parts of a large system.

Prerequisites: [Nodes and Edges](../01-core/03-nodes-and-edges.md), [State and Reducers](../01-core/02-state-and-reducers.md).

## Two ways to plug one in

How a subgraph talks to its parent depends on whether the two share state keys.

| | Add the graph as a node | Call it from a node function |
|---|---|---|
| Use when | Parent and subgraph share state keys | Their schemas differ |
| Data in/out | Automatic, via the shared keys | You map inputs and outputs yourself |
| Code | `addNode("name", childGraph)` | `addNode("name", async (s) => { ... child.invoke(...) })` |

### 1. Shared keys: add it directly

If the subgraph's state schema overlaps with the parent's, pass the compiled graph straight to `addNode`. The overlapping keys flow in, and the subgraph's resulting values for them flow back out.

```ts
import { StateGraph, Annotation, START, END } from "@langchain/langgraph";

// Subgraph
const ChildState = Annotation.Root({
  topic: Annotation<string>(),
  findings: Annotation<string>(),
});

const research = (s: typeof ChildState.State) => ({
  findings: `Notes on ${s.topic}`,
});

const child = new StateGraph(ChildState)
  .addNode("research", research)
  .addEdge(START, "research")
  .addEdge("research", END)
  .compile();

// Parent
const ParentState = Annotation.Root({
  topic: Annotation<string>(),
  findings: Annotation<string>(),
  report: Annotation<string>(),
});

const write = (s: typeof ParentState.State) => ({
  report: `Report: ${s.findings}`,
});

const parent = new StateGraph(ParentState)
  .addNode("researchTeam", child)
  .addNode("write", write)
  .addEdge(START, "researchTeam")
  .addEdge("researchTeam", "write")
  .addEdge("write", END)
  .compile();

await parent.invoke({ topic: "graphs" });
// { topic: "graphs", findings: "Notes on graphs", report: "Report: Notes on graphs" }
```

`topic` and `findings` exist in both schemas, so they're the interface. `report` is parent-only and the subgraph never sees it.

**Watch the reducers on shared keys.** The subgraph's resulting values are written back into the parent *through the parent's reducers*. With an append-style reducer on a shared list, the parent can end up with items counted twice (what it already had, plus the subgraph's copy of them). `messages` is safe because its reducer de-duplicates by message id. For your own shared lists, use an idempotent reducer, or use the wrapper approach below and return only the new items.

### 2. Different schemas: wrap it in a function

When the subgraph has its own vocabulary, call it from a node and translate:

```ts
const ChildState = Annotation.Root({
  question: Annotation<string>(),
  answer: Annotation<string>(),
});

const think = (s: typeof ChildState.State) => ({
  answer: `Answer to: ${s.question}`,
});

const child = new StateGraph(ChildState)
  .addNode("think", think)
  .addEdge(START, "think")
  .addEdge("think", END)
  .compile();

const ParentState = Annotation.Root({
  topic: Annotation<string>(),
  summary: Annotation<string>(),
});

const callChild = async (s: typeof ParentState.State) => {
  const out = await child.invoke({ question: `Tell me about ${s.topic}` });
  return { summary: out.answer };
};

const parent = new StateGraph(ParentState)
  .addNode("callChild", callChild)
  .addEdge(START, "callChild")
  .addEdge("callChild", END)
  .compile();
```

This costs a few lines but gives you full control: the subgraph's internals never leak into the parent's state, and you decide exactly what comes back. It's the right default for reusable subgraphs and for ones that keep large intermediate state you don't want in the parent.

For a narrower public surface, `StateGraph` can also declare separate input and output schemas, so callers only see the keys you intend. See the API reference for the exact constructor form in your version.

## Behavior to know

**Persistence is configured once, on the parent.** Compile only the parent with a checkpointer; it's used for the subgraphs too. Don't give each subgraph its own saver. See [Persistence](../03-stateful/01-persistence.md).

**Interrupts work inside subgraphs.** A subgraph can call `interrupt()` ([Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md)). On resume, the parent node containing the subgraph restarts from its top and the subgraph continues from its pause point, so keep code in the wrapper node before the call idempotent.

**Streaming hides nested events by default.** Pass `subgraphs: true` in the stream options to include events from inside subgraphs; chunks then carry a namespace saying which graph they came from ([Streaming](../02-agents/02-streaming.md)).

**Subgraphs count toward the recursion limit** like any other steps in their own run.

## Navigating to the parent: `Command.PARENT`

A node inside a subgraph can send control to a node in the **parent** graph by returning a `Command` that targets it:

```ts
import { Command } from "@langchain/langgraph";

const escalate = () =>
  new Command({
    graph: Command.PARENT,
    goto: "humanReview", // a node in the parent graph
    update: { status: "escalated" },
  });
```

This is the basis for agent handoffs in [Multi-Agent](./02-multi-agent.md). If the update touches a key that both graphs hold, give that key a reducer in the parent.

## When to use subgraphs

- A workflow is reused in more than one place.
- Separate teams or modules own separate parts of a big graph.
- You want to test a unit in isolation, then compose it.
- You're building multi-agent systems where each agent is its own graph.

When *not* to: a handful of nodes in one graph is simpler. Nesting adds state-mapping decisions and harder debugging, so split when there's a reason, not preemptively.

## Common mistakes

- **Mismatched key names** between parent and subgraph, so data silently doesn't flow. Check spelling and types of the shared keys.
- **Doubled list items** from append reducers on shared keys (see above).
- **Compiling subgraphs with their own checkpointer** instead of letting the parent's apply.
- **Leaking everything into the parent.** If the subgraph's internals don't belong in parent state, use the wrapper approach.
- **Expecting `invoke` on the parent to show inner steps.** Stream with `subgraphs: true`.

## Debugging

- Run the subgraph alone with `child.invoke(...)` first. If it works alone but not nested, the problem is the state mapping.
- Stream with `subgraphs: true` and `streamMode: "updates"` to see which inner node wrote what.
- To see the nested structure in a diagram, ask for the expanded graph: `graph.getGraph({ xray: true })`.

## Quick summary

- A subgraph is a compiled graph used as a node: directly when state keys are shared, or via a wrapper function when schemas differ.
- Shared keys flow automatically, through the parent's reducers; wrap the call when you want control.
- Configure the checkpointer on the parent only; interrupts work and stream events need `subgraphs: true`.
- `Command.PARENT` lets a subgraph node route to a parent node.
- Split into subgraphs for reuse, ownership and testing, not by default.

**Next:** [Multi-Agent](./02-multi-agent.md)
