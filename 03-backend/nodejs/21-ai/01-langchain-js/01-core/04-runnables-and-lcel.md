# Runnables and LCEL

A **Runnable** is LangChain's universal unit of work: something with `invoke`, `stream` and `batch`. Chat models, prompt templates, output parsers, retrievers and tools are all Runnables. **LCEL** (LangChain Expression Language) is just the way of composing them: `a.pipe(b).pipe(c)`.

> Import paths: `@langchain/core/runnables`, `@langchain/core/output_parsers`, `@langchain/core/prompts` (all exported by `@langchain/core@1.2.14`).

**Prerequisites:** [Models and Messages](./02-models-and-messages.md), [Prompt Templates](./03-prompt-templates.md)

---

## The interface

Every Runnable has the same three entry points:

| Method | Does |
|---|---|
| `invoke(input, config?)` | One input, one output |
| `stream(input, config?)` | Yields output chunks |
| `batch(inputs, config?)` | Many inputs in parallel |

That sameness is the whole point. A chain made of Runnables is itself a Runnable, so a chain gets `stream` and `batch` for free, and can be nested inside other chains.

---

## Your first chain

```ts
import { ChatPromptTemplate } from "@langchain/core/prompts";
import { StringOutputParser } from "@langchain/core/output_parsers";

const prompt = ChatPromptTemplate.fromMessages([
  ["system", "Answer in one sentence."],
  ["human", "{question}"],
]);

const chain = prompt.pipe(model).pipe(new StringOutputParser());

const answer = await chain.invoke({ question: "What is a Promise?" });
// answer is a plain string
```

```text
{question} ─► prompt ─► model ─► StringOutputParser ─► string
              (messages)  (AIMessage)   (text)
```

Each step receives the previous step's output. `StringOutputParser` turns the `AIMessage` into a string. (If you only need `.text`, you can also just read it off the message yourself.)

---

## What you get for free

```ts
// streaming: chunks flow through the whole chain
for await (const chunk of await chain.stream({ question: "Explain closures" })) {
  process.stdout.write(chunk);
}

// batching
const answers = await chain.batch(
  [{ question: "Q1" }, { question: "Q2" }],
  { maxConcurrency: 3 }
);
```

Streaming only works end to end if every step can pass chunks along. A step that needs the whole input before producing output will buffer, and tokens arrive in one lump.

---

## Composing different shapes

### RunnableLambda: wrap any function

```ts
import { RunnableLambda } from "@langchain/core/runnables";

const wordCount = RunnableLambda.from((text: string) => text.split(/\s+/).length);

const chain = prompt.pipe(model).pipe(new StringOutputParser()).pipe(wordCount);
```

Use it to put plain code (parsing, formatting, a DB lookup) in the middle of a chain. Plain functions in a `RunnableSequence` are auto-wrapped, but being explicit is clearer.

### RunnableParallel: fan out, then merge

Run several Runnables on the same input and collect results into an object:

```ts
import { RunnableParallel, RunnablePassthrough } from "@langchain/core/runnables";

const analysis = RunnableParallel.from({
  summary: summaryChain,
  keywords: keywordsChain,
  original: new RunnablePassthrough(),
});

await analysis.invoke("long article text...");
// { summary: "...", keywords: "...", original: "long article text..." }
```

A plain object of Runnables inside a `.pipe()` is also coerced to a parallel step. The branches run concurrently, which is a real latency win when each branch is an LLM call.

### RunnablePassthrough.assign: add fields to the flow

Keep the input and add computed keys. Very common in RAG, where you add `context` next to `question`:

```ts
const chain = RunnablePassthrough.assign({
  context: (input: { question: string }) => lookup(input.question),
}).pipe(prompt).pipe(model);
```

### Routing

Return a Runnable from a `RunnableLambda` and it will be run with the same input:

```ts
const router = RunnableLambda.from((input: { topic: string; q: string }) =>
  input.topic === "math" ? mathChain : generalChain
);
```

For heavy branching or loops, a graph (LangGraph) is a better fit than nested lambdas. See [Agents](../03-tools-and-agents/02-agents.md).

---

## Reliability wrappers

Any Runnable can be wrapped:

```ts
const resilient = chain
  .withRetry({ stopAfterAttempt: 3 })
  .withFallbacks({ fallbacks: [cheaperChain] });
```

- `withRetry` re-runs on failure.
- `withFallbacks` tries alternatives in order.
- Chat models already retry network errors, 429 and 5xx internally (default 6), so don't stack a retry on top for those. Use `withRetry` for logic-level failures such as a parse error.

More on this in `04-production/03-reliability-and-cost.md`.

---

## Config: naming and tracing

The second argument to `invoke`/`stream`/`batch` is a `RunnableConfig`. It is inherited by every step in the chain:

```ts
await chain.invoke(
  { question: "..." },
  { runName: "qa-chain", tags: ["prod"], metadata: { userId: "u1" } }
);
```

With LangSmith tracing on, each step of the pipe appears as a node with its own input, output and timing. That is the main debugging tool for chains.

---

## LCEL vs plain async code

LCEL is not always the answer. Compare:

```ts
// plain
const msgs = await prompt.invoke(vars);
const ai = await model.invoke(msgs);
const text = ai.text;
```

versus the chain above. The chain version buys you streaming, batching, tracing per step, retry/fallback wrappers and reuse. If you only need a one-off call, plain code is simpler and perfectly fine.

Rule of thumb: use `.pipe()` for **linear** pipelines and fan-out/fan-in. Use ordinary `if`/`for` code, or a graph/agent, for anything with loops, tool-use cycles or long-lived state.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Passing a string where a step expects an object (or vice versa) | Check each step's input type. Prompts take an object of variables, models take messages/strings |
| Output of a step doesn't match the next step's input shape | Insert a `RunnableLambda` to reshape, or use `RunnablePassthrough.assign` |
| Expecting streaming but getting one chunk | A step in the middle buffers (e.g. a lambda that returns a whole value). Streaming needs every step to support chunks |
| Stacking `withRetry` on top of model retries | Retry the layer that can actually fail logically |
| Debugging a long chain blind | Turn on LangSmith tracing, or `invoke` each piece separately |
| Building loops out of nested lambdas | Move to an agent or graph |

---

## Quick Summary

- Everything is a Runnable with `invoke` / `stream` / `batch`.
- `a.pipe(b)` composes; the result is a Runnable too.
- `RunnableLambda` wraps functions, `RunnableParallel` fans out, `RunnablePassthrough.assign` adds keys, `withRetry` / `withFallbacks` add resilience.
- Config (`runName`, `tags`, `metadata`) propagates through every step into traces.
- Use LCEL for linear pipelines; use plain code or a graph when control flow gets complicated.

**Next:** [Structured Output](./05-structured-output.md)
