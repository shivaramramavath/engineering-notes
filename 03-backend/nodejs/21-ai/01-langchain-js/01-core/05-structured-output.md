# Structured Output

Free text is awkward to build software on. Structured output makes the model return data that matches a **schema**, so you get a typed object instead of prose you have to parse.

> Checked against the LangChain JS 1.x docs (`langchain@1.5.15`).

**Prerequisites:** [Models and Messages](./02-models-and-messages.md)

---

## The basic pattern

Define a schema with Zod, wrap the model, call it:

```ts
import * as z from "zod";
import { initChatModel } from "langchain";

const Movie = z.object({
  title: z.string().describe("The title of the movie"),
  year: z.number().describe("The year the movie was released"),
  director: z.string().describe("The director of the movie"),
});

const model = await initChatModel("gpt-5-nano");
const structured = model.withStructuredOutput(Movie);

const movie = await structured.invoke("Tell me about the movie Inception");
// { title: "Inception", year: 2010, director: "Christopher Nolan" }
```

What to notice:

- The return value is a plain **object**, not an `AIMessage`.
- `.describe(...)` on each field is part of the prompt the model sees. Good descriptions directly improve accuracy.
- With a Zod schema, the output is **validated** with Zod's parse. Bad output throws instead of silently passing through.
- `withStructuredOutput` returns a new Runnable, so it works in chains (`prompt.pipe(structured)`).

---

## Schema options

| Schema type | Validated at runtime? | Notes |
|---|---|---|
| Zod | Yes | Preferred. Gives you TypeScript types via `z.infer` |
| Standard Schema (e.g. Valibot via `toStandardJsonSchema`) | Yes | Any library implementing the Standard Schema spec |
| Raw JSON Schema | **No** | You validate yourself. Pass `{ method: "jsonSchema" }` |

```ts
type Movie = z.infer<typeof Movie>; // reuse the schema as your TS type
```

Nested objects, arrays, enums and nullable fields all work:

```ts
const Details = z.object({
  title: z.string(),
  cast: z.array(z.object({ name: z.string(), role: z.string() })),
  genres: z.array(z.enum(["drama", "comedy", "action", "other"])),
  budget: z.number().nullable().describe("Budget in millions USD"),
});
```

Use `.nullable()` for fields that may be unknown so the model has a legal way to say "not stated" instead of inventing a value.

---

## Options

```ts
model.withStructuredOutput(Movie, {
  includeRaw: true,         // also return the raw AIMessage
  method: "jsonSchema",     // how the provider is asked to enforce the schema
});
```

- **`includeRaw: true`** returns `{ raw, parsed }`. Use it when you need `usage_metadata` or want to inspect what the model actually produced.
- **`method`** selects the mechanism. The docs list `"jsonSchema"`, `"functionCalling"` and `"jsonMode"`; which are supported depends on the provider. Check your provider's integration page.

---

## How it works underneath

There are two mechanisms, and LangChain picks the best one the model supports:

```text
Provider-native   schema sent to the provider's structured-output API;
                  the provider enforces it.         ← most reliable
Tool calling      schema is exposed as a tool the model must call;
                  arguments of that call are your object.
```

Provider-native enforcement is stricter. Tool calling works on nearly any model that supports tools. You usually don't choose manually.

---

## Structured output in agents

With `createAgent`, use `responseFormat`. The parsed result lands on `structuredResponse` in the final state:

```ts
import { createAgent, providerStrategy, toolStrategy } from "langchain";

const agent = createAgent({
  model: "gpt-5-nano",
  tools: [],
  responseFormat: toolStrategy(ProductReview), // or providerStrategy(...)
});

const result = await agent.invoke({
  messages: [{ role: "user", content: "Analyze: 'Great product, fast shipping, pricey'" }],
});
console.log(result.structuredResponse);
```

- `providerStrategy(schema)` forces native provider enforcement.
- `toolStrategy(schema)` forces the tool-calling approach, with options such as `toolMessageContent` and `handleError`.
- Pass a bare schema and LangChain uses the provider strategy when the model supports it, otherwise falls back to tool calling.
- `toolStrategy([A, B])` accepts a union of schemas; the model picks one.
- Tool strategy retries automatically with error feedback when output fails validation or the model returns multiple structured results. Customize with `handleError`.

See [Agents](../03-tools-and-agents/02-agents.md) for the rest of the agent loop.

---

## Practical advice

- **Schema is prompt.** Clear field names and `.describe()` text matter more than prompt wording elsewhere.
- **Keep schemas small and flat** when you can. Deeply nested or very large schemas make failures more likely.
- **Constrain with enums and ranges** (`z.enum`, `.min()`, `.max()`) so bad values are rejected rather than stored.
- **Make unknowns representable** (`.nullable()` / `.optional()`), otherwise the model will guess.
- **Validation passing does not mean the content is correct.** It only guarantees shape and types. Facts can still be wrong.
- **Low temperature** for extraction tasks.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Asking for JSON in the prompt and parsing it with `JSON.parse` | Use `withStructuredOutput`; it enforces the schema and validates |
| Using raw JSON Schema and assuming it is validated | It isn't. Validate manually (or switch to Zod) |
| Required field the text doesn't contain | Make it `.nullable()` or `.optional()`, or the model will invent a value |
| Expecting an `AIMessage` back | You get the parsed object unless you set `includeRaw: true` |
| Model doesn't support tools or native structured output | Pick a model that does; check its profile/integration page |
| Agent with both tools and `responseFormat` on a model that can't do both at once | The model must support tools and structured output together |
| Vague field names (`data`, `info`) | Use specific names plus `.describe()` |

### Debugging

1. Re-run with `includeRaw: true` and inspect `raw`.
2. Read the Zod error. It names the failing field.
3. Trace it in LangSmith to see the exact schema/tool definition the provider received.
4. Simplify the schema until it works, then add fields back.

---

## Quick Summary

- `model.withStructuredOutput(zodSchema)` returns typed, validated objects instead of text.
- Zod and Standard Schema are validated at runtime; raw JSON Schema is not.
- `includeRaw: true` gives `{ raw, parsed }`; `method` picks the enforcement mechanism where supported.
- In agents, use `responseFormat` with `providerStrategy` / `toolStrategy` and read `structuredResponse`.
- Good field names and descriptions are the real prompt; shape validity is not factual correctness.

**Next:** [Streaming](./06-streaming.md)
