# Long-Term Memory

A checkpointer ([Persistence](./01-persistence.md)) remembers one conversation. It can't tell a *new* conversation that the user prefers metric units or lives in Pune. For knowledge that outlives a thread, LangGraph has a separate mechanism: the **store**, a key-value database that nodes can read and write, shared across threads.

Prerequisites: [Persistence](./01-persistence.md), [Nodes and Edges](../01-core/03-nodes-and-edges.md) (`config`).

## Checkpointer vs store

| | Checkpointer | Store |
|---|---|---|
| Remembers | Graph state for one thread | Arbitrary records you choose to save |
| Scope | Per `thread_id` | Across threads, grouped by namespace |
| Written by | LangGraph, automatically each step | Your code, explicitly |
| Read by | LangGraph on the next run | Your nodes, on demand |

They solve different problems and are usually used together.

## Store basics

A store holds **items**. Each item has a *namespace* (an array of strings, like a folder path), a *key* (unique within the namespace) and a *value* (a JSON-like object).

```ts
import { InMemoryStore } from "@langchain/langgraph";

const store = new InMemoryStore();

await store.put(["memories", "user-1"], "pref-units", { text: "Prefers metric units" });

const item = await store.get(["memories", "user-1"], "pref-units");
item?.value; // { text: "Prefers metric units" }

const all = await store.search(["memories", "user-1"]);
// every item under that namespace (prefix match)

await store.delete(["memories", "user-1"], "pref-units");
```

- `put` with an existing namespace and key **overwrites**. That's how you update a fact.
- `search` takes a namespace *prefix*, so `["memories"]` would return every user's items. Scope namespaces carefully (more below).
- `search` also accepts options such as `filter` and `limit`. A store configured with an embeddings model can additionally support semantic queries; see the store docs for the config in your version.

`InMemoryStore` disappears on restart. For production use a persistent store implementation; check what your installed packages offer (the Postgres package family is the usual home).

## Using the store in a graph

Compile with the store and every node can reach it through `config.store`:

```ts
import { randomUUID } from "node:crypto";
import {
  StateGraph,
  Annotation,
  MemorySaver,
  InMemoryStore,
  START,
  END,
  type LangGraphRunnableConfig,
} from "@langchain/langgraph";

const State = Annotation.Root({
  message: Annotation<string>(),
  memories: Annotation<string[]>(),
  reply: Annotation<string>(),
  saved: Annotation<boolean>(),
});

const recall = async (
  _s: typeof State.State,
  config: LangGraphRunnableConfig,
) => {
  const userId = config.configurable?.userId as string;
  const items = (await config.store?.search(["memories", userId])) ?? [];
  return { memories: items.map((i) => i.value.text as string) };
};

const respond = (s: typeof State.State) => ({
  // a model call would go here, with s.memories included in the prompt
  reply: `Known about you: ${s.memories.join("; ") || "nothing yet"}`,
});

const remember = async (
  s: typeof State.State,
  config: LangGraphRunnableConfig,
) => {
  const userId = config.configurable?.userId as string;
  const isFact = s.message.toLowerCase().startsWith("remember:");
  if (isFact) {
    await config.store?.put(["memories", userId], randomUUID(), {
      text: s.message.slice("remember:".length).trim(),
    });
  }
  return { saved: isFact };
};

const graph = new StateGraph(State)
  .addNode("recall", recall)
  .addNode("respond", respond)
  .addNode("remember", remember)
  .addEdge(START, "recall")
  .addEdge("recall", "respond")
  .addEdge("respond", "remember")
  .addEdge("remember", END)
  .compile({ checkpointer: new MemorySaver(), store: new InMemoryStore() });

const thread = (id: string) => ({
  configurable: { thread_id: id, userId: "user-1" },
});

await graph.invoke({ message: "remember: I like chai" }, thread("a"));
// reply: "Known about you: nothing yet"

await graph.invoke({ message: "what do you know?" }, thread("b"));
// reply: "Known about you: I like chai"   ← different thread, same user
```

The second call runs on a brand-new thread, yet it sees the memory from the first. That cross-thread reach is the point of the store.

Notice that the `userId` comes from `config.configurable`, not from state: it describes *who is calling*, not data the graph evolves.

## Designing memory

**Namespace by owner.** `["memories", userId]` keeps each user's data separate. Anything that lists or searches a broader prefix can leak one user's memories into another's context. Derive `userId` from your authenticated session, never from user-supplied text ([Security](../05-production/03-security.md)).

**Decide when to write.**

- *In the node, during the run* ("hot path"): simple and immediate, but adds latency and can clutter the conversation flow.
- *After the response*, in a separate node or background job: keeps replies fast, at the cost of the memory not being available until later.

**Decide what to store.** Short, self-contained facts ("Prefers metric units") retrieve far better than raw transcripts. Typical categories are stable facts about the user, past examples worth reusing, and instructions the user has given.

**Keep memory healthy.** Facts go stale and duplicate. Reuse a key to update a fact rather than adding a near-duplicate, delete items the user retracts, and consider a periodic cleanup pass.

**Mind the prompt budget.** Loading every memory into every prompt doesn't scale. Limit results or use semantic search to pull only what's relevant to the current message.

## Common mistakes

- **Using the checkpointer for cross-thread facts.** A new `thread_id` starts empty; only the store carries over.
- **Forgetting `store` in `compile`.** `config.store` is then undefined; the optional chaining (`?.`) silently does nothing, which looks like "memory isn't saving."
- **Namespaces that don't match.** `["memories", "user-1"]` and `["memory", "user-1"]` are different places.
- **Broad searches.** `search(["memories"])` returns everyone's items.
- **Storing secrets or sensitive data casually.** Treat the store like any user-data database.

## Debugging

- Call `store.search([...])` directly (outside the graph) to see exactly what's saved.
- Log the namespace you read from and the one you write to; mismatches are the usual cause of empty recalls.
- Check `config.store` is defined at the top of a node when memory seems inert.

## Quick summary

- The **store** is cross-thread, explicit, key-value memory; the checkpointer is per-thread, automatic state.
- Items live at `namespace + key`; `put` overwrites, `search` matches namespace prefixes.
- Pass the store to `compile({ store })` and use it via `config.store` in nodes.
- Namespace by user, store compact facts, and keep growth and staleness under control.
- `InMemoryStore` is for development; use a persistent store in production.

**Next:** [Human-in-the-Loop](./03-human-in-the-loop.md)
