# Human-in-the-Loop

Some steps shouldn't run without a person: sending an email, issuing a refund, running a destructive command, or just asking a clarifying question. LangGraph lets a node **pause the run, hand a value to your application, and continue when you supply an answer**, even hours later and from a different process.

Prerequisites: [Persistence](./01-persistence.md) (pausing works by saving a checkpoint), [Routing and Parallelism](../01-core/04-routing-and-parallelism.md) (`Command`).

## How it works

`interrupt(value)` called inside a node stops the run at that point. The graph saves a checkpoint and returns control to the caller, along with `value`. Later you resume the same thread with `new Command({ resume: answer })`, and the original `interrupt(...)` call **returns `answer`**.

```
invoke(input, cfg)
   ▼
 node runs … interrupt({ question })
   │            └─► checkpoint saved, caller receives the question
   ▼  (time passes; process may exit)
invoke(new Command({ resume: "yes" }), cfg)
   ▼
 node restarts from its top; interrupt(...) now returns "yes" … node continues
```

Requirements: a **checkpointer** and a **`thread_id`**. Without them there's nowhere to save the paused state.

## An approval gate

```ts
import {
  StateGraph,
  Annotation,
  MemorySaver,
  Command,
  interrupt,
  START,
  END,
} from "@langchain/langgraph";

const State = Annotation.Root({
  topic: Annotation<string>(),
  draft: Annotation<string>(),
  status: Annotation<string>(),
});

const writeDraft = (s: typeof State.State) => ({
  draft: `Announcement about ${s.topic}`,
});

const approval = (s: typeof State.State) => {
  const decision = interrupt({
    question: "Send this draft?",
    draft: s.draft,
  }) as "yes" | "no";

  return new Command({
    update: { status: decision === "yes" ? "approved" : "rejected" },
    goto: decision === "yes" ? "send" : END,
  });
};

const send = () => ({ status: "sent" });

const graph = new StateGraph(State)
  .addNode("writeDraft", writeDraft)
  .addNode("approval", approval, { ends: ["send", END] })
  .addNode("send", send)
  .addEdge(START, "writeDraft")
  .addEdge("writeDraft", "approval")
  .addEdge("send", END)
  .compile({ checkpointer: new MemorySaver() });

const cfg = { configurable: { thread_id: "email-1" } };

// 1. Runs until the interrupt
const paused = await graph.invoke({ topic: "Q3 launch" }, cfg);
console.log(paused.__interrupt__);
// [{ value: { question: "Send this draft?", draft: "Announcement about Q3 launch" }, ... }]

// 2. Later: resume with the human's answer
const done = await graph.invoke(new Command({ resume: "yes" }), cfg);
// done.status → "sent"
```

Key points:

- The interrupt payload (`value`) is what your UI shows the reviewer. It must be JSON-serializable.
- The result of the first call carries the pending interrupt under `__interrupt__`. You can also see it with `getState(cfg)`: `snap.next` is non-empty (`["approval"]`) and the interrupt is listed under `snap.tasks`.
- The resume value can be any serializable value: a string, a boolean, or an object like `{ action: "edit", text: "..." }`.
- Routing off the answer with `Command` keeps the decision and its state update together.

## Resuming from another process

Everything needed is in the checkpoint. A web handler can pause on one request and a different request, possibly on a different server sharing the same database checkpointer, can resume with the same `thread_id`. With `MemorySaver` this only works inside one process.

## Rules that prevent bugs

**The node restarts from its beginning on resume.** `interrupt()` doesn't freeze the function mid-flight; the whole node runs again and this time `interrupt()` returns the answer. So any code *before* the interrupt runs twice. Keep it cheap and idempotent, and put side effects *after* the interrupt (or in a later node).

```ts
// ❌ sends an email on the first pass and again on resume
const bad = async (s: typeof State.State) => {
  await sendEmail(s.draft);
  const ok = interrupt("Was that OK?");
  /* ... */
};

// ✅ ask first, act after approval
const good = (s: typeof State.State) => {
  const ok = interrupt({ question: "Send it?", draft: s.draft });
  return new Command({ goto: ok ? "send" : END });
};
```

**Don't wrap `interrupt()` in `try/catch`.** It pauses by throwing a special exception; catching it swallows the pause.

**Keep multiple interrupts in a node in a stable order.** Resume values are matched to interrupts by their order in the node, so don't call them conditionally in ways that change between the first run and the resume.

## Editing state before resuming

The reviewer might fix the draft rather than accept or reject it. Either return their edit as the resume value and apply it in the node, or edit the thread directly and then continue:

```ts
await graph.updateState(cfg, { draft: "Reviewed announcement" });
await graph.invoke(new Command({ resume: "yes" }), cfg);
```

## Static breakpoints (for debugging)

`interrupt()` is for asking a human a question from inside a node. For simply *stopping before or after a node* (for example while developing, to inspect state), compile with breakpoints:

```ts
const graph = builder.compile({
  checkpointer,
  interruptBefore: ["send"],
});

await graph.invoke(input, cfg);   // stops before "send"
await graph.invoke(null, cfg);    // continue
```

Resume a static breakpoint with `null`. Resume an `interrupt()` with a `Command`. Use dynamic `interrupt()` for product features and breakpoints for debugging.

## Typical patterns

- **Approve or reject** an action before a node performs it (the example above).
- **Review tool calls** in an agent: pause after the model proposes a tool call and before the tools node runs ([ReAct Agent](../02-agents/01-react-agent.md)).
- **Ask for missing information** mid-run and continue with the answer.
- **Edit and continue**: show the draft, accept a revised one.

## Common mistakes

- No checkpointer, or a different `thread_id` on resume. The new call starts a fresh run instead of resuming.
- Passing normal input to resume instead of `new Command({ resume })`; the input is merged as a new run.
- Side effects before `interrupt()` that repeat on resume.
- Catching the interrupt with `try/catch`.
- Non-serializable values in the interrupt payload or resume value.

## Debugging

- `graph.getState(cfg)`: a non-empty `next` means the thread is waiting. Empty means it finished.
- Stream with `"updates"` and watch for the interrupt entry to confirm where the run paused.
- If a resume seems to "do nothing," verify the `thread_id` matches and that you passed a `Command`, not the original input.

## Quick summary

- `interrupt(value)` pauses a node; `new Command({ resume })` continues it, and `interrupt()` returns the resume value.
- Needs a checkpointer and `thread_id`; the pause survives process restarts with a durable saver.
- The node re-runs from the top on resume, so keep pre-interrupt code idempotent and act after approval.
- Never `try/catch` around `interrupt()`.
- Static `interruptBefore` / `interruptAfter` are for debugging; resume them with `null`.

**Next:** [Time Travel](./04-time-travel.md)
