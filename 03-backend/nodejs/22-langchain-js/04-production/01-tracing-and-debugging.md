# Tracing and Debugging

LLM apps fail in ways a stack trace can't show: the wrong tool got chosen, retrieval returned irrelevant chunks, a prompt rendered differently than you thought. **Tracing** records every step of a run (inputs, outputs, timing, token usage) so you can see what actually happened. In the LangChain ecosystem that tool is LangSmith.

> Checked against the LangSmith tracing docs for JS/TS. Region endpoints and plan limits change, so confirm them in your account.

**Prerequisites:** [Runnables and LCEL](../01-core/04-runnables-and-lcel.md), [Agents](../03-tools-and-agents/02-agents.md)

---

## Turn it on

No code changes for LangChain components. Set environment variables:

```bash
LANGSMITH_TRACING=true
LANGSMITH_API_KEY=lsv2_...
LANGSMITH_PROJECT=my-app-dev      # groups runs; use one per app/environment
# LANGSMITH_ENDPOINT=https://eu.api.smith.langchain.com   # only if your account is not in the default region
```

Every model call, prompt, retriever, tool, chain and agent step now appears as a nested **run** in the LangSmith UI. Use separate projects for dev, staging and prod so noise doesn't bury real traffic.

---

## What a trace shows

```text
qa-chain                       2.4s   1,230 tokens
 ├─ prompt (ChatPromptTemplate)        rendered messages
 ├─ retriever                          query + returned docs
 ├─ ChatOpenAI                         full request, response, usage_metadata
 └─ StringOutputParser                 final string
```

For an agent you see each loop iteration: model call, the tool calls it requested, each tool's input and output, and the next model call. Most agent bugs are visible in that sequence.

---

## Make traces searchable

Name and label runs so you can find them later. Config is inherited by every step in a chain:

```ts
await chain.invoke(input, {
  runName: "support-answer",
  tags: ["prod", "v2-prompt"],
  metadata: { userId: "u_123", tenant: "acme" },
});
```

You can also bake defaults into a runnable:

```ts
const chain = prompt.pipe(model).pipe(parser).withConfig({
  tags: ["config-tag"],
  metadata: { feature: "support" },
});
```

Useful metadata: user or tenant id (a hashed id if privacy matters), prompt version, experiment name, request id. Include your own request id so you can jump from an app log to the trace.

---

## Tracing your own code

Code that isn't a LangChain component (a database lookup, custom business logic) won't show up unless you wrap it. `traceable` from the `langsmith` package makes it appear as a run, nested under the current trace:

```ts
import { traceable } from "langsmith/traceable";

const loadCustomer = traceable(
  async (id: string) => db.customers.find(id),
  { name: "load_customer" },
);
```

---

## Trace only what you want

If you don't want global tracing, pass a tracer for specific invocations:

```ts
import { LangChainTracer } from "@langchain/core/tracers/tracer_langchain";

await chain.invoke(input, { callbacks: [new LangChainTracer()] });
```

---

## Serverless: flush before the function exits

Traces are sent in the background. In serverless environments the process can freeze before they are sent, so traces go missing. Wait for pending callbacks and disable background mode:

```ts
import { awaitAllCallbacks } from "@langchain/core/callbacks/promises";

try {
  return await handler(req);
} finally {
  await awaitAllCallbacks();
}
```

```bash
LANGCHAIN_CALLBACKS_BACKGROUND=false
```

Missing traces in production but not locally is almost always this.

---

## Privacy: traces contain your data

A trace holds the full prompts and outputs, including user messages and retrieved documents. Treat the tracing project like a database of user content:

- Restrict who can open it.
- Keep secrets and card numbers out of prompts in the first place.
- Redact or mask PII before it reaches the model (see [Security](./04-security.md)), which also keeps it out of traces.
- Check your retention settings and any compliance requirements (region, data residency).

---

## A debugging workflow

When an answer is wrong, walk the trace from the top and stop at the first step that is wrong:

1. **Input:** what did the app actually send? (Often a bug in your code, not the model.)
2. **Prompt:** is the rendered prompt what you intended? Missing variables, unescaped braces, huge context?
3. **Retrieval:** are the right chunks present? ([Retrieval and RAG](../02-rag/03-retrieval-and-rag.md))
4. **Model call:** did it receive the tools and schema you expect? Any errors or truncation (`maxTokens`)?
5. **Tools:** correct tool, correct arguments, sensible result, or an error the model ignored?
6. **Output handling:** parser or validation failures after the model call.

To reproduce, copy the failing run's inputs and replay them in a script, then change one thing at a time. Save good failing examples; they become test cases ([Testing and Evaluation](./02-testing-and-evaluation.md)).

### Without LangSmith

You can still debug locally: log `result.messages` for agents, call `await prompt.invoke(vars)` and print the result, call `retriever.invoke(q)` alone, call tools directly with the arguments the model chose. Tracing just saves you from doing this by hand every time.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Traces missing in serverless | `awaitAllCallbacks()` in `finally`, and `LANGCHAIN_CALLBACKS_BACKGROUND=false` |
| All environments in one project | Separate projects per environment |
| Unnamed runs everywhere | Set `runName`, `tags`, `metadata` |
| Custom code invisible in traces | Wrap with `traceable` |
| Wrong region endpoint, no traces appear | Set `LANGSMITH_ENDPOINT` for your account's region |
| Sensitive data flowing into traces | Redact before the model, limit access, check retention |
| Debugging only from the final answer | Open the trace and find the first wrong step |
| Tracing left on for load tests | Sample or disable to avoid cost and noise |

---

## Quick Summary

- Set `LANGSMITH_TRACING`, `LANGSMITH_API_KEY`, `LANGSMITH_PROJECT`; LangChain steps are traced automatically.
- Add `runName`, `tags`, `metadata` through config; wrap your own functions with `traceable`.
- In serverless, flush with `awaitAllCallbacks()` or traces get lost.
- Traces contain user data; protect them like a database.
- Debug top-down: input, prompt, retrieval, model call, tools, output handling. Save failures as test cases.

**Next:** [Testing and Evaluation](./02-testing-and-evaluation.md)
