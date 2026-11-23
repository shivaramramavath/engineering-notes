# Performance Fundamentals in JavaScript and V8

> Latency vs throughput, percentile distributions (p50/p95/p99), V8 compilation pipeline (Ignition and TurboFan), and performance budgets.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Memory Model](file:///e:/Learning/engineering-notes/01-languages/javascript/18_memory-and-garbage-collection/01_memory-model.md), [Execution Context and Call Stack](file:///e:/Learning/engineering-notes/01-languages/javascript/04_scope-and-execution/03_execution-context-and-call-stack.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, V8 Engine (Node.js & Chromium) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Performance in software engineering is frequently misunderstood as a pursuit of micro-syntactic optimizations—such as debating `for` vs `forEach` loops or bitwise hacks. In modern multi-tier architectures, real performance engineering is the disciplined study of **latency distributions**, **system throughput**, **hardware mechanical sympathy**, and the **Just-In-Time (JIT) compilation pipeline**.

JavaScript is not statically compiled to machine code upfront; neither is it purely interpreted. Modern engines like V8 employ a multi-tier adaptive compilation pipeline that dynamically balances fast startup time with peak optimized execution speed. Understanding this pipeline, alongside robust statistical measurement techniques (percentiles over averages), is essential for diagnosing bottlenecks and enforcing performance budgets in production.

---

## 2. Core Concepts

### Latency vs Throughput
- **Latency (Response Time)**: The duration required to process a single unit of work (e.g., time to complete one HTTP request or render one animation frame, measured in milliseconds).
- **Throughput**: The rate of completed work units per unit time (e.g., requests per second, transactions per second).
- A system can be high-throughput yet high-latency (e.g., large batch processing pipelines), or low-latency with constrained throughput.

### The Tyranny of the Average: Percentiles
Averages hide severe performance degradation:
- **Mean (Average)** is distorted by outliers. If 99 users experience 10ms response times and 1 user experiences 10,000ms, the mean is ~110ms—a number describing nobody's actual experience.
- **p50 (Median)**: 50% of requests are faster than this threshold.
- **p95 / p99**: 95% or 99% of requests are faster. Critical for distributed microservices: a web page issuing 20 backend calls will hit the p95 latency on almost every user interaction.

### The Multi-Tier V8 Compilation Pipeline
1. **Parser & AST**: Source code is parsed into an Abstract Syntax Tree (AST).
2. **Ignition Bytecode Interpreter**: Compiles AST into concise bytecode rapidly, enabling near-instant execution start.
3. **Sparkplug Baseline Compiler**: Non-optimizing compiler that translates Ignition bytecode directly into machine code without profiling, bridging the speed gap.
4. **Maglev Mid-Tier Compiler**: Generates fast machine code with medium optimizations.
5. **TurboFan Optimizing Compiler**: Analyzes type feedback gathered during execution ("hot functions") and generates highly optimized, vectorized machine code based on speculative optimizations.
6. **Deoptimization (Bailout)**: If dynamic runtime types violate TurboFan's speculative assumptions, the function is deoptimized and execution falls back to Ignition bytecode.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| JAVASCRIPT SOURCE CODE                                                   |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| PARSER & SCOPE ANALYSIS -> AST (Abstract Syntax Tree)                   |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| IGNITION INTERPRETER                                                     |
| - Fast startup, low memory footprint                                     |
| - Emits Bytecode & collects Type Feedback Vectors (IC)                  |
+--------------------------------------------------------------------------+
          |                                                 ^
          | Hot Code Path                                   | Deoptimization
          v                                                 | (Bailout)
+-----------------------------------------------------------+--------------+
| TURBOFAN OPTIMIZING COMPILER                                             |
| - Speculative Inlining                                                   |
| - Hidden Class (Shape) specialization                                    |
| - Loop unrolling, Dead Code Elimination                                  |
| - Emits Highly Optimized Native Machine Code                             |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### High-Resolution Timing with `performance.now()`
```javascript
import { performance } from 'node:perf_hooks';

const start = performance.now();
doHeavyWork();
const duration = performance.now() - start; // Fractional millisecond accuracy
console.log(`Execution took: ${duration.toFixed(3)}ms`);
```

### Performance Mark and Measure API
```javascript
performance.mark('work-start');
executeSubsystem();
performance.mark('work-end');

performance.measure('Subsystem Latency', 'work-start', 'work-end');

const entries = performance.getEntriesByName('Subsystem Latency');
console.log(`${entries[0].name}: ${entries[0].duration.toFixed(2)}ms`);
```

### V8 Trace Flags for JIT Compilation
```bash
# Log functions being compiled by TurboFan
node --trace-opt app.js

# Log functions being deoptimized and why (e.g. soft deopt, eager deopt)
node --trace-deopt app.js

# Print generated Ignition bytecode
node --print-bytecode app.js
```

---

## 5. Practical Examples

### Example 1: Accurate Percentile Calculator (p50, p90, p95, p99)
```javascript
export class LatencyHistogram {
  constructor() {
    this.samples = [];
  }

  record(durationMs) {
    this.samples.push(durationMs);
  }

  percentile(p) {
    if (this.samples.length === 0) return 0;
    this.samples.sort((a, b) => a - b);
    const index = Math.ceil((p / 100) * this.samples.length) - 1;
    return this.samples[Math.max(0, index)];
  }

  summary() {
    return {
      count: this.samples.length,
      p50: this.percentile(50).toFixed(2),
      p90: this.percentile(90).toFixed(2),
      p95: this.percentile(95).toFixed(2),
      p99: this.percentile(99).toFixed(2),
      max: this.percentile(100).toFixed(2),
    };
  }
}
```

### Example 2: Triggering and Observing TurboFan Optimization
```javascript
// Run with: node --trace-opt --trace-deopt test-jit.js

function add(a, b) {
  return a + b;
}

// 1. Warm-up Phase: Feed consistent types (Integers / Smis)
for (let i = 0; i < 200_000; i++) {
  add(i, i + 1);
}
// TurboFan observes type feedback: always Smis!
// Compiles `add` directly into native machine instruction `add eax, ebx`

// 2. Deoptimization Phase: Feed unexpected types (Strings)
add('hello', 'world');
// Speculation violated! TurboFan bails out and deoptimizes back to Ignition bytecode!
```

### Example 3: Function Inlining Limits
```javascript
// Small functions (typically < 600 bytecode bytes) are automatically inlined
// into calling sites by TurboFan, completely eliminating call stack overhead.
function square(x) {
  return x * x;
}

function calculateHypotenuse(a, b) {
  // TurboFan inlines `square` directly into calculateHypotenuse:
  // Math.sqrt((a * a) + (b * b))
  return Math.sqrt(square(a) + square(b));
}

for (let i = 0; i < 100_000; i++) {
  calculateHypotenuse(3, 4);
}
```

### Example 4: Micro-benchmark Trap: Dead Code Elimination
```javascript
import { performance } from 'node:perf_hooks';

// BAD BENCHMARK: TurboFan notices `result` is never used and eliminates the loop!
function flawedBenchmark() {
  const start = performance.now();
  let result = 0;
  for (let i = 0; i < 10_000_000; i++) {
    result += i;
  }
  // No return or consumer of `result`!
  console.log('Elapsed:', performance.now() - start); // Reports ~0ms!
}

// CORRECT BENCHMARK: Consume output to prevent dead code elimination
function robustBenchmark() {
  const start = performance.now();
  let result = 0;
  for (let i = 0; i < 10_000_000; i++) {
    result += i;
  }
  if (result === -1) console.log('unreachable'); // Consumes result
  console.log('Elapsed:', (performance.now() - start).toFixed(2));
  return result;
}
```

### Example 5: Implementing a Performance Budget Guard
```javascript
export class PerformanceBudgetGuard {
  constructor(budgets) {
    this.budgets = budgets; // { [metric: string]: number }
  }

  verify(metricName, actualDurationMs) {
    const budget = this.budgets[metricName];
    if (budget && actualDurationMs > budget) {
      console.warn(
        `[PERFORMANCE BUDGET EXCEEDED] ${metricName}: ` +
        `Actual ${actualDurationMs.toFixed(2)}ms > Budget ${budget}ms`
      );
      return false;
    }
    return true;
  }
}

// Usage in CI or telemetry:
const guard = new PerformanceBudgetGuard({
  'api:user:get': 50, // 50ms budget
  'dom:render:table': 16, // 16ms frame budget
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **JIT Warm-up Distortions**: Measuring execution time during the first few invocations of a function measures Ignition bytecode interpretation and compilation overhead rather than optimized steady-state speed. Always warm up functions before taking performance measurements.
2. **Date.now() Resolution**: `Date.now()` has coarse 1ms resolution and is subject to system clock synchronization adjustments (NTP steps). Never use `Date.now()` for performance profiling; always use `performance.now()` which provides monotonic sub-millisecond precision.
3. **Speculative Type Feedback Pollution**: If a polymorphic function receives 5 different object shapes, TurboFan transitions its Inline Cache (IC) to "Megamorphic" and stops optimizing property access, defaulting to slow hash-table lookups.
4. **Deoptimization Storms**: Code that repeatedly flips between types in hot loops causes repeated optimization-deoptimization cycles, introducing severe CPU throttling.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Reporting average latency instead of p95/p99 | Hides catastrophic tail-latency outliers | Track percentile histograms |
| Profiling with toy inputs in microbenchmarks | TurboFan optimizes away dead code or overfits shapes | Test under production-scale datasets |
| Premature micro-optimizations (e.g. `i++` vs `++i`) | Zero measurable difference in JIT-compiled bytecode | Focus on algorithmic complexity ($O(N)$) and I/O |
| Mixing types inside hot functions | Violates speculative type feedback, triggering deopts | Maintain monomorphic argument types |
| Measuring code with DevTools / debugger attached | Debugger hooks force V8 into unoptimized interpretation | Profile production release builds without debug hooks |
| Using `Date.now()` for benchmarking | Lacks sub-millisecond resolution and jumps on NTP clock updates | Use monotonic `performance.now()` |

---

## 8. Performance Characteristics

- **Ignition Bytecode vs TurboFan Machine Code**: TurboFan-compiled code typically runs 5x to 20x faster than raw Ignition interpreted bytecode.
- **Inlining Benefits**: Eliminating function call frames reduces CPU register spills, improves instruction pipeline caching, and allows downstream optimizations (constant folding, loop hoisting).

---

## 9. Engine Internals

1. **Sea of Nodes Representation**:
   - TurboFan translates bytecode into an Intermediate Representation (IR) called the "Sea of Nodes," which models both control flow and data dependencies as a unified directed graph.
   - Graph reduction passes eliminate redundant nodes, hoist invariant operations out of loops, and inline call nodes.
2. **Inline Caches (IC)**:
   - Every property access `obj.x` has an associated Inline Cache slot.
   - **Monomorphic** (1 shape observed): Direct offset lookup in machine code ($O(1)$ ~1 CPU cycle).
   - **Polymorphic** (2–4 shapes observed): Small conditional jump table checking map pointers.
   - **Megamorphic** ($> 4$ shapes): Global stub table lookup or dictionary lookup (slow).

---

## 10. Best Practices & Design Guidelines

- **Enforce Performance Budgets**: Establish quantitative thresholds (e.g., p95 $< 100$ms, LCP $< 2.5$s) in CI pipelines.
- **Keep Hot Functions Monomorphic**: Pass objects with identical property structures and initialization order to critical algorithms.
- **Keep Functions Small**: Functions under 600 bytecode bytes are prime candidates for speculative inlining by TurboFan.

---

## 11. Real-World Case Study

### Diagnosing a 10x Latency Regression in an In-Memory Matching Engine
```javascript
// THE REGRESSION: A trading engine's order matching loop degraded from 50,000 ops/s to 4,500 ops/s.
// Investigation via `node --trace-deopt` revealed:
// Reason: "Wrong shape at property access 'price'"

// BEFORE (Caused Megamorphic Deoptimization):
function createOrder(id, price, type) {
  const order = { id, price };
  if (type === 'LIMIT') order.limitDate = Date.now(); // Dynamic shape alteration!
  return order;
}

// AFTER (Maintains Strict Monomorphic Shape):
function createOrderOptimized(id, price, type) {
  return {
    id,
    price,
    type,
    limitDate: type === 'LIMIT' ? Date.now() : null, // Consistent property initialization!
  };
}

// Result: Inline cache remained Monomorphic; TurboFan generated optimal assembly;
// throughput restored to 52,000 ops/s.
```

---

## 12. Interview Questions

<details>
<summary>1. Why should latency always be evaluated using percentiles instead of averages?</summary>
Averages are heavily distorted by outliers and fail to reflect user experience. If 95% of requests take 10ms and 5% take 2000ms, the average is ~110ms, which misrepresents both groups. In distributed systems, a user request that triggers multiple internal queries will almost certainly encounter 95th or 99th percentile latencies.
</details>

<details>
<summary>2. What is the role of Ignition and TurboFan in V8?</summary>
Ignition is V8's fast bytecode interpreter that compiles AST into bytecode with near-zero startup delay and low memory usage, while collecting runtime type feedback. TurboFan is the optimizing compiler that takes frequently executed ("hot") bytecode and speculative type feedback to generate highly optimized native machine code.
</details>

<details>
<summary>3. What is a Deoptimization in V8 and when does it occur?</summary>
Deoptimization (or bailout) occurs when TurboFan has optimized a function based on speculative type assumptions (e.g., assuming an argument is always an integer), and dynamic execution violates that assumption (e.g., passing a string or object). TurboFan must discard the optimized machine code, reconstruct the interpreter stack frame, and fall back to Ignition bytecode.
</details>

<details>
<summary>4. What are the three states of an Inline Cache (IC)?</summary>
1. <b>Monomorphic</b>: The IC has observed exactly 1 hidden class (shape). Property access compiles to a single direct memory offset lookup.
2. <b>Polymorphic</b>: The IC has observed 2 to 4 different shapes. Implemented as a small inline switch table.
3. <b>Megamorphic</b>: The IC has observed more than 4 different shapes. Falls back to a generic global hash table lookup.
</details>

<details>
<summary>5. Why is Date.now() unsuitable for high-precision benchmarking?</summary>
<code>Date.now()</code> has only millisecond granularity and is tied to the system clock, meaning it can jump forwards or backwards during NTP time synchronizations. <code>performance.now()</code> provides monotonic timestamps with sub-millisecond (microsecond) precision that never move backwards.
</details>

<details>
<summary>6. What is Dead Code Elimination in V8?</summary>
It is an optimization where TurboFan identifies instructions whose results are never read or produce no observable side effects, completely removing them from the generated machine code.
</details>

<details>
<summary>7. What is Function Inlining and why is it beneficial?</summary>
Function inlining replaces a function call instruction with the actual body of the called function. This eliminates call stack frame allocation, parameter passing, and return jump instructions, while enabling downstream compiler optimizations across caller/callee boundaries.
</details>

<details>
<summary>8. What is the difference between latency and throughput?</summary>
Latency is the time it takes to complete a single task (measured in units of time, like milliseconds). Throughput is the number of tasks completed per unit of time (measured in operations per second or requests per second).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: High-Performance Timer Benchmark Runner
Write a robust benchmark function `runBenchmark(name, fn, iterations = 100_000)` that warms up the function before timing, prevents dead-code elimination, and outputs p50, p95, and p99 execution latencies.

<details>
<summary>View Solution</summary>

```javascript
import { performance } from 'node:perf_hooks';

export function runBenchmark(name, fn, iterations = 100_000) {
  // 1. Warm-up phase
  for (let i = 0; i < 10_000; i++) {
    fn(i);
  }

  // 2. Timed execution
  const latencies = new Float64Array(iterations);
  let dummySink = 0;

  for (let i = 0; i < iterations; i++) {
    const t0 = performance.now();
    dummySink += fn(i); // Prevent dead code elimination
    latencies[i] = performance.now() - t0;
  }

  latencies.sort();

  const p50 = latencies[Math.floor(iterations * 0.50)];
  const p95 = latencies[Math.floor(iterations * 0.95)];
  const p99 = latencies[Math.floor(iterations * 0.99)];

  console.log(`=== Benchmark: ${name} ===`);
  console.log(`p50: ${(p50 * 1000).toFixed(2)} µs`);
  console.log(`p95: ${(p95 * 1000).toFixed(2)} µs`);
  console.log(`p99: ${(p99 * 1000).toFixed(2)} µs`);

  if (dummySink === -1) console.log(dummySink); // Never reached
}
```
</details>

### Exercise 2: Speculative Type Deoptimization Observer
Write a Node.js script that demonstrates monomorphic versus polymorphic property access overhead by measuring execution duration of property lookups across 1 shape versus 6 distinct shapes.

<details>
<summary>View Solution</summary>

```javascript
import { performance } from 'node:perf_hooks';

function accessPrice(item) {
  return item.price;
}

// Case 1: Monomorphic (1 Shape)
const monoObjects = Array.from({ length: 1_000_000 }, (_, i) => ({ price: i }));
// Warm up
for (let i = 0; i < 10_000; i++) accessPrice(monoObjects[i]);

const startMono = performance.now();
let sumMono = 0;
for (let i = 0; i < 1_000_000; i++) {
  sumMono += accessPrice(monoObjects[i]);
}
const timeMono = performance.now() - startMono;
console.log(`Monomorphic Time: ${timeMono.toFixed(2)}ms`);

// Case 2: Megamorphic (6 Shapes)
const shapeFactories = [
  i => ({ price: i }),
  i => ({ a: 1, price: i }),
  i => ({ b: 1, c: 2, price: i }),
  i => ({ d: 1, e: 2, f: 3, price: i }),
  i => ({ g: 1, h: 2, i: 3, j: 4, price: i }),
  i => ({ k: 1, l: 2, m: 3, n: 4, o: 5, price: i }),
];
const megaObjects = Array.from({ length: 1_000_000 }, (_, i) => shapeFactories[i % 6](i));

const startMega = performance.now();
let sumMega = 0;
for (let i = 0; i < 1_000_000; i++) {
  sumMega += accessPrice(megaObjects[i]);
}
const timeMega = performance.now() - startMega;
console.log(`Megamorphic Time: ${timeMega.toFixed(2)}ms`);
```
</details>
