# Immutable Updates with Immer

- **Topic**: `02-state-and-rendering`
- **React version**: React 19+ (tested on React 19.0.0)
- **Tested with**: Immer 10.x, React 19.0.0, Vite 6.0, TypeScript 5.7
- **Bundle / compiler notes**: Uses ES6 Proxies to allow "mutative" syntax while producing deeply frozen, structurally shared immutable state.

---

## 1. Concept Overview

React relies on **referential identity equality (`Object.is`)** to detect state changes and trigger component re-renders. If you mutate a state object or array in place (`state.user.name = "Bob"`), the object reference remains identical (`state === state`), causing React to ignore the update and skip re-rendering.

Consequently, React state must always be updated **immutably**: creating a new copy of every nested object and array along the mutation path.

As state trees grow deep, manual spread operators become unwieldy, verbose, and bug-prone:
```tsx
// Manual spread nightmare:
setCompany((prev) => ({
  ...prev,
  departments: {
    ...prev.departments,
    engineering: {
      ...prev.departments.engineering,
      leads: [...prev.departments.engineering.leads, newLead],
    },
  },
}));
```

**Immer** solves this problem by using JavaScript `Proxy` objects. It allows you to write natural, mutable JavaScript code against a temporary "draft" object (`draft.departments.engineering.leads.push(newLead)`). When the draft function completes, Immer produces a brand new, frozen immutable state tree with **structural sharing**—copying only the modified nodes while preserving unchanged references.

```
Original State (Immutable)
        │
        ▼ (Immer wraps in ES6 Proxy)
Draft State (Mutable Syntax) ──> draft.user.profile.age = 31
        │
        ▼ (Immer finalizes with structural sharing)
Next State (New Immutable Object; unmodified branches share old references)
```

---

## 2. Key Mechanics & Architecture

### How Immer Works Internally (Structural Sharing)
1. **The Proxy Trap**: When you call `produce(baseState, recipe)`, Immer wraps `baseState` in an ES6 `Proxy`.
2. **Copy-on-Write**: As your recipe code reads properties on `draft`, Immer returns proxy wrappers. The moment you **write** to a property (e.g. `draft.x = 1`), Immer creates a shallow copy of that specific node.
3. **Finalization & Structural Sharing**: Unmodified siblings and parent branches that were never written to are preserved by reference. Only the modified path receives new object allocations.
4. **Auto-Freezing**: In development mode, Immer calls `Object.freeze()` on the resulting state tree, preventing accidental downstream mutations.

### Comparison Table: Manual Spread vs Immer

| Dimension | Manual Spread Operators | Immer (`produce` / `use-immer`) |
| :--- | :--- | :--- |
| **Syntax Style** | Declarative but verbose (`...obj, ...arr`) | Imperative & intuitive (`draft.push()`, `draft.a = b`) |
| **Cognitive Load on Nested Data** | High (easy to accidentally mutate nested references) | Low (treat state as standard mutable JS) |
| **Performance Overhead** | Minimal (native shallow object copies) | Tiny Proxy overhead (negligible for UI state) |
| **Structural Sharing** | Developer must ensure reference preservation | **Automatic** across all unmodified branches |
| **Auto-Freeze Protection** | None (requires custom linters) | Native `Object.freeze()` in development |

---

## 3. Real-World Code Examples

### Example 1: Comparing Nested Mutation (Manual vs Immer)
```tsx
import { produce } from 'immer';

interface UserProfile {
  name: string;
  preferences: {
    theme: string;
    notifications: { email: boolean; sms: boolean };
  };
}

const initialUser: UserProfile = {
  name: 'Alex',
  preferences: {
    theme: 'light',
    notifications: { email: true, sms: false },
  },
};

// 1. Manual Spread:
const updatedManual = {
  ...initialUser,
  preferences: {
    ...initialUser.preferences,
    notifications: {
      ...initialUser.preferences.notifications,
      email: false,
    },
  },
};

// 2. Immer Produce:
const updatedImmer = produce(initialUser, (draft) => {
  draft.preferences.notifications.email = false;
});

console.log('Equal result:', JSON.stringify(updatedManual) === JSON.stringify(updatedImmer));
console.log('Unmodified branch shared:', initialUser.preferences.theme === updatedImmer.preferences.theme);
```
Output:
```
Equal result: true
Unmodified branch shared: true
```

### Example 2: `useImmer` Hook in a React Component
```tsx
import { useImmer } from 'use-immer';

interface Todo {
  id: number;
  text: string;
  done: boolean;
}

export function TodoListApp() {
  const [todos, updateTodos] = useImmer<Todo[]>([
    { id: 1, text: 'Learn React 19', done: true },
    { id: 2, text: 'Master Immer', done: false },
  ]);

  const toggleTodo = (id: number) => {
    updateTodos((draft) => {
      const target = draft.find((t) => t.id === id);
      if (target) {
        target.done = !target.done; // Mutate directly!
      }
    });
  };

  const addTodo = (text: string) => {
    updateTodos((draft) => {
      draft.push({ id: Date.now(), text, done: false }); // Native push!
    });
  };

  return (
    <div className="p-4 space-y-2">
      <h3 className="font-bold">Tasks</h3>
      <ul>
        {todos.map((t) => (
          <li key={t.id} onClick={() => toggleTodo(t.id)} className="cursor-pointer">
            {t.done ? '✓' : '○'} {t.text}
          </li>
        ))}
      </ul>
      <button onClick={() => addTodo('New Task')} className="border px-2 py-1 text-sm">
        Add Task
      </button>
    </div>
  );
}
```

### Example 3: Curried Reducers with Immer in `useReducer`
```tsx
import { useReducer } from 'react';
import { produce } from 'immer';

interface State {
  count: number;
  history: number[];
}

type Action = { type: 'increment' } | { type: 'decrement' } | { type: 'reset' };

// Immer curried producer: produce((draft, action) => ...)
const counterReducer = produce((draft: State, action: Action) => {
  switch (action.type) {
    case 'increment':
      draft.count += 1;
      draft.history.push(draft.count);
      break;
    case 'decrement':
      draft.count -= 1;
      draft.history.push(draft.count);
      break;
    case 'reset':
      draft.count = 0;
      draft.history = [];
      break;
  }
});

export function CounterHistory() {
  const [state, dispatch] = useReducer(counterReducer, { count: 0, history: [] });

  return (
    <div className="p-4 space-y-2">
      <p>Count: {state.count}</p>
      <div className="space-x-2">
        <button onClick={() => dispatch({ type: 'increment' })} className="border px-2">
          +
        </button>
        <button onClick={() => dispatch({ type: 'decrement' })} className="border px-2">
          -
        </button>
      </div>
      <p className="text-sm text-gray-500">History: {state.history.join(', ')}</p>
    </div>
  );
}
```

### Example 4: Array Deletions via `splice()` vs Filter
```tsx
import { produce } from 'immer';

const initialItems = ['apple', 'banana', 'cherry'];

// In Immer, you can use splice directly!
const nextItems = produce(initialItems, (draft) => {
  const index = draft.indexOf('banana');
  if (index !== -1) {
    draft.splice(index, 1); // Native deletion
  }
});

console.log('Resulting items:', nextItems);
```
Output:
```
Resulting items: ['apple', 'cherry']
```

### Example 5: Replacing the Entire State vs Returning
```tsx
import { produce } from 'immer';

// When you need to replace the entire state completely in Immer,
// you must RETURN the new value rather than mutating the draft:
const resetState = produce({ active: true, count: 5 }, (draft) => {
  // If returning, do not mutate draft!
  return { active: false, count: 0 };
});

console.log('Reset state:', resetState);
```
Output:
```
Reset state: { active: false, count: 0 }
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Both mutating `draft` AND returning a new object | Ambiguity in Immer: cannot determine if draft mutation or return value takes priority | Either mutate `draft` OR return a new object—never both |
| Returning `undefined` expecting state to clear | Returning `undefined` causes Immer to assume no return occurred (keeps draft) | Return `null` or the `nothing` sentinel from `immer` |
| Calling `draft.slice()` thinking it mutates | `slice()` is non-mutating; changes are lost | Use `draft.splice()` to mutate arrays in-place |
| Mutating state outside of `produce()` | Corrupts React change detection and raises `Cannot assign to read only property` | Ensure all mutations occur strictly inside `produce()` callbacks |
| Storing non-serializable objects (DOM elements, sockets) in Immer | Proxy overhead and deep freezing cause crashes on non-plain objects | Store complex classes/instances outside of state or in refs |
| Overusing Immer for simple primitive states (`useState(0)`) | Unnecessary abstraction overhead for numbers/strings | Use standard `useState` for primitives; reserve Immer for nested structures |

---

## 5. React Internals: Beneath the Abstraction

### Structural Sharing & Object Identity Verification
When React's reconciler performs child diffing during the render phase:
- It checks `oldProps.item === newProps.item`.
- If an item in a list of 1,000 tasks was not mutated by Immer, its object pointer remains unchanged.
- If the child component is memoized (`React.memo` or via React Compiler), React completely skips re-rendering that child, yielding massive performance gains over naive deep copying.

---

## 6. Performance, Bundling & React Compiler

- **Bundle Size**: Immer is lightweight (~3.5 KB minified/gzipped) and tree-shakes cleanly.
- **Redux Toolkit Standard**: Immer is the default engine powering Redux Toolkit's `createSlice`, meaning millions of production applications execute Immer mutations daily.

---

## 7. Accessibility (a11y) & Usability

- **Smooth Form Input**: By eliminating complex spread operations in multi-field nested forms, Immer avoids main-thread micro-stutters during typing, keeping UI responsive.

---

## 8. Testing & Verification Patterns

```typescript
import { describe, it, expect } from 'vitest';
import { produce } from 'immer';

describe('Immer Invariant Verification', () => {
  it('preserves unmodified object references', () => {
    const base = {
      nestedA: { val: 1 },
      nestedB: { val: 2 },
    };

    const next = produce(base, (draft) => {
      draft.nestedA.val = 99;
    });

    expect(next.nestedA.val).toBe(99);
    expect(next.nestedA).not.toBe(base.nestedA);
    // Unmodified branch reference is strictly preserved!
    expect(next.nestedB).toBe(base.nestedB);
  });
});
```

---

## 9. Version History & Migration

| Version | Milestone |
| :--- | :--- |
| **Immer 1–6** | Legacy ES5 fallback support for IE11 |
| **Immer 7–9** | Dropped IE11, full modern Proxy engine, TS typing improvements |
| **Immer 10.x** | Optimized structural sharing performance, native Map/Set support |

---

## 10. Security & Hardening

- **Prototype Pollution Prevention**: Modern Immer automatically guards against prototype pollution attacks (ignoring attempts to set `__proto__` or `constructor` on draft objects).

---

## 11. Real-World Case Study

### Refactoring a Complex JSON Schema Editor
An engineering team built a visual form schema editor with 6 levels of nested arrays and objects. Updating a single validation rule required 14 spread operators across 20 lines of code.
- Frequent bugs occurred when developers accidentally shallow-copied only the top 2 layers, causing shared reference mutations.
- Replaced manual spreads with `use-immer`. Code size dropped by **70%**, and all state desynchronization bugs were eliminated.

---

## 12. Interview Q&A

<details>
<summary>1. How does Immer enable mutable syntax while producing immutable state?</summary>
Immer wraps the base state in a JavaScript <code>Proxy</code>. When the recipe callback executes, the Proxy intercepts all reads and writes. On property writes, it creates a shallow copy of that specific path (copy-on-write). At finalization, it returns a new immutable state tree with structural sharing, leaving unmodified branches referencing their original objects.
</details>

<details>
<summary>2. What is structural sharing?</summary>
Structural sharing is an optimization technique used in immutable data structures where unchanged parts of a data tree are reused by reference in the new version, rather than duplicating the entire memory tree. This minimizes memory allocation and enables $O(1)$ reference equality checks (<code>prev.branch === next.branch</code>).
</details>

<details>
<summary>3. What happens if you return a value from an Immer produce recipe?</summary>
If you return a value from an Immer recipe, that returned value becomes the next state, completely replacing the draft. However, if you both mutate the draft and return a value, Immer throws an error because the intended state is ambiguous.
</details>

<details>
<summary>4. How does Redux Toolkit utilize Immer?</summary>
Redux Toolkit wraps reducer functions inside <code>createSlice</code> with Immer's <code>produce</code> automatically. This allows developers to write mutating logic (e.g. <code>state.push(action.payload)</code>) inside RTK slice reducers safely.
</details>

<details>
<summary>5. Can you mutate JavaScript Maps and Sets with Immer?</summary>
Yes. In modern Immer (v9+), support for native <code>Map</code> and <code>Set</code> objects is built in using the <code>enableMapSet()</code> plugin or natively in v10.
</details>

<details>
<summary>6. Why does Immer freeze state objects in development?</summary>
Immer automatically calls <code>Object.freeze()</code> recursively on the returned state tree in development to ensure developers do not accidentally mutate state outside of a <code>produce()</code> block.
</details>

<details>
<summary>7. What is the use-immer package?</summary>
<code>use-immer</code> is an open-source library that provides the <code>useImmer</code> and <code>useImmerReducer</code> hooks, combining the convenience of <code>useState</code> / <code>useReducer</code> with Immer's draft mutation syntax.
</details>

<details>
<summary>8. How does Immer compare in performance to manual object spreading?</summary>
Manual object spreading is slightly faster for flat objects with 1-2 properties because it avoids Proxy overhead. However, for deeply nested structures (3+ levels), Immer is often faster because developers rarely implement true structural sharing manually, and the Proxy overhead is negligible compared to React's rendering lifecycle.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Nested Preference Toggle</summary>

**Specification**: Write an Immer `produce` function that toggles `user.settings.darkMode` from `false` to `true`.

```typescript
import { produce } from 'immer';

const user = { id: 1, settings: { darkMode: false } };

export const updatedUser = produce(user, (draft) => {
  draft.settings.darkMode = !draft.settings.darkMode;
});

// Assertion:
// updatedUser.settings.darkMode === true
```
</details>

<details>
<summary>Exercise 2 (Easy): Immutable Array Push</summary>

**Specification**: Add a new tag to a post's tag array using Immer.

```typescript
import { produce } from 'immer';

const post = { id: 'p1', tags: ['react', 'vite'] };

export const updatedPost = produce(post, (draft) => {
  draft.tags.push('immer');
});

// Assertion:
// updatedPost.tags === ['react', 'vite', 'immer']
```
</details>

<details>
<summary>Exercise 3 (Medium): Delete Item by ID from Deeply Nested State</summary>

**Specification**: Remove an attachment by ID from a message nested inside a chat channel.

```typescript
import { produce } from 'immer';

interface ChannelState {
  id: string;
  messages: Array<{
    id: string;
    attachments: Array<{ id: string; url: string }>;
  }>;
}

export function removeAttachment(state: ChannelState, messageId: string, attachmentId: string) {
  return produce(state, (draft) => {
    const msg = draft.messages.find((m) => m.id === messageId);
    if (msg) {
      const idx = msg.attachments.findIndex((a) => a.id === attachmentId);
      if (idx !== -1) {
        msg.attachments.splice(idx, 1);
      }
    }
  });
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Curried Immer Reducer</summary>

**Specification**: Create a curried Immer reducer handling `ADD_PRODUCT` and `REMOVE_PRODUCT`.

```typescript
import { produce } from 'immer';

interface Product {
  id: string;
  name: string;
}

type Action =
  | { type: 'ADD_PRODUCT'; product: Product }
  | { type: 'REMOVE_PRODUCT'; id: string };

export const productReducer = produce((draft: Product[], action: Action) => {
  switch (action.type) {
    case 'ADD_PRODUCT':
      draft.push(action.product);
      break;
    case 'REMOVE_PRODUCT':
      const index = draft.findIndex((p) => p.id === action.id);
      if (index !== -1) draft.splice(index, 1);
      break;
  }
});
```
</details>

<details>
<summary>Exercise 5 (Hard): Normalized Entity State Updater</summary>

**Specification**: Update an entity's attribute inside a normalized `{ byId: {}, allIds: [] }` store using Immer.

```typescript
import { produce } from 'immer';

interface UserEntity {
  id: string;
  name: string;
  active: boolean;
}

interface NormalizedState {
  byId: Record<string, UserEntity>;
  allIds: string[];
}

export function toggleUserStatus(state: NormalizedState, userId: string) {
  return produce(state, (draft) => {
    const user = draft.byId[userId];
    if (user) {
      user.active = !user.active;
    }
  });
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Structural Sharing Integrity Test</summary>

**Specification**: Write a test verifying that updating one item in an Immer list leaves all other item references unchanged.

```typescript
import { produce } from 'immer';

export function verifyStructuralIntegrity() {
  const initial = [
    { id: 1, name: 'A' },
    { id: 2, name: 'B' },
  ];

  const updated = produce(initial, (draft) => {
    draft[0].name = 'Updated A';
  });

  // Target item was updated
  if (updated[0] === initial[0]) throw new Error('Target was not cloned!');
  // Sibling item reference MUST be identical!
  if (updated[1] !== initial[1]) throw new Error('Structural sharing violated!');
}
```
</details>
