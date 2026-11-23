# Retry Pattern & Exponential Backoff in JavaScript

> A production-grade implementation and architectural guide to the retry pattern in JavaScript, featuring exponential backoff, full decorrelated jitter, retry budgets, HTTP Retry-After compliance, and AbortSignal cancellation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced / Production-Grade |
| **Prerequisites** | [11_asynchronous-javascript/02_promises.md](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript/02_promises.md), [11_asynchronous-javascript/05_async-await.md](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript/05_async-await.md), [15_networking/01_fetch-api.md](file:///e:/Learning/engineering-notes/01-languages/javascript/15_networking/01_fetch-api.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / Browser Evergreen |
| **Status** | Production Ready |

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

In distributed systems, microservices, and modern web applications, network communication is inherently unreliable. Transient errors frequently occur due to temporary network blips, TCP connection resets, DNS resolution timeouts, downstream database contention, or temporary rate limiting (HTTP 429 Too Many Requests / HTTP 503 Service Unavailable).

Failing immediately on a transient error degrades user experience and system reliability. However, retrying blindly without pacing causes the **Thundering Herd** problem—where thousands of client instances hammer a recovering downstream service simultaneously, driving it into permanent failure.

The **Retry with Exponential Backoff and Jitter** pattern solves this by:
- Automatically attempting transient operations multiple times before declaring failure.
- Progressively increasing the delay between successive attempts ($2^0, 2^1, 2^2, \dots$).
- Injecting cryptographic or pseudo-random **jitter** to desynchronize competing clients and distribute server load evenly over time.

---

## 2. Core Concepts

### Backoff Algorithms
1. **Constant Delay**: Retries every $K$ ms. Prone to synchronization locks.
2. **Linear Backoff**: Delay increases linearly: $D = \text{base} \times \text{attempt}$.
3. **Exponential Backoff**: Delay doubles on each iteration:
   $$\text{Delay}_{\text{exp}} = \min(\text{maxDelay}, \text{baseDelay} \times 2^{\text{attempt}})$$
4. **Full Jitter (Recommended by AWS Architecture)**: Randomizes delay uniformly between $0$ and the exponential ceiling:
   $$\text{Delay}_{\text{full}} = \text{random}(0, \min(\text{maxDelay}, \text{baseDelay} \times 2^{\text{attempt}}))$$
5. **Decorrelated Jitter**: Balances lower latency and randomness:
   $$\text{Delay}_{i} = \min(\text{maxDelay}, \text{random}(\text{baseDelay}, \text{Delay}_{i-1} \times 3))$$

```
Attempt:       1        2              3                            4
No Jitter:    [--1s--] [--2s------]   [--4s----------------]       [--8s----------------------------]
Full Jitter:  [~0.6s]  [~1.3s---]     [~3.1s-------------]         [~5.8s----------------------]
              (Randomized distribution desynchronizes thundering herds)
```

### Idempotency & Retry Eligibility
Only **idempotent** or transient operations may be retried safely:
- **Retryable**: Network timeouts (ETIMEDOUT, ECONNRESET), HTTP 408, 429, 500, 502, 503, 504.
- **Non-Retryable**: Business logic validation errors (HTTP 400 Bad Request), Authentication failures (HTTP 401, 403), Resource Not Found (HTTP 404), Non-idempotent credit card payment submissions without an Idempotency-Key.

---

## 3. How It Works Internally

The retry engine utilizes an asynchronous execution loop coordinated by `Promise` timers and standard `AbortSignal` listeners.

```
       [ Start Operation ]
               │
               ▼
   [ Execute Async Task ] ─── SUCCESS ──► [ Return Result ]
               │
             ERROR
               │
               ▼
   [ Is error retryable? ] ──── NO ─────► [ Throw Error ]
               │ YES
               ▼
   [ Attempts remaining? ] ──── NO ─────► [ Throw MaxRetriesExceeded ]
               │ YES
               ▼
   [ Calculate Backoff + Jitter ]
   (Check HTTP Retry-After header)
               │
               ▼
   [ Await setTimeout Promise ] ◄── AbortSignal.aborted? ──► [ Throw AbortError ]
               │
               ▼
     [ Loop to Next Attempt ]
```

---

## 4. Syntax & API Reference

### Production-Grade Robust Retry Function
```javascript
/**
 * Executes an async task with exponential backoff, jitter, and cancellation support.
 *
 * @template T
 * @param {() => Promise<T>} task - Async function to execute.
 * @param {Object} [options={}] - Configuration options.
 * @param {number} [options.maxRetries=3] - Maximum retry attempts.
 * @param {number} [options.baseDelay=300] - Base delay in milliseconds.
 * @param {number} [options.maxDelay=10000] - Maximum delay ceiling.
 * @param {number} [options.factor=2] - Exponential multiplication factor.
 * @param {'full'|'equal'|'none'} [options.jitter='full'] - Jitter strategy.
 * @param {(err: Error, attempt: number) => boolean} [options.shouldRetry] - Predicate deciding if error is retryable.
 * @param {(err: Error, attempt: number, delay: number) => void} [options.onRetry] - Hook invoked before delay.
 * @param {AbortSignal} [options.signal] - Cancellation signal.
 * @returns {Promise<T>} Resolves with task output or rejects with final error.
 */
export async function retry(task, options = {}) {
  const {
    maxRetries = 3,
    baseDelay = 300,
    maxDelay = 10000,
    factor = 2,
    jitter = 'full',
    shouldRetry = () => true,
    onRetry = () => {},
    signal,
  } = options;

  let attempt = 0;

  function calculateDelay(attemptNumber) {
    const calculated = Math.min(maxDelay, baseDelay * Math.pow(factor, attemptNumber));
    if (jitter === 'none') return calculated;
    if (jitter === 'equal') return (calculated / 2) + (Math.random() * (calculated / 2));
    // Default 'full' jitter
    return Math.floor(Math.random() * calculated);
  }

  function wait(delayMs) {
    return new Promise((resolve, reject) => {
      if (signal?.aborted) {
        return reject(signal.reason ?? new Error('Operation aborted'));
      }

      const timerId = setTimeout(() => {
        cleanup();
        resolve();
      }, delayMs);

      const onAbort = () => {
        cleanup();
        clearTimeout(timerId);
        reject(signal.reason ?? new Error('Operation aborted'));
      };

      function cleanup() {
        signal?.removeEventListener('abort', onAbort);
      }

      signal?.addEventListener('abort', onAbort, { once: true });
    });
  }

  while (true) {
    if (signal?.aborted) {
      throw signal.reason ?? new Error('Operation aborted before execution');
    }

    try {
      return await task();
    } catch (err) {
      attempt++;

      if (attempt > maxRetries || !shouldRetry(err, attempt)) {
        throw err;
      }

      // Check for standard HTTP Retry-After header if attached to error
      let delayMs = calculateDelay(attempt - 1);
      if (err.retryAfterMs && typeof err.retryAfterMs === 'number') {
        delayMs = Math.min(maxDelay, err.retryAfterMs);
      }

      onRetry(err, attempt, delayMs);
      await wait(delayMs);
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Fetch with Exponential Backoff & `Retry-After` Support
```javascript
import { retry } from './retry.js';

export async function resilientFetch(url, init = {}, retryOptions = {}) {
  return retry(
    async () => {
      const response = await fetch(url, init);

      if (!response.ok) {
        const error = new Error(`HTTP Error ${response.status}: ${response.statusText}`);
        error.status = response.status;

        // Parse RFC 7231 Retry-After header (seconds or HTTP date string)
        const retryAfter = response.headers.get('Retry-After');
        if (retryAfter) {
          const seconds = parseInt(retryAfter, 10);
          if (!isNaN(seconds)) {
            error.retryAfterMs = seconds * 1000;
          } else {
            const dateParsed = Date.parse(retryAfter);
            if (!isNaN(dateParsed)) {
              error.retryAfterMs = Math.max(0, dateParsed - Date.now());
            }
          }
        }

        throw error;
      }

      return response;
    },
    {
      maxRetries: 4,
      baseDelay: 500,
      shouldRetry: (err) => {
        // Retry network failures, 429 Rate Limit, and 5xx Server Errors
        if (!err.status) return true; // Network disconnect/DNS error
        return err.status === 408 || err.status === 429 || (err.status >= 500 && err.status <= 599);
      },
      onRetry: (err, attempt, delay) => {
        console.warn(`[resilientFetch] Attempt ${attempt} failed (${err.message}). Retrying in ${delay}ms...`);
      },
      ...retryOptions,
    }
  );
}
```

### Example 2: Database Reconnection with Decorrelated Jitter
```javascript
export async function connectDatabaseWithRetry(connectFn, maxAttempts = 5) {
  let attempt = 0;
  let currentDelay = 500;
  const maxDelay = 15000;

  while (attempt < maxAttempts) {
    try {
      attempt++;
      console.log(`Connecting to database (Attempt ${attempt}/${maxAttempts})...`);
      return await connectFn();
    } catch (err) {
      if (attempt >= maxAttempts) {
        throw new Error(`Database connection failed after ${maxAttempts} attempts: ${err.message}`);
      }

      // Decorrelated Jitter formula: rand(base, currentDelay * 3)
      currentDelay = Math.min(maxDelay, Math.floor(Math.random() * (currentDelay * 3 - 500) + 500));
      console.warn(`Connection refused. Backing off for ${currentDelay}ms...`);
      await new Promise((resolve) => setTimeout(resolve, currentDelay));
    }
  }
}
```

### Example 3: Idempotent Payment Polling with Timeout
```javascript
import { retry } from './retry.js';

export async function pollPaymentStatus(paymentId, userAbortSignal) {
  const timeoutSignal = AbortSignal.timeout(30000); // Max total duration 30s
  const combinedSignal = AbortSignal.any
    ? AbortSignal.any([userAbortSignal, timeoutSignal])
    : timeoutSignal;

  return retry(
    async () => {
      const res = await fetch(`/api/payments/${paymentId}/status`, { signal: combinedSignal });
      const data = await res.json();

      if (data.status === 'PENDING' || data.status === 'PROCESSING') {
        throw new Error('Payment still pending');
      }

      return data;
    },
    {
      maxRetries: 10,
      baseDelay: 1000,
      factor: 1.5,
      maxDelay: 5000,
      signal: combinedSignal,
      shouldRetry: (err) => err.message === 'Payment still pending',
    }
  );
}
```

### Example 4: Resumable File Chunk Upload with Cancellation
```javascript
import { retry } from './retry.js';

export async function uploadFileChunk(chunkBlob, chunkIndex, uploadUrl, parentSignal) {
  return retry(
    async () => {
      const controller = new AbortController();
      const onParentAbort = () => controller.abort(parentSignal.reason);
      parentSignal.addEventListener('abort', onParentAbort, { once: true });

      try {
        const response = await fetch(uploadUrl, {
          method: 'PUT',
          headers: {
            'Content-Type': 'application/octet-stream',
            'X-Chunk-Index': String(chunkIndex),
          },
          body: chunkBlob,
          signal: controller.signal,
        });

        if (!response.ok) {
          throw new Error(`Chunk ${chunkIndex} upload rejected: ${response.status}`);
        }

        return await response.json();
      } finally {
        parentSignal.removeEventListener('abort', onParentAbort);
      }
    },
    {
      maxRetries: 3,
      baseDelay: 1000,
      signal: parentSignal,
      shouldRetry: (err) => !parentSignal.aborted,
    }
  );
}
```

### Example 5: Multi-Endpoint Regional Failover
```javascript
export async function fetchWithFailover(endpoints, requestPath, options = {}) {
  let lastError;

  for (const endpoint of endpoints) {
    try {
      const targetUrl = new URL(requestPath, endpoint).toString();
      return await retry(
        async () => {
          const res = await fetch(targetUrl, options);
          if (!res.ok && res.status >= 500) throw new Error(`Server error ${res.status}`);
          return await res.json();
        },
        { maxRetries: 2, baseDelay: 200 }
      );
    } catch (err) {
      console.warn(`Endpoint ${endpoint} failed. Failing over to next regional node...`);
      lastError = err;
    }
  }

  throw new Error(`All regional endpoints failed. Last error: ${lastError?.message}`);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Non-Idempotent Mutations (Double Charges)**:
   Retrying a `POST /api/charges` endpoint without an `Idempotency-Key` header will cause multiple credit card charges if the initial request succeeded on the server but the TCP ACK dropped on the return path. Never retry non-idempotent operations without unique transaction idempotency keys.
2. **The Thundering Herd Effect**:
   If 10,000 clients fail simultaneously due to a database restart and all retry at fixed 2-second intervals ($2\text{s}, 4\text{s}, 8\text{s}$ without jitter), their requests arrive in synchronized spikes, immediately knocking the database back offline. Jitter is mandatory in production.
3. **Memory Leaks from Stale Event Listeners**:
   When combining `AbortSignal` with retry timers, failure to remove the `abort` event listener when the timer resolves causes listener accumulation on long-lived signals, leading to memory leaks and Node.js `MaxListenersExceededWarning`.
4. **Retry Storm Amplification**:
   If Service A calls Service B (which retries 3 times), and Service B calls Service C (which retries 3 times), a failure at Service C triggers $3 \times 3 = 9$ calls. In a deep microservice call tree, fan-out multiplier storms crash upstream dependencies.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Retrying HTTP 400 or 404 errors | Wastes network resources; client payload is permanently invalid. | Filter retries through `shouldRetry` checking for transient codes. |
| Retrying without Jitter | Causes periodic synchronized spikes (thundering herd) crushing services. | Apply Full Jitter (`Math.random() * calculatedDelay`). |
| Missing overall timeout ceiling | A 5-attempt retry loop with exponential backoff can hang for minutes. | Impose a global deadline using `AbortSignal.timeout(30000)`. |
| Blindly retrying non-idempotent POSTs | Duplicates side effects (duplicate orders, emails, billing). | Require server-enforced `Idempotency-Key` headers. |
| Infinite retry loops (`while(true)`) | Consumes thread memory and leaks workers if backend is permanently down. | Enforce strict `maxRetries` and circuit breakers. |
| Ignoring `Retry-After` HTTP headers | Violates server rate limits, resulting in prolonged IP bans. | Parse `Retry-After` and pause execution for the requested duration. |

---

## 8. Performance Characteristics

- **Event Loop Impact**: Because delays are backed by `Promise`-wrapped `setTimeout`, the main JavaScript thread remains completely free to handle I/O and user events during the wait periods. Zero CPU is consumed while sleeping.
- **Memory Footprint**: Each paused retry loop retains a single lexical closure containing the current attempt counter and configuration objects (~180 bytes in V8 heap).
- **Latency Distribution**: Full jitter achieves an average delay equal to half the theoretical exponential maximum ($\frac{1}{2}\text{Delay}_{\text{max}}$), delivering lower average recovery latency compared to fixed backoff while maintaining optimal load distribution.

---

## 9. Engine Internals

### V8 Promise Resolution & Async De-sugaring
When executing `await retry(...)`, V8 desugars the `async/await` loop into a finite state machine:
1. When `await wait(delayMs)` is encountered, V8 captures the execution context and suspends the generator state machine.
2. The timer callback registers in the libuv event loop timer queue.
3. Upon timer expiry, libuv schedules the callback in the event loop macrotask queue.
4. Calling `resolve()` transitions the internal Promise from `kPending` to `kFulfilled`, queuing a microtask that resumes the V8 state machine generator at the next iteration.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Jitter**: Add randomized jitter to prevent client synchronization.
2. **Respect `Retry-After`**: Honor downstream server backoff instructions over local formulas.
3. **Combine with Circuit Breakers**: If failure rate exceeds 50% across requests, trip a circuit breaker immediately to prevent wasting compute on doomed retries.
4. **Implement Global Deadlines**: Guard the entire retry lifespan with an `AbortSignal.timeout()`.
5. **Log Every Retry Attempt**: Emit structured warning logs including attempt index, error name, and chosen delay to maintain observability in APM tools.

---

## 11. Real-World Case Study

### Payment Gateway Webhook Dispatcher
**Scenario**: A SaaS billing platform delivers webhook notifications to third-party merchant URLs upon successful invoice payments. Many merchant servers experience intermittent downtime or cold starts.
**Architecture**:
- Webhook dispatches are processed by a Node.js worker pool.
- Each webhook dispatch uses `retry()` with exponential backoff and full jitter:
  - Base delay: 5 seconds; Factor: 2; Max delay: 1 hour; Max retries: 8.
- If an endpoint returns HTTP 429, the worker parses `Retry-After` and re-queues the job accordingly.
- If all 8 attempts fail over a 4-hour window, the webhook payload is routed to a Dead Letter Queue (DLQ) and an alert email is dispatched to the merchant administrator.
**Outcome**:
- Webhook delivery success rate increased from 91.2% to 99.98%.
- Zero instances of webhook servers self-DDoS'ing recovering merchant backends.

---

## 12. Interview Questions

<details>
<summary>1. Why is Full Jitter preferred over pure Exponential Backoff in distributed systems?</summary>

Pure exponential backoff delays requests predictably ($2^i \times \text{base}$). If a network hiccup causes 5,000 clients to fail simultaneously, all 5,000 clients will retry at the exact same intervals, creating massive cyclical traffic spikes ("thundering herds") that crash recovering downstream servers. Full Jitter randomizes the delay uniformly between 0 and the exponential maximum, smoothing the aggregate retry volume into a flat, steady stream of requests.
</details>

<details>
<summary>2. What is an Idempotency Key, and why is it essential when retrying mutating requests?</summary>

An Idempotency Key is a unique UUID generated by the client and sent in an HTTP header (e.g. `Idempotency-Key: <uuid>`). If a network connection drops after the server successfully processes a payment but before the client receives the response, retrying the request could cause a duplicate charge. With an idempotency key, the server detects the duplicate UUID, skips reprocessing the transaction, and returns the cached result of the original execution.
</details>

<details>
<summary>3. Which HTTP status codes should NOT be retried?</summary>

Client error codes (4xx) indicating deterministic invalid input should not be retried because subsequent attempts with the same data will produce the identical error:
- `400 Bad Request` (invalid schema/syntax)
- `401 Unauthorized` / `403 Forbidden` (invalid credentials or insufficient permissions)
- `404 Not Found` (resource does not exist)
- `405 Method Not Allowed`
- `422 Unprocessable Entity`
Exceptions include `408 Request Timeout` and `429 Too Many Requests`, which are transient and explicitly retryable.
</details>

<details>
<summary>4. How does <code>AbortSignal.any()</code> improve retry control in modern JavaScript?</summary>

`AbortSignal.any([userSignal, timeoutSignal])` combines multiple cancellation signals into a single unified signal. This allows a retry loop to be canceled either when the user manually navigates away (`userSignal`) OR when an overall SLA deadline expires (`AbortSignal.timeout(15000)`), whichever occurs first, without having to manually manage separate event listeners.
</details>

<details>
<summary>5. What is a "Retry Budget" and why is it used in high-scale microservices?</summary>

A Retry Budget limits the total percentage of outbound requests that can be retries (e.g., maximum 10% of total traffic). If downstream services experience an outage, aggressive client retries can quickly double or triple total traffic. A retry budget enforces a circuit breaker: once retries exceed the designated percentage, subsequent retries are immediately aborted to protect both client resources and downstream capacity.
</details>

<details>
<summary>6. How do you parse and prioritize the HTTP <code>Retry-After</code> header?</summary>

The `Retry-After` header can appear in two formats:
1. Integer seconds: `Retry-After: 120` (delay for 120 seconds).
2. HTTP-date: `Retry-After: Wed, 21 Oct 2026 07:28:00 GMT`.
The retry logic parses the header; if numeric, it converts seconds to milliseconds; if a date string, it computes `Math.max(0, targetDate - Date.now())`. This value overrides the locally computed backoff delay.
</details>

<details>
<summary>7. What happens to active timer callbacks if an AbortSignal fires during a retry sleep delay?</summary>

If an `AbortSignal` fires while the retry function is awaiting a sleep promise, an unhandled timer would keep the Node.js event loop alive and execute uselessly later. A robust implementation attaches an `abort` listener to the signal that immediately calls `clearTimeout(timerId)`, detaches the listener, and rejects the promise with an `AbortError`.
</details>

<details>
<summary>8. How does the Decorrelated Jitter formula differ from Full Jitter?</summary>

Full Jitter always chooses a delay between $0$ and the exponential backoff ceiling, meaning some retries can fire almost immediately ($0\text{ms}$). Decorrelated Jitter maintains state between attempts, calculating `delay = min(maxDelay, random(baseDelay, previousDelay * 3))`. This prevents the delay from collapsing to near-zero while still providing sufficient randomness to break synchronization.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement a Basic Retry Helper
**Specification**: Write `retryAsync(fn, retries, delayMs)` that attempts `fn` up to `retries` times with a fixed delay between failures.

<details>
<summary>Solution</summary>

```javascript
export async function retryAsync(fn, retries = 3, delayMs = 1000) {
  let lastError;
  for (let i = 0; i <= retries; i++) {
    try {
      return await fn();
    } catch (err) {
      lastError = err;
      if (i < retries) {
        await new Promise((resolve) => setTimeout(resolve, delayMs));
      }
    }
  }
  throw lastError;
}
```
</details>

### Exercise 2: Add Cancellation and Exponential Backoff
**Specification**: Enhance the helper to double the delay on each attempt, incorporate randomized full jitter, and accept an `AbortSignal`.

<details>
<summary>Solution</summary>

```javascript
export async function retryWithBackoffAndSignal(fn, maxRetries = 3, baseDelay = 500, signal = null) {
  let attempt = 0;

  while (true) {
    if (signal?.aborted) {
      throw signal.reason ?? new Error('Aborted');
    }

    try {
      return await fn();
    } catch (err) {
      attempt++;
      if (attempt > maxRetries) throw err;

      const maxWindow = baseDelay * Math.pow(2, attempt - 1);
      const delay = Math.floor(Math.random() * maxWindow);

      await new Promise((resolve, reject) => {
        const timer = setTimeout(resolve, delay);
        if (signal) {
          signal.addEventListener(
            'abort',
            () => {
              clearTimeout(timer);
              reject(signal.reason ?? new Error('Aborted'));
            },
            { once: true }
          );
        }
      });
    }
  }
}
```
</details>
