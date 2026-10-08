# Time Travel

Because a checkpointer saves a snapshot after every superstep, a thread's whole history is available. **Time travel** means picking an earlier checkpoint and either **replaying** from it or **forking** it with changed state. It's how you answer "why did the agent do that?", try "what if it had seen X instead?", or build an undo button.

Prerequisites: [Persistence](./01-persistence.md) (checkpoints, `getState`, `updateState`).

There is no special mode. Time travel is just the checkpoint history plus two operations you already know.

## Listing history

```ts
const history = [];
for await (const snap of graph.getStateHistory(cfg)) {
  history.push(snap);
}
// newest first
```

Note the difference from `stream`: `getStateHistory` is iterated directly, **without** an `await` in front of the call.

Each snapshot has:

- `values`: the state at that checkpoint
- `next`: the nodes that were about to run from there
- `config`: a config containing that checkpoint's `checkpoint_id`; pass it to continue from this exact point
- `metadata`: bookkeeping such as the step number and what created the checkpoint

You can limit the listing: `graph.getStateHistory(cfg, { limit: 10 })`.

## A worked example

A three-step graph so there are several checkpoints to choose from:

```ts
import {
  StateGraph,
  Annotation,
  MemorySaver,
  START,
  END,
} from "@langchain/langgraph";

const State = Annotation.Root({
  value: Annotation<number>(),
  trail: Annotation<string[]>({
    reducer: (a, b) => a.concat(b),
    default: () => [],
  }),
});

const a = (s: typeof State.State) => ({ value: s.value + 1, trail: ["a"] });
const b = (s: typeof State.State) => ({ value: s.value * 10, trail: ["b"] });
const c = (s: typeof State.State) => ({ value: s.value - 3, trail: ["c"] });

const graph = new StateGraph(State)
  .addNode("a", a)
  .addNode("b", b)
  .addNode("c", c)
  .addEdge(START, "a")
  .addEdge("a", "b")
  .addEdge("b", "c")
  .addEdge("c", END)
  .compile({ checkpointer: new MemorySaver() });

const cfg = { configurable: { thread_id: "tt" } };
await graph.invoke({ value: 1 }, cfg);
// value: 1 → a: 2 → b: 20 → c: 17, trail: ["a","b","c"]
```

Find the checkpoint just before `b` runs, which is the one whose `next` includes `"b"`:

```ts
const history = [];
for await (const snap of graph.getStateHistory(cfg)) history.push(snap);

const beforeB = history.find((s) => s.next.includes("b"))!;
beforeB.values; // { value: 2, trail: ["a"] }
```

### Replay

Continue from that checkpoint as it was:

```ts
await graph.invoke(null, beforeB.config);
```

Nodes **before** the checkpoint are not run again. Nodes **after** it are. Replay is re-execution from a point, not a recording of the old outputs, so any node downstream that calls an LLM will call it again and may answer differently.

### Fork

Change the state at that checkpoint, creating a new branch, then continue:

```ts
const forkCfg = await graph.updateState(beforeB.config, { value: 100 });
const forked = await graph.invoke(null, forkCfg);
// b: 1000 → c: 997, trail: ["a","b","c"] on the new branch
```

`updateState` applies the values through the reducers and returns a config pointing at the **new** checkpoint. The original history stays intact; the fork is a sibling branch. Resuming with `null` and that config runs the nodes that were next (`b`, then `c`) against the edited state.

## Things to know

- **Branches share the thread.** The original and the fork both live under the same `thread_id` with different `checkpoint_id`s. Plain `getState({ thread_id })` returns the thread's *latest* checkpoint, which after you run a fork is the fork's. Pass a `checkpoint_id` (via a snapshot's `config`) when you need a specific branch.
- **Reducers still apply to edits.** Editing an append-only key adds to it rather than replacing it. Fork `trail` the same way you'd update it in a node. To overwrite, use a key without a reducer (like `value`) or a reducer that supports resets ([State and Reducers](../01-core/02-state-and-reducers.md)).
- **`asNode`** on `updateState` controls which node the edit counts as coming from, which decides what runs next. Leave it out when the previous writer is unambiguous; set it when several nodes could have produced the state.
- **Replays cost real work.** Re-executed nodes re-call models and tools: money, latency, and **side effects**. A tool that sends an email or writes to a database will do so again if you replay past it. Design such tools to be idempotent, or fork only before pure steps.
- **Results can differ.** Model output isn't deterministic, so a replay isn't a faithful reproduction of the original run.

## Practical uses

- **Debugging an agent decision.** Rewind to the checkpoint before a bad tool call, tweak the state or prompt context, and rerun to see if it behaves.
- **What-if exploration.** Fork with different inputs and compare branches.
- **Undo / "go back" in a UI.** List the history, let the user pick a step, fork from it (optionally with their edit), and continue.
- **Reproducing bugs.** A saved thread plus a `checkpoint_id` is a ready-made repro for a failure.
- **Correcting bad state.** Edit the state at a checkpoint and resume rather than restarting the whole job.

## Common mistakes

- **Using the latest snapshot's config** when you meant an earlier one. Replay or fork only happens from the checkpoint whose config you pass.
- **Awaiting `getStateHistory`.** It's an async iterable; use `for await (const snap of graph.getStateHistory(cfg))`.
- **Forgetting `null`.** `invoke(null, config)` continues from the checkpoint. Passing new input instead starts a new run on top of it.
- **Replaying past side-effecting tools** without realizing they'll fire again.
- **Assuming an edit overwrites a reducer key.** It's merged by the reducer.

## Debugging tips

- Print `s.next` and `s.values` for each snapshot to find the step where state first goes wrong.
- Compare adjacent snapshots; the diff is what one node changed.
- `metadata` helps identify which snapshots came from your input, from a node, or from a manual `updateState`.

## Quick summary

- Time travel = checkpoint history + replay or fork. It requires a checkpointer.
- `getStateHistory` lists snapshots newest first; each snapshot's `config` points at its checkpoint.
- **Replay**: `invoke(null, snapshot.config)` re-runs the nodes after that point.
- **Fork**: `updateState(snapshot.config, values)` creates a new branch; `invoke(null, newConfig)` continues it.
- Replays re-execute models and tools, so mind cost, nondeterminism and side effects.

**Next:** [Subgraphs](../04-patterns/01-subgraphs.md)
