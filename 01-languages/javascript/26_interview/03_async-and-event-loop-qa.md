# JavaScript Interview Mastery: Asynchronous Architecture & Event Loop Internals

> A rigorous technical breakdown of the JavaScript concurrency model, comparing browser macro/microtask turn cycles against Node.js libuv loop phases, `process.nextTick`, and `async/await` state machines.

| Metadata | Details |
| :--- | :--- |
| **Track** | JavaScript Interview Preparation (`26_interview`) |
| **Target Role** | Senior / Staff Frontend & Full-Stack Engineer |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Prerequisites** | [11_asynchronous-javascript](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript), [12_event-loop](file:///e:/Learning/engineering-notes/01-languages/javascript/12_event-loop) |

---

## Table of Contents
1. [Core Concepts & Theoretical Framework](#1-core-concepts--theoretical-framework)
2. [Deep-Dive Technical Questions & Answers](#2-deep-dive-technical-questions--answers)
   - [Q1: What is the exact execution priority order between Microtasks and Macrotasks in the browser?](#q1-what-is-the-exact-execution-priority-order-between-microtasks-and-macrotasks-in-the-browser)
   - [Q2: Detail the 6 phases of the Node.js libuv Event Loop and where `setImmediate` executes.](#q2-detail-the-6-phases-of-the-nodejs-libuv-event-loop-and-where-setimmediate-executes)
   - [Q3: What is the difference between `process.nextTick()` and `queueMicrotask()` in Node.js?](#q3-what-is-the-difference-between-processnexttick-and-queuemicrotask-in-nodejs)
   - [Q4: How does the V8 engine desugar `async/await` into Promises and Generator state machines?](#q4-how-does-the-v8-engine-desugar-asyncawait-into-promises-and-generator-state-machines)
   - [Q5: What happens if a microtask recursively schedules another microtask?](#q5-what-happens-if-a-microtask-recursively-schedules-another-microtask)
   - [Q6: How do `Promise.all`, `Promise.allSettled`, `Promise.race`, and `Promise.any` differ in short-circuiting behavior?](#q6-how-do-promiseall-promiseallsettled-promiserace-and-promiseany-differ-in-short-circuiting-behavior)
   - [Q7: Why does `setTimeout(fn, 0)` not execute in 0 milliseconds?](#q7-why-does-settimeoutfn-0-not-execute-in-0-milliseconds)
   - [Q8: How do you gracefully cancel in-flight Promises and Fetch requests using `AbortController`?](#q8-how-do-you-gracefully-cancel-in-flight-promises-and-fetch-requests-using-abortcontroller)
3. [Tricky Output & Code Prediction Drills](#3-tricky-output--code-prediction-drills)
4. [Staff-Level Architecture & Discussion Prompts](#4-staff-level-architecture--discussion-prompts)

---

## 1. Core Concepts & Theoretical Framework

JavaScript is single-threaded in its execution context, but concurrent in its I/O via runtime host environments (browsers and Node.js libuv).

```
                     BROWSER EVENT LOOP TURN
                                │
                                ▼
                     ┌─────────────────────┐
                     │ Call Stack Cleared  │
                     └──────────┬──────────┘
                                │
                                ▼
                     ┌─────────────────────┐
                     │ Drain Microtask Q   │ ◄── [Promises, MutationObserver,
                     │ Until Completely 0  │      queueMicrotask]
                     └──────────┬──────────┘
                                │
                                ▼
                     ┌─────────────────────┐
                     │ Render Pipeline     │ ◄── [requestAnimationFrame,
                     │ (If 60Hz tick due)  │      Style Recalc, Layout, Paint]
                     └──────────┬──────────┘
                                │
                                ▼
                     ┌─────────────────────┐
                     │ Exactly ONE         │ ◄── [setTimeout, setInterval,
                     │ Macrotask Executed  │      MessageChannel, I/O]
                     └─────────────────────┘
```

---

## 2. Deep-Dive Technical Questions & Answers

### Q1: What is the exact execution priority order between Microtasks and Macrotasks in the browser?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

1. **Current Call Stack**: Executes the active synchronous script until empty.
2. **Microtask Queue Drain**: The engine drains the **entire** microtask queue (`Promise.then/catch/finally`, `queueMicrotask`, `MutationObserver`). If a microtask enqueues another microtask, it is executed in the *same* drain cycle before moving on.
3. **Render Opportunities**: If a display refresh frame is due (typically every 16.6ms for 60Hz):
   - Execute `requestAnimationFrame` callbacks.
   - Run Style Calculation, Layout (Reflow), and Paint.
4. **Macrotask Execution**: Pick and execute **exactly one** task from the Task Queue (`setTimeout`, `setInterval`, UI clicks, network callbacks).
5. Repeat step 2 (drain microtasks immediately after that single macrotask finishes).
</details>

---

### Q2: Detail the 6 phases of the Node.js libuv Event Loop and where `setImmediate` executes.

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Node.js executes via the `libuv` event loop consisting of 6 ordered phases:

1. **Timers Phase**: Executes callbacks scheduled by expired `setTimeout()` and `setInterval()`.
2. **Pending Callbacks Phase**: Executes I/O callbacks deferred to the next loop iteration (e.g. system errors like `ECONNREFUSED`).
3. **Idle, Prepare Phase**: Internal libuv housekeeping.
4. **Poll Phase**: Retrieves new I/O events (incoming network connections, disk read completions); blocks and waits if the queue is empty unless scripts were scheduled with `setImmediate`.
5. **Check Phase**: Executes callbacks scheduled specifically via `setImmediate()`.
6. **Close Callbacks Phase**: Executes close handlers (e.g. `socket.on('close', ...)`).

*Key distinction:* `process.nextTick` and microtasks do **not** belong to any libuv phase; they are drained immediately after the current operation finishes, before transitioning between any phases!
</details>

---

### Q3: What is the difference between `process.nextTick()` and `queueMicrotask()` in Node.js?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In Node.js, `process.nextTick` operates on a dedicated priority queue that **preempts** standard ECMAScript microtasks:
1. `nextTickQueue`: Processed immediately after the current synchronous turn finishes.
2. `microtaskQueue` (`Promise.resolve()`, `queueMicrotask()`): Drained strictly **after** the `nextTickQueue` is completely emptied.

```javascript
queueMicrotask(() => console.log('1. microtask'));
process.nextTick(() => console.log('2. nextTick'));

// Output:
// 2. nextTick
// 1. microtask
```
</details>

---

### Q4: How does the V8 engine desugar `async/await` into Promises and Generator state machines?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Under the hood, an `async` function is compiled into a Generator coroutine wrapped in a promise runner:

```javascript
// Author code:
async function fetchUser(id) {
  const res = await api(id);
  return res.data;
}

// Conceptual desugared representation:
function fetchUser(id) {
  return new Promise((resolve, reject) => {
    const gen = (function* () {
      const res = yield api(id);
      return res.data;
    })();

    function step(verb, arg) {
      try {
        const { value, done } = gen[verb](arg);
        if (done) return resolve(value);
        Promise.resolve(value).then(
          (val) => step('next', val),
          (err) => step('throw', err)
        );
      } catch (err) {
        reject(err);
      }
    }

    step('next');
  });
}
```
</details>

---

### Q5: What happens if a microtask recursively schedules another microtask?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Because the event loop must completely empty the Microtask Queue before yielding to the rendering pipeline or macrotask queue:
- **Event Loop Starvation**: An infinite microtask loop completely starves the event loop.
- **In Browsers**: The UI freezes completely; user clicks, scrolling, animations (`requestAnimationFrame`), and `setTimeout` timers are completely blocked until the browser displays an "Unresponsive Page" alert.
- **In Node.js**: I/O polling and timer phases are starved, blocking network throughput.
</details>

---

### Q6: How do `Promise.all`, `Promise.allSettled`, `Promise.race`, and `Promise.any` differ in short-circuiting behavior?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

| Method | Resolution Trigger | Rejection / Short-Circuit Trigger | Output on Resolution |
| :--- | :--- | :--- | :--- |
| **`Promise.all`** | All promises resolve successfully | **First** promise that rejects | Array of values in input order |
| **`Promise.allSettled`** | **Never short-circuits**; waits for all | Never rejects (always resolves) | Array of `{ status, value/reason }` |
| **`Promise.race`** | **First** promise that settles (resolves or rejects) | **First** promise that settles (resolves or rejects) | Value or reason of fastest promise |
| **`Promise.any`** | **First** promise that resolves | Rejects only if **all** reject (`AggregateError`) | Value of fastest fulfilled promise |
</details>

---

### Q7: Why does `setTimeout(fn, 0)` not execute in 0 milliseconds?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

1. **HTML5 Spec Clamping**: The HTML Living Standard mandates that if a timer is nested 5 or more levels deep, the browser clamps the minimum delay to **4 milliseconds**.
2. **OS Scheduling Jitter**: Node.js and operating system timer interrupts (Windows thread tick is typically 1ms–15.6ms; Linux is 1ms) mean the timer resolution cannot guarantee sub-millisecond precision.
3. **Queue Wait Time**: Even if the timer expires in 0ms, it is merely enqueued in the Macrotask Queue. It cannot run until the current synchronous code and all microtasks finish.
</details>

---

### Q8: How do you gracefully cancel in-flight Promises and Fetch requests using `AbortController`?

<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

JavaScript Promises themselves cannot be cancelled once constructed. Cancellation is achieved using `AbortController` and its `signal` (`AbortSignal`):

```javascript
const controller = new AbortController();
const timeout = setTimeout(() => controller.abort(), 2000); // 2s timeout

try {
  const res = await fetch('https://api.example.com/data', {
    signal: controller.signal
  });
  const data = await res.json();
} catch (err) {
  if (err.name === 'AbortError') {
    console.warn('Request was aborted due to timeout.');
  } else {
    throw err;
  }
} finally {
  clearTimeout(timeout);
}
```
</details>

---

## 3. Tricky Output & Code Prediction Drills

### Drill 1: Comprehensive Event Loop Sequencing
```javascript
console.log('1');

setTimeout(() => {
  console.log('2');
  Promise.resolve().then(() => console.log('3'));
}, 0);

new Promise((resolve) => {
  console.log('4');
  resolve();
}).then(() => {
  console.log('5');
});

queueMicrotask(() => {
  console.log('6');
});

console.log('7');
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
1
4
7
5
6
2
3
```
**Explanation:**
1. Synchronous execution: prints `1`.
2. `setTimeout` scheduled in Macrotask queue.
3. `new Promise` executor runs *synchronously*: prints `4`. Its `.then()` is queued in Microtask queue.
4. `queueMicrotask` queued in Microtask queue.
5. Synchronous `7` prints.
6. Drain Microtask queue: prints `5`, then `6`.
7. Turn ends. Pick first Macrotask: prints `2`.
8. That timer enqueues a microtask (`3`), which drains before the next turn: prints `3`.
</details>

### Drill 2: Async/Await Microtask Timing
```javascript
async function async1() {
  console.log('async1 start');
  await async2();
  console.log('async1 end');
}

async function async2() {
  console.log('async2');
}

console.log('script start');
async1();
console.log('script end');
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
script start
async1 start
async2
script end
async1 end
```
**Explanation:**
`await async2()` executes `async2()` synchronously up to the return. The code following `await` is suspended and enqueued as a microtask. Synchronous `script end` completes, and the microtask resumes `async1 end`.
</details>

---

## 4. Staff-Level Architecture & Discussion Prompts

1. *How would you implement an asynchronous request pipeline with deduplication (request collapsing), token bucket rate limiting, and exponential retry backoff without introducing memory leaks?*
2. *In high-frequency Node.js microservices, how does excessive usage of `process.nextTick` or large microtask chains starve the libuv I/O polling phase, and how do you monitor event loop lag using `perf_hooks` (`monitorEventLoopDelay`)?*
