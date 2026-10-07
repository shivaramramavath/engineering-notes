# Serving and Integration

A notebook script that builds an index and answers one question is not a service. Serving means: load data once, answer many concurrent requests, stream tokens, survive failures, and keep ingestion separate from the request path. This note covers the shape of that service in Node, Next.js and serverless environments, and how LlamaIndex fits next to other tools.

> Prerequisites: [../01-core/04-query-and-chat-engines](../01-core/04-query-and-chat-engines.md), [../03-tools-and-agents/02-agents](../03-tools-and-agents/02-agents.md), [03-reliability-and-cost](./03-reliability-and-cost.md).

## Architecture rules

```text
 INGESTION (job, cron, webhook)                 SERVING (stateless web service)
 ─────────────────────────────                  ───────────────────────────────
 readers → pipeline → vector store  ──────────► attach to store at startup
 (docstore + cache, idempotent)                 build engines/agents ONCE
                                                per request: auth → engine → stream
```

1. **Never ingest in the request path.** Parsing, chunking and embedding are slow and expensive. A separate job writes to a shared vector store; the web service only reads.
2. **Build heavy objects once.** Index handles, query engines, agents and tool definitions are created at startup (or lazily on first use, then cached), not per request.
3. **Keep the service stateless.** Conversation history, caches and index data live in shared stores (database, Redis, vector DB), not in process memory or local disk, so any instance can serve any request.
4. **Use a shared vector store in production.** A `persistDir` on local disk doesn't work across replicas or ephemeral containers ([../01-core/03-indexes-and-storage](../01-core/03-indexes-and-storage.md)).
5. **Stream.** Time to first token is what users feel.

## A Node HTTP server (Express)

```ts
import express from "express";
import { Settings, VectorStoreIndex } from "llamaindex";
import { OpenAI, OpenAIEmbedding } from "@llamaindex/openai";
import { PGVectorStore } from "@llamaindex/postgres";

Settings.llm = new OpenAI({ model: "gpt-4o-mini", maxRetries: 2, timeout: 20_000 });
Settings.embedModel = new OpenAIEmbedding({ model: "text-embedding-3-small" });

// startup: attach to existing vectors, build the engine once
const vectorStore = new PGVectorStore({ clientConfig: { connectionString: process.env.DATABASE_URL }, dimensions: 1536 });
const index = await VectorStoreIndex.fromVectorStore(vectorStore);
const queryEngine = index.asQueryEngine({ similarityTopK: 6 });

const app = express();
app.use(express.json({ limit: "32kb" }));          // cap input size

app.post("/api/ask", async (req, res) => {
  const question = String(req.body?.question ?? "").slice(0, 2000);
  if (!question) return res.status(400).json({ error: "question required" });

  try {
    const response = await queryEngine.query({ query: question });
    res.json({
      answer: response.toString(),
      sources: (response.sourceNodes ?? []).map((n) => n.node.metadata.source),
    });
  } catch (err) {
    console.error(err);
    res.status(502).json({ error: "upstream model error" });
  }
});

app.listen(3000);
```

Authentication, rate limiting and per-user filters belong in middleware in front of this ([04-security](./04-security.md)). Build the user-specific query engine with filters per request if they depend on identity; building a query engine is cheap, while building the index is not.

### Streaming over Server-Sent Events

```ts
app.post("/api/ask-stream", async (req, res) => {
  const question = String(req.body?.question ?? "").slice(0, 2000);

  res.setHeader("Content-Type", "text/event-stream");
  res.setHeader("Cache-Control", "no-cache");
  res.setHeader("Connection", "keep-alive");

  let closed = false;
  req.on("close", () => { closed = true; });          // client went away

  try {
    const stream = await queryEngine.query({ query: question, stream: true });
    for await (const chunk of stream) {
      if (closed) break;                              // stop spending tokens
      res.write(`data: ${JSON.stringify({ delta: chunk.response })}\n\n`);
    }
    res.write(`event: done\ndata: {}\n\n`);
  } catch (err) {
    res.write(`event: error\ndata: ${JSON.stringify({ message: "generation failed" })}\n\n`);
  } finally {
    res.end();
  }
});
```

Points that matter:

- Send an explicit **error event** on mid-stream failure; otherwise the client just sees a truncated answer.
- **Stop iterating on disconnect** so you don't generate tokens for nobody. Whether the underlying LLM request is cancelled depends on the library path; stopping the loop at least limits further work.
- Reverse proxies and CDNs may **buffer** responses and defeat streaming. Disable buffering for these routes (for example `X-Accel-Buffering: no` on nginx) and test through the real proxy.

## Next.js

LlamaIndex.TS documents Next.js integration. Key points from the docs:

**Config.** Wrap your Next config with `withLlamaIndex` so bundling works:

```js
// next.config.mjs
import withLlamaIndex from "llamaindex/next";

/** @type {import('next').NextConfig} */
const nextConfig = {};

export default withLlamaIndex(nextConfig);
```

**Route handler (App Router)** with a lazily created, cached agent (the docs' own "initialize once" pattern):

```ts
// app/api/chat/route.ts
import { agent } from "@llamaindex/workflow";
import { openai } from "@llamaindex/openai";
import { NextRequest, NextResponse } from "next/server";

let myAgent: ReturnType<typeof agent> | null = null;

function getAgent() {
  if (!myAgent) {
    myAgent = agent({
      tools: [/* your tools, e.g. index.queryTool(...) */],
      llm: openai({ model: "gpt-4o-mini" }),
    });
  }
  return myAgent;
}

export async function POST(req: NextRequest) {
  const { message } = await req.json();
  try {
    const result = await getAgent().run(message);
    return NextResponse.json({ response: result.data.result });
  } catch (e) {
    return NextResponse.json({ error: "Internal server error" }, { status: 500 });
  }
}
```

**Streaming from a route handler.** Wrap `runStream` in a `ReadableStream`:

```ts
// app/api/chat-stream/route.ts
import { agentStreamEvent } from "@llamaindex/workflow";

export async function POST(req: NextRequest) {
  const { message } = await req.json();
  const encoder = new TextEncoder();

  const stream = new ReadableStream({
    async start(controller) {
      try {
        for await (const event of getAgent().runStream(message)) {
          if (agentStreamEvent.include(event)) {
            controller.enqueue(encoder.encode(event.data.delta));
          }
        }
      } catch (err) {
        controller.enqueue(encoder.encode("\n[error]"));
      } finally {
        controller.close();
      }
    },
  });

  return new Response(stream, { headers: { "Content-Type": "text/plain; charset=utf-8" } });
}
```

Next.js specifics to remember:

- Run these handlers on the **Node.js runtime**, not Edge, unless you've checked every dependency (see below).
- Streaming routes need dynamic rendering (`export const dynamic = "force-dynamic"`) so responses aren't cached or buffered.
- **Never call the LLM from client components.** Keys live on the server only ([04-security](./04-security.md)).
- Serverless time limits apply to streaming too. A long agent run can be cut off by the platform timeout; set your own deadline below it.

## Serverless and edge runtimes

LlamaIndex.TS targets Node.js 20+, and the docs describe support for other runtimes (Deno, Bun, Vercel Edge "with some limitations", Cloudflare Workers). In practice:

- **Many classes are Node-only**: file readers like `PDFReader`, `SimpleDirectoryReader` (filesystem), and some vector store clients. In edge runtimes some classes aren't exported from the main entry and must be imported by path. Do ingestion in a Node job, not at the edge.
- **No local disk persistence.** A `persistDir` won't survive; use a hosted vector store.
- **Cold starts.** Keep startup light: lazy and dynamic imports (the serverless docs initialize the agent with dynamic `import()` inside the handler), reuse the initialized object across warm invocations (module-level cache), and avoid loading large local indexes.
- **Environment variables on edge runtimes** (such as Cloudflare Workers) aren't a global `process.env`; the docs show passing them into the library via `setEnvs` from `@llamaindex/env` at request time. Follow the serverless guide for your platform.
- **Connection limits.** Serverless functions can open many database connections. Use pooled connections or a pooler (for pgvector, a connection pooler in front of Postgres).
- **Timeouts** are strict; stream, and set deadlines below the platform limit ([03-reliability-and-cost](./03-reliability-and-cost.md)).

If an edge platform forces too many compromises, run the RAG service as a regular Node service and call it from your edge/frontend layer.

## `@llamaindex/server`

For hosting workflows or agents with minimal plumbing, LlamaIndex publishes `@llamaindex/server`: a Next.js-based server that exposes your workflow or agent as API endpoints, with an optional chat UI.

```bash
npm i @llamaindex/server
```

You supply a factory that returns your workflow (for example `() => agent({ tools, llm })`), and the server handles the HTTP layer and streaming. Details worth knowing from its README: events are converted to Server-Sent Events, and the server **filters** backend events down to a small set of text-streaming types so the frontend stays compatible with the Vercel AI SDK. Anything custom (extra event types, UI components) goes through its documented extension points. Follow the package README for current configuration since this layer evolves quickly.

Good for: prototypes, internal tools, quick chat front ends. For tight control of auth, rate limiting and routing, a plain Node/Next.js route handler is easier to reason about.

## Frontends and ecosystem integrations

- **Chat UI:** the `@llamaindex/chat-ui` package provides React chat components designed to work with LlamaIndex backends.
- **Vercel AI SDK:** an `@ai-sdk/llamaindex` adapter is listed in the AI SDK's docs for streaming LlamaIndex responses into `useChat`-style UIs. Check the adapter's page for the current API before adopting.
- **LangChain / LangGraph:** interoperate at the tool boundary ([../03-tools-and-agents/01-tools](../03-tools-and-agents/01-tools.md)). Keep one orchestrator per request path.
- **Observability:** wire tracing at startup, before LlamaIndex imports ([01-tracing-and-debugging](./01-tracing-and-debugging.md)).

## Conversation state

Agents and chat engines hold history in the instance. In a stateless service that means:

- Create a chat engine or call the agent **per request**, loading history from your database by session ID and passing it in (agents accept a `chatHistory` option on `run`; see [../03-tools-and-agents/02-agents](../03-tools-and-agents/02-agents.md)).
- Save the new turns back after the response completes.
- Cap history length (or summarize) so context and cost don't grow without bound.
- Never share one chat-engine instance between users.

## Deploying and operating

- **Config via environment variables**, validated at startup (fail fast if a key or DB URL is missing).
- **Health checks:** a cheap liveness endpoint, and a readiness check that verifies the vector store connection, not a full LLM call.
- **Graceful shutdown:** stop accepting requests, let in-flight streams finish (with a deadline), close DB pools.
- **Index versioning and zero-downtime re-index:** write the new index to a new table/collection/namespace, evaluate it against the golden set ([02-testing-and-evaluation](./02-testing-and-evaluation.md)), then switch the service's configuration to it. Keep the old one for rollback. This is also how you change the embedding model safely: you can't mix vector spaces.
- **Pin versions** of `llamaindex` and provider packages; upgrade via CI with evaluation gates.
- **Observability on day one:** request IDs, per-stage timings, token counts, error rates, cost per request.
- **Capacity:** LLM calls are I/O-bound, so a Node process handles many concurrent requests, but rate limits at the provider and connection limits at the database are the real ceilings. Test with realistic concurrency.

## Common mistakes

**Building the index (or reading files) inside the handler.** Every request pays the full cost.

**Local-disk persistence behind multiple replicas** or on ephemeral filesystems.

**Edge runtime by default**, then discovering a reader or store doesn't run there.

**Buffered streams**: works locally, arrives all at once in production because of a proxy.

**No error event or disconnect handling in streams.**

**One shared chat engine for all users.**

**Ingesting from the web service**, blocking requests and hitting timeouts.

**API keys reachable from the browser.**

**Unpinned dependencies** in a fast-moving ecosystem.

## Quick Summary

- Separate ingestion (job) from serving (stateless, read-only against a shared vector store).
- Build indexes, engines and agents once; create per-user pieces (filters, chat history) per request.
- Stream with SSE or `ReadableStream`; send error events, stop on disconnect, and make sure proxies don't buffer.
- Next.js: wrap config with `withLlamaIndex`, run on the Node runtime, cache the agent, never call models from the client.
- Edge and serverless need extra care: Node-only classes, no local disk, cold starts, env handling, connection pools, strict timeouts.
- `@llamaindex/server` is a fast path for hosting workflows and agents; hand-written routes give you more control.
- Version indexes for safe re-indexing and rollback, pin dependencies, and monitor tokens, latency and errors.

## Next

`../reference/interview.md` and `../reference/cheatsheet.md`: condensing everything into revision material.
