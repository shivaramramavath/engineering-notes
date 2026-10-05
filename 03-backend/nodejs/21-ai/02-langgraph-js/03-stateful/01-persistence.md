# Persistence

By default a graph run is a function call: it starts with the input you give it and forgets everything when it returns. **Persistence** makes the graph remember. Attach a *checkpointer* at compile time and LangGraph saves the state after every superstep, grouped into *threads*. That single feature underpins conversation memory, pausing for humans, crash recovery and time travel.

Prerequisites: [Concepts](../01-core/01-concepts.md) (supersteps), [State and Reducers](../01-core/02-state-and-reducers.md).

## Two ideas: checkpoint and thread

- A **checkpoint** is a snapshot of the state taken at the end of a superstep, plus what's scheduled to run next.
- A **thread** is the ordered series of checkpoints for one conversation or job, identified by a `thread_id` you choose.

```
invoke(input, { thread_id: "t1" })
   │  load the latest checkpoint for t1 (or start empty)
   │  apply the input through the reducers
   ▼
 superstep 1 ──► checkpoint #1
 superstep 2 ──► checkpoint #2
   ⋮
 return final state
```

## Basic usage

```ts
import {
  StateGraph,
  Annotation,
  MemorySaver,
  START,
  END,
} from "@langchain/langgraph";

const State = Annotation.Root({
  message: Annotation<string>(),
  history: Annotation<string[]>({
    reducer: (a, b) => a.concat(b),
    default: () => [],
  }),
  turns: Annotation<number>({
    reducer: (a, b) => a + b,
    default: () => 0,
  }),
});

const record = (s: typeof State.State) => ({
  history: [s.message],
  turns: 1,
});

const graph = new StateGraph(State)
  .addNode("record", record)
  .addEdge(START, "record")
  .addEdge("record", END)
  .compile({ checkpointer: new MemorySaver() });

const t1 = { configurable: { thread_id: "t1" } };

await graph.invoke({ message: "hi" }, t1);
// { message: "hi", history: ["hi"], turns: 1 }

await graph.invoke({ message: "again" }, t1);
// { message: "again", history: ["hi", "again"], turns: 2 }

await graph.invoke({ message: "hello" }, { configurable: { thread_id: "t2" } });
// { message: "hello", history: ["hello"], turns: 1 }  ← separate thread
```

Look at what happened to each key on the second call: `message` was overwritten (no reducer), while `history` and `turns` carried over and were *extended* by their reducers. **Input to an existing thread is merged into the saved state through the reducers**, it doesn't replace the state.

Once a checkpointer is attached, `thread_id` is required. Omitting it raises an error.

## Choosing a checkpointer

| Saver | Package | Use for |
|---|---|---|
| `MemorySaver` | `@langchain/langgraph` | Tests, notebooks, local experiments. Lost on restart. |
| `SqliteSaver` | `@langchain/langgraph-checkpoint-sqlite` | Single-process apps, local persistence |
| `PostgresSaver` | `@langchain/langgraph-checkpoint-postgres` | Production, multiple instances |

```ts
import { PostgresSaver } from "@langchain/langgraph-checkpoint-postgres";

const checkpointer = PostgresSaver.fromConnString(process.env.DATABASE_URL!);
await checkpointer.setup(); // create tables once, e.g. at deploy time

const graph = builder.compile({ checkpointer });
```

Check the package README for the exact setup in the version you install. The graph code is identical whichever saver you use.

## Inspecting and editing a thread

```ts
const snap = await graph.getState(t1);
snap.values;  // current state
snap.next;    // nodes scheduled to run next (empty when the run is finished)
snap.config;  // config pointing at this exact checkpoint
```

Three methods cover most needs:

- `getState(config)` returns the latest checkpoint for the thread (or a specific one if the config has a `checkpoint_id`).
- `getStateHistory(config)` iterates over all checkpoints, newest first. It is used heavily in [Time Travel](./04-time-travel.md).
- `updateState(config, values, asNode?)` writes new values into the thread **through the reducers** and creates a new checkpoint. `asNode` says which node the update pretends to come from, which decides what runs next.

```ts
await graph.updateState(t1, { history: ["injected note"] });
// appended, because `history` has a concat reducer
```

## Resuming and failures

Passing `null` as the input means "don't add anything, continue from the latest checkpoint":

```ts
await graph.invoke(null, t1);
```

This is how a run resumes after an interrupt ([Human-in-the-Loop](./03-human-in-the-loop.md)) or after a crash.

If a node throws, everything from earlier supersteps is already saved. When several nodes run in parallel and one fails, the writes from the ones that succeeded are stored as *pending writes*, so resuming re-runs only what failed. Details and retry strategies are in [Reliability](../05-production/02-reliability.md).

## What gets persisted, and what doesn't

- **Persisted:** the channel values in your state, the "what's next" bookkeeping, and metadata such as the step number.
- **Not persisted:** local variables in a node, closures, anything outside state. Only the state survives.
- **State must be serializable.** Plain data and LangChain message objects are fine. Class instances with custom methods, open connections or functions are not, so store ids or plain data instead.
- `config.configurable` values (like a `userId`) are passed on each call; re-supply them every time.

## Production considerations

- **Checkpoints accumulate.** One per superstep, per thread, forever. Plan retention and cleanup; check your saver's API for deleting a thread.
- **They contain your full state**, including user messages and tool outputs. Treat the database as sensitive data ([Security](../05-production/03-security.md)).
- **Keep state small.** It's serialized at every step. Store large documents elsewhere and keep references in state.
- **Make `thread_id` unguessable and scoped.** Derive it from your own session or conversation id, and make sure one user can't pass another user's `thread_id`. Authorization is your job; LangGraph just uses the id you give it.

## Common misconceptions

- **"A checkpointer is long-term memory."** It's per-thread. Facts that should survive across threads (a user's preferences) belong in a store: [Long-Term Memory](./02-long-term-memory.md).
- **"`MemorySaver` is fine for production."** It lives in process memory; a restart or a second instance loses everything.
- **"Invoking again replays the run."** It starts a *new* run on top of the saved state, beginning at `START`. Replaying from an earlier point is a different operation (time travel).

## Common mistakes

- Forgetting `thread_id`, or generating a fresh one per request so nothing carries over.
- Expecting overwritten keys to accumulate (they only accumulate with reducers).
- Putting non-serializable objects in state.
- Passing input instead of `null` when you meant to resume. The input is merged in and a new run starts.

## Debugging

- `getState(config).next` tells you whether a thread is finished (`[]`) or waiting.
- Walk `getStateHistory(config)` to see how a value changed from step to step.
- If memory "isn't working," confirm the same `thread_id` string is used on both calls and that the graph was compiled with the checkpointer.

## Quick summary

- A checkpointer saves state after every superstep, per `thread_id`; you attach it with `compile({ checkpointer })`.
- New input to an existing thread is merged via reducers; `invoke(null, config)` continues from the last checkpoint.
- `getState`, `getStateHistory` and `updateState` read and edit a thread.
- Use `MemorySaver` for tests, SQLite or Postgres for real apps.
- Threads are per-conversation memory, not cross-conversation memory.

**Next:** [Long-Term Memory](./02-long-term-memory.md)
