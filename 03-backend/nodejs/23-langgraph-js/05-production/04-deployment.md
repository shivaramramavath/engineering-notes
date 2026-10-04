# Deployment

A compiled graph is a library object, not a server. Deploying it means deciding where it runs, where its state lives, and how requests (and streams) reach it. The good news: because state lives in the checkpointer, a graph process is essentially stateless and scales horizontally like any web service.

Prerequisites: [Persistence](../03-stateful/01-persistence.md), [Streaming](../02-agents/02-streaming.md), [Reliability](./02-reliability.md), [Security](./03-security.md).

## Two ways to run it

| | Your own Node service | A LangChain agent server |
|---|---|---|
| What you do | Import the graph into Express, Fastify, Hono, etc. | Point the server at your graph definition; it provides the API |
| Control | Full | Less, in exchange for built-in endpoints for threads, runs and streaming |
| You provide | HTTP layer, auth, queueing, storage, scaling | Config file, secrets, hosting (managed or self-hosted) |
| Choose when | You already have a backend and want the graph as one component | You want a ready-made agent API fast |

LangChain offers a hosted and self-hostable server for this (its product naming has changed over time, so check the current docs). It's configured with a `langgraph.json` that points at your exported graph, roughly:

```json
{
  "node_version": "20",
  "graphs": { "agent": "./src/agent.ts:graph" },
  "env": ".env"
}
```

and run locally with the `@langchain/langgraph-cli` package. Treat that snippet as orientation and confirm field names against the docs for your version. The rest of this note covers the **self-hosted** route, since the concerns (state, streaming, rollout) apply either way.

## Process layout

```
          ┌───────────────────────┐
client ─► │ API instance (Node)    │──┐
          │  graph (compiled once) │  │
          └───────────────────────┘  ├─► Postgres (checkpoints, store)
          ┌───────────────────────┐  │
client ─► │ API instance (Node)    │──┘
          └───────────────────────┘
                 ▲ optional: queue ─► worker instances (same graph)
```

- **Compile once at startup** and reuse the graph across requests. Don't rebuild it per request.
- **State lives in the database**, not in the process. Any instance can serve any thread, which is what lets you scale out and restart freely. This requires a durable checkpointer ([Persistence](../03-stateful/01-persistence.md)); `MemorySaver` is for tests only.
- **Run `checkpointer.setup()` at deploy or migration time**, not on every request.
- Size the database connection pool for concurrent runs; each in-flight run uses connections for checkpoint reads and writes.

## Serving a streamed response

For a chat UI, stream tokens over Server-Sent Events. The shape, using Node's built-in HTTP types:

```ts
import type { IncomingMessage, ServerResponse } from "node:http";

async function streamChat(
  req: IncomingMessage,
  res: ServerResponse,
  input: unknown,
  config: { configurable: Record<string, unknown> },
) {
  const ac = new AbortController();
  req.on("close", () => ac.abort()); // stop work if the client leaves

  res.writeHead(200, {
    "Content-Type": "text/event-stream",
    "Cache-Control": "no-cache",
    Connection: "keep-alive",
  });

  try {
    for await (const [msg, meta] of await graph.stream(input as never, {
      ...config,
      streamMode: "messages",
      signal: ac.signal,
    })) {
      if (meta.langgraph_node !== "agent") continue;
      const text = textOf(msg.content); // helper from the Streaming note
      if (text) res.write(`data: ${JSON.stringify({ text })}\n\n`);
    }
    res.write("event: done\ndata: {}\n\n");
  } catch {
    // Don't leak internal error details to the client.
    res.write(`event: error\ndata: ${JSON.stringify({ message: "Something went wrong" })}\n\n`);
  } finally {
    res.end();
  }
}
```

Things to get right:

- **Abort on disconnect** so abandoned requests don't keep consuming model calls.
- **Send plain JSON** you've shaped, not raw message objects or state ([Security](./03-security.md)).
- **Proxies can buffer SSE.** If tokens arrive in bursts, check the reverse proxy and CDN settings for response buffering.
- **Build `config` on the server** from the authenticated user; never forward client-supplied config ([Security](./03-security.md)).

## Long-running work

A request/response cycle is the wrong place for a run that takes minutes, or one that pauses for a person. Use a job pattern:

1. The API receives the request, authenticates it, and enqueues a job containing the `thread_id` and input.
2. A worker runs the graph (`invoke` or `stream`) and records progress or results.
3. The client polls, subscribes (SSE/WebSocket), or gets a webhook.
4. A crashed worker's job is retried with the resume-or-start helper from [Reliability](./02-reliability.md), so finished steps aren't repeated.

For human-in-the-loop, expose two operations: **start** (runs until the interrupt and returns the pending question) and **resume** (accepts the answer and continues the same `thread_id`). See [Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md).

Serverless platforms with short execution limits and no long-lived connections suit quick, non-streaming runs; for long or streamed runs prefer long-lived containers or workers.

## Packaging

A minimal container for a compiled TypeScript service:

```dockerfile
FROM node:20-slim
WORKDIR /app
COPY package*.json ./
RUN npm ci --omit=dev
COPY dist ./dist
CMD ["node", "dist/server.js"]
```

- Pin dependency versions with a lockfile. LangGraph and its companion packages evolve; upgrade deliberately and run your tests and evals ([Testing](./01-testing.md)).
- Pass secrets (model keys, `DATABASE_URL`) through environment variables or a secret manager, never in the image.
- Add a readiness check that verifies database connectivity.

## Deploying changes while threads are in flight

This is the production concern specific to checkpointed graphs: **old threads were saved with the old graph**.

- **Renaming or removing a node** can break threads that are paused or mid-run, because their saved "what's next" names a node that no longer exists.
- **Renaming or removing a state key** leaves old checkpoints with data your new code doesn't expect (and missing data it does).
- **Changing a reducer's semantics** changes how future updates merge into old state.

Safer rollout practices:

- Make state changes **additive**: new keys with defaults, handled when absent. Tolerate missing keys in nodes.
- Keep old node names until no thread can still be waiting on them, or add an alias node that forwards.
- For breaking changes, **version the graph** (a new graph id or new thread namespace) and let old threads finish on the old version.
- Roll out gradually (canary) and watch error rates, step counts and cost per run.
- Test resuming a checkpoint saved by the previous version as part of release checks.

## Observability

- **Tracing** of prompts, tool calls and token usage (LangSmith tracing is switched on through environment variables; see its docs).
- **Structured logs** carrying `thread_id`, request id, and node names.
- **Metrics**: latency overall and per node, error and retry rates, steps per run, recursion-limit hits, tokens and cost per run, queue depth for workers.
- **Alerts** on cost spikes and rising step counts, which often signal a loop.

## Common mistakes

- **`MemorySaver` in production**, so every restart or second instance loses all threads.
- **Compiling the graph per request.**
- **Running `setup()` on each request** instead of at deploy time.
- **Long runs inside an HTTP request** that time out at the proxy.
- **No abort on disconnect**, wasting model spend.
- **Renaming nodes or state keys** while threads are in flight.
- **Leaking raw errors or state to clients** through the stream.

## Debugging in production

- Find the thread by id, then read its `getState` and `getStateHistory` (behind an authenticated admin path) to see where it stopped ([Time Travel](../03-stateful/04-time-travel.md)).
- Correlate a bad answer to its trace; look at the exact prompt and tool results.
- Reproduce with the thread's state in a test before changing code ([Testing](./01-testing.md)).

## Quick summary

- A graph is a library object; wrap it in your own service or use a hosted/self-hosted agent server.
- Compile once, keep state in a durable checkpointer (Postgres), and scale instances horizontally.
- Stream via SSE with abort-on-disconnect and sanitized chunks; push long or human-gated work to queues and workers.
- Change graphs carefully: additive state, stable node names, versioned breaking changes.
- Instrument everything: traces, logs, per-node latency, cost and step counts.

**Next:** [Interview Questions](../reference/interview.md)
