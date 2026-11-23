# JavaScript Interview Mastery: Senior Machine Coding Practical Challenges

> Comprehensive end-to-end solutions, clean architectures, test suites, and edge-case handling for the 4 most commonly asked Senior JavaScript Machine Coding Rounds.

| Metadata | Details |
| :--- | :--- |
| **Track** | JavaScript Interview Preparation (`26_interview`) |
| **Target Role** | Senior / Staff Frontend & Full-Stack Engineer |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Time Allotment** | 45–60 Minutes per Challenge |

---

## Table of Contents
1. [Framework for Excelling in Machine Coding Rounds](#1-framework-for-excelling-in-machine-coding-rounds)
2. [Challenge 1: $O(1)$ Least Recently Used (LRU) Cache](#challenge-1-o1-least-recently-used-lru-cache)
3. [Challenge 2: Async Auto-Retry with Exponential Backoff & Full Jitter](#challenge-2-async-auto-retry-with-exponential-backoff--full-jitter)
4. [Challenge 3: Bounded Concurrency Promise Pool](#challenge-3-bounded-concurrency-promise-pool)
5. [Challenge 4: Nested Object Flatten & Unflatten Pipeline](#challenge-4-nested-object-flatten--unflatten-pipeline)
6. [Evaluation Rubric for Senior Candidates](#6-evaluation-rubric-for-senior-candidates)

---

## 1. Framework for Excelling in Machine Coding Rounds

Interviewers assess how candidates design, structure, and verify code under timed pressure:
1. **Clarify Requirements (5 mins)**: Concurrency limits, abort signals, edge-case types (`null`, `undefined`, arrays).
2. **Define API Interface (5 mins)**: Method signatures, return types, error contracts.
3. **Write Clean, Modular Implementation (25 mins)**: Separation of concerns, descriptive identifiers.
4. **Author Test Cases (10 mins)**: Verify happy paths, boundary constraints, and race conditions.

---

## 2. Challenge 1: $O(1)$ Least Recently Used (LRU) Cache

### Problem Statement
Design and implement a data structure that follows the constraints of a Least Recently Used (LRU) cache with fixed `capacity`:
- `get(key)`: Return value if present; otherwise return `-1`. Marks key as most recently used.
- `put(key, value)`: Update or insert key. If capacity exceeded, evict the least recently used key.
- Both operations must run in $O(1)$ average time complexity.

### Production Solution (Doubly Linked List + Map)

```javascript
class DNode {
  constructor(key, value) {
    this.key = key;
    this.value = value;
    this.prev = null;
    this.next = null;
  }
}

export class LRUCache {
  constructor(capacity) {
    if (capacity <= 0) throw new Error('Capacity must be greater than zero');
    this.capacity = capacity;
    this.map = new Map();

    // Sentinel dummy head and tail nodes
    this.head = new DNode(0, 0);
    this.tail = new DNode(0, 0);
    this.head.next = this.tail;
    this.tail.prev = this.head;
  }

  get(key) {
    if (!this.map.has(key)) return -1;
    const node = this.map.get(key);
    this._moveToHead(node);
    return node.value;
  }

  put(key, value) {
    if (this.map.has(key)) {
      const node = this.map.get(key);
      node.value = value;
      this._moveToHead(node);
      return;
    }

    if (this.map.size >= this.capacity) {
      const lru = this.tail.prev;
      this._removeNode(lru);
      this.map.delete(lru.key);
    }

    const newNode = new DNode(key, value);
    this.map.set(key, newNode);
    this._addNode(newNode);
  }

  _addNode(node) {
    node.prev = this.head;
    node.next = this.head.next;
    this.head.next.prev = node;
    this.head.next = node;
  }

  _removeNode(node) {
    node.prev.next = node.next;
    node.next.prev = node.prev;
  }

  _moveToHead(node) {
    this._removeNode(node);
    this._addNode(node);
  }
}
```

---

## 3. Challenge 2: Async Auto-Retry with Exponential Backoff & Full Jitter

### Problem Statement
Implement an asynchronous retry wrapper `retryWithBackoff(fn, options)` that:
- Executes async task `fn()`.
- If `fn` throws or rejects, retries up to `maxRetries` times.
- Computes delay using truncated exponential backoff with full jitter:
  $$\text{delay} = \text{random}(0, \min(\text{maxDelay}, \text{baseDelay} \times 2^{\text{attempt}}))$$
- Supports cancellation via `AbortSignal`.

### Production Solution

```javascript
export async function retryWithBackoff(fn, options = {}) {
  const {
    maxRetries = 3,
    baseDelay = 100,
    maxDelay = 2000,
    signal = null,
    shouldRetry = () => true
  } = options;

  let attempt = 0;

  while (true) {
    if (signal?.aborted) {
      throw new Error('Operation aborted prior to execution');
    }

    try {
      return await fn({ attempt });
    } catch (err) {
      if (attempt >= maxRetries || !shouldRetry(err)) {
        throw err;
      }

      // Calculate exponential backoff with full jitter
      const exp = Math.min(maxDelay, baseDelay * Math.pow(2, attempt));
      const jitterDelay = Math.floor(Math.random() * exp);

      // Wait with AbortSignal cancellation support
      await new Promise((resolve, reject) => {
        const timer = setTimeout(resolve, jitterDelay);

        if (signal) {
          signal.addEventListener('abort', () => {
            clearTimeout(timer);
            const abortErr = new Error('Operation aborted during retry wait');
            abortErr.name = 'AbortError';
            reject(abortErr);
          }, { once: true });
        }
      });

      attempt++;
    }
  }
}
```

---

## 4. Challenge 3: Bounded Concurrency Promise Pool

### Problem Statement
Write a utility `promisePool(tasks, concurrency)` that executes an array of async task factory functions. It guarantees that at no point in time are more than `concurrency` promises executing concurrently, and returns results in the original task array order.

### Production Solution

```javascript
export async function promisePool(tasks, concurrency = 2) {
  if (concurrency < 1) throw new Error('Concurrency must be >= 1');
  if (tasks.length === 0) return [];

  const results = new Array(tasks.length);
  let currentIndex = 0;

  async function worker() {
    while (currentIndex < tasks.length) {
      const index = currentIndex++;
      const task = tasks[index];
      results[index] = await task();
    }
  }

  // Spawn pool worker threads up to concurrency limit
  const workerPool = Array.from(
    { length: Math.min(concurrency, tasks.length) },
    () => worker()
  );

  await Promise.all(workerPool);
  return results;
}
```

---

## 5. Challenge 4: Nested Object Flatten & Unflatten Pipeline

### Problem Statement
Implement two inverse functions:
1. `flattenObject(obj)`: Transforms nested objects into flat key-value pairs with dot paths:
   `{ a: { b: 1 } }` $\to$ `{ "a.b": 1 }`
2. `unflattenObject(flatObj)`: Reconstructs the nested structure from dot-path records.

### Production Solution

```javascript
export function flattenObject(obj, prefix = '', result = {}) {
  for (const [key, val] of Object.entries(obj)) {
    const fullPath = prefix ? `${prefix}.${key}` : key;

    if (val !== null && typeof val === 'object' && !Array.isArray(val) && !(val instanceof Date)) {
      flattenObject(val, fullPath, result);
    } else {
      result[fullPath] = val;
    }
  }
  return result;
}

export function unflattenObject(flatObj) {
  const result = {};

  for (const [pathStr, val] of Object.entries(flatObj)) {
    const keys = pathStr.split('.');
    let curr = result;

    for (let i = 0; i < keys.length - 1; i++) {
      const k = keys[i];
      if (!(k in curr) || typeof curr[k] !== 'object' || curr[k] === null) {
        curr[k] = {};
      }
      curr = curr[k];
    }

    curr[keys[keys.length - 1]] = val;
  }

  return result;
}
```

---

## 6. Evaluation Rubric for Senior Candidates

| Evaluation Criteria | Strong Hire (Staff / Senior) | Borderline / Needs Improvement |
| :--- | :--- | :--- |
| **Complexity Analysis** | Explains exact $O(1)$ invariants, memory footprint, and GC effects | Hand-waves Big-O; fails to notice $O(N)$ operations in loops |
| **Resilience & Edge Cases** | Checks `null`, non-objects, negative bounds, abort signals | Crashes on empty inputs or unhandled edge values |
| **Concurrency Cleanliness** | Manages promise queues deterministically with zero race conditions | Employs naive `setTimeout` polling loops |
| **Code Style** | Clean, self-documenting, modular, ES2024 idioms | Monolithic spaghetti code; mutable globals |
