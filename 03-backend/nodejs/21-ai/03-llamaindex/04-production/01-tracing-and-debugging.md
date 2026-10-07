# Tracing and Debugging

A RAG or agent app fails in ways ordinary code doesn't: the code runs fine, no exception is thrown, and the answer is simply wrong. Debugging means answering one question quickly: **which stage produced the bad output?** Was the right chunk never retrieved, retrieved but ranked low, retrieved and then ignored by the LLM, or was the prompt itself wrong?

You need visibility at four levels, from cheap to heavy: inspect retrieval directly, turn on debug logging, hook the callback events, and ship traces to a backend.

> Prerequisites: [../01-core/04-query-and-chat-engines](../01-core/04-query-and-chat-engines.md), [../03-tools-and-agents/03-workflows](../03-tools-and-agents/03-workflows.md).

## Level 0: look at retrieval first

Most "bad answer" bugs are retrieval bugs. Before touching prompts or models, reproduce the failure with the retriever alone:

```ts
const retriever = index.asRetriever({ similarityTopK: 10 });
const hits = await retriever.retrieve({ query: failingQuestion });

hits.forEach((h, rank) => {
  console.log(rank, h.score?.toFixed(3), h.node.id_, h.node.metadata.source);
  console.log("   ", h.node.getContent(MetadataMode.NONE).slice(0, 160).replace(/\n/g, " "));
});
```

Then read it like a detective:

| What you see | Meaning | Where to look |
|---|---|---|
| Right chunk absent | Retrieval miss | Chunking, embedding model, filters, was it ingested? |
| Right chunk present, rank 8 of 10, top-k was 3 | Ranking problem | Bigger `k` + reranker, hybrid search |
| Right chunk at rank 1, answer still wrong | Generation problem | Prompt, synthesis mode, model, context order/size |
| Chunk is a fragment of the answer | Chunking problem | Chunk size, window/parent patterns |

Also print what the embedder and LLM actually see: `node.getContent(MetadataMode.EMBED)` and `MetadataMode.LLM`. Metadata leaking into embeddings is a classic cause of odd matches.

## Level 1: debug logging

The library has built-in debug output. Enable it with the `DEBUG=llamaindex` environment variable (or `Settings.debug = true`):

```bash
DEBUG=llamaindex npx tsx main.ts
```

It prints what is sent to the LLM, which is often enough to spot a wrong prompt, an empty context, or a missing system instruction. Never leave it on in production: it logs user content.

## Level 2: callback events

`Settings.callbackManager` emits events you can subscribe to. The event map includes, among others, `retrieve-start`, `retrieve-end`, `synthesize-end`, `query-end`, `chunking-end`, `llm-end`, `llm-tool-call` and `llm-tool-result` (with matching start/stream events for some).

```ts
import { Settings } from "llamaindex";

Settings.callbackManager.on("retrieve-end", (event) => {
  console.log("[retrieve-end]", event.detail);
});

Settings.callbackManager.on("llm-end", (event) => {
  console.log("[llm-end]", event.detail);
});

Settings.callbackManager.on("llm-tool-call", (event) => {
  console.log("[tool-call]", event.detail);
});
```

The payload is carried in `event.detail`. Its exact shape has changed between versions, so log the whole object once and look, then rely on your installed type definitions.

Use callbacks for lightweight, in-process needs: timing each stage, counting tokens, collecting retrieved IDs for an evaluation log. For anything cross-service, use real tracing.

## Level 3: tracing to a backend

Traces show one request as a tree of spans (retrieval, each LLM call, each tool call) with timings and inputs/outputs. That is the right tool for production incidents: "why was *this* user's request slow and wrong at 14:02?"

The documented route for LlamaIndex.TS goes through **OpenLLMetry (Traceloop)**, which is built on OpenTelemetry and emits standard OTLP, so you can send spans to any OTLP-compatible backend (Jaeger, Grafana Tempo, Logfire, Langfuse via OTel, Datadog, and so on).

```bash
npm i @traceloop/node-server-sdk
```

```ts
// instrumentation.ts  — must run BEFORE llamaindex is imported anywhere
import * as traceloop from "@traceloop/node-server-sdk";

traceloop.initialize({
  appName: "support-bot",
  disableBatch: false,
  // exporter endpoint / headers: configure per your backend, usually via env vars
});

const { agent } = await import("@llamaindex/workflow");   // import AFTER initialize
const { openai } = await import("@llamaindex/openai");
// ...build and run your agent or query engine...

// in short-lived processes (scripts, serverless), flush before exit
await traceloop.forceFlush();
```

Rules that bite:

- **Import order is the #1 problem.** Instrumentation patches modules as they load. If `llamaindex` or the provider packages are imported first, you get empty traces. Initialize first, then dynamically import the rest.
- **Flush in short-lived runtimes.** Serverless functions and scripts can exit before the batch exporter sends spans. Call `forceFlush()` before returning.
- Check Traceloop's docs for the exporter options of your backend; I'm deliberately not listing option names that may differ by version.

**Workflows** have their own tracing hook: wrapping a workflow with the trace-events middleware emits per-handler data that can be exported through OpenTelemetry. See [../03-tools-and-agents/03-workflows](../03-tools-and-agents/03-workflows.md).

Hosted LLM-observability products (Langfuse, Arize Phoenix/LlamaTrace, and others) mostly document Python first. For TypeScript, the OTLP route above is the dependable common denominator; confirm the TS story on the vendor's page before committing.

## What to record for every request

A trace is only as useful as the fields you attach. Minimum viable record:

```ts
type RagTrace = {
  requestId: string;
  userIdHash: string;          // not the raw ID
  query: string;               // consider redaction
  retrieved: { id: string; score?: number; source?: string }[];
  model: string;
  promptTokens?: number;
  completionTokens?: number;
  latencyMs: { retrieve: number; llm: number; total: number };
  toolCalls?: { name: string; ms: number; ok: boolean }[];
  indexVersion: string;        // which build of the index answered
  embeddingModel: string;
  feedback?: "up" | "down";
};
```

`indexVersion` and `embeddingModel` save you hours: half of "it got worse after Tuesday" is a re-index or a model change.

Log retrieved **IDs and scores**, not full chunk text, unless your policy allows storing it. Chunk text may contain personal or confidential data.

## Debugging playbook

1. **Reproduce with the retriever** (Level 0). Is the right node in the top k?
2. **Not retrieved:** confirm it exists (count nodes, search the store directly), then check filters, chunk boundaries, and that the query and index use the same embedding model.
3. **Retrieved but low rank:** try more candidates plus a reranker; try hybrid search if the query has identifiers.
4. **Retrieved, answer wrong:** print the final prompt (debug logging), check context size and order, try `refine` or a stronger model, check the system prompt.
5. **Agent misbehaves:** stream events and read the tool-call sequence; fix tool descriptions before changing models ([../03-tools-and-agents/02-agents](../03-tools-and-agents/02-agents.md)).
6. **Intermittent:** compare traces of a good and a bad run of the same question: different retrieved IDs? different tool path? timeouts retried?
7. **Turn the incident into a test:** add the question and expected source to your evaluation set ([02-testing-and-evaluation](./02-testing-and-evaluation.md)).

## Common mistakes

**Tracing initialized after imports**, leading to silent empty traces.

**Not flushing** in serverless.

**Logging raw prompts and chunks in production** with no redaction or retention policy.

**Debugging the LLM first.** Check retrieval before prompts, always.

**No index or model version on traces**, making regressions impossible to attribute.

**Only tracing happy paths.** Record timeouts, retries and empty-retrieval cases too.

## Quick Summary

- Debugging order: retrieval, then synthesis, then model. Reproduce with `retriever.retrieve` first.
- `DEBUG=llamaindex` shows what is sent to the LLM; `Settings.callbackManager.on(...)` gives in-process stage events (payload in `event.detail`).
- For production tracing use OpenLLMetry/Traceloop over OTLP; initialize it before importing LlamaIndex and flush in short-lived runtimes.
- Record IDs, scores, model, index version, token counts and latency per request; redact content.
- Convert every real failure into a regression test.

## Next

[02-testing-and-evaluation.md](./02-testing-and-evaluation.md): measuring retrieval and answer quality so changes are decisions, not guesses.
