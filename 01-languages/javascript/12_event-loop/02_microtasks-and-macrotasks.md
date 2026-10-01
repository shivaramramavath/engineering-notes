# Microtasks and Macrotasks

JavaScript runtimes keep **two kinds of job queues**. Which one a callback lands in decides when it runs.

| | Macrotask (task) | Microtask |
|---|------------------|-----------|
| Examples | `setTimeout`, `setInterval`, I/O callbacks, DOM events, `postMessage`, `setImmediate` | promise `then`/`catch`/`finally`, `await` continuations, `queueMicrotask`, `MutationObserver` |
| When processed | **one per loop turn** | **all of them**, right after the current task / callback |
| Rendering between jobs | possible between tasks | **never** between microtasks |
| Can starve the loop? | not alone | **yes**, if microtasks keep scheduling microtasks |

## The draining rule

After the call stack empties, the engine runs microtasks **until the queue is completely empty**, including microtasks added during that run. Only then does it move on (to rendering or the next macrotask).

```js
setTimeout(() => console.log("macrotask"), 0);

Promise.resolve().then(() => {
  console.log("micro 1");
  Promise.resolve().then(() => console.log("micro 2 (queued by micro 1)"));
});
// micro 1, micro 2 (queued by micro 1), macrotask
```

## What creates microtasks

```js
Promise.resolve().then(fn);          // reaction job when the promise is/gets fulfilled
promise.catch(fn); promise.finally(fn);
await value;                         // continuation after the await is a microtask
queueMicrotask(fn);                  // explicit
new MutationObserver(cb);            // observed DOM mutations (browsers)
```

`new Promise(executor)` runs `executor` **synchronously**. Only the **reactions** are microtasks.

```js
console.log("a");
new Promise((resolve) => { console.log("b"); resolve(); }).then(() => console.log("d"));
console.log("c");
// a, b, c, d
```

## What creates macrotasks

| Source | Notes |
|--------|-------|
| `setTimeout`, `setInterval` | timers queue |
| UI events (click, input, keydown) | each event dispatch is a task |
| `fetch`/XHR completion, `message` events | networking, `postMessage`, `MessageChannel` |
| `requestAnimationFrame` | not a task: runs in the rendering step |
| Node: I/O callbacks, `setImmediate`, close events | libuv phases |

## `queueMicrotask`

```js
queueMicrotask(() => console.log("soon, but after the current sync code"));
```

Use it to defer work to **just after the current operation**, without yielding to rendering or other tasks. Errors inside are reported like uncaught exceptions.

## Node.js extras: `process.nextTick`

```js
process.nextTick(() => console.log("tick"));
Promise.resolve().then(() => console.log("promise"));
queueMicrotask(() => console.log("microtask"));
// CommonJS: tick, promise, microtask
```

- The **nextTick queue is drained before the promise microtask queue**, after each callback
- In an **ES module**, the module's top-level code itself runs inside a promise job, so promise microtasks run **before** `nextTick`: `promise, microtask, tick`
- Prefer `queueMicrotask` (portable) over `process.nextTick` in new code; recursive `nextTick` can starve I/O

## Starvation

Because microtasks drain completely, an endless chain of microtasks **blocks the event loop**: timers, I/O and rendering never run.

```js
function forever() { queueMicrotask(forever); }
forever();                    // the page or process hangs; setTimeout callbacks never fire

function foreverTimer() { setTimeout(foreverTimer, 0); }
foreverTimer();               // keeps running, but the loop can still render and handle input
```

Same with promise recursion:

```js
const loop = () => Promise.resolve().then(loop);
loop();                       // starves everything else
```

Macrotask recursion yields between iterations; microtask recursion does not.

## Why two queues?

Microtasks give promise code **predictable, immediate follow-up** with consistent state: a promise reaction runs before anything else observable happens (no rendering, no other task). That makes patterns like "update state, then react" atomic from the perspective of the rest of the app.

## Microtask timing relative to the DOM and events

```js
button.addEventListener("click", () => {
  Promise.resolve().then(() => console.log("microtask after handler"));
  console.log("handler");
});
// click by a user: handler, microtask after handler (runs when the handler returns)

button.click();  // synthetic: the stack is NOT empty between listeners, so microtasks wait
```

When a **user** clicks, the browser runs each listener as a separate callback and drains microtasks between listeners. With `el.click()` / `dispatchEvent` from script, listeners run on the same stack, so microtasks run only after the whole dispatch finishes.

## Rendering and microtasks

```js
element.textContent = "A";
queueMicrotask(() => { element.textContent = "B"; });
// the browser never paints "A": the microtask runs before rendering

element.textContent = "A";
setTimeout(() => { element.textContent = "B"; }, 0);
// may paint "A" first (a frame can occur between the tasks), not guaranteed
```

## Choosing the right scheduling tool

| Need | Use |
|------|-----|
| Run right after the current code, before rendering or I/O | `queueMicrotask` / `Promise.resolve().then` |
| Run after other pending work and allow rendering | `setTimeout(fn, 0)` / `MessageChannel` / `scheduler.postTask` |
| Run before the next paint | `requestAnimationFrame` |
| Run when idle | `requestIdleCallback` (feature-detect) |
| Node: after I/O callbacks of this loop turn | `setImmediate` |
| Node: before anything else after this operation | `process.nextTick` (use sparingly) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Infinite microtask loops | Total freeze | Use a macrotask to yield |
| Heavy work inside `.then` callbacks | Blocks rendering and timers | Chunk work, yield with a macrotask |
| Assuming `setTimeout(fn, 0)` runs before promises | Promises always first | Know the order |
| Using `process.nextTick` recursively | Starves I/O | `setImmediate` / `queueMicrotask` |
| Expecting promise executors to be async | They run synchronously | Put work in reactions |
| Relying on microtask timing across ESM vs CJS in Node | `nextTick` vs promise order differs | Avoid depending on it |
| Throwing in `queueMicrotask` callbacks | Becomes an uncaught exception | Catch inside |

## Key takeaways

- Microtasks (promises, `queueMicrotask`) drain completely after each task; macrotasks run one per turn
- Promise executors are synchronous; reactions and `await` continuations are microtasks
- Microtask recursion starves the loop; macrotask recursion does not
- In Node, `nextTick` drains before promise microtasks (except at the start of an ES module)

**Next:** [Timers](./03_timers.md)
