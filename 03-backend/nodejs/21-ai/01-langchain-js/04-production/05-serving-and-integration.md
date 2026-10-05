# Serving and Integration

You've built a chain or agent; now other code has to call it. This note covers the shape of a production endpoint: streaming responses, per-conversation state, cancellation, serverless gotchas, the managed deployment option, and connecting a frontend.

> Checked against the LangChain JS 1.x frontend and LangSmith Deployment docs where noted. The HTTP examples are generic Web-standard code, not LangChain-specific APIs; adapt them to your framework.

**Prerequisites:** [Streaming](../01-core/06-streaming.md), [Agents](../03-tools-and-agents/02-agents.md), [Reliability and Cost](./03-reliability-and-cost.md), [Security](./04-security.md)

---

## Two ways to serve

| Option | You write | You get | Pick when |
|---|---|---|---|
| **Your own HTTP endpoint** (Express, Hono, Next.js route, serverless function) | The route, auth, streaming, persistence wiring | Full control, fits existing infrastructure | Simple apps, existing backend, custom auth |
| **LangSmith Deployment / Agent Server** | A `langgraph.json` pointing at your agent | Durable execution, streaming, threads and runs API, scaling | Long-running or stateful agents, you want a managed runtime |

Both run the same agent code. Start with your own endpoint if you only need request/response and streaming.

---

## A basic endpoint

Keep the agent or chain at **module scope** (created once), and handle one request per call:

```ts
// agent.ts
import { createAgent } from "langchain";
import { PostgresSaver } from "@langchain/langgraph-checkpoint-postgres";

const checkpointer = PostgresSaver.fromConnString(process.env.DATABASE_URL!);

export const agent = createAgent({
  model: "gpt-5-nano",
  tools,
  systemPrompt: "...",
  checkpointer,
});
```

```ts
// route.ts (Web-standard Request/Response; works in Next.js route handlers, Hono, and others)
import { agent } from "./agent";

export async function POST(req: Request) {
  const user = await authenticate(req);            // your auth; reject if missing
  const { threadId, message } = await req.json();

  const result = await agent.invoke(
    { messages: [{ role: "user", content: message }] },
    {
      configurable: { thread_id: `${user.id}:${threadId}` },  // namespaced per user
      context: { userId: user.id },                           // trusted identity for tools
      metadata: { userId: user.id },                          // searchable in traces
    },
  );

  return Response.json({ answer: result.messages.at(-1)?.content });
}
```

Key points:

- **Authenticate first.** The model is not an access-control layer.
- **Namespace `thread_id` with the authenticated user id** so one user can't read another's thread by guessing an id. Never use a client-supplied thread id as-is.
- Pass identity via `context`, not the prompt ([Security](./04-security.md)).
- Use a persistent checkpointer. In-memory state breaks as soon as you run more than one instance or restart.
- Validate the request body (size, type) before it reaches the model.

---

## Streaming to clients

Streaming turns a 10-second wait into a response that starts in about a second. Over plain HTTP you have two common shapes:

### Streamed response body

```ts
export async function POST(req: Request) {
  const { question } = await req.json();
  const stream = await chain.stream({ question });

  const body = new ReadableStream({
    async start(controller) {
      const enc = new TextEncoder();
      try {
        for await (const piece of stream) controller.enqueue(enc.encode(piece));
        controller.close();
      } catch (err) {
        controller.error(err);
      }
    },
  });

  return new Response(body, { headers: { "Content-Type": "text/plain; charset=utf-8" } });
}
```

### Server-Sent Events (SSE)

Use SSE when you need to send **different kinds of events** (tokens, tool-call notices, final metadata, errors). Each event is `data: <json>` followed by a blank line, with `Content-Type: text/event-stream`:

```ts
const enc = new TextEncoder();
const sse = (event: unknown) => enc.encode(`data: ${JSON.stringify(event)}\n\n`);

// inside start(controller):
controller.enqueue(sse({ type: "token", text: chunk.text }));
// ...
controller.enqueue(sse({ type: "done", usage }));
```

Proxies and CDNs sometimes **buffer** streamed responses, which makes streaming look broken in production while it works locally. Check your reverse proxy's buffering settings and any platform limits on response duration.

For agents, stream tokens, tool calls and progress with `agent.stream(...)` and a `streamMode` (or the newer event-streaming API) and translate those into your event format ([Streaming](../01-core/06-streaming.md)).

---

## Cancellation and timeouts

If the client disconnects, stop the model call. Otherwise you keep paying for tokens nobody will read. Connect the request's abort signal to the run:

```ts
const result = await chain.invoke(input, { signal: req.signal });
```

`RunnableConfig` supports a `signal`, but confirm the exact behavior for your chain and provider in the current reference. Also set an overall request deadline so one slow run can't hold a worker indefinitely.

---

## Serverless and edge

- **Flush traces** before the function returns: `await awaitAllCallbacks()` and `LANGCHAIN_CALLBACKS_BACKGROUND=false` ([Tracing and Debugging](./01-tracing-and-debugging.md)).
- **Create heavy objects at module scope** (models, vector store clients, agents) so warm invocations reuse them, but expect cold starts.
- **Don't use `MemorySaver` or `MemoryVectorStore`**: instances don't share memory, and state vanishes. Use a database-backed checkpointer and vector store.
- **Mind duration limits.** Long agent runs can exceed a function's time limit; use streaming, a queue or background worker, or a long-running runtime.
- **Check runtime compatibility.** Some Node-only dependencies (file system, native modules) don't run in edge runtimes. Verify the packages you use.
- Keep **connection pooling** in mind for Postgres-backed checkpointers and stores.

---

## Frontends

For React, Vue, Svelte and Angular, LangChain ships a `useStream` hook (from `@langchain/react`, `@langchain/vue`, `@langchain/svelte`; Angular uses `injectStream` from `@langchain/angular`). It talks to an agent server that exposes the LangGraph streaming API:

```tsx
import { useStream } from "@langchain/react";

const stream = useStream<typeof agent>({
  apiUrl: "http://localhost:2024",
  assistantId: "agent",
});

return stream.messages.map((msg) => <Message key={msg.id} message={msg} />);
```

What the docs list as requirements: a typed agent from the backend (for type inference), an API endpoint the frontend can reach, and a checkpointer for durable thread state. If you serve through your own custom endpoint instead, you consume your own stream format with `fetch` and render incrementally, as in the streaming examples above.

**Never call the model provider from the browser with your key.** The browser talks to your backend; the backend holds secrets and enforces auth.

---

## Managed deployment: LangSmith Deployment

LangSmith Deployment is LangChain's runtime for agent workloads, with durable execution, streaming and scaling. You describe your agent in a `langgraph.json` and deploy with the LangGraph CLI:

```json
{
  "node_version": "20",
  "dependencies": ["./agent"],
  "graphs": {
    "agent": "./src/agent.ts:graph"
  }
}
```

Once deployed you work with three concepts: **assistants** (configuration), **threads** (state) and **runs** (execution), exposed by the Agent Server. The docs list managed (cloud, on a Plus plan or higher) and self-hosted (Kubernetes, Enterprise plan) options. Check current plan requirements and pricing before committing, and note how the checkpointer is provisioned in each mode: the docs say it is provisioned automatically on LangSmith deployments, while locally you configure one yourself.

---

## Integrating with other systems

Common patterns:

- **Webhooks and queues.** For slow or batch work (summarize 1,000 documents), accept the request, enqueue a job, and notify on completion. Don't hold an HTTP request open for minutes.
- **Idempotency.** Clients and queues retry. Use request ids so the same job isn't processed (and billed) twice.
- **Structured output at boundaries.** When another system consumes the result, return validated JSON from `withStructuredOutput` or `responseFormat`, not prose ([Structured Output](../01-core/05-structured-output.md)).
- **MCP and external tools.** Tools from other systems can be exposed to agents. Treat their results as untrusted input and apply the same least-privilege rules ([Tools](../03-tools-and-agents/01-tools.md)).
- **Versioning.** Prompts, tools and models are part of your API's behavior. Tag runs with a version in metadata, evaluate before rollout, and keep a rollback path.

---

## Operational checklist

- [ ] Auth on every route; thread ids namespaced to the user
- [ ] Persistent checkpointer and vector store (not in-memory)
- [ ] Streaming works end to end through your proxy or CDN
- [ ] Abort on client disconnect; request deadline; `timeout` and `maxTokens` set
- [ ] Agent loop limits and retries configured ([Reliability and Cost](./03-reliability-and-cost.md))
- [ ] Per-user rate limits and spend alerts
- [ ] Trace flushing in serverless; separate tracing projects per environment
- [ ] Secrets server-side only; PII handling decided ([Security](./04-security.md))
- [ ] Health check endpoint, structured logs with request id, token usage logged
- [ ] Evaluation suite run before each release

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Creating the agent or model inside every request | Create at module scope |
| Using the client-supplied `thread_id` directly | Namespace with the authenticated user id |
| `MemorySaver` or `MemoryVectorStore` in a multi-instance or serverless deployment | Database-backed checkpointer and store |
| Streaming works locally, arrives in one lump in prod | Disable proxy buffering; check platform response limits |
| Client disconnects but the run keeps going | Pass an abort signal and set a deadline |
| Long agent run inside a short serverless function | Stream, queue it, or use a long-running runtime |
| Traces missing from serverless | Flush callbacks before returning |
| Provider key in the browser | Backend proxy only |
| No idempotency on retried requests | Request ids and de-duplication |
| Shipping prompt or model changes without evaluation | Evaluate first; tag runs with a version |

---

## Quick Summary

- Serve either with your own endpoint (full control) or LangSmith Deployment (managed durable runtime); the agent code is the same.
- Authenticate, namespace `thread_id` by user, pass identity as `context`, and use a persistent checkpointer.
- Stream over a streamed body or SSE; watch for proxy buffering; abort work when the client disconnects.
- In serverless: module-scope clients, no in-memory state, flush traces, respect duration limits.
- Frontends use `useStream` against an agent server, or your own stream format; secrets never leave the backend.
- Run through the operational checklist before launch.

**Next:** [Interview Questions](../reference/interview.md)
