# Streaming

Agents are slow: model calls take seconds and tool loops take longer. Streaming lets you show progress while the graph runs instead of waiting for `invoke` to return. LangGraph streams at several granularities, chosen with `streamMode`.

Prerequisites: [Concepts](../01-core/01-concepts.md) (supersteps), [ReAct Agent](./01-react-agent.md).

## Basics

`stream` takes the same input and config as `invoke`, plus a `streamMode`. It returns a **promise** of an async iterable, so you `await` it and then iterate:

```ts
for await (const chunk of await graph.stream(input, { streamMode: "updates" })) {
  console.log(chunk);
}
```

Forgetting that inner `await` is the classic first bug: the loop fails because a `Promise` isn't async iterable.

## Stream modes

| Mode | Each chunk is | Use it for |
|---|---|---|
| `"values"` | The full state after each superstep | Snapshots, simple UIs that re-render from state |
| `"updates"` | `{ nodeName: update }` for each node that ran | Progress ("searching…", "calling tool…"), logging |
| `"messages"` | `[messageChunk, metadata]`, one per LLM token | Typing effect in chat UIs |
| `"custom"` | Whatever your node chooses to emit | Progress from inside long-running nodes |
| `"debug"` | Detailed execution events | Debugging only |

### `updates`

What each node just returned, labeled by node name:

```ts
// { agent: { messages: [AIMessage(...)] } }
// { tools: { messages: [ToolMessage(...)] } }
// { agent: { messages: [AIMessage("It's 24°C and sunny.")] } }
```

This is the cheapest way to show "which step is the agent on".

### `values`

The whole state each time. Simple, but with a growing `messages` list every chunk repeats everything so far, so avoid it for long conversations over a network.

### `messages`: token streaming

```ts
const textOf = (content: unknown): string =>
  typeof content === "string"
    ? content
    : Array.isArray(content)
      ? content.map((b) => (b?.type === "text" ? b.text : "")).join("")
      : "";

for await (const [msg, meta] of await graph.stream(input, {
  streamMode: "messages",
})) {
  if (meta.langgraph_node !== "agent") continue; // ignore other nodes' models
  process.stdout.write(textOf(msg.content));
}
```

- `meta.langgraph_node` tells you which node's model call produced the token, which is how you filter when several nodes use models.
- `content` is a string for some providers and an **array of content blocks** for others (Anthropic returns blocks, and tool-use chunks may appear among them). The helper above handles both; don't assume a string.
- Tokens are emitted for LangChain chat models called inside nodes. A node that calls a provider SDK directly won't appear here; stream those with `custom` mode.

### `custom`: your own events

A node can emit arbitrary data through `config.writer`:

```ts
import type { LangGraphRunnableConfig } from "@langchain/langgraph";

const crawl = async (state: typeof State.State, config: LangGraphRunnableConfig) => {
  for (let i = 1; i <= 3; i++) {
    config.writer?.({ progress: `page ${i}/3` });
    await fetchPage(i);
  }
  return { done: true };
};

for await (const chunk of await graph.stream(input, { streamMode: "custom" })) {
  console.log(chunk); // { progress: "page 1/3" } ...
}
```

Use it for progress inside a single long node, since `updates` only reports when a node *finishes*. The writer is optional (`?.`) because it's absent when the graph isn't being streamed in `custom` mode.

## Combining modes

Pass an array and each chunk becomes a `[mode, data]` tuple:

```ts
for await (const [mode, data] of await graph.stream(input, {
  streamMode: ["updates", "messages"],
})) {
  if (mode === "messages") {
    const [msg] = data;
    // append tokens to the UI
  } else {
    // data is { nodeName: update }, so show step progress
  }
}
```

A typical chat UI wants exactly this: `messages` for the typing effect and `updates` for status lines like "running get_weather".

## Related options

- **Subgraphs.** Pass `subgraphs: true` in the stream options to include events from nested graphs; chunks then carry a namespace identifying where they came from. See [Subgraphs](../04-patterns/01-subgraphs.md).
- **Cancellation.** Pass an `AbortSignal` in the config and abort when the client disconnects:

  ```ts
  const ac = new AbortController();
  const stream = await graph.stream(input, {
    streamMode: "updates",
    signal: ac.signal,
  });
  // later: ac.abort();
  ```

- **`streamEvents`.** The lower-level LangChain event stream (`graph.streamEvents(input, { version: "v2" })`) yields events such as `on_chat_model_stream`. Reach for it when you need events from specific runnables deep inside; for most apps the modes above are simpler.

## Streaming to a client

Whatever transport you use (SSE, WebSocket), each chunk must be JSON-serializable. Message objects carry class instances, so convert what you send (a plain `{ type, text }` shape is usually enough) rather than serializing internals. Wiring this to a server is covered in [Deployment](../05-production/04-deployment.md).

## Common mistakes

- **`for await (... of graph.stream(...))` without `await`.** Needs `await graph.stream(...)`.
- **Using `invoke` and expecting tokens.** Only `stream` surfaces them.
- **Assuming `msg.content` is a string.** It can be an array of blocks.
- **Expecting `messages` mode to capture raw-SDK calls.** Emit those with `config.writer` and use `custom`.
- **Streaming `values` for long chats.** Each chunk resends the full history; prefer `updates`.
- **Treating `updates` as live.** It fires when a node *completes*; a slow node looks like a stall. Use `custom` or `messages` for finer progress.

## Debugging

- Log the raw chunk shape first (`console.log(JSON.stringify(chunk))`); most "nothing renders" issues are a shape mismatch, especially tuples when `streamMode` is an array.
- Filter by `meta.langgraph_node` to see which node produced which tokens.
- Swap to `"debug"` temporarily to see the superstep-level events when a graph behaves unexpectedly.

## Quick summary

- `stream` = `invoke` plus granularity; remember `await graph.stream(...)` before `for await`.
- `updates` for step progress, `messages` for tokens, `custom` for in-node progress, `values` for full snapshots.
- An array of modes yields `[mode, data]` tuples.
- `content` may be string or content blocks; filter tokens by `langgraph_node`.
- Cancel with an `AbortSignal`; serialize deliberately when sending chunks to a client.

**Next:** [Persistence](../03-stateful/01-persistence.md): checkpointers, threads, and continuing a conversation across calls.