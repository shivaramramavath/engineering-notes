# Chat History

Models are stateless, so a "conversation" is a message list that you keep and resend. Chat history is about three problems: **storing** that list per conversation, **keeping it small enough** to fit the context window, and **keeping it valid** for the provider.

> Checked against the LangChain JS 1.x docs (`langchain@1.5.15`). In 1.x, the documented way to get memory is through agents and checkpointers. Older tutorials use other classes (see the end).

**Prerequisites:** [Models and Messages](./02-models-and-messages.md)

---

## The manual version (and why it is worth knowing)

```ts
import { HumanMessage, type BaseMessage } from "@langchain/core/messages";

const history: BaseMessage[] = [];

async function chat(userText: string) {
  history.push(new HumanMessage(userText));
  const ai = await model.invoke(history);
  history.push(ai);
  return ai.text;
}

await chat("My name is Ravi.");
await chat("What's my name?"); // works: the first turn is in `history`
```

This is all "memory" is at the model level. Everything else in this note is machinery for doing it per user, persistently, and within limits.

Problems with the naive version: one global list (no per-user separation), lost on restart, and it grows until it exceeds the context window or gets expensive.

---

## Short-term memory with an agent and a checkpointer

A **thread** is one conversation. A **checkpointer** saves the agent's state (including the messages) per thread, so each call only sends the new message plus a `thread_id`:

```ts
import { createAgent } from "langchain";
import { MemorySaver } from "@langchain/langgraph";

const agent = createAgent({
  model: "gpt-5-nano",
  tools: [],
  checkpointer: new MemorySaver(), // in-memory: dev only
});

const config = { configurable: { thread_id: "user-42-chat-1" } };

await agent.invoke({ messages: [{ role: "user", content: "Hi! I'm Bob." }] }, config);
const r = await agent.invoke({ messages: [{ role: "user", content: "What's my name?" }] }, config);
console.log(r.messages.at(-1)?.content); // knows it's Bob
```

- Same `thread_id` → same conversation. Different id → separate history.
- `MemorySaver` lives in process memory and disappears on restart. For production, use a database-backed checkpointer, e.g. `PostgresSaver` from `@langchain/langgraph-checkpoint-postgres`:

```ts
import { PostgresSaver } from "@langchain/langgraph-checkpoint-postgres";
const checkpointer = PostgresSaver.fromConnString(process.env.DATABASE_URL!);
```

Pick `thread_id` values you control and namespace them per user. Never take them straight from untrusted client input if it would let one user read another's thread.

Short-term memory is **per thread**. Remembering facts across conversations is a different feature (long-term memory) and not covered here.

---

## Keeping history within limits

Long histories hurt in three ways: they can exceed the context window, they cost more per call, and models get worse when distracted by stale context. Common strategies:

| Strategy | Idea | Trade-off |
|---|---|---|
| **Trim** | Drop old messages, keep recent | Simple; loses early info |
| **Delete** | Remove specific messages from state | Precise; easy to produce invalid history |
| **Summarize** | Replace old messages with a model-written summary | Keeps gist; costs an extra model call |

### Trim with trimMessages

`trimMessages` is exported from `langchain` (and from `@langchain/core/messages`). The docs use it inside a `beforeModel` middleware:

```ts
import { createAgent, createMiddleware, trimMessages } from "langchain";
import { RemoveMessage } from "@langchain/core/messages";
import { MemorySaver, REMOVE_ALL_MESSAGES } from "@langchain/langgraph";

const trim = createMiddleware({
  name: "TrimMessages",
  beforeModel: async (state) => {
    const trimmed = await trimMessages(state.messages, {
      maxTokens: 384,
      strategy: "last",
      startOn: "human",
      endOn: ["human", "tool"],
      tokenCounter: (msgs) => msgs.length, // counts messages, not tokens
    });
    return { messages: [new RemoveMessage({ id: REMOVE_ALL_MESSAGES }), ...trimmed] };
  },
});

const agent = createAgent({
  model: "gpt-5-nano",
  tools: [],
  middleware: [trim],
  checkpointer: new MemorySaver(),
});
```

Things to see in that example:

- `strategy: "last"` keeps the most recent messages.
- `startOn: "human"` makes sure the kept window starts with a user message. Many providers require that.
- `tokenCounter` here counts **messages**, so `maxTokens: 384` really means 384 messages. Swap in a real token counter (or the model) if you want true token budgets.
- Returning `RemoveMessage({ id: REMOVE_ALL_MESSAGES })` followed by the kept messages replaces the stored history.

### Summarize with built-in middleware

```ts
import { createAgent, summarizationMiddleware } from "langchain";

const agent = createAgent({
  model: "gpt-5-nano",
  tools: [],
  middleware: [
    summarizationMiddleware({
      model: "gpt-5-nano",          // cheaper model for summarizing
      trigger: { tokens: 4000 },    // summarize when history passes this
      keep: { messages: 20 },       // keep the last 20 verbatim
    }),
  ],
  checkpointer: new MemorySaver(),
});
```

Summaries lose detail by nature. Exact facts (IDs, amounts, names) that must survive should be stored in structured state or a database, not left to a summary.

---

## Keeping history valid

Providers are strict about message order. When you trim or delete:

- An `AIMessage` with `tool_calls` **must** be followed by matching `ToolMessage` results. Cutting between them produces an API error.
- Some providers need the history to **start with a human message**.
- Keep the system prompt (it is usually set separately, e.g. `systemPrompt` on the agent, so trimming history doesn't remove it).

Most "400 bad request after N turns" bugs are an invalid trim.

---

## Using history with plain prompt chains

If you are not using an agent, you can still feed history into a prompt with `MessagesPlaceholder`:

```ts
const prompt = ChatPromptTemplate.fromMessages([
  ["system", "You are helpful."],
  new MessagesPlaceholder("history"),
  ["human", "{input}"],
]);
const chain = prompt.pipe(model);

const ai = await chain.invoke({ history, input: "What's my name?" });
history.push(new HumanMessage("What's my name?"), ai);
```

You own storage, per-user separation and trimming in this setup. That is fine for small apps; reach for a checkpointer when you need persistence, resumability or tool-using agents.

---

## Older APIs you will see in tutorials

- `RunnableWithMessageHistory` and `InMemoryChatMessageHistory` (the `@langchain/core/chat_history` subpath still exists) came from the pre-agent era. The current docs route conversation memory through checkpointers and `thread_id` instead.
- `ConversationBufferMemory`-style classes belong to the legacy chain API. Don't start new code with them.

If a tutorial's imports don't resolve, it is probably written for an older major version.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Forgetting `thread_id` on follow-up calls | Pass the same config every turn, or each call starts fresh |
| One global history for all users | Separate by thread/user id |
| Using `MemorySaver` in production | Use a persistent checkpointer (e.g. Postgres) |
| Trimming between a tool call and its result | Trim at turn boundaries; use `startOn`/`endOn` |
| `tokenCounter: msgs => msgs.length` treated as tokens | It counts messages; use a real token counter for token budgets |
| Dropping the system prompt when trimming | Keep it outside the trimmed list |
| Relying on a summary for exact data | Store exact data separately |
| Sending the whole history forever | Trim or summarize before it gets expensive |

### Debugging

- Print `result.messages` after a call to see exactly what is stored.
- If the model "forgets", check the `thread_id` first, then whether trimming removed the turn.
- In LangSmith, open the model call and inspect the actual message list it received.

---

## Quick Summary

- Models are stateless; history is a message list you resend.
- With agents, a **checkpointer** plus a `thread_id` gives per-conversation memory. `MemorySaver` is dev-only.
- Control growth by trimming, deleting, or summarizing; keep tool-call/result pairs together and start with a human message.
- Without agents, pass history through `MessagesPlaceholder` and manage storage yourself.
- `RunnableWithMessageHistory` and the old Memory classes are legacy patterns.

**Next:** [Loading and Splitting](../02-rag/01-loading-and-splitting.md)
