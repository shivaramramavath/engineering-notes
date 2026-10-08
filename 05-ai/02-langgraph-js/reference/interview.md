# LangGraph.js Interview Questions

Short, accurate answers to the questions that actually come up, grouped by topic. Each section links to the note with the full explanation. Answers are deliberately brief: say the core idea first, then add the gotcha an interviewer is fishing for.

> Written for `@langchain/langgraph` 1.x. Defaults such as the recursion limit are worth re-checking for your version.

## Fundamentals

**What is LangGraph, and why use it over a plain loop or a chain?**
A library for stateful, multi-step programs modeled as graphs: shared state, nodes that update it, edges that order them. A loop works until you need branching, parallelism, pausing for a human, crash recovery, or per-step visibility. Named steps give LangGraph boundaries to checkpoint, interrupt, stream and retry at. → [Concepts](../01-core/01-concepts.md)

**Does LangGraph require an LLM or LangChain models?**
No. Nodes are ordinary functions. It's most used for agents, but you can use any model SDK or none.

**Explain state, nodes and edges.**
State is the shared data (schema plus per-key update rules). A node is `(state, config) => update`. Edges decide which nodes run next and carry no data; all data moves through state.

**What does `compile()` do?**
Validates the wiring (entry edge exists, edges point at real nodes) and returns a runnable with `invoke`, `stream` and `batch`. It's also where you attach a checkpointer, store and interrupts. `StateGraph` itself is just the builder.

**What is a superstep, and why does it matter?**
One round of execution: run all nodes triggered by the previous step (concurrently), then apply all their updates through the reducers. Consequences: a node sees state as of the start of its step, parallel nodes can't see each other's writes, and checkpoints, interrupts and the recursion limit all operate on supersteps.

**Is a LangGraph graph a DAG?**
No, cycles are allowed and essential: an agent loop is a cycle.

## State and reducers

**What is a reducer?**
A function `(current, update) => next` for one state key. Without one, the key is overwritten by the last write. With one, updates are folded in (append, sum, merge). → [State and Reducers](../01-core/02-state-and-reducers.md)

**Two parallel nodes write the same key. What happens?**
With a reducer, both updates are applied in turn. Without one, LangGraph can't choose a winner and the run fails (`InvalidUpdateError`). Fix with a reducer or separate keys per branch. Don't depend on the order the parallel updates merge in.

**A node returns `{ ...state, log: [...state.log, "x"] }` and the log has duplicates. Why?**
`log` has an append reducer, so the whole array is *appended* to the existing one. Return only the new item: `{ log: ["x"] }`.

**How do you clear a list that has a concat reducer?**
Returning `[]` is a no-op (`concat([])`). Build resets into the reducer (e.g. accept `null` to mean "reset"), or for messages emit `RemoveMessage` entries.

**What does the messages reducer do beyond append?**
Replaces a message with the same `id`, deletes via `RemoveMessage`, coerces `{ role, content }` objects into message classes, and assigns ids.

**Why not mutate `state` inside a node?**
Mutations aren't committed; only the returned update is. And reducers must return new values, not mutate `current`, because history is stored across steps.

## Control flow and parallelism

**Conditional edge vs `Command`: when each?**
Conditional edges when routing logic should be separate and visible in the graph's structure, or shared. `Command({ update, goto })` when the routing decision falls out of the node's own work, or for handoffs. → [Routing and Parallelism](../01-core/04-routing-and-parallelism.md)

**If a node returns a `Command` with `goto` and also has a static edge, what happens?**
Both run. `goto` adds dynamic edges; it doesn't replace static ones.

**How do you wait for several parallel branches before continuing?**
The array form: `addEdge(["a", "b"], "c")`. Two separate edges `a→c` and `b→c` mean "run `c` after either", so `c` can run twice if the branches finish in different supersteps.

**What is `Send` for?**
Dynamic fan-out (map-reduce). A router returns one `Send(node, payload)` per item; each runs the target node with its own payload in parallel, and results merge through a reducer on the shared key.

**How do you avoid infinite loops?**
Exit via state (counters, flags) in the router. The recursion limit (default 25 per call; set `{ recursionLimit }`) is a backstop that throws `GraphRecursionError`, not the intended exit.

**Why shouldn't a conditional-edge function call an LLM?**
Routers should be cheap and pure. Do model judgments in a node that writes a verdict to state, then route on that state.

## Agents and streaming

**Describe the ReAct loop in LangGraph.**
`agent` node calls the model (with tools bound), a conditional edge checks for `tool_calls`, a `tools` node runs them and appends `ToolMessage`s, then back to `agent`; no tool calls means `END`. → [ReAct Agent](../02-agents/01-react-agent.md)

**Who executes tools: the model or your code?**
Your code. The model only emits tool-call requests; `ToolNode` runs them.

**What goes wrong if you trim message history carelessly?**
Removing an `AIMessage` with tool calls without its `ToolMessage`s (or vice versa) leaves an unpaired call, which providers reject.

**How many tool rounds fit under the recursion limit of 25?**
Each round is two supersteps (agent, tools), so roughly a dozen.

**Which stream modes exist and what is each for?**
`values` (full state each step), `updates` (per-node updates, best for progress), `messages` (LLM tokens), `custom` (your own events via `config.writer`), `debug`. An array of modes yields `[mode, data]` tuples. → [Streaming](../02-agents/02-streaming.md)

**`messages` mode gives me content that isn't a string. Why?**
Some providers return an array of content blocks. Handle both; don't assume string.

**Why does `for await (const c of graph.stream(...))` throw?**
`stream` returns a promise: `for await (const c of await graph.stream(...))`. By contrast, `getStateHistory` is iterated directly.

## Persistence and memory

**Checkpointer vs store?**
Checkpointer: automatic, per-thread snapshots of graph state. Store: explicit key-value memory shared across threads, grouped by namespace. Conversation memory vs cross-conversation memory. → [Persistence](../03-stateful/01-persistence.md), [Long-Term Memory](../03-stateful/02-long-term-memory.md)

**What are a checkpoint and a thread?**
A checkpoint is the state snapshot after a superstep (plus what's next). A thread is the ordered series of checkpoints for one conversation/job, identified by `thread_id`.

**I invoke twice on the same thread. What's the second call's input do?**
It's merged into the saved state through the reducers, then a new run starts from `START`. Overwrite keys are replaced; reducer keys are extended.

**What does `invoke(null, config)` mean?**
Continue from the latest checkpoint without new input. Used to resume after an interrupt or failure.

**Which saver in production?**
A durable one such as Postgres (SQLite for single-process apps). `MemorySaver` is for tests; restarts lose everything.

**What needs to be serializable?**
Everything in state, since it's saved each step. Plain data and LangChain messages are fine; functions, connections and custom class instances aren't.

**How does a node failure interact with checkpoints?**
Earlier supersteps are saved. When parallel nodes ran and one failed, the successful siblings' writes are kept, so a resume re-runs only the failed node. → [Reliability](../05-production/02-reliability.md)

## Human-in-the-loop and time travel

**How does `interrupt()` work?**
It pauses the run, saves a checkpoint and returns control with the payload. Resuming with `new Command({ resume })` on the same thread makes the original `interrupt()` call return that value. Needs a checkpointer and `thread_id`. → [Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md)

**What's the biggest gotcha with `interrupt()`?**
The node restarts from its top on resume. Code before the interrupt runs again, so keep it idempotent and do side effects after approval.

**Why must you not wrap `interrupt()` in `try/catch`?**
It pauses by throwing a special exception; catching it swallows the pause.

**Dynamic `interrupt()` vs `interruptBefore`/`interruptAfter`?**
`interrupt()` asks a human a question from inside a node and resumes with a `Command`. Static breakpoints stop before/after a node, mainly for debugging, and resume with `null`.

**Explain replay vs fork.**
Both start from an earlier checkpoint (from `getStateHistory`). Replay: `invoke(null, snapshot.config)` re-runs the nodes after it. Fork: `updateState(snapshot.config, values)` creates a new branch, then `invoke(null, newConfig)` continues it. → [Time Travel](../03-stateful/04-time-travel.md)

**What's risky about replaying?**
It re-executes downstream nodes: models answer differently, and tools with side effects fire again.

## Patterns

**Two ways to use a subgraph?**
Add the compiled graph directly as a node when state keys are shared (data flows via shared keys), or call it from a wrapper node when schemas differ (you map inputs/outputs). → [Subgraphs](../04-patterns/01-subgraphs.md)

**Where do you configure the checkpointer when using subgraphs?**
On the parent only; it applies to the subgraphs.

**Supervisor vs handoffs?**
Supervisor: a central model node routes to workers, which report back; predictable, one bottleneck. Handoffs: agents transfer control to each other via `Command`; flexible, harder to predict. → [Multi-Agent](../04-patterns/02-multi-agent.md)

**When is multi-agent the wrong choice?**
Often. A single agent with good tools is simpler and cheaper; every agent adds calls, latency and failure modes. Split for distinct roles, oversized toolsets, parallel work or team ownership.

**What makes RAG "agentic"?**
The model decides whether to retrieve, grades results, and rewrites the query to retry, in a bounded loop. Costs extra model calls; fixed pipelines suffice when every question needs retrieval and the index is good. → [Agentic RAG](../04-patterns/03-agentic-rag.md)

## Production

**How do you test a LangGraph app?**
Unit-test nodes, routers and tools as plain functions; test graphs with the model injected as a dependency and stubbed; assert the path by streaming `updates`; test loops with a low `recursionLimit`; test interrupts with a `MemorySaver` and fresh `thread_id`; evaluate model quality separately on a dataset, scoring properties not exact text. → [Testing](../05-production/01-testing.md)

**What does idempotency have to do with LangGraph?**
Nodes can run more than once (retries, resume after crash, interrupt restarts). Side effects need stable idempotency keys derived from things like thread and order ids. → [Reliability](../05-production/02-reliability.md)

**Name your defenses against runaway runs.**
Bounded loops in state, `recursionLimit`, budgets tracked in state, abort signals/timeouts, bounded fan-out width.

**How do you handle prompt injection?**
Assume the model is steerable by anything it reads. Enforce security in code: authorize inside tools using identity from server-built config (never tool arguments), narrow tools with least-privilege credentials, approval steps for risky actions, sandboxing, no secrets in state. → [Security](../05-production/03-security.md)

**How do you isolate users?**
Server-built `config`, `thread_id` scoped to the authenticated user, user-scoped store namespaces, authenticated resume endpoints. Never forward client-supplied `configurable`.

**What changes when you deploy a new graph version while threads are in flight?**
Old checkpoints reference the old shape: renamed/removed nodes can strand paused threads, removed state keys leave unexpected data. Make state changes additive, keep node names stable (or alias), and version breaking changes. → [Deployment](../05-production/04-deployment.md)

## Design scenarios (outline your answer)

**1. Customer-support agent that can issue refunds.**
Single ReAct agent with narrow tools; `userId` from config, not args; refund tool behind `interrupt()` approval; idempotency key per order; Postgres checkpointer; per-user rate limits; tests for each branch, evals for answer quality.

**2. Summarize 500 documents.**
`Send` fan-out over chunks with a reducer collecting summaries, a reduce node (possibly hierarchical), bounded fan-out width for rate limits, retries on the summarize node only, resume-after-crash via checkpointer.

**3. Chatbot that remembers users across sessions.**
Checkpointer for per-conversation history (with trimming via `RemoveMessage`), store for cross-session facts namespaced by user, memory writes in a post-response node, dedupe/update by reusing keys.

**4. Research workflow with human review before publishing.**
Graph: gather (parallel branches with reducers), draft, `interrupt()` approval, publish. Resume via a separate endpoint authenticated and validated; publish node idempotent; long run via a queue and worker.

## Spot the bug

```ts
// 1. Duplicated log entries
const note = (s: typeof State.State) => ({ log: [...s.log, "x"] }); // log has a concat reducer
```
Returns the whole list; the reducer appends it again. Return `{ log: ["x"] }`.

```ts
// 2. "TypeError: not async iterable"
for await (const c of graph.stream(input, { streamMode: "updates" })) { /* ... */ }
```
Missing `await` before `graph.stream(...)`.

```ts
// 3. Email sent twice
const approve = async (s: typeof State.State) => {
  await sendEmail(s.draft);
  const ok = interrupt("Looks good?");
  return new Command({ goto: ok ? "done" : END });
};
```
The node restarts on resume, so `sendEmail` runs again. Ask first, send afterward (or in a later node, idempotently).

```ts
// 4. State never updates
const record = (s: typeof State.State) => { s.count = s.count + 1; };
```
Mutation isn't committed. Return `{ count: s.count + 1 }`.

## Quick summary

- Be able to explain supersteps, reducers and checkpoints; most answers hang on them.
- Know the common traps: spreading state with append reducers, `Command` plus static edges, join semantics, node restart on `interrupt`, unpaired tool calls.
- For design questions, talk about state, control flow, persistence, side effects and security, in that order.

**See also:** [Cheatsheet](./cheatsheet.md)