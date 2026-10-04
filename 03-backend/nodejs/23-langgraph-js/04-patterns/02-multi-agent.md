# Multi-Agent

A multi-agent system splits a job across several model-driven workers, each with its own prompt and tools, and coordinates them with graph control flow. In LangGraph an "agent" is just a node (or a subgraph, see [Subgraphs](./01-subgraphs.md)), and "agents talking to each other" means they read and write shared state.

Prerequisites: [ReAct Agent](../02-agents/01-react-agent.md), [Routing and Parallelism](../01-core/04-routing-and-parallelism.md) (`Command`), [Subgraphs](./01-subgraphs.md).

## Should you do this at all?

Start with a single agent and a good set of tools. Reach for multiple agents when:

- one agent's prompt or toolset has grown so large that it picks tools badly
- the work has distinct roles that benefit from different instructions
- independent sub-tasks can run in parallel
- different teams own different capabilities

Every extra agent adds model calls, latency, cost and failure modes. "More agents" is not automatically "better."

## Pattern 1: Supervisor

One model decides who acts next. Workers do their job and report back to the supervisor.

```
          ┌────────► researcher ─┐
START ─► supervisor ◄────────────┤
          └────────► writer ─────┘
              │ FINISH
              ▼
             END
```

The supervisor's routing decision is a model call, so it lives in a **node** and routes with `Command` (not in a conditional-edge router, which should stay cheap and pure).

```ts
import { ChatAnthropic } from "@langchain/anthropic";
import { AIMessage, SystemMessage } from "@langchain/core/messages";
import {
  StateGraph,
  MessagesAnnotation,
  Command,
  START,
  END,
} from "@langchain/langgraph";
import { z } from "zod";

const model = new ChatAnthropic({ model: "claude-sonnet-5-5" });

const Route = z.object({
  next: z.enum(["researcher", "writer", "FINISH"]),
});
const router = model.withStructuredOutput(Route);

const supervisor = async (state: typeof MessagesAnnotation.State) => {
  const { next } = await router.invoke([
    new SystemMessage(
      "You coordinate a researcher and a writer. Choose who should act next, " +
        "or FINISH once the user's request is fully answered.",
    ),
    ...state.messages,
  ]);
  return new Command({ goto: next === "FINISH" ? END : next });
};

const makeWorker =
  (name: string, instruction: string) =>
  async (state: typeof MessagesAnnotation.State) => {
    const reply = await model.invoke([
      new SystemMessage(instruction),
      ...state.messages,
    ]);
    return { messages: [new AIMessage({ content: reply.content, name })] };
  };

const graph = new StateGraph(MessagesAnnotation)
  .addNode("supervisor", supervisor, { ends: ["researcher", "writer", END] })
  .addNode("researcher", makeWorker("researcher", "Gather key facts. Be brief."))
  .addNode("writer", makeWorker("writer", "Turn the facts so far into a clear answer."))
  .addEdge(START, "supervisor")
  .addEdge("researcher", "supervisor")
  .addEdge("writer", "supervisor")
  .compile();

const out = await graph.invoke({
  messages: [{ role: "user", content: "Explain what a checkpointer does." }],
});
```

Points to notice:

- Workers always return to the supervisor via plain edges; the supervisor alone decides when to stop.
- `name` on each worker's `AIMessage` tells the supervisor (and you, in traces) who said what.
- Workers here are single model calls. In practice they're often ReAct agents; wrap them as subgraphs ([Subgraphs](./01-subgraphs.md)).
- **Bound the loop.** A supervisor can ping-pong. The recursion limit is the backstop (`GraphRecursionError`); better, tell the supervisor how to finish and keep an eye on step counts in traces.

## Pattern 2: Handoffs (peer-to-peer)

Instead of a central router, each agent decides who takes over. The agent calls a "transfer" tool, and the tool returns a `Command` that navigates to the other agent, typically with `graph: Command.PARENT` when the agents live in subgraphs ([Subgraphs](./01-subgraphs.md)).

Differences from a supervisor: there's no coordinator to bottleneck, the active agent changes as the conversation moves, and the flow is harder to predict and test. Good for "customer talks to a sales agent that hands off to billing."

One rule to remember: a transfer tool call is still a tool call, so the history needs a matching `ToolMessage` for it, or the next model call will be rejected for an unpaired tool call.

Ready-made helpers exist for both patterns (`@langchain/langgraph-supervisor` and `@langchain/langgraph-swarm`). They build graphs like the ones above. Check their current docs for options; the underlying mechanics are what this note and the earlier ones cover.

## Pattern 3: Hierarchy

A supervisor whose workers are themselves supervisors (each a subgraph). It's the supervisor pattern applied recursively and is useful when a flat team becomes too large for one router to manage. Reach for it only when a single supervisor demonstrably struggles.

## Managing context between agents

What each agent sees matters more than the topology.

- **Share the full transcript** (as above): simple, but every agent pays for everyone's output and can get distracted.
- **Share summaries**: have a worker return only its conclusion rather than its whole reasoning trace. This keeps shared state small and focused.
- **Give agents private state**: with the wrapper approach from [Subgraphs](./01-subgraphs.md), a sub-agent's internal messages stay internal.

A common design: shared `messages` for the user-visible conversation, plus dedicated keys for structured hand-off data (a plan, a list of findings), so agents don't have to parse each other's prose.

## Common mistakes

- **Using multi-agent when one agent with tools would do.**
- **Unbounded supervisor loops.** No finish condition, or a vague one.
- **Dumping everything into shared messages**, so context balloons and quality drops.
- **Unpaired tool calls** after handoffs, which providers reject.
- **Routing in a conditional-edge function with a model call.** Put model decisions in nodes.
- **No way to tell agents apart in the transcript.** Set `name` on messages.

## Debugging

- Stream `updates` with `subgraphs: true` ([Streaming](../02-agents/02-streaming.md)) to see which agent acted at each step.
- Read the supervisor's decisions in sequence; most failures are a bad routing choice early on.
- Test workers individually before composing them, and check the supervisor with fixed transcripts ([Testing](../05-production/01-testing.md)).
- If cost or latency is high, count model calls per request; multi-agent multiplies them.

## Quick summary

- Agents are nodes or subgraphs that coordinate through shared state; "communication" is reading and writing it.
- **Supervisor**: central router in a node, workers return to it. **Handoffs**: agents transfer control with `Command`. **Hierarchy**: supervisors of supervisors.
- Control context deliberately: share summaries and structured keys, keep internals private.
- Bound loops, name your messages, and prefer a single agent until you have a reason to split.

**Next:** [Agentic RAG](./03-agentic-rag.md)
