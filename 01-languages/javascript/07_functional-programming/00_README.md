# 07 · Functional Programming

Functional programming (FP) builds programs from **small, pure functions** that transform data without changing it. JavaScript is not a purely functional language, but its first-class functions, closures and array methods make FP a natural, practical style.

```
data ──► fn ──► fn ──► fn ──► result      (no hidden state, no mutation)
```

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [Pure Functions and Side Effects](./01_pure-functions-and-side-effects.md) | Purity, referential transparency, isolating effects |
| 2 | [Immutability](./02_immutability.md) | Updating data without mutation, `freeze`, structural sharing |
| 3 | [map, filter, reduce](./03_map-filter-reduce.md) | Declarative data pipelines and recipes |
| 4 | [Composition and Pipe](./04_composition-and-pipe.md) | Building functions from functions |
| 5 | [Currying and Partial Application](./05_currying-and-partial-application.md) | Configuring functions one argument at a time |
| 6 | [Point-Free Style](./06_point-free.md) | Tacit programming and when to stop |
| 7 | [Functional Patterns](./07_functional-patterns.md) | Functors, Maybe, Either, monads, transducers (intro) |

## Core ideas in one table

| Idea | Meaning |
|------|---------|
| First-class functions | Functions are values |
| Pure functions | Same input gives same output, no side effects |
| Immutability | Never change data, create new data |
| Declarative style | Say **what**, not **how** |
| Composition | Combine small functions into bigger ones |
| Higher-order functions | Functions that take or return functions |

## Goal

By the end you can write predictable, testable transformations, avoid shared-state bugs, and build pipelines from small reusable functions.

## Prerequisites

- [Higher-Order Functions](../02_functions/06_higher-order-functions.md)
- [Closures](../06_closures/01_closures.md)
- [Copying and Cloning](../03_objects-and-arrays/05_copying-and-cloning.md)

**Next:** [Pure Functions and Side Effects](./01_pure-functions-and-side-effects.md)
