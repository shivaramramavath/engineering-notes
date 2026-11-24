# React useId: Unique Identifier Generation for Accessibility & Hydration

- **Topic**: `03-hooks`
- **React version**: React 18+ (Standard in React 19)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Core React dispatcher hook; zero additional runtime payload; unaffected by React Compiler memoization transforms.

---

## 1. Concept Overview

In web applications, generating unique HTML identifiers is critical for accessibility (a11y). Assistive technologies (screen readers) rely on matching attributes such as `id` and `htmlFor` on `<label>` elements, or linking inputs with descriptive error messages and helper text via `aria-describedby` and `aria-labelledby`.

Prior to React 18, generating unique IDs in components suffered from fundamental architectural flaws:
1. **Global Counters (`let id = 0; id++`)**: Global counters break under Server-Side Rendering (SSR). If the server handles multiple concurrent requests or components render in a different order on the client during hydration, the IDs mismatch, resulting in hydration warnings and broken accessibility linkages.
2. **`Math.random()` or `crypto.randomUUID()`**: Executing random number generation during render causes a guaranteed hydration mismatch between the server-rendered HTML string and the client-rendered virtual DOM.
3. **Prefixing Hacks in State / Effects**: Generating IDs in `useEffect` leaves elements without IDs during the initial server render and first client paint, causing layout shifts or accessibility dead zones before hydration completes.

`useId` is a dedicated React hook introduced in React 18 that generates unique, deterministic identifiers that are guaranteed to match identically between the server and the client.

```
Server Rendering:
┌───────────────────────────┐
│ Component Tree Traversal  │───► Generates deterministic tree path: ":r1:"
└───────────────────────────┘          │
                                       ▼
                              HTML: <input id=":r1:" />
                                       │
Hydration Phase (Browser):             │ (Guaranteed Match)
┌───────────────────────────┐          ▼
│ Client Hydration Process  │───► Recomputes same tree path: ":r1:"
└───────────────────────────┘
```

### When to Use
- Linking form controls (`<input>`, `<select>`, `<textarea>`) to `<label>` elements using `htmlFor` and `id`.
- Linking elements to helper text or error validation messages using `aria-describedby` or `aria-errormessage`.
- Generating unique IDs for complex ARIA composite widgets (tabs, accordions, comboboxes, tree views).

### When NOT to Use
- **Do NOT use `useId` to generate keys for lists**: React list keys must be derived directly from your data (e.g., database IDs, UUIDs from an API) so React can track item insertions, deletions, and moves. `useId` produces identical IDs for a given call site across renders, not per-item keys inside a loop.
- **Do NOT use `useId` as CSS class names or selectors** without escaping: The generated strings contain colon characters (e.g., `:r0:`), which are CSS pseudo-class delimiters.

---

## 2. Key Mechanics & Architecture

Instead of maintaining a naive linear counter, React uses a **tree-structured positional encoding system**. Every Fiber in the React tree has an implicit position determined by its parentage and sibling slot.

React assigns IDs based on the path from the root Fiber node down to the component invoking `useId`. The resulting string is base-32 encoded and wrapped in colons (e.g., `:r0:`, `:R1a:`).

```
                      Root Fiber
                          │
            ┌─────────────┴─────────────┐
        Child 0 (r0)                Child 1 (r1)
            │                           │
      ┌─────┴─────┐                  ┌──┴──┐
   Slot 0      Slot 1              Slot 0 Slot 1
   (:r0:0:)    (:r0:1:)            (:r1:0:) (:r1:1:)
```

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Execution Phase** | Render phase (synchronous) |
| **SSR / Hydration** | Guaranteed deterministic parity between server HTML and client hydration |
| **Format** | Leading and trailing colons (e.g., `:r0:`, `:r1:`), base-32 encoded path |
| **Multiple IDs per Component** | Best practice: call `useId` once, append descriptive suffixes (`${id}-input`, `${id}-error`) |
| **`identifierPrefix` Support** | Configurable via `createRoot(el, { identifierPrefix: 'app1' })` or `hydrateRoot` |
| **Batching / State Impact** | Purely positional; triggers zero re-renders and incurs no side effects |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Linking a text input with a `<label>` and an `aria-describedby` hint using a single `useId` call.

```tsx
import React, { useId } from 'react';

export function AccessibleEmailField() {
  const id = useId();
  const inputId = `${id}-email`;
  const hintId = `${id}-hint`;

  return (
    <div className="form-group">
      <label htmlFor={inputId}>Email Address</label>
      <input
        id={inputId}
        type="email"
        placeholder="alex@example.com"
        aria-describedby={hintId}
      />
      <p id={hintId} className="field-hint">
        We will never share your email with third parties.
      </p>
    </div>
  );
}
```
Output HTML in DOM:
```html
<div class="form-group">
  <label for=":r0:-email">Email Address</label>
  <input id=":r0:-email" type="email" placeholder="alex@example.com" aria-describedby=":r0:-hint" />
  <p id=":r0:-hint" class="field-hint">We will never share your email with third parties.</p>
</div>
```

### Example 2: Production / Real-World Integration
A reusable `FormField` component supporting validation errors, description slots, and complete ARIA linking.

```tsx
import React, { useId, type ReactNode, type InputHTMLAttributes } from 'react';

interface FormFieldProps extends InputHTMLAttributes<HTMLInputElement> {
  label: string;
  description?: string;
  error?: string;
}

export function FormField({ label, description, error, ...inputProps }: FormFieldProps) {
  const baseId = useId();
  const inputId = `${baseId}-input`;
  const descId = `${baseId}-desc`;
  const errorId = `${baseId}-error`;

  // Compute aria-describedby references conditionally
  const describedBy = [
    description ? descId : null,
    error ? errorId : null,
  ].filter(Boolean).join(' ') || undefined;

  return (
    <div className="flex flex-col gap-1.5 mb-4">
      <label htmlFor={inputId} className="text-sm font-semibold text-zinc-900">
        {label}
      </label>
      <input
        {...inputProps}
        id={inputId}
        aria-invalid={Boolean(error)}
        aria-describedby={describedBy}
        aria-errormessage={error ? errorId : undefined}
        className={`px-3 py-2 border rounded-md text-sm ${
          error ? 'border-red-500 focus:ring-red-500' : 'border-zinc-300 focus:ring-blue-500'
        }`}
      />
      {description && (
        <span id={descId} className="text-xs text-zinc-500">
          {description}
        </span>
      )}
      {error && (
        <span id={errorId} role="alert" className="text-xs font-medium text-red-600">
          {error}
        </span>
      )}
    </div>
  );
}
```
Output:
```
// Renders fully accessible input bound to label and error message:
// input: id=":r1:-input", aria-describedby=":r1:-desc :r1:-error", aria-invalid="true"
```

### Example 3: Edge Case / Multi-App Micro-Frontend Prefixing
Preventing ID collisions when multiple React roots or micro-frontends coexist on the same web page using `identifierPrefix`.

```tsx
import { createRoot } from 'react-dom/client';
import React, { useId } from 'react';

function Widget({ title }: { title: string }) {
  const id = useId();
  return (
    <section aria-labelledby={`${id}-header`}>
      <h2 id={`${id}-header`}>{title}</h2>
      <p>Widget unique ID: {id}</p>
    </section>
  );
}

// App 1 Mounting with dedicated prefix:
const root1Element = document.getElementById('micro-app-cart')!;
const root1 = createRoot(root1Element, {
  identifierPrefix: 'cart-app-',
});
root1.render(<Widget title="Shopping Cart" />);

// App 2 Mounting on the same page with different prefix:
const root2Element = document.getElementById('micro-app-nav')!;
const root2 = createRoot(root2Element, {
  identifierPrefix: 'nav-app-',
});
root2.render(<Widget title="Site Navigation" />);
```
Output HTML for App 1:
```html
<section aria-labelledby=":cart-app-r0:-header">
  <h2 id=":cart-app-r0:-header">Shopping Cart</h2>
  <p>Widget unique ID: :cart-app-r0:</p>
</section>
```
Output HTML for App 2:
```html
<section aria-labelledby=":nav-app-r0:-header">
  <h2 id=":nav-app-r0:-header">Site Navigation</h2>
  <p>Widget unique ID: :nav-app-r0:</p>
</section>
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Using `useId` inside loops or mapping over arrays to generate React `key` props.

```tsx
// ❌ WRONG: Calling useId inside a loop violates Hook rules,
// or using one useId for all list item keys causes reconciliation corruption.
export function FaultyList({ items }: { items: { name: string }[] }) {
  const listId = useId();

  return (
    <ul>
      {items.map((item, index) => (
        // BUG: All items share the exact same key prefix + index,
        // and if items reorder, React cannot track item identity properly!
        <li key={`${listId}-${index}`}>{item.name}</li>
      ))}
    </ul>
  );
}

// ✅ CORRECT: Keys MUST come from the underlying data model!
// useId is only for HTML/DOM element IDs.
export function CorrectList({ items }: { items: { id: string; name: string }[] }) {
  const listLabelId = useId();

  return (
    <div>
      <h3 id={listLabelId}>Team Members</h3>
      <ul aria-labelledby={listLabelId}>
        {items.map((item) => (
          <li key={item.id}>{item.name}</li>
        ))}
      </ul>
    </div>
  );
}
```
Output:
```
// React cleanly reconciles list items using data keys (e.g. item.id: "usr_102")
// while the <ul> correctly references the <h3> using ":r3:".
```

### Example 5: Idiomatic Modern React 19 Pattern
Compound Tabs component utilizing React 19's native `ref` prop (no `forwardRef`) and `useId` for full WAI-ARIA Tabs pattern (`aria-controls`, `aria-labelledby`, `role="tab"`, `role="tabpanel"`).

```tsx
import React, { useId, useState, type ReactNode } from 'react';

interface TabItem {
  id: string;
  label: string;
  content: ReactNode;
}

export function AccessibleTabs({ tabs }: { tabs: TabItem[] }) {
  const [activeTab, setActiveTab] = useState(tabs[0]?.id);
  const baseId = useId();

  return (
    <div className="w-full max-w-md mx-auto">
      <div role="tablist" aria-orientation="horizontal" className="flex border-b border-zinc-200">
        {tabs.map((tab) => {
          const tabId = `${baseId}-tab-${tab.id}`;
          const panelId = `${baseId}-panel-${tab.id}`;
          const isSelected = activeTab === tab.id;

          return (
            <button
              key={tab.id}
              id={tabId}
              role="tab"
              type="button"
              aria-selected={isSelected}
              aria-controls={panelId}
              tabIndex={isSelected ? 0 : -1}
              onClick={() => setActiveTab(tab.id)}
              className={`px-4 py-2 text-sm font-medium transition-colors ${
                isSelected ? 'border-b-2 border-indigo-600 text-indigo-600' : 'text-zinc-500 hover:text-zinc-700'
              }`}
            >
              {tab.label}
            </button>
          );
        })}
      </div>

      {tabs.map((tab) => {
        const tabId = `${baseId}-tab-${tab.id}`;
        const panelId = `${baseId}-panel-${tab.id}`;
        const isSelected = activeTab === tab.id;

        return (
          <div
            key={tab.id}
            id={panelId}
            role="tabpanel"
            aria-labelledby={tabId}
            hidden={!isSelected}
            tabIndex={0}
            className="p-4 focus:outline-none"
          >
            {isSelected && tab.content}
          </div>
        );
      })}
    </div>
  );
}
```
Output:
```
// Renders tabs with perfect WAI-ARIA tab/tabpanel linkage:
// tab: id=":r4:-tab-profile" aria-controls=":r4:-panel-profile"
// panel: id=":r4:-panel-profile" aria-labelledby=":r4:-tab-profile"
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Using `useId` to generate React `key` props** | `useId` reflects the component call site, not item identity in dynamic data. Breaks reordering and deletion. | Use persistent entity identifiers from your database or API (e.g., `item.id`). |
| **Calling `useId` multiple times per component** | Inefficient; clutters the Fiber tree and generates multiple string instances. | Call `useId` once per component; append descriptive suffixes (`${id}-fname`, `${id}-lname`). |
| **Using `useId` in CSS selectors directly (`#${id}`)** | React IDs contain `:` which is a CSS pseudo-class operator. `document.querySelector('#:r0:')` throws a syntax error. | Use `CSS.escape(id)` or query via attribute selector: `[id="${id}"]`. |
| **Using `Math.random()` or global counters for SSR** | Server and client trees produce different numbers, causing hydration mismatch errors. | Always use `useId` for any client-server coordinated DOM ID. |
| **Conditional `useId` calls** | Placing `useId` inside `if` statements violates the Rules of Hooks and corrupts the Hook pointer. | Call `useId` unconditionally at the top level of the component. |
| **Hardcoding IDs in reusable components** | Placing `id="search-input"` inside a component renders duplicate IDs if used multiple times on a page. | Generate IDs dynamically via `useId` for all reusable components. |

---

## 5. React Internals: Beneath the Abstraction

Internally, React manages ID allocation inside `ReactFiberHooks.js` through the current rendering fiber's `treeContext`.

When React renders a tree:
1. Every time React descends into a subtree with multiple children or conditional branches, it allocates an ID slot.
2. The tree position is stored as a sequence of bit shifts. A binary tree path records left vs right turns (or child index slots).
3. The bit sequence is converted into a base-32 string (digits `0-9` and letters `a-v`).
4. In server-side rendering (`react-dom/server`), the same depth-first traversal occurs. Because React's traversal algorithm is purely deterministic, the sequence of bit-path tokens generated on the server exactly matches the sequence evaluated during client hydration.
5. Colons (`:`) are prepended and appended to prevent user-defined IDs from colliding with React's generated IDs in the global `document` namespace.

---

## 6. Performance, Bundling & React Compiler

- **Runtime Cost**: Negligible. Involves bitwise operations and short string concatenations during the render phase.
- **Tree-Shaking**: `useId` is exported directly from the core `react` package. Bundlers (Vite, Rollup, Webpack) tree-shake unused exports cleanly.
- **React Compiler**: The React Compiler treats `useId` as a hook with stable return value semantics. It does not memoize the `useId` call itself, but it safely memoizes objects and JSX referencing the derived IDs.

---

## 7. Accessibility (a11y) & Usability

`useId` is the bedrock for implementing accessible components according to W3C WAI-ARIA 1.2:
- **Form Labeling**: Screen readers announce the label text only when `htmlFor` matches the input's `id`.
- **Live Regions & Errors**: `aria-errormessage` and `aria-describedby` require valid space-delimited ID references.
- **Accordion & Disclosure Panels**: The trigger button must specify `aria-controls="panel-id"`, and the panel must specify `aria-labelledby="trigger-id"`.

---

## 8. Testing & Verification Patterns

When testing components that utilize `useId` with Vitest and `@testing-library/react`, do **not** assert against exact hardcoded ID strings like `:r0:`, because test suites or wrapper components may shift positional offsets. Instead, test accessibility linkages using accessible queries (`getByLabelText`, `getByRole`).

```tsx
import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import userEvent from '@testing-library/user-event';
import { FormField } from './FormField';

describe('FormField Accessibility', () => {
  it('correctly associates label and input via generated id', async () => {
    render(
      <FormField
        label="Username"
        description="Choose a unique username"
        error="Username already taken"
      />
    );

    // Verify accessible query finds the input via its label
    const input = screen.getByLabelText('Username');
    expect(input).toBeInTheDocument();

    // Verify aria-describedby points to both description and error
    const desc = screen.getByText('Choose a unique username');
    const error = screen.getByRole('alert');

    const describedBy = input.getAttribute('aria-describedby');
    expect(describedBy).toContain(desc.id);
    expect(describedBy).toContain(error.id);
    expect(input).toHaveAttribute('aria-invalid', 'true');
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17** | `useId` did not exist. Teams used manual incrementing counters or third-party libraries (e.g., `lodash.uniqueId`), causing SSR hydration issues. | Migrate legacy manual counters to `useId`. |
| **React 18** | Introduced `useId` with streaming SSR support and base-32 tree path generation. | Add `identifierPrefix` in `createRoot` if hosting micro-frontends. |
| **React 19** | Fully supported across client, SSR, and Server Components (RSC). | Direct integration with React 19 Actions and native `ref` props. |

---

## 10. Security & Hardening

1. **Avoid Direct DOM Injection**: Never pass raw unvalidated user input concatenated with `useId` into `innerHTML` or `dangerouslySetInnerHTML`.
2. **CSS Selector Injection**: If you must target an element by ID in JavaScript via `document.querySelector`, always wrap the ID with `CSS.escape(id)` to prevent malformed selector exploits.
3. **Prefix Isolation**: In multi-tenant or micro-frontend architectures, always pass unique `identifierPrefix` values to `createRoot` to avoid cross-tenant element ID collisions.

---

## 11. Real-World Case Study

### High-Density Enterprise Financial Data Table with Expandable Row Details

An enterprise trading platform renders 500 financial rows with expandable audit logs. Every row has keyboard-accessible expand triggers, inline trade edit fields, and audit summaries.

```tsx
import React, { useId, useState } from 'react';

interface TradeRowProps {
  symbol: string;
  amount: number;
  status: 'PENDING' | 'EXECUTED' | 'FAILED';
  auditLog: string;
}

export function TradeRow({ symbol, amount, status, auditLog }: TradeRowProps) {
  const [isExpanded, setIsExpanded] = useState(false);
  const rowId = useId();
  const triggerId = `${rowId}-trigger`;
  const detailsId = `${rowId}-details`;

  return (
    <div className="border-b border-zinc-200 py-2">
      <div className="flex items-center justify-between">
        <span className="font-mono font-bold">{symbol}</span>
        <span>${amount.toLocaleString()}</span>
        <button
          id={triggerId}
          type="button"
          aria-expanded={isExpanded}
          aria-controls={detailsId}
          onClick={() => setIsExpanded((prev) => !prev)}
          className="text-xs bg-zinc-100 hover:bg-zinc-200 px-2 py-1 rounded"
        >
          {isExpanded ? 'Hide Details' : 'View Audit'}
        </button>
      </div>

      <div
        id={detailsId}
        role="region"
        aria-labelledby={triggerId}
        hidden={!isExpanded}
        className="mt-2 text-xs text-zinc-600 bg-zinc-50 p-2 rounded"
      >
        <p><strong>Status:</strong> {status}</p>
        <p><strong>Log:</strong> {auditLog}</p>
      </div>
    </div>
  );
}
```

---

## 12. Interview Q&A

<details>
<summary>1. Why can't we use a simple incremental counter (e.g. `let count = 0; const id = count++`) instead of `useId`?</summary>

In server-side rendering (SSR), a global counter fails for two critical reasons:
1. **Concurrent Server Requests**: The server handles multiple user requests concurrently. Request A and Request B share the same process-level counter, causing non-deterministic ID assignment based on thread execution timing.
2. **Hydration Order Differences**: In client-side hydration (especially with selective hydration and streaming introduced in React 18), components may hydrate in an order different from how they were rendered on the server (e.g., prioritized by user clicks). A global counter would assign different IDs during hydration, throwing hydration mismatch warnings and breaking ARIA associations.
`useId` avoids this by deriving the ID deterministically from the component's positional path in the Fiber tree.
</details>

<details>
<summary>2. How does React's tree-based ID algorithm prevent collisions during tree mutations?</summary>

React generates IDs based on the sequence of child slots traversed from the root. When a parent fiber has multiple children, it encodes the child index into a bitfield. If the component re-renders, its position within its parent fiber remains identical, guaranteeing the exact same ID. Even if sibling trees unmount or re-render, the local slot index for an unchanged fiber produces the same base-32 string.
</details>

<details>
<summary>3. Can `useId` be used inside a loop or `map()` function?</summary>

No. Calling `useId` inside a `.map()` callback violates the fundamental Rules of Hooks: hooks must be called at the top level and in the exact same order on every render. If you need IDs for elements inside a loop, call `useId` once in the parent component and append the loop index or a unique entity key (e.g., `${baseId}-item-${item.id}`).
</details>

<details>
<summary>4. Why do `useId` strings start and end with colons (e.g., `:r0:`)?</summary>

The colon wrapper serves two purposes:
1. **Namespace Isolation**: Standard HTML `id` attributes created by developers rarely begin with colons, ensuring React-generated IDs do not collide with manually authored DOM IDs.
2. **Selector Safety**: In `querySelector`, `:` indicates pseudo-classes (`:hover`, `:first-child`). This intentionally discourages developers from writing fragile DOM queries like `document.querySelector(id)` directly against React internal IDs without explicitly escaping them.
</details>

<details>
<summary>5. How do you configure `useId` in micro-frontends sharing a single DOM page?</summary>

You pass the `identifierPrefix` option to `createRoot` or `hydrateRoot`:
```tsx
const root = createRoot(container, {
  identifierPrefix: 'checkout-app-'
});
```
This prepends `checkout-app-` into the generated string (e.g., `:checkout-app-r0:`), preventing cross-application ID collisions.
</details>

<details>
<summary>6. Does `useId` cause re-renders when state changes?</summary>

No. `useId` is synchronous and pure. It inspects the current Fiber's `treeContext` and returns a stable string during both the initial mount and every subsequent re-render. It maintains no local state and schedules no updates.
</details>

<details>
<summary>7. What is the difference between `aria-labelledby` and `aria-describedby`, and how does `useId` support them?</summary>

`aria-labelledby` specifies which element serves as the primary accessible name (overriding the element's own text or label). `aria-describedby` provides secondary, detailed information or helper text. Both take space-separated lists of DOM IDs. `useId` enables clean, declarative generation of these reference IDs across multiple child DOM elements.
</details>

<details>
<summary>8. How does `useId` interact with React Server Components (RSC)?</summary>

`useId` is fully supported in Server Components. When an RSC renders on the server, it evaluates `useId` deterministically and streams the generated ID down as part of the rendered payload, ensuring seamless alignment if child client components consume the ID.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Accessible Switch / Toggle Component</summary>

**Specification**: Create a reusable `ToggleSwitch` component that renders a button with `role="switch"` linked to an external label via `useId` and `aria-labelledby`.

```tsx
import React, { useId, useState } from 'react';

export function ToggleSwitch({ label }: { label: string }) {
  const [checked, setChecked] = useState(false);
  const id = useId();
  const labelId = `${id}-label`;
  const switchId = `${id}-switch`;

  return (
    <div className="flex items-center gap-3">
      <span id={labelId} className="text-sm font-medium text-zinc-900">
        {label}
      </span>
      <button
        id={switchId}
        type="button"
        role="switch"
        aria-checked={checked}
        aria-labelledby={labelId}
        onClick={() => setChecked((prev) => !prev)}
        className={`w-11 h-6 flex items-center rounded-full p-1 transition-colors ${
          checked ? 'bg-indigo-600' : 'bg-zinc-300'
        }`}
      >
        <span
          className={`bg-white w-4 h-4 rounded-full shadow-md transform transition-transform ${
            checked ? 'translate-x-5' : 'translate-x-0'
          }`}
        />
      </button>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Multi-Input Credit Card Field</summary>

**Specification**: Build a credit card expiration input with separate month and year selects, bound to a single descriptive group label using `useId`.

```tsx
import React, { useId } from 'react';

export function ExpirationPicker() {
  const id = useId();
  const groupId = `${id}-group`;
  const monthId = `${id}-month`;
  const yearId = `${id}-year`;

  return (
    <fieldset aria-labelledby={groupId} className="border p-3 rounded">
      <legend id={groupId} className="text-sm font-semibold">
        Expiration Date
      </legend>
      <div className="flex gap-2 mt-2">
        <div>
          <label htmlFor={monthId} className="sr-only">Month</label>
          <select id={monthId} className="border p-1 rounded text-sm">
            <option value="01">01</option>
            <option value="02">02</option>
            <option value="12">12</option>
          </select>
        </div>
        <div>
          <label htmlFor={yearId} className="sr-only">Year</label>
          <select id={yearId} className="border p-1 rounded text-sm">
            <option value="2026">2026</option>
            <option value="2027">2027</option>
            <option value="2028">2028</option>
          </select>
        </div>
      </div>
    </fieldset>
  );
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Accessible Accordion with Multiple Sections</summary>

**Specification**: Build an `Accordion` component accepting a list of sections. Each section must link its header button (`aria-controls`) to its expandable content body (`aria-labelledby`) using `useId`.

```tsx
import React, { useId, useState } from 'react';

interface Section {
  title: string;
  content: string;
}

export function Accordion({ sections }: { sections: Section[] }) {
  const baseId = useId();
  const [openIndex, setOpenIndex] = useState<number | null>(0);

  return (
    <div className="divide-y border rounded-lg max-w-lg">
      {sections.map((section, idx) => {
        const headerId = `${baseId}-header-${idx}`;
        const panelId = `${baseId}-panel-${idx}`;
        const isOpen = openIndex === idx;

        return (
          <div key={idx}>
            <h3>
              <button
                id={headerId}
                type="button"
                aria-expanded={isOpen}
                aria-controls={panelId}
                onClick={() => setOpenIndex(isOpen ? null : idx)}
                className="w-full flex justify-between p-3 font-medium text-left hover:bg-zinc-50"
              >
                {section.title}
                <span>{isOpen ? '−' : '+'}</span>
              </button>
            </h3>
            <div
              id={panelId}
              role="region"
              aria-labelledby={headerId}
              hidden={!isOpen}
              className="p-3 bg-zinc-50 text-sm text-zinc-700"
            >
              {section.content}
            </div>
          </div>
        );
      })}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Combobox with Active Descendant</summary>

**Specification**: Build an accessible autocomplete combobox using `aria-activedescendant` where options have dynamic IDs derived via `useId`.

```tsx
import React, { useId, useState } from 'react';

export function AccessibleCombobox({ items }: { items: string[] }) {
  const id = useId();
  const inputId = `${id}-input`;
  const listboxId = `${id}-listbox`;
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [query, setQuery] = useState('');

  const activeOptionId = `${id}-opt-${selectedIndex}`;

  return (
    <div>
      <label htmlFor={inputId} className="block text-sm font-medium mb-1">
        Select Country
      </label>
      <input
        id={inputId}
        type="text"
        role="combobox"
        aria-autocomplete="list"
        aria-expanded="true"
        aria-controls={listboxId}
        aria-activedescendant={activeOptionId}
        value={query}
        onChange={(e) => setQuery(e.target.value)}
        className="border px-3 py-1.5 rounded w-full"
      />
      <ul id={listboxId} role="listbox" className="border mt-1 rounded divide-y max-h-40 overflow-y-auto">
        {items.map((item, index) => {
          const optId = `${id}-opt-${index}`;
          const isSelected = index === selectedIndex;
          return (
            <li
              key={item}
              id={optId}
              role="option"
              aria-selected={isSelected}
              onClick={() => setSelectedIndex(index)}
              className={`p-2 text-sm cursor-pointer ${isSelected ? 'bg-indigo-100 font-semibold' : ''}`}
            >
              {item}
            </li>
          );
        })}
      </ul>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Reusable Multi-Step Modal Dialog Wizard</summary>

**Specification**: Build a multi-step modal dialog component that manages `aria-labelledby`, `aria-describedby`, and focus containment across dynamic step transitions without losing accessibility anchors.

```tsx
import React, { useId, useState, useEffect, useRef } from 'react';

interface WizardStep {
  title: string;
  description: string;
  render: () => React.ReactNode;
}

export function AccessibleWizardModal({
  isOpen,
  onClose,
  steps,
}: {
  isOpen: boolean;
  onClose: () => void;
  steps: WizardStep[];
}) {
  const [currentStep, setCurrentStep] = useState(0);
  const baseId = useId();
  const titleId = `${baseId}-modal-title`;
  const descId = `${baseId}-modal-desc`;
  const dialogRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (isOpen) {
      dialogRef.current?.focus();
    }
  }, [isOpen, currentStep]);

  if (!isOpen) return null;

  const step = steps[currentStep];

  return (
    <div className="fixed inset-0 bg-black/50 flex items-center justify-center p-4">
      <div
        ref={dialogRef}
        role="dialog"
        aria-modal="true"
        aria-labelledby={titleId}
        aria-describedby={descId}
        tabIndex={-1}
        className="bg-white rounded-xl p-6 max-w-lg w-full shadow-2xl focus:outline-none"
      >
        <h2 id={titleId} className="text-xl font-bold text-zinc-900">
          {step.title}
        </h2>
        <p id={descId} className="text-sm text-zinc-600 mt-1 mb-4">
          {step.description}
        </p>

        <div className="my-4">{step.render()}</div>

        <div className="flex justify-between mt-6">
          <button
            type="button"
            disabled={currentStep === 0}
            onClick={() => setCurrentStep((c) => c - 1)}
            className="px-4 py-2 text-sm border rounded disabled:opacity-50"
          >
            Back
          </button>
          {currentStep < steps.length - 1 ? (
            <button
              type="button"
              onClick={() => setCurrentStep((c) => c + 1)}
              className="px-4 py-2 text-sm bg-indigo-600 text-white rounded"
            >
              Next
            </button>
          ) : (
            <button
              type="button"
              onClick={onClose}
              className="px-4 py-2 text-sm bg-green-600 text-white rounded"
            >
              Complete
            </button>
          )}
        </div>
      </div>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): SSR-Safe Micro-Frontend Host Container with Dynamic Prefixes</summary>

**Specification**: Implement a host loader pattern that isolates React sub-trees dynamically mounted into disparate DOM slots, generating unique ID scopes that prevent DOM clashing even during concurrent streaming hydration.

```tsx
import React, { createContext, useContext, useId, type ReactNode } from 'react';

interface ScopeContextValue {
  scopeId: string;
}

const ScopeContext = createContext<ScopeContextValue | null>(null);

export function MicroFrontendScope({
  appName,
  children,
}: {
  appName: string;
  children: ReactNode;
}) {
  const rootId = useId();
  const scopeId = `${appName}-${rootId}`;

  return (
    <ScopeContext.Provider value={{ scopeId }}>
      <section data-micro-app={appName} data-scope={scopeId}>
        {children}
      </section>
    </ScopeContext.Provider>
  );
}

export function useScopedId(subName: string): string {
  const context = useContext(ScopeContext);
  const localId = useId();

  if (!context) {
    return `${localId}-${subName}`;
  }

  return `${context.scopeId}-${localId}-${subName}`;
}

export function ScopedButton({ actionLabel }: { actionLabel: string }) {
  const buttonId = useScopedId('btn');
  const tipId = useScopedId('tip');

  return (
    <div className="inline-block relative">
      <button id={buttonId} aria-describedby={tipId} className="px-3 py-1 bg-zinc-800 text-white rounded text-sm">
        {actionLabel}
      </button>
      <span id={tipId} role="tooltip" className="text-xs text-zinc-500 block">
        Triggered via scoped id: {buttonId}
      </span>
    </div>
  );
}
```
</details>
