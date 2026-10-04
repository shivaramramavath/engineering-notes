# useReducer

`useReducer` is an alternative to `useState` for state whose updates involve **several related changes or non-trivial logic**. Instead of calling setters all over your component, you describe *what happened* with an **action**, and a single **reducer** function decides how state changes. This file is the API reference; combining it with context at scale is in [`../13-state-management/02-reducer-and-context-pattern.md`](../13-state-management/02-reducer-and-context-pattern.md).

## Prerequisites

[`01-useState.md`](./01-useState.md) and [`../02-state-and-rendering/01-state-updates-and-batching.md`](../02-state-and-rendering/01-state-updates-and-batching.md)

---

## Syntax

```jsx
const [state, dispatch] = useReducer(reducer, initialState);
```

| Part | Meaning |
|------|---------|
| `reducer(state, action)` | A **pure function** that takes the current state and an action and returns the **next state** |
| `initialState` | The starting value |
| `state` | Current state (a snapshot, like `useState`) |
| `dispatch(action)` | Sends an action to the reducer and schedules a re-render |

An action is usually an object with a `type` and optional data:

```js
dispatch({ type: "added", text: "Buy milk" });
```

---

## Example: a todo list

```jsx
import { useReducer } from "react";

function todosReducer(todos, action) {
  switch (action.type) {
    case "added":
      return [...todos, { id: action.id, text: action.text, done: false }];
    case "toggled":
      return todos.map((t) =>
        t.id === action.id ? { ...t, done: !t.done } : t
      );
    case "deleted":
      return todos.filter((t) => t.id !== action.id);
    default:
      throw new Error(`Unknown action: ${action.type}`);
  }
}

let nextId = 0;

function TodoApp() {
  const [todos, dispatch] = useReducer(todosReducer, []);

  return (
    <>
      <AddTodo
        onAdd={(text) => dispatch({ type: "added", id: nextId++, text })}
      />
      <ul>
        {todos.map((t) => (
          <li key={t.id}>
            <label>
              <input
                type="checkbox"
                checked={t.done}
                onChange={() => dispatch({ type: "toggled", id: t.id })}
              />
              {t.text}
            </label>
            <button onClick={() => dispatch({ type: "deleted", id: t.id })}>
              Delete
            </button>
          </li>
        ))}
      </ul>
    </>
  );
}
```

The component only describes **what happened** (`added`, `toggled`, `deleted`). *How* state changes lives in one place, the reducer.

---

## Writing reducers

Reducers must be **pure**, just like rendering ([`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md)):

- Same inputs → same output.
- **Don't mutate** `state` — return a new value (same immutable-update rules as `useState`).
- No side effects: no network calls, timers, or random values. Generate ids and call APIs in the **event handler**, then dispatch the result.
- React may call a reducer twice in development (StrictMode) to catch impurity.

Conventions:

- Use a `switch` on `action.type` and **throw on unknown actions** so typos fail loudly.
- Name action types as **what happened** (`"item_added"`, `"form_reset"`), not as setter commands (`"set_items"`).
- Each action should describe **one user interaction**, even if it changes several state fields.

Handle multiple fields in one action:

```jsx
case "reset_form":
  return { ...state, name: "", email: "", errors: {} };
```

This is the main advantage: a single action updates many related fields atomically.

---

## `useState` vs `useReducer`

| | `useState` | `useReducer` |
|---|-----------|--------------|
| Code size | Less for simple state | More upfront (reducer + actions) |
| Many handlers updating the same state | Logic scattered | Centralized |
| Related fields changing together | Several setters | One action |
| Complex transitions / state machines | Awkward | Natural |
| Testing | Test the component | Test the reducer as a plain function |
| Debugging | Log in each setter | Log each action in one place |

Start with `useState`. Move to `useReducer` when:

- many event handlers modify the same state in similar ways,
- state fields must change together or depend on each other,
- the next state depends on complex rules, or
- you want to test the update logic in isolation.

You can mix both in one component.

---

## Lazy initialization

A third argument, an init function, computes the initial state once:

```jsx
function init(initialTodos) {
  return initialTodos.map((t) => ({ ...t, done: false }));
}

const [todos, dispatch] = useReducer(todosReducer, initialTodos, init);
```

Passing `init(initialTodos)` (calling it) would run it every render; pass the function itself.

---

## A status-based reducer

Reducers are a great fit for state machines, which prevent impossible states ([`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)):

```jsx
function fetchReducer(state, action) {
  switch (action.type) {
    case "fetch_started":
      return { status: "loading", data: null, error: null };
    case "fetch_succeeded":
      return { status: "success", data: action.data, error: null };
    case "fetch_failed":
      return { status: "error", data: null, error: action.error };
    default:
      throw new Error(`Unknown action: ${action.type}`);
  }
}
```

A given action always leads to a well-defined state — you can't end up "loading and error at once".

---

## `dispatch` is stable

The `dispatch` function has a **stable identity** across renders, so you can safely pass it to children or list it in effect dependencies without causing re-runs. This is one reason `useReducer` pairs well with context ([`../13-state-management/02-reducer-and-context-pattern.md`](../13-state-management/02-reducer-and-context-pattern.md)).

---

## Debugging

Because every change flows through the reducer, add a `console.log(action, state)` at the top to see exactly what happened and in what order. Libraries and DevTools for Redux-style tools work the same way ([`../13-state-management/04-redux-toolkit.md`](../13-state-management/04-redux-toolkit.md)).

---

## TypeScript

Discriminated unions for actions give you exhaustive checking in the `switch` — see [`../04-typescript-with-react/02-typing-hooks.md`](../04-typescript-with-react/02-typing-hooks.md).

---

## Common mistakes

- **Mutating state in the reducer** (`state.push(...)`) — return a new value.
- **Side effects in a reducer** (API calls, `Math.random()`, `Date.now()`) — do them in the handler and pass results in the action.
- **Forgetting `default: throw`** — typos in action types silently do nothing.
- **Actions named like setters** (`"set_name"`) — describe the event instead (`"name_changed"`).
- **Using a reducer for trivial state** — a single boolean doesn't need one.
- **Dispatching multiple actions to do one conceptual thing** — make one action that updates all affected fields.
- **Returning nothing in a `case`** — you'd set state to `undefined`; always return the new state.

## Quick summary

- `useReducer(reducer, initialState)` returns `[state, dispatch]`
- Reducers are pure functions: `(state, action) => nextState`, with no mutation or side effects
- Dispatch actions describing what happened; keep update logic in one place
- Choose it over `useState` for related fields, complex transitions, or testable logic
- `dispatch` is stable; reducers pair well with context for larger state

## Next

**[`07-useMemo-and-useCallback.md`](./07-useMemo-and-useCallback.md)** covers caching values and functions between renders.
