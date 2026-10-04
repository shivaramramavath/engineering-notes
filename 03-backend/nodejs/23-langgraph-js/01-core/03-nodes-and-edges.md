# Nodes and Edges

Nodes do the work; edges decide what runs next. This note covers their mechanics: signatures, config, the kinds of edge, naming rules and compilation. Choosing *dynamically* between edges is in [Routing and Parallelism](./04-routing-and-parallelism.md).

Prerequisites: [Concepts](./01-concepts.md), [State and Reducers](./02-state-and-reducers.md).

## Nodes

A node is a function from state to an update. Sync or async, both work; prefer async for anything doing I/O.

```ts
import { StateGraph, Annotation, START, END } from "@langchain/langgraph";
import type { RunnableConfig } from "@langchain/core/runnables";

const State = Annotation.Root({
  question: Annotation<string>(),
  answer: Annotation<string>(),
});

const answer = async (
  state: typeof State.State,
  config: RunnableConfig,
): Promise<typeof State.Update> => {
  const user = config.configurable?.userId ?? "anonymous";
  return { answer: `(${user}) you asked: ${state.question}` };
};

const graph = new StateGraph(State)
  .addNode("answer", answer)
  .addEdge(START, "answer")
  .addEdge("answer", END)
  .compile();

await graph.invoke(
  { question: "What is a node?" },
  { configurable: { userId: "u42" } },
);
```

The second argument, `config`, carries per-run settings passed as the second argument of `invoke`/`stream`. Things like `thread_id` (for persistence) and your own values under `configurable` live there. Use it for data that is *about the run*, not data the graph evolves.

What a node may return:

- a partial update object (the normal case)
- a `Command`, to update state **and** pick the next node in one step (see [Routing](./04-routing-and-parallelism.md))

Anything else a runnable can do also works as a node. A compiled graph can be a node (see [Subgraphs](../04-patterns/01-subgraphs.md)), as can prebuilt pieces like `ToolNode` (see [ReAct Agent](../02-agents/01-react-agent.md)).

### Rules a node must follow

**Return the update. Don't mutate state.**

```ts
// ❌ edits a local object; nothing is committed
const bad = (s: typeof State.State) => {
  s.answer = "x";
};

// ✅
const good = (s: typeof State.State) => ({ answer: "x" });
```

**Don't assume you can see siblings' writes.** Within a superstep every node reads the state from the start of that step.

**Keep side effects idempotent.** A node can run again: after a retry, or when a run resumes from a checkpoint (a node that pauses for human input restarts from its top when resumed). Sending an email or charging a card inside a node needs a guard. See [Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md) and [Reliability](../05-production/02-reliability.md).

### Node names

- Must be unique strings. `__start__` and `__end__` are reserved (`START` and `END` are those constants).
- **A node name can't equal a state key.** Having a `summary` key and a `summary` node is an error at `addNode`; rename one, for example the node to `summarize`.
- Names show up in streamed updates and diagrams, so make them readable verbs.

### addNode options

`addNode(name, fn, options?)`. Two options you'll meet:

- `ends: [...]` declares where a `Command`-returning node can go, so diagrams are accurate.
- `retryPolicy` configures automatic retries for flaky nodes (covered in [Reliability](../05-production/02-reliability.md)).

## Edges

| Kind | API | Behavior |
|---|---|---|
| Entry | `addEdge(START, "a")` | Where the run begins. Several entry edges start branches in parallel. |
| Normal | `addEdge("a", "b")` | When `a` finishes, `b` runs in the next step. |
| Finish | `addEdge("a", END)` | This path is done. The run ends when nothing else is pending; other branches keep going. |
| Join | `addEdge(["a", "b"], "c")` | `c` waits until **all** listed nodes have finished. |
| Conditional | `addConditionalEdges("a", router, ...)` | Next node is chosen at runtime from state. |

Edges carry no data. They only schedule nodes. All data moves through state.

Add explicit `END` edges even where a dead end would probably end the run anyway. It makes intent obvious and the diagram readable.

## Compiling

```ts
const builder = new StateGraph(State)
  .addNode("answer", answer)
  .addEdge(START, "answer")
  .addEdge("answer", END);

const graph = builder.compile();
```

`compile()` checks the wiring and throws early for mistakes like having no entry edge or pointing an edge at a node that doesn't exist. This is why name typos in `addEdge` fail at startup, while typos in values returned by a *router function* only fail at runtime (more on that in the routing note).

`compile` also takes the options that turn on the stateful features:

- `checkpointer` for persistence ([Persistence](../03-stateful/01-persistence.md))
- `store` for long-term memory ([Long-Term Memory](../03-stateful/02-long-term-memory.md))
- `interruptBefore` / `interruptAfter` for pausing ([Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md))

## Common mistakes

- **Update "disappears".** The node mutated `state` or forgot to `return`.
- **Join runs too early or twice.** Two separate `addEdge("a","c")` and `addEdge("b","c")` mean "run `c` after *either*". Use the array form to wait for both.
- **`addNode` throws about a state attribute.** Node name collides with a state key.
- **Unclear endings.** A node with no outgoing edge is easy to misread in a diagram. Add the explicit `END` edge so every path visibly terminates.

## Debugging

- Print the wiring and compare it with your mental model:

  ```ts
  console.log((await graph.getGraph()).drawMermaid());
  ```

- Stream `"updates"` to see which nodes ran, in what order, and what each returned.
- If a node ran twice unexpectedly, look at the edges *into* it before looking at the node.

## Quick summary

- A node is `(state, config) => update | Command`, sync or async. Return updates; never mutate state.
- Edges only schedule. Kinds: entry, normal, finish, join (array form), conditional.
- Node names must be unique and can't collide with state keys.
- `compile()` validates the wiring and is where checkpointers, stores and interrupts are attached.
- Nodes can re-run, so make side effects idempotent.

**Next:** [Routing and Parallelism](./04-routing-and-parallelism.md)
