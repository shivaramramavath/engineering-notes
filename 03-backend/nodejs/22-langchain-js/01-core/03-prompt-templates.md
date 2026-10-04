# Prompt Templates

A prompt template is a reusable message list (or string) with **named holes**. You fill the holes at call time and get a ready-to-send prompt. It is the piece that turns "hard-coded string concatenation" into something testable and traceable.

> Import paths below are from `@langchain/core/prompts` (`@langchain/core@1.2.14` exports `./prompts`).

**Prerequisite:** [Models and Messages](./02-models-and-messages.md)

---

## Why bother

String interpolation works until it doesn't. Templates give you:

- A single place that defines the prompt, separate from the call.
- Validation that every variable was supplied.
- A prompt that is a `Runnable`, so it plugs straight into a chain with `.pipe()` ([Runnables and LCEL](./04-runnables-and-lcel.md)) and shows up as its own step in traces.
- Safe handling of `{` and `}` characters (see mistakes below).

---

## Chat prompt templates (what you will use most)

```ts
import { ChatPromptTemplate } from "@langchain/core/prompts";

const prompt = ChatPromptTemplate.fromMessages([
  ["system", "You are a {role}. Answer in {language}."],
  ["human", "{question}"],
]);

const messages = await prompt.invoke({
  role: "senior TypeScript developer",
  language: "English",
  question: "What is a closure?",
});
```

Key points:

- Each entry is `[role, template]`. Roles: `"system"`, `"human"`, `"ai"`.
- Variables use single braces: `{question}`.
- `prompt.invoke(vars)` returns a **prompt value**, which a chat model accepts directly as messages. You don't convert it yourself.
- Missing variable: it throws, which is what you want.

Used with a model:

```ts
const res = await model.invoke(
  await prompt.invoke({ role: "chef", language: "French", question: "Boil an egg?" })
);
```

In practice you rarely do that by hand. You pipe them together:

```ts
const chain = prompt.pipe(model);
const res = await chain.invoke({ role: "chef", language: "French", question: "Boil an egg?" });
```

---

## Plain string templates

For single-string prompts (completion-style or when you just need a string):

```ts
import { PromptTemplate } from "@langchain/core/prompts";

const p = PromptTemplate.fromTemplate("Summarize in {n} bullets:\n{text}");
const out = await p.format({ n: 3, text: "..." }); // string
```

With chat models, prefer `ChatPromptTemplate`. A string template becomes a single human message.

---

## Injecting message history: MessagesPlaceholder

When part of the prompt is a whole **list** of messages (chat history, agent scratchpad), use a placeholder instead of a string variable:

```ts
import { ChatPromptTemplate, MessagesPlaceholder } from "@langchain/core/prompts";

const prompt = ChatPromptTemplate.fromMessages([
  ["system", "You are a helpful assistant."],
  new MessagesPlaceholder("history"),
  ["human", "{input}"],
]);

await prompt.invoke({
  history: [
    { role: "user", content: "My name is Ravi." },
    { role: "assistant", content: "Nice to meet you, Ravi." },
  ],
  input: "What's my name?",
});
```

`history` expects an array of messages, not a string. This is the template-level building block for conversation memory; see [Chat History](./07-chat-history.md).

---

## Partial variables

Fix some variables early, supply the rest later:

```ts
const base = ChatPromptTemplate.fromMessages([
  ["system", "Today is {date}. You are a {role}."],
  ["human", "{question}"],
]);

const withDate = await base.partial({ date: new Date().toDateString() });
await withDate.invoke({ role: "tutor", question: "Explain promises." });
```

Useful for values known at startup (date, app name, tone).

---

## Few-shot examples

The simplest reliable version is to put examples in the message list as real human/AI turns:

```ts
const prompt = ChatPromptTemplate.fromMessages([
  ["system", "Classify sentiment as positive or negative. One word."],
  ["human", "I love this!"],
  ["ai", "positive"],
  ["human", "This is awful."],
  ["ai", "negative"],
  ["human", "{text}"],
]);
```

Models follow example turns well, and it is easy to read and diff. Reach for dynamic example selection only when you have many examples and need to pick relevant ones per query.

---

## Escaping braces

`{` and `}` are template syntax. To include literal braces (JSON examples in a prompt, for instance), double them:

```ts
ChatPromptTemplate.fromMessages([
  ["system", 'Reply as JSON like {{"answer": "..."}}'],
  ["human", "{question}"],
]);
```

The output has single braces. Forgetting to escape is the most common template bug; it surfaces as a "missing variable" error for a name you never meant as a variable.

> If you need structured JSON back, don't instruct it in the prompt. Use [Structured Output](./05-structured-output.md).

---

## Do you always need a template?

No. For a one-off call, a plain message list is fine:

```ts
await model.invoke([
  { role: "system", content: "You are terse." },
  { role: "user", content: userText },
]);
```

Use a template when the prompt is reused, has variables you want validated, or sits inside a chain you want traced step by step. Don't build string prompts by hand and then wrap them in a template just to have one.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Unescaped `{}` in the prompt text | Double the braces: `{{` and `}}` |
| Passing a string for a `MessagesPlaceholder` | It needs an array of messages |
| Missing variable at runtime | Pass every variable; the error names the missing one |
| Variable name typos between template and `invoke` | Names must match exactly (case-sensitive) |
| User input injected into the **system** message | Keep untrusted input in the human message; see `04-production/04-security.md` |
| Huge static instructions repeated per call | Keep them in the system message; consider provider prompt caching for long, stable prefixes |

### Debugging

Call `await prompt.invoke(vars)` on its own and `console.log` the result to see the exact messages the model would receive. In LangSmith the template appears as its own step with inputs and rendered output.

---

## Quick Summary

- `ChatPromptTemplate.fromMessages([[role, text], ...])` for chat models; variables are `{name}`.
- A template is a `Runnable`: `prompt.invoke(vars)` gives messages, `prompt.pipe(model)` makes a chain.
- `MessagesPlaceholder` injects a list of messages (history).
- `partial()` pre-fills variables; few-shot is simplest as example turns.
- Escape literal braces as `{{ }}`.

**Next:** [Runnables and LCEL](./04-runnables-and-lcel.md)
