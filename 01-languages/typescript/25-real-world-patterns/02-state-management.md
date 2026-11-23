# Advanced State Management in TypeScript: Reducers, Zustand, and State Machines

> Architectural mastery of modern TypeScript state management: discriminated union reducers, immutable lenses, Zustand store typing, finite state machines, and optimistic updates.

| Metadata | Details |
| :--- | :--- |
| **Category** | Real-World Patterns |
| **Difficulty** | Advanced |
| **Prerequisites** | [01-fundamentals/06-type-widening-narrowing.md](../01-fundamentals/06-type-widening-narrowing.md), [19-react-and-frontend/08-client-state-zustand.md](../19-react-and-frontend/08-client-state-zustand.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
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

State management is the most frequent source of bugs in modern applications. Common failures include:
- Unhandled action types failing silently in reducers.
- Accidental state mutations causing missed UI re-renders.
- Race conditions during asynchronous optimistic updates.
- Impossible state transitions (e.g. transitioning from `LOGGED_OUT` to `PROCESSING_PAYMENT` without authenticating).

TypeScript transforms state management from an error-prone task into a mathematically verified state machine:
- **Discriminated Union Reducers**: Action types are statically matched; unhandled actions are prevented via exhaustive checks.
- **Finite State Machines (FSM)**: Allowed state transitions are codified in the type system, making invalid transitions unrepresentable.
- **Zustand & Immutable Stores**: Full inference of state, actions, and selectors without boilerplate.

---

## 2. Core Concepts

### Discriminated Action Reducer
An action is defined as a discriminated union tagged by `type`. In the reducer, switching on `action.type` narrows the payload type automatically.

```
+-------------------------------------------------------------------------+
|                  Type-Safe State Machine Execution Flow                 |
+-------------------------------------------------------------------------+
|                                                                         |
|                          Current State: IDLE                            |
|                                   |                                     |
|                                   v (Action: FETCH)                     |
|                        State Transition Matrix                          |
|                                   |                                     |
|          +------------------------+------------------------+            |
|          | Valid Transition                                | Invalid    |
|          v                                                 v            |
|  New State: LOADING                                   Compile Error or  |
|          |                                            Rejected at FSM   |
|          v (Action: SUCCESS)                                            |
|  New State: SUCCESS (with Data)                                         |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Finite State Machine (FSM) Principle
Instead of tracking independent booleans (`isLoading: boolean`, `isSuccess: boolean`), an FSM defines explicit discrete states and the exact events that trigger transitions between them.

---

## 3. How It Works Internally

1. **Control Flow Analysis**: When `switch (action.type)` is executed, TypeScript removes unmatched union branches from the active scope.
2. **Exhaustive Default Check**: By assigning the remaining action to type `never`, the compiler ensures that every union member has been handled.

---

## 4. Syntax & API Reference

### Discriminated Action Types
```typescript
export type AuthAction =
  | { readonly type: "AUTH_START" }
  | { readonly type: "AUTH_SUCCESS"; readonly token: string; readonly user: { id: string } }
  | { readonly type: "AUTH_FAILURE"; readonly error: string }
  | { readonly type: "LOGOUT" };
```

### Type-Safe Pure Reducer
```typescript
export interface AuthState {
  readonly status: "UNAUTHENTICATED" | "AUTHENTICATING" | "AUTHENTICATED";
  readonly token?: string;
  readonly error?: string;
}

export function authReducer(state: AuthState, action: AuthAction): AuthState {
  switch (action.type) {
    case "AUTH_START":
      return { status: "AUTHENTICATING" };
    case "AUTH_SUCCESS":
      return { status: "AUTHENTICATED", token: action.token };
    case "AUTH_FAILURE":
      return { status: "UNAUTHENTICATED", error: action.error };
    case "LOGOUT":
      return { status: "UNAUTHENTICATED" };
    default: {
      const _unreachable: never = action;
      throw new Error(`Unhandled action: ${_unreachable}`);
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Type-Safe Finite State Machine (FSM)
```typescript
type MachineState =
  | { readonly status: "IDLE" }
  | { readonly status: "RECORDING"; readonly startTime: number }
  | { readonly status: "PAUSED"; readonly recordedMs: number };

type MachineEvent =
  | { readonly type: "START" }
  | { readonly type: "PAUSE" }
  | { readonly type: "RESUME" }
  | { readonly type: "STOP" };

export function audioRecorderFsm(state: MachineState, event: MachineEvent): MachineState {
  switch (state.status) {
    case "IDLE":
      if (event.type === "START") return { status: "RECORDING", startTime: Date.now() };
      return state;

    case "RECORDING":
      if (event.type === "PAUSE") {
        return { status: "PAUSED", recordedMs: Date.now() - state.startTime };
      }
      if (event.type === "STOP") return { status: "IDLE" };
      return state;

    case "PAUSED":
      if (event.type === "RESUME") {
        return { status: "RECORDING", startTime: Date.now() - state.recordedMs };
      }
      if (event.type === "STOP") return { status: "IDLE" };
      return state;
  }
}
```

### Example 2: Zustand Store with Curried Create and Actions
```typescript
import { create } from "zustand";

interface CounterState {
  count: number;
  increment: (by?: number) => void;
  reset: () => void;
}

export const useCounterStore = create<CounterState>()((set) => ({
  count: 0,
  increment: (by = 1) => set((state) => ({ count: state.count + by })),
  reset: () => set({ count: 0 }),
}));
```

### Example 3: Edge Case - Optimistic Update Rollback Manager
```typescript
export interface OptimisticTransaction<T> {
  readonly id: string;
  readonly previousState: T;
  readonly optimisticState: T;
}

export class OptimisticManager<T> {
  private history: OptimisticTransaction<T>[] = [];

  constructor(private currentState: T) {}

  public applyOptimistic(id: string, updateFn: (state: T) => T): T {
    const previous = this.currentState;
    const next = updateFn(previous);
    this.history.push({ id, previousState: previous, optimisticState: next });
    this.currentState = next;
    return this.currentState;
  }

  public rollback(id: string): T {
    const entry = this.history.find((h) => h.id === id);
    if (entry) {
      this.currentState = entry.previousState;
      this.history = this.history.filter((h) => h.id !== id);
    }
    return this.currentState;
  }

  public commit(id: string): void {
    this.history = this.history.filter((h) => h.id !== id);
  }
}
```

### Example 4: Compiler Error & Unhandled Action in Exhaustive Reducer
```typescript
type SimpleAction = { type: "A" } | { type: "B" } | { type: "NEW_ACTION" };

function testReducer(action: SimpleAction): void {
  switch (action.type) {
    case "A": break;
    case "B": break;
    default: {
      // @ts-expect-error - TS2322: Type '{ type: "NEW_ACTION"; }' is not assignable to type 'never'.
      const _exhaustive: never = action;
    }
  }
}
```

### Example 5: Immutable Lens Setter
```typescript
export function updateNestedProperty<T, K1 extends keyof T, K2 extends keyof T[K1]>(
  state: T,
  prop1: K1,
  prop2: K2,
  value: T[K1][K2]
): T {
  return {
    ...state,
    [prop1]: {
      ...state[prop1],
      [prop2]: value,
    },
  };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Object Mutation in Zustand**:
   In Zustand, modifying `state.user.name = "Alice"` inside `set()` mutates the existing state reference, breaking shallow equality comparison and preventing subscribed UI components from re-rendering. Always return new immutable object copies.
2. **Payload Widening**:
   Ensure action payloads use literal types or `as const` where appropriate to avoid widening literal union types to `string`.
3. **Out-of-Order Optimistic Commits**:
   If Transaction A commits after Transaction B rolls back, state can become corrupted unless tracked via an append-only transaction log.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Missing Exhaustive Check** | Empty `default:` in reducer | Assert `never` in default branch | Unhandled actions fail silently |
| **Direct State Mutation** | `state.count++` in store | Return new object `{ count: state.count + 1 }` | UI fails to re-render |
| **Loose Action Strings** | `type: string` in actions | Use string literal union | Typo in action type uncaught |
| **Over-subscribing in Zustand** | `useStore()` without selector | `useStore(s => s.count)` | Component re-renders on any store edit |
| **Independent Booleans** | `isPending`, `isSuccess`, `isFailed` | Discriminated status union | Conflicting state combinations |
| **Untyped Middleware** | Untyped dispatch intercepts | Strongly typed Redux/Zustand middleware | Action payloads bypass validation |

---

## 8. Performance & Type-Checker Complexity

- Action unions with up to 100 variants evaluate in linear $O(N)$ time during switch-case checking.
- Zustand selectors (`useStore(state => state.property)`) use shallow reference equality checks, bypassing the virtual DOM reconciliation tree when unrelated state changes.

---

## 9. Compiler Internals

During reducer evaluation:
1. The compiler's **Control Flow Analysis** tracks the discriminative key (`action.type`).
2. Inside each `case "LABEL"`, the TypeChecker narrows the symbol `action` to the exact subtype with `type: "LABEL"`.
3. In the `default` branch, CFA determines that all union branches have been exhausted, narrowing `action` to `never`.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Discriminated Unions for Actions and States**.
2. **Guarantee Reducer Exhaustiveness via `never`**.
3. **Keep State Immutable**: Treat all store states as `readonly`.
4. **Use Granular Selectors in Zustand**: Prevent unnecessary UI re-renders.

---

## 11. Real-World Case Study

A collaborative document editing web app suffered from lost updates and corrupted cursor selections. The root cause was race conditions in an untyped Redux reducer that accepted untyped WebSocket messages.

**Architectural Overhaul**:
- Converted editor state to a formal Finite State Machine (`DISCONNECTED`, `CONNECTING`, `SYNCING`, `READY`).
- Replaced 22 loosely typed actions with a discriminated union of 8 verified action types.
- Enforced exhaustive compiler checking in the root reducer.

**Outcome**: Document synchronization errors were **eliminated**, and state-related bug reports decreased by **84%**.

---

## 12. Interview Questions

<details>
<summary>1. How does TypeScript ensure exhaustive checking in a reducer?</summary>

By assigning the unhandled action in the `default:` branch of the switch statement to a variable of type `never` (`const _exhaustive: never = action`). If any action variant is not handled in a `case` block, TypeScript throws a compile-time error TS2322 because that action type cannot be assigned to `never`.
</details>

<details>
<summary>2. Why are Finite State Machines (FSMs) superior to multiple boolean flags?</summary>

Multiple booleans create $2^N$ combinatorial states, allowing impossible or contradictory states (such as `isLoading: true` and `isSuccess: true` simultaneously). An FSM restricts the system to a single active state at any time and explicitly governs valid transitions.
</details>

<details>
<summary>3. Why must Zustand stores return immutable updates?</summary>

Zustand uses shallow reference equality (`===`) to determine whether state has changed. If state properties are mutated directly in-place, the object reference remains identical, causing Zustand to conclude nothing changed and skipping UI subscriber re-renders.
</details>

<details>
<summary>4. What is a selector in Zustand and why is it important for performance?</summary>

A selector is a function passed to the store hook (e.g. `useStore(s => s.user.name)`). It instructs the hook to subscribe *only* to changes in the selected slice of state, preventing the component from re-rendering when unrelated state properties change.
</details>

<details>
<summary>5. How do you model an optimistic update with rollback capability?</summary>

By capturing the state before applying the optimistic update in a history stack. If the remote network mutation succeeds, the history record is cleared; if it fails, the previous state is restored from the history stack.
</details>

<details>
<summary>6. What does the "curried create" syntax in Zustand (`create<T>()(...)`) achieve?</summary>

It works around a TypeScript limitation where passing type parameters to a function disables contextual type inference for the function's arguments. Currying allows developers to explicitly specify the store interface `create<State>()` while preserving full type inference inside the callback.
</details>

<details>
<summary>7. What is an immutable lens?</summary>

A lens is a functional programming concept providing composable, type-safe getters and setters that produce new immutable data structures when updating deeply nested properties.
</details>

<details>
<summary>8. How do you type asynchronous actions in a custom state store?</summary>

By defining actions as methods returning `Promise<void>`, updating the store state before the async call (loading), after resolution (success), and within a catch block (error).
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Basic counter reducer with exhaustive check</summary>

**Task**: Write an exhaustive reducer for counter actions: `INCREMENT`, `DECREMENT`, `RESET`.

**Solution**:
```typescript
type CounterAction =
  | { type: "INCREMENT"; by?: number }
  | { type: "DECREMENT"; by?: number }
  | { type: "RESET" };

export function counterReducer(count: number, action: CounterAction): number {
  switch (action.type) {
    case "INCREMENT": return count + (action.by ?? 1);
    case "DECREMENT": return count - (action.by ?? 1);
    case "RESET": return 0;
    default: {
      const _unreachable: never = action;
      throw new Error(`Unhandled action: ${_unreachable}`);
    }
  }
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Minimal Zustand store</summary>

**Task**: Create a typed Zustand store managing a boolean modal visibility toggle.

**Solution**:
```typescript
import { create } from "zustand";

interface ModalStore {
  isOpen: boolean;
  open: () => void;
  close: () => void;
}

export const useModalStore = create<ModalStore>()((set) => ({
  isOpen: false,
  open: () => set({ isOpen: true }),
  close: () => set({ isOpen: false }),
}));
```
</details>

<details>
<summary>Challenge 3 (Medium): Type-Safe Traffic Light FSM</summary>

**Task**: Implement a state transition function for a traffic light that cycles `GREEN` -> `YELLOW` -> `RED` -> `GREEN`.

**Solution**:
```typescript
export type LightColor = "GREEN" | "YELLOW" | "RED";

export function nextTrafficLight(current: LightColor): LightColor {
  switch (current) {
    case "GREEN": return "YELLOW";
    case "YELLOW": return "RED";
    case "RED": return "GREEN";
  }
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Generic Action Creator Factory</summary>

**Task**: Write a type-safe action creator factory function that pairs action types with payloads.

**Solution**:
```typescript
export function createAction<TType extends string, TPayload>(type: TType) {
  return (payload: TPayload) => ({ type, payload } as const);
}

const setUser = createAction<"SET_USER", { id: string; name: string }>("SET_USER");
const action = setUser({ id: "1", name: "Alice" });
```
</details>

<details>
<summary>Challenge 5 (Hard): Async Thunk Action Middleware in Pure TypeScript</summary>

**Task**: Implement a lightweight state container supporting asynchronous thunk actions with dispatch typing.

**Solution**:
```typescript
export type Listener<T> = (state: T) => void;

export class MiniStore<TState> {
  private listeners = new Set<Listener<TState>>();

  constructor(private state: TState) {}

  public getState(): TState {
    return this.state;
  }

  public setState(updater: (prev: TState) => TState): void {
    this.state = updater(this.state);
    this.listeners.forEach((fn) => fn(this.state));
  }

  public subscribe(listener: Listener<TState>): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  public dispatch(action: (store: MiniStore<TState>) => Promise<void> | void): void {
    action(this);
  }
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Optimistic Todo List with Revert Action</summary>

**Task**: Implement a Todo list manager that supports adding items optimistically with instant ID generation and rolling back if network sync fails.

**Solution**:
```typescript
export interface Todo {
  id: string;
  text: string;
  synced: boolean;
}

export class TodoListManager {
  private todos: Todo[] = [];

  public addTodoOptimistic(tempId: string, text: string): void {
    this.todos.push({ id: tempId, text, synced: false });
  }

  public confirmAdd(tempId: string, permanentId: string): void {
    const todo = this.todos.find((t) => t.id === tempId);
    if (todo) {
      todo.id = permanentId;
      todo.synced = true;
    }
  }

  public rollbackAdd(tempId: string): void {
    this.todos = this.todos.filter((t) => t.id !== tempId);
  }

  public getTodos(): readonly Todo[] {
    return this.todos;
  }
}
```
</details>
