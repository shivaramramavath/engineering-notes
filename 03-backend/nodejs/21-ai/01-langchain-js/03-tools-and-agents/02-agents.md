# Agents

An **agent** is a model running in a loop with tools: it decides what to do, calls a tool, reads the result, and repeats until it can answer. A chain follows steps *you* wrote. An agent chooses its own steps at runtime.

> Checked against the LangChain JS 1.x docs (`langchain@1.5.15`). Agents are built on LangGraph underneath. The middleware API is the newest part of the library, so verify option names against the current docs.

**Prerequisites:** [Tools](./01-tools.md), [Chat History](../01-core/07-chat-history.md)

---

## The loop

```text
          ┌──────────────────────────────┐
          ▼                              │
user ─► model ──(tool calls?)── yes ─► tools ──┘
          │
          no
          ▼
      final answer
```

Each pass: the model sees the messages so far, either answers or requests tool calls, and the results are appended before the next pass. This is exactly the manual loop from the Tools note, automated.

---

## createAgent

```ts
import { createAgent, tool } from "langchain";
import * as z from "zod";

const getWeather = tool(
  async ({ city }) => `It's always sunny in ${city}.`,
  {
    name: "get_weather",
    description: "Get the weather for a city.",
    schema: z.object({ city: z.string() }),
  },
);

const agent = createAgent({
  model: "gpt-5-nano",            // string or a model instance
  tools: [getWeather],
  systemPrompt: "You are a concise weather assistant.",
});

const result = await agent.invoke({
  messages: [{ role: "user", content: "What's the weather in SF?" }],
});

console.log(result.messages.at(-1)?.content);
```

- Input is `{ messages: [...] }`. Output is the final **state**; `result.messages` holds the whole conversation including tool calls and results.
- The last message is normally the answer.
- `model` accepts a `"provider:model"` string or an instance (`ChatOpenAI`, ...) when you need custom parameters.

### Options

| Option | Purpose |
|---|---|
| `model` | The chat model |
| `tools` | Tools the agent may call |
| `systemPrompt` | Instructions (string or `SystemMessage`) |
| `responseFormat` | Zod/JSON schema for a validated final result in `result.structuredResponse` (see [Structured Output](../01-core/05-structured-output.md)) |
| `checkpointer` | Persists state per thread for multi-turn memory |
| `stateSchema` | Extra state fields beyond `messages` |
| `contextSchema` | Typed per-run data (user id, etc.) passed separately from state |
| `middleware` | Hooks that customize or guard the loop |
| `name` | Identifier, useful in multi-agent setups |

---

## Memory across turns

Add a checkpointer and pass a `thread_id`:

```ts
import { MemorySaver } from "@langchain/langgraph";

const agent = createAgent({ model: "gpt-5-nano", tools: [getWeather], checkpointer: new MemorySaver() });

const config = { configurable: { thread_id: "session-123" } };
await agent.invoke({ messages: [{ role: "user", content: "I'm in Paris." }] }, config);
await agent.invoke({ messages: [{ role: "user", content: "Weather here?" }] }, config);
```

`MemorySaver` is for development. Use a database-backed checkpointer in production. Trimming and summarizing long histories are covered in [Chat History](../01-core/07-chat-history.md).

---

## Per-run context

Data that belongs to *this request* but not to the conversation (user id, API handles) goes in `context`, not in the messages:

```ts
await agent.invoke(
  { messages: [{ role: "user", content: "Show my orders" }] },
  { context: { userId: "user-123" } },
);
```

Tools read it through their runtime object (see [Tools](./01-tools.md)). This is how you keep identity out of anything the model can edit.

---

## Middleware: customizing the loop

Middleware hooks into specific points of the loop. Custom middleware is created with `createMiddleware`. Hooks shown in the docs:

| Hook | Runs |
|---|---|
| `beforeModel` | Before each model call (trim history, inject state) |
| `afterModel` | After each model call (validate or filter the response) |
| `wrapModelCall` | Around the model call (swap models, edit the request) |
| `wrapToolCall` | Around each tool call (catch errors, log, block) |

Built-in middleware covers common needs:

| Middleware | Purpose |
|---|---|
| `modelCallLimitMiddleware` | Cap model calls (`threadLimit`, `runLimit`) to stop runaway loops and cost |
| `toolCallLimitMiddleware` | Cap tool calls, optionally per tool |
| `modelRetryMiddleware` / `toolRetryMiddleware` | Retry failed calls with exponential backoff |
| `humanInTheLoopMiddleware` | Pause for human approve, edit or reject on chosen tools |
| `summarizationMiddleware` | Summarize history when it approaches a token limit |

```ts
import {
  createAgent,
  modelCallLimitMiddleware,
  toolRetryMiddleware,
  humanInTheLoopMiddleware,
} from "langchain";

const agent = createAgent({
  model: "gpt-5-nano",
  tools: [searchTool, sendEmailTool],
  checkpointer,
  middleware: [
    modelCallLimitMiddleware({ runLimit: 5 }),
    toolRetryMiddleware({ maxRetries: 3 }),
    humanInTheLoopMiddleware({ interruptOn: { send_email: true } }),
  ],
});
```

Option names above come from the docs' examples. Check the middleware reference for the full set and exact tool-name keys. Pausing for human approval needs a checkpointer so the run can be resumed.

### Dynamic model selection

Route easy conversations to a cheaper model by swapping the model inside `wrapModelCall`:

```ts
const dynamicModel = createMiddleware({
  name: "DynamicModel",
  wrapModelCall: (request, handler) =>
    handler({ ...request, model: request.messages.length > 10 ? bigModel : smallModel }),
});
```

---

## Streaming and structured results

- Stream progress, tokens and tool calls with `agent.stream(...)` and a `streamMode`, or the newer event-streaming API. See [Streaming](../01-core/06-streaming.md).
- Set `responseFormat` when downstream code needs a typed object instead of prose; read `result.structuredResponse`.

---

## Agent or chain?

| Situation | Use |
|---|---|
| Steps are known in advance (summarize, then classify, then format) | A chain or plain code |
| One retrieval then an answer | 2-step RAG ([Retrieval and RAG](../02-rag/03-retrieval-and-rag.md)) |
| Number and order of steps depends on the input; several tools; may need to retry or search again | Agent |
| Complex branching, parallel workers, explicit state machine | A LangGraph graph (what `createAgent` is built on) |

Agents buy flexibility at the price of **more model calls, higher latency, higher cost and less predictability**. Don't use one when a fixed pipeline would do. If you need the underlying graph, the agent object exposes it as `agent.graph`.

---

## Production concerns (short version)

- **Bound the loop.** Always set limits (`modelCallLimitMiddleware`, tool limits). An unbounded agent can loop and burn tokens.
- **Least privilege tools** and approval for destructive actions.
- **Trace everything** in LangSmith. Agent bugs are almost always visible in the sequence of tool calls.
- **Test with real transcripts**, including adversarial and out-of-scope inputs. See `04-production/02-testing-and-evaluation.md`.
- **Retrieved or tool-returned text is untrusted** and can carry prompt injection. See `04-production/04-security.md`.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| No limits on the loop | Add model and tool call limits |
| Forgetting `thread_id` on follow-ups | Pass the same config each turn |
| `MemorySaver` in production | Persistent checkpointer |
| Passing user identity in the prompt or as tool args | Use `context` and read it in tools |
| Too many or overlapping tools | Fewer, clearer tools |
| Using an agent for a fixed pipeline | Use a chain or plain code |
| Assuming the final message is always a string | Check the content type, or use `responseFormat` |
| Giving destructive tools with no approval step | Add `humanInTheLoopMiddleware` |
| Debugging by reading only the final answer | Read `result.messages` or the trace for the full tool sequence |

### Debugging

1. Print `result.messages` and read the tool calls and results in order.
2. Look for: wrong tool chosen (fix descriptions), bad arguments (fix schema descriptions), tool errors the model ignored (return actionable errors), loops (add limits, tighten the prompt).
3. Reproduce a failing tool call by invoking the tool directly.
4. Open the run in LangSmith to see the exact prompts the model received at each step.

---

## Quick Summary

- An agent is a model plus tools in a loop; `createAgent({ model, tools, systemPrompt })` builds one.
- Input `{ messages }`, output state with `result.messages`; add `responseFormat` for typed results.
- Memory = `checkpointer` + `thread_id`; per-request data = `context`.
- Middleware (`beforeModel`, `afterModel`, `wrapModelCall`, `wrapToolCall`, plus built-ins for limits, retries, approvals, summarization) shapes and guards the loop.
- Prefer a chain when steps are known; use an agent when the path depends on the input, and always bound it.

**Next:** [Tracing and Debugging](../04-production/01-tracing-and-debugging.md)