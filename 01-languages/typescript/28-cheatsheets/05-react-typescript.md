# Module 28: React & TypeScript Cheatsheet

> **Quick Reference**: Essential React 18/19 + TypeScript cheat sheet: component props, hooks, event handlers, DOM refs, and context typing.

---

## 1. Component Props & Children

```tsx
import React, { ReactNode, CSSProperties } from "react";

export interface ButtonProps {
  label: string;
  onClick: (event: React.MouseEvent<HTMLButtonElement>) => void;
  variant?: "primary" | "secondary";
  disabled?: boolean;
  style?: CSSProperties;
  children?: ReactNode; // Standard type for renderable React children
}

export function Button({ label, onClick, variant = "primary", disabled = false, children }: ButtonProps) {
  return (
    <button className={`btn-${variant}`} onClick={onClick} disabled={disabled}>
      {label}
      {children}
    </button>
  );
}
```

---

## 2. Standard React Event Handlers

| Event Type | React Type Definition |
|---|---|
| Button Click | `React.MouseEvent<HTMLButtonElement>` |
| Form Submit | `React.FormEvent<HTMLFormElement>` |
| Input Text Change | `React.ChangeEvent<HTMLInputElement>` |
| Keyboard Press | `React.KeyboardEvent<HTMLInputElement>` |
| Element Focus / Blur | `React.FocusEvent<HTMLInputElement>` |

---

## 3. Typing Hooks

```tsx
import { useState, useRef, useReducer, useEffect } from "react";

// useState with explicit union
const [user, setUser] = useState<User | null>(null);

// useRef for DOM element (read-only ref)
const inputRef = useRef<HTMLInputElement>(null);
// useRef for mutable instance variable (mutable ref)
const timerRef = useRef<number | null>(null);

// useReducer with Discriminated Action Union
type Action =
  | { type: "increment"; amount: number }
  | { type: "reset" };

function counterReducer(state: number, action: Action): number {
  switch (action.type) {
    case "increment": return state + action.amount;
    case "reset": return 0;
  }
}
```

---

## 4. Generic Components

```tsx
interface ListProps<T> {
  items: T[];
  renderItem: (item: T, index: number) => React.ReactNode;
  keyExtractor: (item: T) => string;
}

export function List<T>({ items, renderItem, keyExtractor }: ListProps<T>) {
  return (
    <ul>
      {items.map((item, idx) => (
        <li key={keyExtractor(item)}>{renderItem(item, idx)}</li>
      ))}
    </ul>
  );
}
```\n