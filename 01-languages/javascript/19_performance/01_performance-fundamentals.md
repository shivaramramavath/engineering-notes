# Performance Fundamentals

Before changing any code you need to know **what** you are measuring, **how** to measure it, and **what good looks like**. This file covers the vocabulary, the mindset, and the mental model of how JavaScript engines run your code.

## What "fast" means

| Metric | Meaning | Typical unit |
|--------|---------|--------------|
| **Latency** | Time for one operation to complete | ms |
| **Throughput** | Operations completed per unit of time | requests/s, ops/s |
| **Response time** | Latency as the caller sees it (includes queueing) | ms |
| **Load time** | Time until a page is usable | s |
| **Responsiveness** | How quickly the UI reacts to input | ms |
| **Memory use** | Heap, RSS, bundle size | MB |
| **Cost** | CPU, memory, bandwidth, and money | $/month |

Latency and throughput interact: a system can have high throughput and poor latency (large batches), or low latency and low throughput (one request at a time). Decide which one your users care about.

## Averages lie: use percentiles

An average hides slow outliers. Report **percentiles**:

| Percentile | Meaning |
|-----------|---------|
| p50 (median) | Half of the requests are faster than this |
| p95 | 95% are faster; 1 in 20 is slower |
| p99 | 1 in 100 is slower |
| max | The worst case |

```js
function percentile(sortedAsc, p) {
  const i = Math.ceil((p / 100) * sortedAsc.length) - 1;
  return sortedAsc[Math.max(0, i)];
}

const times = [12, 15, 14, 13, 250, 16, 14, 15, 13, 900].sort((a, b) => a - b);
const mean = times.reduce((a, b) => a + b, 0) / times.length;   // 126.2: describes nobody's experience
percentile(times, 50);   // 14
percentile(times, 95);   // 900
```

A page that makes 20 backend calls will hit the p99 of at least one of them most of the time, so **tail latency** matters more than it seems.

## Set a budget

A **performance budget** is a limit you enforce: if a change exceeds it, the change is rejected or fixed.

| Area | Example budget |
|------|----------------|
| API | p95 < 200 ms at 500 requests/s |
| Page load | LCP < 2.5 s, INP < 200 ms, CLS < 0.1 (75th percentile of real users) |
| Bundle | Initial JavaScript < 170 KB compressed |
| Memory | Heap < 512 MB under peak load |
| Build/CI | Tests under 5 minutes |

Without a target you cannot tell whether you are done.

## The measure-first mindset

> Premature optimization is the root of all evil. (Donald Knuth, in context: about small efficiencies in non-critical code.)

Guidelines:

1. **Make it correct, then clear, then fast** (only if needed)
2. **Measure** with realistic data and load; microbenchmarks on toy inputs mislead
3. **Find the bottleneck**: typically a small part of the code accounts for most of the time (the 80/20 rule)
4. **Optimize the bottleneck**, not the code that looks slow
5. **Re-measure**; keep changes that help, revert those that do not
6. **Stop** at the goal

Amdahl's law: speeding up a part that is 10% of total time can improve the whole by at most 10%, no matter how much faster you make it.

## Where time goes

| Layer | Examples | Typical cost |
|-------|----------|--------------|
| Network | DNS, TCP/TLS handshake, round trips, payload size | 10s to 100s of ms per round trip |
| Disk / database | Queries, missing indexes, N+1 queries | ms to seconds |
| CPU (JavaScript) | Loops, parsing, serialization, regex | µs to seconds |
| Memory / GC | Allocation pressure, large heaps | Occasional ms pauses |
| Rendering (browser) | Style, layout, paint, composite | Frames of ~16 ms |

Orders of magnitude (approximate):

| Operation | Time |
|-----------|------|
| CPU register / simple arithmetic | ~1 ns |
| Main memory access | ~100 ns |
| SSD read | ~100 µs |
| Same-datacenter network round trip | ~0.5 ms |
| Cross-continent round trip | ~100 to 150 ms |

Most real slowness is **waiting on I/O** or **doing unnecessary work**, not slow JavaScript syntax.

## Algorithmic complexity

How cost grows with input size *n* usually matters far more than constant factors.

| Complexity | Name | n = 1,000 | n = 1,000,000 | Example |
|-----------|------|-----------|---------------|---------|
| O(1) | Constant | 1 | 1 | `Map.get`, array index |
| O(log n) | Logarithmic | 10 | 20 | Binary search |
| O(n) | Linear | 1,000 | 1,000,000 | `for` loop, `indexOf` |
| O(n log n) | Linearithmic | 10,000 | 20,000,000 | `Array.prototype.sort` |
| O(n²) | Quadratic | 1,000,000 | 10¹² | Nested loops, repeated `includes` |
| O(2ⁿ) | Exponential | huge | impossible | Naive recursive Fibonacci |

```js
// O(n²): for each item, scan the whole other list
const common = a.filter((x) => b.includes(x));

// O(n): build a Set once, then constant-time lookups
const setB = new Set(b);
const common2 = a.filter((x) => setB.has(x));
```

For n = 100,000 the first version performs about 10 billion comparisons; the second about 200,000 operations. No micro-optimization beats a better algorithm.

Watch for hidden quadratics:

```js
let result = [];
for (const item of items) result = [...result, item];     // copies the array each time: O(n²)

let text = '';
for (const line of lines) text = text.concat(line);       // generally OK in engines (ropes), but measure

items.shift();                                             // O(n) on large arrays; use an index or a queue
```

## How JavaScript engines make code fast

V8 (Chrome, Node.js, Deno) uses tiers:

| Tier | Role |
|------|------|
| **Parser** | Turns source into an AST (lazily parses functions not yet called) |
| **Ignition** | Interpreter that runs bytecode immediately and collects type feedback |
| **Sparkplug** | Fast baseline compiler (no heavy optimization) |
| **Maglev** | Mid-tier optimizing compiler |
| **TurboFan** | Top-tier optimizing compiler for hot code, using type feedback |

Code starts in the interpreter. As functions run often ("hot"), the engine compiles them to machine code with assumptions based on what it observed (for example "this argument is always a small integer"). If an assumption breaks, the engine **deoptimizes** back to slower code.

Other engines (SpiderMonkey in Firefox, JavaScriptCore in Safari) have similar multi-tier designs.

### What helps the engine

| Habit | Why |
|-------|-----|
| **Monomorphic code**: a function sees the same types and object shapes every call | Enables fast property access and inlining |
| **Consistent object shapes**: initialize all fields in the same order | Hidden classes stay shared |
| **Homogeneous arrays** (all numbers or all objects) | Compact element kinds |
| **Small, simple hot functions** | Easier to inline |
| **Typed arrays for numeric data** | No boxing, predictable layout |
| **Avoiding `delete`, `arguments` tricks, and changing array/object kinds in hot paths** | Avoids dictionary mode and deopts |

```js
// Polymorphic: add() sees numbers, strings, and arrays: slower
function add(a, b) { return a + b; }
add(1, 2); add('a', 'b'); add([1], [2]);

// Monomorphic: one type in the hot path
function addNumbers(a, b) { return a + b; }
```

Do not contort ordinary code for these effects. They matter in **hot paths** that profiling identified.

### Warm-up

The first calls run slowly (interpreted, then compiled). Benchmarks must warm up, and short scripts may never reach the optimizing tiers.

## JavaScript is single-threaded: the cost of blocking

Browser main thread and Node's main thread each run **one thing at a time**. Long synchronous work delays everything else:

| Environment | Effect of a long task |
|-------------|-----------------------|
| Browser | Input lag, dropped frames, "Page Unresponsive" |
| Node server | Every concurrent request waits |

Rule of thumb for the browser: keep tasks under **50 ms** (a "long task" is anything longer). Tools: break work into chunks and yield, move it to a Web Worker, or reduce it. See [Web Workers](../17_concurrency-and-parallelism/02_web-workers.md) and [Worker Threads](../17_concurrency-and-parallelism/03_worker-threads.md).

## Perceived performance

Users judge speed by what they **see and feel**, not by raw numbers.

| Technique | Effect |
|-----------|--------|
| Show something early (skeleton screens, streaming HTML) | Feels faster than a blank page |
| Optimistic UI (update first, confirm later) | Instant feedback |
| Progress indicators for long tasks | Reduces perceived wait |
| Prefetch likely next actions | Next step feels instant |
| Keep interactions under ~100 ms | Feels immediate |

Rough human thresholds: ~100 ms feels instant, ~1 s keeps the flow of thought, ~10 s loses attention.

## Trade-offs

Optimizations often cost something. Be explicit about it:

| Gain | Possible cost |
|------|---------------|
| Caching (speed) | Memory, stale data, invalidation bugs |
| Batching (throughput) | Higher latency for individual items |
| Parallelism (speed) | Complexity, memory, coordination bugs |
| Compression (bandwidth) | CPU time |
| Denormalized data (read speed) | Write complexity, consistency |
| Precomputation (response time) | Storage, staleness |
| Micro-optimized code (speed) | Readability and maintainability |

## Performance anti-patterns

| Anti-pattern | Better |
|--------------|--------|
| Guessing the bottleneck | Profile |
| Optimizing before the code is correct or has a goal | Set a target, measure |
| Benchmarking in a different environment from production | Use realistic data, hardware, and load |
| Trusting averages | Track p50, p95, p99 |
| Chasing micro-benchmarks | Measure end-to-end behavior as well |
| Premature caching with no invalidation plan | Cache measured hot spots with clear expiry |
| Ignoring the network | Count round trips and bytes first |
| Adding libraries without checking their cost | Check bundle size and runtime cost |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Optimizing code that is 1% of the runtime | No visible gain | Optimize the dominant cost |
| Using the mean to judge latency | Hides slow requests | Percentiles |
| Single-run measurements | Noise, warm-up, GC | Many runs; report spread |
| Microbenchmarks on tiny inputs | Engine may eliminate or inline the work | Realistic inputs and sizes |
| Assuming new syntax is slower or faster without measuring | Engines change constantly | Measure on your targets |
| Making code unreadable for a gain nobody can measure | Costs maintenance | Revert it |
| Fixing client code when the server is slow | Wrong layer | Trace the whole request |

## Key takeaways

- Define goals with metrics and budgets; use percentiles, not averages
- Find the bottleneck by measuring; most time goes to I/O and unnecessary work
- Algorithmic complexity usually beats micro-optimization
- V8 optimizes hot code using type feedback: consistent types and shapes help
- Never block the main thread (browser) or the event loop (Node) with long synchronous work
- Perceived performance is part of performance
- Every optimization has a trade-off: write it down and keep only what pays off

**Next:** [Profiling](./02_profiling.md)