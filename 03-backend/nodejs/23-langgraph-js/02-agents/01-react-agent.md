# ReAct Agent

A ReAct agent is a loop: the model looks at the conversation, either answers or asks to call tools, your code runs the tools, and the results go back to the model. It repeats until the model answers without requesting a tool. In LangGraph this is a small graph with one cycle, built from pieces you already know.

Prerequisites: [State and Reducers](../01-core/02-state-and-reducers.md) (`MessagesAnnotation`), [Routing and Parallelism](../01-core/04-routing-and-parallelism.md) (conditional edges and loops).

## The shape

```
START ─► agent ──► has tool calls? ──yes──► tools ─┐
          ▲              │                         │
          │              no                        │
          │              ▼                         │
          │             END                        │
          └────────────────────────────────────────┘
```

Two nodes and one conditional edge:

- **`agent`** calls the chat model with the message history and appends its reply.
- **`tools`** executes every tool call in that reply and appends the results.
- **Router** after `agent` sends control to `tools` if the reply contains tool calls, otherwise to `END`.

The model never executes anything. It only emits *requests* (`tool_calls` on an `AIMessage`). Your `tools` node does the running.

## Building it by hand

This example needs an API key (`ANTHROPIC_API_KEY`); any LangChain chat model that supports tool calling works the same way.

```bash
npm install @langchain/langgraph @langchain/core @langchain/anthropic zod
```

```ts
import { ChatAnthropic } from "@langchain/anthropic";
import { tool } from "@langchain/core/tools";
import { AIMessage, SystemMessage } from "@langchain/core/messages";
import {
  StateGraph,
  MessagesAnnotation,
  START,
  END,
} from "@langchain/langgraph";
import { ToolNode } from "@langchain/langgraph/prebuilt";
import { z } from "zod";

const getWeather = tool(
  async ({ city }) => `It is 24°C and sunny in ${city}.`,
  {
    name: "get_weather",
    description: "Get the current weather for a city.",
    schema: z.object({ city: z.string().describe("City name") }),
  },
);

const tools = [getWeather];
const model = new ChatAnthropic({ model: "claude-sonnet-5-5" }).bindTools(tools);

const callModel = async (state: typeof MessagesAnnotation.State) => {
  const response = await model.invoke([
    new SystemMessage("You are a concise assistant. Use tools when needed."),
    ...state.messages,
  ]);
  return { messages: [response] };
};

const route = (state: typeof MessagesAnnotation.State) => {
  const last = state.messages.at(-1) as AIMessage;
  return last.tool_calls?.length ? "tools" : END;
};

const graph = new StateGraph(MessagesAnnotation)
  .addNode("agent", callModel)
  .addNode("tools", new ToolNode(tools))
  .addEdge(START, "agent")
  .addConditionalEdges("agent", route, ["tools", END])
  .addEdge("tools", "agent")
  .compile();

const result = await graph.invoke({
  messages: [{ role: "user", content: "What's the weather in Pune?" }],
});
console.log(result.messages.at(-1)?.content);
```

Choices worth noting:

- **`bindTools(tools)`** attaches the tool names, descriptions and schemas to every model call. That metadata is all the model has to decide *whether* and *how* to call a tool.
- **The system prompt is prepended inside the node**, not stored in state. It is re-applied on every call and never piles up in the history.
- **`ToolNode`** (from `@langchain/langgraph/prebuilt`) reads the tool calls off the last `AIMessage`, runs them, and returns one `ToolMessage` per call. The `messages` reducer appends them.
- The router is hand-written to show the mechanism. The prebuilt `toolsCondition` does the same check.

## What the messages look like

```
HumanMessage   "What's the weather in Pune?"
AIMessage      tool_calls: [{ name: "get_weather", args: { city: "Pune" }, id: "call_1" }]
ToolMessage    "It is 24°C and sunny in Pune."          (tool_call_id: "call_1")
AIMessage      "It's 24°C and sunny in Pune."           ← no tool_calls → END
```

Each tool result is linked to its request by `tool_call_id`. Providers reject histories where a tool call has no matching result, or the reverse.

## Behavior to know

**Parallel tool calls.** A model can request several tools in one reply. `ToolNode` runs them concurrently and returns a `ToolMessage` for each.

**Tool errors.** `ToolNode` catches errors thrown by a tool and returns them to the model as a tool message, so the model can adjust and retry (controlled by its `handleToolErrors` option). Throwing is therefore a reasonable way for a tool to say "bad input".

**Loop budget.** One round trip is two supersteps (`agent`, then `tools`). With the default recursion limit of 25, that is roughly a dozen tool rounds before `GraphRecursionError`. Set it per call:

```ts
await graph.invoke(input, { recursionLimit: 50 });
```

Treat hitting the limit as a signal: the model may be stuck repeating a failing call. Raising the limit hides that.

**Memory across turns.** `invoke` with no checkpointer starts fresh every time. To continue a conversation, compile with a checkpointer and pass a `thread_id`. See [Persistence](../03-stateful/01-persistence.md).

## Prebuilt vs hand-built

LangGraph and LangChain ship a high-level agent helper that builds this same graph for you. In 1.x the recommended entry point is `createAgent` from the `langchain` package; `createReactAgent` in `@langchain/langgraph/prebuilt` is the older equivalent. Option names differ between versions, so check the docs for the one you install.

| Use the prebuilt helper when | Build the graph yourself when |
|---|---|
| The standard model-and-tools loop is enough | You need extra state keys beyond `messages` |
| You want to ship quickly | You need custom routing, an approval step ([Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md)), or pre/post-processing nodes |
| | You want agents as parts of a larger graph ([Multi-Agent](../04-patterns/02-multi-agent.md)) |

Everything those helpers do is expressible with the pieces above, which is why this note builds it by hand.

## Common mistakes

- **Vague tool descriptions.** The model chooses tools from the name and description. "Does stuff with data" gets misused or ignored; say what the tool does, when to use it, and what the arguments mean (`.describe(...)` on schema fields).
- **Breaking tool-call pairs when trimming history.** If you remove an `AIMessage` that has tool calls, remove its `ToolMessage`s too, and vice versa. A lone one makes the next model call fail.
- **Forgetting `bindTools`.** The model answers in prose and never calls anything.
- **Putting the system prompt into state on every turn.** It duplicates in history. Prepend it in the node.
- **Returning non-string tool output carelessly.** Return a string (or serialize objects with `JSON.stringify`) so the model sees something it can read.

## Debugging

- Stream updates and watch the `agent` / `tools` alternation:

  ```ts
  for await (const chunk of await graph.stream(input, { streamMode: "updates" })) {
    console.log(JSON.stringify(chunk, null, 2));
  }
  ```

- Inspect `tool_calls` on each `AIMessage`: wrong tool or wrong arguments means fix the description or schema first, not the loop.
- If the run ends immediately with no tool use, check that `bindTools` was applied to the model your node actually calls.
- If it loops forever, look at what the `ToolMessage` says. Often the tool is returning an error the model can't act on.

## Quick summary

- A ReAct agent is `agent` ⇄ `tools` joined by a conditional edge that checks for `tool_calls`.
- The model *requests* tools; `ToolNode` *runs* them and appends `ToolMessage`s.
- State is just `MessagesAnnotation`; tool results are linked to requests by `tool_call_id`.
- Each tool round costs two supersteps, so the recursion limit bounds the number of rounds.
- Prebuilt helpers build the same graph; hand-build when you need custom state or control flow.

**Next:** [Streaming](./02-streaming.md)