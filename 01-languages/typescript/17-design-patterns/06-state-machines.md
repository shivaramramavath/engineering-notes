# State Machine Pattern and Finite State Machines (FSM) in TypeScript

> Modeling finite state machines in TypeScript: discriminated union state transitions, illegal state elimination, pure reducer transition tables, XState concepts, and hierarchical state machines.

| Metadata | Details |
| :--- | :--- |
| **Category** | Design Patterns |
| **Difficulty** | Advanced |
| **Prerequisites** | [04-objects-and-interfaces/02-discriminated-unions.md](../04-objects-and-interfaces/02-discriminated-unions.md), [10-advanced-types/00-conditional-types.md](../10-advanced-types/00-conditional-types.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

A **Finite State Machine (FSM)** is a mathematical model of computation that can be in exactly one of a finite number of states at any given time. The machine changes from one state to another in response to external inputs (events); the change from one state to another is called a **transition**.

In naive TypeScript codebases, developers model UI and domain states using multiple independent boolean flags:
```typescript
interface BadState {
  isLoading: boolean;
  isError: boolean;
  isSuccess: boolean;
  data: User | null;
  error: Error | null;
}
```
This flag-based approach introduces the **Boolean Explosion** problem:
With 3 booleans, there are $2^3 = 8$ possible combinations, including illegal impossible states like `{ isLoading: true, isSuccess: true, isError: true }`! Codebases become riddled with defensive `if (isLoading && !isError)` guards.

By implementing the State Machine pattern with TypeScript's **discriminated unions** and **literal transition tables**, impossible states are rendered unrepresentable at compile time.

```
       +-------------------------------------------------------+
       |                                                       |
       v                   FETCH                               |
  [ IDLE ] -------------------------------------> [ LOADING ]  |
                                                      |        |
                                         +------------+        |
                                 SUCCESS |            | ERROR  |
                                         v            v        |
                                   [ SUCCESS ]    [ ERROR ]    |
                                         |            |        |
                                         +------------+--------+
                                                 RESET
```

---

## 2. Core Concepts

### 1. States as Discriminated Unions
Each state is an object with a literal `status` tag and only the data relevant to that state:
```typescript
export type FetchState<T> =
  | { readonly status: 'idle' }
  | { readonly status: 'loading' }
  | { readonly status: 'success'; readonly data: T }
  | { readonly status: 'error'; readonly error: Error };
```

### 2. Events as Tagged Unions
Events are discriminated by a `type` property, carrying optional payload data:
```typescript
export type FetchEvent<T> =
  | { readonly type: 'FETCH' }
  | { readonly type: 'RESOLVE'; readonly data: T }
  | { readonly type: 'REJECT'; readonly error: Error }
  | { readonly type: 'RESET' };
```

### 3. Transition Function (Pure Reducer)
The transition function $T: (\text{State}, \text{Event}) \to \text{State}$ deterministically maps the current state and incoming event to the next state:
```typescript
export type TransitionFn<S, E> = (state: S, event: E) => S;
```

---

## 3. How It Works Internally

When processing a transition:
1. The transition handler examines `state.status`.
2. Within each state branch, it inspects `event.type`.
3. If the transition is valid, it returns the new state object.
4. If the transition is invalid, it either ignores the event (returns the unmodified state) or throws an invalid transition exception.

```
Input: State({ status: 'idle' }) + Event({ type: 'RESOLVE' })
  -> Check transition table for 'idle' + 'RESOLVE'
  -> No valid transition! Return unmodified state ({ status: 'idle' }).
```

Flow-sensitive analysis ensures that inside `case 'success':`, accessing `state.data` is guaranteed to be safe without optional chaining (`?.`).

---

## 4. Syntax & API Reference

### Declarative Transition Table Interface
```typescript
export interface MachineConfig<TState extends string, TEvent extends string> {
  initial: TState;
  transitions: {
    [KState in TState]?: {
      [KEvent in TEvent]?: TState;
    };
  };
}
```

---

## 5. Practical Examples

### Example 1: Basic Declarative State Machine
```typescript
export type LightState = 'green' | 'yellow' | 'red';
export type LightEvent = 'TIMER';

export const trafficLightTransitions: Record<LightState, Record<LightEvent, LightState>> = {
  green: { TIMER: 'yellow' },
  yellow: { TIMER: 'red' },
  red: { TIMER: 'green' }
};

export class TrafficLightMachine {
  private current: LightState = 'green';

  public transition(event: LightEvent): LightState {
    const next = trafficLightTransitions[this.current][event];
    if (next) this.current = next;
    return this.current;
  }

  public getState(): LightState { return this.current; }
}
```

### Example 2: Real-World Async Data Fetching State Machine
```typescript
export type RequestState<T> =
  | { status: 'idle' }
  | { status: 'pending' }
  | { status: 'resolved'; data: T }
  | { status: 'rejected'; error: string };

export type RequestEvent<T> =
  | { type: 'START' }
  | { type: 'SUCCESS'; data: T }
  | { type: 'FAILURE'; error: string }
  | { type: 'RESET' };

export function requestReducer<T>(state: RequestState<T>, event: RequestEvent<T>): RequestState<T> {
  switch (state.status) {
    case 'idle':
      return event.type === 'START' ? { status: 'pending' } : state;
    case 'pending':
      if (event.type === 'SUCCESS') return { status: 'resolved', data: event.data };
      if (event.type === 'FAILURE') return { status: 'rejected', error: event.error };
      return state;
    case 'resolved':
    case 'rejected':
      return event.type === 'RESET' ? { status: 'idle' } : state;
  }
}
```

### Example 3: Edge Case - State Machine with Side-Effect Actions
```typescript
export interface DoorMachineContext {
  state: 'locked' | 'unlocked' | 'open';
  autoLockTimer?: ReturnType<typeof setTimeout>;
}

export function handleDoorEvent(
  ctx: DoorMachineContext,
  event: 'KEY_TURN' | 'PUSH' | 'CLOSE'
): DoorMachineContext {
  switch (ctx.state) {
    case 'locked':
      return event === 'KEY_TURN' ? { state: 'unlocked' } : ctx;
    case 'unlocked':
      if (event === 'KEY_TURN') return { state: 'locked' };
      if (event === 'PUSH') return { state: 'open' };
      return ctx;
    case 'open':
      return event === 'CLOSE' ? { state: 'unlocked' } : ctx;
  }
}
```

### Example 4: Failure Case - Attempting to Access State-Specific Properties
```typescript
const currentState: RequestState<string[]> = { status: 'idle' };

// @ts-expect-error TS2339: Property 'data' does not exist on type '{ status: "idle"; }'.
console.log(currentState.data.length);
```

### Example 5: Idiomatic Type-Safe FSM Class with Subscription Listener
```typescript
export class StateMachine<TState extends string, TEvent extends string> {
  private listeners: ((state: TState) => void)[] = [];

  constructor(
    private current: TState,
    private table: Record<TState, Partial<Record<TEvent, TState>>>
  ) {}

  public send(event: TEvent): void {
    const nextState = this.table[this.current]?.[event];
    if (nextState) {
      this.current = nextState;
      this.listeners.forEach((l) => l(this.current));
    }
  }

  public subscribe(listener: (state: TState) => void): () => void {
    this.listeners.push(listener);
    return () => {
      this.listeners = this.listeners.filter((l) => l !== listener);
    };
  }

  public get state(): TState { return this.current; }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Ignoring Invalid Transitions**: Depending on the domain, attempting an invalid transition should either fail silently (idempotent ignore) or throw a descriptive error. Be explicit about which behavior your machine implements.
2. **Context Mutability**: If the state machine stores an extended context object, mutating properties in place causes race conditions in React or asynchronous pipelines. Always return new immutable objects.
3. **State Explosion in Hierarchical Machines**: Without hierarchical or parallel states, combining orthogonal states (e.g., `theme: dark/light` and `auth: loggedIn/loggedOut`) requires squaring the state count. Use parallel state machines for orthogonal concerns.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Independent Boolean Flags** | `isLoaded`, `isLoading`, `isFailed` | Discriminated union of states | Impossible illegal states coexist at runtime |
| **Direct Mutation of State** | `state.status = 'success'` | Return `{ ...state, status: 'success' }` | Breaks time-travel debugging and reactivity |
| **Missing Exhaustiveness Check** | Incomplete `switch (state.status)` | Ensure `default: assertNever(state)` | Unhandled states cause silent bugs |
| **Coupling Actions to Reducers** | Fetching data inside the pure reducer | Dispatch events; handle side effects in effects | Reducer becomes impure and non-deterministic |
| **Missing Illegal Transition Guard** | Permitting payment transitions from "Cancelled" | Reject transition explicitly | Critical business logic corruption |
| **Flattening Orthogonal Concerns** | Single giant 64-state enum | Parallel decomposed state machines | Exponential complexity explosion |

---

## 8. Performance & Type-Checker Complexity

- **Lookups**: Object-based transition tables (`table[state][event]`) execute in constant $O(1)$ time, outperforming long sequential `if/else` checks.
- **Type Checker**: Discriminated unions with fewer than 20 variants compile instantly.

---

## 9. Compiler Internals

During flow analysis of a discriminated union:
```typescript
type S = { status: 'a'; val: number } | { status: 'b'; text: string };
```
The TypeScript compiler assigns a discriminator property ID to `status`. When inspecting `if (s.status === 'a')`, the compiler intersects the flow-node type with the literal constraint, eliminating variant `'b'` from the active type.

---

## 10. Best Practices & Design Guidelines

1. **Make Impossible States Unrepresentable**: Design types so that illegal combinations fail to compile.
2. **Keep State Transitions Pure**: The transition function must be a pure, deterministic function.
3. **Separate State from Side Effects**: Reducers transition state; listeners or middleware execute I/O.
4. **Use Visual Tools (XState)**: For complex business workflows, visualize states with XState or statecharts.

---

## 11. Real-World Case Study

### Payment Gateway Order Checkout Finite State Machine
A mission-critical payment processing machine:

```typescript
export type OrderState =
  | { status: 'DRAFT'; cartItems: string[] }
  | { status: 'AWAITING_PAYMENT'; orderId: string; totalCents: number }
  | { status: 'PAID'; orderId: string; transactionId: string }
  | { status: 'CANCELLED'; reason: string };

export type OrderEvent =
  | { type: 'CHECKOUT'; orderId: string; totalCents: number }
  | { type: 'PAYMENT_CONFIRMED'; transactionId: string }
  | { type: 'CANCEL'; reason: string };

export function orderStateMachine(state: OrderState, event: OrderEvent): OrderState {
  switch (state.status) {
    case 'DRAFT':
      if (event.type === 'CHECKOUT') {
        return { status: 'AWAITING_PAYMENT', orderId: event.orderId, totalCents: event.totalCents };
      }
      return state;
    case 'AWAITING_PAYMENT':
      if (event.type === 'PAYMENT_CONFIRMED') {
        return { status: 'PAID', orderId: state.orderId, transactionId: event.transactionId };
      }
      if (event.type === 'CANCEL') {
        return { status: 'CANCELLED', reason: event.reason };
      }
      return state;
    case 'PAID':
    case 'CANCELLED':
      return state; // Terminal states
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What does it mean to "make impossible states unrepresentable" in TypeScript?</summary>

It means structuring domain types using discriminated unions rather than loose independent properties or boolean flags. For instance, instead of `{ isLoading: boolean, data: User | null, error: Error | null }` (which permits `{ isLoading: false, data: null, error: null }`), we use `{ status: 'loading' } | { status: 'success', data: User } | { status: 'error', error: Error }`. The compiler ensures code can only access `data` when `status === 'success'`.
</details>

<details>
<summary>2. What are the key mathematical components of a Finite State Machine?</summary>

A Finite State Machine consists of:
1. A finite set of **States** ($S$).
2. An **Initial State** ($s_0 \in S$).
3. A finite set of **Events** or Inputs ($\Sigma$).
4. A **Transition Function** ($\delta: S \times \Sigma \to S$).
5. An optional set of **Terminal / Final States** ($F \subseteq S$).
</details>

<details>
<summary>3. Why must the state transition function be a pure function?</summary>

A pure transition function ensures that given the identical state and event, the resulting next state is 100% deterministic and predictable. This allows state machines to be easily tested, recorded, replayed, and debugged without hidden side-effects.
</details>

<details>
<summary>4. What is the difference between a Mealy machine and a Moore machine?</summary>

In a **Moore machine**, outputs or side-effects depend solely on the current state. In a **Mealy machine**, outputs or side-effects depend on both the current state and the incoming transition event.
</details>

<details>
<summary>5. How do you model terminal (absorbing) states in a TypeScript FSM?</summary>

A terminal state is a state from which no outgoing transitions exist. In code, the transition function returns the unmodified state for all incoming events, or rejects them.
</details>

<details>
<summary>6. How do you handle asynchronous side-effects (e.g. HTTP requests) in a State Machine?</summary>

By dispatching events before and after the async operation:
1. Machine enters `'loading'` state.
2. An external effect runner triggers the HTTP request.
3. Upon completion, it dispatches `'SUCCESS'` or `'FAILURE'` events back to the machine.
</details>

<details>
<summary>7. What are the benefits of declarative transition tables over nested switch statements?</summary>

Declarative tables (`table[state][event]`) are data-driven, can be serialized to JSON, visualized automatically in diagramming tools, and checked for missing transitions via mapped types.
</details>

<details>
<summary>8. How do you handle orthogonal (independent) states without state explosion?</summary>

By decomposing them into parallel state machines. Rather than creating a single state machine with $N \times M$ combined states, manage two independent state machines concurrently.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Audio Player State Machine</summary>

**Problem:** Model states `'playing' | 'paused' | 'stopped'` with events `'PLAY'`, `'PAUSE'`, `'STOP'`.

**Solution:**
```typescript
export type PlayerState = 'playing' | 'paused' | 'stopped';
export type PlayerEvent = 'PLAY' | 'PAUSE' | 'STOP';

export const playerTransitions: Record<PlayerState, Partial<Record<PlayerEvent, PlayerState>>> = {
  stopped: { PLAY: 'playing' },
  playing: { PAUSE: 'paused', STOP: 'stopped' },
  paused: { PLAY: 'playing', STOP: 'stopped' }
};
```
</details>

<details>
<summary>Exercise 2 (Easy): Type Guard for Specific State</summary>

**Problem:** Given `RequestState<T>`, write a type guard `isResolved<T>(state: RequestState<T>): state is { status: 'resolved'; data: T }`.

**Solution:**
```typescript
export function isResolved<T>(state: RequestState<T>): state is { status: 'resolved'; data: T } {
  return state.status === 'resolved';
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Strict Transition Table Verifier</summary>

**Problem:** Write a mapped type that verifies every declared state has at least one transition defined.

**Solution:**
```typescript
export type CompleteTable<TState extends string, TEvent extends string> = {
  [K in TState]: Record<TEvent, TState>;
};
```
</details>

<details>
<summary>Exercise 4 (Medium): Event History Logger Machine</summary>

**Problem:** Implement an FSM wrapper that records all transitions into an audit log array.

**Solution:**
```typescript
export class AuditedFSM<S extends string, E extends string> {
  public history: { from: S; event: E; to: S }[] = [];

  constructor(
    private current: S,
    private transitions: Record<S, Partial<Record<E, S>>>
  ) {}

  public transition(event: E): S {
    const next = this.transitions[this.current]?.[event];
    if (next) {
      this.history.push({ from: this.current, event, to: next });
      this.current = next;
    }
    return this.current;
  }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Machine with Context Guards</summary>

**Problem:** Implement a transition evaluator that supports boolean guard functions `(ctx: Context) => boolean` before allowing a transition.

**Solution:**
```typescript
export interface GuardedTransition<S, C> {
  target: S;
  guard?: (ctx: C) => boolean;
}

export function evaluateTransition<S extends string, C>(
  transitions: GuardedTransition<S, C>[],
  ctx: C
): S | null {
  for (const t of transitions) {
    if (!t.guard || t.guard(ctx)) return t.target;
  }
  return null;
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Hierarchical State Matcher</summary>

**Problem:** Create a utility function `matchesState(currentState: string, statePattern: string): boolean` that supports hierarchical dot notation (e.g., `'checkout.payment'` matches `'checkout'`).

**Solution:**
```typescript
export function matchesState(current: string, pattern: string): boolean {
  if (current === pattern) return true;
  return current.startsWith(`${pattern}.`);
}
```
</details>
