# Networking Architecture Patterns

> Enterprise networking patterns in JavaScript: resilient Circuit Breakers, Request Preemption, Single-Flight deduplication, offline synchronization queues, and protocol selection heuristics.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [HTTP Fundamentals](./01_http-fundamentals.md), [The Fetch API](./02_fetch.md), [WebSockets & SSE](./05_websockets-and-sse.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | Enterprise Distributed Systems Standards / Microservices Resilience Patterns |

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

In distributed systems, the network is fundamentally unreliable. Remote services experience transient latency spikes, database connection pools exhaust, reverse proxies restart, and mobile clients lose cellular coverage. Naive client implementations that blindly retry requests or send unbounded concurrent queries turn minor downstream hiccups into total cascading system outages (the "thundering herd" problem).

To build resilient, high-availability software, JavaScript engineers must implement battle-tested **Networking Architecture Patterns**:
- **Circuit Breaker**: Prevents a client from repeatedly hammering an already-failing downstream service.
- **Request Preemption (Single-Flight & Abort)**: Cancels superseded in-flight requests during rapid user interactions.
- **Exponential Backoff with Full Jitter**: Desynchronizes retry attempts to allow recovering servers to heal.
- **Offline Mutation Queues**: Queues operations locally in `IndexedDB` and synchronizes them upon network restoration.

---

## 2. Core Concepts

### The Circuit Breaker Pattern
A Circuit Breaker wraps an asynchronous network operation and tracks failures. It operates across three states:
1. **CLOSED (Normal Operation)**: Requests pass through freely. If consecutive failures exceed a threshold, the breaker **TRIPS** into the OPEN state.
2. **OPEN (Fail Fast)**: All requests fail immediately without attempting network I/O, preventing resource starvation and protecting the backend. After a cooldown duration, it transitions to HALF-OPEN.
3. **HALF-OPEN (Trial Probe)**: A limited number of trial requests are permitted through. If they succeed, the breaker resets to CLOSED. If they fail, it immediately reverts to OPEN.

```text
               +-----------------------------------------+
               |                 CLOSED                  |
               |         (Normal Network Traffic)        |
               +-----------------------------------------+
                     │                             ▲
      Failures reach │                             │ Trial requests
      threshold      ▼                             │ succeed
               +-----------------------------------------+
               |                  OPEN                   |
               |         (Fail-Fast Immediately)         |
               +-----------------------------------------+
                     │                             ▲
      Cooldown timer │                             │ Trial request
      expires        ▼                             │ fails
               +-----------------------------------------+
               |                HALF-OPEN                |
               |         (Send Trial Probe Query)        |
               +-----------------------------------------+
```

### Protocol Selection Matrix

| Use Case | Recommended Protocol | Primary Rationale |
| :--- | :--- | :--- |
| **Standard CRUD REST / GraphQL** | HTTP/2 or HTTP/3 `fetch()` | Multiplexed, cachable, simple error boundaries |
| **Live Dashboards & LLM Streaming**| Server-Sent Events (SSE) | Built-in reconnection, works over HTTP/2 |
| **Collaborative Docs & Live Gaming**| WebSockets (`wss://`) | Low latency, bidirectional, binary frame support |
| **High-Frequency Unreliable Telemetry**| WebTransport Datagrams | Runs over UDP (QUIC); zero head-of-line blocking |

---

## 3. How It Works Internally

### Single-Flight Request Deduplication Architecture

When five independent UI components request identical user profile data simultaneously:

```
 Component A: fetch('/user/10') ────┐
 Component B: fetch('/user/10') ────┼───> [ Single-Flight Registry ]
 Component C: fetch('/user/10') ────┘               │
                                                    ▼
                                         Is request active?
                                               /         \
                                         YES  /           \ NO
                                             v             v
                                 [ Return Existing ]  [ Fire 1 Real Network Request ]
                                 [ Pending Promise ]  [ Store Promise in Map       ]
                                                           │
                                                           ▼
                                         Network settles (resolve/reject)
                                         All components resolve simultaneously
                                         Delete key from Registry in finally block
```

---

## 4. Syntax & API Reference

### Architectural Interface Contracts

```javascript
// Circuit Breaker signature
const breaker = new CircuitBreaker(asyncFetchFn, {
  failureThreshold: 5,  // Trip after 5 consecutive failures
  cooldownMs: 30000,    // Remain open for 30 seconds
});
const data = await breaker.execute('/api/resource');

// Idempotent retry with jitter signature
const result = await retryWithJitter(fetchFn, {
  maxRetries: 4,
  baseDelayMs: 300,
  maxDelayMs: 10000,
});
```

---

## 5. Practical Examples

### Example 1: Production-Grade Circuit Breaker Class
```javascript
class CircuitBreaker {
  #fn;
  #failureThreshold;
  #cooldownMs;
  #state = 'CLOSED'; // 'CLOSED' | 'OPEN' | 'HALF_OPEN'
  #failureCount = 0;
  #nextAttemptTime = 0;

  constructor(fn, options = {}) {
    this.#fn = fn;
    this.#failureThreshold = options.failureThreshold || 5;
    this.#cooldownMs = options.cooldownMs || 30000;
  }

  async execute(...args) {
    const now = Date.now();

    // 1. OPEN Check: Fail fast if still within cooldown
    if (this.#state === 'OPEN') {
      if (now > this.#nextAttemptTime) {
        this.#state = 'HALF_OPEN';
        console.warn('[CircuitBreaker] Transitioned to HALF-OPEN. Probing downstream...');
      } else {
        throw new Error('[CircuitBreaker] Circuit is OPEN. Request rejected immediately.');
      }
    }

    try {
      // 2. Execute underlying network call
      const result = await this.#fn(...args);

      // 3. Success handling: Reset breaker
      if (this.#state === 'HALF_OPEN') {
        console.log('[CircuitBreaker] Probe succeeded. Resetting to CLOSED.');
      }
      this.#reset();
      return result;
    } catch (error) {
      // 4. Failure handling
      this.#recordFailure();
      throw error;
    }
  }

  #recordFailure() {
    this.#failureCount++;
    console.warn(`[CircuitBreaker] Failure recorded (${this.#failureCount}/${this.#failureThreshold})`);

    if (this.#state === 'HALF_OPEN' || this.#failureCount >= this.#failureThreshold) {
      this.#state = 'OPEN';
      this.#nextAttemptTime = Date.now() + this.#cooldownMs;
      console.error(`[CircuitBreaker] Circuit TRIPPED to OPEN. Cooldown: ${this.#cooldownMs}ms`);
    }
  }

  #reset() {
    this.#state = 'CLOSED';
    this.#failureCount = 0;
  }

  get state() { return this.#state; }
}
```

### Example 2: Autocomplete Search with Debounce & Request Preemption
```javascript
class PreemptiveSearchController {
  #currentAbortController = null;
  #endpoint;

  constructor(endpoint) {
    this.#endpoint = endpoint;
  }

  async search(query) {
    // 1. Abort any previous pending in-flight request
    if (this.#currentAbortController) {
      this.#currentAbortController.abort('New keystroke entered');
    }

    // 2. Initialize fresh AbortController for current query
    this.#currentAbortController = new AbortController();
    const { signal } = this.#currentAbortController;

    try {
      const response = await fetch(`${this.#endpoint}?q=${encodeURIComponent(query)}`, {
        signal,
      });

      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      return await response.json();
    } catch (err) {
      if (err.name === 'AbortError') {
        console.log(`Query "${query}" preempted by newer keystroke.`);
        return null; // Gracefully ignore cancelled requests
      }
      throw err;
    }
  }
}
```

### Example 3: Full Jitter Exponential Backoff Algorithm
```javascript
async function fetchWithFullJitter(requestFn, options = {}) {
  const { maxRetries = 4, baseDelayMs = 200, maxDelayMs = 8000 } = options;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    try {
      return await requestFn();
    } catch (error) {
      if (attempt === maxRetries) throw error;

      // Full Jitter Formula: Sleep = rand(0, min(maxDelay, baseDelay * 2^attempt))
      const exponentialCap = Math.min(maxDelayMs, baseDelayMs * 2 ** attempt);
      const sleepDuration = Math.floor(Math.random() * exponentialCap);

      console.warn(`Attempt ${attempt + 1} failed. Jittered sleep: ${sleepDuration}ms`);
      await new Promise(resolve => setTimeout(resolve, sleepDuration));
    }
  }
}
```

### Example 4: Single-Flight (In-Flight Request Deduplication)
```javascript
function createSingleFlight() {
  const inFlightMap = new Map();

  return async function execute(key, asyncFn) {
    if (inFlightMap.has(key)) {
      return inFlightMap.get(key); // Return shared active promise
    }

    const promise = (async () => {
      try {
        return await asyncFn();
      } finally {
        inFlightMap.delete(key); // Clean up immediately when done
      }
    })();

    inFlightMap.set(key, promise);
    return promise;
  };
}
```

### Example 5: Offline Mutation Queue with IndexedDB
```javascript
class OfflineMutationQueue {
  #db;

  async init() {
    this.#db = await new Promise((resolve) => {
      const req = indexedDB.open('OfflineQueueDB', 1);
      req.onupgradeneeded = () => req.result.createObjectStore('mutations', { autoIncrement: true });
      req.onsuccess = () => resolve(req.result);
    });

    window.addEventListener('online', () => this.flush());
  }

  async enqueue(url, method, body) {
    if (navigator.onLine) {
      try {
        return await fetch(url, { method, body: JSON.stringify(body) });
      } catch {
        // Fall through to queue on failure
      }
    }

    const tx = this.#db.transaction('mutations', 'readwrite');
    tx.objectStore('mutations').add({ url, method, body, createdAt: Date.now() });
    console.log('Action queued offline');
  }

  async flush() {
    const tx = this.#db.transaction('mutations', 'readwrite');
    const store = tx.objectStore('mutations');
    const allRecordsReq = store.getAll();

    allRecordsReq.onsuccess = async () => {
      const mutations = allRecordsReq.result;
      store.clear(); // Clear local store before replay

      for (const m of mutations) {
        try {
          await fetch(m.url, { method: m.method, body: JSON.stringify(m.body) });
          console.log(`Synced offline mutation to ${m.url}`);
        } catch (e) {
          console.error('Failed to replay mutation, re-queuing:', e);
          this.enqueue(m.url, m.method, m.body);
        }
      }
    };
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Thundering Herd after Circuit Breaker Closes**: When a service recovers and a circuit breaker resets, thousands of paused clients may flood the service simultaneously. Use randomized delays (jitter) when transitioning from OPEN to HALF-OPEN.
2. **Deduplication Key Collision**: Naive string concatenation for deduplication keys (`url + JSON.stringify(body)`) fails if body properties are ordered differently (`{ a: 1, b: 2 }` vs `{ b: 2, a: 1 }`). Use deterministic object sorting.
3. **Memory Leaks in In-Flight Maps**: If an async operation hangs indefinitely without a timeout, the promise remains permanently in the in-flight deduplication map, leaking memory and preventing future queries. Always pair single-flight operations with `AbortSignal.timeout()`.
4. **Out-of-Order Execution in Offline Queues**: Replaying offline mutations asynchronously with `Promise.all` can cause race conditions (e.g., executing a DELETE before an UPDATE). Replay offline queues sequentially using strict FIFO ordering.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Unjittered Retries** | Retrying every exact 1000ms | Add randomized full jitter | Causes synchronized traffic spikes that re-crash recovering servers. |
| **Hammering Downstream Services**| Retrying in tight loop on 503 | Implement a Circuit Breaker | Keeps failing servers saturated, preventing recovery. |
| **Orphaned Search Requests** | Firing fetch on every keystroke without abort | Cancel previous via `AbortController` | Stale responses arrive out of order and overwrite newer results. |
| **Parallel Offline Sync** | `Promise.all(offlineMutations.map(fetch))` | Replay sequentially (`for...of`) | Can commit database state out of chronological order. |
| **Permanent Single-Flight Errors**| Caching rejected promise permanently | Clear key in `finally` block | One transient network error causes all future callers to fail forever. |
| **Missing Request Timeout** | Infinite fetch without abort signal | Always set `AbortSignal.timeout()` | Sockets stay hung in memory during network blackholes. |

---

## 8. Performance Characteristics

- **Bandwidth Reduction via Preemption**: Aborting superseded autocomplete requests closes network sockets early, saving up to 80% of unnecessary payload downloads on mobile.
- **Circuit Breaker Overhead**: In the CLOSED state, a circuit breaker adds $< 0.01\text{ms}$ execution overhead (simple integer and condition checks). In the OPEN state, it avoids $500\text{ms} - 30\text{s}$ timeout delays by failing in microseconds ($0.05\text{ms}$).

---

## 9. Engine Internals

1. **AbortSignal Memory Reclaim**: When an `AbortController` aborts, Chromium immediately disconnects the C++ socket stream and frees underlying TCP buffers without waiting for application JavaScript garbage collection.
2. **LevelDB Serialization in IndexedDB**: Chromium flushes IndexedDB mutations to disk asynchronously using LevelDB batch transactions, preventing main thread I/O stalls during offline queue saves.

---

## 10. Best Practices & Design Guidelines

1. **Always Combine Retries with Full Jitter**: Randomize sleep durations to break synchronization across clients.
2. **Wrap Remote Dependencies in Circuit Breakers**: Isolate third-party APIs (payment processors, SMS gateways) with circuit breakers.
3. **Enforce Strict Sequential Replay for Offline Queues**: Maintain causal consistency by replaying offline mutations in exact FIFO order.
4. **Preempt In-Flight Requests in Interactive UIs**: Cancel active requests when users type new queries or navigate to new tabs.

---

## 11. Real-World Case Study

### Resilient Order Dispatch System with Circuit Breaker and Offline Queue

A warehouse logistics mobile web app transmits shipment scans. Workers regularly walk through Wi-Fi dead zones, and the central inventory microservice occasionally experiences database connection spikes.

```javascript
class WarehouseScanManager {
  #circuitBreaker;
  #offlineQueue;

  constructor() {
    // 1. Initialize Circuit Breaker around central inventory endpoint
    this.#circuitBreaker = new CircuitBreaker(
      async (scanData) => {
        const res = await fetch('/api/inventory/scan', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(scanData),
          signal: AbortSignal.timeout(5000), // 5s timeout
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      },
      { failureThreshold: 3, cooldownMs: 15000 }
    );

    // 2. Initialize Offline Queue
    this.#offlineQueue = new OfflineMutationQueue();
    this.#offlineQueue.init();
  }

  async recordBarcodeScan(barcode, employeeId) {
    const scanData = { barcode, employeeId, scannedAt: Date.now() };

    try {
      // Attempt live dispatch through Circuit Breaker
      const result = await this.#circuitBreaker.execute(scanData);
      this.#displayStatus('Scan recorded live', 'green');
      return result;
    } catch (err) {
      console.warn('Live dispatch failed. Storing in offline queue:', err.message);

      // Automatically store in IndexedDB offline queue on failure or circuit open
      await this.#offlineQueue.enqueue('/api/inventory/scan', 'POST', scanData);
      this.#displayStatus('Scan saved offline (Queued)', 'orange');
    }
  }

  #displayStatus(msg, color) {
    const statusEl = document.getElementById('scan-status');
    statusEl.textContent = msg;
    statusEl.style.color = color;
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the Circuit Breaker pattern, and what problem does it solve?</strong></summary>

The Circuit Breaker pattern wraps calls to remote services and monitors failure rates. When failures exceed a threshold, it "trips" open, causing subsequent calls to fail immediately without attempting network I/O. This protects failing services from being overwhelmed and prevents client worker threads from blocking on long timeouts.
</details>

<details>
<summary><strong>2. What is "Full Jitter" in retry algorithms, and why is standard exponential backoff insufficient?</strong></summary>

Standard exponential backoff causes all clients that failed simultaneously to retry at identical intervals ($200\text{ms}, 400\text{ms}, 800\text{ms}$), hitting the recovering backend in synchronized waves. Full Jitter picks a random sleep duration uniformly between $0$ and the exponential backoff cap, spreading retries smoothly across time.
</details>

<details>
<summary><strong>3. What is Request Preemption, and how is it implemented in JavaScript?</strong></summary>

Request Preemption is the cancellation of an ongoing, in-flight asynchronous request when a newer, superseding operation is initiated (e.g. typing in an autocomplete search bar). It is implemented by calling `abort()` on an active `AbortController` before initializing a new `fetch()` call.
</details>

<details>
<summary><strong>4. How does Single-Flight (Request Deduplication) prevent cache stampedes?</strong></summary>

When multiple parts of an application request identical data simultaneously, Single-Flight intercepts subsequent requests and binds them to the existing in-flight Promise. When the network request settles, all callers receive the result while executing only a single network request.
</details>

<details>
<summary><strong>5. Why must offline mutation queues be replayed sequentially rather than in parallel?</strong></summary>

Replaying mutations in parallel (`Promise.all`) can cause operations to settle out of chronological order (e.g. processing an "Update Item" before a "Create Item"), corrupting database state. Sequential FIFO replay preserves causal ordering.
</details>

<details>
<summary><strong>6. What are the three states of a Circuit Breaker?</strong></summary>

- **CLOSED**: Normal operation; requests pass through.
- **OPEN**: Fail-fast; requests are rejected immediately without network calls.
- **HALF-OPEN**: Probe state; a small number of trial requests are sent to test if the service has recovered.
</details>

<details>
<summary><strong>7. When should you use Server-Sent Events (SSE) instead of WebSockets?</strong></summary>

When communication is strictly unidirectional from server to client (such as stock price tickers, live sports scores, system notifications, or LLM token streaming). SSE uses standard HTTP, handles reconnection natively, and works over HTTP/2 without custom framing.
</details>

<details>
<summary><strong>8. What is the danger of caching rejected promises in a Single-Flight deduplication cache?</strong></summary>

If a rejected promise is kept in the cache without being removed in a `finally` block, all future callers requesting that key will permanently receive the cached rejection, even after network connectivity is fully restored.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an In-Flight Deduplication Utility with TTL
Write a function `dedupeAsync(fn, ttlMs)` that deduplicates in-flight calls to `fn(...args)` and caches the resolved result for `ttlMs` milliseconds.

<details>
<summary>View Solution</summary>

```javascript
function dedupeAsync(fn, ttlMs = 0) {
  const inFlight = new Map();
  const cache = new Map();

  return async function (...args) {
    const key = JSON.stringify(args);
    const now = Date.now();

    // Check TTL cache
    if (cache.has(key)) {
      const entry = cache.get(key);
      if (now < entry.expiry) {
        return entry.value;
      }
      cache.delete(key);
    }

    // Check In-Flight
    if (inFlight.has(key)) {
      return inFlight.get(key);
    }

    const promise = (async () => {
      try {
        const result = await fn(...args);
        if (ttlMs > 0) {
          cache.set(key, { value: result, expiry: Date.now() + ttlMs });
        }
        return result;
      } finally {
        inFlight.delete(key);
      }
    })();

    inFlight.set(key, promise);
    return promise;
  };
}
```
</details>

### Challenge 2: Implement a Half-Open Circuit Breaker Probe
Extend the `CircuitBreaker` pattern so that in the `HALF_OPEN` state, it allows exactly **1 probe request** through while rejecting any concurrent requests until the probe settles.

<details>
<summary>View Solution</summary>

```javascript
class ProbeCircuitBreaker {
  #fn;
  #state = 'CLOSED';
  #isProbing = false;
  #nextAttempt = 0;
  #cooldownMs = 10000;

  constructor(fn) {
    this.#fn = fn;
  }

  async execute(...args) {
    const now = Date.now();

    if (this.#state === 'OPEN') {
      if (now > this.#nextAttempt) {
        this.#state = 'HALF_OPEN';
        this.#isProbing = false;
      } else {
        throw new Error('Circuit OPEN');
      }
    }

    if (this.#state === 'HALF_OPEN') {
      if (this.#isProbing) {
        // Reject secondary concurrent requests while trial probe is in flight!
        throw new Error('Circuit HALF-OPEN: Probe already in flight');
      }
      this.#isProbing = true;
    }

    try {
      const result = await this.#fn(...args);
      if (this.#state === 'HALF_OPEN') {
        this.#state = 'CLOSED';
      }
      return result;
    } catch (err) {
      this.#state = 'OPEN';
      this.#nextAttempt = Date.now() + this.#cooldownMs;
      throw err;
    } finally {
      this.#isProbing = false;
    }
  }
}
```
</details>
