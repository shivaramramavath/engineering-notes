# JavaScript Map, Filter & Reduce: Declarative Data Pipelines & Transducers

> Comprehensive exploration of functional iteration pipelines, higher-order sequence transformations, Catamorphism via `reduce`, intermediate array allocation hazards, and single-pass Transducers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [02_functions/06_higher-order-functions.md](../02_functions/06_higher-order-functions.md), [03_objects-and-arrays/07_array-methods.md](../03_objects-and-arrays/07_array-methods.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 Array Higher-Order Methods |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Map, Filter, Reduce as Mathematical Operations](#2-core-concepts-map-filter-reduce-as-mathematical-operations)
3. [How It Works: Chained Pipelines vs. Single-Pass Iteration](#3-how-it-works-chained-pipelines-vs-single-pass-iteration)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Loop Fusion & Allocation Overhead](#9-engine-internals-v8-loop-fusion--allocation-overhead)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: High-Throughput Log Stream Aggregator](#11-real-world-case-study-high-throughput-log-stream-aggregator)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In traditional imperative programming, iterating over data requires initializing index counters, tracking termination bounds, and mutating temporary accumulator variables with `for` or `while` loops. Imperative loops mix **how** to iterate with **what** business computation to perform.

Functional programming separates iteration mechanics from business logic using three foundational higher-order operations:
- **`map`**: Transforms each element in a sequence independently ($A \to B$).
- **`filter`**: Selects a subset of elements that satisfy a predicate ($A \to \text{Boolean}$).
- **`reduce`**: Folds (accumulates) a sequence of values into a single summary structure ($(\text{Acc}, A) \to \text{Acc}$).

Mastering this functional triad enables declarative, readable, and composable data transformation pipelines.

---

## 2. Core Concepts: Map, Filter, Reduce as Mathematical Operations

```
                             The Functional Triad
                                       │
         ┌─────────────────────────────┼─────────────────────────────┐
         ▼                             ▼                             ▼
        map                          filter                        reduce
   [ 1, 2, 3 ]                    [ 1, 2, 3 ]                   [ 1, 2, 3 ]
        │ (x => x * 2)                 │ (x => x > 1)                │ ((acc, x) => acc + x, 0)
        ▼                              ▼                             ▼
   [ 2, 4, 6 ]                      [ 2, 3 ]                         6
   Length: Preserved (N)          Length: 0 <= K <= N           Output: Single Arbitrary Type
```

### The Universality of Reduce (Catamorphism)
`reduce` is the most primitive and powerful of the three: **any** list transformation can be expressed in terms of `reduce`. Both `map` and `filter` can be implemented using `reduce`.

---

## 3. How It Works: Chained Pipelines vs. Single-Pass Iteration

When methods are chained:
```javascript
const result = data
  .filter(x => x > 0)    // Step 1: Allocates Intermediate Array 1
  .map(x => x * 2)       // Step 2: Allocates Intermediate Array 2
  .slice(0, 10);         // Step 3: Allocates Final Array
```

```
Chained Pipeline Memory Allocation
[ Raw Data: 1M items ] ──► .filter() ──► [ Intermed Array: 500k items ]
                                                │
                                                ▼
                                         .map()
                                                │
                                                ▼
                                         [ Intermed Array: 500k items ]
                                                │
                                                ▼
                                         [ Final Output: 10 items ]
```

For small datasets ($<10,000$ items), chained pipelines are fast and highly readable. For millions of records, intermediate array allocations generate severe Garbage Collection (GC) pressure. This motivates single-pass transformations and **Transducers**.

---

## 4. Syntax & API Reference

### Signatures
```javascript
// Map: Transformed Array of identical length
arr.map((item, index, array) => newItem, thisArg);

// Filter: Subset Array containing items where predicate returns truthy
arr.filter((item, index, array) => Boolean(match), thisArg);

// Reduce: Accumulated value of any type
arr.reduce((accumulator, item, index, array) => nextAccumulator, initialValue);
```

---

## 5. Practical Examples

### Example 1: Basic Pipeline Transforming User Records
```javascript
const users = [
  { id: 1, name: "Alice", active: true, score: 85 },
  { id: 2, name: "Bob",   active: false, score: 40 },
  { id: 3, name: "Carol", active: true, score: 92 },
  { id: 4, name: "David", active: true, score: 60 }
];

// Pipeline: Active users with score >= 70, mapped to badge strings
const leaderBadges = users
  .filter(u => u.active && u.score >= 70)
  .map(u => `${u.name} (Score: ${u.score})`);

console.log(leaderBadges);
// Output: [ 'Alice (Score: 85)', 'Carol (Score: 92)' ]
```

### Example 2: Re-implementing Map and Filter Using Reduce
```javascript
// Re-implementing map via reduce
function myMap(array, transformFn) {
  return array.reduce((acc, item, index) => {
    acc.push(transformFn(item, index, array));
    return acc;
  }, []);
}

// Re-implementing filter via reduce
function myFilter(array, predicate) {
  return array.reduce((acc, item, index) => {
    if (predicate(item, index, array)) {
      acc.push(item);
    }
    return acc;
  }, []);
}

console.log(myMap([1, 2, 3], x => x * 10)); // [10, 20, 30]
console.log(myFilter([1, 2, 3, 4], x => x % 2 === 0)); // [2, 4]
```

### Example 3: Edge Case with Missing `initialValue` in Reduce
```javascript
// DANGER: Omitting initialValue on empty array throws TypeError!
try {
  [].reduce((acc, val) => acc + val);
} catch (err) {
  console.log("Empty reduce error:", err.message);
  // Reduce of empty array with no initial value
}

// DANGER: Omitting initialValue when folding objects
const inventory = [{ count: 5 }, { count: 10 }, { count: 15 }];

// BUG: accumulator starts as { count: 5 }, then evaluates { count: 5 } + 10 -> "[object Object]10"!
const badTotal = inventory.reduce((acc, item) => acc + item.count);
console.log("Bad total:", badTotal); // "[object Object]1015"

// FIX: Always provide explicit initial accumulator
const goodTotal = inventory.reduce((acc, item) => acc + item.count, 0);
console.log("Good total:", goodTotal); // 30
```

### Example 4: Failure Mode with Spreading Accumulator in Reduce ($O(n^2)$ Trap)
```javascript
// FAILURE MODE: Spreading the accumulator inside reduce creates O(n^2) time complexity!
function slowDedupe(arr) {
  return arr.reduce((acc, item) => {
    // Spreading acc duplicates the entire array on EVERY iteration!
    return acc.includes(item) ? acc : [...acc, item];
  }, []);
}

// FIX: Mutate the local accumulator inside the reduce callback (Pure on outside!)
function fastDedupe(arr) {
  const seen = new Set();
  return arr.reduce((acc, item) => {
    if (!seen.has(item)) {
      seen.add(item);
      acc.push(item); // O(1) push on local accumulator
    }
    return acc;
  }, []);
}

console.log(fastDedupe([1, 2, 2, 3, 1, 4])); // [1, 2, 3, 4]
```

### Example 5: Idiomatic Single-Pass Transducer Pattern
```javascript
// Transducers compose mapping and filtering into a single pass with NO intermediate arrays
const mapTransducer = fn => nextReducer => (acc, input) => nextReducer(acc, fn(input));
const filterTransducer = predicate => nextReducer => (acc, input) =>
  predicate(input) ? nextReducer(acc, input) : acc;

// Standard array push reducer
const pushReducer = (acc, item) => {
  acc.push(item);
  return acc;
};

// Compose transformations: double even numbers
const isEven = filterTransducer(x => x % 2 === 0);
const double = mapTransducer(x => x * 2);

// Compose: double(isEven(pushReducer))
const composedTransform = double(isEven(pushReducer));

const sourceData = [1, 2, 3, 4, 5, 6];
const result = sourceData.reduce(composedTransform, []);

console.log("Transducer single-pass result:", result); // [4, 8, 12]
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`map` Skips Empty Holes**: Calling `[1, , 3].map(x => x * 2)` produces `[2, <1 empty item>, 6]`. The callback is never invoked for empty holes.
2. **`parseInt` in `map`**: Writing `['1', '2', '10'].map(parseInt)` returns `[1, NaN, 2]` because `map` passes `(element, index)` and `parseInt` treats `index` as the radix base!
3. **`filter` Does Not Clone Elements**: `filter` creates a new outer array, but elements inside the array share identical memory references with the original.
4. **`reduce` on Single Element Without Initial Value**: If an array has exactly 1 element and no `initialValue` is passed, `reduce` returns that single element immediately **without invoking the callback function**.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Omitting `initialValue` in reduce | `arr.reduce((acc, x) => acc + x.price)` | `arr.reduce(..., 0)` | Fails on empty arrays; casts initial object to string. |
| Spreading accumulator in reduce | `acc => [...acc, item]` | `acc.push(item); return acc;` | Turns $O(n)$ iteration into $O(n^2)$ catastrophic slowdown. |
| Using `map` for side effects | `arr.map(x => save(x))` | `arr.forEach(x => save(x))` | Allocates an unused array that immediately becomes garbage. |
| Mutating input objects inside `map` | `arr.map(x => { x.v = 1; return x; })` | `arr.map(x => ({ ...x, v: 1 }))` | Violates purity and mutates caller's objects. |
| Passing multi-arg functions to `map` | `['1', '2'].map(parseInt)` | `['1', '2'].map(Number)` | `map` passes index as radix, yielding `NaN`. |
| Excessive chaining on huge datasets | `.filter().map().filter().map()` | Combine into single reduce/transducer | Allocates 3 massive intermediate arrays in heap memory. |

---

## 8. Performance Characteristics

- **Time Complexity**: `map`, `filter`, and `reduce` each run in $O(n)$ linear time.
- **Memory Footprint**:
  - `map` and `filter` each allocate a new array of size $O(n)$ or $O(k)$.
  - A chain of 3 operations allocates 2 intermediate arrays that must be garbage-collected.
- **V8 Ignition / TurboFan Optimization**: TurboFan inlines simple callbacks directly into the traversal loop. However, V8 does **not** perform automatic loop fusion across chained `.filter().map()` calls.

---

## 9. Engine Internals: V8 Loop Fusion & Allocation Overhead

Inside Google V8:
1. When evaluating `arr.filter(p).map(m)`:
   - V8 allocates an internal `JSArray` for `filter`.
   - It iterates `arr`, invoking `p` and pushing to the new array.
   - It then invokes `map` on that newly allocated array, allocating a *second* `JSArray`.
2. Although compilers in functional languages (like Haskell or Rust) perform **Loop Fusion** (merging filter and map into a single machine loop), V8 does not perform loop fusion on array methods because JavaScript callbacks can produce observable side effects during iteration.
3. Transducers manually achieve loop fusion at the JavaScript level.

---

## 10. Best Practices & Design Guidelines

1. **Always Supply Initial Accumulator to `reduce`**: Never call `reduce` without a second argument unless computing simple numeric reductions on non-empty arrays.
2. **Combine Adjacent Operations When Processing Large Data**: If processing $>50,000$ items, combine `filter` and `map` into a single `reduce` pass or `for` loop to avoid intermediate memory allocations.
3. **Keep Callbacks Pure**: Ensure callbacks passed to `map` and `filter` do not mutate external state or input parameters.

---

## 11. Real-World Case Study: High-Throughput Log Stream Aggregator

A telemetry ingestion service aggregating API server access logs to calculate average latency, error rates, and endpoints requiring optimization.

```javascript
class LogAnalyticsAggregator {
  static analyzeLogs(rawLogEntries) {
    // Single-pass reduction: computes metrics without allocating intermediate arrays
    return rawLogEntries.reduce(
      (stats, log) => {
        // 1. Update global counters
        stats.totalRequests++;
        stats.totalLatencyMs += log.durationMs;

        // 2. Track status codes
        if (log.statusCode >= 500) {
          stats.serverErrors++;
        } else if (log.statusCode >= 400) {
          stats.clientErrors++;
        }

        // 3. Aggregate per-route statistics
        const route = log.path;
        if (!stats.routes[route]) {
          stats.routes[route] = { count: 0, totalMs: 0, errors: 0 };
        }
        stats.routes[route].count++;
        stats.routes[route].totalMs += log.durationMs;
        if (log.statusCode >= 400) stats.routes[route].errors++;

        return stats;
      },
      {
        totalRequests: 0,
        totalLatencyMs: 0,
        serverErrors: 0,
        clientErrors: 0,
        routes: {},
        get averageLatencyMs() {
          return this.totalRequests === 0 ? 0 : parseFloat((this.totalLatencyMs / this.totalRequests).toFixed(2));
        }
      }
    );
  }
}

// Verification
const mockLogs = [
  { path: "/api/users", statusCode: 200, durationMs: 45 },
  { path: "/api/users", statusCode: 500, durationMs: 120 },
  { path: "/api/checkout", statusCode: 200, durationMs: 250 },
  { path: "/api/checkout", statusCode: 400, durationMs: 30 }
];

const report = LogAnalyticsAggregator.analyzeLogs(mockLogs);
console.log("Total Requests:", report.totalRequests);       // 4
console.log("Average Latency:", report.averageLatencyMs);     // 111.25 ms
console.log("Server Errors:", report.serverErrors);         // 1
console.log("Checkout Route:", report.routes["/api/checkout"]);
// { count: 2, totalMs: 280, errors: 1 }
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. Why is <code>reduce</code> considered the most fundamental array transformation method?</strong></summary>

`reduce` is a catamorphism: an operation that deconstructs an algebraic data structure into a single summary value. Because the accumulator can be of any arbitrary type (including an array, object, Map, or primitive), both `map` and `filter` can be implemented directly using `reduce`.
</details>

<details>
<summary><strong>2. What is the performance danger of writing <code>acc = [...acc, item]</code> inside <code>reduce</code>?</strong></summary>

Using array spread inside a reduce loop copies all existing accumulated items into a newly allocated array on **every single iteration**. If an array has $N$ items, the engine performs $1 + 2 + 3 + ... + N = \frac{N(N+1)}{2}$ operations, degrading performance from linear $O(n)$ to quadratic $O(n^2)$.
</details>

<details>
<summary><strong>3. Why does <code>['1', '2', '3'].map(parseInt)</code> return <code>[1, NaN, NaN]</code>?</strong></summary>

`Array.prototype.map` passes three arguments to its callback: `(element, index, array)`. `parseInt` accepts two arguments: `(string, radix)`. 
Therefore, `map` executes:
- `parseInt('1', 0)`: Radix 0 evaluates to decimal 10 -> `1`.
- `parseInt('2', 1)`: Radix 1 is mathematically invalid -> `NaN`.
- `parseInt('3', 2)`: Radix 2 (binary) cannot parse digit '3' -> `NaN`.
</details>

<details>
<summary><strong>4. What is a Transducer, and what problem does it solve in JavaScript?</strong></summary>

A Transducer is a composable higher-order reducer that decouples transformation logic from the underlying data structure. It solves the performance penalty of chained array methods (`.filter().map().slice()`) by fusing multiple transformation steps into a **single pass** over the data with zero intermediate array allocations.
</details>

<details>
<summary><strong>5. How does <code>reduce</code> behave when called on an array with a single element?</strong></summary>

- If an `initialValue` is provided, the callback is invoked once with `(initialValue, singleElement)`.
- If NO `initialValue` is provided, `reduce` returns the single element immediately **without ever invoking the callback**.
</details>

<details>
<summary><strong>6. What is the difference between <code>Array.prototype.flatMap()</code> and chaining <code>.map().flat()</code>?</strong></summary>

`arr.map(fn).flat()` allocates an intermediate array containing mapped sub-arrays, which immediately becomes garbage before being flattened. `arr.flatMap(fn)` computes elements and flattens them into the target array in a single pass without allocating the intermediate container.
</details>

<details>
<summary><strong>7. Why does V8 not automatically fuse chained <code>.filter().map()</code> calls?</strong></summary>

JavaScript callbacks can have observable side effects (such as modifying global variables, logging, or mutating the host array). Because the ECMAScript specification guarantees that all iterations of `filter` finish before the first iteration of `map` begins, the engine cannot safely fuse the loops without potentially altering observable program behavior.
</details>

<details>
<summary><strong>8. Does <code>Array.prototype.filter</code> produce a deep copy of elements?</strong></summary>

No. `filter` produces a shallow copy of the array container. If the elements inside the array are objects, the filtered array contains references to the exact same objects in memory. Mutating an object inside the filtered array mutates the object in the original array.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Grouping by Property via Reduce (Easy)
Implement `groupBy(array, key)` using `reduce` that partitions objects by property value without using native `Object.groupBy`.

<details>
<summary>View Solution</summary>

```javascript
function groupBy(array, key) {
  return array.reduce((acc, item) => {
    const val = item[key];
    if (!acc[val]) acc[val] = [];
    acc[val].push(item);
    return acc;
  }, {});
}

const data = [{ dept: "IT", name: "Alice" }, { dept: "HR", name: "Bob" }, { dept: "IT", name: "Charlie" }];
console.log(groupBy(data, "dept"));
// { IT: [ { dept: 'IT', ... }, { dept: 'IT', ... } ], HR: [ { dept: 'HR', ... } ] }
```
</details>

### Exercise 2: Object Frequency Map via Reduce (Easy)
Write a function `countFrequencies(words)` that returns a count of each word in an array using `reduce`.

<details>
<summary>View Solution</summary>

```javascript
function countFrequencies(words) {
  return words.reduce((acc, word) => {
    acc[word] = (acc[word] || 0) + 1;
    return acc;
  }, {});
}

console.log(countFrequencies(["apple", "banana", "apple", "orange", "banana", "apple"]));
// { apple: 3, banana: 2, orange: 1 }
```
</details>

### Exercise 3: Partitioning Array via Single Reduce (Medium)
Implement `partition(arr, predicate)` returning `[truthyArr, falsyArr]` in a single pass using `reduce`.

<details>
<summary>View Solution</summary>

```javascript
function partition(arr, predicate) {
  return arr.reduce(
    ([pass, fail], item) => {
      if (predicate(item)) {
        pass.push(item);
      } else {
        fail.push(item);
      }
      return [pass, fail];
    },
    [[], []]
  );
}

const [evens, odds] = partition([1, 2, 3, 4, 5, 6], n => n % 2 === 0);
console.log("Evens:", evens); // [2, 4, 6]
console.log("Odds:", odds);   // [1, 3, 5]
```
</details>

### Exercise 4: Flattening Arbitrary Depth via Reduce (Medium)
Implement a recursive function `flattenDeep(arr)` using `reduce` and recursion without native `flat()`.

<details>
<summary>View Solution</summary>

```javascript
function flattenDeep(arr) {
  return arr.reduce((acc, item) => {
    return Array.isArray(item) ? acc.concat(flattenDeep(item)) : acc.concat(item);
  }, []);
}

console.log(flattenDeep([1, [2, [3, [4, 5]]]])); // [1, 2, 3, 4, 5]
```
</details>

### Exercise 5: Asynchronous Sequential Pipeline via Reduce (Hard)
Implement `asyncReducePipeline(initialValue, ...asyncFns)` where each asynchronous function receives the resolved output of the preceding function.

<details>
<summary>View Solution</summary>

```javascript
function asyncReducePipeline(initialValue, ...asyncFns) {
  return asyncFns.reduce(
    (promiseChain, currentFn) => promiseChain.then(currentFn),
    Promise.resolve(initialValue)
  );
}

const step1 = async n => n + 10;
const step2 = async n => n * 2;
const step3 = async n => `Final: ${n}`;

asyncReducePipeline(5, step1, step2, step3).then(res => console.log(res));
// Output: "Final: 30" ((5 + 10) * 2 = 30)
```
</details>

### Exercise 6: Complete Transducer Pipeline Builder (Hard)
Build a generic `transduce(iterable, xform, reducer, initialValue)` engine supporting both Arrays and Sets with single-pass iteration.

<details>
<summary>View Solution</summary>

```javascript
function transduce(iterable, xform, reducer, initialValue) {
  const transformedReducer = xform(reducer);
  let acc = initialValue;

  for (const item of iterable) {
    acc = transformedReducer(acc, item);
  }

  return acc;
}

// Transducer building blocks
const filterX = predicate => step => (acc, val) => (predicate(val) ? step(acc, val) : acc);
const mapX = fn => step => (acc, val) => step(acc, fn(val));

// Test with Set output
const addReducer = (setAcc, item) => {
  setAcc.add(item);
  return setAcc;
};

// Compose: only numbers > 2, squared
const xform = step => filterX(n => n > 2)(mapX(n => n * n)(step));

const source = [1, 2, 3, 4, 5];
const outputSet = transduce(source, xform, addReducer, new Set());

console.log("Transduced Set:", Array.from(outputSet)); // [9, 16, 25]
```
</details>
