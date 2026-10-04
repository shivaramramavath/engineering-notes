# Tools

A **tool** is a function the model can ask you to run. The model never executes anything itself. It reads each tool's name, description and input schema, and when useful it replies with a **request**: "call `get_weather` with `{ location: "Paris" }`". Your code (or an agent) runs the function and sends the result back.

> Checked against the LangChain JS 1.x docs (`langchain@1.5.15`).

**Prerequisites:** [Models and Messages](../01-core/02-models-and-messages.md), [Structured Output](../01-core/05-structured-output.md) (tools use the same schema machinery)

---

## Defining a tool

```ts
import { tool } from "langchain";
import * as z from "zod";

const getWeather = tool(
  async ({ location }) => `It's sunny in ${location}.`,
  {
    name: "get_weather",
    description: "Get the current weather for a city.",
    schema: z.object({
      location: z.string().describe("City name, e.g. 'Paris'"),
    }),
  },
);
```

A tool is **a function plus metadata**:

| Piece | What the model does with it |
|---|---|
| `name` | Identifies the tool in its request. Use `snake_case`; spaces and special characters cause problems across providers |
| `description` | The main signal for *when* to use the tool. Write it like instructions to a new colleague |
| `schema` (Zod) | Defines the arguments. `.describe()` on each field tells the model what to put there |
| function | What runs when the model's request is executed |

Arguments are validated against the schema before your function runs.

> The description and field descriptions are effectively a prompt. Vague ones ("does stuff", `data: z.string()`) are the most common reason a model picks the wrong tool or fills arguments badly.

---

## The tool-calling loop by hand

This is what an agent automates. Understanding it makes agents easy to debug.

```ts
const modelWithTools = model.bindTools([getWeather]);
const messages = [{ role: "user", content: "What's the weather in Boston?" }];

// 1. Model decides to call a tool
const ai = await modelWithTools.invoke(messages);
messages.push(ai);

// 2. You execute each requested call
for (const call of ai.tool_calls ?? []) {
  const toolMessage = await getWeather.invoke(call); // returns a ToolMessage
  messages.push(toolMessage);
}

// 3. Send results back; the model writes the final answer
const final = await modelWithTools.invoke(messages);
console.log(final.text);
```

```text
user ─► model ─► AIMessage(tool_calls)
                      │ you run the tool(s)
                      ▼
               ToolMessage(s) ─► model ─► final AIMessage
```

Details worth knowing:

- Invoking a tool with the **tool call object** returns a `ToolMessage` with the right `tool_call_id`, ready to append. Invoking with plain arguments returns the raw result.
- A model may request **several tool calls at once** (parallel calls). Run them all and return one `ToolMessage` per call.
- The `AIMessage` containing the calls must stay in the history, followed by its `ToolMessage`s. Dropping either breaks the next request.
- You can steer the model: `bindTools([t], { toolChoice: "any" })` forces some tool call, and `toolChoice: "tool_name"` forces a specific one.

---

## What a tool should return

| Return | The model receives |
|---|---|
| String | Text to reason over. The safest default |
| Object | Structured data it can inspect |
| Content blocks | Text, images or other media |
| `Command` | Updates agent state directly (must include a matching `ToolMessage`) |

Return what helps the **next model step**: concise, relevant, and clearly formatted. Don't dump a 50 KB API response. Trim it, summarize it, or paginate.

`ToolMessage` also supports an `artifact` for data you want to keep programmatically without sending it to the model (e.g. raw results or document ids behind a text summary).

### returnDirect

`returnDirect: true` ends the agent loop right after the tool runs and returns its output without another model call. Good for tools whose result *is* the answer. Don't use it when the result needs further reasoning. If several tool calls happen in parallel and only some use `returnDirect`, control goes back to the model.

---

## Runtime access inside a tool

Tools can receive a runtime object as a second parameter. It is **hidden from the model's view of the schema**:

| Field | Gives you |
|---|---|
| `context` | Per-run immutable data you passed in (user id, tenant, API handles) |
| `state` | Current agent state, e.g. messages and custom fields |
| `store` | Persistent long-term memory across conversations |
| `writer` | Emit progress updates for streaming |
| `toolCallId` | Id of the current call (needed when returning a `Command`) |
| `executionInfo` | Thread id, run id, retry state |

```ts
import { tool, type ToolRuntime } from "langchain";

const myOrders = tool(
  async (_input, runtime: ToolRuntime) => {
    const userId = runtime.context?.userId; // trusted, set by your server
    return await db.ordersFor(userId);
  },
  { name: "my_orders", description: "List the current user's orders.", schema: z.object({}) },
);
```

**Why this matters for security:** identity, tenant and permissions should come from `context` (set by your code), **never** from tool arguments the model fills in. A model can be tricked into passing someone else's `userId`; it cannot override your server-side context.

For exact typing of `ToolRuntime` and context schemas, see the agents note and the current reference.

---

## Errors

Tools fail: network errors, bad input, upstream outages. The docs' pattern is middleware that wraps tool execution and turns exceptions into a `ToolMessage`, so the model sees the error and can adapt instead of the whole run crashing:

```ts
import { createMiddleware, ToolMessage } from "langchain";

const handleToolErrors = createMiddleware({
  name: "HandleToolErrors",
  wrapToolCall: async (request, handler) => {
    try {
      return await handler(request);
    } catch (error) {
      return new ToolMessage({
        content: `Tool error: ${String(error)}`,
        tool_call_id: request.toolCall.id!,
      });
    }
  },
});
```

Retries for transient failures are available as `toolRetryMiddleware` (see [Agents](./02-agents.md)).

Make error messages **actionable** ("city not found; try a larger nearby city") because the model reads them and decides what to do next.

---

## Provider built-in tools

Some providers run tools on their own servers (web search, code interpreters). You enable them on the model, and the results appear as content blocks in the response, with no `ToolMessage` for you to send back:

```ts
const model = await initChatModel("gpt-5-nano");
const withSearch = model.bindTools([{ type: "web_search" }]);
```

Which built-ins exist, and their option shape, depends on the provider. Check its integration page.

---

## Designing good tools

- **Few, well-named tools beat many overlapping ones.** Models choose worse as the list grows and similar tools blur together.
- **One clear job per tool**, with a description that says when to use it *and when not to*.
- **Small, flat schemas.** Use enums for fixed choices. Describe every field.
- **Return compact, useful text.** Include identifiers the model may need next.
- **Make tools idempotent where possible.** Models retry and sometimes repeat calls.
- **Read-only first.** Add write or destructive tools deliberately.

---

## Security: tool arguments are untrusted input

The model produces the arguments, and the model can be influenced by user text and by retrieved documents. Treat every call like an HTTP request from the public internet:

- **Validate** beyond the schema (ownership, ranges, allowed values).
- **Authorize** using server-side `context`, not model-supplied ids.
- **Least privilege:** give the tool only the access it needs (read-only DB role, scoped API key).
- **Require approval** for irreversible or costly actions (payments, deletes, sending messages). Human-in-the-loop middleware exists for this; see [Agents](./02-agents.md).
- **Never** build SQL, shell commands or file paths directly from arguments without sanitization.

Details in `04-production/04-security.md`.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Vague `description` or missing `.describe()` on fields | Write them as instructions; test with real prompts |
| Tool name with spaces or special characters | Use `snake_case` |
| Expecting `bindTools` to execute the tool | It only lets the model *request* calls; you (or an agent) run them |
| Not appending the `AIMessage` with `tool_calls` before the `ToolMessage` | Keep the pair in history |
| `tool_call_id` mismatch | Use the call's own `id`; invoking the tool with the call object does this for you |
| Returning huge payloads | Trim, summarize, paginate |
| Letting the model pass `userId` or `tenantId` | Read identity from `context` |
| Using `returnDirect` when the model should reason over the result | Remove it |
| Unhandled tool exceptions killing the run | Wrap with error-handling middleware |
| Dozens of overlapping tools | Consolidate; narrow per task |

### Debugging

1. Print `ai.tool_calls` to see exactly which tool and arguments the model chose.
2. Invoke the tool directly with those arguments to separate "model chose badly" from "tool is broken".
3. In LangSmith, open the model call and read the tool definitions it received. Often the description is the culprit.
4. If the model never calls the tool, improve the description or use `toolChoice` to confirm the wiring works.

---

## Quick Summary

- A tool = Zod schema + description + function. The description is a prompt.
- `model.bindTools()` lets the model request calls. You run them and return `ToolMessage`s with matching `tool_call_id`.
- Return compact strings or structured data; `Command` updates agent state; `returnDirect` ends the loop.
- Use the runtime (`context`, `state`, `store`, `writer`) for trusted, server-side data. Never trust model-supplied identity.
- Handle errors via middleware, validate arguments, apply least privilege, and gate destructive actions.

**Next:** [Agents](./02-agents.md)