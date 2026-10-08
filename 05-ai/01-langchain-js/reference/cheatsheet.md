# LangChain JS Cheatsheet

One-page reference. Details and caveats live in the linked notes. Version context: `langchain@1.x`, `@langchain/core@1.x`, Node 20+. Model names are placeholders; use what your provider offers.

---

## Install and env

```bash
npm install langchain @langchain/core @langchain/openai   # + one package per provider
npm ls @langchain/core                                     # must show ONE version
```

```bash
OPENAI_API_KEY=...
LANGSMITH_TRACING=true
LANGSMITH_API_KEY=...
LANGSMITH_PROJECT=my-app-dev
```

```bash
npx tsx --env-file=.env src/app.ts     # Node 20.6+ loads .env, no dotenv needed
```

→ [Setup](../01-core/01-setup.md)

---

## Package map

| Package | Holds |
|---|---|
| `langchain` | `createAgent`, `initChatModel`, `tool`, middleware, messages |
| `@langchain/core` | Runnables, prompts, output parsers, messages, documents, tools |
| `@langchain/<provider>` | Chat models and embeddings (`openai`, `anthropic`, ...) |
| `@langchain/textsplitters` | Text splitters |
| `@langchain/classic` | `MemoryVectorStore` (dev only) |
| `@langchain/langgraph` | `MemorySaver` and other checkpointer building blocks |

---

## Models

```ts
import { initChatModel } from "langchain";
import { ChatOpenAI } from "@langchain/openai";

const m1 = await initChatModel("gpt-5-nano", { temperature: 0, maxTokens: 1000 });
const m2 = await initChatModel("anthropic:<model-id>");
const m3 = new ChatOpenAI({ model: "gpt-5-nano" });

const res = await m1.invoke("Hi");            // AIMessage
res.text; res.content; res.tool_calls; res.usage_metadata; res.contentBlocks;

for await (const c of await m1.stream("Hi")) process.stdout.write(c.text);
await m1.batch(["a", "b"], { maxConcurrency: 3 });
await m1.invoke("Hi", { runName: "greet", tags: ["x"], metadata: { u: "1" } });
```

| Param | Note |
|---|---|
| `maxRetries` | Default 6 (network, 429, 5xx) |
| `timeout` | Check unit for your provider class |
| `maxTokens` | Always set in production |

→ [Models and Messages](../01-core/02-models-and-messages.md)

---

## Messages

```ts
import { SystemMessage, HumanMessage, AIMessage, ToolMessage } from "langchain";

await model.invoke([
  new SystemMessage("Be terse."),
  new HumanMessage("Explain closures."),
]);

await model.invoke([{ role: "user", content: "Hi" }]);   // dict form also works

new HumanMessage({ contentBlocks: [
  { type: "text", text: "Describe this." },
  { type: "image", url: "https://example.com/cat.jpg" },
]});
```

| Field on `AIMessage` | Meaning |
|---|---|
| `.text` | Plain text |
| `.content` | Raw: string or blocks |
| `.contentBlocks` | Standardized blocks |
| `.tool_calls` | Requested tool calls |
| `.usage_metadata` | Token counts |

Models are **stateless**: resend history.

---

## Prompts

```ts
import { ChatPromptTemplate, MessagesPlaceholder } from "@langchain/core/prompts";

const prompt = ChatPromptTemplate.fromMessages([
  ["system", "You are a {role}."],
  new MessagesPlaceholder("history"),
  ["human", "{input}"],
]);

await prompt.invoke({ role: "tutor", history: [], input: "Hi" });
const withDate = await prompt.partial({ role: "tutor" });
```

Literal braces: `{{` and `}}`. → [Prompt Templates](../01-core/03-prompt-templates.md)

---

## Runnables and LCEL

```ts
import { StringOutputParser } from "@langchain/core/output_parsers";
import {
  RunnableLambda, RunnableParallel, RunnablePassthrough, RunnableSequence,
} from "@langchain/core/runnables";

const chain = prompt.pipe(model).pipe(new StringOutputParser());

await chain.invoke({ role: "tutor", history: [], input: "Hi" });
await chain.stream(vars);
await chain.batch([v1, v2], { maxConcurrency: 3 });

RunnableLambda.from((s: string) => s.length);
RunnableParallel.from({ a: chainA, b: chainB, original: new RunnablePassthrough() });
RunnablePassthrough.assign({ context: (i) => lookup(i.question) });

chain.withRetry({ stopAfterAttempt: 3 }).withFallbacks({ fallbacks: [backup] });
```

→ [Runnables and LCEL](../01-core/04-runnables-and-lcel.md)

---

## Structured output

```ts
import * as z from "zod";

const Movie = z.object({
  title: z.string().describe("Movie title"),
  year: z.number(),
  budget: z.number().nullable(),
});

const out = await model.withStructuredOutput(Movie).invoke("Tell me about Inception");
const both = await model.withStructuredOutput(Movie, { includeRaw: true }).invoke("...");
// both = { raw: AIMessage, parsed: {...} }
```

Zod validated; raw JSON Schema is not. Agents: `responseFormat: toolStrategy(S)` or `providerStrategy(S)`, read `result.structuredResponse`. → [Structured Output](../01-core/05-structured-output.md)

---

## Streaming

| Level | Call |
|---|---|
| Model / chain | `for await (const c of await x.stream(input))` |
| Combine chunks | `full = full ? full.concat(c) : c` |
| Typed events | `x.streamEvents(input)` |
| Agent | `agent.stream(input, { streamMode: "updates" \| "messages" \| "custom" })` |

Tool-call chunk args are partial JSON: accumulate before parsing. → [Streaming](../01-core/06-streaming.md)

---

## Chat history and memory

```ts
import { createAgent, summarizationMiddleware, trimMessages } from "langchain";
import { MemorySaver } from "@langchain/langgraph";

const agent = createAgent({
  model: "gpt-5-nano",
  tools: [],
  checkpointer: new MemorySaver(),            // dev only; use a DB-backed saver in prod
  middleware: [summarizationMiddleware({
    model: "gpt-5-nano", trigger: { tokens: 4000 }, keep: { messages: 20 },
  })],
});

const cfg = { configurable: { thread_id: "user-42:chat-1" } };
await agent.invoke({ messages: [{ role: "user", content: "Hi" }] }, cfg);
```

Keep tool-call and tool-result pairs together when trimming. → [Chat History](../01-core/07-chat-history.md)

---

## RAG

```ts
import { Document } from "@langchain/core/documents";
import { RecursiveCharacterTextSplitter } from "@langchain/textsplitters";
import { OpenAIEmbeddings } from "@langchain/openai";
import { MemoryVectorStore } from "@langchain/classic/vectorstores/memory";

const splitter = new RecursiveCharacterTextSplitter({ chunkSize: 1000, chunkOverlap: 200 });
const chunks = await splitter.splitDocuments(docs);        // keeps metadata

const embeddings = new OpenAIEmbeddings({ model: "text-embedding-3-large" });
const store = new MemoryVectorStore(embeddings);           // dev only
await store.addDocuments(chunks);

await store.similaritySearch("query", 4);
await store.similaritySearchWithScore("query", 4);         // score meaning varies by store
const retriever = store.asRetriever({ k: 4 });
await retriever.invoke("query");
```

2-step chain:

```ts
const ragChain = RunnableSequence.from([
  { context: async (i: { question: string }) => format(await retriever.invoke(i.question)),
    question: (i: { question: string }) => i.question },
  prompt, model, new StringOutputParser(),
]);
```

Rules: same embedding model for index and query; chunk size is **characters**; tenant filters in code; retrieved text is untrusted. → [Loading and Splitting](../02-rag/01-loading-and-splitting.md), [Embeddings and Vector Stores](../02-rag/02-embeddings-and-vector-stores.md), [Retrieval and RAG](../02-rag/03-retrieval-and-rag.md)

---

## Tools

```ts
import { tool } from "langchain";

const getWeather = tool(
  async ({ location }) => `Sunny in ${location}.`,
  {
    name: "get_weather",                       // snake_case
    description: "Get the weather for a city.",
    schema: z.object({ location: z.string().describe("City name") }),
  },
);

const ai = await model.bindTools([getWeather]).invoke(messages);
for (const call of ai.tool_calls ?? []) messages.push(await getWeather.invoke(call)); // ToolMessage
```

| Option | Effect |
|---|---|
| `bindTools(t, { toolChoice: "any" })` | Force some tool call |
| `returnDirect: true` | End the loop with the tool's output |
| runtime `context` / `state` / `store` / `writer` / `toolCallId` | Trusted server-side data inside a tool |

Identity comes from `context`, never model-supplied args. → [Tools](../03-tools-and-agents/01-tools.md)

---

## Agents

```ts
import { createAgent } from "langchain";

const agent = createAgent({
  model: "gpt-5-nano",
  tools: [getWeather],
  systemPrompt: "Be concise.",
  checkpointer,                    // memory per thread_id
  // responseFormat, stateSchema, contextSchema, middleware, name
});

const result = await agent.invoke(
  { messages: [{ role: "user", content: "Weather in SF?" }] },
  { configurable: { thread_id: "t1" }, context: { userId: "u1" } },
);
result.messages.at(-1)?.content;
```

| Middleware | Purpose |
|---|---|
| `modelCallLimitMiddleware` / `toolCallLimitMiddleware` | Bound loops and cost |
| `modelRetryMiddleware` / `toolRetryMiddleware` | Retries with backoff |
| `humanInTheLoopMiddleware` | Approval for risky tools (needs a checkpointer) |
| `summarizationMiddleware` | Compress long history |
| `piiMiddleware` | Redact, mask, hash or block PII |
| `createMiddleware` hooks | `beforeModel`, `afterModel`, `wrapModelCall`, `wrapToolCall` |

Chain when steps are known; agent when the path depends on the input. → [Agents](../03-tools-and-agents/02-agents.md)

---

## Production quick list

| Area | Do |
|---|---|
| Tracing | `LANGSMITH_*` env vars; `runName`, `tags`, `metadata`; `traceable` for custom code; serverless: `awaitAllCallbacks()` + `LANGCHAIN_CALLBACKS_BACKGROUND=false` |
| Testing | Model-free unit tests; few tolerant integration tests; dataset evaluations; test RAG retrieval and generation separately |
| Reliability | `timeout`, `maxTokens`, call limits, `withFallbacks`, one retry layer, idempotent write tools |
| Cost | Log `usage_metadata`; trim or summarize; stable prompt prefix first; route models by difficulty |
| Security | Least privilege; defense in depth; enforce in code; approvals for destructive tools; escape outputs; secrets server-side; never `load()` untrusted data |
| Serving | Auth first; `thread_id` namespaced by user; persistent checkpointer and vector store; abort on disconnect; watch proxy buffering |

→ [Tracing](../04-production/01-tracing-and-debugging.md), [Testing](../04-production/02-testing-and-evaluation.md), [Reliability and Cost](../04-production/03-reliability-and-cost.md), [Security](../04-production/04-security.md), [Serving](../04-production/05-serving-and-integration.md)

---

## Gotchas

| Symptom | Likely cause |
|---|---|
| Weird `instanceof` or type errors | Two `@langchain/core` versions |
| `res` is an object, not a string | `invoke` returns `AIMessage`; use `.text` |
| Model "forgets" | No history resent, or missing `thread_id` |
| 400 after N turns | Trimmed between a tool call and its result |
| Missing variable error for something you never meant as one | Unescaped `{}` in a prompt |
| Chain streams in one lump | A middle step buffers |
| Bad RAG answers | Check retrieved chunks before touching the prompt |
| Wrong tool chosen | Vague tool or field descriptions |
| Traces missing in serverless | Callbacks not flushed |
| Cost spike | Unbounded loop, long context, stacked retries |

---

## Debug checklist

1. `node -v` is 20+; `npm ls @langchain/core` shows one version.
2. Smallest possible `model.invoke("hi")` works.
3. Print `await prompt.invoke(vars)`; run `retriever.invoke(q)` alone; call tools directly.
4. Print `result.messages` for agents.
5. Open the LangSmith trace; find the first wrong step.
6. Save the failure as a test case.

---

**See also:** [Interview Questions](./interview.md)