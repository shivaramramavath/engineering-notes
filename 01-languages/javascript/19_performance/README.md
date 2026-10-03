# 19 · Performance

Fast software feels better, costs less to run, and scales further. But performance work is easy to get wrong: people optimize the wrong thing, trade away readability for no measurable gain, or "fix" problems that were never there. This chapter teaches a **measure-first** approach: define what fast means, find the real bottleneck with a profiler, fix it, and verify the fix.

## What you will learn

- How to define and measure performance: latency, throughput, percentiles, budgets
- How JavaScript engines make code fast, and what slows them down
- Profiling in the browser (DevTools) and in Node (`--cpu-prof`, `--inspect`, flame graphs)
- Browser-specific performance: Core Web Vitals, rendering, layout, network, bundle size
- Node-specific performance: event-loop health, I/O, HTTP, workers, databases
- Reusable optimization patterns: caching, batching, laziness, debouncing, algorithmic improvements

## Contents

| # | File | Topic |
|---|------|-------|
| 01 | [Performance Fundamentals](./01_performance-fundamentals.md) | Metrics, percentiles, budgets, the measure-first mindset, how engines optimize |
| 02 | [Profiling](./02_profiling.md) | Benchmarking, DevTools, Node profilers, flame graphs, tracing |
| 03 | [Browser Performance](./03_browser-performance.md) | Core Web Vitals, rendering pipeline, layout thrashing, loading strategies |
| 04 | [Node Performance](./04_node-performance.md) | Event loop, I/O, HTTP servers, workers, databases, production tuning |
| 05 | [Optimization Patterns](./05_optimization-patterns.md) | Algorithms, caching, batching, lazy work, data structures, concurrency |

## Prerequisites

- [Event Loop](../12_event-loop/00_README.md) and [Asynchronous JavaScript](../11_asynchronous-javascript/00_README.md)
- [Memory and Garbage Collection](../18_memory-and-garbage-collection/00_README.md)
- [Concurrency and Parallelism](../17_concurrency-and-parallelism/00_README.md)
- [Node.js](../16_nodejs/00_README.md) and [DOM and Browser](../14_dom-and-browser/00_README.md)

## The performance loop

```
1. Define a goal      "p95 API latency under 200 ms", "LCP under 2.5 s"
        │
2. Measure            realistic data, realistic load, production-like environment
        │
3. Find the bottleneck  profile; do not guess
        │
4. Change one thing   smallest fix with the biggest effect
        │
5. Verify             measure again; keep the change only if it helps
        │
        └──────▶ back to 1 until the goal is met, then stop
```

## Key takeaways

- Performance is a feature with a measurable target, not a vague wish
- Measure first, optimize the bottleneck, and verify the result
- Most wins come from doing less work (algorithms, caching, fewer requests), not clever micro-tricks
- Browser performance is mostly about the network, the main thread, and rendering
- Node performance is mostly about keeping the event loop free and using I/O well
- Stop when you reach the goal: extra optimization costs clarity

**Next:** [Performance Fundamentals](./01_performance-fundamentals.md)