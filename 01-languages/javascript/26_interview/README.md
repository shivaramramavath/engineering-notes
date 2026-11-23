# JavaScript Interview Mastery (26_interview)

> An exhaustive, staff-level interview curriculum designed for senior and lead JavaScript engineers. Covers runtime semantics, execution context internals, async event queues, output prediction puzzles, browser rendering, V8 optimization pipelines, machine coding architectures, and standard polyfills from scratch.

---

## Curriculum Structure

This module equips candidates with deep theoretical and empirical command over JavaScript. Moving beyond standard interview trivia, each guide drills into exact ECMAScript specifications, V8 engine internals, memory models, and production-tested patterns.

```
                              SENIOR INTERVIEW CURRICULUM
                                           │
         ┌───────────────────┬─────────────┴─────────────┬───────────────────┐
         ▼                   ▼                           ▼                   ▼
    [Core & Scopes]     [Async & DOM]              [Performance]       [Coding & Polyfills]
    - Language Core     - Event Loop Internals     - V8 JIT & Hidden   - Machine Coding (45m)
    - Closures & Heap   - DOM, CRP & Storage         Classes & GC      - Specification
    - this & Prototypes - Tricky Output Drills     - INP & CWV Profiling Polyfills
```

---

## Topic Catalog & Study Guides

| Module Code | Topic Guide | Key Senior Concepts | Target Role Level | File Link |
| :--- | :--- | :--- | :--- | :--- |
| `INT-00` | Language Core & Runtime Foundations | Abstract Operations (`ToPrimitive`), Tagged Smis, TDZ, IEEE 754 floats | Senior / Staff | [00_language-core-qa.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/00_language-core-qa.md) |
| `INT-01` | Closures, Scope Chains & Memory Retention | Heap Context Allocation, Meteor closure leak, Escaping vs Stack locals | Senior / Staff | [01_closures-and-scope-qa.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/01_closures-and-scope-qa.md) |
| `INT-02` | `this` Context, Prototypes & Object Models | 4 binding rules, Arrow lexical capture, `[[Get]]` receiver, IC Map transitions | Senior / Staff | [02_this-and-prototypes-qa.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/02_this-and-prototypes-qa.md) |
| `INT-03` | Asynchronous Architecture & Event Loop Internals | Libuv 6 phases vs Browser Microtasks, `process.nextTick`, Desugared `await` | Senior / Staff | [03_async-and-event-loop-qa.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/03_async-and-event-loop-qa.md) |
| `INT-04` | Output-Based Tricky Questions & Riddles | 15 code prediction challenges, sparse arrays, parameter scope shadowing | Senior / Staff | [04_output-based-questions.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/04_output-based-questions.md) |
| `INT-05` | DOM, Browser Architecture & Web APIs | W3C Propagation (Capturing/Bubbling), Critical Rendering Path, Layout Thrashing | Senior / Staff | [05_dom-and-browser-qa.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/05_dom-and-browser-qa.md) |
| `INT-06` | Engine Internals, V8 Optimization & Memory Profiling | Ignition/Sparkplug/Maglev/Turbofan, Monomorphic ICs, Orinoco Generational GC | Staff / Principal | [06_performance-qa.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/06_performance-qa.md) |
| `INT-07` | Senior Machine Coding Practical Challenges | $O(1)$ LRU Cache, Async Backoff with Jitter, Promise Pool, Flatten/Unflatten | Senior / Staff | [07_machine-coding-rounds.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/07_machine-coding-rounds.md) |
| `INT-08` | Standard Library Polyfills From Scratch | `Function.prototype.myBind/Call/Apply`, `Array.prototype.myReduce`, `Promise.myAll` | Senior / Staff | [08_polyfills-from-scratch.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/08_polyfills-from-scratch.md) |

---

## Recommended Preparation Sequence

1. **Foundations (Days 1–2)**: Review `00_language-core-qa.md` and `01_closures-and-scope-qa.md`. Ensure you can explain stack allocation vs heap context promotion.
2. **Object Model & Concurrency (Days 3–4)**: Master `02_this-and-prototypes-qa.md` and `03_async-and-event-loop-qa.md`. Walk through the microtask drain order step-by-step.
3. **Drills & Browsers (Days 5–6)**: Solve all 15 puzzles in `04_output-based-questions.md` and review Critical Rendering Path mechanics in `05_dom-and-browser-qa.md`.
4. **V8 Optimization (Day 7)**: Study `06_performance-qa.md`. Understand why modifying prototypes or deleting properties de-optimizes Turbofan JIT code.
5. **Hands-On Coding (Days 8–10)**: Implement the 4 projects in `07_machine-coding-rounds.md` and all polyfills in `08_polyfills-from-scratch.md` from memory under timed conditions.
