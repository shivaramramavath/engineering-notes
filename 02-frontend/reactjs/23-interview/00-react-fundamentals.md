# React Fundamentals — Senior Interview Questions & Deep Dives

> In-depth technical interview questions, reconciliation internals, architectural trade-offs, and edge cases covering modern React fundamentals, JSX transformation, and React 19 additions.

---

## Question 1: What actually happens during JSX compilation in React 19, and how does the new JSX transform differ from legacy `React.createElement`?

### The Core Answer
In legacy React (pre-17), JSX expressions like `<div className="box">Hello</div>` were compiled by Babel into:
```js
React.createElement('div', { className: 'box' }, 'Hello');
```
This required `import React from 'react'` in every file, produced an unnecessary nested `props.children` array argument, and allocated overhead objects.

Starting with the modern JSX transform (and expanded in React 19), JSX compiles to functions from `react/jsx-runtime` or `react/jsx-dev-runtime`:
```js
import { jsx as _jsx } from 'react/jsx-runtime';
_jsx('div', { className: 'box', children: 'Hello' });
```

### Critical Architectural Nuances
1. **Key Extraction Separation**: In the modern runtime, `key` is passed as a separate third parameter `_jsx(type, props, key)` rather than embedded inside `props`. This allows React to avoid inspecting or cloning the props object to extract keys.
2. **Prop Immutability**: Objects returned by `_jsx` have a frozen or optimized shape depending on development/production mode.
3. **Automatic Import Injection**: Compilers like Vite (esbuild/Babel) and SWC automatically inject the import, eliminating bundle overhead when `React` namespace is not otherwise needed.

```
[ JSX Syntax ] ──(Modern Transform: SWC/Babel)──> _jsx(Component, { id: 1 }, "unique-key")
                                                              │
                                                              ▼
                                                   [ ReactElement Object ]
                                                   { $$typeof: Symbol(react.transitional.element),
                                                     type, key, props, ref }
```

---

## Question 2: Why does React use `Symbol.for('react.transitional.element')` or `Symbol.for('react.element')` in `$$typeof`?

### The Core Answer
The `$$typeof` property is an essential **security mechanism** designed to protect React applications against **Cross-Site Scripting (XSS)** vulnerabilities involving arbitrary JSON injection.

### Attack Vector Explained
Suppose a server returns user-controlled data directly as JSON, and a naive React component renders it directly:
```json
{
  "title": {
    "type": "div",
    "props": {
      "dangerouslySetInnerHTML": {
        "__html": "<img src=x onerror=stealCookies() />"
      }
    }
  }
}
```
If React only checked whether an object has `type` and `props` to treat it as a valid element, malicious JSON stored in a database could trick the renderer into executing arbitrary scripts.

### The Defense
Because JSON cannot serialize JavaScript `Symbol` primitives (`JSON.stringify({ sym: Symbol('foo') })` drops symbols, and `JSON.parse` cannot instantiate them), React validates:
```js
if (element.$$typeof !== Symbol.for('react.transitional.element')) {
  throw new Error('Invalid element');
}
```
An attacker cannot craft a genuine `Symbol` through JSON ingestion from a remote REST/GraphQL payload.

---

## Question 3: How does React's Reconciliation Algorithm (Diffing) achieve $O(n)$ complexity instead of the general tree edit distance of $O(n^3)$?

### The Core Answer
The general minimum edit distance problem between two arbitrary trees has a theoretical complexity of $O(n^3)$ (Levenshtein-style tree matching). For 1,000 nodes, $1000^3 = 1,000,000,000$ comparisons would completely freeze the browser frame rate.

React achieves linear $O(n)$ time by introducing **two heuristic assumptions**:
1. **Different Element Types Produce Different Trees**: If `<div>` changes to `<span>`, React does not attempt to diff attributes or children. It unmounts the entire old subtree (firing cleanup effects) and mounts the new one from scratch.
2. **Keys Identify Stable Identity Across Renders**: For lists of sibling elements, child items with unique, consistent keys are matched across renders, allowing insertions, deletions, and moves to be detected in a single linear pass using a hash map lookup.

---

## Question 4: What are the catastrophic failure modes of using array indices as `key` props?

### The Failure Scenarios
1. **State Preservation Bug in Uncontrolled Inputs**:
   If an array `['Apple', 'Banana', 'Cherry']` has items with inputs, and `'Apple'` is deleted from the beginning:
   - Index `0` now points to `'Banana'`, index `1` points to `'Cherry'`.
   - React matches the old index `0` component instance with the new index `0`. The internal DOM state (e.g., text typed into input, checked status) of `'Apple'` remains in the DOM node now labeled `'Banana'`.
2. **Animation & Transition Breakage**:
   Exit animations will animate the last element out instead of the deleted item because the indices shift downward.
3. **Severe De-optimization**:
   Prepending an item causes every subsequent sibling index to increment (`0 -> 1`, `1 -> 2`), forcing React to re-render and patch every single DOM node in the list rather than executing a single DOM insertion.

---

## Question 5: How does React 19 eliminate `forwardRef` and `defaultProps`?

### Ref as a Regular Prop
In React 18 and earlier, `ref` was reserved and stripped from `props`:
```tsx
// Legacy React 18
const Input = forwardRef<HTMLInputElement, InputProps>((props, ref) => {
  return <input ref={ref} {...props} />;
});
```
In React 19, `ref` is passed directly in the component's `props` parameter:
```tsx
// Modern React 19
interface InputProps extends React.ComponentProps<'input'> {
  label: string;
}

export function Input({ label, ref, ...props }: InputProps) {
  return (
    <label>
      {label}
      <input ref={ref} {...props} />
    </label>
  );
}
```

### Deprecation of `defaultProps`
Function components should use native ES6 default argument destructuring instead of `Component.defaultProps`:
```tsx
// Clean, standard TypeScript/ES6
export function Card({ variant = 'elevated', elevation = 2 }: CardProps) {
  /* ... */
}
```
This removes extra property lookups inside the React reconciler and improves compilation speed and static typing.
