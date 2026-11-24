# Render Phases: Render vs Commit

- **Topic**: `02-state-and-rendering`
- **React version**: React 19+ (tested on React 19.0.0)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Understanding the two-phase pipeline is essential for debugging concurrent rendering, effect scheduling, and DOM layout measurements.

---

## 1. Concept Overview

In React, updating the user interface is not a single, monolithic operation. React separates rendering into two distinct, sequential phases:
1. **The Render (Reconciliation) Phase**:
   - React invokes your component functions to figure out what changed.
   - It constructs the work-in-progress Fiber tree and compares (diffs) it against the current Fiber tree.
   - **Characteristics**: **Asynchronous, concurrent, and interruptible**. React can pause, abandon, or restart the render phase without user-visible consequences.
   - **Golden Rule**: Must be **100% pure with zero side effects**!
2. **The Commit Phase**:
   - React applies the computed changes to the real browser DOM.
   - It synchronously mounts, updates, or deletes real DOM elements.
   - It fires layout effects (`useLayoutEffect`), paints to the screen, and then schedules passive effects (`useEffect`).
   - **Characteristics**: **Synchronous and non-interruptible**. Once the commit phase begins, it runs to completion to avoid rendering partially updated UI to users.

```
+─────────────────────────────────────────────────────────────+
│                       1. RENDER PHASE                       │
│  • Calls Component(props)                                   │
│  • Computes JSX elements                                    │
│  • Reconciles & diffs Fiber nodes                           │
│  • Can be interrupted, paused, or restarted!                │
│  • STRICTLY PURE (NO DOM MUTATIONS, NO SIDE EFFECTS)        │
+─────────────────────────────────────────────────────────────+
                               │
                               ▼
+─────────────────────────────────────────────────────────────+
│                       2. COMMIT PHASE                       │
│  • Synchronously updates Real Browser DOM                   │
│  • Runs useLayoutEffect() synchronously                     │
│  • Browser paints pixels to screen                          │
│  • Runs useEffect() asynchronously                          │
│  • CANNOT BE INTERRUPTED (Runs to completion)               │
+─────────────────────────────────────────────────────────────+
```

---

## 2. Key Mechanics & Architecture

### Phase Breakdown Table

| Dimension | Render Phase | Commit Phase (DOM Mutation) | Passive Effect Phase (`useEffect`) |
| :--- | :--- | :--- | :--- |
| **Execution Timing** | Background / Asynchronous | Synchronous (immediately before paint) | Asynchronous (immediately after paint) |
| **Interruptible?** | **Yes** (in Concurrent React) | **No** (atomic transaction) | N/A (runs per effect) |
| **DOM Access** | **Forbidden** (DOM does not reflect WIP) | Real DOM mutated and accessible | Real DOM rendered and visible |
| **Relevant Hooks** | `useState`, `useReducer`, `useMemo` | `useLayoutEffect`, `useInsertionEffect` | `useEffect` |
| **Can Trigger Re-render?** | Yes (bad: causes infinite loop) | Yes (sync: causes visual reflow) | Yes (async: standard state update) |

### Double-Buffering (Current vs Work-in-Progress)
React maintains two Fiber trees in memory at all times (like graphics video cards):
- **Current Tree**: Represents the UI currently painted on screen.
- **Work-in-Progress (WIP) Tree**: Built during the Render phase.
When the Commit phase completes, React swaps a single pointer (`root.current = workInProgress`), instantly promoting the newly committed tree to the active screen.

---

## 3. Real-World Code Examples

### Example 1: Observing Render vs Commit Order
```tsx
import { useEffect, useLayoutEffect, useState } from 'react';

export function PhaseTracer() {
  const [count, setCount] = useState(0);

  // 1. Render Phase
  console.log(`[Render Phase] Rendering component with count: ${count}`);

  // 2. Commit Phase: Layout Effect (Pre-paint)
  useLayoutEffect(() => {
    console.log(`[Commit Phase - useLayoutEffect] Real DOM attached. count: ${count}`);
  }, [count]);

  // 3. Post-Paint Phase: Passive Effect
  useEffect(() => {
    console.log(`[Passive Phase - useEffect] Browser painted. count: ${count}`);
  }, [count]);

  return (
    <div className="p-4">
      <p>Count: {count}</p>
      <button onClick={() => setCount((c) => c + 1)} className="border px-2 py-1">
        Increment
      </button>
    </div>
  );
}
```
Console Output on Initial Mount:
```
[Render Phase] Rendering component with count: 0
[Commit Phase - useLayoutEffect] Real DOM attached. count: 0
[Passive Phase - useEffect] Browser painted. count: 0
```

### Example 2: DOM Measurement in `useLayoutEffect` (Pre-Paint)
```tsx
import { useState, useLayoutEffect, useRef } from 'react';

export function Tooltip({ text }: { text: string }) {
  const [tooltipHeight, setTooltipHeight] = useState(0);
  const ref = useRef<HTMLDivElement>(null);

  // useLayoutEffect runs in the Commit phase BEFORE the browser paints!
  // Measuring and adjusting layout here prevents visible UI flicker.
  useLayoutEffect(() => {
    if (ref.current) {
      const { height } = ref.current.getBoundingClientRect();
      setTooltipHeight(height);
    }
  }, [text]);

  return (
    <div ref={ref} className="border p-2 bg-yellow-50">
      <p>{text}</p>
      <span className="text-xs text-gray-500">Measured Height: {tooltipHeight}px</span>
    </div>
  );
}
```
Output:
```
// Tooltip renders and measures its height without any visible visual jump or flicker!
```

### Example 3: Simulating Concurrent Interruptions (Render Aborted)
```tsx
import { useState, useTransition } from 'react';

export function ConcurrentInterruptDemo() {
  const [isPending, startTransition] = useTransition();
  const [urgentInput, setUrgentInput] = useState('');
  const [heavyData, setHeavyData] = useState<number[]>([]);

  const handleInputChange = (val: string) => {
    // High-priority urgent render: commits immediately!
    setUrgentInput(val);

    // Low-priority transition render: can be interrupted by next keystroke!
    startTransition(() => {
      const largeArray = Array.from({ length: 10000 }, (_, i) => i * val.length);
      setHeavyData(largeArray);
    });
  };

  return (
    <div className="p-4 space-y-2">
      <input
        value={urgentInput}
        onChange={(e) => handleInputChange(e.target.value)}
        placeholder="Type rapidly..."
        className="border p-1"
      />
      {isPending && <p className="text-sm text-blue-500">Rendering list in background...</p>}
      <p>Data points generated: {heavyData.length}</p>
    </div>
  );
}
```

### Example 4: The Dangerous State Mutation in Render
```tsx
// DANGER: Calling setState directly in render causes an infinite render loop!
export function BrokenLoop() {
  const [val, setVal] = useState(0);

  // if (val < 5) {
  //   setVal(val + 1); // CRASH: Maximum update depth exceeded!
  // }

  return <div>Value: {val}</div>;
}
```

### Example 5: Clean Passive Cleanup Execution
```tsx
import { useEffect, useState } from 'react';

export function TimerTracker() {
  const [seconds, setSeconds] = useState(0);

  useEffect(() => {
    console.log('[Commit] Setting up interval timer');
    const interval = setInterval(() => {
      setSeconds((s) => s + 1);
    }, 1000);

    // Cleanup executes in the commit phase of the next render or unmount!
    return () => {
      console.log('[Commit] Tearing down interval timer');
      clearInterval(interval);
    };
  }, []);

  return <div>Seconds: {seconds}</div>;
}
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| Interacting with the real DOM during render (`ref.current.focus()`) | Ref is not attached yet during render; causes null pointers or stale DOM reads | Access and mutate DOM strictly inside `useEffect` or `useLayoutEffect` |
| Using `useLayoutEffect` for standard data fetching | Blocks the browser from painting pixels to the screen, causing perceived freeze | Use `useEffect` for non-visual operations |
| Mutating external variables inside render | Render phase can be paused or run multiple times (StrictMode), duplicating mutations | Keep render strictly pure; move mutations to effects or events |
| Triggering synchronous re-renders inside `useLayoutEffect` unnecessarily | Forces the browser into an immediate second synchronous render pass before paint | Compute derived values during render or use asynchronous `useEffect` |
| Assuming `useEffect` runs before the screen updates | `useEffect` is intentionally deferred until *after* the browser has completed painting | Use `useLayoutEffect` only when pre-paint layout measurement is mandatory |
| Relying on render phase timing for performance metrics | Render phase is decoupled from real paint times | Use the `<Profiler>` component or Performance Timeline API |

---

## 5. React Internals: Beneath the Abstraction

### Fiber Execution Flow Across Phases
1. **Work Loop (`workLoopConcurrent`)**:
   - `performUnitOfWork(wip)` iterates down the Fiber tree calling `beginWork()`.
   - If time expires on the current browser frame (5ms deadline), React yields control back to the browser via MessageChannel / scheduler.
   - When finished, `completeWork()` bubbles up, aggregating DOM mutation flags (`Placement`, `Update`, `Deletion`) into the effect list.
2. **Commit Root (`commitRoot`)**:
   - React enters `commitRootImpl()`.
   - **Before Mutation Phase**: Calls `getSnapshotBeforeUpdate`.
   - **Mutation Phase**: Traverses effect flags and calls DOM operations (`appendChild`, `removeChild`).
   - **Layout Phase**: Calls `useLayoutEffect` callbacks synchronously.
   - React releases the call stack; the browser paints.
   - React fires scheduled `useEffect` passive effect queues via microtasks/macrotasks.

---

## 6. Performance, Bundling & React Compiler

- **Concurrent Time Slicing**: Because the Render phase is pure, React can slice a 200ms render into five 40ms chunks, giving the browser time to process user scrolls and clicks in between.
- **Compiler Hoisting**: Static JSX nodes identified by the React Compiler bypass `beginWork()` comparisons entirely during the Render phase.

---

## 7. Accessibility (a11y) & Usability

- **Preventing Layout Shifts (CLS)**: Measuring dynamic popover positions in `useLayoutEffect` guarantees that tooltips never render in the top-left corner for a split second before jumping to the target button.

---

## 8. Testing & Verification Patterns

```tsx
import { render, screen, act } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { PhaseTracer } from './PhaseTracer';

describe('Render and Commit Cycle', () => {
  it('executes render and commit phases cleanly without errors', () => {
    render(<PhaseTracer />);
    expect(screen.getByText('Count: 0')).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration

| React Version | Pipeline Architecture |
| :--- | :--- |
| **React 15** | Synchronous Stack Reconciler (Render and Commit were tightly intertwined) |
| **React 16** | Fiber architecture decoupled Render phase from Commit phase |
| **React 18 & 19** | Concurrent rendering enabled; Render phase is fully interruptible by default |

---

## 10. Security & Hardening

- **Atomic DOM Mutations**: Because Commit is non-interruptible, the DOM is never left in a partially mutated, insecure state (e.g. rendering private user details before an authorization check completes).

---

## 11. Real-World Case Study

### Fixing a Jittery Navigation Header
A dashboard navigation bar measured the active tab's bounding box and animated an underline indicator.
- **Initial Code**: Measured DOM in `useEffect`.
- **Symptom**: On every page transition, the underline rendered at the previous tab for 1 frame, then visibly jumped to the new tab (Flicker).
- **Fix**: Switched from `useEffect` to `useLayoutEffect`. The measurement and DOM adjustment occurred synchronously in the Commit phase before the browser painted, eliminating the visual jump completely.

---

## 12. Interview Q&A

<details>
<summary>1. What are the key differences between the Render phase and the Commit phase in React?</summary>
The <b>Render phase</b> executes component functions, calculates JSX, and builds the work-in-progress Fiber tree. It is asynchronous, pure, and can be paused, interrupted, or restarted by React. The <b>Commit phase</b> applies the calculated changes to the real browser DOM, runs <code>useLayoutEffect</code>, paints, and triggers <code>useEffect</code>. The Commit phase is synchronous and cannot be interrupted.
</details>

<details>
<summary>2. Why must the Render phase be completely free of side effects?</summary>
In Concurrent React, React may begin rendering a component, interrupt it to process an urgent user event, and restart the render from scratch later. If the component executes side effects (like API calls or DOM mutations) during render, those side effects will execute multiple times, leading to data corruption and memory leaks.
</details>

<details>
<summary>3. What is the difference between useLayoutEffect and useEffect in relation to the Commit phase?</summary>
Both hooks execute as part of the commit lifecycle:
- <code>useLayoutEffect</code> executes <b>synchronously</b> immediately after DOM mutations, but <b>before</b> the browser paints to the screen. Ideal for layout measurements.
- <code>useEffect</code> executes <b>asynchronously</b> <i>after</i> the browser has finished painting pixels to the screen. Ideal for data fetching and subscriptions.
</details>

<details>
<summary>4. What is the Fiber double-buffering technique?</summary>
React maintains two Fiber trees: the <code>current</code> tree (reflecting what is visible on screen) and the <code>workInProgress</code> tree (built in memory during the render phase). Once the commit phase completes, React updates a single root pointer to point to the work-in-progress tree, instantly swapping it to the active tree.
</details>

<details>
<summary>5. What happens if an error is thrown in the Render phase versus the Commit phase?</summary>
Errors in the Render phase are caught by the nearest Error Boundary's <code>static getDerivedStateFromError()</code> to render a fallback UI. Errors in the Commit phase (e.g. inside <code>useEffect</code> or <code>useLayoutEffect</code>) trigger <code>componentDidCatch()</code> and unmount the failing tree if unhandled.
</details>

<details>
<summary>6. Can the Commit phase ever be asynchronous?</summary>
No. The commit phase must be strictly synchronous. If the commit phase were asynchronous, users would observe half-updated UI states (e.g. a header updated but body not yet rendered), creating severe visual tearing.
</details>

<details>
<summary>7. What is useInsertionEffect and where does it fit in the Commit phase?</summary>
Introduced in React 18, <code>useInsertionEffect</code> runs synchronously <i>before</i> any DOM mutations occur. It is designed specifically for CSS-in-JS libraries (like styled-components) to inject <code>&lt;style&gt;</code> tags before layout effects measure geometry.
</details>

<details>
<summary>8. How does React batch state updates across phases?</summary>
In React 18 and 19, automatic batching groups multiple state updates (even across promises and timeouts) into a single Render phase pass followed by a single Commit phase, preventing unnecessary intermediate paints.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Refactoring DOM Measurement to useLayoutEffect</summary>

**Specification**: Fix a flickering box component by migrating its DOM height measurement to `useLayoutEffect`.

```tsx
import { useLayoutEffect, useRef, useState } from 'react';

export function NonFlickeringBox() {
  const [width, setWidth] = useState(0);
  const boxRef = useRef<HTMLDivElement>(null);

  useLayoutEffect(() => {
    if (boxRef.current) {
      setWidth(boxRef.current.offsetWidth);
    }
  }, []);

  return (
    <div ref={boxRef} className="p-4 border">
      Measured Width: {width}px
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Safe Timer Lifecycle</summary>

**Specification**: Implement an effect that logs when it commits and when it cleans up.

```tsx
import { useEffect } from 'react';

export function LifecycleLogger({ componentId }: { componentId: string }) {
  useEffect(() => {
    console.log(`[Commit] Mounted ${componentId}`);
    return () => {
      console.log(`[Commit] Cleaned up ${componentId}`);
    };
  }, [componentId]);

  return <div>Component {componentId}</div>;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Scroll Position Restoration Before Paint</summary>

**Specification**: Use `useLayoutEffect` to restore a container's scroll position before the browser paints to prevent visual jumping.

```tsx
import { useLayoutEffect, useRef } from 'react';

export function PreservedScrollList({ scrollOffset }: { scrollOffset: number }) {
  const containerRef = useRef<HTMLDivElement>(null);

  useLayoutEffect(() => {
    if (containerRef.current) {
      containerRef.current.scrollTop = scrollOffset;
    }
  }, [scrollOffset]);

  return (
    <div ref={containerRef} className="h-40 overflow-y-auto border">
      <div className="h-96">Long Content List...</div>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Concurrent Render Transition Logger</summary>

**Specification**: Build a component that uses `useTransition` and renders a pending indicator while processing a heavy list render.

```tsx
import { useState, useTransition } from 'react';

export function HeavyTransition() {
  const [isPending, startTransition] = useTransition();
  const [items, setItems] = useState<string[]>([]);

  const loadItems = () => {
    startTransition(() => {
      const generated = Array.from({ length: 5000 }, (_, i) => `Item #${i}`);
      setItems(generated);
    });
  };

  return (
    <div>
      <button onClick={loadItems} disabled={isPending}>
        {isPending ? 'Generating...' : 'Load 5000 Items'}
      </button>
      <p>Rendered items: {items.length}</p>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Dynamic Tooltip Positioning Hook</summary>

**Specification**: Write a custom hook `useAnchorPosition(anchorRef)` that calculates coordinates in `useLayoutEffect` and repositions a popover synchronously.

```tsx
import { useState, useLayoutEffect, RefObject } from 'react';

export function useAnchorPosition(anchorRef: RefObject<HTMLElement | null>) {
  const [coords, setCoords] = useState<{ top: number; left: number }>({ top: 0, left: 0 });

  useLayoutEffect(() => {
    if (anchorRef.current) {
      const rect = anchorRef.current.getBoundingClientRect();
      setCoords({
        top: rect.bottom + window.scrollY,
        left: rect.left + window.scrollX,
      });
    }
  }, [anchorRef]);

  return coords;
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Render Phase Execution Counter</summary>

**Specification**: Write a debug hook `useRenderCounter(name)` that tracks how many times a component entered the Render phase vs how many times it reached the Commit phase.

```tsx
import { useRef, useEffect } from 'react';

export function usePhaseAuditor(name: string) {
  const renderPasses = useRef(0);
  const commitPasses = useRef(0);

  // Increments synchronously during the Render phase
  renderPasses.current++;

  useEffect(() => {
    // Increments asynchronously in the Commit phase
    commitPasses.current++;
    console.log(`[${name}] Renders: ${renderPasses.current}, Commits: ${commitPasses.current}`);
  });
}
```
</details>
