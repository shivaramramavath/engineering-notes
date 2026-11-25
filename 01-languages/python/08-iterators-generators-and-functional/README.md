# Module: Iterators, Generators & Functional Programming (`08-iterators-generators-and-functional`)

> **Scope**: Iterator protocols (`__iter__` and `__next__`), generators and `yield from`, streaming generator pipelines, the `itertools` algebra suite, comprehensions deep dive, lazy evaluation strategies, pure functional programming patterns, the `operator` module, and the historical evolution towards modern `async`/`await`.

---

## 📋 Module Overview

| Metric | Details |
|---|---|
| **Directory** | `01-languages/python/08-iterators-generators-and-functional` |
| **Status** | 100% Complete (9/9 Notes Authored & Verified) |
| **Previous Module** | [07-exceptions-and-debugging](../07-exceptions-and-debugging/README.md) |
| **Next Module** | [09-files-and-io](../09-files-and-io/README.md) |

---

## 🎯 Learning Outcomes
By completing this module, you will:
1. Master the iterator protocol and understand how CPython's `GET_ITER` and `FOR_ITER` drive loops.
2. Build memory-efficient, single-pass streaming data pipelines processing gigabytes of data in constant $O(1)$ memory.
3. Utilize `itertools` and `operator` primitives to write idiomatic, C-accelerated iterator transformations.
4. Deepen architectural mastery of Python coroutines from historical PEP 342 generators to modern native `async def` and `await`.

---

## 📑 Syllabus & Notes Directory

*All concept notes in this module strictly follow the 13-section standard (`_templates/concept-template.md`).*

| # | Note | Description | Key Focus Areas |
|---|---|---|---|
| **00** | [00-iterator-protocol.md](00-iterator-protocol.md) | The Core Iterator Protocol | `__iter__` vs `__next__`, `StopIteration`, self-iterability, sentinel `iter(callable, sentinel)`. |
| **01** | [01-generators-and-yield-from.md](01-generators-and-yield-from.md) | Generators & Yield From | Generator functions, `yield from` delegation, return values, two-way communication (`send`, `throw`, `close`). |
| **02** | [02-generator-pipelines.md](02-generator-pipelines.md) | Streaming Data Pipelines | Unix-pipe composition, pull-driven architecture, $O(1)$ RAM processing, memory benchmarking. |
| **03** | [03-itertools.md](03-itertools.md) | The Itertools Algebra Suite | Infinite iterators, `batched()` (3.12+), `pairwise()` (3.10+), `groupby` sorting invariants, combinatorics. |
| **04** | [04-comprehensions-deep-dive.md](04-comprehensions-deep-dive.md) | Comprehensions Deep Dive | List/dict/set/generator syntax, `LIST_APPEND` opcodes, scope isolation, walrus operator `:=` leakage. |
| **05** | [05-lazy-evaluation.md](05-lazy-evaluation.md) | Lazy Evaluation & Call-by-Need | Infinite sequences, `@cached_property` non-data descriptors, thunks, eager validation vs lazy execution. |
| **06** | [06-functional-patterns.md](06-functional-patterns.md) | Functional Programming Patterns | First-class functions, pure functions, `functools.partial` C-dispatch, `reduce()` fold operations, currying. |
| **07** | [07-operator-module.md](07-operator-module.md) | The Operator Standard Library | `itemgetter`, `attrgetter` dotted paths, `methodcaller`, sorting performance benchmarks. |
| **08** | [08-coroutines-via-generators-history.md](08-coroutines-via-generators-history.md) | Coroutines Evolution History | PEP 342 `.send()`, PEP 380 `yield from`, PEP 3156, PEP 492 `async def` / `await`, `__await__` protocol. |
