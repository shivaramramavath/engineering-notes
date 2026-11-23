# Loops, Iteration Protocols & Labeled Statements

> An exhaustive technical reference on JavaScript repetition constructs: classic `for`, `while`, `do...while`, `for...of` (Iteration Protocol), `for...in` (prototype traversal hazards), labeled statements, and engine optimization mechanics.

| Metadata | Details |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_variables.md](./01_variables.md), [06_conditionals.md](./06_conditionals.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Standard** | ES6 (ES2015) to Current (ES2024/ES2025) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works: The ECMAScript Iteration Protocol](#3-how-it-works-the-ecmascript-iteration-protocol)
4. [Syntax & Loop Construct Reference](#4-syntax--loop-construct-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases & Gotchas](#6-edge-cases--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Loop Unrolling & Bounds Check Elimination](#9-engine-internals-v8-loop-unrolling--bounds-check-elimination)
10. [Best Practices & Style Guide](#10-best-practices--style-guide)
11. [Real-World Applications & Case Studies](#11-real-world-applications--case-studies)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Loops automate repetitive computation by executing a statement block across a range of numbers, collections, streams, or conditions.

JavaScript offers four distinct categories of iteration:
1. **Index-Based Counting Loops**: `for (let i = 0; i < len; i++)` — provides fine-grained index control and raw maximum CPU performance.
2. **Conditional While Loops**: `while (condition)` and `do...while (condition)` — optimal for polling, state machines, and unknown iteration counts.
3. **Iterable Traversal**: `for...of` — consumes the standard ECMAScript **Iteration Protocol** (`Symbol.iterator`), supporting Arrays, Maps, Sets, Strings, and custom generators.
4. **Property Enumeration**: `for...in` — walks an object's enumerable keys, including its **prototype chain** (dangerous for arrays!).

```
┌────────────────────────────────────────────────────────────────────────┐
│                        JavaScript Iteration Matrix                     │
├─────────────────┬───────────────────┬────────────────┬─────────────────┤
│ Construct       │ Target Data       │ Yields         │ Prototype Walk? │
├─────────────────┼───────────────────┼────────────────┼─────────────────┤
│ `for (;;)`      │ Arrays / Indices  │ Index (number) │ No              │
│ `while`         │ Any Condition     │ Custom         │ No              │
│ `for...of`      │ Iterables         │ Values         │ No              │
│ `for...in`      │ Objects           │ Keys (strings) │ **YES (Risky)** │
│ `arr.forEach()` │ Arrays            │ Value + Index  │ No              │
└─────────────────┴───────────────────┴────────────────┴─────────────────┘
```

Selecting the proper loop determines whether your algorithm supports `break`/`continue`, allows async iteration (`for await...of`), or avoids catastrophic prototype pollution bugs.

---

## 2. Core Concepts

### 2.1 The Classic `for` Loop and `let` Binding
A classic `for` loop contains three header expressions: `for (initialization; condition; final-expression)`.
- **Per-Iteration Lexical Binding**: When declaring `for (let i = 0; ...)` with `let`, the ECMAScript specification guarantees that a **brand-new lexical scope binding is instantiated for every loop iteration**. This allows closures created inside the loop body to capture the exact value of `i` at that iteration.
- With `var`, a single function-scoped variable is shared across all iterations.

### 2.2 `for...of`: The Iteration Protocol
Introduced in ES6, `for...of` operates on any object implementing the **Iterable Protocol**: an object with a method keyed by `[Symbol.iterator]()` that returns an iterator object with a `.next()` method.
- Supported built-ins: `Array`, `String`, `Map`, `Set`, `TypedArray`, `NodeList`, `arguments`.
- **Does NOT work on plain objects `{}`**: Objects do not implement `[Symbol.iterator]`. To iterate an object, use `Object.keys()`, `Object.values()`, or `Object.entries()`.

### 2.3 `for...in`: Property Enumeration Hazards
`for...in` iterates over all **enumerable property names** (keys) of an object:
1. Returns keys as **strings**, never numbers (even for array indices).
2. **Traverses the Prototype Chain**: Inspects all enumerable properties on the object's prototypes!
3. **Unspecified Order**: Key order is historical and non-deterministic across symbol and string properties.
4. **Never Use on Arrays**: If a third-party library adds a helper method to `Array.prototype`, `for...in` will yield that method's name as an element key. Always use `Object.hasOwn(obj, key)` if `for...in` must be used.

### 2.4 Control Jumps: `break`, `continue`, and Labeled Statements
- `break`: Terminates the innermost loop immediately.
- `continue`: Skips the remainder of the current iteration and jumps to the update expression / next condition.
- **Labeled Statements**: Prefixing a loop with an identifier (`outerLoop: for (...)`) allows `break outerLoop;` or `continue outerLoop;` to jump out of multiple nested loop levels.

---

## 3. How It Works: The ECMAScript Iteration Protocol

The `for...of` loop is syntactic sugar over the two-part Iteration Protocol:

```mermaid
sequenceDiagram
    autonumber
    participant Loop as for...of Engine
    participant Iterable as Target (e.g. Array)
    participant Iter as Iterator Instance

    Loop->>Iterable: Call [Symbol.iterator]()
    Iterable-->>Loop: Returns Iterator instance
    
    loop Each Iteration
        Loop->>Iter: Call .next()
        Iter-->>Loop: Returns { value: item, done: boolean }
        alt done === false
            Loop->>Loop: Bind item to loop variable, execute body
        else done === true
            Loop->>Loop: Exit loop
        end
    end
```

If an error or `break` occurs during a `for...of` loop, the engine checks for an optional `.return()` cleanup method on the iterator, ensuring file descriptors and database connections are closed.

---

## 4. Syntax & Loop Construct Reference

### 4.1 Loop Declaration Forms

```javascript
// 1. Classic indexed loop
for (let i = 0; i < array.length; i++) {
  console.log(array[i]);
}

// 2. While loop (entry condition)
while (queue.length > 0) {
  processItem(queue.shift());
}

// 3. Do-While loop (guaranteed at least one execution)
do {
  retryCount++;
} while (!connect() && retryCount < 3);

// 4. for...of loop (destructuring iteration)
for (const [key, value] of map.entries()) {
  console.log(`${key} => ${value}`);
}

// 5. Labeled multi-level loop
searchGrid: for (let r = 0; r < rows; r++) {
  for (let c = 0; c < cols; c++) {
    if (grid[r][c] === target) break searchGrid;
  }
}
```

---

## 5. Practical Examples

### Example 1: Performance Benchmark Across Iteration Strategies

```javascript
// Benchmark comparing indexed for, for...of, and forEach over 10,000,000 elements
const size = 10_000_000;
const buffer = new Float64Array(size);
for (let i = 0; i < size; i++) buffer[i] = i * 0.5;

// 1. Classic for loop
console.time("Classic For");
let sum1 = 0;
for (let i = 0; i < buffer.length; i++) {
  sum1 += buffer[i];
}
console.timeEnd("Classic For"); // ~8ms (V8 compiles to tight CPU SIMD loop)

// 2. for...of loop
console.time("For...Of");
let sum2 = 0;
for (const val of buffer) {
  sum2 += val;
}
console.timeEnd("For...Of"); // ~14ms (Minor iterator overhead)

// 3. Array.prototype.forEach
console.time("forEach");
let sum3 = 0;
buffer.forEach((val) => {
  sum3 += val;
});
console.timeEnd("forEach"); // ~42ms (Function call overhead per element)
```

### Example 2: Prototype Pollution Defense in Property Enumeration

```javascript
// Simulating an unsafe third-party library augmenting Object.prototype
Object.prototype.injectedHelper = function () { return true; };

const record = { id: 101, username: "Grace" };

// FLAWED: for...in traverses the prototype chain
console.log("Unfiltered for...in keys:");
for (const key in record) {
  console.log(`- ${key}`); // Logs: id, username, injectedHelper!
}

// SAFE APPROACH 1: Object.hasOwn() check
console.log("Filtered for...in keys:");
for (const key in record) {
  if (Object.hasOwn(record, key)) {
    console.log(`- ${key}`); // Logs: id, username
  }
}

// SAFE APPROACH 2: Object.keys() / Object.entries() (Preferred)
console.log("Object.keys (Only own enumerable properties):");
for (const key of Object.keys(record)) {
  console.log(`- ${key}`); // Logs: id, username
}

// Clean up polluted prototype
delete Object.prototype.injectedHelper;
```

### Example 3: Sequential Async Queue Processing with `for...of`

```javascript
/**
 * Processes asynchronous tasks in strict sequential order.
 * Array.prototype.forEach CANNOT pause or await properly!
 */
async function processTasksSequentially(tasks) {
  const results = [];

  for (const task of tasks) {
    console.log(`Starting task ${task.id}...`);
    // Pauses loop until current promise resolves before moving to next iteration
    const result = await task.execute();
    results.push(result);
    console.log(`Task ${task.id} completed.`);
  }

  return results;
}

const mockTasks = [
  { id: 1, execute: () => new Promise((res) => setTimeout(() => res("A"), 10)) },
  { id: 2, execute: () => new Promise((res) => setTimeout(() => res("B"), 10)) }
];

processTasksSequentially(mockTasks).then((res) => console.log("Final:", res));
```

### Example 4: Implementing a Custom Iterable Generator Class

```javascript
class NumberRange {
  constructor(start, end, step = 1) {
    this.start = start;
    this.end = end;
    this.step = step;
  }

  /**
   * Generator function implementing ECMAScript Iteration Protocol
   */
  *[Symbol.iterator]() {
    for (let current = this.start; current <= this.end; current += this.step) {
      yield current;
    }
  }
}

const range = new NumberRange(10, 50, 10);

// Consumed directly by for...of
for (const val of range) {
  console.log(`Range value: ${val}`); // 10, 20, 30, 40, 50
}

// Consumed by spread operator and Array.from
console.log([...range]); // [10, 20, 30, 40, 50]
```

### Example 5: Labeled Loop Matrix Coordinate Search

```javascript
function findCell(matrix, predicate) {
  let matched = null;

  rowLoop: for (let r = 0; r < matrix.length; r++) {
    for (let c = 0; c < matrix[r].length; c++) {
      if (predicate(matrix[r][c])) {
        matched = { row: r, col: c, value: matrix[r][c] };
        // Instantly exits both nested loops without flags
        break rowLoop;
      }
    }
  }

  return matched;
}

const table = [
  ["A", "B", "C"],
  ["D", "TARGET", "F"],
  ["G", "H", "I"]
];

console.log(findCell(table, (val) => val === "TARGET"));
// Output: { row: 1, col: 1, value: 'TARGET' }
```

---

## 6. Edge Cases & Gotchas

### 6.1 Mutating Collections During Iteration
Modifying an array while iterating over it with an index counter skips elements because indices shift:
```javascript
const items = [1, 2, 3, 4, 5];

// BUG: Deleting during forward iteration
for (let i = 0; i < items.length; i++) {
  if (items[i] === 2) {
    items.splice(i, 1); // Removes '2'. '3' shifts to index 1!
    // i increments to 2, skipping the number 3 entirely!
  }
}
console.log(items); // [1, 3, 4, 5]

// SOLUTION: Iterate backwards when deleting items:
const safeList = [1, 2, 3, 4, 5];
for (let i = safeList.length - 1; i >= 0; i--) {
  if (safeList[i] === 2) {
    safeList.splice(i, 1);
  }
}
console.log(safeList); // [1, 3, 4, 5]
```

### 6.2 `const` in `for` vs. `for...of`
- In `for (const item of list)`: **Valid**. A new block scope and new `const` binding is created on each pass.
- In `for (const i = 0; i < 5; i++)`: **TypeError**. The `i++` operation attempts to reassign the constant identifier `i` in the header!

### 6.3 `return` Inside `forEach` Does Not Return From Enclosing Function
```javascript
function findElement(arr) {
  arr.forEach((item) => {
    if (item === 42) {
      return item; // Only returns from the inner anonymous callback, NOT from findElement!
    }
  });
  return -1;
}
console.log(findElement([1, 42, 3])); // -1! (Not 42!)

// Correct approach: use a for...of loop:
function findElementClean(arr) {
  for (const item of arr) {
    if (item === 42) return item; // Returns from findElementClean immediately
  }
  return -1;
}
console.log(findElementClean([1, 42, 3])); // 42
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Using `for...in` on arrays** | Iterates over array properties as strings and traverses prototype extensions. | Use `for...of` or classic indexed `for`. |
| **Using `await` inside `forEach`** | `forEach` ignores promises returned by callbacks; calls run concurrently without waiting. | Use standard `for...of` with `await`. |
| **Using `for (const i = 0; i < n; i++)`** | Throws `TypeError: Assignment to constant variable` on `i++`. | Use `for (let i = 0; ...)`. |
| **Modifying array length during forward loop** | Causes index drift and skips elements. | Iterate in reverse (`i--`) or use `.filter()`. |
| **Using `for...of` on plain `{}`** | Plain objects do not implement `Symbol.iterator`, throwing `TypeError`. | Use `for (const [k, v] of Object.entries(obj))`. |
| **Creating functions inside loops with `var`** | All functions capture the same shared `var` binding. | Use `let` so each iteration gets an isolated binding. |

---

## 8. Performance Characteristics

In high-throughput loops over 100,000+ elements:

| Loop Construct | V8 JIT Optimization Mode | Relative Speed |
| :--- | :--- | :--- |
| **Classic Indexed `for`** | TurboFan Bounds-Check Elimination (BCE) + Inlined CPU increments | **Fastest** (~1.0x baseline) |
| **`for...of` on TypedArray / Array** | Optimized iterator inlining (Zero allocation in TurboFan) | Very Fast (~1.1x–1.3x) |
| **`Array.prototype.forEach`** | Closure allocation + Function call overhead per element | Moderate (~3.5x–4.5x slower) |
| **`for...in` on Objects** | Requires prototype chain verification and string property hash lookup | **Slowest** (~8x–15x slower) |

---

## 9. Engine Internals: V8 Loop Unrolling & Bounds Check Elimination

When V8's TurboFan compiler optimizes a classic counting loop:
1. **Loop Unrolling**: For small, fixed-size loops (e.g. 4–8 iterations), TurboFan unrolls the loop into linear assembly instructions, eliminating jump and branch check instructions.
2. **Bounds-Check Elimination (BCE)**: In an array access `arr[i]`, JavaScript engines normally perform safety checks to verify that `i >= 0` and `i < arr.length` to avoid out-of-bounds memory reads.
   - If TurboFan proves that `i` starts at 0 and increments strictly until `i < arr.length`, it **completely eliminates the bounds checks inside the loop body**, reducing memory access to a raw single CPU pointer offset.

---

## 10. Best Practices & Style Guide

1. **Default to `for...of` for General Iteration**: Clean, declarative, supports `break`, `continue`, `return`, and handles destructuring cleanly.
2. **Use Classic `for` When Index Calculation or Maximum Performance is Required**: Essential for high-performance math, game engines, graphics buffers, and algorithms requiring reverse traversal.
3. **Never Use `for...in` on Arrays**: Strictly reserve `for...in` for inspecting plain objects with `Object.hasOwn()`, or prefer `Object.keys()` / `Object.entries()`.
4. **Use `for await...of` for Async Streams**: Cleanly consume Node.js readable streams and async generators.

---

## 11. Real-World Applications & Case Studies

### Case Study: Memory Leak in Asynchronous Batch Processor
A backend batch-processing service ran out of memory processing 50,000 records:
```javascript
// BUGGY IMPLEMENTATION:
async function processBatch(records) {
  records.forEach(async (record) => {
    // BUG: forEach is synchronous; it launches 50,000 asynchronous DB requests simultaneously!
    await database.save(record);
  });
}
```
- **Consequence**: The application opened 50,000 concurrent database sockets in a fraction of a second, causing socket exhaustion and crash loop restarts.
- **Resolution**: Replaced `forEach` with a sequential `for...of` loop with controlled batching chunks:
  ```javascript
  async function processBatchClean(records) {
    for (const record of records) {
      await database.save(record); // Clean, backpressure-safe sequential execution
    }
  }
  ```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. Explain the difference between `for...in` and `for...of`.</strong></summary>

**Answer:**
- `for...in` iterates over **enumerable property names (keys)** of an object, returning them as strings. It traverses both the object's own properties and its prototype chain.
- `for...of` iterates over **values** produced by an iterable object conforming to the ECMAScript Iteration Protocol (`Symbol.iterator`). It does not inspect keys or traverse prototype properties.
</details>

<details>
<summary><strong>2. Why does `for (const x of arr)` work without throwing an assignment error?</strong></summary>

**Answer:**
In a `for...of` loop, the ECMAScript specification dictates that a brand-new lexical environment record is created for each iteration. The identifier `x` is declared and initialized with that iteration's value in this new scope. Because `x` is never reassigned within that scope, declaring it with `const` is 100% valid.

In contrast, a classic `for (const i = 0; i < 5; i++)` reuses the same header scope where `i++` attempts to reassign the constant binding, triggering a `TypeError`.
</details>

<details>
<summary><strong>3. What happens if you call `break` or `return` inside a `for...of` loop?</strong></summary>

**Answer:**
When a `for...of` loop terminates abruptly (via `break`, `return`, or an uncaught exception), the JavaScript engine checks if the underlying iterator object defines a `.return()` method. If present, the engine automatically invokes `.return()`. This cleanup protocol allows iterators (such as generators or file streams) to execute `finally` blocks and release system resources.
</details>

<details>
<summary><strong>4. How does `Array.prototype.forEach()` differ fundamentally from a `for` loop?</strong></summary>

**Answer:**
1. `forEach` executes a callback function for each element; it cannot use `break` or `continue` to alter control flow.
2. A `return` statement inside `forEach` only exits that specific callback invocation, not the enclosing function.
3. `forEach` cannot pause execution when passed an `async` callback.
4. `forEach` skips empty holes in sparse arrays, whereas `for...of` yields `undefined`.
</details>

<details>
<summary><strong>5. What is a labeled statement and when is it necessary?</strong></summary>

**Answer:**
A labeled statement prefixes an expression with an identifier and a colon: `myLabel: for (...)`. It is strictly required when an algorithm contains nested loops and needs to break out of or continue an outer loop directly from within an inner loop. Without labels, `break` or `continue` only ever targets the innermost enclosing loop.
</details>

<details>
<summary><strong>6. Why is iterating over an array in reverse sometimes safer when mutating?</strong></summary>

**Answer:**
When removing elements from an array using `.splice(i, 1)` during forward iteration (`i++`), all subsequent elements shift left, occupying the current index `i`. When `i` increments on the next iteration, the element that shifted into index `i` is skipped entirely.

Iterating in reverse (`for (let i = arr.length - 1; i >= 0; i--)`) ensures that removing an element only affects indices greater than `i`, leaving the remaining unprocessed elements at lower indices untouched.
</details>

<details>
<summary><strong>7. How do you make a plain JavaScript object iterable with `for...of`?</strong></summary>

**Answer:**
Implement a generator method or iterator factory on the object under the key `[Symbol.iterator]`:
```javascript
const userList = {
  users: ["Alice", "Bob"],
  *[Symbol.iterator]() {
    for (const user of this.users) yield user;
  }
};
for (const u of userList) console.log(u); // Alice, Bob
```
</details>

<details>
<summary><strong>8. What is Bounds-Check Elimination in modern JavaScript engines?</strong></summary>

**Answer:**
Bounds-Check Elimination (BCE) is an optimizing compiler technique in V8/TurboFan. In JavaScript, reading `arr[i]` normally requires checking if `i` is within `[0, arr.length)` to prevent buffer overruns or returning out-of-bound `undefined`. If TurboFan proves through induction variables in a counting `for` loop that `i` is mathematically guaranteed to stay within bounds, it removes these checks from the compiled machine code, yielding native memory read speeds.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Reverse Array Traversal
**Task**: Write a function `printReverse(arr)` that prints the elements of an array from last to first using a classic `for` loop without mutating or copying the array.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function printReverse(arr) {
  for (let i = arr.length - 1; i >= 0; i--) {
    console.log(arr[i]);
  }
}

printReverse(["a", "b", "c"]); // Logs 'c', 'b', 'a'
```
</details>

### Exercise 2 (Easy): Safe Object Property Extractor
**Task**: Write a function `getOwnEnumerableProperties(obj)` that returns an array of `[key, value]` pairs using a `for...in` loop while strictly shielding against prototype pollution.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function getOwnEnumerableProperties(obj) {
  const result = [];
  for (const key in obj) {
    if (Object.hasOwn(obj, key)) {
      result.push([key, obj[key]]);
    }
  }
  return result;
}

const proto = { inherited: true };
const target = Object.create(proto);
target.own = 123;

console.log(getOwnEnumerableProperties(target)); // [ ['own', 123] ]
```
</details>

### Exercise 3 (Medium): Chunked In-Place Array Filter
**Task**: Implement an in-place array filter `filterInPlace(arr, predicate)` that removes elements not matching the predicate without allocating a new array, using reverse loop iteration.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function filterInPlace(arr, predicate) {
  for (let i = arr.length - 1; i >= 0; i--) {
    if (!predicate(arr[i])) {
      arr.splice(i, 1);
    }
  }
  return arr;
}

const numbers = [1, 2, 3, 4, 5, 6];
filterInPlace(numbers, (n) => n % 2 === 0);
console.log(numbers); // [2, 4, 6] (Mutated in-place!)
```
</details>

### Exercise 4 (Medium): Custom Fibonacci Iterable with Iteration Protocol
**Task**: Build an object `fibonacciSequence(limit)` that implements `[Symbol.iterator]` and yields Fibonacci numbers up to `limit` without using generator functions (`function*`).

<details>
<summary><strong>Solution</strong></summary>

```javascript
function fibonacciSequence(limit) {
  return {
    [Symbol.iterator]() {
      let prev = 0;
      let curr = 1;
      return {
        next() {
          if (prev <= limit) {
            const value = prev;
            const nextVal = prev + curr;
            prev = curr;
            curr = nextVal;
            return { done: false, value };
          }
          return { done: true, value: undefined };
        }
      };
    }
  };
}

for (const num of fibonacciSequence(20)) {
  console.log(`Fib: ${num}`); // 0, 1, 1, 2, 3, 5, 8, 13
}
```
</details>

### Exercise 5 (Hard): Multi-Dimensional Labeled Substring Search
**Task**: Given an array of text paragraphs, write a function `findFirstOccurrence(paragraphs, term)` using labeled loops to locate the paragraph index and character offset of the target term, stopping search immediately upon first discovery.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function findFirstOccurrence(paragraphs, term) {
  let location = null;

  paragraphSearch: for (let pIdx = 0; pIdx < paragraphs.length; pIdx++) {
    const text = paragraphs[pIdx];
    const matchIdx = text.indexOf(term);

    if (matchIdx !== -1) {
      location = { paragraphIndex: pIdx, characterIndex: matchIdx };
      break paragraphSearch;
    }
  }

  return location;
}

const corpus = [
  "Introduction to modern JavaScript execution environments.",
  "Deep dive into V8 engine internals and memory layout.",
  "Conclusion and best practices."
];

console.log(findFirstOccurrence(corpus, "V8 engine"));
// Output: { paragraphIndex: 1, characterIndex: 15 }
```
</details>

### Exercise 6 (Hard): Build an Async Rate-Limited Pool Processor
**Task**: Write an async function `processPool(items, concurrency, asyncWorker)` that processes an array of items with a fixed maximum number of concurrent workers using loops and `Promise.race`, returning results in completion order.

<details>
<summary><strong>Solution</strong></summary>

```javascript
async function processPool(items, concurrency, asyncWorker) {
  const executing = new Set();
  const results = [];

  for (const item of items) {
    const promise = asyncWorker(item).then((res) => {
      executing.delete(promise);
      return res;
    });

    executing.add(promise);
    results.push(promise);

    if (executing.size >= concurrency) {
      // Pause loop until at least one ongoing worker promise finishes
      await Promise.race(executing);
    }
  }

  return Promise.all(results);
}

// Verification
const tasks = [10, 20, 30, 40, 50];
const worker = (n) => new Promise((resolve) => setTimeout(() => resolve(n * 2), n));

processPool(tasks, 2, worker).then((res) => console.log("Pool Output:", res));
// Output: [20, 40, 60, 80, 100]
```
</details>
