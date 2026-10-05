# Streaming

LLM responses take seconds. Streaming lets you show output as it is generated, which makes an app feel fast even when total latency is unchanged. In LangChain there are three levels: a **model**, a **chain**, and an **agent**.

> Checked against the LangChain JS 1.x docs (`langchain@1.5.15`). The agent streaming API has changed recently, so verify agent-level details against the current docs.

**Prerequisites:** [Models and Messages](./02-models-and-messages.md), [Runnables and LCEL](./04-runnables-and-lcel.md)

---

## Streaming a model

```ts
const stream = await model.stream("Why do parrots have colorful feathers?");

for await (const chunk of stream) {
  process.stdout.write(chunk.text);
}
```

Each `chunk` is an **`AIMessageChunk`**: a piece of an `AIMessage`. Chunks are designed to be added together:

```ts
import type { AIMessageChunk } from "@langchain/core/messages";

let full: AIMessageChunk | undefined;
for await (const chunk of await model.stream("Tell me a joke")) {
  full = full ? full.concat(chunk) : chunk;
}
console.log(full?.text);
```

The accumulated result behaves like what `invoke` would have returned, so you can append it to history and continue the conversation.

---

## Not just text

Chunks can carry more than text. Iterate `contentBlocks` to handle reasoning, tool-call fragments and text uniformly:

```ts
for await (const chunk of await model.stream("What color is the sky?")) {
  for (const block of chunk.contentBlocks) {
    if (block.type === "reasoning") process.stdout.write(`[thinking] ${block.reasoning}`);
    else if (block.type === "text") process.stdout.write(block.text);
    else if (block.type === "tool_call_chunk") { /* partial tool call */ }
  }
}
```

Reasoning output only appears if the model supports it and it is enabled.

### Streaming tool calls

Tool calls arrive as `tool_call_chunk`s with **partial JSON** in `args`. Don't `JSON.parse` a single chunk. Concatenate chunks first (`full.concat(chunk)`) and read `full.tool_calls` at the end, or when the call is complete.

---

## Streaming a chain

A chain built with `.pipe()` streams end to end:

```ts
const chain = prompt.pipe(model).pipe(new StringOutputParser());

for await (const piece of await chain.stream({ question: "Explain closures" })) {
  process.stdout.write(piece);
}
```

Chunks flow through each step. If a step in the middle needs the complete input (for example, a function that processes the whole string), the stream stalls at that step and you get one lump. See the mistakes table.

---

## streamEvents: when you need to know which step emitted

`stream` gives you the final step's output. `streamEvents` gives a firehose of **typed events** from every step, useful for showing intermediate progress or isolating one component's tokens:

```ts
const events = await model.streamEvents("Hello");
for await (const event of events) {
  if (event.event === "on_chat_model_start") console.log("started");
  if (event.event === "on_chat_model_stream") process.stdout.write(event.data.chunk.text);
  if (event.event === "on_chat_model_end") console.log("\ndone");
}
```

Event names above come from the docs' chat-model example. For chains, events also carry the name/tags of the step that produced them, which you can filter on. `streamEvents` on chat models, chains and agents differs in options, so check the current reference for the one you use.

---

## Streaming an agent

Agents produce several kinds of things: model tokens, tool calls, tool results, state updates. The classic API picks a **stream mode**:

| `streamMode` | Yields |
|---|---|
| `"updates"` | State update after each agent step (model step, tool step, ...) |
| `"messages"` | `[token, metadata]` tuples: LLM tokens plus which node produced them |
| `"custom"` | Arbitrary data your tools emit through a writer |

```ts
for await (const [token, metadata] of await agent.stream(
  { messages: [{ role: "user", content: "weather in SF?" }] },
  { streamMode: "messages" },
)) {
  console.log(metadata.langgraph_node, token.contentBlocks);
}
```

You can pass an array of modes (`["updates", "messages", "custom"]`); output then arrives as `[mode, chunk]` pairs.

For new code the docs recommend **event streaming** (introduced in LangChain v1.3), which exposes separate typed iterators (messages, tool calls, final output) instead of branching on modes:

```ts
const stream = await agent.streamEvents(input, { ...config, version: "v3" });
// then consume stream.messages, stream.toolCalls, await stream.output
```

I'd treat the exact shape of this API as the thing to re-check in the docs before relying on it; it is the newest part of the library.

### Custom progress from tools

A tool can emit its own updates ("fetched 10/100 records") through the config's `writer`, and you read them with `streamMode: "custom"`. Note: a tool that requires `writer` can't be called outside a graph run without supplying one.

---

## Turning streaming off

Sometimes you don't want a particular model's tokens streamed to clients (hidden sub-agents, deployments where only the final answer should show):

```ts
new ChatOpenAI({ model: "gpt-5-nano", streaming: false });
// or, on any chat model:
// disableStreaming: true
```

Not all integrations support `streaming`; `disableStreaming` is on the base class.

---

## Streaming to a browser

Typical Node setup: stream chunks over **Server-Sent Events** or a streamed `Response`, and render incrementally on the client.

```ts
// minimal Node/Web-standard shape
export async function POST(req: Request) {
  const { question } = await req.json();
  const stream = await chain.stream({ question });

  const body = new ReadableStream({
    async start(controller) {
      const enc = new TextEncoder();
      for await (const piece of stream) controller.enqueue(enc.encode(piece));
      controller.close();
    },
  });
  return new Response(body, { headers: { "Content-Type": "text/plain; charset=utf-8" } });
}
```

Serving details (SSE framing, abort handling, backpressure) are in `04-production/05-serving-and-integration.md`.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Forgetting `await` before `.stream()` | `for await (const c of await model.stream(...))` |
| `JSON.parse` on a tool-call chunk | Args are partial. Concatenate chunks first |
| Reading `chunk.content` as a string | Use `chunk.text`, or iterate `contentBlocks` |
| Chain "streams" but arrives in one piece | A middle step needs the full input (lambda, non-streaming parser). Make it chunk-aware or move it after the streamed part |
| Not handling client disconnect | Pass an abort `signal` in the config and cancel on disconnect to stop paying for tokens |
| Streaming structured output and expecting valid JSON mid-stream | Partial JSON is incomplete until the end; parse the final result |
| Assuming every model streams tokens | Some integrations or settings don't; check `streaming` / `disableStreaming` |

---

## Quick Summary

- `model.stream()` yields `AIMessageChunk`s; read `.text`, iterate `.contentBlocks`, combine with `.concat()`.
- A `.pipe()` chain streams end to end only if every step passes chunks along.
- `streamEvents` gives typed events from every step; use it to see or filter intermediate progress.
- Agents stream by mode (`updates`, `messages`, `custom`) or via the newer event-streaming API.
- Tool-call args arrive as partial JSON; accumulate before parsing.

**Next:** [Chat History](./07-chat-history.md)
