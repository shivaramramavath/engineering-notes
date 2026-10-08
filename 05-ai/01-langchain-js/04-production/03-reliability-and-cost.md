# Reliability and Cost

In production, model calls fail, slow down, hit rate limits, and cost real money in proportion to tokens. Reliability and cost are the same discipline: **bound everything** (time, retries, loops, context size) and **measure everything** (tokens, latency, failures).

> Checked against the LangChain JS 1.x docs where noted. Rate-limiter details were not verified against the JS docs; see that section.

**Prerequisites:** [Models and Messages](../01-core/02-models-and-messages.md), [Runnables and LCEL](../01-core/04-runnables-and-lcel.md), [Agents](../03-tools-and-agents/02-agents.md)

---

## Failure modes to plan for

| Failure | Typical cause | Main defense |
|---|---|---|
| Transient errors (network, 5xx) | Provider or network blip | Retries with backoff |
| Rate limits (429) | Too many requests or tokens per minute | Backoff, concurrency limits, client-side rate limiting |
| Slow or hung calls | Provider latency, huge prompts | Timeouts, streaming, smaller prompts |
| Provider outage | Provider-wide incident | Fallback model or provider |
| Runaway agent loop | Model keeps calling tools | Call limits |
| Invalid output | Model returns the wrong shape | Structured output, validation, retry with feedback |
| Context overflow | Unbounded history or retrieval | Trim, summarize, cap `k` |
| Cost spikes | Long contexts, loops, retries | Limits, caching, model routing, budgets |

---

## Retries

Chat models **retry automatically** on network errors, rate limits (429) and server errors (5xx), with exponential backoff and jitter. The default is 6 attempts. Client errors such as 401 and 404 are not retried because retrying can't fix them.

```ts
const model = await initChatModel("gpt-5-nano", {
  maxRetries: 10,      // raise for flaky networks (docs suggest 10 to 15 for long-running agents)
  timeout: 120_000,    // check the unit your provider class expects
});
```

Notes:

- Don't stack your own retry loop on top for the same errors; you will multiply attempts.
- When you use `modelRetryMiddleware`, it takes over retries for the calls it wraps, and the model's own `maxRetries` doesn't apply to them (per the docs, on providers that support it).
- For **logic-level failures** (output didn't parse, validation failed), retry with feedback or use `withRetry` on the step that can fail. Structured output via tool strategy already retries with error feedback inside agents.
- Retries of non-idempotent **tools** can duplicate side effects. Make write tools idempotent (idempotency keys) before enabling tool retries.

```ts
import { createAgent, modelRetryMiddleware, toolRetryMiddleware } from "langchain";

const agent = createAgent({
  model: "gpt-5-nano",
  tools,
  middleware: [
    modelRetryMiddleware({ maxRetries: 3, backoffFactor: 2.0 }),
    toolRetryMiddleware({ maxRetries: 3, initialDelayMs: 1000 }),
  ],
});
```

---

## Timeouts

Every network call needs a time bound, or one stuck request ties up a worker forever. Set `timeout` on the model, and an overall deadline on the request in your server code (and abort the work when the client disconnects; see [Serving and Integration](./05-serving-and-integration.md)).

Also set `maxTokens` so a runaway generation can't produce unbounded output (and cost).

---

## Fallbacks

If a provider or model is down or overloaded, fall back to another:

```ts
const resilient = primaryChain.withFallbacks({ fallbacks: [backupChain] });
```

Considerations:

- The fallback may behave differently (tone, tool-calling quirks). Include it in your evaluation dataset.
- Fallback to a **cheaper, weaker** model is a degradation strategy. Decide on purpose whether that is acceptable.
- Don't fall back on errors that retrying elsewhere can't fix, such as an invalid request or a content-policy rejection.

---

## Bounding agents

An unbounded agent loop is the biggest cost and reliability risk. Use the built-in middleware:

```ts
import { modelCallLimitMiddleware, toolCallLimitMiddleware } from "langchain";

middleware: [
  modelCallLimitMiddleware({ threadLimit: 10, runLimit: 5 }),
  toolCallLimitMiddleware({ toolName: "search", threadLimit: 5 }),
]
```

Option names are from the docs' examples. Pick limits from observed traces (e.g. the 99th percentile of calls for legitimate tasks, with some headroom), and decide what the user sees when a limit is hit.

For long-running agents on unreliable networks, add a **checkpointer** so progress survives failures and the run can resume.

---

## Rate limits and concurrency

- `batch` and parallel chains can burst past your provider's limits. Set `maxConcurrency` in the config:

```ts
await chain.batch(inputs, { maxConcurrency: 5 });
```

- In a multi-instance service, per-process limits don't add up to a global one. Put queueing or a shared limiter in front for hard limits.
- LangChain core includes an in-memory rate limiter abstraction that can be attached to chat models. I did not verify its JS API in the current docs; check the reference before relying on it.
- Watch both **requests per minute** and **tokens per minute**; long prompts hit the second limit first.

---

## Cost: where the money goes

Cost is roughly `input tokens + output tokens` summed over every model call. Biggest levers:

| Lever | How |
|---|---|
| **Shrink input** | Trim or summarize history ([Chat History](../01-core/07-chat-history.md)); retrieve fewer, better chunks; keep tool outputs compact |
| **Cap output** | `maxTokens`; ask for concise formats; structured output instead of prose |
| **Right-size the model** | Use a small model for easy steps (classification, routing, summarization) and a larger one only where needed |
| **Prompt caching** | Reuse a long, stable prefix (system prompt, tool definitions, large documents) |
| **Fewer calls** | Prefer a chain over an agent when the path is known; limit loops; batch independent work |
| **Application-level caching** | Cache answers to identical requests where staleness is acceptable |

### Measure first

Every `AIMessage` carries `usage_metadata` (`input_tokens`, `output_tokens`, `total_tokens`, plus cache-read and reasoning-token details when available):

```ts
const res = await model.invoke(messages);
log.info({ usage: res.usage_metadata, requestId });
```

Log it per request, tagged with feature and user or tenant, and chart it. Tracing also records token usage per run. Optimize the biggest line item, not the one you guess.

### Prompt caching

Providers offer caching at three levels, per the docs:

- **Implicit:** the provider caches automatically and passes savings on (no configuration), e.g. OpenAI and Gemini.
- **Explicit provider controls:** you mark cache points yourself, e.g. OpenAI `prompt_cache_key`, Anthropic `cache_control` on content blocks.
- Caching usually only engages above a minimum prompt size, and cache hits show up in `usage_metadata`.

To benefit, put **stable content first** (system prompt, tool definitions, reference documents) and variable content (the user's message) last, and keep the stable prefix byte-identical between calls.

### Dynamic model routing

Choose the model per request using middleware:

```ts
const route = createMiddleware({
  name: "Route",
  wrapModelCall: (request, handler) =>
    handler({ ...request, model: request.messages.length > 10 ? largeModel : smallModel }),
});
```

Whatever rule you use, evaluate it: a cheaper model that fails more often can cost more after retries and escalations.

### Budgets and alerts

- Per-request limits (`maxTokens`, call limits).
- Per-user or per-tenant quotas enforced in your server code.
- Alerts on spend and token anomalies from your provider dashboard and your own metrics.

---

## Latency

- **Stream** responses so users see output immediately ([Streaming](../01-core/06-streaming.md)).
- Run independent steps in parallel (`RunnableParallel`, parallel tool calls).
- Smaller prompts and smaller models are faster.
- Avoid unnecessary agent loops.
- Keep retries bounded; six attempts with backoff can add a lot of wall-clock time to a failing request. Decide whether a user would rather get a fast error.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Unbounded agent loops | Model and tool call limits |
| No timeout or `maxTokens` | Set both |
| Stacked retries multiplying delays and cost | Retry at one layer |
| Retrying non-idempotent tools | Make writes idempotent first |
| `batch` with no `maxConcurrency` | Cap it to stay under rate limits |
| Never logging token usage | Log `usage_metadata` with feature and tenant tags |
| Variable text at the start of the prompt, killing cache hits | Stable prefix first, variable content last |
| Fallback model never evaluated | Include it in your evaluation runs |
| Sending full history and all retrieved chunks forever | Trim, summarize, tune `k` |
| Using the biggest model everywhere | Route by difficulty, then measure |

---

## Quick Summary

- Models retry network, 429 and 5xx errors by default (6 attempts); don't stack extra retry layers, and keep retried tools idempotent.
- Bound time (`timeout`), output (`maxTokens`) and agent loops (call-limit middleware).
- Use `withFallbacks` for outages, and evaluate the fallback.
- Control rate-limit pressure with `maxConcurrency` and queueing.
- Cost levers: smaller inputs, capped outputs, right-sized models, prompt caching (stable prefix first), fewer calls.
- Measure with `usage_metadata` and traces before optimizing.

**Next:** [Security](./04-security.md)
