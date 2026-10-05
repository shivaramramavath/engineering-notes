# State and Reducers

State is the shared memory of a graph. It is a schema plus, for each key, a rule for **how an incoming update combines with the current value**. Most of LangGraph's behavior (parallel branches, message history, loops) follows from getting those rules right.

Prerequisite: [Concepts](./01-concepts.md), especially supersteps.

## State as channels

Each key in the state is a *channel*. A channel holds a value and knows how to absorb updates:

- **No reducer** (the default): last write wins. The update overwrites the value.
- **With a reducer**: `(current, update) => next`. The update is folded into the current value.

```ts
import { Annotation } from "@langchain/langgraph";

const State = Annotation.Root({
  topic: Annotation<string>(), // overwrite
  log: Annotation<string[]>({ // append
    reducer: (current, update) => current.concat(update),
    default: () => [],
  }),
});
```

`Annotation.Root` also gives you the two types you need for node signatures:

```ts
type S = typeof State.State;  // { topic: string; log: string[] }
type U = typeof State.Update; // { topic?: string; log?: string[] }
```

Nodes read `State`, and return an `Update`: any subset of the keys.

```ts
import { StateGraph, START, END } from "@langchain/langgraph";

const a = (s: typeof State.State): typeof State.Update => ({
  log: ["a ran"],
});

const b = (s: typeof State.State): typeof State.Update => ({
  topic: s.topic.trim(),
  log: ["b ran"],
});

const graph = new StateGraph(State)
  .addNode("a", a)
  .addNode("b", b)
  .addEdge(START, "a")
  .addEdge("a", "b")
  .addEdge("b", END)
  .compile();

await graph.invoke({ topic: "  graphs  " });
// { topic: "graphs", log: ["a ran", "b ran"] }
```

Without the reducer on `log`, `b`'s `["b ran"]` would have *replaced* `["a ran"]`. The reducer is what turns "write" into "append".

## Writing reducers

A reducer gets the current value and one update, and returns the next value.

```ts
// counter
Annotation<number>({ reducer: (a, b) => a + b, default: () => 0 });

// shallow-merge an object
Annotation<Record<string, unknown>>({
  reducer: (a, b) => ({ ...a, ...b }),
  default: () => ({}),
});

// keep the best score seen
Annotation<number>({ reducer: Math.max, default: () => 0 });
```

Rules of thumb:

- **Return a new value; don't mutate `current`.** State history is stored across steps, so in-place mutation can leak into earlier snapshots.
- **Always give a reducer key a `default`.** Otherwise the first update has nothing sensible to fold into.
- **Make the update type wider than the value type when it helps.** The second type argument is the update type:

  ```ts
  log: Annotation<string[], string | string[]>({
    reducer: (cur, upd) => cur.concat(upd),
    default: () => [],
  }),
  ```

  Now a node can return `{ log: "one line" }` or `{ log: ["a", "b"] }`.

### You can't clear an append-only key by returning `[]`

With the concat reducer above, `{ log: [] }` is a no-op: `current.concat([])` is just `current`. If you need resets, build them into the reducer:

```ts
log: Annotation<string[], string[] | null>({
  reducer: (cur, upd) => (upd === null ? [] : cur.concat(upd)),
  default: () => [],
}),
// a node can now return { log: null } to wipe it
```

### Don't spread the whole state into your return

```ts
// ❌ concat reducer sees [...old, ...old, "x"]: duplicates everything
return { ...state, log: [...state.log, "x"] };

// ✅ return only what's new; the reducer does the appending
return { log: ["x"] };
```

This is the most common bug when people port "mutate and return state" habits into a reducer-based graph.

## Messages: the built-in reducer you'll use constantly

Chat-style state is so common that LangGraph ships `MessagesAnnotation`: a single `messages` key backed by `messagesStateReducer`.

```ts
import { StateGraph, MessagesAnnotation, START, END } from "@langchain/langgraph";
import { AIMessage } from "@langchain/core/messages";

const reply = () => ({ messages: [new AIMessage("Hello!")] });

const graph = new StateGraph(MessagesAnnotation)
  .addNode("reply", reply)
  .addEdge(START, "reply")
  .addEdge("reply", END)
  .compile();

const out = await graph.invoke({
  messages: [{ role: "user", content: "Hi" }],
});
// out.messages → [HumanMessage("Hi"), AIMessage("Hello!")]
```

What the reducer does beyond plain concat:

- **Appends** new messages.
- **Replaces** a message when an update carries the same `id` as an existing one.
- **Deletes** with `RemoveMessage`.
- **Coerces** message-like objects such as `{ role: "user", content: "Hi" }` into message classes, and assigns ids to messages that lack one.

Trimming history therefore means *emitting removals*, not overwriting the list:

```ts
import { RemoveMessage } from "@langchain/core/messages";

const trim = (s: typeof MessagesAnnotation.State) => ({
  messages: s.messages.slice(0, -4).map((m) => new RemoveMessage({ id: m.id! })),
});
```

To add your own keys alongside messages, spread the spec:

```ts
const State = Annotation.Root({
  ...MessagesAnnotation.spec,
  userId: Annotation<string>(),
});
```

If you need the same behavior under a different key name, `messagesStateReducer` is exported for use in your own `Annotation`.

## Reducers and parallel writes

Because parallel nodes in one superstep can't see each other, two of them writing the same key is a real conflict. With no reducer, LangGraph can't decide who wins and fails with an `InvalidUpdateError`. With a reducer, it applies each update in turn. Details and fixes are in [Routing and Parallelism](./04-routing-and-parallelism.md).

## Designing state

- **Keep it small.** State is serialized at each checkpoint (see [Persistence](../03-stateful/01-persistence.md)). Store ids, paths and summaries, not large blobs.
- **One owner per key where possible.** Keys written by many nodes are where bugs hide.
- **Put per-run settings in `config`, not state.** User ids and model names aren't part of the graph's evolving data.
- StateGraph also accepts Zod schemas; this repo uses `Annotation` throughout for consistency.

## Common mistakes

| Symptom | Likely cause |
|---|---|
| List grows with duplicates | Spread `...state` into the return alongside an append reducer |
| List never shrinks | Returning `[]` through a concat reducer |
| Value "resets" to the last node's write | Forgot the reducer, so the key overwrites |
| Key typo silently does nothing useful | Node return not typed; annotate as `typeof State.Update` |

## Debugging

- `streamMode: "updates"` shows exactly what each node returned, which is what the reducers received.
- `streamMode: "values"` shows the merged state after each step, which is what the next node will read.
- Comparing the two for a surprising key usually pinpoints the faulty reducer.

## Quick summary

- State = schema + per-key update rule. No reducer means overwrite; a reducer means fold.
- Nodes return **partial** updates, typed as `typeof State.Update`.
- Reducers are pure, return new values, and need defaults.
- Never spread the whole state into a return when a key has an append reducer.
- Use `MessagesAnnotation` for chat history; delete with `RemoveMessage`.

**Next:** [Nodes and Edges](./03-nodes-and-edges.md)
