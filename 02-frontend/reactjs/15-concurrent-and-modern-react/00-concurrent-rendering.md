# Concurrent Rendering

"Concurrent React" sounds like multithreading. It isn't. **JavaScript is still single-threaded.** What changed is that React can now **pause, resume, restart, or abandon a render in progress**, instead of being forced to finish every render in one uninterruptible chunk.

That sounds like an implementation detail. It's what lets React keep typing responsive while it renders something expensive.

## The problem: blocking renders

Before concurrent rendering, once React started rendering, it ran to completion:

```text
user types "a" ──► React renders 5,000 list rows (200 ms, main thread blocked)
user types "b" ──► …input frozen… the keystroke waits
```

During that 200 ms, the browser can't process input or paint. Typing feels laggy, and no amount of cleverness inside the render fixes it, because it's one long synchronous task. ([Long tasks](../14-performance/00-profiling-and-measuring.md#chrome-devtools) over 50 ms are what users feel as jank.)

## The idea: render in interruptible slices

```text
Blocking:    [───────────── render list (200 ms) ─────────────]  input blocked

Concurrent:  [render slice][yield][render slice][yield]…
                              ▲ browser handles input / paints here
             a new, more urgent update can interrupt and take over
```

React renders in **small slices**, handing control back to the browser between them. If a more urgent update arrives (a keystroke), React can **drop the in-progress render**, handle the urgent update first, and then resume or restart the less urgent one.

The total work isn't reduced. In fact, interrupted renders can mean *more* total work. What improves is **responsiveness**: urgent things happen immediately.

## Urgent vs non-urgent updates

Concurrent rendering needs to know which updates matter most. React distinguishes:

| | Urgent | Non-urgent (a *transition*) |
|---|---|---|
| Examples | Typing, clicking, pressing, hovering | Filtering a big list, switching a heavy tab, navigating to a new view |
| Expectation | Immediate feedback | Can take a moment; may lag |
| Behavior | Rendered first, never interrupted by transitions | Interruptible, can be replaced by newer updates |

**You** declare which updates are non-urgent. React won't guess. That's what [`startTransition`](./01-transitions.md) and [`useDeferredValue`](./02-useDeferredValue.md) are for. Without them, updates are urgent by default.

Internally React tracks priorities with "lanes" ([scheduler and lanes](../17-react-internals/03-scheduler-and-lanes.md)). You never touch those directly.

## It's opt-in per update, not a global mode

Using `createRoot` (the React 18+ root API) **enables the capability**, but nothing becomes interruptible until you use a feature that marks work as non-urgent: transitions, deferred values, or Suspense boundaries.

```tsx
import { createRoot } from "react-dom/client"
createRoot(document.getElementById("root")!).render(<App />)   // concurrent-capable
```

The legacy `ReactDOM.render` is removed in React 19. So "are we using concurrent React?" really means "do we use the features that exploit it?"

## What this means for how you write components

Because React may **start a render and then throw it away** (an urgent update interrupted it), or render the same component more than once before committing, one rule becomes essential:

> **Render must be pure.** Calling a component with the same props, state, and context must produce the same output and must not cause side effects.

Things that are now actively dangerous inside the render body:

```tsx
function Bad({ items }: { items: Item[] }) {
  analytics.track("viewed")               // ✗ side effect: may fire for renders that never commit
  items.sort(byDate)                      // ✗ mutates props
  externalCounter++                       // ✗ mutates something outside
  const id = Math.random()                // ✗ different output each call
  return <List items={items} />
}
```

Side effects belong in event handlers or effects. [StrictMode](../03-hooks/00-hook-rules.md) double-invokes component bodies in development specifically to flush these bugs out. If StrictMode makes something misbehave, your component isn't pure, and concurrent rendering would have exposed it eventually anyway.

Related consequences:

- **Effects only run for committed renders.** A render that's abandoned never runs its effects.
- **Don't assume render ↔ commit is 1:1.** A component may render multiple times per commit (or render and never commit).
- Reading mutable external data during render is risky, which is the next point.

## Tearing and `useSyncExternalStore`

If a component reads from a *mutable external store* (not React state) during a render that is paused partway, the store might change between slices. Different components in the same render pass would then see different values: **tearing**, a UI showing inconsistent data.

React's answer is `useSyncExternalStore`, which libraries like Zustand and Redux use under the hood. It lets React detect store changes during a render and force a consistent, synchronous result. If you subscribe to an external source (browser APIs, your own store), use it rather than `useState` + `useEffect`. See [external stores](../16-advanced-react/02-external-stores.md).

## Automatic batching

Related, and also from React 18: updates are **batched** wherever they occur, so multiple `setState` calls produce one render, not just inside React event handlers:

```tsx
setTimeout(() => {
  setCount((c) => c + 1)
  setFlag((f) => !f)       // one render, not two (React 18+)
}, 1000)
```

Batching isn't unique to concurrent rendering, but it's part of the same overhaul. See [state updates and batching](../02-state-and-rendering/01-state-updates-and-batching.md). In the rare case you need the DOM updated *synchronously* between two updates, `flushSync` forces it, at a performance cost. It's an escape hatch, not a tool.

## What concurrent rendering does *not* do

- **It doesn't make rendering faster.** A 200 ms render is still 200 ms of work. It just no longer blocks input.
- **It doesn't use multiple threads.** For genuinely heavy computation, use a Web Worker or move the work to the server.
- **It doesn't happen automatically** for slow components. You choose what's non-urgent.
- **It doesn't remove the need to fix slow renders.** Prefer [reducing the work](../14-performance/01-rendering-performance.md) when you can. Transitions are for when you *can't* make it cheaper.
- **It doesn't change event handlers.** They still run synchronously and see the state from their render.

## The features built on it

| Feature | What it does with interruptibility |
|---|---|
| [`startTransition` / `useTransition`](./01-transitions.md) | Marks a state update as non-urgent so urgent input isn't blocked |
| [`useDeferredValue`](./02-useDeferredValue.md) | Renders with a stale value first, then re-renders with the new one at low priority |
| [Suspense](./03-suspense.md) | Lets rendering "wait" for data or code without blocking, and keep old UI visible during transitions |
| Streaming SSR ([07](./07-server-components-and-ssr.md)) | Sends HTML in pieces and hydrates boundaries independently |

## Seeing it in practice

1. Build a text input that filters a list of ~5,000 heavy rows, with the filter state updated directly. Type quickly and notice the input lag.
2. Wrap the *filter* update in `startTransition` (or defer the value). The input stays instant while the list catches up.
3. Profile ([Profiler](../14-performance/00-profiling-and-measuring.md#react-devtools-profiler)) both versions. The slow commit is still there, but it no longer blocks keystrokes.

## Common misconceptions

- **"Concurrent React is multithreaded."** No, it's cooperative scheduling on one thread.
- **"Upgrading to React 18/19 made my app faster."** Only if you use the features. Updates remain urgent by default.
- **"`startTransition` makes slow code fast."** It keeps the UI responsive; the work remains.
- **"A render always results in a commit."** Not with concurrent features.
- **"StrictMode double rendering is a bug."** It's a purity check, in development only.
- **"I can still put side effects in render if I'm careful."** Concurrent rendering makes that unreliable.

## Common mistakes

- **Side effects or mutations in render**, which break under interruption and StrictMode.
- **Reaching for transitions before fixing an obviously avoidable slow render.**
- **Using `useState` + `useEffect` to mirror an external store** (tearing risk) instead of `useSyncExternalStore`.
- **Assuming one render per commit**, and logging or counting renders for business logic.
- **Marking text-input state as a transition**, which makes typing feel broken (see [01](./01-transitions.md#what-you-cant-do)).
- **Overusing `flushSync`.**

## Quick summary

- Concurrent rendering = React can **pause, resume, restart, or abandon renders**, keeping the single-threaded UI responsive.
- You mark **non-urgent** updates ([transitions](./01-transitions.md), [deferred values](./02-useDeferredValue.md)); everything else is urgent.
- It improves **responsiveness, not total speed**, and doesn't use extra threads.
- Components must be **pure**: no side effects, mutation, or randomness in render. StrictMode helps catch violations.
- Effects run only for committed renders; external stores need `useSyncExternalStore` to avoid tearing.
- Suspense and streaming SSR build on the same foundation.

## Next

[01 — Transitions](./01-transitions.md)