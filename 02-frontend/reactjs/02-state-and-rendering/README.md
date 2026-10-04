# 02 — State and Rendering

How React remembers data between renders, and how it decides what to show on screen. This chapter is the mental model behind every hook, every bug about "stale values", and every performance discussion later in the repo. It's conceptual on purpose: the hook APIs live in [`../03-hooks/`](../03-hooks/README.md), while this chapter explains **why** they behave the way they do.

## What you'll be able to do after this chapter

- Explain why a normal variable can't hold UI data and what state adds
- Predict the value a handler sees, using the "state is a snapshot" model
- Update objects and arrays in state without mutating them
- Use updater functions and understand how updates are batched
- Structure state to avoid duplication and contradictions
- Decide when to lift state up
- Describe the trigger → render → commit cycle
- Map "mount, update, unmount" onto modern function components
- Predict when a component keeps its state and when it resets

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-state-and-snapshots.md](./00-state-and-snapshots.md) | Why state exists; state as a snapshot per render | `../01-fundamentals/` |
| 01 | [01-state-updates-and-batching.md](./01-state-updates-and-batching.md) | Updater functions, immutability, batching | 00 |
| 02 | [02-state-structure-and-lifting.md](./02-state-structure-and-lifting.md) | Choosing state shape; lifting state up; derived data | 01 |
| 03 | [03-rendering.md](./03-rendering.md) | Trigger, render, commit; purity; StrictMode | 00 |
| 04 | [04-component-lifecycle.md](./04-component-lifecycle.md) | Mount, update, unmount in function components | 03 |
| 05 | [05-state-preservation-and-reset.md](./05-state-preservation-and-reset.md) | When state persists or resets; the `key` technique | 03 |

`02` and `03` are independent after `01`/`00`; read them in order the first time.

## Boundary with other chapters

- **This chapter:** the mental model (snapshots, batching, render phases).
- **[`../03-hooks/01-useState.md`](../03-hooks/01-useState.md):** the `useState` API reference (lazy initialization, patterns).
- **[`../17-react-internals/`](../17-react-internals/README.md):** how Fiber, reconciliation, and scheduling implement all of this.

## Exercises

1. **Predict the output.** A button calls `setCount(count + 1)` three times in one handler. What does the counter show after one click? Fix it so it shows 3.
2. **Immutable updates.** Given `todos` in state, write handlers to add, remove, toggle, and rename a todo without mutating the array.
3. **Fix contradictory state.** Replace `isLoading`, `isError`, and `isSuccess` booleans with a single `status` value.
4. **Lift it up.** Two sibling components must show the same selected tab; move the state to their parent.
5. **Reset with a key.** Build a profile form that fully resets when you switch between users, without writing any reset code.

## Next

**[`../03-hooks/README.md`](../03-hooks/README.md)** covers the full set of hooks, starting with their rules.
