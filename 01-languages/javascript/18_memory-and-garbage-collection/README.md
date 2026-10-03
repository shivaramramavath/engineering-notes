# 18 · Memory and Garbage Collection

JavaScript manages memory for you: you create values, the engine allocates space, and a **garbage collector** frees what you no longer use. That convenience hides important details. Understanding where values live, when they become collectable, and what keeps them alive is the difference between an app that runs for months and one that slowly eats all available RAM.

## What you will learn

- How values are stored: primitives, objects, the call stack, and the heap
- How garbage collection works in V8 (reachability, generations, mark-sweep-compact)
- The common sources of memory leaks and how to find them with DevTools and Node tools
- Practical techniques to reduce memory use and GC pressure

## Contents

| # | File | Topic |
|---|------|-------|
| 01 | [Memory Model](./01_memory-model.md) | Stack vs heap, primitives vs objects, references, object layout |
| 02 | [Garbage Collection](./02_garbage-collection.md) | Reachability, mark-and-sweep, generational GC, `WeakRef`, `FinalizationRegistry` |
| 03 | [Memory Leaks](./03_memory-leaks.md) | Common leak patterns, heap snapshots, detection workflow |
| 04 | [Memory Optimization](./04_memory-optimization.md) | Allocation habits, object pooling, typed arrays, streaming |

## Prerequisites

- [Data Types](../01_fundamentals/02_data-types.md): primitives vs objects
- [Closures](../06_closures/00_README.md), especially [Closure Pitfalls](../06_closures/03_closure-pitfalls.md)
- [Scope and Execution](../04_scope-and-execution/00_README.md): call stack and lexical environments
- [WeakMap, WeakSet, WeakRef](../09_built-in-objects/07_weakmap-weakset-weakref.md)

## The big picture

```
 Call stack                         Heap
┌──────────────┐          ┌─────────────────────────────┐
│ main()       │          │  { name: 'Ada' }  ◀──┐      │
│  user ───────┼─────────▶│  [1, 2, 3]           │      │
│  count = 42  │          │  function () {...} ◀─┼──┐   │
└──────────────┘          │  Map { ... }         │  │   │
                          └──────────────────────┼──┼───┘
   GC roots: stack, globals, active closures ────┘  │
                                                    │
   An object is garbage when no path from a root reaches it
```

## Key takeaways

- Primitives are small values; objects, arrays, functions, and closures live on the heap
- The GC frees objects that are **unreachable**, not objects you are "done with"
- A leak is memory that is still reachable but no longer needed
- Measure with heap snapshots and allocation profiles instead of guessing
- Most optimization is about allocating less, retaining less, and releasing references

**Next:** [Memory Model](./01_memory-model.md)