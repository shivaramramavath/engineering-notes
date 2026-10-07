# Scheduler and Lanes

[Concurrent rendering](../15-concurrent-and-modern-react/00-concurrent-rendering.md) says React can interrupt a render for something more urgent. This note explains *how* React decides **when** to work, **which** update is more urgent, and what actually happens to the work it interrupts.

> Simplified mental model. Lane names, the number of lanes, and scheduling heuristics are internal and change between versions, so the concepts are what matter.

## Two layers

React's scheduling has two cooperating layers:

| Layer | Question it answers | Mechanism |
|---|---|---|
| **Scheduler** | *When* does JavaScript get to run React's work, without freezing the browser? | A small cooperative scheduler (the `scheduler` package) that slices work into short tasks |
| **Lanes** | *Which updates* are in this render, and *which is more urgent*? | Priority bits assigned to every update |

## The Scheduler: cooperative time slicing

JavaScript can't be preempted. Once a function is running, nothing interrupts it. So React **volunteers to stop**: the [work loop](./00-fiber-architecture.md#the-work-loop) checks `shouldYield()` between fibers and, when its time slice is up, hands control back to the browser.

```text
main thread:  [ input ][ React work ~5ms ]  [ paint ] [ React work ~5ms ] [ input ] [ React work ]…
                                  ▲ yields here so the browser can handle events and paint
```

Details of the approach:

- Slices are **short** (on the order of a few milliseconds), so the browser can respond to input and keep frames smooth.
- React **schedules its continuation** with a macrotask rather than `setTimeout` (which is clamped and slow) or `requestAnimationFrame` (tied to frames). The Scheduler uses `MessageChannel` where available, which fires promptly after the browser has had a chance to process events and render.
- It also has its own notion of **task priority** (immediate, user-blocking, normal, low, idle) used to order React's callbacks with each other.

This is also why heavy synchronous work **inside** a single component's render can't be sliced: yielding happens *between* fibers, never in the middle of your function. A 300 ms render in one component still blocks for 300 ms. See [rendering performance](../14-performance/01-rendering-performance.md#fix-5-do-less-work-per-render).

## Lanes: priority as bit flags

Every update (a `setState` call, a transition, a retry after Suspense) is assigned a **lane**, a bit in a bitmask representing its priority. Lower bit positions mean higher priority.

```text
Lane (conceptual, highest → lowest priority)
  SyncLane             discrete user input: click, keypress     ← flush ASAP
  InputContinuousLane  continuous input: scroll, mousemove, drag
  DefaultLane          normal updates (timers, network results, most setState)
  TransitionLanes      startTransition / useDeferredValue       ← non-urgent, interruptible
  RetryLanes           Suspense retries after data arrives
  IdleLane / Offscreen background / hidden work
```

Why bitmasks? Combining and comparing priorities becomes cheap bit operations: "which lanes have pending work?" is one integer; "is lane X among the lanes in this render?" is a bitwise AND. A fiber's `lanes` field is "the set of priorities with pending work here", and the root tracks the union.

### How an update gets its lane

React maps **what triggered the update** to a priority:

| Trigger | Lane (roughly) |
|---|---|
| Discrete events: `click`, `keydown`, `input`, `submit` | Sync |
| Continuous events: `scroll`, `mousemove`, `pointermove`, `drag` | Input continuous |
| Updates outside events (timers, promises, network) | Default |
| Inside `startTransition` or from `useDeferredValue` | **Transition** |
| Suspense retry | Retry |

This is why the event system ([04](./04-event-system.md)) matters: the *kind* of event sets the priority of everything you `setState` inside its handler. And it's why wrapping a `setState` in `startTransition` changes its priority without changing what it does.

## Picking what to render

When React starts a render, it **chooses a set of lanes** to work on, generally the highest-priority pending lane (plus compatible lanes). During the render phase:

- A fiber's updates are processed **only if their lane is part of this render**.
- Updates in other (lower-priority) lanes are **skipped**, and left in the queue for a later render.

```text
Pending updates:
   A: setQuery("ab")                     → Sync lane          (urgent: typing)
   B: setResults(filter("ab")) in transition → Transition lane   (non-urgent)

Render 1: lanes = {Sync}
   → processes A only. Input shows "ab" immediately. B is skipped.
Render 2: lanes = {Transition}
   → processes B. Heavy list renders in interruptible slices.
```

## Interruption

Suppose Render 2 (the transition) is in progress and the user types another character, creating a new **Sync** update:

```text
Render 2 (transition) ──[yield]──► new Sync update arrives
   │
   ├─ React sees a higher-priority lane has pending work
   ├─ Abandons the work-in-progress tree (cheap: the current tree is untouched)
   ├─ Starts Render 3 with lanes = {Sync}: handles the keystroke immediately, commits
   └─ Later: restarts the transition from scratch, now including the new keystroke
```

It's a **restart, not a resume**. React throws away the partly-built WIP tree and begins that lane's work again on top of the newly committed state. This is why total work can *increase* under interruption and why render must be pure: the same component might be rendered several times, with only the last attempt committed.

## Rebasing: keeping updates consistent

If lower-priority updates are skipped and a higher-priority update commits first, the state must still end up **correct** when everything's applied, *in the original order*. React handles this with **rebasing** in each hook's update queue:

```text
Queue (in order):   u1 (Default)   u2 (Transition)   u3 (Default)
Render lanes = {Default}:
   process u1 → state S1
   u2 not in this render → SKIP it, and mark the queue "base state = S1"
   process u3 → but it came after u2, so it's kept in the queue to be re-applied on top of u2
   commit result: S1 + u3 shown   (urgent changes visible right away)
Later render lanes = {Transition}:
   start from base state S1, apply u2 then u3 in original order → final consistent state
```

You don't write this code, but it explains why **updater functions** (`setX(prev => …)`) are important in concurrent mode: skipped updates get replayed on top of a different base state, and functional updates compose correctly while captured-value updates can clobber each other.

## Starvation prevention

A steady stream of high-priority updates could starve low-priority ones forever. React tracks an **expiration time** per lane: a transition that's been waiting long enough is promoted so it can't be postponed indefinitely, and it eventually renders **without yielding** to finish. So transitions are non-urgent, not optional.

## Batching

All updates in the same lane that arrive before React renders are processed **together in one render**. That's the mechanism behind [automatic batching](../02-state-and-rendering/01-state-updates-and-batching.md): three `setState` calls in one click handler all get the Sync lane, so one render handles all three.

Sync-lane work is flushed promptly at the end of the event (in a microtask), without waiting for the Scheduler's macrotask. That's why a click-triggered update appears immediately, while default and transition lanes go through the time-sliced path.

## How the features you use map onto this

| API | Effect on scheduling |
|---|---|
| `startTransition` / `useTransition` | Updates inside get a **transition lane**: interruptible, replaceable, processed after urgent lanes ([01](../15-concurrent-and-modern-react/01-transitions.md)) |
| `useDeferredValue` | Renders first with the old value on the urgent lane, then schedules a **transition-lane** re-render with the new value ([02](../15-concurrent-and-modern-react/02-useDeferredValue.md)) |
| Suspense | A suspended render doesn't commit; when the promise resolves React schedules a **retry lane** render ([03](../15-concurrent-and-modern-react/03-suspense.md)) |
| `flushSync` | Forces a **sync** render and commit immediately; bypasses batching and time slicing |
| Plain `setState` in an event | Priority from the **event type** |
| Plain `setState` in a timer/promise | **Default** lane |

## What this explains

- **Why input stays responsive during a transition**: the keystroke is Sync lane and preempts the Transition lane.
- **Why a transition can render several times** before committing: restarts after interruptions.
- **Why a transition shows stale content for a while**: the committed UI is the old one until the transition's render completes.
- **Why `isPending` can be true while the old UI remains.**
- **Why a long single-component render still janks**: yielding happens between fibers only.
- **Why updater functions matter**: rebasing replays skipped updates on a different base.
- **Why effects don't run for interrupted renders**: they never commit.

## Seeing it yourself

- The React DevTools Profiler shows commits and which updates caused them. Compare an urgent update and a transition on the same interaction.
- The Chrome Performance panel shows React's work as many short tasks instead of one long one ([profiling](../14-performance/00-profiling-and-measuring.md#chrome-devtools)). Recent React versions can also add scheduler and component tracks there in development and profiling builds.
- A deliberately slow component (`const t = performance.now(); while (performance.now() - t < 50) {}`) rendered 100 times makes the effect obvious: with a transition, input stays instant; without it, typing freezes.

## Common misconceptions

- **"React runs on multiple threads."** No: one thread, cooperative yielding.
- **"Lanes are a public API."** They're internal. You influence them only through `startTransition`, `useDeferredValue`, `flushSync`, and event types.
- **"Interrupted work resumes where it left off."** It restarts from the root of that lane's work.
- **"Transitions are slower, so they never finish if I keep typing."** They're protected from starvation by expiration.
- **"The Scheduler can pause my function mid-render."** Only between fibers, never inside a component's own body.
- **"`startTransition` delays the update by a fixed time."** It lowers its priority; on an idle device it runs immediately.

## Common mistakes

- **Expecting transitions to speed up a heavy single render** rather than keep input responsive.
- **Capturing stale state in updates** instead of using updater functions, which matters more when updates get skipped and replayed.
- **Side effects or mutation in render**, which may run many times under restarts.
- **Putting urgent feedback in a transition** (controlled input state).
- **Using `flushSync` casually**, defeating batching and slicing.
- **Treating internals as API** (reading lane values, relying on exact priority behavior).

## Quick summary

- Two layers: the **Scheduler** slices work and yields to the browser (cooperative time slicing on one thread); **lanes** assign each update a priority.
- Lanes are **bitmask priorities**: sync (discrete input), input-continuous, default, **transition**, retry, idle. The event type, `startTransition`, and `useDeferredValue` determine an update's lane.
- React renders a **set of lanes**, skipping updates in other lanes and keeping them queued.
- A more urgent update **interrupts** lower-priority work by **discarding the work-in-progress tree** and restarting later, which is why render purity is non-negotiable.
- **Rebasing** replays skipped updates in order on the right base state; use functional updates.
- Starvation is prevented by expiration; same-lane updates batch into one render.
- It's all internal; learn the model, use the public APIs.

## Next

[04 — Event system](./04-event-system.md)
