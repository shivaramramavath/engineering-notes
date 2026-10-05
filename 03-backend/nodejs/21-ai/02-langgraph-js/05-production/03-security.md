# Security

An agent is a program whose control flow is partly decided by a model reading text that someone else may have written. That changes the threat model. The core rule is simple: **treat the model as an untrusted component that can be steered by any text it reads**, and put your security boundaries in code, outside the prompt.

Prerequisites: [ReAct Agent](../02-agents/01-react-agent.md) (tools), [Persistence](../03-stateful/01-persistence.md) (threads and checkpoints), [Human-in-the-Loop](../03-stateful/03-human-in-the-loop.md).

## The central risk: prompt injection

- **Direct injection**: a user types instructions meant to override yours.
- **Indirect injection**: instructions hide in content the agent *reads*: retrieved documents, web pages, emails, tool outputs, file contents. The user never typed them.

A system prompt saying "never reveal secrets" or "only do X" is a request, not a control. Anything the model can be talked into, an attacker can try to talk it into. So design on the assumption that **the model may be fully controlled by an attacker**, and ask of every tool:

> If an attacker chose exactly what this tool is called with, what is the worst that happens?

If the answer is unacceptable, the fix is in the tool, the permissions, or an approval step, not in a better prompt.

## Authorize in tools, using the caller's identity

The model must never decide *who the user is*. Take identity from the server-side config, not from tool arguments.

```ts
import { tool } from "@langchain/core/tools";
import { z } from "zod";

declare const db: {
  orders: { findMany(args: { where: { userId: string }; take: number }): Promise<unknown[]> };
};

const getMyOrders = tool(
  async ({ limit }, config) => {
    const userId = config.configurable?.userId as string | undefined;
    if (!userId) throw new Error("Not authenticated");
    return JSON.stringify(await db.orders.findMany({ where: { userId }, take: limit }));
  },
  {
    name: "get_my_orders",
    description: "List the current user's recent orders.",
    schema: z.object({ limit: z.number().int().min(1).max(20) }),
  },
);
```

Notice what's *not* a parameter: there's no `userId` argument for an injected prompt to change. The same goes for tenant ids, account ids and file paths.

## Build the config on the server

`config.configurable` is where `userId` and `thread_id` live. If your API forwards a client-supplied config into `graph.invoke`, a client can set `userId` to anyone. Construct it yourself from the authenticated session:

```ts
async function handleChat(user: { id: string }, conversationId: string, message: string) {
  return graph.invoke(
    { messages: [{ role: "user", content: message }] },
    {
      configurable: {
        thread_id: `${user.id}:${conversationId}`, // scoped to the user
        userId: user.id,
      },
      recursionLimit: 25,
    },
  );
}
```

## Isolate threads and memory between users

- **Threads.** A `thread_id` grants access to everything in that conversation's checkpoints. Scope it to the authenticated user as above, or use opaque random ids with a server-side ownership table, and check ownership before `invoke`, `getState`, `getStateHistory` or `updateState`.
- **Store namespaces.** Namespace long-term memory by user (`["memories", userId]`) and never search a broader prefix on behalf of a user ([Long-Term Memory](../03-stateful/02-long-term-memory.md)).
- **Resume endpoints.** Whoever can send `Command({ resume })` can approve an action. Authenticate and authorize that endpoint like the action itself, and validate the payload:

  ```ts
  const Decision = z.enum(["yes", "no"]);
  const decision = Decision.parse(interrupt({ question: "Send this draft?" }));
  ```

## Limit what tools can do

- **Prefer narrow tools to general ones.** `get_my_orders` beats `run_sql`. `fetch_status_page` beats `http_request`.
- **Least privilege credentials.** Give tools read-only database users and scoped API tokens, so a hijacked call can't do more than the credential allows.
- **Separate reading from acting.** Tools that read untrusted content shouldn't sit in the same agent as tools that send email, move money, or delete data. If they must, require approval before the dangerous step with `interrupt()`.
- **Allowlist outbound requests.** A URL-fetching tool can be pointed at internal services (SSRF). Allow specific hosts and block private address ranges.
- **Sandbox code and shell tools.** Never `eval` model output or run it in the app's own process. Use an isolated sandbox without secrets or broad network access.
- **Parameterize queries.** Never build SQL by string-concatenating model-supplied values.

Validate tool arguments with schemas and bounds (`.max(20)` above); schema validation is cheap and catches both mistakes and abuse.

## Secrets and sensitive data

State is persisted in checkpoints, streamed to clients, and visible in traces. So:

- **Never put secrets in state or prompts.** Load API keys from the environment or a secret manager inside the tool that needs them.
- **Treat the checkpoint database as sensitive**: it holds full conversations and tool outputs. Encrypt at rest, restrict access, and set a retention policy ([Persistence](../03-stateful/01-persistence.md)).
- **Mind your traces and logs.** Tracing captures prompts and tool results. Redact or disable for sensitive fields, and keep logs free of full message bodies.
- **Don't stream internals.** Convert chunks to the shape your UI needs rather than forwarding raw state or error details ([Deployment](./04-deployment.md)).

## Abuse and cost

- **Per-user rate limits** and request size limits at your API.
- **Step limits.** Set `recursionLimit` per call; track budgets in state ([Reliability](./02-reliability.md)).
- **Output limits** on tool results so one call can't flood the context.

A looping or maliciously prompted agent can burn money quickly ("denial of wallet").

## Retrieval and untrusted content

For RAG-style graphs ([Agentic RAG](../04-patterns/03-agentic-rag.md)), documents are untrusted input. Keep the answering step free of powerful tools, instruct the model to treat retrieved text as data, and enforce document permissions inside the search function using the caller's identity, not in the prompt.

## Misconceptions

- **"The system prompt is a security boundary."** It isn't; code is.
- **"State is private to the run."** It's persisted, streamed and traced.
- **"The model only calls tools I gave it, so tools are safe."** The model chooses the arguments, and untrusted text influences the model.
- **"Prompt injection is solved by a better prompt."** Mitigate by reducing blast radius.

## Review checklist

- Identity and tenant come from server-side config, never from tool args.
- Config is built on the server; `thread_id` is scoped to the user.
- Dangerous tools need approval or sit behind narrower permissions.
- Tools are narrow, validated, parameterized and run with least privilege.
- No secrets in state or prompts; checkpoints and traces are protected.
- Per-user limits, recursion limit and budgets are in place.
- Retrieved and tool-returned text is treated as untrusted.

## Quick summary

- Assume the model can be steered by anything it reads; enforce security in code.
- Authorize inside tools with identity from server-built config, not model-provided arguments.
- Isolate threads, store namespaces and resume endpoints per user.
- Narrow tools, least-privilege credentials, approval for risky actions, sandboxing for code.
- Keep secrets out of state, protect checkpoints and traces, and bound cost.

**Next:** [Deployment](./04-deployment.md)
