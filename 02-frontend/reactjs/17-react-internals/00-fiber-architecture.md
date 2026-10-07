# Fiber Architecture

**Fiber** is the name of React's reconciler architecture (introduced in React 16 and the foundation of everything in 18 and 19). A **fiber** is a plain JavaScript object that represents **one unit of work**: roughly, one component instance, DOM element, or text node in your tree.

Understanding fibers explains three things that otherwise seem arbitrary: why rendering can be interrupted, why state survives re-renders, and why components must be pure.

> This is a simplified mental model. Field names exist in the source, but the details evolve, so don't treat this as an API.

## The problem fibers solved

Before React 16, the reconciler was **recursive**: React walked the tree by calling functions that called functions, using the JavaScript call stack. That has a fatal limitation: **you can't pause a recursion halfway.** Once rendering started, it ran to the end, blocking the main thread however long it took.

To make rendering **interruptible**, React replaced the call stack with its own data structure it controls: a **linked list of fibers** and a loop that processes one fiber at a time. Between any two fibers, React can stop, let the browser breathe, and resume later (or throw the work away).

## What a fiber holds

```ts
// Conceptual shape: real fibers have many more fields
type Fiber = {
  // Identity
  type: Function | string | symbol   // your component function, "div", Fragment, ...
  key: string | null
  stateNode: any                     // the DOM node (host fibers) or class instance

  // Tree structure (a linked list, not nested arrays)
  return: Fiber | null               // parent
  child: Fiber | null                // first child
  sibling: Fiber | null              // next sibling

  // Props and state
  pendingProps: any                  // props for this render
  memoizedProps: any                 // props from the last render
  memoizedState: any                 // for function components: the hooks linked list (see 02)

  // Work tracking
  flags: number                      // what must happen at commit: Placement, Update, Deletion...
  lanes: number                      // pending update priorities on this fiber (see 03)
  alternate: Fiber | null            // the "other" version of this fiber (see below)
}
```

Notice what's **not** here: your component function's local variables. They're recreated every call. **State lives on the fiber**, not in the function, which is how it survives between renders.

### The tree is a linked list

```text
        App
        │ child
        ▼
      Layout ───sibling──► Footer
        │ child
        ▼
      Header ───sibling──► Main
                            │ child
                            ▼
                          Card ───sibling──► Card
```

Each fiber points to its **first child**, its **next sibling**, and its **parent** (`return`). That's enough to walk the whole tree **iteratively** with a plain `while` loop, going down through children, across siblings, and back up through parents, with no recursion and therefore the ability to stop at any node and resume exactly there.

## The work loop

```ts
// Greatly simplified
function workLoop() {
  while (nextUnitOfWork !== null && !shouldYield()) {
    nextUnitOfWork = performUnitOfWork(nextUnitOfWork)
  }
  if (nextUnitOfWork !== null) {
    scheduleContinuation()          // ran out of time: yield to the browser, resume later
  } else {
    commit()                        // finished the whole tree
  }
}
```

`performUnitOfWork` processes one fiber in two steps:

1. **`beginWork`** (going down): call the component (run hooks, get its JSX output), reconcile that output against the children ([01](./01-reconciliation.md)), and create or update child fibers. Returns the next child to work on.
2. **`completeWork`** (coming back up): once a fiber has no more children to process, finish it, creating or preparing its DOM node (not yet attached to the page) and bubbling up info about which descendants have changes.

```text
beginWork(App) → beginWork(Layout) → beginWork(Header) → completeWork(Header)
              → beginWork(Main) → beginWork(Card) → completeWork(Card) → ...
              → completeWork(Main) → completeWork(Layout) → completeWork(App)
```

**`shouldYield()`** is what makes it concurrent. In a concurrent render, React checks whether its time slice is up between fibers ([03](./03-scheduler-and-lanes.md)). In a synchronous (urgent) render, it doesn't yield and processes the whole tree.

## Two trees: current and work-in-progress

React keeps **two** fiber trees:

- **current**: the tree that matches what's on screen right now.
- **work-in-progress** (WIP): the tree React is building for the next update.

Each fiber in one tree points to its counterpart in the other through `alternate`. When React renders an update, it builds the WIP tree by **reusing and updating** the alternate fibers (cheaper than allocating new ones). It never mutates the `current` tree.

```text
        current tree                     work-in-progress tree
        (what's on screen)               (being built)
          App  ◄──── alternate ────►  App'
           │                            │
          Card ◄──── alternate ────►  Card'   ← new props/state computed here
```

When the WIP tree is complete and committed, React **swaps the pointer**: WIP becomes `current`, and the old current becomes the spare for the next render. This is **double buffering**, the same trick graphics programs use to avoid drawing a half-finished frame.

Why it matters:

- **Abandoning a render is free**: if a more urgent update arrives, React just throws away the WIP tree. `current` (and the screen) are untouched.
- **The UI is always consistent**: users never see a half-rendered tree. They see the old `current` until the new one is complete.

## Render phase vs commit phase

| | Render phase | Commit phase |
|---|---|---|
| What happens | Call components, run hooks, diff, mark changes on fibers | Apply DOM changes, run effects |
| Interruptible | **Yes** (concurrent renders) | **No**, always synchronous |
| Can be restarted/discarded | **Yes** | No |
| Side effects allowed | **No**: must be pure | Yes, this is where effects live |
| Touches the DOM | **No** | **Yes** |

### Commit sub-phases

Once the render phase finishes, React commits in one uninterrupted pass:

```text
1. Before mutation   read DOM state if needed (e.g. snapshots)
2. Mutation          apply DOM changes (insert, update, remove); cleanup of layout effects
3. [current ← WIP]   swap the trees
4. Layout            run layout effects (useLayoutEffect), set refs; DOM is updated but not yet painted
   ─── browser paints ───
5. Passive effects   run useEffect cleanups and effects (scheduled after paint)
```

This ordering is why `useLayoutEffect` can measure DOM and update state **before** paint (no flicker) but blocks painting, while `useEffect` runs **after** paint and doesn't block it ([useLayoutEffect](../03-hooks/08-useLayoutEffect.md)). A passive effect that sets state causes another render pass.

## Fibers explain behaviors you've seen

**Components must be pure.** The render phase can run, pause, restart, or be discarded entirely. Any side effect in render could happen zero, one, or many times ([concurrent rendering](../15-concurrent-and-modern-react/00-concurrent-rendering.md#what-this-means-for-how-you-write-components)). StrictMode intentionally calls your component twice in development to expose violations.

**State survives re-renders.** The function runs fresh each time, but its hooks read from `fiber.memoizedState` ([02](./02-how-hooks-work.md)). If the fiber is **destroyed** (the component unmounts, or its identity changes), the state goes with it ([01](./01-reconciliation.md)).

**Parent re-render re-renders children.** Re-rendering a fiber means calling its component, and `beginWork` then reconciles its children. React only **skips** a child fiber (a **bailout**) when the child's props are identical (same reference) and it has no pending updates of its own. `React.memo` makes that comparison shallow-equality-based ([memoization](../14-performance/02-memoization.md)).

```text
beginWork(fiber):
  if props unchanged AND no pending lanes on this fiber AND no context change:
      bail out → reuse the existing child fibers (and skip the subtree if nothing below has work either)
  else:
      call the component, reconcile its output
```

That bailout is also why the `children` pattern avoids re-renders: the element reference for `children` is unchanged, so React reuses the subtree ([rendering performance](../14-performance/01-rendering-performance.md#fix-2-lift-content-up-pass-children)).

**Rendering isn't the same as updating the DOM.** The render phase produces a *description* of changes (flags on fibers). Only fibers marked with changes cause DOM operations in the commit phase. A component can render and cause zero DOM work.

**A render can produce no commit.** An interrupted or abandoned render never reaches the commit phase, so its effects never run.

## Fiber types you'll see in DevTools

Each fiber has a **tag** indicating what it represents (Function Component, Class Component, Host Component for DOM elements, Host Text, Fragment, Context Provider/Consumer, Suspense Boundary, Memo, Forward Ref, and so on). React DevTools shows the tree of components, which is a filtered view of this fiber tree (it hides host nodes and internals by default). The Profiler's "why did this render" reads fiber data like changed props, state, and hooks.

## What you don't control

You never create, hold, or inspect fibers in app code. Practical takeaways:

- **Identity matters.** Whether React treats an element as "the same fiber as before" or a new one decides whether state is kept or reset ([01](./01-reconciliation.md)).
- **Don't depend on render counts or order** for logic.
- **Don't rely on internals.** Anything accessed through private properties like `__reactFiber$…` on DOM nodes can break on any update. DevTools uses internal hooks for exactly this reason, and your app shouldn't.

## Common misconceptions

- **"Fiber is a faster algorithm."** Not primarily. It's a re-architecture that makes work **interruptible and prioritizable**. Total work isn't smaller.
- **"A component re-renders = the DOM updates."** No. Rendering is calling your function; the DOM changes only where the diff found differences.
- **"The virtual DOM is a copy of the real DOM."** It's a tree of lightweight descriptions (elements, then fibers) used to compute minimal changes.
- **"State is stored in the component."** It's stored on the fiber and handed to your function on each call.
- **"React updates the DOM as it renders."** DOM mutation happens only in the commit phase, in one pass.
- **"Effects run during render."** They run in the commit phase (layout effects synchronously, passive effects after paint).

## Quick summary

- A **fiber** is a JS object for one unit of work (component, DOM element, text), linked by `child`, `sibling`, and `return`, so the tree can be walked with a loop instead of recursion.
- The **work loop** processes one fiber at a time (`beginWork` down, `completeWork` up) and can **yield** between fibers, which is what makes concurrent rendering possible.
- Two trees, **current** and **work-in-progress**, linked by `alternate`; commit swaps them (double buffering), so abandoning a render is free and the UI is never half-updated.
- **Render phase**: pure, interruptible, no DOM. **Commit phase**: synchronous; DOM mutations, then layout effects, then (after paint) passive effects.
- State lives **on the fiber** (hooks list); identity of the fiber decides whether state persists.
- Treat all of this as a mental model; internals change between versions.

## Next

[01 — Reconciliation](./01-reconciliation.md)
