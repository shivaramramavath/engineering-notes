# 17 — React Internals

You can build React apps for years without knowing how React works inside. But the moment something behaves strangely (state resets, an effect fires twice, a list item keeps the wrong input value, typing lags) the explanation lives in the internals. This folder builds a **mental model** of what React does between "you called `setState`" and "pixels changed".

> **Read these as mental models, not source code.** React's internals are private and change between versions. Names like *fiber*, *lanes*, and *work loop* are real and stable enough to learn, but field names, flag values, and algorithms get reworked. Where a detail is simplified, the note says so. The goal is to predict behavior, not to memorize implementation.

## The pipeline

```text
setState / render() ─► SCHEDULE ─► RENDER PHASE ─────────────► COMMIT PHASE ──► browser paints
                       (03)        call components,              apply DOM changes,
                                   diff results (01)             run layout effects (sync),
                                   build the fiber tree (00)     passive effects after paint
                                   interruptible                 synchronous, can't be interrupted
                                   must be pure
```

- **Schedule** (03): decide *when* and *at what priority* to do the work.
- **Render phase** (00, 01, 02): call your components (running hooks, 02), compare the output with the previous tree (reconciliation, 01), and record what changed on **fibers** (00). It's *interruptible* and can be restarted or thrown away.
- **Commit phase**: apply the recorded changes to the DOM in one synchronous pass, then run effects.
- **Events** (04) are how user input enters the system and starts the whole cycle.

## Prerequisites

- [Rendering](../02-state-and-rendering/03-rendering.md): triggers, render, and commit at the user level
- [Hooks](../03-hooks/README.md): you'll learn *why* the rules exist
- [Concurrent rendering](../15-concurrent-and-modern-react/00-concurrent-rendering.md): what interruptible rendering enables

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Fiber architecture](./00-fiber-architecture.md) | What a fiber is, the two trees, the work loop, render vs commit |
| 01 | [Reconciliation](./01-reconciliation.md) | The diffing heuristics, keys, why state resets |
| 02 | [How hooks work](./02-how-hooks-work.md) | Hooks as a linked list on the fiber, closures, effects timing |
| 03 | [Scheduler and lanes](./03-scheduler-and-lanes.md) | Time slicing, priorities, how transitions get interrupted |
| 04 | [Event system](./04-event-system.md) | Delegation, synthetic events, bubbling through portals |

## Suggested order

00 → 01 → 02 gives the core. 03 builds on 00 and explains the concurrent features. 04 is independent.

## Why this is worth learning

- **Debugging**: "why did this state reset?" and "why is this effect stale?" have mechanical answers.
- **Performance**: you can reason about what a change costs instead of guessing ([performance](../14-performance/README.md)).
- **Interviews**: reconciliation, keys, hooks ordering, and batching are staple questions ([interview prep](../23-interview/02-rendering-and-internals.md)).
- **Confidence with new features**: transitions, Suspense, and the compiler stop being magic.
