# React Synthetic Events and DOM Event Typing in TypeScript

> Typing React events in TypeScript: SyntheticEvent vs native Event, MouseEvent, ChangeEvent, FormEvent, KeyboardEvent, event target vs currentTarget, and custom event handlers.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Intermediate |
| **Prerequisites** | [02-functions/00-function-types.md](../02-functions/00-function-types.md), [19-react-and-frontend/00-component-props-and-children.md](./00-component-props-and-children.md) |
| **TypeScript Baseline** | TS 5.0+, React 19 / @types/react 19 |
| **Tested tsconfig** | `"strict": true, "jsx": "react-jsx", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In browser JavaScript, event handling differs across browsers and historical DOM specifications. React normalizes browser events through **Synthetic Events**—a cross-browser wrapper around the browser’s native event system.

When using TypeScript with React event handlers, developers frequently encounter type friction:
- Trying to access `e.target.value` on an `e: React.SyntheticEvent` results in compiler errors because `target` is typed as a generic `EventTarget` (which lacks a `.value` property).
- Confusing `e.target` (the element that triggered the event) with `e.currentTarget` (the element that attaches the event listener).
- Using untyped inline functions `(e: any) => ...`, which strips compiler safety.

TypeScript provides specialized generic event types (`React.ChangeEvent<HTMLInputElement>`, `React.MouseEvent<HTMLButtonElement>`, `React.FormEvent<HTMLFormElement>`) that give accurate autocomplete and type safety over event properties.

```
+-----------------------------------------------------------+
|                   React SyntheticEvent<T>                 |
|                                                           |
|  nativeEvent: MouseEvent / KeyboardEvent                  |
|  currentTarget: T (Guaranteed HTMLInputElement / Button)   |
|  target: EventTarget (Can be child span, icon, etc.)      |
|  preventDefault(): void                                   |
|  stopPropagation(): void                                  |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. `currentTarget` vs `target`
- **`e.currentTarget`**: The element to which the event handler is attached. In TypeScript, `currentTarget` is strictly typed to the generic parameter `T` in `React.SyntheticEvent<T>`. Accessing `e.currentTarget.value` on `ChangeEvent<HTMLInputElement>` is always safe!
- **`e.target`**: The DOM node where the event originated (due to event bubbling). If you click an icon inside a button, `target` may be the `<svg>` or `<span>`, while `currentTarget` is the `<button>`. `target` is typed broadly as `EventTarget`.

### 2. Standard React Event Handlers
| Event Type | Typical Elements | Common Handler Prop |
| :--- | :--- | :--- |
| `ChangeEvent<HTMLInputElement>` | `<input>`, `<textarea>`, `<select>` | `onChange` |
| `MouseEvent<HTMLButtonElement>` | `<button>`, `<div>`, `<a>` | `onClick`, `onMouseEnter` |
| `FormEvent<HTMLFormElement>` | `<form>` | `onSubmit` |
| `KeyboardEvent<HTMLInputElement>` | Any focusable element | `onKeyDown`, `onKeyUp` |
| `FocusEvent<HTMLInputElement>` | Inputs, buttons | `onFocus`, `onBlur` |
| `DragEvent<HTMLDivElement>` | Draggable containers | `onDragStart`, `onDrop` |

---

## 3. How It Works Internally

The `@types/react` definitions model events as generic interfaces extending `SyntheticEvent`:
```typescript
interface ChangeEvent<T = Element> extends SyntheticEvent<T> {
  target: EventTarget & T;
}
```
When you supply `HTMLInputElement` as `T`:
`e.target` is intersected with `HTMLInputElement`, giving access to `.value`, `.checked`, `.name`, and `.files`.

---

## 4. Syntax & API Reference

### Event Handler Prop Types
```typescript
import type { ChangeEventHandler, MouseEventHandler, FormEventHandler } from 'react';

// Explicit Function Handler Types:
type InputChangeHandler = ChangeEventHandler<HTMLInputElement>;
type ButtonClickHandler = MouseEventHandler<HTMLButtonElement>;
type FormSubmitHandler = FormEventHandler<HTMLFormElement>;
```

---

## 5. Practical Examples

### Example 1: Basic Input Change and Form Submit Handlers
```typescript
import { useState, type ChangeEvent, type FormEvent } from 'react';

export function LoginForm(): React.JSX.Element {
  const [email, setEmail] = useState('');

  const handleEmailChange = (e: ChangeEvent<HTMLInputElement>) => {
    setEmail(e.target.value); // Strongly typed!
  };

  const handleSubmit = (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    console.log('Submitting email:', email);
  };

  return (
    <form onSubmit={handleSubmit}>
      <input type="email" value={email} onChange={handleEmailChange} />
      <button type="submit">Log In</button>
    </form>
  );
}
```

### Example 2: Real-World Keyboard Event Filtering (Enter / Escape)
```typescript
import type { KeyboardEvent } from 'react';

export interface SearchBarProps {
  onSearch: (query: string) => void;
  onCancel: () => void;
}

export function SearchBar({ onSearch, onCancel }: SearchBarProps) {
  const handleKeyDown = (e: KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      onSearch(e.currentTarget.value);
    } else if (e.key === 'Escape') {
      e.preventDefault();
      onCancel();
    }
  };

  return <input type="text" placeholder="Search..." onKeyDown={handleKeyDown} />;
}
```

### Example 3: Edge Case - Delegated Mouse Click on Compound Element
```typescript
import type { MouseEvent } from 'react';

export function ComplexCardList() {
  const handleContainerClick = (e: MouseEvent<HTMLDivElement>) => {
    // currentTarget is HTMLDivElement (the container)
    console.log('Container clicked at:', e.currentTarget.id);

    // target could be any clicked child node:
    const clickedElement = e.target as HTMLElement;
    if (clickedElement.tagName === 'BUTTON') {
      console.log('Button clicked inside card!');
    }
  };

  return (
    <div id="card-container" onClick={handleContainerClick}>
      <div className="card">
        <h3>Card Title</h3>
        <button type="button">Delete</button>
      </div>
    </div>
  );
}
```

### Example 4: Failure Case - Type Discrepancy on Element Generics
```typescript
import type { ChangeEvent } from 'react';

function handleSelectChange(e: ChangeEvent<HTMLSelectElement>) {
  console.log(e.target.value);
}

// @ts-expect-error TS2322: Type '(e: ChangeEvent<HTMLSelectElement>) => void' is not assignable to type 'ChangeEventHandler<HTMLInputElement>'.
const input = <input type="text" onChange={handleSelectChange} />;
```

### Example 5: Idiomatic Custom Event Callback Props
```typescript
export interface SliderProps {
  value: number;
  min: number;
  max: number;
  // Idiomatic React pattern: pass parsed values in domain callbacks
  onChange: (value: number) => void;
}

export function Slider({ value, min, max, onChange }: SliderProps) {
  return (
    <input
      type="range"
      min={min}
      max={max}
      value={value}
      onChange={(e) => onChange(Number(e.target.value))}
    />
  );
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`target` vs `currentTarget` with SVGs**:
   If an icon inside a button is an SVG, clicking the icon makes `e.target` an `SVGElement`, not an `HTMLElement`. Accessing `(e.target as HTMLElement).innerText` will fail or return `undefined`. Always use `e.currentTarget` to inspect the button that owns the event listener.
2. **File Input Typing**:
   On `<input type="file" />`, `e.target.files` is typed as `FileList | null`. Always guard with `if (e.target.files?.[0])` before accessing uploaded files.
3. **Event Pooling (Obsolete in React 17+)**:
   In React 16, accessing synthetic events in asynchronous callbacks (`setTimeout`) required calling `e.persist()`. In React 17 and React 19, synthetic event pooling was completely removed, so synthetic event properties remain accessible asynchronously without `e.persist()`.
4. **Custom Synthetic Events**:
   React synthetic events are not native browser events. Passing a synthetic event to an external non-React library expecting a native DOM `Event` fails unless you pass `e.nativeEvent`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Using `any` for Event** | `(e: any) => e.target.value` | `(e: ChangeEvent<HTMLInputElement>) => ...` | Loss of autocomplete and compiler checking |
| **Using DOM `MouseEvent` Instead of React's** | `import MouseEvent from 'global'` | `import type { MouseEvent } from 'react'` | Type collision between native and React events |
| **Relying on `e.target` on Bubbled Events** | `e.target.id` on parent container | `e.currentTarget.id` | Returns child span ID instead of container ID |
| **Missing `e.preventDefault()` on Submit** | Omitting preventDefault | `e.preventDefault()` | Triggers browser full-page reload |
| **Un-guarded `files` Access** | `const file = e.target.files[0]` | `const file = e.target.files?.[0]` | Runtime `TypeError: Cannot read properties of null` |
| **Leaking Synthetic Events to DOM** | Passing React event directly to 3rd party lib | Pass `e.nativeEvent` | 3rd party lib fails on missing native methods |

---

## 8. Performance & Type-Checker Complexity

- **Inline vs Named Handlers**: Declaring inline arrow functions `onChange={(e) => setValue(e.target.value)}` allows TypeScript to contextually infer the event type automatically without manual type annotations! For 90% of handlers, contextual inference is preferred over manual type annotation.
- **Compiler Speed**: React event types check instantly with zero compilation delay.

---

## 9. Compiler Internals

When you attach an inline handler:
```typescript
<input onChange={(e) => console.log(e.target.value)} />
```
The compiler's `checkExpression` inspects the expected property type of `onChange` in `JSX.IntrinsicElements['input']`, which is `ChangeEventHandler<HTMLInputElement>`. TypeScript performs **Contextual Typing** on the parameter `e`, automatically inferring it as `React.ChangeEvent<HTMLInputElement>`.

---

## 10. Best Practices & Design Guidelines

1. **Rely on Contextual Typing for Inline Handlers**: Write `onChange={(e) => ...}` and let TypeScript infer the event.
2. **Use `e.currentTarget` for Listener Attributes**: Avoid bubbling discrepancies.
3. **Wrap Primitive Values in Custom Component Props**: Custom components should expose `onChange: (val: string) => void` rather than raw DOM event objects.
4. **Import Types Explicitly**: Use `import type { MouseEvent } from 'react'` to avoid shadowing global DOM events.

---

## 11. Real-World Case Study

### File Drag-and-Drop Dropzone Component
A production file uploader handling dragging, drag-leave, and drop events with strict file validation:

```typescript
import { useState, type DragEvent } from 'react';

export interface FileDropzoneProps {
  onFilesAccepted: (files: File[]) => void;
  maxFiles?: number;
}

export function FileDropzone({ onFilesAccepted, maxFiles = 5 }: FileDropzoneProps): React.JSX.Element {
  const [isDragOver, setIsDragOver] = useState(false);

  const handleDragOver = (e: DragEvent<HTMLDivElement>) => {
    e.preventDefault(); // Necessary to allow drop
    setIsDragOver(true);
  };

  const handleDragLeave = (e: DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    setIsDragOver(false);
  };

  const handleDrop = (e: DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    setIsDragOver(false);

    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      const accepted = Array.from(e.dataTransfer.files).slice(0, maxFiles);
      onFilesAccepted(accepted);
    }
  };

  return (
    <div
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
      className={`dropzone ${isDragOver ? 'active' : ''}`}
    >
      <p>Drag and drop files here</p>
    </div>
  );
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between `React.ChangeEvent` and native DOM `Event`?</summary>

`React.ChangeEvent` is a SyntheticEvent providing cross-browser normalization across different browser engines. In addition, `React.ChangeEvent` is generic over the target element (`ChangeEvent<HTMLInputElement>`), whereas the native DOM `Event` types `target` as generic `EventTarget`.
</details>

<details>
<summary>2. What is the difference between `e.target` and `e.currentTarget` in React?</summary>

`e.currentTarget` is always the element to which the event handler was attached. `e.target` is the actual element that triggered the event, which could be any nested child element due to event bubbling. In TypeScript, `e.currentTarget` is typed to the element specified in the synthetic event generic parameter.
</details>

<details>
<summary>3. Why doesn't `e.target.value` compile on a generic `React.SyntheticEvent`?</summary>

Because generic `SyntheticEvent` types `target` as `EventTarget`. Not all DOM event targets have a `.value` property (for example, `window`, `document`, or `div`). You must use `React.ChangeEvent<HTMLInputElement>` or inspect `e.currentTarget.value`.
</details>

<details>
<summary>4. What was Event Pooling in React and does it still exist?</summary>

Event pooling was a performance optimization in React 16 and earlier where synthetic event instances were reused across events for memory efficiency, requiring `e.persist()` to read properties asynchronously. It was completely removed in React 17+.
</details>

<details>
<summary>5. How does Contextual Typing simplify event handler declarations in React?</summary>

When an event handler is written inline (e.g. `<button onClick={(e) => ...} />`), TypeScript infers the type of `e` from the component's prop definition, eliminating the need to manually annotate `(e: React.MouseEvent<HTMLButtonElement>)`.
</details>

<details>
<summary>6. How do you type a file input change handler?</summary>

```typescript
const handleFile = (e: React.ChangeEvent<HTMLInputElement>) => {
  const file = e.target.files?.[0];
};
```
</details>

<details>
<summary>7. What is the return type of a React event handler?</summary>

Event handlers in React typically return `void`. Returning a boolean (like `return false` in old jQuery) does NOT prevent default behavior in React; you must explicitly call `e.preventDefault()`.
</details>

<details>
<summary>8. How do you pass the native browser event to an external library?</summary>

By reading `e.nativeEvent`, which contains the underlying browser DOM event.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Typed Checkbox Toggle Handler</summary>

**Problem:** Type a toggle handler for a checkbox `<input type="checkbox" />` that reads `.checked`.

**Solution:**
```typescript
import type { ChangeEvent } from 'react';

export function handleToggle(e: ChangeEvent<HTMLInputElement>): void {
  console.log('Checked status:', e.target.checked);
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Typed Mouse Coordinate Logger</summary>

**Problem:** Write a click handler on a `<div>` that logs `clientX` and `clientY`.

**Solution:**
```typescript
import type { MouseEvent } from 'react';

export function logCoordinates(e: MouseEvent<HTMLDivElement>): void {
  console.log(`Clicked at ${e.clientX}, ${e.clientY}`);
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Generic Input Handler Factory</summary>

**Problem:** Write a higher-order function that generates a change handler for a state setter function.

**Solution:**
```typescript
import type { ChangeEvent } from 'react';

export function createInputBinder<T extends string>(setter: (val: T) => void) {
  return (e: ChangeEvent<HTMLInputElement | HTMLTextAreaElement>) => {
    setter(e.target.value as T);
  };
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Number Input Clamping Handler</summary>

**Problem:** Create a change handler for `<input type="number" />` that parses the value and clamps it between a min and max.

**Solution:**
```typescript
import type { ChangeEvent } from 'react';

export function createClampedHandler(min: number, max: number, onValid: (val: number) => void) {
  return (e: ChangeEvent<HTMLInputElement>) => {
    const num = e.target.valueAsNumber;
    if (!isNaN(num)) {
      onValid(Math.max(min, Math.min(max, num)));
    }
  };
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Multi-Field Form Binder</summary>

**Problem:** Implement a generic `useFormFields<T>` helper that returns a change handler that extracts the input's `name` attribute and updates the corresponding property in state.

**Solution:**
```typescript
import { useState, type ChangeEvent } from 'react';

export function useFormFields<T extends Record<string, string>>(initial: T) {
  const [fields, setFields] = useState<T>(initial);

  const handleChange = (e: ChangeEvent<HTMLInputElement | HTMLTextAreaElement>) => {
    const { name, value } = e.target;
    setFields((prev) => ({ ...prev, [name]: value }));
  };

  return [fields, handleChange] as const;
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Custom Window Event Listener Hook</summary>

**Problem:** Write a type-safe `useEventListener` hook that accepts any native window event name from `WindowEventMap` and accurately types the callback.

**Solution:**
```typescript
import { useEffect } from 'react';

export function useWindowListener<K extends keyof WindowEventMap>(
  event: K,
  handler: (e: WindowEventMap[K]) => void
): void {
  useEffect(() => {
    window.addEventListener(event, handler);
    return () => window.removeEventListener(event, handler);
  }, [event, handler]);
}
```
</details>
