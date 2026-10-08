# State Machines

A **state machine** describes something that is always in exactly one of a fixed set of **states**, and moves between them only through defined **transitions** triggered by **events**. TypeScript is unusually good at this: discriminated unions make each state a distinct type carrying only the data valid for that state, so many illegal states cannot even be written down.

**Prerequisites:**
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)
- [Exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md)
- [Type narrowing](../03-unions-and-narrowing/03-type-narrowing.md)

---

## The problem: a pile of booleans

```ts
interface FetchState {
  isLoading: boolean;
  isError: boolean;
  data?: User;
  error?: Error;
}
```

This type permits nonsense: `isLoading: true` together with `isError: true`, `data` present alongside an error, or `isError: true` with no `error`. Code must defend against combinations that should never exist, and bugs appear in the combinations nobody handled.

## Model states as a discriminated union

```ts
type FetchState =
  | { status: "idle" }
  | { status: "loading" }
  | { status: "success"; data: User }
  | { status: "error"; error: Error };
```

Each state carries exactly the data that makes sense for it. There is no way to have `data` while `loading`, or `error` while `success`. Narrowing on `status` gives you the right fields:

```ts
function render(state: FetchState): string {
  switch (state.status) {
    case "idle":    return "Press load";
    case "loading": return "Loading...";
    case "success": return `Hello ${state.data.name}`;    // data exists only here
    case "error":   return `Failed: ${state.error.message}`;
  }
}
```

The principle is **make illegal states unrepresentable**: use the type system so invalid combinations do not compile.

## Add events and a transition function

States say where you are. **Events** say what happened. A pure **transition function** computes the next state from the current state and an event:

```ts
type FetchEvent =
  | { type: "FETCH" }
  | { type: "RESOLVE"; data: User }
  | { type: "REJECT"; error: Error }
  | { type: "RESET" };

function transition(state: FetchState, event: FetchEvent): FetchState {
  switch (state.status) {
    case "idle":
      return event.type === "FETCH" ? { status: "loading" } : state;

    case "loading":
      if (event.type === "RESOLVE") return { status: "success", data: event.data };
      if (event.type === "REJECT")  return { status: "error", error: event.error };
      return state;

    case "success":
    case "error":
      if (event.type === "FETCH") return { status: "loading" };
      if (event.type === "RESET") return { status: "idle" };
      return state;
  }
}
```

Properties worth having:

- **Pure:** no side effects, same input gives same output, trivially testable.
- **Total:** every state handles every event, usually by returning `state` unchanged (ignore) or throwing (a bug).
- **Exhaustive:** the outer `switch` on `state.status` is checked, so adding a state forces you to decide how it behaves.

It plugs directly into a reducer: `const [state, dispatch] = useReducer(transition, { status: "idle" })` ([hooks](../19-react-and-frontend/02-hooks.md), [state management](../25-real-world-patterns/02-state-management.md)).

## A transition table

For machines with many states, a **table** makes the allowed transitions visible and reviewable. Describe which events each state accepts and where they lead:

```ts
type OrderStatus = "draft" | "submitted" | "paid" | "shipped" | "cancelled";
type OrderEventType = "SUBMIT" | "PAY" | "SHIP" | "CANCEL";

const transitions: Record<OrderStatus, Partial<Record<OrderEventType, OrderStatus>>> = {
  draft:     { SUBMIT: "submitted", CANCEL: "cancelled" },
  submitted: { PAY: "paid", CANCEL: "cancelled" },
  paid:      { SHIP: "shipped", CANCEL: "cancelled" },
  shipped:   {},                       // terminal
  cancelled: {},                       // terminal
};

function next(status: OrderStatus, event: OrderEventType): OrderStatus | null {
  return transitions[status][event] ?? null;      // null means "not allowed"
}

next("draft", "SUBMIT");     // "submitted"
next("shipped", "CANCEL");   // null
```

`Record<OrderStatus, ...>` guarantees every state has an entry. The whole workflow is readable in a few lines, and tests can iterate over the table.

You can go further and derive, at the type level, **which events are valid in which state**, so calling an invalid one fails to compile:

```ts
type Transitions = typeof transitions;
type EventsFor<S extends OrderStatus> = keyof Transitions[S] & OrderEventType;

type A = EventsFor<"draft">;     // "SUBMIT" | "CANCEL"
type B = EventsFor<"shipped">;   // never
```

This only helps if the *state itself* is known statically. Often it is a runtime value, so a runtime check (`next` returning `null`) is still needed.

## States with data and guards

Real workflows attach data to states, and the table above loses it. Combine both: union states for data, a function for transitions.

```ts
type Order =
  | { status: "draft"; items: Item[] }
  | { status: "submitted"; items: Item[]; submittedAt: Date }
  | { status: "paid"; items: Item[]; submittedAt: Date; paymentId: string }
  | { status: "shipped"; items: Item[]; paymentId: string; trackingNumber: string }
  | { status: "cancelled"; reason: string };

function submit(order: Order): Order {
  if (order.status !== "draft") throw new Error(`Cannot submit from ${order.status}`);
  if (order.items.length === 0) throw new Error("Cannot submit an empty order");   // a guard
  return { status: "submitted", items: order.items, submittedAt: new Date() };
}

function pay(order: Order, paymentId: string): Order {
  if (order.status !== "submitted") throw new Error(`Cannot pay from ${order.status}`);
  return { ...order, status: "paid", paymentId };
}
```

- A **guard** is a condition that must hold for a transition (non-empty order, sufficient balance).
- Each transition function accepts the broad `Order`, narrows on `status`, and returns the next state with the new data it requires.
- Fields that only exist in later states (`paymentId`, `trackingNumber`) cannot be read earlier, because the type forbids it.

### Typing transitions more tightly

You can make functions accept only the state they are valid for, so illegal calls do not compile:

```ts
type DraftOrder = Extract<Order, { status: "draft" }>;
type SubmittedOrder = Extract<Order, { status: "submitted" }>;

function submit(order: DraftOrder): SubmittedOrder { /* ... */ }
function pay(order: SubmittedOrder, paymentId: string): PaidOrder { /* ... */ }

pay(draftOrder, "p1");    // error: a draft order cannot be paid
```

`Extract` picks a member out of the union ([Exclude, Extract, NonNullable](../07-utility-types/02-exclude-extract-nonnullable.md)). This moves checking from runtime to compile time when the caller statically knows the state, and you still keep the broad `Order` type for storage and loading.

## Effects: keep the machine pure

State machines handle *what state comes next*. They should not perform I/O. Return the **effects** to run, separately:

```ts
type Effect = { type: "fetchUser"; id: string } | { type: "log"; message: string };

function step(state: FetchState, event: FetchEvent): [FetchState, Effect[]] {
  if (state.status === "idle" && event.type === "FETCH") {
    return [{ status: "loading" }, [{ type: "fetchUser", id: "42" }]];
  }
  return [state, []];
}
```

A small runner executes the effects and feeds results back as events. The transition logic stays pure and testable, and the side effects are visible as data.

## Libraries

For complex behavior (nested or parallel states, delays, history, visualization), a library such as **XState** implements statecharts with TypeScript support, and tools to visualize and simulate machines. Its APIs have changed across major versions, so follow its current documentation. For small and medium cases, a union plus a reducer is often all you need.

## Where state machines fit

- **UI flows:** forms, wizards, data fetching, modals, drag and drop, media players.
- **Business workflows:** orders, approvals, subscriptions, document lifecycles.
- **Protocols and connections:** websocket reconnection, authentication handshakes.
- **Games and simulations:** character or level state.
- Anywhere you find yourself writing `isThis && !isThat` conditions across several booleans.

## Testing

Because transitions are pure functions, tests are table-driven:

```ts
it.each([
  ["idle", { type: "FETCH" }, "loading"],
  ["loading", { type: "REJECT", error: new Error("x") }, "error"],
] as const)("%s + %j -> %s", (from, event, to) => { /* build state, apply, assert status */ });
```

Also test **invalid transitions**: assert the state is unchanged (or an error is raised) when an event is not allowed. For persistent machines, test that stored states can be loaded again ([unit testing](../18-testing-and-debugging/00-unit-testing.md)).

## Persisting and loading states

Saved states come back from storage or a network as untrusted data. Validate them against the same union (a discriminated-union schema) before treating them as an `Order` ([validation recipes](../15-runtime-validation/04-validation-recipes.md)). A state that does not match any member of the union must be rejected, not coerced.

## Important rules and misconceptions

- **A state is not a flag.** Model it as a union member that carries its own data.
- **The transition function should be pure.** Side effects belong outside it.
- **"Ignore unknown events" is a decision.** For UI machines it is usually fine. For business workflows, an invalid transition is often an error to surface.
- **Types make illegal states unrepresentable only if you let them.** Optional fields on a single broad object bring the problem back.
- **A union of states and a union of events are separate things.** Do not merge them.

## Common mistakes

- Boolean flags (`isLoading`, `isError`) instead of a union.
- One object type with many optional fields instead of per-state data.
- Forgetting a `default` / `never` check, so a new state silently falls through.
- Transition functions that perform I/O or read the clock.
- Allowing transitions that business rules forbid because the table is not enforced in the code that mutates state.
- Mutating the state object instead of returning a new one.
- Trusting persisted state without validation.
- Reaching for a heavy library for a three-state fetch.

## Debugging

- Log each `(state, event) -> next state` transition. The sequence usually reveals the bug immediately.
- If a transition is unexpectedly ignored, check whether the current state handles that event type at all.
- If TypeScript complains about missing fields when constructing a state, that is the type system pointing at a transition that forgets required data.
- Add an exhaustive `never` check in the outer `switch` so adding a state flags every place to update.
- Visualize the table (or use a tool) to spot unreachable states and dead ends.

## Quick summary

- Model each state as a member of a discriminated union carrying only its own data. That makes illegal states unrepresentable.
- Drive change with events and a **pure transition function**, exhaustive over states. It plugs into `useReducer` and tests easily.
- Use a transition table when the workflow is large, `Extract` to type state-specific functions, and guards for rules.
- Keep side effects out by returning effects as data.
- Validate persisted state, and consider a library like XState only when you need statechart features.

**Next:** [Typed event emitter](./07-typed-event-emitter.md)
