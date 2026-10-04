# Routing and Parallelism

Static edges give you a fixed path. Real graphs need to choose a path from state, loop until a condition holds, and run work in parallel. LangGraph gives you four tools for this: conditional edges, `Command`, multiple edges (fan-out and join), and `Send`.

Prerequisites: [State and Reducers](./02-state-and-reducers.md), [Nodes and Edges](./03-nodes-and-edges.md).

## Conditional edges

`addConditionalEdges(source, router, pathMap?)` runs `router` **after `source` finishes**, with the state that includes `source`'s update. The router returns the name(s) of what to run next.

A loop that runs until a counter reaches 3:

```ts
import { StateGraph, Annotation, START, END } from "@langchain/langgraph";

const State = Annotation.Root({
  count: Annotation<number>(),
});

const increment = (s: typeof State.State) => ({ count: s.count + 1 });

const route = (s: typeof State.State) => (s.count < 3 ? "increment" : END);

const graph = new StateGraph(State)
  .addNode("increment", increment)
  .addEdge(START, "increment")
  .addConditionalEdges("increment", route, ["increment", END])
  .compile();

await graph.invoke({ count: 0 }); // { count: 3 }
```

Notes:

- The router is a plain function of state. Keep it cheap and side-effect free; put any expensive decision (like an LLM call) in a node that writes its verdict to state, and route on that.
- It can return one name, an array of names (run them in parallel), `END`, or `Send` objects (below).
- `pathMap` is optional. An array lists the possible destinations; a record maps the router's return values to node names:

  ```ts
  .addConditionalEdges("check", (s) => (s.ok ? "ok" : "retry"), {
    ok: END,
    retry: "work",
  })
  ```

  It mainly matters for accurate diagrams, and the record form lets the router return domain words instead of node names.
- A router returning a name that isn't a registered node fails at **runtime**, not at `compile()`.

### Loops need a way out

Every cycle counts supersteps against the recursion limit (default 25). Exceeding it throws `GraphRecursionError`. Terminate through state, as the counter above does. Raising the limit (`{ recursionLimit: 100 }` in the call's config) is for graphs that legitimately need many steps, not a fix for a loop that never ends.

## `Command`: update and route together

When the routing decision is a by-product of the node's work, return a `Command` and skip the separate router:

```ts
import { Command } from "@langchain/langgraph";

const State = Annotation.Root({
  score: Annotation<number>(),
  label: Annotation<string>(),
  handledBy: Annotation<string>(),
});

const triage = (s: typeof State.State) =>
  s.score > 0.8
    ? new Command({ update: { label: "urgent" }, goto: "escalate" })
    : new Command({ update: { label: "normal" }, goto: "archive" });

const escalate = () => ({ handledBy: "oncall" });
const archive = () => ({ handledBy: "inbox" });

const graph = new StateGraph(State)
  .addNode("triage", triage, { ends: ["escalate", "archive"] })
  .addNode("escalate", escalate)
  .addNode("archive", archive)
  .addEdge(START, "triage")
  .addEdge("escalate", END)
  .addEdge("archive", END)
  .compile();
```

- `ends` tells LangGraph the possible destinations. It affects diagrams only; the `goto` is what actually routes.
- **`goto` adds to static edges, it doesn't replace them.** If `triage` also had a normal `addEdge("triage", "archive")`, both would run.
- `goto` accepts a node name, an array of names, or `Send` objects.

| Use conditional edges when | Use `Command` when |
|---|---|
| Routing logic should be visible and separate from the node | The decision falls out of the node's own work |
| Several nodes share the same router | You want update + route atomically, with no recomputation |
| You want the graph's shape readable from `addConditionalEdges` | Handing off between agents ([Multi-Agent](../04-patterns/02-multi-agent.md)) |

## Parallel branches: fan-out and join

Two edges out of one node start two branches **in the same superstep**. Because the branches are blind to each other, any key they both write needs a reducer.

```ts
const State = Annotation.Root({
  query: Annotation<string>(),
  results: Annotation<string[]>({
    reducer: (a, b) => a.concat(b),
    default: () => [],
  }),
  summary: Annotation<string>(),
});

const searchWeb = async (s: typeof State.State) => ({
  results: [`web:${s.query}`],
});
const searchDocs = async (s: typeof State.State) => ({
  results: [`docs:${s.query}`],
});
const summarize = (s: typeof State.State) => ({
  summary: s.results.slice().sort().join(" | "),
});

const graph = new StateGraph(State)
  .addNode("searchWeb", searchWeb)
  .addNode("searchDocs", searchDocs)
  .addNode("summarize", summarize)
  .addEdge(START, "searchWeb")
  .addEdge(START, "searchDocs")
  .addEdge(["searchWeb", "searchDocs"], "summarize")
  .addEdge("summarize", END)
  .compile();

await graph.invoke({ query: "cats" });
// summary: "docs:cats | web:cats"
```

Points that bite:

- **Don't depend on the order** parallel updates are merged in. The example sorts before joining; do the same, or use an order-insensitive reducer.
- **Same key, no reducer = `InvalidUpdateError`.** If `searchWeb` and `searchDocs` both wrote a plain `Annotation<string>()`, the run fails because LangGraph can't pick a winner. The fix is a reducer, or separate keys per branch.
- **The array form of `addEdge` is what waits for all.** With uneven branches the difference is visible:

  ```
  START ─┬─> A ─────────────┐
         └─> B ──> B2 ──────┴─> C
  ```

  - `addEdge(["A", "B2"], "C")`: `C` runs **once**, after both finish.
  - `addEdge("A", "C")` + `addEdge("B2", "C")`: `C` runs **twice**, once after `A` and again after `B2`.
- Concurrency here is I/O concurrency on Node's single thread. It speeds up network and disk waits, not CPU-bound work.

## `Send`: dynamic fan-out (map-reduce)

When the number of branches depends on data, return `Send` objects from a router. Each `Send(node, payload)` runs `node` with **its own payload as input** (not the graph state), in parallel.

```ts
import { Send } from "@langchain/langgraph";

const State = Annotation.Root({
  topics: Annotation<string[]>(),
  summaries: Annotation<string[]>({
    reducer: (a, b) => a.concat(b),
    default: () => [],
  }),
  report: Annotation<string>(),
});

const fanOut = (s: typeof State.State) =>
  s.topics.map((topic) => new Send("summarizeTopic", { topic }));

const summarizeTopic = async (s: { topic: string }) => ({
  summaries: [`summary of ${s.topic}`],
});

const collect = (s: typeof State.State) => ({
  report: s.summaries.slice().sort().join("\n"),
});

const graph = new StateGraph(State)
  .addNode("summarizeTopic", summarizeTopic)
  .addNode("collect", collect)
  .addConditionalEdges(START, fanOut)
  .addEdge("summarizeTopic", "collect")
  .addEdge("collect", END)
  .compile();

await graph.invoke({ topics: ["cats", "dogs"] });
// report: "summary of cats\nsummary of dogs"
```

- The branch outputs flow back through the **reducer** on `summaries`, which is why it needs one.
- All the `Send` branches run in the same superstep, so `collect` runs once after they all finish.
- The payload shape is yours to define; the target node's parameter type should match it.
- Fan-out width follows your data. A thousand items means a thousand concurrent tasks, so chunk large lists yourself before fanning out.

## Which tool when

| Need | Use |
|---|---|
| Fixed order | `addEdge` |
| Branch on state, or loop | `addConditionalEdges` |
| Update state and choose next node together | `Command` |
| A fixed set of parallel branches, then merge | multiple edges + array-form join |
| A data-dependent number of parallel branches | `Send` |

## Common mistakes

- Parallel nodes writing one key without a reducer (`InvalidUpdateError`).
- Joining with two separate edges and getting a double run.
- A router with side effects or slow I/O; it belongs in a node.
- Raising `recursionLimit` instead of fixing a loop's exit condition.
- Expecting `Command.goto` to replace static edges.

## Debugging

- `streamMode: "updates"` emits one chunk per node, so you can see which branches ran and how many times a node fired.
- Draw the graph with `(await graph.getGraph()).drawMermaid()`. Missing or extra arrows from a `Command` node usually mean `ends` is wrong or incomplete.
- For a loop that won't stop, stream `"values"` and watch the key your router reads.

## Quick summary

- Conditional edges route on state after a node finishes; routers must be cheap and pure.
- `Command` couples an update with a `goto`; it supplements static edges rather than replacing them.
- Parallelism = multiple outgoing edges in one superstep. Shared keys need reducers; don't rely on merge order.
- Use the **array form** of `addEdge` to wait for all branches.
- `Send` gives each dynamic branch its own input; results merge through a reducer.
- Loops must exit via state; the recursion limit is a safety net.

**Next:** [ReAct Agent](../02-agents/01-react-agent.md), where this machinery becomes a model-and-tools loop.
