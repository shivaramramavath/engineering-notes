# Reliability

Agent runs are long, call flaky external services, and take decisions you can't fully predict. Things will fail: rate limits, timeouts, malformed model output, a deploy mid-run. Reliability here means three things: **recover** from transient failures, **never repeat** a side effect by accident, and **always stop** (no runaway loops or spend).

Prerequisites: [Persistence](../03-stateful/01-persistence.md) (checkpoints), [Nodes and Edges](../01-core/03-nodes-and-edges.md).

## Know your failure modes

| Failure | Typical cause | Main defense |
|---|---|---|
| Transient error | Rate limit, network blip, 5xx | Retries with backoff |
| Slow or hung call | Provider stall | Timeouts / abort signal |
| Bad model output | Invalid tool args, unparsable JSON | Validation + bounded repair |
| Runaway loop | Router never exits, agent stuck | Step and budget limits |
| Crash or deploy mid-run | Process killed | Durable checkpointer + resume |
| Duplicate side effect | Retry or resume re-runs a node | Idempotency |

## Retries

Put a `retryPolicy` on a node that makes flaky calls:

```ts
graph.addNode("callApi", callApi, {
  retryPolicy: {
    maxAttempts: 4,
    initialInterval: 500, // ms
    backoffFactor: 2,
    retryOn: (err) => isTransient(err), // your predicate
  },
});
```

- Use an explicit `retryOn` so you retry what's actually transient (429, 5xx, network errors) and not your own bugs or validation errors. Check the default for your version rather than relying on it.
- Retry the **narrowest** thing. If a node does a model call, a database write and an email, a retry re-runs all three. Split it into separate nodes so only the flaky step retries.
- **Don't stack retries.** Chat model clients have their own `maxRetries`. A node policy of 4 on top of a client's 2 is up to 8 attempts per call. Decide where retries live and set the other to a small number.

## Timeouts and cancellation

Bound how long a run can take by passing an abort signal; make sure your own network calls honor it too.

```ts
const out = await graph.invoke(input, {
  signal: AbortSignal.timeout(60_000),
});
```

Model calls made through LangChain respect the signal. In custom nodes, pass `config.signal` to `fetch` and other cancellable APIs.

## Stop runaway runs

- **Recursion limit.** Each superstep counts; exceeding the limit throws `GraphRecursionError`. Set it deliberately per call (`{ recursionLimit: 40 }`) and catch the error to return a graceful message.
- **Loop counters in state.** Bounded retries belong in state, like the `rewrites` counter in [Agentic RAG](../04-patterns/03-agentic-rag.md). The limit is a backstop, not the exit condition.
- **Budgets.** Track tokens or cost in state with a summing reducer and route to a wrap-up node when exceeded:

  ```ts
  spent: Annotation<number>({ reducer: (a, b) => a + b, default: () => 0 }),

  const afterStep = (s: typeof State.State) => (s.spent > MAX_BUDGET ? "wrapUp" : "agent");
  ```

- **Fan-out width.** `Send` over a large list creates that many concurrent tasks; chunk the list so you don't trip rate limits ([Routing and Parallelism](../01-core/04-routing-and-parallelism.md)).

## Idempotency: the rule that matters most

A node can run more than once: after a retry, after a crash-and-resume, or when a human-in-the-loop node restarts from its top. Reads are harmless. **Side effects must be safe to repeat.**

```ts
const refund = async (s: typeof State.State, config: LangGraphRunnableConfig) => {
  const key = `${config.configurable?.thread_id}:refund:${s.orderId}`;
  await payments.refund({ orderId: s.orderId, idempotencyKey: key });
  return { status: "refunded" };
};
```

Derive the key from stable identifiers (thread, order), never from a timestamp or random value generated inside the node. Most payment, email and queue APIs accept an idempotency key; where they don't, record "done" in your own database first and check it.

## Handling errors on purpose

Not every error should crash the run. Catch the ones you expect, record them in state, and route to a fallback:

```ts
const callApi = async (s: typeof State.State) => {
  try {
    return { data: await fetchData(s.id) };
  } catch (err) {
    return { error: err instanceof Error ? err.message : "unknown error" };
  }
};

const afterCall = (s: typeof State.State) => (s.error ? "fallback" : "process");
```

Related defenses:

- **Tool errors in agents.** `ToolNode` returns tool errors to the model as tool messages so it can adjust ([ReAct Agent](../02-agents/01-react-agent.md)). Good tools throw clear, actionable messages.
- **Validate model output.** Use structured output with a schema. If parsing fails, allow a bounded number of repair attempts (counter in state), then fail clearly.
- **Fallback models.** Chat models support `model.withFallbacks({ fallbacks: [backupModel] })`, which tries the backup when the primary errors.

## Recovering from crashes

With a durable checkpointer, state survives restarts. After a failure, resume instead of starting over:

```ts
async function runOrResume(threadId: string, input: GraphInput) {
  const cfg = { configurable: { thread_id: threadId } };
  const snap = await graph.getState(cfg);

  // Unfinished work → continue from the last checkpoint; otherwise start fresh.
  return graph.invoke(snap.next.length > 0 ? null : input, cfg);
}
```

Resuming re-runs only what hadn't completed. When parallel nodes were mid-flight, writes from the ones that finished are kept, so they aren't re-run. One caveat: a thread paused for human input also has a non-empty `next`. Distinguish "crashed" from "waiting for a person" (for example, by tracking job status yourself) before blindly resuming.

This needs a persistent saver such as Postgres. `MemorySaver` loses everything with the process ([Persistence](../03-stateful/01-persistence.md)).

## Observe what you can't predict

Reliability work is guesswork without visibility. At minimum:

- Log `thread_id`, the node names that ran, durations, and errors, with a request id you can search on.
- Turn on tracing for prompts, tool calls and token counts (LangSmith tracing is enabled via environment variables; check its docs for the current names).
- Alert on rising step counts per run, retry rates, recursion-limit hits and cost per run.

Traces contain prompts and tool outputs, which may include personal data ([Security](./03-security.md)).

## Common mistakes

- **Retrying a node that does several things**, repeating the side effects that already worked.
- **Non-idempotent side effects** in nodes that retry or resume.
- **Retrying non-transient errors** (bad input, auth failures), wasting calls and delaying the real error.
- **Relying on the recursion limit** as the loop exit.
- **Swallowing errors** in a `catch` without recording them, so failures vanish.
- **Resuming a thread that is waiting for a human**, or restarting a crashed one from scratch and redoing work.

## Debugging

- Stream `updates` and look at where a failed run stopped; `getState(cfg).next` names the node that didn't complete.
- Walk `getStateHistory(cfg)` to see the last good checkpoint ([Time Travel](../03-stateful/04-time-travel.md)).
- Count attempts: if a call fires more times than expected, check for stacked retry layers.
- Reproduce failures in tests by stubbing the dependency to throw ([Testing](./01-testing.md)).

## Quick summary

- Retry narrow, transient steps with backoff; don't stack retry layers.
- Bound everything: time (abort signal), steps (recursion limit plus state counters), spend (budget in state), fan-out width.
- Assume any node can run twice; give side effects stable idempotency keys.
- Handle expected errors in state and route to fallbacks; validate model output.
- Use a durable checkpointer and resume with `invoke(null, cfg)` after crashes, taking care not to confuse "crashed" with "waiting for a human".

**Next:** [Security](./03-security.md)
