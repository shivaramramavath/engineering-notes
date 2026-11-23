# JavaScript Array Methods: Mutating, Non-Mutating & Functional Pipelines

> Comprehensive exploration of JavaScript array methods, differentiating in-place mutations from pure immutable transformations (including ES2023 `toSorted`, `toReversed`, `toSpliced`, `with`), and building robust data processing pipelines.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [02_functions/06_higher-order-functions.md](../02_functions/06_higher-order-functions.md), [03_objects-and-arrays/06_arrays.md](./06_arrays.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES1 (1997) to ES2023 (Non-mutating tuple-like array methods) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Mutating vs Non-Mutating](#2-core-concepts-mutating-vs-non-mutating)
3. [How It Works: Method Execution & Callback Semantics](#3-how-it-works-method-execution--callback-semantics)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: JIT Inlining & Polymorphic Callbacks](#9-engine-internals-jit-inlining--polymorphic-callbacks)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Financial Transaction Reconciliation](#11-real-world-case-study-financial-transaction-reconciliation)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

JavaScript arrays provide an extensive suite of built-in higher-order methods for querying, transforming, and mutating sequences. Historically, methods like `sort()`, `reverse()`, and `splice()` mutated the array in place, leading to accidental state bugs in React, Redux, and functional architectures. 

With ECMAScript 2023 (ES14), JavaScript introduced pure, non-mutating counterparts: `toSorted()`, `toReversed()`, `toSpliced()`, and `with()`. Understanding the exact behavioral differences, prototype lookups, and algorithmic complexities of these methods is essential for writing predictable, high-performance code.

---

## 2. Core Concepts: Mutating vs Non-Mutating

```
                         Array Methods Classification
                                     │
         ┌───────────────────────────┴───────────────────────────┐
         ▼                                                       ▼
   Mutating (In-Place)                                    Non-Mutating (Pure)
   • push(), pop()                                        • concat(), slice()
   • shift(), unshift()                                   • map(), filter()
   • splice()                                             • flat(), flatMap()
   • sort()                                               • reduce(), reduceRight()
   • reverse()                                            • toSorted(), toReversed() (ES2023)
   • fill(), copyWithin()                                 • toSpliced(), with() (ES2023)
```

### Pure vs In-Place Transformations
- **Mutating methods** alter the original array's backing store and return either the mutated array itself (e.g., `sort`, `reverse`) or a metadata scalar (e.g., `push` returns new length, `pop` returns removed element).
- **Non-mutating methods** allocate a brand-new array instance, leaving the source array completely untouched.

---

## 3. How It Works: Method Execution & Callback Semantics

When higher-order methods like `map()`, `filter()`, or `reduce()` execute:
1. The method reads `this.length` at the start of execution.
2. The engine loops from index `0` to `length - 1`.
3. For each index, it verifies whether the property index exists (`index in this`). If the index represents an uninitialized hole, the callback is skipped entirely.
4. The callback receives `(element, index, array)` along with an optional `thisArg`.

```
Execution Flow of .map() over [10, <hole>, 30]
Step 1: Check Index 0 -> Exists? Yes (10) -> Invoke fn(10, 0, arr) -> Store Result
Step 2: Check Index 1 -> Exists? No (Hole) -> Skip callback! Result[1] = <hole>
Step 3: Check Index 2 -> Exists? Yes (30) -> Invoke fn(30, 2, arr) -> Store Result
```

---

## 4. Syntax & API Reference

### 1. Modern ES2023 Non-Mutating Methods
- `arr.toSorted(compareFn?)`: Returns a new sorted array.
- `arr.toReversed()`: Returns a new reversed array.
- `arr.toSpliced(start, deleteCount, ...items)`: Returns a new array with items inserted/removed.
- `arr.with(index, value)`: Returns a new array with the element at `index` replaced by `value`. Supports negative indices.

### 2. Modern Search Methods
- `arr.find(predicate)` / `arr.findLast(predicate)`: Returns first/last matching value or `undefined`.
- `arr.findIndex(predicate)` / `arr.findLastIndex(predicate)`: Returns first/last matching index or `-1`.
- `arr.includes(value, fromIndex?)`: Returns boolean; uses SameValueZero (`NaN === NaN` is true).

### 3. Transformation & Flattening
- `arr.flatMap(callback)`: Equivalent to `arr.map(callback).flat(1)`, but in a single pass without intermediate allocation.
- `arr.reduce(callback, initialValue?)`: Accumulates elements into a single accumulator. If `initialValue` is omitted, index 0 is used as the initial accumulator and iteration begins at index 1.

---

## 5. Practical Examples

### Example 1: Basic Modern ES2023 Immutability
```javascript
const original = [3, 1, 4, 1, 5, 9];

const sorted = original.toSorted((a, b) => a - b);
const reversed = original.toReversed();
const updated = original.with(0, 99);

console.log("Original untouched:", original); // [3, 1, 4, 1, 5, 9]
console.log("Sorted copy:", sorted);         // [1, 1, 3, 4, 5, 9]
console.log("Reversed copy:", reversed);     // [9, 5, 1, 4, 1, 3]
console.log("Updated copy:", updated);       // [99, 1, 4, 1, 5, 9]
```

### Example 2: Real-World Grouping & Aggregating with Reduce
```javascript
const orders = [
  { id: 1, customer: "Alice", total: 45.50, status: "completed" },
  { id: 2, customer: "Bob", total: 120.00, status: "pending" },
  { id: 3, customer: "Alice", total: 80.00, status: "completed" },
  { id: 4, customer: "Charlie", total: 15.00, status: "failed" },
];

const customerTotals = orders
  .filter(order => order.status === "completed")
  .reduce((acc, order) => {
    acc[order.customer] = (acc[order.customer] || 0) + order.total;
    return acc;
  }, {});

console.log(customerTotals); 
// Output: { Alice: 125.5 }
```

### Example 3: Edge Case with Missing `initialValue` in Reduce
```javascript
// Omission of initialValue on empty array throws TypeError
try {
  [].reduce((acc, curr) => acc + curr);
} catch (err) {
  console.error("Caught expected error:", err.message);
  // Reduce of empty array with no initial value
}

// Single element array with no initialValue returns the element without invoking callback:
let invoked = false;
const single = [42].reduce((acc, curr) => {
  invoked = true;
  return acc + curr;
});
console.log("Result:", single, "Invoked callback?", invoked); // 42, false
```

### Example 4: Failure Mode with `sort()` Default Comparator
```javascript
// Default comparator converts all items to STRINGS before sorting!
const scores = [100, 25, 5, 9];
scores.sort(); 
console.log(scores); 
// Output: [100, 25, 5, 9] (because "100" < "25" < "5" < "9")

// Correct numerical sort:
scores.sort((a, b) => a - b);
console.log(scores); // [5, 9, 25, 100]
```

### Example 5: Idiomatic Pipeline with `flatMap`
```javascript
// Expand sentence tokens and filter stop words in one clean pipeline
const sentences = ["Hello world", "Functional JavaScript pipelines", ""];
const stopWords = new Set(["in", "and", "the"]);

const tokens = sentences
  .flatMap(s => s.toLowerCase().split(/\s+/))
  .filter(word => word.length > 0 && !stopWords.has(word));

console.log(tokens); 
// Output: ['hello', 'world', 'functional', 'javascript', 'pipelines']
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`sort()` Modifies In-Place**: Calling `arr.sort()` mutates `arr`. If `arr` is stored in a shared state or store (e.g. Redux state), it violates immutability contracts. Always use `arr.toSorted()` or `[...arr].sort()`.
2. **`flatMap` Only Flattens One Level**: `flatMap(fn)` is strictly shallow (depth 1). If your mapping function produces arrays nested two or more levels deep, you must use `.map(fn).flat(Infinity)`.
3. **`splice()` Return Value**: `splice(start, count)` returns the *deleted* elements as an array, not the remaining elements.
4. **`every()` Returns `true` for Empty Arrays**: Calling `[].every(predicate)` returns `true` for *any* predicate (vacuous truth in boolean logic). Conversely, `[].some(predicate)` returns `false`.
5. **Callback Mutation Vulnerability**: Mutating the host array inside a `forEach` or `map` loop shifts the internal traversal index and leads to skipped elements.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Default `sort()` on numbers | `[10, 2, 5].sort()` | `[10, 2, 5].sort((a, b) => a - b)` | Lexicographical sort treats 10 as smaller than 2. |
| In-place reverse in state | `state.users.reverse()` | `state.users.toReversed()` | In-place mutation breaks memoization and UI reactivity. |
| Using `map` for side effects | `items.map(i => save(i))` | `items.forEach(i => save(i))` | `map` allocates a new array that gets immediately discarded. |
| Omitting initial accumulator | `arr.reduce((acc, x) => acc + x.price)` | `arr.reduce((acc, x) => acc + x.price, 0)` | First item object gets cast to `"[object Object]"` string. |
| Chaining `map().filter()` when flatMap fits | `arr.map(fn).filter(Boolean)` | `arr.flatMap(x => fn(x) ? [fn(x)] : [])` | Avoids intermediate array allocations. |
| Using `indexOf` to find `NaN` | `[NaN].indexOf(NaN)` -> `-1` | `[NaN].includes(NaN)` -> `true` | `indexOf` uses Strict Equality (`===`), where `NaN !== NaN`. |

---

## 8. Performance Characteristics

- **`push` / `pop`**: $O(1)$ amortized constant time.
- **`shift` / `unshift`**: $O(n)$ linear time.
- **`slice` / `concat` / `toSorted`**: $O(n)$ time due to allocating new backing buffers and copying elements.
- **`sort()` / `toSorted()`**: $O(n \log n)$ time complexity using V8's Timsort algorithm.
- **Method Chaining Allocation Overhead**: Writing `arr.filter().map().slice()` creates two intermediate arrays. For millions of items, combining operations into a single `for` loop or `reduce` pass prevents GC pressure.

---

## 9. Engine Internals: JIT Inlining & Polymorphic Callbacks

Modern engines optimize higher-order array methods via TurboFan compiler built-ins. 
When you pass a callback function to `map()` or `forEach()`:
1. **Monomorphic Callback Sites**: If the callback passed to `arr.map(fn)` has a single, consistent call signature and shape, TurboFan inlines the callback directly into the array traversal loop.
2. **Polymorphic / Megamorphic Callbacks**: If different closures with differing hidden classes are repeatedly passed, the JIT deoptimizes and performs dynamic IC (Inline Cache) lookups on every single iteration.
3. **Allocation Sinking**: If an array produced by `.map()` is immediately consumed and discarded within the same function scope, Escape Analysis in TurboFan may eliminate the heap allocation entirely.

---

## 10. Best Practices & Design Guidelines

1. **Adopt ES2023 Non-Mutating Methods**: Prefer `toSorted()`, `toReversed()`, and `toSpliced()` over `sort()`, `reverse()`, and `splice()` in application logic.
2. **Always Provide Initial Values to `reduce()`**: Omitting the initial accumulator is the number one cause of production runtime `TypeError` exceptions on empty arrays.
3. **Use `includes()` for Membership Checks**: Prefer `arr.includes(target)` over `arr.indexOf(target) !== -1` for readability and correct `NaN` handling.
4. **Differentiate `forEach` from `map`**: Use `forEach` strictly for side-effects (logging, network calls, event triggers) and `map` strictly for pure value transformations.

---

## 11. Real-World Case Study: Financial Transaction Reconciliation

A reconciliation engine matches internal ledger records against external bank settlement files, detecting discrepancies, duplicate records, and unmatched debits.

```javascript
class TransactionReconciler {
  static reconcile(internalLedger, bankStatements) {
    // 1. Build lookup map from bank statements for O(1) matching
    const bankMap = bankStatements.reduce((map, stmt) => {
      const key = `${stmt.referenceId}_${stmt.amount.toFixed(2)}`;
      map.set(key, stmt);
      return map;
    }, new Map());

    const matched = [];
    const unmatchedInternal = [];

    // 2. Process ledger entries
    for (const entry of internalLedger) {
      const key = `${entry.referenceId}_${entry.amount.toFixed(2)}`;
      if (bankMap.has(key)) {
        matched.push({
          internal: entry,
          bank: bankMap.get(key),
          status: "reconciled"
        });
        bankMap.delete(key); // Prevent duplicate reconciliation
      } else {
        unmatchedInternal.push(entry);
      }
    }

    const unmatchedBank = Array.from(bankMap.values());

    return {
      matched,
      unmatchedInternal,
      unmatchedBank,
      summary: {
        totalInternal: internalLedger.length,
        totalMatched: matched.length,
        discrepancies: unmatchedInternal.length + unmatchedBank.length
      }
    };
  }
}

// Test Run
const internal = [
  { referenceId: "TX-100", amount: 250.00 },
  { referenceId: "TX-101", amount: 12.50 },
  { referenceId: "TX-102", amount: 99.00 }
];

const bank = [
  { referenceId: "TX-100", amount: 250.00, settlementDate: "2026-03-31" },
  { referenceId: "TX-103", amount: 500.00, settlementDate: "2026-03-31" }
];

const result = TransactionReconciler.reconcile(internal, bank);
console.log("Matched count:", result.summary.totalMatched); // 1
console.log("Discrepancies:", result.summary.discrepancies); // 2
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the difference between <code>splice()</code> and <code>slice()</code>?</strong></summary>

`slice(start, end)` is a non-mutating method that extracts a shallow copy of a portion of an array into a new array. `splice(start, deleteCount, ...items)` is a mutating method that modifies the original array directly by removing, replacing, or adding elements, returning an array of deleted elements.
</details>

<details>
<summary><strong>2. How do ES2023 <code>toSorted()</code> and traditional <code>sort()</code> differ?</strong></summary>

`sort()` sorts the array in-place and returns the reference to the original array. `toSorted()` leaves the original array completely unchanged, allocates a new array with the sorted elements, and returns the new instance. Both accept the exact same comparator signature and utilize Timsort under the hood.
</details>

<details>
<summary><strong>3. What does <code>[].reduce((a, b) => a + b)</code> do, and why?</strong></summary>

It throws a `TypeError: Reduce of empty array with no initial value`. According to the ECMAScript specification, if no initial value is provided, `reduce` attempts to take the first element (index 0) as the accumulator. Since the array is empty, no initial value exists to seed the accumulator.
</details>

<details>
<summary><strong>4. Why does <code>['1', '7', '11'].map(parseInt)</code> return <code>[1, NaN, 3]</code>?</strong></summary>

`map` passes three arguments to its callback: `(element, index, array)`. `parseInt` accepts two arguments: `(string, radix)`. 
Therefore, `map(parseInt)` executes:
- `parseInt('1', 0)`: Radix 0 defaults to decimal -> 1.
- `parseInt('7', 1)`: Radix 1 is invalid -> NaN.
- `parseInt('11', 2)`: Radix 2 parses binary string `'11'` -> 3.
Fix: `['1', '7', '11'].map(num => parseInt(num, 10))` or `.map(Number)`.
</details>

<details>
<summary><strong>5. What is the difference between <code>some()</code> and <code>every()</code> when invoked on an empty array?</strong></summary>

`[].every(predicate)` returns `true` regardless of the predicate, because in formal logic, a universal quantification over an empty set is vacuously true. `[].some(predicate)` returns `false` because an existential quantification requires at least one element to satisfy the predicate.
</details>

<details>
<summary><strong>6. How does <code>Array.prototype.at()</code> handle negative indices?</strong></summary>

`arr.at(index)` converts the index using integer conversion. If `index < 0`, it computes `arr[arr.length + index]`. If the computed index is out of bounds, it returns `undefined`. Unlike bracket notation (`arr[-1]`), it does not perform string property key lookup.
</details>

<details>
<summary><strong>7. How does <code>flatMap()</code> compare to <code>map().flat()</code> in terms of memory efficiency?</strong></summary>

`arr.map(fn).flat()` creates an intermediate array containing the mapped arrays, which immediately becomes garbage before being flattened. `arr.flatMap(fn)` computes the mapped elements and inserts them directly into the target output array in a single pass, avoiding the allocation and GC overhead of the intermediate container.
</details>

<details>
<summary><strong>8. What is the difference between <code>indexOf()</code> and <code>includes()</code> regarding <code>NaN</code>?</strong></summary>

`indexOf()` uses the Strict Equality Comparison (`===`). In JavaScript, `NaN === NaN` is `false`, so `[NaN].indexOf(NaN)` returns `-1`. `includes()` uses the `SameValueZero` comparison algorithm, which treats `NaN` as equal to `NaN`, so `[NaN].includes(NaN)` returns `true`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Custom Array Map Implementation (Easy)
Implement `myMap(array, callback)` from scratch without using native `Array.prototype.map`. Correctly handle empty slots (holes).

<details>
<summary>View Solution</summary>

```javascript
function myMap(array, callback, thisArg) {
  if (!Array.isArray(array)) throw new TypeError("Expected an array");
  const result = new Array(array.length);
  for (let i = 0; i < array.length; i++) {
    if (i in array) {
      result[i] = callback.call(thisArg, array[i], i, array);
    }
  }
  return result;
}

const res = myMap([1, , 3], x => x * 2);
console.log(res); // [2, <1 empty item>, 6]
```
</details>

### Exercise 2: Object Key Inversion via Reduce (Easy)
Write a function `invertObject(obj)` that swaps keys and values using `Object.entries()` and `Array.prototype.reduce()`.

<details>
<summary>View Solution</summary>

```javascript
function invertObject(obj) {
  return Object.entries(obj).reduce((acc, [key, val]) => {
    acc[String(val)] = key;
    return acc;
  }, {});
}

console.log(invertObject({ a: 1, b: 2, c: 3 }));
// Output: { '1': 'a', '2': 'b', '3': 'c' }
```
</details>

### Exercise 3: Recursive Deep Flatten (Medium)
Implement `deepFlat(array, depth = 1)` supporting arbitrary depth without using native `Array.prototype.flat()`.

<details>
<summary>View Solution</summary>

```javascript
function deepFlat(array, depth = 1) {
  if (depth <= 0) return array.slice();
  return array.reduce((acc, item) => {
    if (Array.isArray(item)) {
      acc.push(...deepFlat(item, depth - 1));
    } else {
      acc.push(item);
    }
    return acc;
  }, []);
}

console.log(deepFlat([1, [2, [3, [4]]]], 2)); // [1, 2, 3, [4]]
```
</details>

### Exercise 4: Efficient Array Intersection (Medium)
Implement `intersection(arrA, arrB)` that returns common unique elements in $O(n + m)$ time.

<details>
<summary>View Solution</summary>

```javascript
function intersection(arrA, arrB) {
  const setB = new Set(arrB);
  const matched = new Set();
  for (const item of arrA) {
    if (setB.has(item)) {
      matched.add(item);
    }
  }
  return Array.from(matched);
}

console.log(intersection([1, 2, 2, 1], [2, 2])); // [2]
```
</details>

### Exercise 5: Asynchronous Sequential Pipeline (Hard)
Write a utility `asyncPipe(fns)` that takes an array of asynchronous functions and returns a composite function executing them sequentially like `Array.prototype.reduce()`.

<details>
<summary>View Solution</summary>

```javascript
function asyncPipe(...fns) {
  return initialVal => fns.reduce(
    (promise, fn) => promise.then(fn),
    Promise.resolve(initialVal)
  );
}

const add1 = async x => x + 1;
const double = async x => x * 2;
const square = async x => x * x;

const pipeline = asyncPipe(add1, double, square);
pipeline(2).then(res => console.log("Pipeline result:", res)); // ((2 + 1) * 2)^2 = 36
```
</details>

### Exercise 6: In-Place Non-Mutating Array Proxy (Hard)
Implement an `ImmutableArray` class that wraps an array and transparently converts mutating method calls (`push`, `pop`, `sort`, `splice`) into non-mutating clones.

<details>
<summary>View Solution</summary>

```javascript
class ImmutableArray {
  static create(initialArray = []) {
    return new Proxy([...initialArray], {
      get(target, prop, receiver) {
        if (prop === "push") {
          return (...items) => ImmutableArray.create([...target, ...items]);
        }
        if (prop === "pop") {
          return () => ImmutableArray.create(target.slice(0, -1));
        }
        if (prop === "sort") {
          return (compareFn) => ImmutableArray.create([...target].sort(compareFn));
        }
        const val = Reflect.get(target, prop, receiver);
        return typeof val === "function" ? val.bind(target) : val;
      }
    });
  }
}

const imm = ImmutableArray.create([3, 1, 2]);
const pushed = imm.push(4);
const sorted = imm.sort((a, b) => a - b);

console.log("Original:", imm);   // [3, 1, 2]
console.log("Pushed:", pushed); // [3, 1, 2, 4]
console.log("Sorted:", sorted); // [1, 2, 3]
```
</details>
