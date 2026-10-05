# LangGraph.js Cheatsheet

One-page syntax reference for `@langchain/langgraph` 1.x. Each section links to the note with the explanation. Check the API reference for your installed version where a snippet is marked as version-sensitive in the notes.

```bash
npm install @langchain/langgraph @langchain/core
```

```ts
import {
  StateGraph, Annotation, MessagesAnnotation, START, END,
  Command, Send, interrupt, MemorySaver, InMemoryStore,
  type LangGraphRunnableConfig,
} from "@langchain/langgraph";
import { ToolNode } from "@langchain/langgraph/prebuilt";
```

## Minimal graph → [Concepts](../01-core/01-concepts.md)

```ts
const graph = new StateGraph(State)
  .addNode("a", a)
  .addNode("b", b)
  .addEdge(START, "a")
  .addEdge("a", "b")
  .addEdge("b", END)
  .compile();

await graph.invoke({ name: "Ada" });
```

Execution = supersteps: run triggered nodes (in parallel) → apply updates via reducers → checkpoint → repeat.

## State → [State and Reducers](../01-core/02-state-and-reducers.md)

```ts
const State = Annotation.Root({
  topic: Annotation<string>(),              // no reducer: last write wins
  log: Annotation<string[]>({               // reducer: fold updates in
    reducer: (cur, upd) => cur.concat(upd),
    default: () => [],
  }),
});
type S = typeof State.State;                // what nodes read
type U = typeof State.Update;               // what nodes may return (keys optional)
```

```ts
new StateGraph(MessagesAnnotation)          // chat state: `messages`
Annotation.Root({ ...MessagesAnnotation.spec, userId: Annotation<string>() })
```

| Reducer idea | Code |
|---|---|
| Append | `(a, b) => a.concat(b)` |
| Sum | `(a, b) => a + b` |
| Merge object | `(a, b) => ({ ...a, ...b })` |
| Max | `Math.max` |
| Reset support | `(a, b) => (b === null ? [] : a.concat(b))` |

Messages: append, replace on same `id`, delete with `RemoveMessage`.

## Nodes and edges → [Nodes and Edges](../01-core/03-nodes-and-edges.md)

```ts
const node = async (state: S, config: LangGraphRunnableConfig): Promise<U> => {
  const user = config.configurable?.userId;   // per-run settings
  return { topic: "x" };                      // partial update; never mutate `state`
};

.addNode("name", node, { ends: ["x", END], retryPolicy: { maxAttempts: 3 } })
```

| Edge | Code |
|---|---|
| Entry | `addEdge(START, "a")` |
| Normal | `addEdge("a", "b")` |
| Finish | `addEdge("a", END)` |
| Join (wait for all) | `addEdge(["a", "b"], "c")` |
| Conditional | `addConditionalEdges("a", router, ["b", END])` |

Node names: unique, can't equal a state key.

## Routing and parallelism → [Routing and Parallelism](../01-core/04-routing-and-parallelism.md)

```ts
// conditional edge: router reads state after the source node
const route = (s: S) => (s.count < 3 ? "again" : END);
.addConditionalEdges("again", route, ["again", END])

// record form
.addConditionalEdges("check", (s) => (s.ok ? "ok" : "retry"), { ok: END, retry: "work" })

// Command: update + route (adds to static edges, doesn't replace them)
return new Command({ update: { label: "x" }, goto: "next" });
.addNode("n", fn, { ends: ["next", END] })

// fan-out: two edges from one node run in the same superstep
.addEdge(START, "searchWeb").addEdge(START, "searchDocs")
.addEdge(["searchWeb", "searchDocs"], "summarize")   // join once, after both

// Send: dynamic fan-out; each branch gets its own payload
const fanOut = (s: S) => s.items.map((item) => new Send("work", { item }));
.addConditionalEdges(START, fanOut)
```

Parallel writes to one key need a reducer, or the run fails (`InvalidUpdateError`).

| Need | Use |
|---|---|
| Fixed order | `addEdge` |
| Branch/loop on state | `addConditionalEdges` |
| Update + choose next | `Command` |
| Fixed parallel branches | multiple edges + array join |
| N dynamic branches | `Send` |

## Running → [Streaming](../02-agents/02-streaming.md)

```ts
const cfg = {
  configurable: { thread_id: "t1", userId: "u1" },
  recursionLimit: 25,                         // GraphRecursionError when exceeded
  signal: AbortSignal.timeout(60_000),
};

await graph.invoke(input, cfg);
for await (const chunk of await graph.stream(input, { ...cfg, streamMode: "updates" })) {}
```

| `streamMode` | Chunk |
|---|---|
| `"values"` | full state each step |
| `"updates"` | `{ node: update }` |
| `"messages"` | `[messageChunk, metadata]`, tokens; `metadata.langgraph_node` |
| `"custom"` | whatever nodes emit via `config.writer?.(x)` |
| `"debug"` | execution events |
| `[...modes]` | `[mode, data]` tuples |

`subgraphs: true` includes nested graph events. `msg.content` may be an array of blocks.

## Agent loop → [ReAct Agent](../02-agents/01-react-agent.md)

```ts
const getWeather = tool(async ({ city }) => `Sunny in ${city}`, {
  name: "get_weather",
  description: "Get the weather for a city.",
  schema: z.object({ city: z.string() }),
});

const model = new ChatAnthropic({ model: "claude-sonnet-5-5" }).bindTools([getWeather]);

new StateGraph(MessagesAnnotation)
  .addNode("agent", async (s) => ({ messages: [await model.invoke(s.messages)] }))
  .addNode("tools", new ToolNode([getWeather]))
  .addEdge(START, "agent")
  .addConditionalEdges("agent", (s) =>
    (s.messages.at(-1) as AIMessage).tool_calls?.length ? "tools" : END, ["tools", END])
  .addEdge("tools", "agent")
  .compile();
```

Model *requests* tools; `ToolNode` *runs* them. Keep `AIMessage` tool calls and `ToolMessage`s paired. Each round ≈ 2 supersteps.

## Persistence → [Persistence](../03-stateful/01-persistence.md)

```ts
const graph = builder.compile({ checkpointer: new MemorySaver(), store: new InMemoryStore() });

await graph.invoke(input, { configurable: { thread_id: "t1" } });   // thread_id required
await graph.invoke(null, cfg);                                       // resume from last checkpoint

const snap = await graph.getState(cfg);        // snap.values, snap.next, snap.config
for await (const s of graph.getStateHistory(cfg)) {}   // newest first; no await on the call
await graph.updateState(cfg, { key: "v" }, "asNode?"); // goes through reducers
```

New input to an existing thread is **merged via reducers**. Savers: `MemorySaver` (tests), SQLite, Postgres (production).

## Long-term memory → [Long-Term Memory](../03-stateful/02-long-term-memory.md)

```ts
await store.put(["memories", userId], key, { text: "..." });   // same key overwrites
await store.get(["memories", userId], key);                    // item?.value
await store.search(["memories", userId]);                      // namespace prefix match
await store.delete(["memories", userId], key);

// in a node
await config.store?.search(["memories", config.configurable?.userId as string]);
```

## Human-in-the-loop → [Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md)

```ts
const decision = interrupt({ question: "Send it?", draft: s.draft });   // returns the resume value

const paused = await graph.invoke(input, cfg);         // paused.__interrupt__ ; getState(cfg).next non-empty
await graph.invoke(new Command({ resume: "yes" }), cfg);

compile({ checkpointer, interruptBefore: ["send"] });  // static breakpoint; resume with invoke(null, cfg)
```

Node **restarts from the top** on resume. No side effects before `interrupt()`. Never `try/catch` around it.

## Time travel → [Time Travel](../03-stateful/04-time-travel.md)

```ts
const before = history.find((s) => s.next.includes("b"))!;
await graph.invoke(null, before.config);                          // replay from there
const fork = await graph.updateState(before.config, { value: 100 });
await graph.invoke(null, fork);                                   // continue the new branch
```

Replays re-run later nodes (models, tools, side effects).

## Subgraphs and multi-agent → [Subgraphs](../04-patterns/01-subgraphs.md), [Multi-Agent](../04-patterns/02-multi-agent.md)

```ts
.addNode("team", childGraph)                       // shared keys flow automatically
.addNode("wrap", async (s) => {                    // different schemas: map yourself
  const out = await child.invoke({ question: s.topic });
  return { summary: out.answer };
});

new Command({ graph: Command.PARENT, goto: "parentNode", update: { ... } })
```

Checkpointer on the parent only. Supervisor = model-routing node + `Command`; workers return to it.

## Reliability → [Reliability](../05-production/02-reliability.md)

| Concern | Tool |
|---|---|
| Transient errors | `retryPolicy` (explicit `retryOn`), don't stack with client `maxRetries` |
| Hangs | `signal: AbortSignal.timeout(ms)` |
| Loops | state counters + `recursionLimit` |
| Spend | budget key with summing reducer |
| Duplicate side effects | idempotency keys (thread + entity id) |
| Crash recovery | durable saver + `invoke(null, cfg)` |
| Model outage | `model.withFallbacks({ fallbacks: [backup] })` |

## Security and deployment → [Security](../05-production/03-security.md), [Deployment](../05-production/04-deployment.md)

- Identity from server-built `config`, never from tool arguments; never forward client `configurable`.
- `thread_id` scoped to the user; user-scoped store namespaces; authenticate resume endpoints.
- Narrow tools, least-privilege credentials, approval for risky actions, no secrets in state.
- Compile once at startup; Postgres checkpointer; `setup()` at deploy time.
- Stream over SSE; abort on client disconnect; sanitize chunks.
- Changing graphs with threads in flight: additive state, stable node names, version breaking changes.

## Gotchas

| Symptom | Cause | Fix |
|---|---|---|
| Update disappears | Mutated `state` / forgot to `return` | Return a partial update |
| Duplicated list items | Spread state with append reducer | Return only new items |
| List won't clear | `[]` through concat reducer | Reset-aware reducer / `RemoveMessage` |
| Node runs twice | Separate edges into one node | `addEdge([a, b], c)` |
| Both branches ran after `Command` | `goto` adds to static edges | Remove the static edge |
| `InvalidUpdateError` | Parallel writes, no reducer | Add reducer or split keys |
| `not async iterable` | Missing `await` on `graph.stream` | `await graph.stream(...)` |
| Resume starts a new run | Wrong `thread_id` or passed input, not `Command` | Same thread, `new Command({ resume })` |
| Email sent twice | Side effect before `interrupt()` | Act after approval; idempotency key |
| Unpaired tool call error | Trimmed `AIMessage` without its `ToolMessage` | Trim in pairs |
| `GraphRecursionError` | Loop never exits | Exit via state; raise limit only if legitimate |
| Memory not saving | No `store` in `compile` (`config.store` undefined) | Pass `store` |

## Debug fast

```ts
for await (const c of await graph.stream(input, { streamMode: "updates" })) console.log(c);  // who ran, wrote what
console.log((await graph.getState(cfg)).next);                                                // pending nodes
console.log((await graph.getGraph()).drawMermaid());                                          // wiring
```

**See also:** [Interview Questions](./interview.md)