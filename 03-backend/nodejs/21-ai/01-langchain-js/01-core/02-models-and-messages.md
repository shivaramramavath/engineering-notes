# Models and Messages

Everything in LangChain revolves around two things: a **chat model** you call, and the **messages** you send and receive. Once these two are clear, prompts, chains, tools and agents are just ways of building and routing message lists.

> Checked against the LangChain JS 1.x docs (`langchain@1.5.15`, `@langchain/core@1.2.14`). Model names in examples change often; swap in whatever your provider offers.

**Prerequisite:** [Setup](./01-setup.md)

---

## Chat models in one picture

```text
messages[] ──► chat model ──► AIMessage
   ▲                              │
   └──── append, call again ◄─────┘
```

A chat model is **stateless**. It only knows what is in the list you pass in. "Conversation memory" is you (or a framework) appending messages and sending the whole list again. This is the single most important idea in the whole chapter.

---

## Creating a model

Two equivalent ways:

```ts
import { initChatModel } from "langchain";
import { ChatOpenAI } from "@langchain/openai";

// 1. Provider-agnostic: pick by string
const a = await initChatModel("gpt-5-nano");
const b = await initChatModel("anthropic:<model-id>"); // "provider:model" form

// 2. Provider class: direct, full access to provider-specific options
const c = new ChatOpenAI({ model: "gpt-5-nano" });
```

Use `initChatModel` when the provider is config-driven. Use the class when you need provider-specific options (e.g. OpenAI's Responses API toggle). The matching provider package must be installed either way.

### Common parameters

| Param | Meaning |
|---|---|
| `model` | Model id. Can be `"provider:model"` with `initChatModel` |
| `temperature` | Randomness. Lower is more deterministic |
| `maxTokens` | Cap on output tokens |
| `timeout` | Request timeout (seconds per the docs table; one docs example passes milliseconds, so check your provider class) |
| `maxRetries` | Retries on network errors, 429 and 5xx. Default 6. Not retried: 401, 404 |
| `apiKey` | Defaults to the provider's env var |

```ts
const model = await initChatModel("gpt-5-nano", {
  temperature: 0,
  maxTokens: 1000,
  maxRetries: 6,
});
```

---

## Calling a model: invoke, stream, batch

```ts
// invoke: one complete response
const res = await model.invoke("Why do parrots talk?");
console.log(res.text);          // string
console.log(res.content);       // string or content blocks

// stream: chunks as they are generated
for await (const chunk of await model.stream("Tell me a joke")) {
  process.stdout.write(chunk.text);
}

// batch: many independent inputs, run in parallel
const outs = await model.batch(["Q1", "Q2", "Q3"], { maxConcurrency: 2 });
```

- `invoke` accepts a **string** (treated as one human message) or a **message list**.
- `batch` runs inputs in parallel. Set `maxConcurrency` to avoid rate limits.
- Streaming has its own note: [Streaming](./06-streaming.md).

### Per-call config

Second argument is a `RunnableConfig`. Useful keys: `runName`, `tags`, `metadata`, `callbacks`, `maxConcurrency`. These show up in LangSmith traces, so name your runs.

```ts
await model.invoke("Hi", { runName: "greeting", tags: ["demo"] });
```

---

## Messages

A message is **role + content + metadata**. LangChain's message types work the same across providers.

| Class | Role | Used for |
|---|---|---|
| `SystemMessage` | system | Instructions, persona, rules |
| `HumanMessage` | user | User input (text, images, files) |
| `AIMessage` | assistant | Model output, including tool calls |
| `ToolMessage` | tool | Result of one tool call, sent back to the model |

```ts
import { SystemMessage, HumanMessage, AIMessage } from "langchain";

const res = await model.invoke([
  new SystemMessage("You translate English to French."),
  new HumanMessage("I love programming."),
  new AIMessage("J'adore la programmation."),
  new HumanMessage("I love building applications."),
]);
```

Message classes are importable from `langchain` or from `@langchain/core/messages`.

### Shorthand: plain objects

OpenAI-style dictionaries are accepted anywhere a message list is:

```ts
await model.invoke([
  { role: "system", content: "You are terse." },
  { role: "user", content: "Explain closures in one line." },
]);
```

Handy for quick code. Message classes are better when you need `name`, `id`, or metadata, or when you want type safety.

### Inserting fake AI turns

You can build an `AIMessage` yourself and put it in history, which is how few-shot examples and restored conversations work. The model treats it as something it said.

---

## Anatomy of an AIMessage

This is what `invoke` returns, and it carries more than text.

| Field | What it holds |
|---|---|
| `text` | Plain text of the response |
| `content` | Raw content: string, or list of blocks (provider-native or standard) |
| `contentBlocks` | Standardized, provider-agnostic view of `content` (text, reasoning, tool calls, images...) |
| `tool_calls` | Tool calls the model requested (empty if none) |
| `usage_metadata` | Token counts: `input_tokens`, `output_tokens`, `total_tokens`, plus details like cache reads and reasoning tokens |
| `response_metadata` | Provider-specific response info |
| `id` | Message id |

```ts
const res = await model.invoke("Hello!");
console.log(res.usage_metadata);
// { input_tokens, output_tokens, total_tokens, ... }
```

`usage_metadata` is how you track cost per call. Log it from day one.

### content vs text vs contentBlocks

- `.text` is what you want 90% of the time.
- `.content` is **loosely typed**: a string for simple replies, an array when the model returns reasoning, tool calls or multimodal output. Don't assume it is a string if you use reasoning or tools.
- `.contentBlocks` normalizes provider formats. For example, Anthropic `thinking` blocks and OpenAI `reasoning` summaries both come out as a `reasoning` block, so one code path works for both.

---

## Multimodal input

Pass content blocks instead of a string:

```ts
import { HumanMessage } from "langchain";

const msg = new HumanMessage({
  contentBlocks: [
    { type: "text", text: "Describe this image." },
    { type: "image", url: "https://example.com/cat.jpg" },
  ],
});
```

Standard blocks exist for image, audio, video and file. Not every model supports every type, so check your provider. Provider-native formats (such as OpenAI's `image_url` block) also work.

---

## Tool calling at the message level

Tool calling is covered fully in [Tools](../03-tools-and-agents/01-tools.md). The message mechanics matter here, so briefly:

```ts
const withTools = model.bindTools([getWeather]);
const ai = await withTools.invoke("Weather in Paris?");

for (const call of ai.tool_calls ?? []) {
  console.log(call.name, call.args, call.id);
}
```

The model never runs the tool. It returns a **request**. You (or an agent) run it and send back a `ToolMessage` whose `tool_call_id` matches the call's `id`:

```ts
import { ToolMessage } from "langchain";

const messages = [
  new HumanMessage("Weather in Paris?"),
  ai,                                           // contains the tool_calls
  new ToolMessage({ content: "Sunny, 22°C", tool_call_id: ai.tool_calls![0].id! }),
];
const final = await withTools.invoke(messages);
```

`ToolMessage` also has an `artifact` field for data you want to keep programmatically without sending it to the model.

---

## Persisting messages

Messages serialize to plain objects:

```ts
import { HumanMessage } from "@langchain/core/messages";
import { load } from "@langchain/core/load";

const json = JSON.stringify(new HumanMessage("hi").toJSON());
const restored = await load<HumanMessage>(json);
```

> **Security:** `load()` instantiates classes and runs constructors. Never call it on untrusted or user-supplied input. Only load data you wrote yourself, such as from your own database.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Treating `res` as a string | It is an `AIMessage`. Use `res.text` |
| Assuming `res.content` is always a string | Use `res.text`, or handle block arrays |
| Expecting the model to remember the last call | Models are stateless. Resend the history |
| Sending a `ToolMessage` with the wrong `tool_call_id` | It must match the `id` in the `AIMessage`'s `tool_calls` |
| Dropping the `AIMessage` with tool calls from history | Providers require the tool-call message before its `ToolMessage` results |
| Hitting rate limits with `batch` | Set `maxConcurrency` |
| Relying on `name` on messages | Behavior varies by provider; some ignore it |
| Calling `load()` on user data | Never. It executes constructors |

---

## Quick Summary

- A chat model is stateless: messages in, `AIMessage` out.
- Create with `initChatModel("provider:model")` or a provider class.
- `invoke` / `stream` / `batch`; strings and plain `{role, content}` objects are accepted.
- Four message types: system, human, AI, tool.
- Read `.text` for text, `.usage_metadata` for tokens, `.tool_calls` for tool requests, `.contentBlocks` for a provider-agnostic view.
- Tool results go back as `ToolMessage` with a matching `tool_call_id`.

**Next:** [Prompt Templates](./03-prompt-templates.md)
