# State Colocation and Derived State

- **Topic**: `02-state-and-rendering`
- **React version**: React 19+ (tested on React 19.0.0)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Eliminating redundant state variables dramatically reduces reconciliation complexity and maximizes React Compiler efficiency.

---

## 1. Concept Overview

Two of the most common causes of bloat, desynchronization, and performance degradation in React applications are:
1. **Lifting State Too High**: Placing local state in global stores (Redux, Zustand) or root components when only a leaf component needs it.
2. **Redundant State (State Duplication)**: Storing values in `useState` that could easily be **calculated on the fly during render** (derived state).

**State Colocation** is the principle of keeping state as close as possible to the components that consume and mutate it. If only a single form input or modal dialog needs a state value, that state should reside strictly within that component.

**Derived State** refers to values that can be computed synchronously from existing props or state. Storing derived state in `useState` and trying to synchronize it via `useEffect` introduces:
- Two render cycles instead of one.
- Impossible intermediate states (stale UI).
- Subtle synchronization bugs when props change.

```
Anti-Pattern: Storing Derived State in useState + useEffect
Props (items, filter) ──> useState(filteredItems) ──> useEffect(sync) ──> Second Render!

Idiomatic React: Pure Derived State in Render
Props (items, filter) ──> const filteredItems = items.filter(...) ──> Single Pure Render!
```

---

## 2. Key Mechanics & Architecture

### State Colocation Decision Framework

```
Does multiple sibling components need this state?
            │
            ├── NO  ──> Colocate state locally inside the single consuming component!
            │
            └── YES ──> Can it be lifted to their immediate common parent?
                          │
                          ├── YES ──> Lift state to common parent and pass down via props.
                          │
                          └── NO  ──> (Deeply nested or global?) Use Context or Zustand store.
```

### The "Can It Be Computed?" Checklist
Before typing `const [x, setX] = useState(...)`, ask:
1. Does this value change over time? If not, it's not state—it's a plain constant.
2. Can it be computed from existing props or state? If yes, **do not store it in state**. Calculate it directly in the component body.
3. Is it passed down from a parent via props? If yes, it's not state for this component.

---

## 3. Real-World Code Examples

### Example 1: The Redundant FullName Bug vs Pure Derived State
```tsx
import { useState } from 'react';

// BAD: Storing fullName in state and trying to sync with useEffect!
// export function BadUserForm() {
//   const [first, setFirst] = useState('');
//   const [last, setLast] = useState('');
//   const [full, setFull] = useState(''); // REDUNDANT!
//   useEffect(() => { setFull(`${first} ${last}`); }, [first, last]);
// }

// GOOD: Compute fullName synchronously during render!
export function GoodUserForm() {
  const [firstName, setFirstName] = useState('');
  const [lastName, setLastName] = useState('');

  // Pure derived state: 0 extra renders, 0 desync bugs!
  const fullName = `${firstName} ${lastName}`.trim();

  return (
    <div className="space-y-2 max-w-sm">
      <input
        placeholder="First name"
        value={firstName}
        onChange={(e) => setFirstName(e.target.value)}
        className="border p-1 w-full"
      />
      <input
        placeholder="Last name"
        value={lastName}
        onChange={(e) => setLastName(e.target.value)}
        className="border p-1 w-full"
      />
      <p className="font-semibold">Full Name: {fullName || '(None)'}</p>
    </div>
  );
}
```
Output:
```
// Updates instantly on every keystroke without lag or double renders
```

### Example 2: Colocating State to Eliminate Unnecessary Parent Re-renders
```tsx
import { useState } from 'react';

// Heavy parent component
export function ExpensiveDashboard() {
  return (
    <div className="p-6 space-y-4">
      <h1 className="text-xl font-bold">Analytics Engine</h1>
      {/* Colocated modal button: Opening modal does NOT re-render ExpensiveDashboard! */}
      <FeedbackModalButton />
      <HeavyChart />
    </div>
  );
}

// State is colocated strictly within this leaf button!
function FeedbackModalButton() {
  const [isOpen, setIsOpen] = useState(false);

  return (
    <>
      <button onClick={() => setIsOpen(true)} className="bg-blue-600 text-white px-3 py-1 rounded">
        Give Feedback
      </button>
      {isOpen && (
        <div className="fixed inset-0 bg-black/50 flex items-center justify-center">
          <div className="bg-white p-4 rounded shadow">
            <h3>Feedback Form</h3>
            <button onClick={() => setIsOpen(false)} className="mt-2 text-sm text-red-500">
              Close
            </button>
          </div>
        </div>
      )}
    </>
  );
}

function HeavyChart() {
  console.log('HeavyChart rendered');
  return <div className="h-40 bg-gray-100 flex items-center justify-center">Complex Chart</div>;
}
```
Output:
```
// Clicking 'Give Feedback' opens modal without triggering a re-render of HeavyChart!
```

### Example 3: Filtered Lists and Active Item Selection
```tsx
import { useState } from 'react';

interface Task {
  id: string;
  title: string;
  completed: boolean;
}

export function TaskManager({ tasks }: { tasks: Task[] }) {
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [showCompletedOnly, setShowCompletedOnly] = useState(false);

  // Derived filtered collection
  const visibleTasks = showCompletedOnly ? tasks.filter((t) => t.completed) : tasks;

  // Derived selected object (never store the whole object in state!)
  const selectedTask = tasks.find((t) => t.id === selectedId);

  return (
    <div className="p-4 space-y-2">
      <label className="flex items-center gap-2">
        <input
          type="checkbox"
          checked={showCompletedOnly}
          onChange={(e) => setShowCompletedOnly(e.target.checked)}
        />
        <span>Completed Only</span>
      </label>

      <ul>
        {visibleTasks.map((t) => (
          <li
            key={t.id}
            onClick={() => setSelectedId(t.id)}
            className={`cursor-pointer p-1 ${selectedId === t.id ? 'bg-blue-100 font-bold' : ''}`}
          >
            {t.title}
          </li>
        ))}
      </ul>

      {selectedTask && <div className="mt-4 p-2 border">Selected: {selectedTask.title}</div>}
    </div>
  );
}
```

### Example 4: Expensive Derived Calculation with useMemo
```tsx
import { useState, useMemo } from 'react';

export function HeavyStatsViewer({ data }: { data: number[] }) {
  const [multiplier, setMultiplier] = useState(1);

  // When calculating derived state is computationally heavy (O(n log n) or O(n^2)),
  // memoize it so unrelated state changes (like multiplier) don't re-run it needlessly.
  const sortedStats = useMemo(() => {
    console.log('Sorting 100,000 items...');
    return [...data].sort((a, b) => a - b);
  }, [data]);

  return (
    <div>
      <p>Items Count: {sortedStats.length}</p>
      <button onClick={() => setMultiplier((m) => m + 1)}>Multiplier: {multiplier}</button>
    </div>
  );
}
```

### Example 5: Resetting Colocated State via Key Prop
```tsx
import { useState } from 'react';

export function ContactPicker() {
  const [contactId, setContactId] = useState('alice');

  return (
    <div>
      <div className="space-x-2 mb-4">
        <button onClick={() => setContactId('alice')}>Alice</button>
        <button onClick={() => setContactId('bob')}>Bob</button>
      </div>

      {/* Passing key={contactId} automatically resets EmailForm's internal state! */}
      <EmailForm key={contactId} recipientId={contactId} />
    </div>
  );
}

function EmailForm({ recipientId }: { recipientId: string }) {
  // State is colocated. When key changes, React completely remounts this component,
  // naturally resetting message back to "" without writing an imperative useEffect!
  const [message, setMessage] = useState('');

  return (
    <div className="border p-2">
      <p>Drafting email to: {recipientId}</p>
      <textarea
        value={message}
        onChange={(e) => setMessage(e.target.value)}
        className="border w-full p-1"
        placeholder="Type message..."
      />
    </div>
  );
}
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Storing selected objects in state: `setSelected(item)` | If the parent collection updates, the selected object becomes a stale reference | Store only the identifier: `setSelectedId(item.id)` and derive `find()` |
| Using `useEffect` to sync state with props (`setDerived(props.x * 2)`) | Triggers an unnecessary second render pass and causes UI flickering | Calculate directly in render: `const derived = props.x * 2` |
| Putting ephemeral UI state (dropdown open, hover) in global stores | Forces the entire app or root layouts to re-render on tiny UI interactions | Keep UI state colocated locally inside the component |
| Duplicating form fields into local state and parent state simultaneously | Creates dual sources of truth that drift out of sync | Choose one owner: either make the child controlled or fully uncontrolled |
| Lifting state to root `<App />` "just in case" | Incur huge re-render trees whenever any state changes | Keep state as low in the tree as possible |
| Resetting form state with `useEffect` when an ID prop changes | Prone to race conditions and displays stale content for 1 frame | Pass `key={id}` to the form component to trigger a clean remount |

---

## 5. React Internals: Beneath the Abstraction

### Render Invalidation Boundaries
When state updates via `setCount(c => c + 1)`, React marks that specific Fiber node with an update lane and schedules work starting from that Fiber.
- If state is lifted to `<Root />`, React must re-evaluate `<Root />` and all its child subtrees down to the leaves (unless protected by manual memoization or the React Compiler).
- If state is colocated in `<LeafButton />`, React begins reconciliation strictly at `<LeafButton />`, skipping 99% of the component hierarchy completely.

---

## 6. Performance, Bundling & React Compiler

- **React Compiler Harmony**: The React Compiler excels when state is pure and derived values are calculated inline. It automatically caches derived calculations without needing explicit `useMemo` hooks.

---

## 7. Accessibility (a11y) & Usability

- **Synchronous Input Mirroring**: Calculating derived labels synchronously ensures that assistive screen readers announce matching, up-to-date values immediately, rather than reading stale values before an effect completes.

---

## 8. Testing & Verification Patterns

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, it, expect } from 'vitest';
import { GoodUserForm } from './GoodUserForm';

describe('Derived State Verification', () => {
  it('updates full name synchronously without secondary effect passes', async () => {
    const user = userEvent.setup();
    render(<GoodUserForm />);

    const first = screen.getByPlaceholderText('First name');
    const last = screen.getByPlaceholderText('Last name');

    await user.type(first, 'Grace');
    await user.type(last, 'Hopper');

    expect(screen.getByText('Full Name: Grace Hopper')).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration

| React Version | State Colocation Paradigm |
| :--- | :--- |
| **React 15 & earlier** | Heavy reliance on Redux for all application and UI state |
| **React 16.8 (Hooks)** | `useState` made colocating local component state ergonomic |
| **React 18 & 19** | StrictMode double rendering and React Compiler penalize redundant sync effects heavily |

---

## 10. Security & Hardening

- **Preventing Stale Account Contexts**: Storing selected account data in global state across user switches risks displaying cached sensitive financial data. Resetting colocated state via `key={userId}` ensures clean memory reclamation.

---

## 11. Real-World Case Study

### Eliminating 40 Re-renders on Keystroke in a Search Grid
A data grid lifted search query state to a global context consumed by 50 independent dashboard widgets. Typing a single letter caused all 50 widgets to re-render, dropping framerates to 18 FPS.
- Colocated the search input state inside the grid header.
- Filtered table rows synchronously as pure derived state.
- Frame rate stabilized at a locked **60 FPS** with 0 lag.

---

## 12. Interview Q&A

<details>
<summary>1. What is the fundamental difference between component state and derived state?</summary>
Component state is raw, mutable data managed by React (via <code>useState</code> or <code>useReducer</code>) that triggers re-renders when updated. Derived state is any value that can be calculated synchronously from existing props or state during the render pass. Derived state should never be stored in its own <code>useState</code>.
</details>

<details>
<summary>2. Why is syncing props to state with useEffect considered an anti-pattern?</summary>
Syncing props to state via <code>useEffect</code> causes an extra render pass: the component renders with stale state, commits to the DOM, fires the effect, updates state, and renders a second time. This can cause visible UI flickering, breaks during concurrent interruptions, and introduces state synchronization bugs.
</details>

<details>
<summary>3. What is State Colocation?</summary>
State Colocation is the architectural practice of storing state as close to where it is used as possible. It isolates re-renders to only the components that care about that state, keeping parent components lean and maximizing rendering performance.
</details>

<details>
<summary>4. How does the key prop help reset colocated state?</summary>
When a component's <code>key</code> prop changes, React destroys the previous Fiber node and creates a fresh, brand new component instance. This cleanly reinitializes all internal <code>useState</code> hooks back to their initial values without writing complex imperative reset logic.
</details>

<details>
<summary>5. When is it appropriate to lift state up?</summary>
State should be lifted up only when two or more sibling components genuinely need access to the same state (either to display it or mutate it). Even then, state should only be lifted to their closest common ancestor.
</details>

<details>
<summary>6. Why shouldn't you store a selected item object in state?</summary>
If you store <code>const [selected, setSelected] = useState(item)</code>, any subsequent updates to <code>item</code> in the parent data list will not be reflected in <code>selected</code>. Storing only <code>selectedId</code> and deriving the item via <code>items.find(i => i.id === selectedId)</code> guarantees the selected item is always 100% up-to-date.
</details>

<details>
<summary>7. How does the React Compiler affect derived state?</summary>
The React Compiler automatically memoizes derived state calculations that depend on unchanged inputs, eliminating the need to manually wrap derived expressions in <code>useMemo</code>.
</details>

<details>
<summary>8. How do you distinguish between server state and client UI state?</summary>
Server state (cached data from an API, database records) should be managed by specialized server-state tools like TanStack Query. Client UI state (accordion open, active tab, draft input) should be colocated locally inside components using <code>useState</code>.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Refactoring Redundant State to Derived</summary>

**Specification**: Remove the redundant `totalCount` state variable and compute it synchronously in render.

```tsx
// SOLUTION:
export function ItemList({ items }: { items: string[] }) {
  // Pure derived state:
  const totalCount = items.length;

  return (
    <div>
      <p>Total items: {totalCount}</p>
      <ul>
        {items.map((i) => (
          <li key={i}>{i}</li>
        ))}
      </ul>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Colocated Counter Button</summary>

**Specification**: Create a `<ClickCounter />` component that encapsulates its own click count so parent components don't re-render on increment.

```tsx
import { useState } from 'react';

export function ClickCounter() {
  const [count, setCount] = useState(0);

  return (
    <button onClick={() => setCount((c) => c + 1)} className="border px-3 py-1 rounded">
      Clicks: {count}
    </button>
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): ID-Based Selection Derivation</summary>

**Specification**: Implement a list selection view storing only `selectedId: string | null` and deriving the selected object.

```tsx
import { useState } from 'react';

interface Article {
  id: string;
  title: string;
}

export function ArticleSelector({ articles }: { articles: Article[] }) {
  const [selectedId, setSelectedId] = useState<string | null>(null);

  // Derived selected object
  const activeArticle = articles.find((a) => a.id === selectedId);

  return (
    <div>
      <div className="space-x-2">
        {articles.map((a) => (
          <button key={a.id} onClick={() => setSelectedId(a.id)}>
            {a.title}
          </button>
        ))}
      </div>
      <p className="mt-2">Active: {activeArticle ? activeArticle.title : 'None'}</p>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Key-Driven Form Reset</summary>

**Specification**: Build a user selector where selecting a different user ID forces an `<EditBioForm />` to reset its draft state via `key`.

```tsx
import { useState } from 'react';

export function UserBioManager() {
  const [userId, setUserId] = useState('user-1');

  return (
    <div>
      <select value={userId} onChange={(e) => setUserId(e.target.value)}>
        <option value="user-1">User 1</option>
        <option value="user-2">User 2</option>
      </select>
      <BioEditor key={userId} userId={userId} />
    </div>
  );
}

function BioEditor({ userId }: { userId: string }) {
  const [bio, setBio] = useState('');
  return (
    <div className="mt-2">
      <textarea value={bio} onChange={(e) => setBio(e.target.value)} placeholder={`Bio for ${userId}`} />
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Multi-Filter Derived Pipeline</summary>

**Specification**: Implement a product table that computes search filtering, category filtering, and sorting purely in render.

```tsx
import { useState } from 'react';

interface Product {
  id: string;
  name: string;
  category: string;
  price: number;
}

export function ProductCatalog({ products }: { products: Product[] }) {
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('all');

  // Multi-stage pure derived pipeline
  const filtered = products
    .filter((p) => (category === 'all' ? true : p.category === category))
    .filter((p) => p.name.toLowerCase().includes(query.toLowerCase()));

  return (
    <div className="space-y-2">
      <input value={query} onChange={(e) => setQuery(e.target.value)} placeholder="Search..." />
      <select value={category} onChange={(e) => setCategory(e.target.value)}>
        <option value="all">All</option>
        <option value="electronics">Electronics</option>
        <option value="clothing">Clothing</option>
      </select>
      <ul>
        {filtered.map((p) => (
          <li key={p.id}>
            {p.name} - ${p.price}
          </li>
        ))}
      </ul>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Custom Derived State Inspector</summary>

**Specification**: Write a test utility that verifies a component produces identical derived outputs without triggering additional renders.

```tsx
import { renderHook } from '@testing-library/react';
import { useMemo } from 'react';

export function testDerivedCalculation<T, R>(input: T, calcFn: (val: T) => R) {
  const { result, rerender } = renderHook(({ data }) => useMemo(() => calcFn(data), [data]), {
    initialProps: { data: input },
  });

  const firstValue = result.current;
  rerender({ data: input });
  // Reference identity should be preserved
  if (result.current !== firstValue) {
    throw new Error('Calculation was not cleanly memoized or pure!');
  }
}
```
</details>
