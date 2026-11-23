# Concurrency Control in JavaScript

> Synchronization patterns, asynchronous mutexes and semaphores, check-then-act race conditions, actor architectures, and rate limiting.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Asynchronous Model](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript/01_asynchronous-model.md), [Concurrency vs Parallelism](file:///e:/Learning/engineering-notes/01-languages/javascript/17_concurrency-and-parallelism/01_concurrency-vs-parallelism.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js, Evergreen Browsers |

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

A widespread misconception among JavaScript developers is that because JavaScript is "single-threaded," race conditions and data corruption cannot occur. While it is true that two instructions cannot execute simultaneously on the same physical thread, **asynchronous operations interleave at every `await` or callback boundary**.

Whenever a program reads a shared resource (a database row, a file on disk, an in-memory token, or an object property), awaits an asynchronous operation, and then updates that resource based on the earlier read, a **Time-of-Check to Time-of-Use (TOCTOU)** race condition exists. Other concurrent requests can interleave and mutate that state during the wait period.

Concurrency control provides mechanisms—such as Asynchronous Mutexes, Counting Semaphores, Read-Write Locks, Token Buckets, and the Actor Model—to coordinate access to shared state across concurrent asynchronous tasks and parallel threads.

---

## 2. Core Concepts

### Single-Threaded Asynchronous Race Conditions
Consider a bank account balance deduction:
```javascript
// DANGEROUS: Classic Check-Then-Act Race Condition
async function withdraw(userId, amount) {
  const balance = await db.getBalance(userId); // 1. Read
  if (balance >= amount) {                      // 2. Check
    await db.saveBalance(userId, balance - amount); // 3. Act
    return true;
  }
  return false;
}
```
If two simultaneous withdrawal requests of $100 arrive for an account with $100 balance, both check the balance before either updates it. Both pass the check, deducting $100 twice and resulting in a -$100 balance.

### Asynchronous Mutex (Mutual Exclusion)
An asynchronous mutex guarantees that only one promise-returning asynchronous operation can execute a critical section at any given moment. Subsequent callers queue in a FIFO promise chain.

### Counting Semaphore
A semaphore generalizes the mutex by permitting up to $N$ concurrent callers to access a limited pool of resources (such as database connections or external API rate limits).

### The Actor Model
In the actor model, shared mutable state is completely eliminated. Instead, an "Actor" is an isolated entity that encapsulates its own private state and processes incoming messages sequentially from an internal message mailbox queue.

---

## 3. How It Works Internally

```
ASYNC MUTEX EXECUTION TIMELINE

Request 1: [ Lock Acquired ]----(await async I/O)----[ Release ]
                                                          |
Request 2:              [ Queued in Promise Chain ]------>[ Lock Acquired ]----(await I/O)
                                                                                  |
Request 3:                           [ Queued ]---------------------------------->[ Lock Acquired ]

PROMISE QUEUE MECHANICS:
+-------------------------------------------------------------------------+
| Mutex.prototype.dispatch                                                |
|   let currentChain = Promise.resolve();                                 |
|                                                                         |
|   function runExclusive(task) {                                         |
|     const result = currentChain.then(() => task());                     |
|     currentChain = result.catch(() => {}); // Maintain unbroken chain  |
|     return result;                                                      |
|   }                                                                     |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Comparison of Concurrency Control Primitives

| Primitive | Allowed Concurrency | Typical Use Case |
| :--- | :--- | :--- |
| **Mutex** | Exactly 1 | Protecting in-memory cache updates, write locks |
| **Semaphore** | Fixed $N$ | Throttling database connection pools, file descriptor usage |
| **Read-Write Lock** | Multiple Readers OR 1 Writer | Read-heavy caches with infrequent mutations |
| **Token Bucket** | Sustained rate $R$ + Burst $B$ | API rate limiting |
| **Actor Mailbox** | 1 message at a time per Actor | Entity state machines, chat rooms, game sessions |

---

## 5. Practical Examples

### Example 1: Robust Asynchronous Mutex Implementation
```javascript
export class AsyncMutex {
  constructor() {
    this._queue = [];
    this._locked = false;
  }

  acquire() {
    return new Promise((resolve) => {
      if (!this._locked) {
        this._locked = true;
        resolve(this._release.bind(this));
      } else {
        this._queue.push(resolve);
      }
    });
  }

  _release() {
    if (this._queue.length > 0) {
      const next = this._queue.shift();
      next(this._release.bind(this));
    } else {
      this._locked = false;
    }
  }

  async runExclusive(task) {
    const release = await this.acquire();
    try {
      return await task();
    } finally {
      release();
    }
  }
}

// Usage:
const mutex = new AsyncMutex();
const result = await mutex.runExclusive(async () => {
  const balance = await getBalance(userId);
  await saveBalance(userId, balance - amount);
});
```

### Example 2: Counting Semaphore for Connection Limiting
```javascript
export class AsyncSemaphore {
  constructor(maxConcurrency) {
    this.maxConcurrency = maxConcurrency;
    this.current = 0;
    this.queue = [];
  }

  acquire() {
    return new Promise((resolve) => {
      if (this.current < this.maxConcurrency) {
        this.current++;
        resolve(this._release.bind(this));
      } else {
        this.queue.push(resolve);
      }
    });
  }

  _release() {
    this.current--;
    if (this.queue.length > 0) {
      this.current++;
      const nextResolve = this.queue.shift();
      nextResolve(this._release.bind(this));
    }
  }

  async use(fn) {
    const release = await this.acquire();
    try {
      return await fn();
    } finally {
      release();
    }
  }
}

// Usage: Limit scraping to max 3 concurrent HTTP sockets
const semaphore = new AsyncSemaphore(3);
const pages = await Promise.all(urls.map(url => semaphore.use(() => fetch(url))));
```

### Example 3: Read-Write Lock (RWLock)
```javascript
export class AsyncRWLock {
  constructor() {
    this.readers = 0;
    this.writer = false;
    this.waitingWriters = [];
    this.waitingReaders = [];
  }

  async acquireRead() {
    if (this.writer || this.waitingWriters.length > 0) {
      await new Promise(resolve => this.waitingReaders.push(resolve));
    }
    this.readers++;
    return () => this.releaseRead();
  }

  releaseRead() {
    this.readers--;
    if (this.readers === 0 && this.waitingWriters.length > 0) {
      const nextWriter = this.waitingWriters.shift();
      this.writer = true;
      nextWriter();
    }
  }

  async acquireWrite() {
    if (this.writer || this.readers > 0) {
      await new Promise(resolve => this.waitingWriters.push(resolve));
    }
    this.writer = true;
    return () => this.releaseWrite();
  }

  releaseWrite() {
    this.writer = false;
    if (this.waitingReaders.length > 0) {
      // Prioritize flushing all waiting readers
      const readersToWake = this.waitingReaders.splice(0);
      this.readers = readersToWake.length;
      readersToWake.forEach(r => r());
    } else if (this.waitingWriters.length > 0) {
      const nextWriter = this.waitingWriters.shift();
      this.writer = true;
      nextWriter();
    }
  }
}
```

### Example 4: The Actor Model Pattern in JavaScript
```javascript
export class UserActor {
  constructor(userId, initialBalance = 0) {
    this.userId = userId;
    this._balance = initialBalance;
    this._mailbox = Promise.resolve(); // Sequential FIFO queue
  }

  send(action) {
    // Enqueue message execution sequentially
    this._mailbox = this._mailbox.then(async () => {
      try {
        await action(this);
      } catch (err) {
        console.error(`Actor ${this.userId} error:`, err);
      }
    });
    return this._mailbox;
  }

  withdraw(amount) {
    return this.send(async (self) => {
      if (self._balance >= amount) {
        await new Promise(r => setTimeout(r, 50)); // Simulating remote I/O
        self._balance -= amount;
        console.log(`Withdrew ${amount}. Balance: ${self._balance}`);
      } else {
        console.log(`Declined withdrawal of ${amount}. Balance: ${self._balance}`);
      }
    });
  }
}

// Concurrent requests are automatically serialized safely!
const account = new UserActor('user-100', 100);
account.withdraw(100);
account.withdraw(100); // Safely rejected!
```

### Example 5: Token Bucket Rate Limiter
```javascript
export class TokenBucket {
  constructor(capacity, refillRatePerSecond) {
    this.capacity = capacity;
    this.refillRate = refillRatePerSecond;
    this.tokens = capacity;
    this.lastRefill = Date.now();
  }

  _refill() {
    const now = Date.now();
    const elapsed = (now - this.lastRefill) / 1000;
    this.tokens = Math.min(this.capacity, this.tokens + elapsed * this.refillRate);
    this.lastRefill = now;
  }

  tryConsume(tokens = 1) {
    this._refill();
    if (this.tokens >= tokens) {
      this.tokens -= tokens;
      return true;
    }
    return false;
  }

  async waitForToken(tokens = 1) {
    while (!this.tryConsume(tokens)) {
      await new Promise(r => setTimeout(r, 50));
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Deadlocks via Nested Lock Acquisition**: If Task A acquires Lock 1 and awaits Lock 2, while Task B acquires Lock 2 and awaits Lock 1, both tasks permanently freeze execution. Always acquire locks in a globally consistent hierarchical order or use a single unified lock.
2. **Missing `finally` Blocks**: If an asynchronous function acquires a lock and encounters an unhandled exception before calling `release()`, the lock is permanently leaked and all subsequent requests are blocked forever. Always wrap lock releases in `try ... finally`.
3. **Promise Chain Unhandled Rejection Leaks**: When building FIFO promise queues, if a rejected promise is not caught inside the internal queue chain (`currentChain = result.catch(() => {})`), the queue breaks and Node.js emits an `UnhandledPromiseRejection`.
4. **Keyed Locks for Multi-Tenant Systems**: A global mutex serializes *all* users, destroying backend throughput. Always use a keyed lock map (`Map<userId, AsyncMutex>`) so operations on User A do not block User B.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Checking balance then saving without locks | TOCTOU race condition allows double spending | Protect critical section with an Async Mutex |
| Releasing locks outside `try ... finally` | Uncaught exceptions leak locks permanently | Always call `release()` in a `finally` block |
| Global mutex across different entities | Serializes unrelated users, destroying server throughput | Use Keyed Mutexes partitioned by Entity ID |
| Unbounded in-memory queue growth | Leaks memory if producer is faster than consumer | Enforce queue size limits and apply backpressure |
| Spinning on a boolean flag `while (isLocked) await sleep()` | Inefficient, violates FIFO fairness, causes starvation | Use promise-based queues with `resolve` callbacks |
| Storing distributed locks purely in local Node RAM | Fails across multiple server replicas or cluster workers | Use Redis Distributed Locks (Redlock) or DB row locks |

---

## 8. Performance Characteristics

- **Promise Queue Overhead**: Creating and resolving microtask promises incurs $< 1$ microsecond of overhead.
- **Keyed Lock Garbage Collection**: Keyed lock maps must prune idle mutex instances when their queue is empty to avoid leaking memory over millions of unique keys.
- **Fairness Guarantee**: FIFO promise queues guarantee starvation-free execution for queued requests.

---

## 9. Engine Internals

1. **Microtask Queue Serialization**:
   - When `nextResolve()` is invoked in a release function, the pending promise moves into V8's microtask queue.
   - Microtasks run immediately after the current synchronous stack frame unwinds, ensuring the next queued task begins execution without delay.
2. **WeakRef and FinalizationRegistry for Keyed Mutexes**:
   - Advanced keyed lock managers use `WeakRef` to hold lock references, allowing the V8 garbage collector to automatically reclaim memory when tasks finish and callers discard references.

---

## 10. Best Practices & Design Guidelines

- **Keep Critical Sections Short**: Minimize the duration a lock is held. Never perform unnecessary network calls or slow disk operations while holding an exclusive lock.
- **Apply Lock Timeouts**: Always equip lock acquisitions with an `AbortController` or timeout so that stuck I/O operations abort rather than indefinitely holding resources.
- **Partition Granularity**: Lock at the finest possible granularity (e.g., individual order ID rather than entire product inventory).

---

## 11. Real-World Case Study

### Keyed Mutex with Auto-Cleanup and Timeout Protection
```javascript
class KeyedMutexPool {
  constructor(timeoutMs = 5000) {
    this.timeoutMs = timeoutMs;
    this.locks = new Map();
  }

  async runExclusive(key, task) {
    let mutex = this.locks.get(key);
    if (!mutex) {
      mutex = { queue: Promise.resolve(), refCount: 0 };
      this.locks.set(key, mutex);
    }
    mutex.refCount++;

    const execution = mutex.queue.then(async () => {
      let timer;
      const timeoutPromise = new Promise((_, reject) => {
        timer = setTimeout(() => reject(new Error(`Lock timeout for ${key}`)), this.timeoutMs);
      });

      try {
        return await Promise.race([task(), timeoutPromise]);
      } finally {
        clearTimeout(timer);
      }
    });

    mutex.queue = execution.catch(() => {});

    try {
      return await execution;
    } finally {
      mutex.refCount--;
      if (mutex.refCount === 0) {
        this.locks.delete(key); // Free memory when queue empty
      }
    }
  }
}

// Production usage:
const orderLocker = new KeyedMutexPool(3000);
await orderLocker.runExclusive(`order:${orderId}`, async () => {
  await processPayment(orderId);
  await markOrderPaid(orderId);
});
```

---

## 12. Interview Questions

<details>
<summary>1. Why can race conditions happen in single-threaded JavaScript?</summary>
Because JavaScript's single thread can interleave asynchronous tasks across <code>await</code> or callback boundaries. If one task reads a value, awaits an asynchronous operation, and then updates the value based on that stale read, another concurrent task can interleave during the wait and mutate the state, causing data corruption (Check-then-Act / TOCTOU race condition).
</details>

<details>
<summary>2. What is the purpose of an asynchronous Mutex?</summary>
An asynchronous mutex ensures mutual exclusion for a block of asynchronous code, ensuring that only one task can execute the critical section at a time, queueing any additional concurrent requests in a FIFO promise chain.
</details>

<details>
<summary>3. Why must lock releases always be placed inside a finally block?</summary>
If an exception or promise rejection occurs within the critical section and the release call is not inside a <code>finally</code> block, the release will never be executed. The lock remains locked forever, permanently deadlocking all subsequent queued requests.
</details>

<details>
<summary>4. What is the difference between a Mutex and a Semaphore?</summary>
A Mutex allows only a single task (concurrency of 1) to access a resource at any given time. A Semaphore allows a configurable number $N$ ($N \ge 1$) of concurrent tasks to access a shared resource pool simultaneously.
</details>

<details>
<summary>5. How does a Read-Write Lock improve performance over a standard Mutex?</summary>
In read-heavy applications, multiple tasks can read shared data concurrently without causing data races. A Read-Write Lock allows an arbitrary number of concurrent readers to access the data simultaneously, while granting exclusive access to only one writer when mutations occur.
</details>

<details>
<summary>6. How can you prevent deadlocks when acquiring multiple locks?</summary>
Deadlocks can be prevented by establishing a strict global locking hierarchy (always acquiring Lock A before Lock B), using lock timeouts that abort acquisition if a lock cannot be obtained in time, or grouping resources into a single coarse-grained lock.
</details>

<details>
<summary>7. What is the Actor model and how does it prevent race conditions?</summary>
The Actor model avoids shared mutable state by encapsulating state inside isolated actors. Actors communicate exclusively by sending messages. Each actor maintains a FIFO mailbox queue, processing one message at a time sequentially, thereby eliminating concurrent mutation races by design.
</details>

<details>
<summary>8. Why is a local in-memory Mutex insufficient in a clustered Node.js application?</summary>
In clustered Node.js setups or multi-container Kubernetes deployments, each process runs in its own memory space. An in-memory Mutex only synchronizes tasks within that single OS process. To synchronize across processes and servers, a distributed lock manager (such as Redis Redlock or database row locks) is required.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Fair Batching Queue
Implement an asynchronous batcher `AsyncBatcher` that collects individual items into an array and flushes them to a database in batches of up to 10 items or after a 50ms timeout, returning the individual database results back to the original callers.

<details>
<summary>View Solution</summary>

```javascript
export class AsyncBatcher {
  constructor(batchHandler, maxBatchSize = 10, maxWaitMs = 50) {
    this.batchHandler = batchHandler;
    this.maxBatchSize = maxBatchSize;
    this.maxWaitMs = maxWaitMs;
    this.queue = [];
    this.timer = null;
  }

  add(item) {
    return new Promise((resolve, reject) => {
      this.queue.push({ item, resolve, reject });

      if (this.queue.length >= this.maxBatchSize) {
        this.flush();
      } else if (!this.timer) {
        this.timer = setTimeout(() => this.flush(), this.maxWaitMs);
      }
    });
  }

  async flush() {
    clearTimeout(this.timer);
    this.timer = null;

    if (this.queue.length === 0) return;

    const currentBatch = this.queue.splice(0, this.maxBatchSize);
    const items = currentBatch.map(b => b.item);

    try {
      const results = await this.batchHandler(items);
      currentBatch.forEach((b, idx) => b.resolve(results[idx]));
    } catch (err) {
      currentBatch.forEach(b => b.reject(err));
    }
  }
}
```
</details>

### Exercise 2: Implementing a Distributed Lock Interface (Redlock Pattern)
Design an asynchronous distributed lock interface that implements acquire with TTL, heartbeats (lock renewal), and safe release using unique lock ownership tokens.

<details>
<summary>View Solution</summary>

```javascript
export class DistributedLock {
  constructor(client, resourceKey, ttlMs = 5000) {
    this.client = client;
    this.resourceKey = `lock:${resourceKey}`;
    this.ttlMs = ttlMs;
    this.token = crypto.randomUUID();
    this.heartbeat = null;
  }

  async acquire() {
    // Redis SET resourceKey token NX PX ttlMs
    const acquired = await this.client.set(this.resourceKey, this.token, 'PX', this.ttlMs, 'NX');
    if (!acquired) {
      return false;
    }

    // Start auto-renewal heartbeat
    this.heartbeat = setInterval(async () => {
      const renewScript = `
        if redis.call("get", KEYS[1]) == ARGV[1] then
          return redis.call("pexpire", KEYS[1], ARGV[2])
        else
          return 0
        end
      `;
      await this.client.eval(renewScript, 1, this.resourceKey, this.token, this.ttlMs);
    }, Math.floor(this.ttlMs / 2));

    return true;
  }

  async release() {
    clearInterval(this.heartbeat);
    // Safe release: only delete if the token matches (avoid releasing someone else's expired lock)
    const releaseScript = `
      if redis.call("get", KEYS[1]) == ARGV[1] then
        return redis.call("del", KEYS[1])
      else
        return 0
      end
    `;
    return await this.client.eval(releaseScript, 1, this.resourceKey, this.token);
  }
}
```
</details>
