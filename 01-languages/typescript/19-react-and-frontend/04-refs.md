# React Refs and Imperative Handles Typing in TypeScript

> Comprehensive typing for React refs: useRef mutable containers vs DOM element refs, RefObject vs MutableRefObject, forwardRef and React 19 native ref props, and useImperativeHandle.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md), [19-react-and-frontend/00-component-props-and-children.md](./00-component-props-and-children.md) |
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

In React, state updates trigger re-renders. However, applications frequently require:
1. **Accessing underlying DOM nodes**: Measuring element dimensions, triggering focus, scrolling to an anchor, or calling `.play()` on an HTML5 `<video>`.
2. **Storing mutable instance variables**: Retaining timer IDs, previous values, or WebSocket handles without triggering a component re-render when modified.

`useRef` serves both purposes, but its typing changes subtly depending on its initial argument:
- **DOM Ref**: `const ref = useRef<HTMLInputElement>(null);` -> `ref.current` is managed by React.
- **Mutable Variable Ref**: `const ref = useRef<number>(0);` -> `ref.current` is directly mutable by your code.

In **React 19**, ref handling has been vastly streamlined:
- The clunky `forwardRef` higher-order function is deprecated for standard components; `ref` is passed as a regular component prop!
- `useImperativeHandle` cleanly types custom exposed public interfaces.

```
+-----------------------------------------------------------+
|                   React 19 Ref Handling                   |
|                                                           |
|   interface SearchInputProps {                            |
|     ref?: React.Ref<HTMLInputElement>; // Native in R19   |
|   }                                                       |
|                                                           |
|   export function SearchInput({ ref }: SearchInputProps) {|
|     return <input ref={ref} type="search" />;             |
|   }                                                       |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. `RefObject<T>` vs `MutableRefObject<T>` (Pre-React 19)
In React 18:
- When initialized with `null`: `useRef<HTMLInputElement>(null)` returns `RefObject<HTMLInputElement>`, where `.current` is `readonly current: HTMLInputElement | null`. React modifies `.current` when mounting/unmounting, but your code cannot directly reassign `ref.current = otherElement`.
- When initialized with a value or without `null`: `useRef<number>(0)` returns `MutableRefObject<number>`, where `.current` is mutable (`current: number`).

In React 19 / modern `@types/react`:
`RefObject<T>` simply defines `current: T | null`.

### 2. Forwarding Refs (React 18 vs React 19)
- **React 18**: Requires `forwardRef<RefType, PropsType>((props, ref) => ...)` which swaps the order of generic parameters (`Ref` first, `Props` second).
- **React 19**: Accept `ref?: React.Ref<Element>` directly in component props!

### 3. `useImperativeHandle`
Restricts what parent components can access through a ref, exposing only a tailored API instead of the raw DOM node:
```typescript
export interface CustomInputHandle {
  focus(): void;
  clear(): void;
}
```

---

## 3. How It Works Internally

When a JSX tag renders `<input ref={myRef} />`:
1. During the commit phase, React checks if `ref` is a callback function or an object with a `current` property.
2. If an object, React assigns the mounted DOM node to `ref.current`.
3. When the element unmounts, React sets `ref.current = null`.

---

## 4. Syntax & API Reference

### Typing DOM Elements for Refs
```typescript
import { useRef, type RefObject } from 'react';

const inputRef = useRef<HTMLInputElement>(null);
const divRef = useRef<HTMLDivElement>(null);
const canvasRef = useRef<HTMLCanvasElement>(null);
const videoRef = useRef<HTMLVideoElement>(null);
```

---

## 5. Practical Examples

### Example 1: Basic Input Focus DOM Ref
```typescript
import { useRef, type ReactElement } from 'react';

export function AutoFocusInput(): ReactElement {
  const inputRef = useRef<HTMLInputElement>(null);

  const handleClick = () => {
    // inputRef.current is guarded
    inputRef.current?.focus();
  };

  return (
    <div>
      <input ref={inputRef} type="text" placeholder="Focus me..." />
      <button onClick={handleClick}>Focus Input</button>
    </div>
  );
}
```

### Example 2: Real-World Mutable Timer Ref (Interval Management)
```typescript
import { useState, useRef, useEffect } from 'react';

export function Stopwatch() {
  const [seconds, setSeconds] = useState(0);
  const timerRef = useRef<NodeJS.Timeout | null>(null);

  const start = () => {
    if (timerRef.current !== null) return;
    timerRef.current = setInterval(() => {
      setSeconds((s) => s + 1);
    }, 1000);
  };

  const stop = () => {
    if (timerRef.current !== null) {
      clearInterval(timerRef.current);
      timerRef.current = null;
    }
  };

  useEffect(() => {
    return () => stop(); // Cleanup on unmount
  }, []);

  return (
    <div>
      <p>Elapsed: {seconds}s</p>
      <button onClick={start}>Start</button>
      <button onClick={stop}>Stop</button>
    </div>
  );
}
```

### Example 3: Edge Case - `useImperativeHandle` with Typed Public API
```typescript
import { useImperativeHandle, useRef, type Ref, type ReactElement } from 'react';

export interface VideoPlayerHandle {
  play(): void;
  pause(): void;
  reset(): void;
}

export interface VideoPlayerProps {
  src: string;
  ref?: Ref<VideoPlayerHandle>;
}

export function VideoPlayer({ src, ref }: VideoPlayerProps): ReactElement {
  const videoRef = useRef<HTMLVideoElement>(null);

  useImperativeHandle(ref, () => ({
    play: () => videoRef.current?.play(),
    pause: () => videoRef.current?.pause(),
    reset: () => {
      if (videoRef.current) {
        videoRef.current.pause();
        videoRef.current.currentTime = 0;
      }
    }
  }));

  return <video ref={videoRef} src={src} />;
}
```

### Example 4: Failure Case - Missing Initial `null` on DOM Ref
```typescript
import { useRef } from 'react';

// Calling useRef<HTMLInputElement>() without null:
const badRef = useRef<HTMLInputElement>();

// @ts-expect-error TS2322: Type 'MutableRefObject<HTMLInputElement | undefined>' is not assignable to type 'LegacyRef<HTMLInputElement> | undefined'.
const input = <input ref={badRef} />;
```

### Example 5: Idiomatic Ref Callback (Measuring DOM Elements)
```typescript
import { useState, useCallback } from 'react';

export function MeasuredBox() {
  const [height, setHeight] = useState(0);

  // Ref callbacks are invoked whenever the node mounts/unmounts
  const measuredRef = useCallback((node: HTMLDivElement | null) => {
    if (node !== null) {
      setHeight(node.getBoundingClientRect().height);
    }
  }, []);

  return (
    <div ref={measuredRef} style={{ padding: 20 }}>
      <p>My rendered height is: {height}px</p>
    </div>
  );
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Always Pass `null` for DOM Refs**:
   Writing `useRef<HTMLDivElement>()` (without `null`) causes type mismatches because the initial value is `undefined`, whereas React expects the ref to hold `HTMLDivElement | null`. Always write `useRef<HTMLDivElement>(null)`.
2. **Accessing `.current` During Render**:
   Do NOT read or write `ref.current` during rendering (`const width = ref.current.clientWidth` in the component body). In concurrent React, renders can be paused or discarded. Always access DOM refs inside `useEffect` or event handlers.
3. **Ref Callbacks vs Ref Objects**:
   When you need to react to a DOM node being mounted (e.g. measuring its size), `ref.current` inside `useEffect` might miss initial measurements. Use a callback ref (`useCallback((node) => ...)`) instead.
4. **`forwardRef` Generic Order in React 18**:
   In React 18, `forwardRef<RefType, PropsType>` places the ref type first and props second, the reverse of standard component signatures. React 19 eliminates this confusion entirely by accepting `ref` as a normal prop.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Omitting `null` in DOM Ref** | `useRef<HTMLInputElement>()` | `useRef<HTMLInputElement>(null)` | TS2322 assignability failure on `<input ref={...}>` |
| **Reading Ref During Render** | `const w = ref.current?.offsetWidth` | Read inside `useEffect` or handler | Stale / undefined values in concurrent mode |
| **Using State When Ref Suffices** | `useState(timerId)` | `useRef(timerId)` | Unnecessary re-renders on timer updates |
| **Exposing Raw DOM Node** | Passing full HTMLAudioElement to parent | `useImperativeHandle` with limited methods | Leaky abstraction; violates encapsulation |
| **Reassigning DOM `ref.current`** | `ref.current = document.createElement(...)` | Let React manage DOM assignment | Overwrites React's internal reference |
| **Missing Ref Cleanup** | Storing listener in ref without cleanup | Clean up timers and handles in unmount | Memory leaks |

---

## 8. Performance & Type-Checker Complexity

- **Zero Re-renders**: Mutating `ref.current` does not trigger a re-render.
- **Type Checking**: Ref interfaces are lightweight objects containing a single `current` property; compiler check latency is zero.

---

## 9. Compiler Internals

When type checking `<div ref={divRef} />`:
The compiler matches the `ref` prop against `React.Ref<HTMLDivElement>`.
`React.Ref<T>` is defined as `RefCallback<T> | RefObject<T> | null`.
Because `divRef` is `RefObject<HTMLDivElement>`, assignability succeeds.

---

## 10. Best Practices & Design Guidelines

1. **Always Initialize DOM Refs with `null`**: `useRef<T>(null)`.
2. **Never Read/Write Refs in the Render Function**: Keep rendering pure.
3. **Use React 19 Native Ref Props**: Pass `ref` directly as a prop in modern codebases.
4. **Use `useImperativeHandle` for Reusable Custom Widgets**: Restrict child surface area.

---

## 11. Real-World Case Study

### Auto-Resizing Expanding Textarea
A textarea that automatically recalculates its height based on input content using ref callbacks:

```typescript
import { useCallback, type ChangeEvent } from 'react';

export interface AutoResizeTextareaProps {
  value: string;
  onChange: (e: ChangeEvent<HTMLTextAreaElement>) => void;
  placeholder?: string;
}

export function AutoResizeTextarea({ value, onChange, placeholder }: AutoResizeTextareaProps) {
  const textareaRef = useCallback((node: HTMLTextAreaElement | null) => {
    if (node) {
      node.style.height = 'auto';
      node.style.height = `${node.scrollHeight}px`;
    }
  }, [value]);

  return (
    <textarea
      ref={textareaRef}
      value={value}
      onChange={onChange}
      placeholder={placeholder}
      rows={1}
      style={{ overflow: 'hidden', resize: 'none' }}
    />
  );
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between a DOM ref and a mutable value ref in React?</summary>

A **DOM ref** points to an actual HTML DOM node in the browser (`useRef<HTMLInputElement>(null)`); React assigns `.current` when the element mounts and clears it when it unmounts. A **mutable value ref** (`useRef(0)`) is an arbitrary instance container that holds any JavaScript value across renders without triggering a re-render when mutated.
</details>

<details>
<summary>2. Why does `useRef<HTMLInputElement>()` fail when assigned to `<input ref={ref} />`?</summary>

When called without arguments, `useRef` infers `current: HTMLInputElement | undefined`. The React `ref` attribute expects `RefObject<HTMLInputElement>`, which requires `current` to be `HTMLInputElement | null`. Passing `null` as the initial argument aligns the types.
</details>

<details>
<summary>3. Why should you avoid reading or mutating `ref.current` during component rendering?</summary>

Rendering must be pure. In Concurrent React, React can render components multiple times before committing, or discard rendered trees entirely. Mutating or reading refs during rendering produces unpredictable visual tearing and bugs. Refs should only be accessed in `useEffect` or event handlers.
</details>

<details>
<summary>4. How does React 19 change the way refs are forwarded to child components?</summary>

In React 19, `forwardRef` is no longer needed. Function components can accept `ref` directly as a regular prop in their props parameter list: `function MyInput({ ref, ...props }: Props)`.
</details>

<details>
<summary>5. What is `useImperativeHandle` and when should it be used?</summary>

`useImperativeHandle` customizes the instance value that is exposed to parent components when using `ref`. Instead of exposing the entire raw DOM node, it exposes a restricted set of custom methods (e.g. `focus()`, `scrollIntoView()`, `reset()`), preserving encapsulation.
</details>

<details>
<summary>6. What is a Ref Callback and when is it preferred over `useRef`?</summary>

A ref callback is a function passed to `ref`: `<div ref={(node) => ...} />`. React calls it with the DOM node when mounting and `null` when unmounting. It is preferred when you need to execute code immediately upon DOM attachment (e.g. calculating bounding boxes or attaching third-party libraries).
</details>

<details>
<summary>7. How do you type a ref that could be attached to one of multiple elements?</summary>

Use a union: `useRef<HTMLInputElement | HTMLTextAreaElement>(null)`.
</details>

<details>
<summary>8. How do you store the previous render's value of a prop using a ref?</summary>

```typescript
function usePrevious<T>(value: T): T | undefined {
  const ref = useRef<T | undefined>(undefined);
  useEffect(() => { ref.current = value; }, [value]);
  return ref.current;
}
```
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Typed Input Focus Ref</summary>

**Problem:** Create a function component that focuses an input upon button click using a typed ref.

**Solution:**
```typescript
import { useRef } from 'react';

export function FocusButton() {
  const inputRef = useRef<HTMLInputElement>(null);
  return (
    <>
      <input ref={inputRef} />
      <button onClick={() => inputRef.current?.focus()}>Focus</button>
    </>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Canvas Context Getter Ref</summary>

**Problem:** Type a ref attached to a `<canvas>` element and safely obtain its 2D rendering context.

**Solution:**
```typescript
import { useRef, useEffect } from 'react';

export function CanvasWidget() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const ctx = canvasRef.current?.getContext('2d');
    if (ctx) ctx.fillRect(0, 0, 100, 100);
  }, []);
  return <canvas ref={canvasRef} width={200} height={200} />;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Forwarding Ref in React 18</summary>

**Problem:** Type a `CustomInput` component using React 18's `forwardRef`.

**Solution:**
```typescript
import { forwardRef, type ComponentPropsWithoutRef } from 'react';

export interface CustomInputProps extends ComponentPropsWithoutRef<'input'> {
  label: string;
}

export const CustomInput = forwardRef<HTMLInputElement, CustomInputProps>(
  ({ label, ...props }, ref) => (
    <label>
      {label}
      <input ref={ref} {...props} />
    </label>
  )
);
CustomInput.displayName = 'CustomInput';
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Exposed Handle Interface</summary>

**Problem:** Use `useImperativeHandle` to expose a `selectText()` method from a child input component.

**Solution:**
```typescript
import { useImperativeHandle, useRef, type Ref } from 'react';

export interface SelectHandle { selectText(): void; }

export function SelectableInput({ ref }: { ref?: Ref<SelectHandle> }) {
  const innerRef = useRef<HTMLInputElement>(null);
  useImperativeHandle(ref, () => ({
    selectText: () => innerRef.current?.select()
  }));
  return <input ref={innerRef} defaultValue="Select me" />;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Merging Two Refs (Inner Ref and Forwarded Ref)</summary>

**Problem:** Implement a utility `mergeRefs(...refs)` that accepts multiple refs (callbacks or objects) and assigns a mounted DOM node to all of them.

**Solution:**
```typescript
import type { Ref } from 'react';

export function mergeRefs<T>(...refs: (Ref<T> | undefined)[]): (instance: T | null) => void {
  return (instance: T | null) => {
    for (const ref of refs) {
      if (!ref) continue;
      if (typeof ref === 'function') {
        ref(instance);
      } else {
        (ref as any).current = instance;
      }
    }
  };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Click Outside Dismissal Hook</summary>

**Problem:** Write a `useClickOutside<T extends HTMLElement>(handler: () => void)` hook that returns a ref and listens for clicks outside the referenced element.

**Solution:**
```typescript
import { useRef, useEffect } from 'react';

export function useClickOutside<T extends HTMLElement>(handler: () => void) {
  const ref = useRef<T>(null);

  useEffect(() => {
    const listener = (event: MouseEvent | TouchEvent) => {
      const el = ref.current;
      if (!el || el.contains(event.target as Node)) return;
      handler();
    };

    document.addEventListener('mousedown', listener);
    document.addEventListener('touchstart', listener);
    return () => {
      document.removeEventListener('mousedown', listener);
      document.removeEventListener('touchstart', listener);
    };
  }, [handler]);

  return ref;
}
```
</details>
