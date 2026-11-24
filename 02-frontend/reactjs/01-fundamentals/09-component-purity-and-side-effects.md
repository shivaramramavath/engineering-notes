# Component Purity and Side Effects

- **Topic**: `01-fundamentals`
- **React version**: React 19+ (tested on React 19.0.0)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Component purity is the foundational contract required by the React Compiler and Concurrent Mode.

---

## 1. Concept Overview

In React, the core formula of rendering is:
$$\text{UI} = f(\text{State}, \text{Props})$$

A **pure component** is a mathematical function that adheres strictly to two rules:
1. **Same Inputs, Same Outputs**: Given the exact same props, state, and context, the component must always return the exact same JSX output.
2. **Mind Its Own Business (Zero Side Effects During Render)**: The component must **never mutate** any object, variable, or external state that existed before the render call was initiated.

Rendering can happen at any time, in any order, and can even be aborted or restarted by React's Concurrent Scheduler. If a component introduces side effects (e.g. modifying global variables, firing API requests, or altering the DOM) directly inside its render body, your application will suffer from erratic UI glitches, duplicate network traffic, and desynchronized state.

Side effects in React belong strictly in two places:
1. **Event Handlers & Actions**: Triggered by explicit user interactions (clicks, form submits, key presses).
2. **`useEffect`**: Executed after the render and commit phases have completed.

```
Impure Component (Dangerous):
Render Function ────> Modifies Global Array / Triggers Fetch
                     (Causes bugs in StrictMode, Concurrent Mode & Compiler)

Pure Component (Idiomatic React):
Render Function ────> Returns Pure JSX Description
                           │
                           ▼
              DOM Committed to Screen
                           │
                           ▼
          Side Effects Run Safely in useEffect / Event Handlers
```

---

## 2. Key Mechanics & Architecture

### Permitted vs Forbidden Actions During Render

| Action | Inside Render Body | Inside Event Handler / Action | Inside `useEffect` |
| :--- | :---: | :---: | :---: |
| Calculating derived data from props | **Allowed (Pure)** | Allowed | Anti-pattern (causes extra render) |
| Mutating an object created *inside* that same render | **Allowed (Local mutation)** | Allowed | Allowed |
| Mutating props or parent state | **FORBIDDEN (Crash / Glitch)** | Allowed (via `setState`) | Allowed (via `setState`) |
| Modifying global / module-level variables | **FORBIDDEN** | Allowed | Allowed |
| Firing an HTTP network request | **FORBIDDEN** | **Allowed (Idiomatic)** | Allowed (for subscriptions/sync) |
| Modifying the browser DOM directly (`document.title = ...`) | **FORBIDDEN** | Allowed | **Allowed (Idiomatic)** |
| Reading current time (`Date.now()`) or `Math.random()` | **FORBIDDEN (Impure)** | Allowed | Allowed |

### Local Mutation is Pure!
Mutating an object created *locally within the current render execution* is 100% pure because no other component or previous render can observe that mutation:
```tsx
// This is completely PURE!
function CupList({ count }: { count: number }) {
  const cups = []; // Created freshly in this render
  for (let i = 1; i <= count; i++) {
    cups.push(<span key={i}>Cup #{i} </span>); // Local mutation is fine!
  }
  return <div>{cups}</div>;
}
```

---

## 3. Real-World Code Examples

### Example 1: Impure Render Bug vs Pure Fix
```tsx
// BAD: Mutates external variable; StrictMode double-rendering renders 2, 4, 6...
let guestCount = 0;

export function BadCup() {
  guestCount = guestCount + 1; // IMPURE MUTATION!
  return <h2>Tea cup for guest #{guestCount}</h2>;
}

// GOOD: Passes data down as pure props; outputs 1, 2, 3 reliably!
export function GoodCup({ guestNumber }: { guestNumber: number }) {
  return <h2>Tea cup for guest #{guestNumber}</h2>;
}
```
Output in StrictMode Development:
```
// BadCup renders: Guest #2, Guest #4
// GoodCup renders: Guest #1, Guest #2 (Deterministic!)
```

### Example 2: In-Place Prop Mutation Bug
```tsx
interface ProfileProps {
  user: { name: string; roles: string[] };
}

// BAD: Mutating props directly corrupts parent state and memoization!
export function BadProfile({ user }: ProfileProps) {
  user.roles.push('viewer'); // MUTATION BUG!
  return <div>Roles: {user.roles.join(', ')}</div>;
}

// GOOD: Immutably derive new data
export function GoodProfile({ user }: ProfileProps) {
  const allRoles = [...user.roles, 'viewer']; // Pure copy!
  return <div>Roles: {allRoles.join(', ')}</div>;
}
```
Output:
```
// BadProfile permanently mutates the parent's user object on every re-render!
// GoodProfile leaves parent object untouched.
```

### Example 3: Pure Derived State vs Redundant Effects
```tsx
import { useState } from 'react';

// ANTI-PATTERN: Using useEffect to sync derived state (triggers 2 renders!)
export function SlowFilter({ items }: { items: string[] }) {
  const [query, setQuery] = useState('');
  const [filtered, setFiltered] = useState<string[]>([]);

  // BAD: Redundant effect causes extra render pass!
  // useEffect(() => {
  //   setFiltered(items.filter(i => i.includes(query)));
  // }, [items, query]);

  // IDIOMATIC: Calculate pure derived state directly during render (1 render!)
  const filteredItems = items.filter((i) => i.toLowerCase().includes(query.toLowerCase()));

  return (
    <div>
      <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="Search..." />
      <ul>
        {filteredItems.map((item) => (
          <li key={item}>{item}</li>
        ))}
      </ul>
    </div>
  );
}
```
Output:
```
// Renders filtered results in a single render pass without unnecessary effect cascades
```

### Example 4: Deep Copying with Structured Clone
```tsx
export function cloneStateTree<T>(state: T): T {
  // Fast, native deep clone ensuring pure state transformations
  return structuredClone(state);
}
```

### Example 5: Pure Event-Driven Side Effect
```tsx
import { useState } from 'react';

export function CartSummary({ price }: { price: number }) {
  const [discountApplied, setDiscountApplied] = useState(false);

  // Pure derived calculation
  const finalPrice = discountApplied ? price * 0.9 : price;

  // Side effects belong in the event handler!
  const handleApplyDiscount = () => {
    setDiscountApplied(true);
    console.log(`Discount applied. New total: $${price * 0.9}`);
  };

  return (
    <div>
      <p>Total: ${finalPrice.toFixed(2)}</p>
      <button onClick={handleApplyDiscount} className="border px-2 py-1">
        Apply 10% Coupon
      </button>
    </div>
  );
}
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Writing `Math.random()` or `Date.now()` inside render | Returns different values on each render pass, breaking hydration and reconciliation | Pass generated IDs/timestamps as props, or use `useId()` |
| Mutating an existing state array: `items.sort()` | `Array.prototype.sort()` mutates the array in-place, bypassing React change detection | Use `[...items].sort()` or `items.toSorted()` |
| Calling `fetch()` or `axios()` directly in the component body | Triggers an endless network loop on every render pass | Fetch inside `useEffect`, React 19 Actions, or TanStack Query |
| Writing to `localStorage` or `document.title` inside render | Side effects inside render execute prematurely before commit | Place DOM and storage side effects in `useEffect` |
| Calling `setState` unconditionally in render: `if (x) setX(y)` | Triggers immediate re-render, resulting in `Too many re-renders` crash | Move state updates into event handlers or effects |
| Using global caches without subscription synchronization | Causes tearing and stale UI in Concurrent React | Use `useSyncExternalStore` |

---

## 5. React Internals: Beneath the Abstraction

### Why the React Compiler Requires Pure Components
The React Compiler operates by performing static analysis on your code to determine when values, JSX elements, and function callbacks can be safely memoized.
- If a component mutates an external variable during render, the compiler's memoization boundary will cache the return value, **preventing the mutation from running on subsequent renders**.
- This desynchronizes the application's internal state. Adhering strictly to component purity guarantees that build-time compiler optimizations produce 100% correct code.

---

## 6. Performance, Bundling & React Compiler

- **Zero Memoization Overhead**: When components are pure, the React Compiler automatically memoizes heavy computations without requiring developers to write boilerplate `useMemo` hooks.

---

## 7. Accessibility (a11y) & Usability

- **Deterministic Screen Reader Text**: Impure components that return varying labels on re-renders cause screen readers to announce fluctuating, confusing text to visually impaired users. Pure components guarantee stable accessibility announcements.

---

## 8. Testing & Verification Patterns

```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { GoodCup } from './GoodCup';

describe('Component Purity Verification', () => {
  it('renders deterministically with identical props', () => {
    const { rerender } = render(<GoodCup guestNumber={1} />);
    expect(screen.getByText('Tea cup for guest #1')).toBeInTheDocument();

    // Rerender with same props must yield exact same output
    rerender(<GoodCup guestNumber={1} />);
    expect(screen.getByText('Tea cup for guest #1')).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration

| React Version | Purity Enforcement |
| :--- | :--- |
| **React 16** | StrictMode introduced double-calling component renders |
| **React 18** | Concurrent rendering interruptions introduced; purity became mandatory to prevent race conditions |
| **React 19** | React Compiler enforces purity at build time; Action transitions isolate mutation lifecycles |

---

## 10. Security & Hardening

- **Avoiding State Leaks Between Requests in SSR**: In Server-Side Rendering (SSR), impure module-level variables (e.g. `const currentSession = {}`) are shared across all incoming user requests, creating critical security vulnerabilities where User A sees User B's session data! Keep state strictly inside component scopes.

---

## 11. Real-World Case Study

### Fixing a Multi-Tenant Data Leak in Server-Rendered React
A fintech dashboard rendered customer balances. A developer created a module-level variable `let cachedBalance = 0;` to store values during render.
- Under high concurrent traffic in SSR, Request A wrote Account A's balance to the module-level variable right before Request B finished rendering.
- Request B sent Account A's balance to User B's browser!
- **Fix**: The team audited the codebase for render impurities, moved balance storage strictly into component state, and added ESLint rules to disallow module-level variable reassignments.

---

## 12. Interview Q&A

<details>
<summary>1. What makes a React component function "pure"?</summary>
A React component is pure if:
1. It is deterministic: Given the same inputs (props, state, context), it always returns the same output JSX.
2. It produces zero side effects during render: It does not modify variables, arrays, DOM nodes, or state that existed before the component was invoked.
</details>

<details>
<summary>2. Is mutating a variable created inside the render function considered impure?</summary>
No. Local mutation is completely pure. For example, creating an empty array <code>const items = []</code> inside the component body and pushing items into it using a for-loop does not affect any external state outside that single render execution.
</details>

<details>
<summary>3. Why is calling Math.random() or Date.now() inside a component render an anti-pattern?</summary>
Both functions return different values every time they are called. This violates the deterministic invariant of React components, causes hydration mismatch errors between server-rendered HTML and client hydration, and confuses the React Compiler's memoization caches.
</details>

<details>
<summary>4. What happens when an impure component runs in Concurrent React?</summary>
In Concurrent React, React can pause rendering a component, abandon an in-progress render to handle an urgent click event, and restart the render from scratch later. If the component performed side effects during render (like pushing to a list or calling an API), those side effects will execute multiple times, corrupting data.
</details>

<details>
<summary>5. Where should side effects live in a React application?</summary>
Side effects belong in:
1. Event handlers (e.g. <code>onClick</code>, <code>onChange</code>) and React 19 Actions.
2. <code>useEffect</code> (for synchronizing with external systems after commit).
They must never live directly in the component render body.
</details>

<details>
<summary>6. Why shouldn't you use useEffect to calculate derived state from props?</summary>
Using <code>useEffect</code> to update state based on props causes an unnecessary second render pass: React renders with stale state, commits to the DOM, fires the effect, updates state, and renders a second time. Calculating derived state directly in the render body is synchronous, pure, and completes in a single render pass.
</details>

<details>
<summary>7. How does Array.prototype.toSorted() help maintain purity?</summary>
Introduced in ECMAScript 2023, <code>toSorted()</code> returns a brand new sorted array copy without mutating the original array, whereas <code>sort()</code> mutates the source array in-place.
</details>

<details>
<summary>8. How does React StrictMode detect impure components?</summary>
StrictMode intentionally executes component render functions twice in development. If a component contains an impure mutation (such as <code>counter++</code>), the displayed value will diverge between the first and second render, alerting the developer immediately.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Refactoring Array Mutation</summary>

**Specification**: Refactor an impure component using `items.reverse()` to use pure immutable array copying.

```tsx
// PURE SOLUTION:
export function ReversedList({ items }: { items: string[] }) {
  // Use toReversed() or [...items].reverse()
  const reversed = [...items].reverse();

  return (
    <ul>
      {reversed.map((item) => (
        <li key={item}>{item}</li>
      ))}
    </ul>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Deterministic ID Generation</summary>

**Specification**: Replace impure `Math.random()` ID generation in a form input with React's native `useId()` hook.

```tsx
import { useId } from 'react';

export function FormField({ label }: { label: string }) {
  const id = useId();

  return (
    <div>
      <label htmlFor={id}>{label}</label>
      <input id={id} type="text" className="border ml-2" />
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Pure Derived Cart Calculations</summary>

**Specification**: Build a shopping cart summary component that calculates subtotal, tax, and total directly in render without `useState` or `useEffect`.

```tsx
interface CartItem {
  id: string;
  price: number;
  quantity: number;
}

export function CartCalculator({ items }: { items: CartItem[] }) {
  // Pure synchronous derived calculations
  const subtotal = items.reduce((acc, item) => acc + item.price * item.quantity, 0);
  const tax = subtotal * 0.08;
  const total = subtotal + tax;

  return (
    <div className="border p-4 rounded">
      <p>Subtotal: ${subtotal.toFixed(2)}</p>
      <p>Tax (8%): ${tax.toFixed(2)}</p>
      <p className="font-bold">Total: ${total.toFixed(2)}</p>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Isolating Document Title Side Effect</summary>

**Specification**: Refactor an impure component that writes `document.title = ...` directly in render into a clean `useEffect` hook.

```tsx
import { useEffect } from 'react';

export function PageHeading({ title }: { title: string }) {
  useEffect(() => {
    document.title = `${title} — Portal`;
  }, [title]);

  return <h1 className="text-2xl font-bold">{title}</h1>;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Pure Infinite Tree Node Renderer</summary>

**Specification**: Implement a recursive tree component that cleanly renders nested folders without mutating the input tree structure.

```tsx
interface TreeNode {
  id: string;
  name: string;
  children?: TreeNode[];
}

export function FolderTree({ node }: { node: TreeNode }) {
  return (
    <div className="pl-4 border-l">
      <span>📁 {node.name}</span>
      {node.children && node.children.length > 0 && (
        <div className="space-y-1 mt-1">
          {node.children.map((child) => (
            <FolderTree key={child.id} node={child} />
          ))}
        </div>
      )}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Component Purity Lint Rule Simulation</summary>

**Specification**: Write a test utility that detects if a component mutates its received props object during execution.

```tsx
export function assertPropsImmutable<P extends object>(Component: React.ComponentType<P>, props: P) {
  const initialSnapshot = JSON.stringify(props);
  // Execute component
  Component(props);
  const afterSnapshot = JSON.stringify(props);

  if (initialSnapshot !== afterSnapshot) {
    throw new Error('Component impurity detected: Component mutated its props!');
  }
}
```
</details>
