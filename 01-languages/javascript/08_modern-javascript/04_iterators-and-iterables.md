# JavaScript Iterators & Iterables: Protocols, Consumption & Engine Execution

> Deep-dive technical guide into the ECMAScript Iteration Protocols, `Symbol.iterator`, the Iterator contract (`next`, `return`, `throw`), consumption mechanisms, and V8 iterator optimization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | [08_modern-javascript/03_symbols.md](03_symbols.md), [03_objects-and-arrays/01_object-basics.md](../03_objects-and-arrays/01_object-basics.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Iteration Protocols) / ES2024 (Iterator Helpers Stage 3 / Node 22+) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: The Iterable & Iterator Protocols](#2-core-concepts-the-iterable--iterator-protocols)
3. [How It Works: Engine Iteration Mechanics & State Flow](#3-how-it-works-engine-iteration-mechanics--state-flow)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Ignition Bytecode & Iterator Inlining](#9-engine-internals-v8-ignition-bytecode--iterator-inlining)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Chunked Paginated API Stream Consumer](#11-real-world-case-study-chunked-paginated-api-stream-consumer)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6), traversing collections in JavaScript suffered from fragmentation, inconsistent APIs, and lack of a unified sequence abstraction:
1. **Arrays** used indexed `for` loops or `Array.prototype.forEach()`.
2. **Objects** used `for...in` loops (which traverse prototype chains and require `hasOwnProperty` checks).
3. **DOM collections** (`NodeList`, `HTMLCollection`) and `arguments` were array-like objects lacking standard array methods.
4. **Strings** had no clean character-level traversal mechanism that respected multi-byte UTF-16 surrogate pairs.

```javascript
// Pre-ES6 fragmented iteration:
for (var i = 0; i < arr.length; i++) { /* array */ }
for (var key in obj) { if (obj.hasOwnProperty(key)) { /* object */ } }
Array.prototype.forEach.call(domNodes, function(node) { /* DOM */ });
```

**ECMAScript 2015 introduced the Iteration Protocols**: a formal, language-level contract that unifies how data structures expose sequences and how language constructs consume them. Any object can become iterable simply by implementing the protocol, unlocking native integration with `for...of`, spread syntax (`...`), destructuring, and data structures like `Map` and `Set`.

---

## 2. Core Concepts: The Iterable & Iterator Protocols

The iteration system is split into two distinct interfaces: **Iterable** and **Iterator**.

### 2.1 The Iterable Protocol
An object is an **Iterable** if it defines a zero-argument method keyed by `[Symbol.iterator]` that returns an **Iterator**:
```typescript
interface Iterable<T> {
  [Symbol.iterator](): Iterator<T>;
}
```
Built-in Iterables include: `Array`, `String`, `Map`, `Set`, `TypedArray`, `Intl.Segments`, and `arguments`. Plain objects (`{}`) are **not** iterable by default.

### 2.2 The Iterator Protocol
An object is an **Iterator** if it implements a zero-argument `next()` method returning an **IteratorResult** object:
```typescript
interface Iterator<T> {
  next(): IteratorResult<T>;
  return?(value?: any): IteratorResult<T>; // Optional: invoked on early termination
  throw?(error?: any): IteratorResult<T>;  // Optional: invoked by generators
}

interface IteratorResult<T> {
  value: T | undefined;
  done: boolean;
}
```

```
+------------------+         [Symbol.iterator]()        +--------------------+
|  Iterable Object | ---------------------------------> |   Iterator Object  |
+------------------+                                    +--------------------+
                                                                  |
                                                                  | next()
                                                                  v
                                                        +--------------------+
                                                        |   IteratorResult   |
                                                        |  { value, done }   |
                                                        +--------------------+
```

### 2.3 The Iteration Invariants
- `done: false`: The iterator produced a valid intermediate value.
- `done: true`: The iterator has reached the end of the sequence. If `value` is present, it is the completion return value (which `for...of` ignores, but generators consume).
- Once an iterator yields `{ done: true }`, all subsequent calls to `next()` must continue returning `{ done: true }`.

---

## 3. How It Works: Engine Iteration Mechanics & State Flow

When a consumer like `for (const x of iterable)` executes, the engine carries out the following operational steps defined in ECMA-262:

```
[Start for...of]
       |
       v
GetMethod(iterable, @@iterator)
       |
       v
Call [Symbol.iterator]()  ---> Yields Iterator
       |
+----->|
|      v
| IteratorNext(iterator)  ---> Calls iterator.next()
|      |
|      v
| IteratorComplete(result)
|      |
|      +-- If done === true  ---> Exit Loop Normally
|      |
|      +-- If done === false
|              |
|              v
|          IteratorValue(result)
|              |
|              v
|          Assign to loop variable (e.g., const x)
|              |
|              v
|          Execute loop body
|              |
|              +-- Normal iteration: Loop back to (IteratorNext)
|              +-- break / return / throw: Call iterator.return() -> Exit Loop
+--------------+
```

### Early Termination via `return()`
If a loop exits before completing (via `break`, `return`, or an uncaught exception), the engine checks for the optional `.return()` method on the iterator. This allows the iterator to close file handles, release database connections, or cancel underlying timers.

---

## 4. Syntax & API Reference

### 4.1 Native Iteration Consumers
The following ECMAScript language constructs consume iterables automatically:

| Consumer | Description | Example |
| :--- | :--- | :--- |
| `for...of` | Iterates over values in sequence | `for (const x of iterable) {}` |
| Spread operator `...` | Flattens elements into array or arguments | `const arr = [...iterable];` |
| Destructuring | Extracts elements sequentially | `const [first, second] = iterable;` |
| `Array.from()` | Converts iterable or array-like to Array | `const arr = Array.from(iterable);` |
| Collection Constructors | Populates `Map`, `Set`, `WeakMap`, etc. | `new Set(iterable);` |
| `Promise.all()`, `race()` | Accepts any iterable of Promises | `Promise.all(iterableOfPromises)` |
| `yield*` | Delegates iteration to another iterable | `function* gen() { yield* iterable; }` |

### 4.2 Iterator Helpers (ES2024 / Node 22+)
Standardized in ES2024, `Iterator.prototype` provides functional chainable helpers similar to Array methods, but lazily evaluated without allocating intermediate arrays:
```javascript
// Native in Node v22+ / modern browsers:
const iterator = [1, 2, 3, 4, 5].values();

const evensSquared = iterator
  .filter(x => x % 2 === 0)
  .map(x => x ** 2)
  .take(2);

console.log([...evensSquared]); // [4, 16]
```

---

## 5. Practical Examples

### Example 1: Basic Number Range Iterable
```javascript
class Range {
  constructor(start, end, step = 1) {
    this.start = start;
    this.end = end;
    this.step = step;
  }

  [Symbol.iterator]() {
    let current = this.start;
    const end = this.end;
    const step = this.step;

    return {
      next() {
        if (current <= end) {
          const val = current;
          current += step;
          return { value: val, done: false };
        }
        return { value: undefined, done: true };
      }
    };
  }
}

for (const n of new Range(0, 10, 2)) {
  process.stdout.write(n + " "); // 0 2 4 6 8 10 
}
console.log();
```

### Example 2: Reusable vs Single-Use Iterable
A clean pattern is to make an iterator itself an iterable by having its `[Symbol.iterator]` return `this`:
```javascript
function makeCounter(limit) {
  let count = 0;
  return {
    next() {
      if (count < limit) {
        return { value: count++, done: false };
      }
      return { value: undefined, done: true };
    },
    [Symbol.iterator]() {
      return this; // Allows passing iterator directly to for...of
    }
  };
}

const counter = makeCounter(3);
console.log([...counter]); // [0, 1, 2]
console.log([...counter]); // [] (Exhausted! Single-use)
```

### Example 3: Linked List with Custom Iterator
```javascript
class Node {
  constructor(value, next = null) {
    this.value = value;
    this.next = next;
  }
}

class LinkedList {
  constructor() {
    this.head = null;
  }

  append(value) {
    const node = new Node(value);
    if (!this.head) {
      this.head = node;
      return;
    }
    let curr = this.head;
    while (curr.next) curr = curr.next;
    curr.next = node;
  }

  [Symbol.iterator]() {
    let current = this.head;
    return {
      next() {
        if (current) {
          const val = current.value;
          current = current.next;
          return { value: val, done: false };
        }
        return { value: undefined, done: true };
      }
    };
  }
}

const list = new LinkedList();
list.append("Node A");
list.append("Node B");
list.append("Node C");

for (const node of list) {
  console.log(node); // Node A, Node B, Node C
}
```

### Example 4: Iterator Resource Management via `return()`
```javascript
function openFileStream(filename) {
  let index = 0;
  const lines = ["line 1", "line 2", "line 3", "line 4"];
  let isClosed = false;

  return {
    next() {
      if (isClosed || index >= lines.length) {
        return { value: undefined, done: true };
      }
      return { value: lines[index++], done: false };
    },
    return() {
      isClosed = true;
      console.log(`[FileStream] Cleanup: Handle for '${filename}' closed.`);
      return { value: undefined, done: true };
    },
    [Symbol.iterator]() {
      return this;
    }
  };
}

// Early break triggers .return()
for (const line of openFileStream("data.log")) {
  console.log("Read:", line);
  if (line === "line 2") break; // Exits early!
}
// Output:
// Read: line 1
// Read: line 2
// [FileStream] Cleanup: Handle for 'data.log' closed.
```

### Example 5: UTF-16 Surrogate Pair Aware String Traverser
```javascript
// Plain string indexing splits emoji surrogate pairs:
const text = "JS🚀Code";

// Native string iteration respects Unicode code points:
const chars = [];
for (const char of text) {
  chars.push(char);
}

console.log(chars); // ['J', 'S', '🚀', 'C', 'o', 'd', 'e']
console.log(chars.length); // 7 (Correct code point count)
console.log(text.length);  // 8 (Raw UTF-16 code units, splitting the emoji)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Plain Objects Are Not Iterable
Executing `for...of` on a standard plain object throws a `TypeError: obj is not iterable`.
```javascript
const user = { name: "Alan", role: "Admin" };

// Throws TypeError:
// for (const val of user) {}

// Correct options:
for (const key of Object.keys(user)) {}
for (const val of Object.values(user)) {}
for (const [key, val] of Object.entries(user)) {}
```

### 6.2 Infinite Iterables and Greedy Consumers
An iterator can represent an infinite sequence. However, consuming an infinite iterable with greedy operations like spread (`[...infinite]`), `Array.from()`, or `destructuring with rest` will exhaust the heap and crash the process with `RangeError: Out of memory`.
```javascript
function infiniteNumbers() {
  let n = 0;
  return {
    next: () => ({ value: n++, done: false }),
    [Symbol.iterator]() { return this; }
  };
}

// DANGEROUS: Heap exhaustion
// const all = [...infiniteNumbers()];

// SAFE: Controlled loop with break
for (const n of infiniteNumbers()) {
  if (n > 5) break;
  console.log(n);
}
```

### 6.3 Exhausted Single-Use Iterators
When an iterator yields `{ done: true }`, it cannot be reset unless the generator or factory function is invoked again to create a new instance:
```javascript
const set = new Set([1, 2, 3]);
const iter = set.values();

console.log([...iter]); // [1, 2, 3]
console.log([...iter]); // [] (Empty! Iteration completed)
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Iterating Objects with `for...of`** | `for (const x of plainObj)` | `for (const [k, v] of Object.entries(plainObj))` | Plain objects lack `[Symbol.iterator]`; throws `TypeError`. |
| **Omitting `[Symbol.iterator]` on Iterators** | Returning `{ next() {} }` without `[Symbol.iterator]` hook | Add `[Symbol.iterator]() { return this; }` | Allows iterators to be passed directly to `for...of` and spread. |
| **Spreading Infinite Iterables** | `const items = [...infiniteStream]` | Use `for...of` with a terminating `break` condition | Spreading an infinite sequence causes out-of-memory crash. |
| **Ignoring `return()` in Custom Iterators** | Omitting cleanup when managing sockets or handles | Implement `return()` method on iterator | Sockets/handles remain leaked if caller breaks early. |
| **Mutating Collection During Iteration** | `array.forEach(...)` mutating length in-place | Use immutable methods or iterate backwards | Unpredictable index skips and off-by-one errors. |
| **Returning non-boolean `done`** | `return { value: x, done: 0 }` | Always return boolean `done: false` or `done: true` | Violates specification contract; results in unexpected truthy checks. |

---

## 8. Performance Characteristics

1. **Object Allocation Overhead:**
   - Every invocation of `iterator.next()` allocates an object `{ value: T, done: boolean }`.
   - In tight loops of millions of iterations, allocating millions of result objects increases Garbage Collector (GC) pressure compared to a traditional primitive `for (let i = 0; i < len; i++)` index loop.
2. **TurboFan Escape Analysis & Inlining:**
   - In modern V8, if an iterator's `next()` method is small and predictable (monomorphic call site), TurboFan's Escape Analysis can eliminate the allocation of the `{ value, done }` object, keeping the loop on the stack with performance matching a raw indexed `for` loop.
3. **Lazy Evaluation:**
   - Iterators compute elements on demand. Instead of creating a 10-million element array in memory, an iterator computes one element per step at $O(1)$ memory overhead.

---

## 9. Engine Internals: V8 Ignition Bytecode & Iterator Inlining

When compiling a `for...of` loop, V8's Ignition compiler produces specialized bytecode instructions:

```
[Ignition Bytecode for for...of loop]:
GetIterator          a0, [0]          ; Checks Symbol.iterator and extracts iterator
LdaNamedProperty     a1, [1], [2]     ; Loads .next method
Star                 r1               ; Stores .next function
...
CallProperty0        r1, a1, [4]      ; Calls iterator.next()
Star                 r2               ; Stores IteratorResult object
GetNamedProperty     r2, [3], [6]     ; Reads 'done' property
BranchIfToBooleanTrue [Target]        ; If done is true, exit loop
GetNamedProperty     r2, [4], [8]     ; Reads 'value' property
Star                 r3               ; Assigns value to loop variable
```

### TurboFan Optimization:
When TurboFan optimizes this bytecode:
1. It verifies that `Symbol.iterator` has not been overridden on the target prototype.
2. It inlines the `next()` call directly into the loop body.
3. If the `{ value, done }` object does not escape the loop, V8 performs **Scalar Replacement of Aggregates (SRA)**, storing `value` and `done` directly in CPU registers rather than allocating heap objects.

---

## 10. Best Practices & Design Guidelines

1. **Implement Both Protocols on Custom Iterators:** Whenever authoring an iterator object, always include `[Symbol.iterator]() { return this; }`. This makes it both an iterator and an iterable.
2. **Always Implement `return()` for Resources:** If your iterator allocates OS resources, subscriptions, or file descriptors, provide a `.return()` cleanup hook.
3. **Favor Iterables for Large or Infinite Data:** Use iterables to stream large files, database result sets, or dynamic sequences without loading everything into memory.
4. **Use `Iterator.prototype` Helpers (ES2024):** In modern runtimes, prefer `.map()` and `.filter()` directly on iterators to avoid creating intermediate Array allocations.

---

## 11. Real-World Case Study: Chunked Paginated API Stream Consumer

### Problem
A microservice needs to fetch 50,000 transaction records from an upstream REST API. The API only allows fetching records in pages of 100 items. Fetching all 50,000 records into an array up front requires 500 HTTP requests, takes 15 seconds, and spikes memory by 250 MB.

### Solution
Wrap the paginated API in a lazy chunked Iterable. Consumers process records individually or in batches as pages arrive, keeping memory flat:

```javascript
class PaginatedRecordStream {
  constructor(fetchPageFn, pageSize = 100) {
    this.fetchPageFn = fetchPageFn;
    this.pageSize = pageSize;
  }

  [Symbol.iterator]() {
    let currentPage = 1;
    let buffer = [];
    let bufferIndex = 0;
    let isComplete = false;
    const fetchPage = this.fetchPageFn;
    const pageSize = this.pageSize;

    return {
      next() {
        // If buffer exhausted and not complete, fetch synchronously or from cache
        if (bufferIndex >= buffer.length && !isComplete) {
          const response = fetchPage(currentPage, pageSize);
          buffer = response.records;
          bufferIndex = 0;
          currentPage++;

          if (response.records.length < pageSize || response.isLastPage) {
            isComplete = true;
          }
        }

        if (bufferIndex < buffer.length) {
          return { value: buffer[bufferIndex++], done: false };
        }

        return { value: undefined, done: true };
      },
      return() {
        isComplete = true;
        buffer = [];
        console.log("[Stream] Paginated iteration aborted early. Memory released.");
        return { value: undefined, done: true };
      },
      [Symbol.iterator]() {
        return this;
      }
    };
  }
}

// Mocking synchronized page fetcher:
function mockApiFetcher(page, limit) {
  console.log(`[API Call] Fetching page ${page} (limit: ${limit})...`);
  const records = [];
  for (let i = 0; i < limit; i++) {
    records.push({ id: (page - 1) * limit + i + 1, amount: 100.50 });
  }
  return { records, isLastPage: page >= 3 };
}

// Consuming lazily:
const stream = new PaginatedRecordStream(mockApiFetcher, 3);
let totalProcessed = 0;

for (const record of stream) {
  console.log(`Processed: Transaction #${record.id}`);
  totalProcessed++;
  if (totalProcessed === 5) {
    console.log("Reached processing threshold. Stopping early.");
    break; // Triggers .return()
  }
}
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the exact difference between an Iterable and an Iterator?</strong></summary>

An **Iterable** is any data structure that implements the method `[Symbol.iterator]()`, which returns an Iterator. An **Iterator** is an object with a `next()` method that returns `{ value: any, done: boolean }`. In short, an Iterable is a factory for Iterators.
</details>

<details>
<summary><strong>Q2: Why are plain JavaScript objects <code>{}</code> not iterable by default?</strong></summary>

Plain objects are used for two distinct purposes: data records (keyed structures with heterogeneous fields) and hash maps. If `{}` were iterable, TC39 would have had to choose between iterating over keys, values, or entries. Furthermore, iterating over prototype properties could introduce severe security and correctness bugs. TC39 left plain objects non-iterable and introduced explicit helpers (`Object.keys()`, `Object.values()`, `Object.entries()`) and `Map`.
</details>

<details>
<summary><strong>Q3: What happens behind the scenes when a <code>for...of</code> loop encounters a <code>break</code> or throws an error?</strong></summary>

The engine invokes the `IteratorClose` abstract operation. If the iterator object has a `.return()` method, the engine invokes it. If an exception was thrown inside the loop body, the exception is propagated after `.return()` executes. If `.return()` throws its own error, the engine prioritizes the original loop error.
</details>

<details>
<summary><strong>Q4: How do iterators handle Unicode surrogate pairs in JavaScript strings?</strong></summary>

Standard index access (`str[i]`) and `str.charCodeAt(i)` operate on 16-bit UTF-16 code units, splitting 4-byte astral characters (like emojis or mathematical symbols). In contrast, string iteration via `[Symbol.iterator]` implements the Unicode code point abstract operation, correctly returning whole astral characters as single string elements.
</details>

<details>
<summary><strong>Q5: Can an iterator return a <code>value</code> when <code>done: true</code>? How do consumers handle it?</strong></summary>

Yes, an iterator may return `{ value: "final", done: true }`. However, language constructs like `for...of` and array spread ignore the `value` property when `done === true`. Only generator delegation via `yield*` captures and returns this completion value (`const result = yield* subGen()`).
</details>

<details>
<summary><strong>Q6: How does the new ES2024 Iterator Helpers specification differ from standard Array methods?</strong></summary>

Array methods (`map()`, `filter()`, `slice()`) are eager: they immediately iterate through the entire collection and allocate a new intermediate array in heap memory at every step in a chain. Iterator helpers (`Iterator.prototype.map()`, `.filter()`, `.take()`) are **lazy**: they return a new iterator and do not process items until `.next()` is called, allocating zero intermediate arrays.
</details>

<details>
<summary><strong>Q7: What is the time and space complexity of spreading an iterator vs iterating with <code>for...of</code>?</strong></summary>

Both take $O(n)$ time. However, spreading an iterator (`[...iter]`) has $O(n)$ space complexity because it buffers every yielded element in a new array. A `for...of` loop can achieve $O(1)$ auxiliary space complexity by processing and discarding each value sequentially.
</details>

<details>
<summary><strong>Q8: How does V8 optimize monomorphic iterator calls in high-throughput loops?</strong></summary>

V8 uses monomorphic Inline Caches (ICs) to track the hidden class (Map) of the iterator and its `.next()` method. If the call site remains monomorphic, TurboFan inlines `.next()` directly. Through Escape Analysis and Scalar Replacement of Aggregates (SRA), V8 can completely eliminate the allocation of the intermediate `{ value, done }` wrapper object.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement a Zip Iterator
Write a function `zip(iterableA, iterableB)` that returns an iterable yielding pairs of elements `[a, b]` until the shorter iterable completes.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function zip(iterA, iterB) {
  return {
    [Symbol.iterator]() {
      const itA = iterA[Symbol.iterator]();
      const itB = iterB[Symbol.iterator]();

      return {
        next() {
          const resA = itA.next();
          const resB = itB.next();

          if (resA.done || resB.done) {
            // Trigger cleanup if one was longer
            if (!resA.done && typeof itA.return === "function") itA.return();
            if (!resB.done && typeof itB.return === "function") itB.return();
            return { value: undefined, done: true };
          }

          return { value: [resA.value, resB.value], done: false };
        },
        [Symbol.iterator]() { return this; }
      };
    }
  };
}

const names = ["Alice", "Bob", "Charlie"];
const scores = [98, 85, 92, 100];

console.log([...zip(names, scores)]);
// Output: [ ['Alice', 98], ['Bob', 85], ['Charlie', 92] ]
```
</details>

### Exercise 2: Circular Ring Buffer Iterable
Implement a `RingBuffer` class with a fixed capacity that overwrites the oldest element when full and is fully iterable from oldest to newest element.

<details>
<summary><strong>Solution</strong></summary>

```javascript
class RingBuffer {
  constructor(capacity) {
    this.capacity = capacity;
    this.buffer = new Array(capacity);
    this.size = 0;
    this.head = 0; // write pointer
  }

  push(item) {
    this.buffer[this.head] = item;
    this.head = (this.head + 1) % this.capacity;
    if (this.size < this.capacity) this.size++;
  }

  *[Symbol.iterator]() {
    const start = this.size < this.capacity ? 0 : this.head;
    for (let i = 0; i < this.size; i++) {
      const idx = (start + i) % this.capacity;
      yield this.buffer[idx];
    }
  }
}

const rb = new RingBuffer(3);
rb.push("A");
rb.push("B");
rb.push("C");
rb.push("D"); // Overwrites "A"

console.log([...rb]); // ['B', 'C', 'D']
```
</details>

### Exercise 3: Take and Filter Iterator Decorator
Implement a lazy pipeline helper `take(iterable, count)` that yields at most `count` elements and ensures the underlying iterator's `.return()` method is called when `count` is reached.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function take(iterable, count) {
  return {
    [Symbol.iterator]() {
      const iterator = iterable[Symbol.iterator]();
      let remaining = count;

      return {
        next() {
          if (remaining <= 0) {
            if (typeof iterator.return === "function") {
              iterator.return();
            }
            return { value: undefined, done: true };
          }

          const result = iterator.next();
          if (result.done) {
            return result;
          }

          remaining--;
          return result;
        },
        return() {
          if (typeof iterator.return === "function") {
            return iterator.return();
          }
          return { value: undefined, done: true };
        },
        [Symbol.iterator]() { return this; }
      };
    }
  };
}

const nums = [10, 20, 30, 40, 50];
console.log([...take(nums, 3)]); // [10, 20, 30]
```
</details>
