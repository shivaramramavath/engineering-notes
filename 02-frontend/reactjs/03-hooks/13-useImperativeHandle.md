# React useImperativeHandle: Customizing Exposed Ref Handles

- **Topic**: `03-hooks`
- **React version**: React 16.8+ (Updated for React 19 Native Ref Props)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Eliminates boilerplate when paired with React 19 regular `ref` props; `forwardRef` is deprecated in React 19.

---

## 1. Concept Overview

React embraces a declarative mental model: parents control children by passing props down, and children communicate upward by triggering callbacks. However, certain imperative interactions with the underlying DOM or imperative third-party libraries require a direct command interface:
- Focusing an input element or moving cursor focus.
- Scrolling a specific container or virtualized list to an exact coordinate.
- Triggering imperatively controlled media play/pause states on `<video>` or `<audio>` elements.
- Measuring layout boundaries via `getBoundingClientRect()`.

If a parent assigns a `ref` to a child component, the default behavior in classic React (via `forwardRef`) exposed the **entire raw DOM node**. Exposing raw DOM nodes violates encapsulation: parent components can mutate arbitrary attributes, manipulate inner DOM trees, or bypass component invariants.

`useImperativeHandle` allows a child component to customize and restrict the object instance exposed to parent refs. Instead of handing over the raw DOM node, the child exposes an explicit, limited interface containing only sanctioned methods.

```
Without useImperativeHandle (Raw DOM leak):
Parent Ref ─────────────────────────────► <input> DOM Node (Exposes 100+ native properties)
                                          (Parent can mutate className, style, innerHTML!)

With useImperativeHandle (Encapsulated Handle):
Parent Ref ──► Custom Handle Object ───► Child Component Internal Logic
               {                         │
                 focus: () => void,      ├──► inputRef.current.focus()
                 clear: () => void       └──► setValue('')
               }
```

### When to Use
- Exposing a focused, minimal imperative contract (`focus()`, `scrollIntoView()`, `reset()`).
- Integrating with non-React imperative canvas, WebGL, or mapping engines (Leaflet, Mapbox, Three.js).
- Building composite UI library primitives (modals, video players, rich text editors).

### When NOT to Use
- **Do NOT use to duplicate state**: Avoid calling child methods to trigger state changes that could be managed declaratively via props.
- **Do NOT expose internal component state**: Exposing getters for internal child state breaks unidirectional data flow.

---

## 2. Key Mechanics & Architecture

### React 19 Paradigm Shift: `ref` as a Regular Prop
In React 19, `React.forwardRef` is **deprecated**. `ref` is now passed directly as a standard component prop, simplifying `useImperativeHandle` declarations.

### API Signature
```ts
useImperativeHandle(
  ref: React.Ref<THandle> | undefined,
  createHandle: () => THandle,
  dependencies?: any[]
): void;
```

1. **`ref`**: The ref received from the parent component (in React 19, directly via props).
2. **`createHandle`**: A function that returns the custom object exposed to the parent's `ref.current`.
3. **`dependencies`**: Optional dependency array. If omitted, `createHandle` re-executes on every render; if specified, React only recreates the handle when dependencies change. (Note: With React Compiler, dependency arrays are automatically optimized).

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Execution Phase** | Layout phase (`useLayoutEffect` timing) before passive effects run |
| **React 19 Changes** | Directly accepts `ref` prop; `forwardRef` wrapper is obsolete |
| **Encapsulation Level** | High: hides internal DOM structure and prevents arbitrary DOM mutations |
| **TypeScript Support** | Strictly typed via `React.Ref<THandle>` and interface declarations |
| **Cleanup Timing** | Ref set to `null` on unmount before DOM node detachment |
| **Compiler Optimization** | Handle creation is safely memoized when dependencies are stable |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Restricting an input component to only expose `focus` and `reset` in React 19.

```tsx
import React, { useRef, useImperativeHandle, useState } from 'react';

export interface FormInputHandle {
  focus: () => void;
  reset: () => void;
}

interface FormInputProps {
  label: string;
  ref?: React.Ref<FormInputHandle>;
}

// React 19: ref is a regular prop (NO forwardRef needed!)
export function FormInput({ label, ref }: FormInputProps) {
  const [value, setValue] = useState('');
  const internalInputRef = useRef<HTMLInputElement>(null);

  useImperativeHandle(ref, () => ({
    focus: () => {
      internalInputRef.current?.focus();
    },
    reset: () => {
      setValue('');
    },
  }), []);

  return (
    <label className="block mb-2 text-sm font-medium">
      {label}
      <input
        ref={internalInputRef}
        value={value}
        onChange={(e) => setValue(e.target.value)}
        className="block w-full border px-3 py-1.5 rounded mt-1"
      />
    </label>
  );
}

export function ParentForm() {
  const inputRef = useRef<FormInputHandle>(null);

  return (
    <div className="p-4">
      <FormInput ref={inputRef} label="User Handle" />
      <div className="flex gap-2 mt-2">
        <button
          type="button"
          onClick={() => inputRef.current?.focus()}
          className="px-3 py-1 bg-zinc-800 text-white rounded text-sm"
        >
          Focus Input
        </button>
        <button
          type="button"
          onClick={() => inputRef.current?.reset()}
          className="px-3 py-1 bg-zinc-200 rounded text-sm"
        >
          Reset Input
        </button>
      </div>
    </div>
  );
}
```
Output:
```
// Parent clicking "Focus Input" sets focus without having direct access to raw input DOM element.
```

### Example 2: Production / Real-World Integration
A custom HTML5 Video Player component exposing media playback controls while hiding native DOM elements.

```tsx
import React, { useRef, useImperativeHandle } from 'react';

export interface VideoPlayerHandle {
  play: () => Promise<void>;
  pause: () => void;
  seekTo: (seconds: number) => void;
  getCurrentTime: () => number;
}

interface VideoPlayerProps {
  src: string;
  poster?: string;
  ref?: React.Ref<VideoPlayerHandle>;
}

export function VideoPlayer({ src, poster, ref }: VideoPlayerProps) {
  const videoRef = useRef<HTMLVideoElement>(null);

  useImperativeHandle(ref, () => ({
    play: async () => {
      if (videoRef.current) {
        await videoRef.current.play();
      }
    },
    pause: () => {
      videoRef.current?.pause();
    },
    seekTo: (seconds: number) => {
      if (videoRef.current) {
        videoRef.current.currentTime = seconds;
      }
    },
    getCurrentTime: () => {
      return videoRef.current?.currentTime ?? 0;
    },
  }), []);

  return (
    <div className="rounded-xl overflow-hidden shadow-lg bg-black">
      <video ref={videoRef} src={src} poster={poster} className="w-full h-auto" />
    </div>
  );
}
```
Output:
```
// Parent component controls video playback imperatively via typed handle methods.
```

### Example 3: Edge Case / Virtualized Scroll Container Navigation
Exposing smooth scrolling commands (`scrollToTop`, `scrollToItem`) on a virtualized list container.

```tsx
import React, { useRef, useImperativeHandle } from 'react';

export interface ScrollListHandle {
  scrollToIndex: (index: number) => void;
  scrollToTop: () => void;
}

interface ScrollListProps {
  items: string[];
  ref?: React.Ref<ScrollListHandle>;
}

export function ScrollList({ items, ref }: ScrollListProps) {
  const containerRef = useRef<HTMLDivElement>(null);

  useImperativeHandle(ref, () => ({
    scrollToIndex: (index: number) => {
      if (!containerRef.current) return;
      const targetElement = containerRef.current.children[index] as HTMLElement | undefined;
      targetElement?.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    },
    scrollToTop: () => {
      containerRef.current?.scrollTo({ top: 0, behavior: 'smooth' });
    },
  }), []);

  return (
    <div ref={containerRef} className="h-48 overflow-y-auto border rounded p-2">
      {items.map((item, idx) => (
        <div key={idx} className="p-2 border-b last:border-b-0 text-sm">
          #{idx + 1}: {item}
        </div>
      ))}
    </div>
  );
}
```
Output:
```
// Parent can call listRef.current?.scrollToIndex(45) to scroll directly to item 45.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Capturing stale component state inside `createHandle` by omitting dependency arrays.

```tsx
// ❌ WRONG: Stale closure trap!
export function StaleHandleComponent({ ref, count }: { ref?: React.Ref<any>; count: number }) {
  // BUG: Empty dependency array means createHandle captures count from initial mount (0)!
  useImperativeHandle(ref, () => ({
    logCount: () => {
      console.log('Count is:', count); // Always prints 0!
    },
  }), []); // Missing count dependency!

  return <div>Count: {count}</div>;
}

// ✅ CORRECT: Include reactive dependencies or use functional ref patterns!
export function FixedHandleComponent({ ref, count }: { ref?: React.Ref<any>; count: number }) {
  const countRef = useRef(count);
  countRef.current = count;

  useImperativeHandle(ref, () => ({
    logCount: () => {
      // Always reads latest count through mutable ref
      console.log('Count is:', countRef.current);
    },
  }), []); // Stable handle reference, zero stale closures

  return <div>Count: {count}</div>;
}
```
Output:
```
// Calling logCount() now prints the updated count value accurately.
```

### Example 5: Idiomatic Modern React 19 Pattern
Modal Dialog with accessible focus trapping, utilizing React 19 native ref props and `useImperativeHandle`.

```tsx
import React, { useRef, useImperativeHandle, useState } from 'react';

export interface ModalHandle {
  open: () => void;
  close: () => void;
}

interface ModalProps {
  title: string;
  children: React.ReactNode;
  ref?: React.Ref<ModalHandle>;
}

export function AccessibleModal({ title, children, ref }: ModalProps) {
  const [isOpen, setIsOpen] = useState(false);
  const dialogRef = useRef<HTMLDialogElement>(null);

  useImperativeHandle(ref, () => ({
    open: () => {
      setIsOpen(true);
      dialogRef.current?.showModal();
    },
    close: () => {
      dialogRef.current?.close();
      setIsOpen(false);
    },
  }), []);

  return (
    <dialog
      ref={dialogRef}
      onClose={() => setIsOpen(false)}
      className="p-6 rounded-2xl shadow-2xl backdrop:bg-black/50 border border-zinc-200"
    >
      <h3 className="text-lg font-bold mb-2">{title}</h3>
      <div className="text-sm text-zinc-600 mb-4">{children}</div>
      <button
        type="button"
        onClick={() => dialogRef.current?.close()}
        className="px-4 py-2 bg-zinc-900 text-white rounded text-sm font-medium"
      >
        Dismiss
      </button>
    </dialog>
  );
}
```
Output:
```
// Parent opens modal natively via modalRef.current?.open(), triggering <dialog>.showModal().
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Using `forwardRef` in React 19** | `React.forwardRef` is deprecated and generates unnecessary AST wrappers. | Pass `ref` as a standard component prop: `{ ref }: ComponentProps`. |
| **Exposing component state getters via ref** | `ref.current.getState()` breaks declarative data flow and misses re-renders. | Lift state up to parent or pass state via props/context. |
| **Stale closures in handle methods** | Passing an empty `[]` dependency array while referencing changing state. | Keep a mutable ref to the latest state or include dependencies in the hook array. |
| **Overusing imperative handles for simple actions** | Using ref handles to toggle flags (e.g. `setIsOpen(true)`) instead of passing props. | Use declarative boolean props (e.g. `isOpen={true}`) for UI state. |
| **Mutating `ref.current` manually** | Manually reassigning `ref.current = { ... }` in component body breaks React lifecycle. | Always let `useImperativeHandle` manage ref binding and cleanup. |
| **Calling handle methods during render** | Calling `ref.current.method()` during the render phase causes side effects and race conditions. | Only call ref methods inside event handlers or `useEffect`. |

---

## 5. React Internals: Beneath the Abstraction

Inside `ReactFiberHooks.js`:
1. `useImperativeHandle` delegates internally to `useLayoutEffect`.
2. When the layout phase executes (after DOM mutation but before browser paint):
   - React invokes the `createHandle` function.
   - If `ref` is a function (callback ref), React invokes `ref(handle)`.
   - If `ref` is an object with a `current` property (`RefObject`), React assigns `ref.current = handle`.
3. When the component unmounts or re-renders with changed dependencies:
   - React's layout cleanup runs.
   - It sets `ref.current = null` (or invokes `ref(null)`).
   - If re-rendering, it immediately evaluates `createHandle` and binds the new handle.

---

## 6. Performance, Bundling & React Compiler

- **Object Allocation**: The handle object is only allocated when dependencies change.
- **Render Phase Impact**: `useImperativeHandle` executes in the layout effect queue, meaning it does not block the initial virtual DOM diffing.
- **React Compiler**: The React Compiler memoizes the returned handle object automatically, ensuring parent components observing the ref do not receive unstable handle references.

---

## 7. Accessibility (a11y) & Usability

`useImperativeHandle` is essential for WCAG 2.2 focus management:
- **Focus Restoration**: When closing a modal or flyout, focus must return to the trigger button. Exposing `focus()` allows the parent to restore focus smoothly.
- **Error Stepping**: In complex multi-field forms, when a user submits invalid data, calling `ref.current.focus()` on the first failing field directs screen readers directly to the problem.

---

## 8. Testing & Verification Patterns

Testing imperative handles with Vitest and `@testing-library/react`:

```tsx
import { render, screen, act } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import React, { useRef } from 'react';
import { FormInput, type FormInputHandle } from './FormInput';

describe('useImperativeHandle - FormInput', () => {
  it('exposes focus and reset methods to parent ref', () => {
    let handle: FormInputHandle | null = null;

    function TestWrapper() {
      const ref = useRef<FormInputHandle>(null);
      return (
        <div>
          <FormInput ref={ref} label="Username" />
          <button
            type="button"
            onClick={() => {
              handle = ref.current;
            }}
          >
            Capture Ref
          </button>
        </div>
      );
    }

    render(<TestWrapper />);
    screen.getByText('Capture Ref').click();

    expect(handle).toBeDefined();
    expect(typeof (handle as any)?.focus).toBe('function');
    expect(typeof (handle as any)?.reset).toBe('function');
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17 / 18** | Required `React.forwardRef((props, ref) => ...)` to forward refs into child components. | Remove `forwardRef` wrapper when upgrading to React 19. |
| **React 19** | `ref` is a native prop. `forwardRef` is formally deprecated. | Declare `ref?: React.Ref<T>` directly in component props interface. |

---

## 10. Security & Hardening

1. **Information Hiding**: Exposing a raw DOM node via ref allows untrusted third-party scripts to access `.innerHTML` or `.outerHTML`. Custom handles act as a security sandbox, exposing only explicit methods.
2. **Method Sanitization**: Ensure methods exposed on the handle validate arguments (e.g. bounds checking `seekTo(seconds)` to prevent negative time errors).

---

## 11. Real-World Case Study

### Complex Rich Text Editor Handle in an Enterprise CMS

In a publishing CMS, the parent authoring toolbar needs to command the rich text editor (insert markdown, toggle bold, get word count) without manipulating ProseMirror or DOM nodes directly.

```tsx
import React, { useRef, useImperativeHandle, useState } from 'react';

export interface EditorHandle {
  insertText: (text: string) => void;
  clear: () => void;
  getWordCount: () => number;
}

export function MarkdownEditor({ ref }: { ref?: React.Ref<EditorHandle> }) {
  const [content, setContent] = useState('');
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  useImperativeHandle(ref, () => ({
    insertText: (text: string) => {
      setContent((prev) => prev + text);
    },
    clear: () => {
      setContent('');
    },
    getWordCount: () => {
      return content.trim().split(/\s+/).filter(Boolean).length;
    },
  }), [content]);

  return (
    <div className="border rounded-lg p-2">
      <textarea
        ref={textareaRef}
        value={content}
        onChange={(e) => setContent(e.target.value)}
        rows={6}
        className="w-full p-2 border-0 focus:outline-none font-mono text-sm"
        placeholder="Type markdown content..."
      />
      <div className="text-xs text-zinc-400 text-right mt-1">
        Words: {content.trim().split(/\s+/).filter(Boolean).length}
      </div>
    </div>
  );
}
```

---

## 12. Interview Q&A

<details>
<summary>1. How does React 19 change the way `useImperativeHandle` is used compared to React 18?</summary>

In React 18 and earlier, functional components could not receive a `ref` prop directly; they had to be wrapped in `React.forwardRef((props, ref) => ...)`.
In React 19, `ref` is a first-class prop on functional components. `React.forwardRef` is deprecated. You can declare `ref?: React.Ref<T>` directly in the component's props interface and pass that prop directly to `useImperativeHandle(ref, createHandle)`.
</details>

<details>
<summary>2. Why does `useImperativeHandle` execute during the layout phase rather than the passive effect phase?</summary>

Parent components expect `ref.current` to be populated immediately after DOM mutations occur. If a parent has a `useLayoutEffect` that inspects or manipulates the child's ref, the handle must already exist. Running `useImperativeHandle` in the layout phase guarantees ref synchronization before browser paint and before parent passive effects execute.
</details>

<details>
<summary>3. What is the danger of returning component state directly from `useImperativeHandle`?</summary>

If you expose a method like `getData: () => state`, the parent calling that method reads state outside React's reactive rendering cycle. When the child state changes, the parent will not automatically re-render, leading to stale UI. State should always flow down via props or shared context.
</details>

<details>
<summary>4. How does `useImperativeHandle` handle callback refs passed from a parent?</summary>

React internally detects whether `ref` is a `RefObject` (`{ current: ... }`) or a callback ref (`(node) => void`). If it is a callback ref, React invokes the function with the handle returned by `createHandle`. On unmount, it invokes the callback with `null`.
</details>

<details>
<summary>5. How can you prevent `useImperativeHandle` from recreating the handle object on every render?</summary>

Pass a dependency array as the third argument: `useImperativeHandle(ref, () => ({ ... }), [dep1, dep2])`. React will reuse the existing handle object as long as the dependencies do not change. With the React Compiler enabled, this memoization is applied automatically.
</details>

<details>
<summary>6. Can you combine `useImperativeHandle` with native DOM element forwarding?</summary>

Yes. You can assign an internal `useRef` to the native DOM element and expose both custom methods and the underlying DOM element:
```tsx
useImperativeHandle(ref, () => ({
  focus: () => internalRef.current?.focus(),
  domNode: internalRef.current,
}));
```
However, exposing `domNode` defeats encapsulation and should only be done when external libraries require native node references.
</details>

<details>
<summary>7. What happens if a parent does not pass a `ref` to a component using `useImperativeHandle`?</summary>

If `ref` is `undefined` or `null`, `useImperativeHandle` acts as a no-op. It will not evaluate `createHandle` unnecessarily and will not throw an error.
</details>

<details>
<summary>8. Is `useImperativeHandle` supported in Server Components (RSC)?</summary>

No. Server Components run only on the server and do not produce DOM instances or support interactive ref objects or hooks. `useImperativeHandle` can only be used in Client Components (`'use client'`).
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Form Reset Handle</summary>

**Specification**: Create a `SearchBox` component that exposes a `clearSearch()` imperative method.

```tsx
import React, { useRef, useImperativeHandle, useState } from 'react';

export interface SearchBoxHandle {
  clearSearch: () => void;
}

export function SearchBox({ ref }: { ref?: React.Ref<SearchBoxHandle> }) {
  const [query, setQuery] = useState('');
  const inputRef = useRef<HTMLInputElement>(null);

  useImperativeHandle(ref, () => ({
    clearSearch: () => {
      setQuery('');
      inputRef.current?.focus();
    },
  }), []);

  return (
    <input
      ref={inputRef}
      value={query}
      onChange={(e) => setQuery(e.target.value)}
      placeholder="Search..."
      className="border p-2 rounded"
    />
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Audio Player Trigger</summary>

**Specification**: Build an `AudioPlayer` that exposes `playAudio()` and `pauseAudio()` without revealing the `<audio>` tag.

```tsx
import React, { useRef, useImperativeHandle } from 'react';

export interface AudioHandle {
  playAudio: () => void;
  pauseAudio: () => void;
}

export function AudioPlayer({ src, ref }: { src: string; ref?: React.Ref<AudioHandle> }) {
  const audioRef = useRef<HTMLAudioElement>(null);

  useImperativeHandle(ref, () => ({
    playAudio: () => audioRef.current?.play(),
    pauseAudio: () => audioRef.current?.pause(),
  }), []);

  return <audio ref={audioRef} src={src} />;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Canvas Drawing Surface with Clear Command</summary>

**Specification**: Create a 2D HTML5 Canvas drawing component that exposes `clearCanvas()` and `getBlob()` through an imperative handle.

```tsx
import React, { useRef, useImperativeHandle, useEffect } from 'react';

export interface CanvasHandle {
  clearCanvas: () => void;
  toDataURL: () => string;
}

export function DrawingCanvas({ ref }: { ref?: React.Ref<CanvasHandle> }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useImperativeHandle(ref, () => ({
    clearCanvas: () => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext('2d');
      ctx?.clearRect(0, 0, canvas.width, canvas.height);
    },
    toDataURL: () => {
      return canvasRef.current?.toDataURL() ?? '';
    },
  }), []);

  return <canvas ref={canvasRef} width={400} height={300} className="border bg-white" />;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): OTP 6-Digit Code Input Focus Manager</summary>

**Specification**: Build a 6-digit OTP component that exposes `focusFirstEmpty()` and `reset()` to the parent form.

```tsx
import React, { useRef, useImperativeHandle, useState } from 'react';

export interface OTPHandle {
  focusFirstEmpty: () => void;
  reset: () => void;
}

export function OTPInput({ ref }: { ref?: React.Ref<OTPHandle> }) {
  const [digits, setDigits] = useState(['', '', '', '', '', '']);
  const inputsRef = useRef<(HTMLInputElement | null)[]>([]);

  useImperativeHandle(ref, () => ({
    focusFirstEmpty: () => {
      const firstEmptyIndex = digits.findIndex((d) => d === '');
      const idx = firstEmptyIndex === -1 ? 0 : firstEmptyIndex;
      inputsRef.current[idx]?.focus();
    },
    reset: () => {
      setDigits(['', '', '', '', '', '']);
      inputsRef.current[0]?.focus();
    },
  }), [digits]);

  const handleChange = (val: string, index: number) => {
    const next = [...digits];
    next[index] = val.slice(-1);
    setDigits(next);
    if (val && index < 5) {
      inputsRef.current[index + 1]?.focus();
    }
  };

  return (
    <div className="flex gap-2">
      {digits.map((digit, idx) => (
        <input
          key={idx}
          ref={(el) => (inputsRef.current[idx] = el)}
          value={digit}
          maxLength={1}
          onChange={(e) => handleChange(e.target.value, idx)}
          className="w-10 h-10 border text-center font-bold text-lg rounded"
        />
      ))}
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Virtualized Tree View Expansion Controller</summary>

**Specification**: Implement a hierarchical tree view that allows a parent search bar to programmatically expand and scroll to specific nested node IDs via an imperative handle.

```tsx
import React, { useRef, useImperativeHandle, useState } from 'react';

interface TreeNode {
  id: string;
  label: string;
  children?: TreeNode[];
}

export interface TreeViewHandle {
  expandPath: (nodeId: string) => void;
}

export function TreeView({ data, ref }: { data: TreeNode[]; ref?: React.Ref<TreeViewHandle> }) {
  const [expandedIds, setExpandedIds] = useState<Set<string>>(new Set());

  useImperativeHandle(ref, () => ({
    expandPath: (targetId: string) => {
      const toExpand = new Set<string>();
      function findPath(nodes: TreeNode[], path: string[]): boolean {
        for (const node of nodes) {
          if (node.id === targetId) {
            path.forEach((id) => toExpand.add(id));
            return true;
          }
          if (node.children && findPath(node.children, [...path, node.id])) {
            return true;
          }
        }
        return false;
      }
      findPath(data, []);
      setExpandedIds((prev) => new Set([...prev, ...toExpand]));
    },
  }), [data]);

  const renderNodes = (nodes: TreeNode[]) => (
    <ul className="pl-4">
      {nodes.map((node) => {
        const isExpanded = expandedIds.has(node.id);
        return (
          <li key={node.id} className="py-1">
            <span
              onClick={() => {
                setExpandedIds((prev) => {
                  const next = new Set(prev);
                  next.has(node.id) ? next.delete(node.id) : next.add(node.id);
                  return next;
                });
              }}
              className="cursor-pointer font-medium"
            >
              {node.children ? (isExpanded ? '▼ ' : '▶ ') : '• '}
              {node.label}
            </span>
            {isExpanded && node.children && renderNodes(node.children)}
          </li>
        );
      })}
    </ul>
  );

  return <div>{renderNodes(data)}</div>;
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Headless Dropdown Trigger Alignment Engine</summary>

**Specification**: Create a floating dropdown container that exposes an imperative recalculate/reposition method so parent scroll/resize listeners can synchronously recompute bounding rect coordinates.

```tsx
import React, { useRef, useImperativeHandle, useState, type CSSProperties } from 'react';

export interface DropdownHandle {
  reposition: (triggerRect: DOMRect) => void;
}

export function FloatingMenu({ ref }: { ref?: React.Ref<DropdownHandle> }) {
  const [style, setStyle] = useState<CSSProperties>({ display: 'none' });
  const menuRef = useRef<HTMLDivElement>(null);

  useImperativeHandle(ref, () => ({
    reposition: (triggerRect: DOMRect) => {
      setStyle({
        position: 'fixed',
        top: `${triggerRect.bottom + 8}px`,
        left: `${triggerRect.left}px`,
        display: 'block',
        zIndex: 50,
      });
    },
  }), []);

  return (
    <div
      ref={menuRef}
      style={style}
      className="bg-white border shadow-xl rounded-lg p-2 min-w-[160px]"
    >
      <div className="p-2 hover:bg-zinc-100 rounded text-sm cursor-pointer">Profile</div>
      <div className="p-2 hover:bg-zinc-100 rounded text-sm cursor-pointer">Settings</div>
      <div className="p-2 hover:bg-rose-50 text-rose-600 rounded text-sm cursor-pointer">Logout</div>
    </div>
  );
}
```
</details>
