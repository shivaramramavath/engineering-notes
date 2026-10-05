# Agentic RAG

Classic RAG is a fixed pipeline: retrieve documents for every question, stuff them into the prompt, generate. **Agentic RAG** puts a model in charge of the retrieval step: it decides *whether* to retrieve, checks whether what came back is any good, and retries with a better query if not. In LangGraph that's a graph with a short, bounded loop.

Prerequisites: [ReAct Agent](../02-agents/01-react-agent.md) (tool calls and `ToolNode`), [Routing and Parallelism](../01-core/04-routing-and-parallelism.md) (conditional edges and loops).

This note is about the **control flow**. Chunking, embeddings and the vector store are separate concerns, so retrieval appears here as one function you supply.

## Why not plain RAG?

| Fixed pipeline | Agentic |
|---|---|
| Always retrieves, even for "hi" | Retrieves only when the question needs documents |
| One shot: bad query means bad answer | Grades results and can rewrite the query and retry |
| Predictable cost and latency | Extra model calls (grading, rewriting) and variable latency |
| Simple to debug | More moving parts |

If your questions all need retrieval and your index is good, the fixed pipeline is often enough. Go agentic when queries are messy, retrieval quality varies, or a share of questions don't need documents.

## The graph

```
START ─► agent ──(tool call?)── no ──► END
           ▲          │ yes
           │          ▼
           │       retrieve ─► grade ──► relevant, or retries used up? ── yes ─► generate ─► END
           │                      │
           │                      no
           │                      ▼
           └────────────────── rewrite
```

- **agent**: a model bound to a search tool decides to search or answer directly.
- **retrieve**: runs the search tool.
- **grade**: a model judges whether the results can answer the question and records the verdict in state.
- **rewrite**: reformulates the query for another attempt.
- **generate**: writes the final answer from the retrieved context.

Grading is a model call, so it's a **node that writes a verdict to state**; the conditional edge then just reads that state. Routers stay cheap and pure, as in [Routing and Parallelism](../01-core/04-routing-and-parallelism.md).

## Implementation

```ts
import { ChatAnthropic } from "@langchain/anthropic";
import { tool } from "@langchain/core/tools";
import {
  AIMessage,
  HumanMessage,
  SystemMessage,
  ToolMessage,
  type BaseMessage,
} from "@langchain/core/messages";
import {
  StateGraph,
  Annotation,
  MessagesAnnotation,
  START,
  END,
} from "@langchain/langgraph";
import { ToolNode } from "@langchain/langgraph/prebuilt";
import { z } from "zod";

// Supplied by you: query your vector store / search index.
declare function searchIndex(
  query: string,
): Promise<{ id: string; text: string }[]>;

const searchDocs = tool(
  async ({ query }) => {
    const hits = await searchIndex(query);
    return hits.map((h) => `[${h.id}] ${h.text}`).join("\n\n") || "No results.";
  },
  {
    name: "search_docs",
    description:
      "Search the product documentation. Use for any question about the product.",
    schema: z.object({ query: z.string().describe("A focused search query") }),
  },
);

const model = new ChatAnthropic({ model: "claude-sonnet-5-5" });
const modelWithSearch = model.bindTools([searchDocs]);
const grader = model.withStructuredOutput(
  z.object({
    relevant: z
      .boolean()
      .describe("Do the search results contain information that answers the question?"),
  }),
);

const State = Annotation.Root({
  ...MessagesAnnotation.spec,
  relevant: Annotation<boolean>(),
  rewrites: Annotation<number>({ reducer: (a, b) => a + b, default: () => 0 }),
});
type S = typeof State.State;

const text = (m?: BaseMessage) =>
  typeof m?.content === "string" ? m.content : JSON.stringify(m?.content ?? "");
const lastOf = (s: S, cls: typeof HumanMessage | typeof ToolMessage) =>
  [...s.messages].reverse().find((m) => m instanceof cls);

const agent = async (s: S) => ({
  messages: [
    await modelWithSearch.invoke([
      new SystemMessage(
        "Use search_docs for questions about the product. Answer directly otherwise.",
      ),
      ...s.messages,
    ]),
  ],
});

const grade = async (s: S) => {
  const question = text(lastOf(s, HumanMessage));
  const results = text(s.messages.at(-1)); // the ToolMessage from `retrieve`
  const verdict = await grader.invoke(
    `Question: ${question}\n\nSearch results:\n${results}\n\n` +
      "Do the results contain information that helps answer the question?",
  );
  return { relevant: verdict.relevant };
};

const rewrite = async (s: S) => {
  const question = text(lastOf(s, HumanMessage));
  const better = await model.invoke(
    "Rewrite this as a better documentation search query. " +
      `Return only the new query.\n\nOriginal: ${question}`,
  );
  return { messages: [new HumanMessage(text(better))], rewrites: 1 };
};

const generate = async (s: S) => {
  const question = text(s.messages.find((m) => m instanceof HumanMessage)); // the original question
  const context = text(lastOf(s, ToolMessage));
  const reply = await model.invoke([
    new SystemMessage(
      "Answer using only the context below and cite source ids in brackets. " +
        "If the context does not answer the question, say you don't know.\n\n" +
        `Context:\n${context}`,
    ),
    new HumanMessage(question),
  ]);
  return { messages: [reply] };
};

const afterAgent = (s: S) =>
  (s.messages.at(-1) as AIMessage).tool_calls?.length ? "retrieve" : END;

const afterGrade = (s: S) =>
  s.relevant || s.rewrites >= 2 ? "generate" : "rewrite";

const graph = new StateGraph(State)
  .addNode("agent", agent)
  .addNode("retrieve", new ToolNode([searchDocs]))
  .addNode("grade", grade)
  .addNode("rewrite", rewrite)
  .addNode("generate", generate)
  .addEdge(START, "agent")
  .addConditionalEdges("agent", afterAgent, ["retrieve", END])
  .addEdge("retrieve", "grade")
  .addConditionalEdges("grade", afterGrade, ["generate", "rewrite"])
  .addEdge("rewrite", "agent")
  .addEdge("generate", END)
  .compile();

const result = await graph.invoke({
  messages: [{ role: "user", content: "How do I rotate an API key?" }],
});
console.log(result.messages.at(-1)?.content);
```

Design choices worth noting:

- **The retry loop is bounded by state.** `rewrites` counts attempts and `afterGrade` stops at 2. Without a cap, a strict grader plus a hard question loops until the recursion limit.
- **When retries run out, generate anyway**, but the generation prompt says to admit when the context doesn't answer. Honest "I don't know" beats a confident invention.
- **`searchIndex` returns ids**, which flow through the tool output so the answer can cite sources.
- **`rewrite` appends the new query as a user message** to keep the example short. In a real system, give the rewritten query its own state key so it doesn't muddy the visible transcript.

## Production considerations

- **Retrieved text is untrusted input.** A document can contain instructions ("ignore previous instructions…") that the model may follow. Keep the generation prompt strict, don't give the answering step powerful tools, and see [Security](../05-production/03-security.md).
- **Cost and latency.** The grade-and-rewrite loop multiplies model calls. Use a smaller model for grading and rewriting if quality allows, and cap retries.
- **Context size.** Large chunks make prompts expensive and noisy. Limit how much of each result the tool returns.
- **Evaluate retrieval separately from generation.** If answers are wrong, first check whether the right documents were retrieved; a grader can't fix a bad index.
- **Permissions.** If documents have access controls, enforce them inside `searchIndex` using the caller's identity, not in the prompt.

## Common mistakes

- **No retry cap**, ending in `GraphRecursionError`.
- **A grader that's too strict or too lenient.** Strict loops and wastes calls; lenient lets irrelevant context through. Tune it with real examples.
- **Model calls inside the conditional-edge function.** Put grading in a node.
- **Vague tool description**, so the agent never (or always) searches. The description is what drives the decision.
- **Answering from memory when context is empty**, because the generation prompt didn't say to admit ignorance.
- **Returning whole documents** from the tool instead of focused passages.

## Debugging

- Stream `updates` and read the path taken: `agent → retrieve → grade → rewrite → agent …`. Unexpected paths point at the grader or the router.
- Log each query the agent and `rewrite` produce, and what `searchIndex` returned for it.
- Print the grader's verdict alongside the results it judged to see whether it's being unreasonable.
- Keep a small set of question/expected-source pairs and run it after every change ([Testing](../05-production/01-testing.md)).

## Quick summary

- Agentic RAG lets the model decide when to retrieve, grade the results, and rewrite the query, using a bounded loop in a graph.
- Model-based judgments (grading) live in **nodes** that write to state; edges just route on that state.
- Bound retries in state, and make the final step admit when the context isn't enough.
- Treat retrieved text as untrusted, evaluate retrieval and generation separately, and watch the cost of extra model calls.

**Next:** [Testing](../05-production/01-testing.md)
