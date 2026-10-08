# Reliability and Cost

Reliability and cost are the same topic viewed from two sides. Both are driven by **how many model calls a request makes, how big they are, and what happens when one fails**. Retries make a flaky request slow and expensive. An agent loop makes a simple question cost ten calls. A re-index with the wrong settings costs a day of embeddings.

> Prerequisites: [../01-core/03-indexes-and-storage](../01-core/03-indexes-and-storage.md), [../03-tools-and-agents/02-agents](../03-tools-and-agents/02-agents.md).

## Where the money and latency go

```text
INDEXING (one-off, then incremental)
  documents ─► chunks ─► embedding calls (one per chunk, batched)
                └─► LLM extractors (title/summary/questions): per-chunk LLM calls  ← often the surprise

PER QUERY
  embed the query ─► vector search ─► [reranker call] ─► LLM synthesis (tokens ≈ topK × chunk size + prompt)
                                                   └─► agent: repeat the LLM call per step, context growing each time
```

Rough cost model for a query-engine request:

```text
input tokens  ≈ system prompt + question + (similarityTopK × chunk tokens) [+ history]
output tokens ≈ answer length
```

So the biggest levers, in order: **how much context you send** (top-k and chunk size), **how many LLM calls per request** (agent steps, `refine`, query transforms, sub-questions), and **which model** handles each call.

## Cost controls

**Send less context.** Retrieve wide, rerank, keep a few: `similarityTopK: 20` → reranker → top 4 sends far fewer tokens than `similarityTopK: 20` straight to the LLM. See [../02-rag/03-retrievers-and-postprocessors](../02-rag/03-retrievers-and-postprocessors.md).

**Choose the synthesis mode deliberately.** `compact` packs chunks into few prompts. `refine` makes one call per chunk. Don't use `refine` as the default ([../01-core/04-query-and-chat-engines](../01-core/04-query-and-chat-engines.md)).

**Cap output.** Provider LLM classes expose generation parameters such as `maxTokens` and `temperature`. Set a sensible `maxTokens` so a runaway answer can't burn the budget.

**Use the right model for each job.** Routing, classification, query rewriting and metadata extraction rarely need your most expensive model. Keep the strong model for the final answer. Since models are plain objects you pass around, you can give each component its own:

```ts
const cheap = new OpenAI({ model: "gpt-4o-mini", temperature: 0, maxTokens: 300 });
const strong = new OpenAI({ model: "gpt-4o", temperature: 0.2, maxTokens: 800 });
// router/selector and query rewriting use `cheap`; final synthesis uses `strong`
```

(Model names are examples; use what you have access to.)

**Don't recompute.**

- Persist the index; never rebuild on startup ([../01-core/03-indexes-and-storage](../01-core/03-indexes-and-storage.md)).
- Use an ingestion pipeline with a docstore and cache so unchanged documents and transformation steps are skipped ([../02-rag/02-ingestion-pipelines](../02-rag/02-ingestion-pipelines.md)).
- Cache LlamaParse results by file hash ([../02-rag/01-readers-and-llamaparse](../02-rag/01-readers-and-llamaparse.md)).
- Cache answers for repeated questions (below).

**Batch embeddings.** Embedding classes have an `embedBatchSize` property; tuning it trades fewer requests against provider limits.

**Be careful with per-chunk LLM extractors.** `TitleExtractor`, `QuestionsAnsweredExtractor` and friends add LLM calls for every chunk. Measure whether they improve retrieval before enabling them on a large corpus.

**Cap agents.** Limit steps, set a request-level timeout, and keep tool outputs short. Check for a max-iterations option in your agent version; if there isn't one, build the loop as a workflow with a counter ([../03-tools-and-agents/03-workflows](../03-tools-and-agents/03-workflows.md)).

**Measure per request.** Record prompt and completion tokens (taken from the provider response or callback events) plus model name, and compute cost per route and per user. You can't optimize what you don't log ([01-tracing-and-debugging](./01-tracing-and-debugging.md)).

## Timeouts and retries

The provider classes expose `maxRetries` and `timeout` (milliseconds). For the OpenAI embedding class the docs state the defaults: **10 retries and a 60 second timeout**. Check the same properties on your LLM class. Those defaults are generous for a background job and terrible for an interactive request: a failing call can quietly spend minutes retrying.

```ts
Settings.llm = new OpenAI({
  model: "gpt-4o-mini",
  maxRetries: 2,        // fail fast for interactive traffic
  timeout: 20_000,      // ms
});

Settings.embedModel = new OpenAIEmbedding({
  model: "text-embedding-3-small",
  maxRetries: 3,
  timeout: 15_000,
});
```

Guidelines:

- **Interactive path:** low retries (1 to 2), timeout around what a user will wait. Fail fast and show a useful message.
- **Batch ingestion:** higher retries with backoff, low concurrency, and resumable progress.
- **Retries multiply latency and cost.** Three retries on a 20 s timeout can be a minute of waiting. Retried calls that did reach the provider may still be billed.
- Retry only on retryable failures (timeouts, 429, 5xx). Don't retry on 400/401, which will fail identically.

### A request-level deadline

Per-call timeouts don't bound a multi-step request. Add an overall deadline:

```ts
async function withDeadline<T>(work: Promise<T>, ms: number): Promise<T> {
  let timer: NodeJS.Timeout;
  const deadline = new Promise<never>((_, reject) => {
    timer = setTimeout(() => reject(new Error("deadline exceeded")), ms);
  });
  try {
    return await Promise.race([work, deadline]);
  } finally {
    clearTimeout(timer!);
  }
}

const res = await withDeadline(queryEngine.query({ query }), 25_000);
```

`Promise.race` stops *waiting*, not the underlying work: the abandoned call may keep running (and being billed). Where an API accepts an `AbortSignal`, pass one so the work really stops; workflow contexts expose a signal for this ([../03-tools-and-agents/03-workflows](../03-tools-and-agents/03-workflows.md)).

## Rate limits and concurrency

Ingestion is where you hit provider rate limits: thousands of chunks, many embedding calls. Control concurrency explicitly rather than firing `Promise.all` over everything:

```ts
async function mapLimit<T, R>(items: T[], limit: number, fn: (x: T) => Promise<R>): Promise<R[]> {
  const out: R[] = new Array(items.length);
  let next = 0;
  const workers = Array.from({ length: limit }, async () => {
    while (next < items.length) {
      const i = next++;
      out[i] = await fn(items[i]);
    }
  });
  await Promise.all(workers);
  return out;
}

// e.g. ingest files 4 at a time
await mapLimit(files, 4, (f) => ingestOne(f));
```

Make ingestion **idempotent and resumable**: stable document IDs plus a docstore means a crashed run can simply be re-run and will skip finished work.

## Failure modes to handle on purpose

| Failure | Behavior to design |
|---|---|
| LLM provider down or rate limited | Fail fast with a clear message; optionally fall back to a second model or provider |
| Embedding provider down at query time | Can't search; return an error. Don't silently skip retrieval and let the LLM guess |
| Vector store slow or down | Timeout, error response, alert; no unbounded waiting |
| **Empty or weak retrieval** | Explicit "I couldn't find this" path; instruct the model to say so rather than improvise |
| Reranker/tool call fails | Degrade gracefully (skip the reranker, log it) if quality loss is acceptable |
| Agent loops or stalls | Step cap plus deadline; return partial result or error |
| Stream breaks midway | Send an error event to the client; don't leave it hanging |

The dangerous failures are **silent** ones: a skipped retrieval that still produces a fluent, ungrounded answer. Prefer failing visibly.

A simple model fallback:

```ts
async function answerWithFallback(question: string) {
  try {
    return await withDeadline(primaryEngine.query({ query: question }), 20_000);
  } catch (err) {
    log.warn({ err }, "primary failed, using fallback");
    return await withDeadline(fallbackEngine.query({ query: question }), 20_000);
  }
}
```

Fallbacks need their own evaluation: a smaller model may behave differently, and a different embedding model can't search an index built with another one. Fall back on the *LLM*, not the embedder, unless you maintain parallel indexes.

## Caching answers

Many questions repeat. An exact-match cache on a normalized question, keyed with the index version, avoids all model calls for repeats:

```ts
const cache = new Map<string, { text: string; at: number }>();
const TTL = 10 * 60_000;

function key(q: string, indexVersion: string) {
  return `${indexVersion}:${q.trim().toLowerCase().replace(/\s+/g, " ")}`;
}

async function cachedAnswer(q: string) {
  const k = key(q, INDEX_VERSION);
  const hit = cache.get(k);
  if (hit && Date.now() - hit.at < TTL) return hit.text;

  const res = (await queryEngine.query({ query: q })).toString();
  cache.set(k, { text: res, at: Date.now() });
  return res;
}
```

Production versions use a shared store (Redis) and a size limit. Caution: **never cache across users when answers depend on access rights**. Include the tenant or permission scope in the key, or don't cache. Invalidate (new `INDEX_VERSION`) whenever the data changes. Semantic (fuzzy) caches exist but can return a wrong answer for a near-duplicate question, so measure before using one.

## Latency

Typical breakdown of a RAG request: embed query (tens of ms), vector search (tens of ms), optional reranker (hundreds of ms), LLM time-to-first-token plus generation (seconds). So:

- **Stream the response.** Perceived latency is time to first token. See [05-serving-and-integration](./05-serving-and-integration.md).
- **Parallelize** independent work (multiple retrievers, multi-query) with `Promise.all`.
- **Cut sequential LLM calls.** Routers, query rewriting, sub-questions and agents each add a full round trip before the answer starts.
- Keep **prompts and context small**; generation speed and time to first token both depend on input size.
- Warm up at startup (load the persisted index, open DB pools) so the first user request isn't a cold start.

## A budget you can enforce

Decide up front and encode it:

- Max tokens in, max tokens out, max LLM calls per request.
- Per-user and per-tenant rate and daily spend limits (see [04-security](./04-security.md): cost abuse is an attack).
- Alert on cost per request drifting upward (a prompt or retrieval change that doubled the context is invisible without it).

## Common mistakes

**Leaving default retries and timeouts** on interactive traffic.

**Rebuilding the index at startup** or on every deploy.

**`Promise.all` over thousands of embedding calls**, then fighting rate limits.

**Silent fallback to "just ask the LLM"** when retrieval fails.

**Unbounded agent loops** and giant tool outputs.

**Caching without scoping by user/permissions**, or without invalidating on data updates.

**Optimizing before measuring.** Log tokens and calls per request first, then cut the largest line item.

## Quick Summary

- Cost and latency = number of model calls × size of each call. Retrieve wide then narrow, pick `compact` over `refine`, cap `maxTokens`, and use cheaper models for routing and rewriting.
- Persist the index; use ingestion docstore and cache; cache parse results and repeated answers (scoped by tenant, invalidated by index version).
- Set `maxRetries` and `timeout` explicitly (embedding defaults are 10 retries and 60 s); add a request-level deadline; remember `Promise.race` doesn't cancel work.
- Control ingestion concurrency, and make ingestion idempotent and resumable.
- Fail visibly: no silent retrieval skips; explicit "not found" behavior; step caps for agents.
- Stream, parallelize, and log tokens, calls and cost per request.

## Next

[04-security.md](./04-security.md): the failure modes where the cost isn't money.
