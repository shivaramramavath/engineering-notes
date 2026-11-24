# The State Reducer Pattern: Inverting State Control in React

- **Topic**: `05-component-design`
- **React version**: React 19+ (Pattern introduced by Kent C. Dodds)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Advanced architectural pattern for component libraries; provides maximum customizability without prop explosion.

---

## 1. Concept Overview

When designing reusable components (like dropdowns, accordions, wizards, or date pickers), component authors frequently encounter edge-case requests from consumers:
- *"Can the dropdown stay open when selecting with the Shift key held down?"*
- *"Can the accordion prevent closing if the form inside has unsaved edits?"*
- *"Can the stepper skip Step 3 if the user is an enterprise customer?"*

The naive approach is adding a boolean prop for every request:
`<Dropdown closeOnSelect={false} preventClosingOnShift={true} keepOpenOnMultiple={true} ... />`

This leads to **prop bloat**, conflicting prop configurations, and an unmaintainable codebase.

The **State Reducer Pattern** solves this problem through **Inversion of Control (IoC)**. Instead of adding dozens of configuration props, the component exposes its internal state transitions to the consumer. The component computes its proposed next state, passes it through an optional user-defined `stateReducer` function, and applies whatever state the consumer returns!

```
Standard Component (Rigid):
User Event ──► Internal Reducer ──► Computes Next State ──► Updates UI

State Reducer Pattern (Inverted Control):
User Event ──► Internal Reducer
                     │
                     ▼
             Proposed Next State
                     │
                     ▼
         Consumer's `stateReducer` ◄── Consumer can inspect, modify, or veto!
                     │
                     ▼
            Final State Committed ──► Updates UI
```

### When to Use
- Reusable UI component libraries where consumers have varying edge-case requirements.
- Multi-step wizards or checkout funnels with conditional progression rules.
- Autocomplete / Combobox widgets (e.g., Downshift) where selection behaviors vary widely.

### When NOT to Use
- Standard application components where business rules are fixed.
- Simple state transitions (e.g. basic modal open/close) where standard props suffice.

---

## 2. Key Mechanics & Architecture

### The State Reducer Contract
1. The component defines its **Action Types** as a typed enum or constant:
   ```ts
   export const ActionTypes = {
     toggle: 'TOGGLE',
     reset: 'RESET',
     selectItem: 'SELECT_ITEM',
   } as const;
   ```
2. The component defines its default reducer:
   ```ts
   function defaultReducer(state: State, action: Action): State { ... }
   ```
3. The component exposes a `stateReducer` prop to the consumer:
   ```ts
   type StateReducer = (state: State, actionAndChanges: { type: ActionType; changes: State }) => State;
   ```
4. On every dispatch, the component computes `changes = defaultReducer(state, action)` and then calls `stateReducer(state, { type: action.type, changes })`.

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Control Mechanism** | Inversion of Control (IoC) |
| **API Footprint** | Single `stateReducer` prop replaces dozens of boolean flags |
| **Default Fallback** | Returns `changes` if consumer doesn't override |
| **Veto Power** | Consumer can return `state` directly to cancel a state transition |
| **Action Context** | Dispatches carry explicit action types so consumers can target specific events |
| **React Compiler Impact** | `stateReducer` functions must be pure and free of side-effects |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
A reusable `Toggle` hook with State Reducer support allowing consumers to cap max clicks.

```tsx
import React, { useReducer } from 'react';

export const ToggleActionTypes = {
  toggle: 'TOGGLE',
  reset: 'RESET',
} as const;

type Action =
  | { type: typeof ToggleActionTypes.toggle }
  | { type: typeof ToggleActionTypes.reset; initial: boolean };

interface ToggleState {
  on: boolean;
}

function defaultToggleReducer(state: ToggleState, action: Action): ToggleState {
  switch (action.type) {
    case ToggleActionTypes.toggle:
      return { on: !state.on };
    case ToggleActionTypes.reset:
      return { on: action.initial };
    default:
      return state;
  }
}

export type ToggleStateReducer = (
  state: ToggleState,
  actionAndChanges: Action & { changes: ToggleState }
) => ToggleState;

export function useToggle({
  initial = false,
  stateReducer = (state, { changes }) => changes,
}: {
  initial?: boolean;
  stateReducer?: ToggleStateReducer;
} = {}) {
  const [{ on }, dispatch] = useReducer(
    (state: ToggleState, action: Action) => {
      const changes = defaultToggleReducer(state, action);
      return stateReducer(state, { ...action, changes });
    },
    { on: initial }
  );

  return {
    on,
    toggle: () => dispatch({ type: ToggleActionTypes.toggle }),
    reset: () => dispatch({ type: ToggleActionTypes.reset, initial }),
  };
}

// Consumer Usage: Prevent clicking more than 4 times!
export function RateLimitedToggle() {
  const [clickCount, setClickCount] = React.useState(0);

  const { on, toggle } = useToggle({
    stateReducer: (state, actionAndChanges) => {
      if (actionAndChanges.type === ToggleActionTypes.toggle && clickCount >= 4) {
        // Veto! Return current state unchanged
        return state;
      }
      return actionAndChanges.changes;
    },
  });

  return (
    <div className="p-4 border rounded max-w-xs">
      <p className="text-sm">Status: {on ? 'ON' : 'OFF'} (Clicks: {clickCount})</p>
      <button
        onClick={() => {
          setClickCount((c) => c + 1);
          toggle();
        }}
        className="mt-2 px-3 py-1 bg-zinc-900 text-white rounded text-xs"
      >
        Toggle
      </button>
      {clickCount >= 4 && <p className="text-xs text-red-500 mt-1">Click limit reached!</p>}
    </div>
  );
}
```
Output:
```
// Button toggles for the first 4 clicks; subsequent clicks are vetoed by the stateReducer.
```

### Example 2: Production / Real-World Integration
A production Combobox / Multi-Select with State Reducer allowing consumers to keep the menu open when holding Shift.

```tsx
import React, { useReducer } from 'react';

export const ComboboxActions = {
  selectItem: 'SELECT_ITEM',
  openMenu: 'OPEN_MENU',
  closeMenu: 'CLOSE_MENU',
} as const;

interface ComboboxState<T> {
  isOpen: boolean;
  selectedItem: T | null;
}

type ComboboxAction<T> =
  | { type: typeof ComboboxActions.selectItem; item: T; shiftKey?: boolean }
  | { type: typeof ComboboxActions.openMenu }
  | { type: typeof ComboboxActions.closeMenu };

function defaultComboboxReducer<T>(
  state: ComboboxState<T>,
  action: ComboboxAction<T>
): ComboboxState<T> {
  switch (action.type) {
    case ComboboxActions.selectItem:
      return { ...state, selectedItem: action.item, isOpen: false }; // Defaults to closing on select
    case ComboboxActions.openMenu:
      return { ...state, isOpen: true };
    case ComboboxActions.closeMenu:
      return { ...state, isOpen: false };
    default:
      return state;
  }
}

export function useCombobox<T>({
  items,
  stateReducer = (state, { changes }) => changes,
}: {
  items: T[];
  stateReducer?: (
    state: ComboboxState<T>,
    actionAndChanges: ComboboxAction<T> & { changes: ComboboxState<T> }
  ) => ComboboxState<T>;
}) {
  const [state, dispatch] = useReducer(
    (s: ComboboxState<T>, a: ComboboxAction<T>) => {
      const changes = defaultComboboxReducer(s, a);
      return stateReducer(s, { ...a, changes });
    },
    { isOpen: false, selectedItem: null }
  );

  return { state, dispatch };
}

// Consumer Component:
export function MultiSelectConsumer() {
  const items = ['React 19', 'TypeScript', 'Tailwind', 'Next.js'];

  const { state, dispatch } = useCombobox({
    items,
    stateReducer: (currentState, actionAndChanges) => {
      // Custom consumer rule: If shift was held, keep the menu open!
      if (
        actionAndChanges.type === ComboboxActions.selectItem &&
        actionAndChanges.shiftKey
      ) {
        return {
          ...actionAndChanges.changes,
          isOpen: true, // Override the default close!
        };
      }
      return actionAndChanges.changes;
    },
  });

  return (
    <div className="p-4 border rounded max-w-sm">
      <button
        onClick={() => dispatch({ type: ComboboxActions.openMenu })}
        className="px-3 py-1 bg-blue-600 text-white rounded text-sm"
      >
        Select Framework (Hold Shift for multi)
      </button>

      {state.isOpen && (
        <ul className="border mt-2 divide-y rounded">
          {items.map((item) => (
            <li
              key={item}
              onClick={(e) =>
                dispatch({
                  type: ComboboxActions.selectItem,
                  item,
                  shiftKey: e.shiftKey,
                })
              }
              className="p-2 text-sm hover:bg-zinc-100 cursor-pointer"
            >
              {item} {state.selectedItem === item && '✓'}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
```
Output:
```
// Standard click selects and closes menu; Shift+Click selects and keeps menu open!
```

### Example 3: Edge Case / Vetoing Transitions in Multi-Step Wizard
A multi-step checkout wizard where consumer state reducer blocks next step if verification fails.

```tsx
import React, { useReducer } from 'react';

export const WizardActions = {
  nextStep: 'NEXT_STEP',
  prevStep: 'PREV_STEP',
} as const;

interface WizardState {
  step: number;
}

type WizardAction =
  | { type: typeof WizardActions.nextStep }
  | { type: typeof WizardActions.prevStep };

function defaultWizardReducer(state: WizardState, action: WizardAction): WizardState {
  switch (action.type) {
    case WizardActions.nextStep:
      return { step: state.step + 1 };
    case WizardActions.prevStep:
      return { step: Math.max(1, state.step - 1) };
    default:
      return state;
  }
}

export function useWizard(
  totalSteps: number,
  stateReducer?: (state: WizardState, action: WizardAction & { changes: WizardState }) => WizardState
) {
  return useReducer(
    (state: WizardState, action: WizardAction) => {
      const changes = defaultWizardReducer(state, action);
      return stateReducer ? stateReducer(state, { ...action, changes }) : changes;
    },
    { step: 1 }
  );
}

// Consumer: Prevent proceeding from Step 1 to Step 2 if age < 18:
export function AgeGatedCheckout() {
  const [age, setAge] = React.useState(16);

  const [state, dispatch] = useWizard(3, (currentState, actionAndChanges) => {
    if (actionAndChanges.type === WizardActions.nextStep && currentState.step === 1) {
      if (age < 18) {
        alert('You must be 18+ to proceed!');
        return currentState; // Veto transition!
      }
    }
    return actionAndChanges.changes;
  });

  return (
    <div className="p-4 border rounded max-w-sm">
      <h3 className="font-bold">Step {state.step} of 3</h3>
      {state.step === 1 && (
        <div className="my-3">
          <label className="text-xs">Enter Age: </label>
          <input
            type="number"
            value={age}
            onChange={(e) => setAge(Number(e.target.value))}
            className="border p-1 text-xs w-16"
          />
        </div>
      )}
      <button
        onClick={() => dispatch({ type: WizardActions.nextStep })}
        className="px-3 py-1 bg-black text-white text-xs rounded"
      >
        Next Step →
      </button>
    </div>
  );
}
```
Output:
```
// If age < 18, clicking Next Step triggers alert and blocks navigation.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Forgetting to return `actionAndChanges.changes` by default.

```tsx
// ❌ CRITICAL BUG: Forgetting to return default changes in unhandled branches!
const badReducer = (state: any, actionAndChanges: any) => {
  if (actionAndChanges.type === 'SPECIAL_CASE') {
    return { ...actionAndChanges.changes, special: true };
  }
  // ⚠️ BUG: Returns undefined! React state becomes undefined and crashes!
};

// ✅ REMEDIATION: Always return actionAndChanges.changes as the fallback!
const goodReducer = (state: any, actionAndChanges: any) => {
  if (actionAndChanges.type === 'SPECIAL_CASE') {
    return { ...actionAndChanges.changes, special: true };
  }
  return actionAndChanges.changes; // Safe default
};
```
Output:
```
TypeError: Cannot read properties of undefined (reading 'step')
```

### Example 5: Idiomatic Modern React 19 Pattern
State Reducer paired with React 19 Actions and Optimistic UI.

```tsx
import React, { useReducer, useOptimistic, startTransition } from 'react';

export const TaskActions = {
  complete: 'COMPLETE',
} as const;

interface Task {
  id: string;
  done: boolean;
}

export function TaskListManager({
  initialTasks,
  stateReducer = (_, { changes }) => changes,
}: {
  initialTasks: Task[];
  stateReducer?: (state: Task[], action: { type: string; id: string; changes: Task[] }) => Task[];
}) {
  const [tasks, dispatch] = useReducer((state: Task[], action: { type: string; id: string }) => {
    const changes = state.map((t) => (t.id === action.id ? { ...t, done: true } : t));
    return stateReducer(state, { ...action, changes });
  }, initialTasks);

  const [optTasks, setOptTasks] = useOptimistic(tasks, (current, id: string) =>
    current.map((t) => (t.id === id ? { ...t, done: true } : t))
  );

  const handleComplete = (id: string) => {
    startTransition(async () => {
      setOptTasks(id);
      await new Promise((r) => setTimeout(r, 600)); // Network delay
      dispatch({ type: TaskActions.complete, id });
    });
  };

  return (
    <ul className="divide-y border rounded max-w-xs">
      {optTasks.map((t) => (
        <li key={t.id} className="p-2 flex justify-between text-sm">
          <span>Task {t.id}</span>
          <button
            onClick={() => handleComplete(t.id)}
            disabled={t.done}
            className="text-xs text-blue-600 disabled:text-zinc-400"
          >
            {t.done ? '✓ Done' : 'Complete'}
          </button>
        </li>
      ))}
    </ul>
  );
}
```
Output:
```
// Instant optimistic checkmark; state reducer can intercept and enforce business logic before commit.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Forgetting to return `changes` as fallback** | Falling off the end of the reducer returns `undefined`, corrupting state. | Always return `actionAndChanges.changes` at end of consumer reducer. |
| **Executing side-effects inside state reducers** | Reducers must be pure functions. Side effects run twice in StrictMode. | Trigger side-effects in event handlers or `useEffect`. |
| **Not exporting Action Types** | Consumers have to hardcode fragile magic strings (`'TOGGLE'`). | Export a typed `ActionTypes` constant alongside the component/hook. |
| **Mutating state in-place** | React uses `Object.is` for state comparison. Mutations cause missed renders. | Always return a new object copy (`{ ...changes }`). |
| **Using State Reducer for simple prop toggles** | Over-engineering when a simple callback (`onSelect`) would suffice. | Reserve for complex multi-branch state machines. |
| **Ignoring the `changes` argument** | Recomputing entire state trees from scratch inside consumer reducers. | Modify `changes` selectively instead of reinventing default logic. |

---

## 5. React Internals: Beneath the Abstraction

The State Reducer pattern is built on function composition:
```
reducer = (state, action) => consumerReducer(state, { ...action, changes: internalReducer(state, action) })
```
1. Inside `ReactFiberHooks.js`, React stores the composed reducer function in the fiber's `memoizedState.queue.lastRenderedReducer`.
2. When `dispatch(action)` is called, React invokes the composed reducer during the render phase.
3. Because the composition happens inside React's native `useReducer`, state transitions participate fully in React batching and Transitions.

---

## 6. Performance, Bundling & React Compiler

- **Bundle Overhead**: Exactly zero bytes of additional libraries required; uses native `useReducer`.
- **Pure Function Memoization**: The React Compiler validates that both the internal and consumer state reducers are pure functions, caching state transitions efficiently.

---

## 7. Accessibility (a11y) & Usability

When consumers override state transitions via `stateReducer`, they must preserve accessibility invariants:
- If a consumer prevents a menu from closing on selection, the trigger's `aria-expanded` must remain `true`.
- Focus must not be dropped into void space when an expected transition is vetoed.

---

## 8. Testing & Verification Patterns

Testing state reducers with Vitest:

```tsx
import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import React from 'react';
import { RateLimitedToggle } from './Toggle';

describe('State Reducer Pattern', () => {
  it('allows state transitions up to the limit then vetos', () => {
    render(<RateLimitedToggle />);
    const btn = screen.getByRole('button', { name: /toggle/i });

    // Click 1: ON
    fireEvent.click(btn);
    expect(screen.getByText(/status: on/i)).toBeInTheDocument();

    // Click 2: OFF
    fireEvent.click(btn);
    expect(screen.getByText(/status: off/i)).toBeInTheDocument();

    // Click 3: ON
    fireEvent.click(btn);
    // Click 4: OFF (Limit hit)
    fireEvent.click(btn);
    expect(screen.getByText(/click limit reached!/i)).toBeInTheDocument();

    // Click 5: VETOED! Should remain OFF
    fireEvent.click(btn);
    expect(screen.getByText(/status: off/i)).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 16.8 / 17** | Introduced and popularized in Downshift by Kent C. Dodds. | Replaced cumbersome prop flags with inverted control. |
| **React 18** | Integrated with automatic batching and concurrent transitions. | Ensured reducers remain strictly pure. |
| **React 19** | Combined with Actions and `useOptimistic` for instantaneous feedback. | Direct integration with React 19 Action dispatchers. |

---

## 10. Security & Hardening

1. **State Injection**: When allowing consumer reducers to alter critical state (e.g. user authentication or billing tiers), validate state schema boundaries to prevent invalid enum injection.
2. **Infinite Loops**: Ensure consumer reducers do not schedule further dispatches synchronously within their own execution.

---

## 11. Real-World Case Study

### Downshift: The Canonical State Reducer Library

PayPal's open-source library **Downshift** is the most celebrated implementation of the State Reducer pattern. Downshift powers comboboxes and autocomplete menus for millions of users:
```tsx
const { isOpen, getToggleButtonProps } = useSelect({
  items: ['Apple', 'Banana', 'Orange'],
  stateReducer: (state, actionAndChanges) => {
    switch (actionAndChanges.type) {
      case useSelect.stateChangeTypes.ItemClick:
        return {
          ...actionAndChanges.changes,
          isOpen: true, // Keep menu open after item click!
        };
      default:
        return actionAndChanges.changes;
    }
  },
});
```
Instead of supporting 50 different boolean configuration options, Downshift exposes `stateReducer`, giving consumers infinite flexibility.

---

## 12. Interview Q&A

<details>
<summary>1. What is the State Reducer pattern in React?</summary>

The State Reducer pattern is an architectural design pattern where a reusable component or hook allows the consumer to intercept, modify, or veto internal state transitions before they are applied. The component computes its default state changes, passes them to a consumer-supplied reducer function, and commits the state returned by that function.
</details>

<details>
<summary>2. How does the State Reducer pattern provide Inversion of Control (IoC)?</summary>

Normally, the component controls when and how its state transitions occur. By accepting a `stateReducer` prop, the component yields authority (inverts control) to the consumer. The consumer can inspect the action that occurred and decide whether to accept the proposed state, modify it, or reject it entirely.
</details>

<details>
<summary>3. Why is the State Reducer pattern superior to adding multiple boolean configuration props?</summary>

Boolean props lead to combinatorial explosion (`closeOnSelect`, `preventCloseOnShift`, `allowDeselect`, etc.), creating bugs where props conflict. The State Reducer pattern handles all present and future edge cases through a single unified API without altering the component's internal code.
</details>

<details>
<summary>4. What are the two parameters passed to a consumer's `stateReducer`?</summary>

1. `currentState`: The state before the action occurred.
2. `actionAndChanges`: An object containing the dispatched `action` (with its `type` and payloads) and the proposed `changes` computed by the component's default reducer.
</details>

<details>
<summary>5. How can a consumer veto a state change?</summary>

By returning `currentState` directly:
```ts
stateReducer: (state, actionAndChanges) => {
  if (shouldBlock) return state; // Veto!
  return actionAndChanges.changes;
}
```
Because `state === state`, React detects no change and cancels the re-render.
</details>

<details>
<summary>6. Why must action types be explicitly exported?</summary>

If action types are not exported, consumers must rely on hardcoded string literals (e.g. `'SELECT'`), which are prone to typos and break during refactoring. Exporting a typed enum or object (`ToggleActions.toggle`) ensures full TypeScript autocomplete and compile-time verification.
</details>

<details>
<summary>7. Can side effects be performed inside a state reducer?</summary>

No. Reducers must be pure mathematical functions. Side effects (API calls, logging, focus management) must be executed in event handlers or `useEffect`.
</details>

<details>
<summary>8. How does the State Reducer pattern interact with React 19's `useActionState`?</summary>

`useActionState` itself follows the reducer signature `(previousState, formData) => nextState`. You can wrap a form action in a state reducer to allow consumers to intercept server-returned validation results before committing them to form UI.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): State Reducer Counter with Cap</summary>

**Specification**: Build a counter hook accepting a `stateReducer` that caps the counter at a maximum of 10.

```tsx
import { useReducer } from 'react';

export function useCounter({
  stateReducer = (_, { changes }) => changes,
}: {
  stateReducer?: (state: number, action: { type: 'INC' | 'DEC'; changes: number }) => number;
} = {}) {
  return useReducer((state: number, action: { type: 'INC' | 'DEC' }) => {
    const changes = action.type === 'INC' ? state + 1 : state - 1;
    return stateReducer(state, { ...action, changes });
  }, 0);
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Disabling Reset via State Reducer</summary>

**Specification**: Prevent a form component's reset action from resetting fields if dirty state is present.

```tsx
import { useReducer } from 'react';

export function useFormState(isDirty: boolean) {
  return useReducer(
    (state: string, action: { type: 'CHANGE' | 'RESET'; val?: string }) => {
      const changes = action.type === 'CHANGE' ? action.val || '' : '';
      if (action.type === 'RESET' && isDirty) {
        return state; // Veto reset
      }
      return changes;
    },
    ''
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Accordion Single-Open Enforcer</summary>

**Specification**: Multi-item accordion that uses a state reducer to restrict opening to only one section at a time.

```tsx
import { useReducer } from 'react';

export function useAccordion() {
  return useReducer(
    (openIds: string[], action: { type: 'TOGGLE'; id: string }) => {
      const isOpen = openIds.includes(action.id);
      // Consumer rule: enforce single open item
      return isOpen ? [] : [action.id];
    },
    []
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Date Range Picker Boundary Guard</summary>

**Specification**: Date range picker state reducer that prevents selecting an end date earlier than the start date.

```tsx
import { useReducer } from 'react';

interface Range {
  start: Date | null;
  end: Date | null;
}

export function useDateRange() {
  return useReducer((state: Range, action: { type: 'SELECT'; date: Date }) => {
    if (!state.start || (state.start && state.end)) {
      return { start: action.date, end: null };
    }
    // Guard: end cannot be before start
    if (action.date < state.start) {
      return { start: action.date, end: null };
    }
    return { ...state, end: action.date };
  }, { start: null, end: null });
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Multi-Level Undo/Redo State Reducer</summary>

**Specification**: Implement a generic higher-order state reducer that decorates any component reducer with undo and redo capabilities.

```tsx
interface UndoState<T> {
  past: T[];
  present: T;
  future: T[];
}

export function withUndoRedo<T, A extends { type: string }>(
  baseReducer: (state: T, action: A) => T
) {
  return (state: UndoState<T>, action: A | { type: 'UNDO' } | { type: 'REDO' }): UndoState<T> => {
    if (action.type === 'UNDO') {
      if (state.past.length === 0) return state;
      const previous = state.past[state.past.length - 1];
      const newPast = state.past.slice(0, state.past.length - 1);
      return {
        past: newPast,
        present: previous,
        future: [state.present, ...state.future],
      };
    }
    if (action.type === 'REDO') {
      if (state.future.length === 0) return state;
      const next = state.future[0];
      const newFuture = state.future.slice(1);
      return {
        past: [...state.past, state.present],
        present: next,
        future: newFuture,
      };
    }
    const newPresent = baseReducer(state.present, action as A);
    if (newPresent === state.present) return state;
    return {
      past: [...state.past, state.present],
      present: newPresent,
      future: [],
    };
  };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Headless Dropdown with Composable Reducer Pipeline</summary>

**Specification**: Build a state reducer engine that chains multiple independent consumer reducer middleware functions sequentially.

```tsx
export function composeReducers<S, A>(
  ...reducers: (((state: S, action: A) => S) | undefined)[]
) {
  return (initialState: S, action: A): S => {
    return reducers.reduce((state, reducer) => {
      return reducer ? reducer(state, action) : state;
    }, initialState);
  };
}
```
</details>
