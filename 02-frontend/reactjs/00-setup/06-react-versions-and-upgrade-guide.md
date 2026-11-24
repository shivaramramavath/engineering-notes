# React Versions and Upgrade Guide: 17 -> 18 -> 19

- **Topic**: `00-setup`
- **React version**: React 19+ (introduced in React 19.0.0, December 2024)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: React 19 introduces automatic action handling, `useActionState`, and deprecated `forwardRef`. Compatible with the React Compiler.

---

## 1. Concept Overview

Upgrading through the major versions of React represents a steady evolution from a purely synchronous client-side UI library into a concurrent, full-stack application runtime:
- **React 17 (The Stepping Stone)**: Introduced no new developer-facing features; focused entirely on the **New JSX Transform** (no longer requiring `import React from 'react'`) and changing event delegation from `document` to the root DOM container (`#root`), making it possible to embed multiple React versions on a single page.
- **React 18 (The Concurrent Foundation)**: Introduced the **Concurrent React** architecture, automatic batching across all asynchronous callbacks, Transitions (`useTransition`, `startTransition`), `useDeferredValue`, client-side Suspense, and new root rendering via `createRoot`.
- **React 19 (Actions, Simplification & Native Primitives)**: Shipped in December 2024. Introduces **Actions** (async transitions handling pending states, errors, and form resets automatically), companion hooks (`useActionState`, `useFormStatus`, `useOptimistic`), the versatile `use()` hook for consuming promises and contexts, **`ref` as a standard prop** (deprecating `forwardRef`), and native document metadata hoisting (`<title>`, `<meta>`, `<link>`).

```
React 17 (Safe Embedding)
  │  ├── New JSX Transform (automatic runtime)
  │  └── Event delegation at root DOM container
  ▼
React 18 (Concurrency)
  │  ├── createRoot API
  │  ├── Automatic batching everywhere
  │  └── Transitions (startTransition, useTransition)
  ▼
React 19 (Actions & Native Primitives)
     ├── Actions (<form action={asyncFn}>)
     ├── useActionState, useFormStatus, useOptimistic
     ├── use(Promise | Context)
     ├── ref as a regular prop (forwardRef deprecated)
     └── Native <title>, <meta>, <link> hoisting
```

---

## 2. Key Mechanics & Architecture

### Complete Migration Matrix (17 -> 18 -> 19)

| Feature / Area | React 17 | React 18 | React 19 (Current Baseline) |
| :--- | :--- | :--- | :--- |
| **Root Rendering** | `ReactDOM.render(<App />, root)` | `ReactDOM.createRoot(root).render(<App />)` | `createRoot` required; legacy `render` removed |
| **Passing Refs** | `forwardRef((props, ref) => ...)` | `forwardRef` required | **`ref` is a regular prop** (`function Comp({ ref, ...props })`) |
| **Form Mutations** | Manual `onSubmit`, `e.preventDefault()`, state flags | Manual `useTransition` with async handlers | **Native Actions**: `<form action={fn}>`, `useActionState` |
| **Form Pending Status**| Manual prop drilling | Manual prop drilling | **`useFormStatus()`** reads parent form status directly |
| **Optimistic UI** | Complex custom rollback state | Custom reducer / cache manipulation | **`useOptimistic(state, updateFn)`** |
| **Reading Context** | `useContext(MyContext)` | `useContext(MyContext)` | `use(MyContext)` (callable conditionally!) |
| **Rendering Context** | `<MyContext.Provider value={...}>` | `<MyContext.Provider value={...}>` | `<MyContext value={...}>` directly |
| **Document Metadata** | `react-helmet` or manual DOM mutations | `react-helmet-async` | **Native `<title>`, `<meta>` hoisting** to `<head>` |
| **Automatic Batching**| Only inside React event handlers | Everywhere (promises, timeouts, native events) | Everywhere |
| **`defaultProps`** | Supported on function components | Deprecated | **Removed**; use JS default params (`{ x = 1 }`) |
| **`propTypes`** | Runtime warnings | Runtime warnings | **Removed** from core packages |

---

## 3. Real-World Code Examples

### Example 1: Root Initialization (React 17 vs 18/19)
```tsx
// React 17 (Legacy - Removed in 19):
// import ReactDOM from 'react-dom';
// ReactDOM.render(<App />, document.getElementById('root'));

// React 18 & 19 (Modern Standard):
import { createRoot } from 'react-dom/client';
import { App } from './App';

const container = document.getElementById('root');
if (!container) throw new Error('Root container missing');

const root = createRoot(container);
root.render(<App />);
```
Output:
```
// App mounted cleanly without ReactDOM.render deprecation warnings
```

### Example 2: React 19 Actions with `useActionState`
```tsx
import { useActionState } from 'react';

async function updateUsername(previousState: { error: string | null }, formData: FormData) {
  const newName = formData.get('username') as string;
  if (!newName || newName.length < 3) {
    return { error: 'Username must be at least 3 characters' };
  }
  // Simulate network mutation
  await new Promise((res) => setTimeout(res, 500));
  return { error: null };
}

export function ProfileForm() {
  const [state, formAction, isPending] = useActionState(updateUsername, { error: null });

  return (
    <form action={formAction} className="flex flex-col gap-2">
      <input name="username" placeholder="Enter username" className="border p-2" />
      <button type="submit" disabled={isPending} className="bg-blue-600 text-white p-2">
        {isPending ? 'Saving...' : 'Save Profile'}
      </button>
      {state.error && <p className="text-red-500 text-sm">{state.error}</p>}
    </form>
  );
}
```
Output:
```
// Renders form; when submitted, button displays 'Saving...', resets automatically on success
```

### Example 3: `ref` as a Regular Prop (No More `forwardRef`)
```tsx
import { useRef } from 'react';

// In React 19, functional components receive 'ref' directly in props!
interface CustomInputProps {
  label: string;
  ref?: React.Ref<HTMLInputElement>;
}

function CustomInput({ label, ref }: CustomInputProps) {
  return (
    <label className="flex flex-col gap-1">
      <span>{label}</span>
      <input ref={ref} className="border rounded px-2 py-1" />
    </label>
  );
}

export function ParentForm() {
  const inputRef = useRef<HTMLInputElement>(null);

  const focusInput = () => {
    inputRef.current?.focus();
  };

  return (
    <div>
      <CustomInput label="Full Name" ref={inputRef} />
      <button onClick={focusInput} className="mt-2 text-sm text-blue-500">
        Focus Input
      </button>
    </div>
  );
}
```
Output:
```
// Clicking button invokes inputRef.current.focus() cleanly without forwardRef wrapper
```

### Example 4: Native Document Metadata Hoisting
```tsx
interface ArticleProps {
  title: string;
  summary: string;
}

export function ArticlePage({ title, summary }: ArticleProps) {
  return (
    <article className="prose">
      {/* React 19 hoists these tags directly to <head> automatically! */}
      <title>{title} — Engineering Notes</title>
      <meta name="description" content={summary} />
      <link rel="canonical" href="https://notes.internal/article" />

      <h1>{title}</h1>
      <p>{summary}</p>
    </article>
  );
}
```
Output:
```
// Document <head> dynamically receives the <title> and <meta> tags on render
```

### Example 5: Reading Context Conditionally with `use()`
```tsx
import { createContext, use } from 'react';

const AdminContext = createContext<{ role: string } | null>(null);

export function DashboardBadge({ showRole }: { showRole: boolean }) {
  if (!showRole) {
    return <span>Viewer</span>; // Early return before reading context!
  }

  // use() CAN be called conditionally inside if statements!
  const admin = use(AdminContext);
  return <span>Role: {admin?.role ?? 'Guest'}</span>;
}
```
Output:
```
// When showRole=false: renders 'Viewer'
// When showRole=true: evaluates context and renders 'Role: admin'
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Wrapping new React 19 components in `React.forwardRef` | `forwardRef` is deprecated and adds unnecessary higher-order wrapper overhead | Accept `ref` directly as a prop: `function Input({ ref, ...props })` |
| Using `ReactDOM.render` in React 19 | Removed from `react-dom`; throws runtime error | Migrate to `createRoot` from `react-dom/client` |
| Calling `e.preventDefault()` inside React 19 `<form action={fn}>` | Action handlers are invoked via Transitions; manual preventDefault breaks Action state | Let React handle submission lifecycle natively via Action functions |
| Calling `useFormStatus()` in the same component rendering `<form>` | `useFormStatus` reads status from the *parent* form in Context; it cannot read its own component's form | Extract the submit button/status indicator into a child component |
| Relying on `defaultProps` in functional components | Completely removed in React 19 | Use standard JavaScript default parameters: `({ size = "md" })` |
| Using `useContext` after early returns | Violates the Rules of Hooks in older React versions | Migrate to React 19 `use(MyContext)` which supports conditional execution |

---

## 5. React Internals: Beneath the Abstraction

### Fiber Structure in React 19
In React 19, the underlying Fiber node architecture remains centered around the double-buffering work loop (`current` vs `workInProgress` trees). However, Hook allocation has been updated:
- The internal dispatcher table (`ReactCurrentDispatcher`) includes support for the `use` primitive, which checks whether the passed resource is a Promise (`thenable`) or a Context.
- When `use(promise)` is invoked on a pending promise, React throws the promise up the component tree to the nearest `<Suspense>` boundary, registering an internal listener that schedules a re-render lane when the promise fulfills.

---

## 6. Performance, Bundling & React Compiler

- **React Compiler Integration**: React 19 is fully optimized to work alongside the React Compiler. Code written with standard props, default arguments, and pure functions is automatically memoized at build time, eliminating the need to write manual `useMemo` and `useCallback` guards.
- **Tree-Shaking**: Legacy features (`propTypes`, `defaultProps`, `forwardRef`) have been pruned, resulting in a cleaner core runtime footprint.

---

## 7. Accessibility (a11y) & Usability

- **Focus Preservation**: When forms submit using React 19 Actions, focus is preserved cleanly across pending transitions rather than being reset by unmounting DOM elements.
- **Form Status Feedback**: Pairing `useFormStatus` with `aria-busy={pending}` ensures screen readers announce that an asynchronous operation is in progress without custom event emitters.

---

## 8. Testing & Verification Patterns

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, it, expect } from 'vitest';
import { ProfileForm } from './ProfileForm';

describe('React 19 Form Actions', () => {
  it('disables submit button during action transition', async () => {
    const user = userEvent.setup();
    render(<ProfileForm />);

    const input = screen.getByPlaceholderText('Enter username');
    const button = screen.getByRole('button', { name: /save profile/i });

    await user.type(input, 'alex');
    await user.click(button);

    // Verify button shows pending state
    expect(screen.getByRole('button')).toBeDisabled();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Key Additions | Breaking Removals |
| :--- | :--- | :--- |
| **React 17** | New JSX Transform, root container event bubbling | Event pooling removed |
| **React 18** | `createRoot`, automatic batching, `useTransition`, `useId` | Deprecated synchronous `ReactDOM.render` |
| **React 19** | Actions, `useActionState`, `useFormStatus`, `useOptimistic`, `use()`, `ref` as prop | `ReactDOM.render` removed, `defaultProps` removed, `propTypes` removed |

---

## 10. Security & Hardening

- **Server Action Boundaries**: In full-stack frameworks, Action functions executing on the server must validate all incoming `FormData` using schemas like Zod to prevent mass-assignment vulnerabilities.
- **Metadata Tag Sanitization**: When rendering `<title>` and `<meta>` tags dynamically, ensure inputs are strictly strings to prevent attribute-based XSS injection.

---

## 11. Real-World Case Study

### Migrating an Enterprise Form from React 17 to React 19
A production dashboard previously managed forms with 5 distinct state variables: `values`, `errors`, `isSubmitting`, `isSuccess`, and `dirty`. By migrating to React 19:
- Replaced 5 `useState` calls and manual `e.preventDefault()` with a single `useActionState` hook.
- Extracted `<SubmitButton />` into a modular design system component using `useFormStatus()`.
- Reduced component boilerplate by 62% while gaining automatic form resets on successful submissions.

---

## 12. Interview Q&A

<details>
<summary>1. What are Actions in React 19 and how do they differ from normal async event handlers?</summary>
In React 19, an Action is an async function passed to elements like <code>&lt;form action={...}&gt;</code> or executed via transitions. Unlike standard async handlers where developers must manually call <code>e.preventDefault()</code> and manage <code>isPending</code> and error states with multiple <code>useState</code> hooks, React Actions automatically manage pending states, surface errors, handle optimistic rollbacks, and reset uncontrolled form fields on success.
</details>

<details>
<summary>2. How has ref handling changed in React 19 compared to React 18?</summary>
In React 18 and earlier, passing a <code>ref</code> to a functional component required wrapping it in <code>React.forwardRef((props, ref) => ...)</code>. In React 19, <code>ref</code> is treated as a regular prop. Functional components can accept <code>ref</code> directly in their props signature (<code>function MyInput({ ref, ...props })</code>). <code>forwardRef</code> is deprecated.
</details>

<details>
<summary>3. Why can the new use() hook be called conditionally, unlike standard React hooks?</summary>
Standard hooks rely on a strictly ordered linked list on the Fiber node, requiring identical invocation order across renders. The <code>use()</code> function is implemented as a runtime reader rather than an indexed state slot; when passed a Promise, it integrates with the Suspense unwinding mechanism, and when passed a Context, it performs a dynamic lookup.
</details>

<details>
<summary>4. What is the difference between useActionState and useFormStatus?</summary>
<code>useActionState</code> is invoked in the component managing the Action to track the returned state, pending flag, and dispatch action. <code>useFormStatus</code> is a contextual hook from <code>react-dom</code> designed for child components (like submit buttons) to read the parent form's submission state without prop drilling.
</details>

<details>
<summary>5. What happened to defaultProps and propTypes in React 19?</summary>
<code>defaultProps</code> on function components and <code>propTypes</code> have been completely removed from React 19 core. Default values must be declared using standard JavaScript default parameters, and static type safety should be enforced via TypeScript.
</details>

<details>
<summary>6. How does React 19 handle document metadata tags like &lt;title&gt; and &lt;meta&gt;?</summary>
React 19 natively hoists metadata tags rendered anywhere inside the component tree into the document's <code>&lt;head&gt;</code> section, eliminating the need for third-party libraries like <code>react-helmet</code>.
</details>

<details>
<summary>7. What was the purpose of React 17's change to event delegation?</summary>
React 17 moved event listeners from the global <code>document</code> node down to the root DOM container (<code>#root</code>). This prevented event bubbling conflicts when embedding multiple React versions or integrating micro-frontends on the same page.
</details>

<details>
<summary>8. How does useOptimistic improve perceived user experience?</summary>
<code>useOptimistic</code> immediately renders an expected mutation result while the asynchronous network action is in flight. If the network request fails, React catches the rejection and automatically rolls the UI back to the true state.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Refactoring forwardRef to React 19 Ref Prop</summary>

**Specification**: Refactor a legacy `forwardRef` button component into a clean React 19 functional component accepting `ref` as a prop.

```tsx
// Modern React 19 Solution
interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  ref?: React.Ref<HTMLButtonElement>;
  variant?: 'primary' | 'secondary';
}

export function Button({ ref, variant = 'primary', children, ...props }: ButtonProps) {
  const baseClass = variant === 'primary' ? 'bg-blue-600 text-white' : 'bg-gray-200 text-black';
  return (
    <button ref={ref} className={`px-4 py-2 rounded ${baseClass}`} {...props}>
      {children}
    </button>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Context Provider Modernization</summary>

**Specification**: Render a theme context using React 19's direct `<Context>` syntax instead of `<Context.Provider>`.

```tsx
import { createContext } from 'react';

export const ThemeContext = createContext<'light' | 'dark'>('light');

export function ThemeProvider({ theme, children }: { theme: 'light' | 'dark'; children: React.ReactNode }) {
  // Direct <Context value={...}> syntax in React 19
  return <ThemeContext value={theme}>{children}</ThemeContext>;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Standalone Submit Button with useFormStatus</summary>

**Specification**: Create a reusable `<SubmitButton />` component that disables itself and shows a spinner when its enclosing form is submitting.

```tsx
import { useFormStatus } from 'react-dom';

export function SubmitButton({ label = 'Submit' }: { label?: string }) {
  const { pending } = useFormStatus();

  return (
    <button
      type="submit"
      disabled={pending}
      aria-busy={pending}
      className="bg-indigo-600 text-white px-4 py-2 rounded disabled:opacity-50"
    >
      {pending ? 'Processing...' : label}
    </button>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Optimistic Item Counter</summary>

**Specification**: Implement an upvote button that uses `useOptimistic` to immediately increment the count while an async API call resolves.

```tsx
import { useOptimistic, useTransition } from 'react';

async function sendVoteToServer(newCount: number): Promise<void> {
  await new Promise((res) => setTimeout(res, 600));
}

export function UpvoteButton({ initialCount }: { initialCount: number }) {
  const [isPending, startTransition] = useTransition();
  const [optimisticCount, setOptimisticCount] = useOptimistic(
    initialCount,
    (current, increment: number) => current + increment
  );

  const handleVote = () => {
    startTransition(async () => {
      setOptimisticCount(1);
      await sendVoteToServer(optimisticCount + 1);
    });
  };

  return (
    <button onClick={handleVote} disabled={isPending} className="border p-2 rounded">
      ▲ Upvote ({optimisticCount})
    </button>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Async Form with useActionState & Validation</summary>

**Specification**: Build a newsletter subscription form using `useActionState`. Validate email format, simulate network latency, return errors, and display success messaging.

```tsx
import { useActionState } from 'react';

type FormState = { error: string | null; success: boolean };

async function subscribeAction(_prevState: FormState, formData: FormData): Promise<FormState> {
  const email = formData.get('email') as string;
  if (!email || !email.includes('@')) {
    return { error: 'Please provide a valid email address.', success: false };
  }
  await new Promise((res) => setTimeout(res, 800));
  return { error: null, success: true };
}

export function NewsletterForm() {
  const [state, formAction, isPending] = useActionState(subscribeAction, { error: null, success: false });

  if (state.success) {
    return <p className="text-green-600 font-semibold">Thank you for subscribing!</p>;
  }

  return (
    <form action={formAction} className="space-y-2">
      <input name="email" type="email" placeholder="you@domain.com" required className="border p-2 rounded w-full" />
      {state.error && <p className="text-red-500 text-sm">{state.error}</p>}
      <button type="submit" disabled={isPending} className="bg-black text-white px-4 py-2 rounded">
        {isPending ? 'Subscribing...' : 'Subscribe'}
      </button>
    </form>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Conditional Promise Resolution with use()</summary>

**Specification**: Implement a user profile card that conditionally uses `use(promise)` to fetch extra billing data only when the "View Billing" toggle is enabled, wrapped in a Suspense boundary.

```tsx
import { use, Suspense, useState } from 'react';

interface BillingInfo {
  tier: string;
  renewalDate: string;
}

function fetchBillingDetails(): Promise<BillingInfo> {
  return new Promise((resolve) =>
    setTimeout(() => resolve({ tier: 'Enterprise Pro', renewalDate: '2026-12-31' }), 1000)
  );
}

function BillingDetails({ promise }: { promise: Promise<BillingInfo> }) {
  const info = use(promise);
  return (
    <div className="bg-gray-50 p-2 rounded mt-2">
      <p>Tier: {info.tier}</p>
      <p>Renews: {info.renewalDate}</p>
    </div>
  );
}

export function UserCard() {
  const [billingPromise, setBillingPromise] = useState<Promise<BillingInfo> | null>(null);

  return (
    <div className="border p-4 rounded max-w-sm">
      <h3 className="font-bold">Organization Account</h3>
      {!billingPromise ? (
        <button onClick={() => setBillingPromise(fetchBillingDetails())} className="text-blue-600 text-sm mt-2">
          Load Billing Details
        </button>
      ) : (
        <Suspense fallback={<p className="text-sm text-gray-400">Loading billing...</p>}>
          <BillingDetails promise={billingPromise} />
        </Suspense>
      )}
    </div>
  );
}
```
</details>
