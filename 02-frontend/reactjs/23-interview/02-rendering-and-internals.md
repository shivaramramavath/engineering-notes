# Rendering & Internals — Senior Interview Questions & Deep Dives

> React Fiber architecture, work loop, double buffering, Lane priority models, Render phase vs Commit phase, and React Compiler transforms.

---

## Question 1: What is a React Fiber, and why did React rewrite its reconciler from the Stack Reconciler to the Fiber Reconciler?

### The Core Problem with the Stack Reconciler
In React 15 and earlier, reconciliation was recursive. When state changed at the root, React traversed the component tree synchronously down the JavaScript call stack.
- Once recursion began, it **could not be interrupted or paused**.
- If tree traversal took 30ms, the main thread was completely blocked, dropping frames (60fps budget = 16.6ms) and causing visible user input lag.

### The Fiber Reconciler Solution
A **Fiber** is a plain JavaScript object representing a unit of work that maps directly to a component or DOM node, but structured as a linked list (`child`, `sibling`, `return`):
```ts
interface FiberNode {
  tag: WorkTag;
  key: null | string;
  type: any;
  stateNode: any; // Actual DOM node or component instance
  return: FiberNode | null; // Parent
  child: FiberNode | null;  // First child
  sibling: FiberNode | null;// Next sibling
  lanes: Lanes;             // Priority bitmask
  alternate: FiberNode | null; // WorkInProgress <-> Current twin
}
```
Because the tree is organized as linked pointers rather than a recursive call stack, the scheduler can pause the work loop (`while (workInProgress !== null && !shouldYield())`), yield control back to the browser to paint or handle a user click, and resume reconciliation seamlessly later.

---

## Question 2: Explain React's "Double Buffering" technique. What are `current` and `workInProgress` trees?

### The Double Buffering Pattern
Borrowed from graphics rendering, React maintains two Fiber trees simultaneously in memory:
1. **`current` tree**: Represents the exact UI currently rendered and visible on the user's screen.
2. **`workInProgress` (WIP) tree**: A duplicate draft tree constructed off-screen during the asynchronous Render Phase.

```
       HostRoot
          │
    currentFiber <────────────── alternate ──────────────> workInProgressFiber
  (Screen Display)                                           (Background Diff)
```

### The Transition
1. When state changes occur, React allocates or clones nodes from `current` to `workInProgress` via `alternate` references.
2. React calculates diffs, runs hooks, and prepares DOM mutations entirely within `workInProgress`.
3. In the synchronous **Commit Phase**, React commits DOM mutations and simply flips the root pointer:
   ```js
   root.current = workInProgress;
   ```
4. The WIP tree becomes the new `current` tree in a single atomic pointer swap, completely eliminating partial, incomplete, or torn UI states.

---

## Question 3: How does React's 31-bit Lanes Model schedule and interrupt concurrent tasks?

### From Priorities to Lanes
React 16 used numeric priorities (`Immediate`, `UserBlocking`, `Normal`, `Low`). This had limitations: a numeric priority could only represent a single level and could not express multi-threaded "sets" of work.

React 18 and 19 use a **31-bit Lane bitmask**:
```js
const SyncLane = 0b0000000000000000000000000000001;
const InputContinuousLane = 0b0000000000000000000000000000100;
const DefaultLane = 0b0000000000000000000001000000000;
const TransitionLanes = 0b0000000001111111110000000000000;
```

### Bitwise Concurrency Mechanics
- **Bitwise OR (`|`)**: Combines multiple pending tasks into an active work set (`fiber.lanes |= nextLane`).
- **Bitwise AND (`&`)**: Instantly checks if a fiber has work matching the currently rendering lane.
- **Interruption**: If a `TransitionLane` (low priority) render is running in the background and the user types into an input (`SyncLane`), React checks `includesHigherPriorityLanes()`, aborts or pauses the transition render immediately, and dispatches the sync click/keystroke render to preserve input responsiveness.

---

## Question 4: How does the React Compiler (React Forget) automate memoization, and why does it render `useMemo` and `useCallback` largely obsolete?

### The Compiler's SSA & Reactive Scope Analysis
The React Compiler operates at build time (Babel/Vite plugin). It converts JavaScript AST into High-level Intermediate Representation (HIR) in Static Single Assignment (SSA) form.

1. **Dependency Inference**: The compiler computes the precise inputs and outputs of every expression, identifying values that mutate or depend on props/state.
2. **Scope Grouping**: Code blocks are segmented into reactive scopes.
3. **Flat Cache Emission**: Instead of nested `useMemo`/`useCallback` calls, the compiler inserts a flat array cache lookup (`_c()`):
   ```js
   // Emitted Compiler Code
   const $ = _c(4); // Fiber-associated memoization slots
   let t0;
   if ($[0] !== propA || $[1] !== propB) {
     t0 = expensiveCalculation(propA, propB);
     $[0] = propA;
     $[1] = propB;
     $[2] = t0;
   } else {
     t0 = $[2];
   }
   ```
This provides fine-grained, component-wide memoization with zero developer maintenance overhead and no human dependency array bugs.
