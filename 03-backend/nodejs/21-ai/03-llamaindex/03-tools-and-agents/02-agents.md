# Agents

A query engine runs a fixed pipeline: retrieve, then synthesize. An **agent** replaces the fixed pipeline with a loop where the LLM decides what to do next: call a tool, look at the result, call another, or answer. That flexibility is the point, and also the cost: more LLM calls, more latency, and less predictable behavior.

> Prerequisites: [01-tools](./01-tools.md), [../01-core/04-query-and-chat-engines](../01-core/04-query-and-chat-engines.md).

## The agent loop

```text
user message + history
        │
        ▼
 ┌─► LLM (sees tool names, descriptions, schemas)
 │      │
 │      ├─ "call tool X with args"  ──► your code runs X ──► result appended to context ─┐
 │      │                                                                                 │
 │      └─ "final answer" ───────────────────────────────────────────────► return        │
 └─────────────────────────────────────────────────────────────────────────────────────────┘
```

Each trip around the loop is a full LLM call carrying the whole conversation so far (including every tool result). Cost and latency grow with the number of steps, and context grows with every result.

The agent needs an LLM with **tool/function calling** support. That is where model choice matters: weaker models call tools wrongly, loop, or ignore them.

## A first agent

In LlamaIndex.TS, agents live in the `@llamaindex/workflow` package. `agent()` builds a single-agent workflow:

```ts
import { agent } from "@llamaindex/workflow";
import { openai } from "@llamaindex/openai";
import { tool } from "llamaindex";
import { z } from "zod";

const sum = tool({
  name: "sum",
  description: "Add two numbers",
  parameters: z.object({ a: z.number(), b: z.number() }),
  execute: ({ a, b }) => String(a + b),
});

const divide = tool({
  name: "divide",
  description: "Divide a by b",
  parameters: z.object({ a: z.number(), b: z.number() }),
  execute: ({ a, b }) => String(a / b),
});

const myAgent = agent({
  tools: [sum, divide],
  llm: openai({ model: "gpt-4o-mini" }),
});

const result = await myAgent.run("What is (5 + 5) divided by 2?");
console.log(result.data.result);   // the final text
console.log(result.data.message);  // { role: "assistant", content: "..." }
```

Note that the LLM here is passed to the agent (`openai(...)` from `@llamaindex/openai`), not taken from `Settings.llm`. Set it explicitly.

Create the agent **once** at startup and reuse it, not per request. In serverless handlers, the docs initialize it lazily and cache it.

## Agentic RAG

Give the agent your query engine as a tool and it decides when to retrieve, how many times, and with what query:

```ts
const ragAgent = agent({
  tools: [
    index.queryTool({
      metadata: {
        name: "sf_budget",
        description: "Answers detailed questions about the 2023-2024 San Francisco city budget.",
      },
      options: { similarityTopK: 10 },
    }),
    sum,
  ],
  llm: openai({ model: "gpt-4o-mini" }),
});

const res = await ragAgent.run(
  "What is the total budget, and what would 2% of it be?",
);
```

The agent can retrieve the total, then call `sum`/arithmetic tools for the follow-up, something a plain query engine can't do.

## Structured output

Ask for a typed object alongside the text by passing a Zod schema as `responseFormat`:

```ts
const schema = z.object({ temperature: z.number(), humidity: z.number() });

const result = await weatherAgent.run("What's the weather in Tokyo?", {
  responseFormat: schema,
});

console.log(result.data.result);  // natural-language answer
console.log(result.data.object);  // { temperature: 72, humidity: 50 } validated shape
```

## Streaming and observing the loop

`runStream` yields events as the agent works. Filter them with the exported event types:

```ts
import { agentStreamEvent, agentToolCallEvent } from "@llamaindex/workflow";

const events = myAgent.runStream("What is (5 + 5) divided by 2?");

for await (const event of events) {
  if (agentToolCallEvent.include(event)) {
    console.log(`\n[tool] ${event.data.toolName}`);
  }
  if (agentStreamEvent.include(event)) {
    process.stdout.write(event.data.delta); // text tokens as they arrive
  }
}
```

This is also your main debugging tool: you can see the tool the agent chose and in what order. In an HTTP handler, the docs wrap these events in a `ReadableStream` to stream to the browser (see `../04-production/05-serving-and-integration.md`).

## Conversation history

`run` accepts a `chatHistory` option (an array of chat messages) next to `userInput`, so you can supply prior turns yourself:

```ts
const result = await myAgent.run("And what about 3 + 4?", {
  chatHistory: previousMessages,
});
```

The multi-agent workflow constructor also accepts an optional predefined `memory`. I could not verify a documented multi-turn memory pattern for the single-agent `agent()` helper in the TypeScript docs, so the safest approach is to own history in your application (store messages per session, pass them back in) and treat per-request agents as stateless. Check the current docs if you need built-in memory.

## Agent or query engine?

| Question type | Use |
|---|---|
| "What does the policy say about X?" (one lookup) | **Query engine** |
| Fixed multi-source pattern ("compare A and B") | `SubQuestionQueryEngine` or a router ([../02-rag/06-query-transforms-and-routing](../02-rag/06-query-transforms-and-routing.md)) |
| Needs calculation, API calls, or data from several tools in an unpredictable order | **Agent** |
| Steps are known and fixed, you want control and cheap retries | **Workflow** ([03-workflows](./03-workflows.md)) |
| Safety-critical, auditable processes | Workflow with explicit steps (and human approval) |

Questions to ask before reaching for an agent:

1. Could a retriever and one prompt do it? Then do that: it's faster, cheaper, and testable.
2. Is the sequence of steps actually unknown in advance? If you can draw the flowchart, build it as a workflow.
3. Can you tolerate variable cost and latency? Agent runs can take 3 to 10+ LLM calls.

A good default is: query engine first, workflow when the steps are known, agent when the model genuinely has to choose.

## Cost, latency and reliability

- **Calls multiply.** Every tool call adds an LLM round trip. A "simple" agentic RAG question can cost several times a plain query engine.
- **Context grows.** All previous tool outputs ride along on later calls. Keep tool results short.
- **Non-determinism.** The same question can take different paths. Evaluate on a set of questions, not one demo (see `../04-production/02-testing-and-evaluation.md`).
- **Loops.** An agent that can't find an answer may retry the same failing call. Set limits. Check whether your version exposes a maximum-steps or timeout option on the agent. If not, build the loop yourself with an explicit counter in a workflow ([03-workflows](./03-workflows.md)).
- **Failure handling.** Tools should return actionable errors so the model can adjust.

## Security notes

Retrieved documents and tool results are *data*, but a model may treat instructions inside them as commands (prompt injection). The risk is real as soon as an agent has write tools or access to other users' data. Mitigations: read-only tools by default, server-side authorization inside each tool, no secrets in prompts, human confirmation for consequential actions. Details in `../04-production/04-security.md`.

## Common mistakes

**Using an agent where one retrieval would do.** Slower, costlier, less predictable.

**Too many tools.** More tools means more confusion about which to use. Start with a few and sharply distinct descriptions.

**Not setting the LLM on the agent**, then wondering why it uses a different model than `Settings.llm`.

**Unbounded loops** with no cap on steps, time or cost.

**Giant tool outputs** inflating every subsequent call.

**Judging the agent by one successful run.** Test on a spread of questions, including ones that should be refused or answered "I don't know".

**Re-creating the agent on every request**, rebuilding indexes or tools each time.

## Debugging

1. Stream events and read the sequence: which tools, in what order, with what arguments?
2. If the wrong tool is chosen, fix descriptions before changing the model.
3. If the right tool returns the wrong thing, test that tool alone, outside the agent.
4. If it loops, look at the repeated call's result: is the error message useful to the model?
5. Compare a failing run with a weaker and a stronger model; if only the strong one works, your prompts/tools rely on capability you may not be able to afford.

## Quick Summary

- An agent is an LLM in a loop with tools; it trades predictability and cost for flexibility.
- `agent({ tools, llm })` from `@llamaindex/workflow`; run with `await agent.run(msg)`; result in `result.data.result` / `result.data.message`.
- `index.queryTool(...)` makes agentic RAG; `responseFormat` gives typed output; `runStream` plus `agentToolCallEvent` / `agentStreamEvent` lets you observe and stream.
- Pass `chatHistory` for multi-turn use and own session storage in your app.
- Default order: query engine, then workflow for known steps, then agent when choices are open-ended.
- Always cap steps and time, keep tool outputs small, and evaluate on many questions.

## Next

[03-workflows.md](./03-workflows.md): event-driven workflows for when you want the control flow in your hands, including loops, branching and multi-agent handoffs.