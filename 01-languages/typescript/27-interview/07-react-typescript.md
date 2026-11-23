# Module 27: React & TypeScript Interview Mastery

> **Topic Overview**: Elite React + TypeScript interview questions covering prop variance, generic components, forwardRef typing, discriminated union state, custom hooks, and event handlers.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Interview & Preparation |
| **Target Level** | Senior to Staff Frontend Engineer |
| **TypeScript Baseline** | TypeScript 5.0 - 5.8+ & React 18/19 |
| **Focus Areas** | React Types, Generics, Component Polymorphism, Context API |

---

## Key Interview Questions

### Q1: Why is `React.FC` (or `React.FunctionComponent`) widely discouraged in modern TypeScript React applications?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

Historically (React <18), `React.FC` automatically injected `children?: ReactNode` into every component's props whether intended or not, weakening type safety:
```tsx
// Anti-pattern in legacy code:
const Card: React.FC<{ title: string }> = ({ title }) => <div>{title}</div>;
<Card title="Hello"><span>Extra children not handled!</span></Card>; // Passes silently!
```
Other drawbacks:
1. Breaks Generics: You cannot easily define generic components using `React.FC<Props<T>>`.
2. Clutters IDE tooltips and component signatures.
3. Incompatible with standard function declarations.

**Modern Standard**:
Explicitly type function arguments and explicitly opt-in to `React.ReactNode` for children:
```tsx
interface CardProps {
  title: string;
  children?: React.ReactNode;
}
export function Card({ title, children }: CardProps) {
  return <div className="card"><h3>{title}</h3>{children}</div>;
}
```
</details>

---

### Q2: How do you build a fully type-safe polymorphic component (the `as` prop pattern) in React with TypeScript?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

A polymorphic component allows consumers to render a custom HTML element or React component while automatically inheriting all valid HTML attributes for that specific element without runtime or compile-time collisions.

```tsx
import React, { ElementType, ComponentPropsWithoutRef } from "react";

type AsProp<C extends ElementType> = {
  as?: C;
};

type PolymorphicProps<C extends ElementType, P = {}> = P &
  AsProp<C> &
  Omit<ComponentPropsWithoutRef<C>, keyof (AsProp<C> & P)>;

interface BaseButtonProps {
  variant?: "primary" | "secondary";
}

export function Button<C extends ElementType = "button">({
  as,
  variant = "primary",
  ...restProps
}: PolymorphicProps<C, BaseButtonProps>) {
  const Component = as || "button";
  return <Component className={`btn-${variant}`} {...restProps} />;
}

// Verification:
<Button variant="primary" onClick={(e) => console.log(e.clientX)}>Submit</Button>; // type button
<Button as="a" href="https://example.com" target="_blank">Link</Button>; // valid anchor
// @ts-expect-error - 'target' is not a valid attribute on 'button'
<Button target="_blank">Invalid</Button>;
```
</details>

---

### Q3: How do you model complex React component state using Discriminated Unions to make invalid states unrepresentable?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

Inexperienced developers often represent async request state with multiple independent flags:
```typescript
// ANTI-PATTERN: Allows invalid state like { isLoading: true, error: Error, data: [...] }
interface State {
  isLoading: boolean;
  data: string[] | null;
  error: Error | null;
}
```
**Senior Pattern: Discriminated Union**:
```typescript
type AsyncState<T> =
  | { status: "idle" }
  | { status: "loading" }
  | { status: "success"; data: T }
  | { status: "error"; error: Error };

function renderView(state: AsyncState<string[]>) {
  switch (state.status) {
    case "idle":
      return <div>Ready to load</div>;
    case "loading":
      return <div className="spinner">Loading...</div>;
    case "error":
      return <div className="error">{state.error.message}</div>;
    case "success":
      return <ul>{state.data.map(item => <li key={item}>{item}</li>)}</ul>;
  }
}
```
In `case "success"`, TS narrows `state` to `{ status: "success"; data: T }`, guaranteeing `data` cannot be `null` without defensive assertions.
</details>

---

### Q4: How do you properly type custom hooks with return value tuple inference (`as const`) vs object returns?
<details>
<summary><strong>Answer & Technical Analysis</strong></summary>

When a hook returns a tuple (like `useState`), TypeScript infers the array as a union: `(boolean | (() => void))[]`. Indexing into it causes type ambiguity.

```typescript
import { useState, useCallback } from "react";

// Problematic inference:
function useToggleBad(initial = false) {
  const [state, setState] = useState(initial);
  const toggle = useCallback(() => setState(s => !s), []);
  return [state, toggle]; // Inferred as (boolean | (() => void))[]
}

// Correct Pattern 1: 'as const'
function useToggleTuple(initial = false) {
  const [state, setState] = useState(initial);
  const toggle = useCallback(() => setState(s => !s), []);
  return [state, toggle] as const; // Inferred as readonly [boolean, () => void]
}

// Correct Pattern 2: Return Interface (recommended for >2 values)
interface UseToggleReturn {
  value: boolean;
  toggle: () => void;
  setTrue: () => void;
  setFalse: () => void;
}
```
</details>\n