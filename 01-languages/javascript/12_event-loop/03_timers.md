# Timers

Timers schedule a callback to run **later** as a macrotask. They are the oldest async tool and one of the most misunderstood.

## The API

| Function | Behavior |
|----------|----------|
| `setTimeout(fn, ms, ...args)` | run once after at least `ms` |
| `clearTimeout(id)` | cancel a pending timeout |
| `setInterval(fn, ms, ...args)` | run repeatedly about every `ms` |
| `clearInterval(id)` | stop an interval |
| `setImmediate(fn)` (Node) | run in the **check** phase, right after the poll phase |
| `requestAnimationFrame(fn)` (browser) | run before the next paint |
| `requestIdleCallback(fn, { timeout })` (browser) | run when idle |

```js
const id = setTimeout((name) => console.log(`hello ${name}`), 1000, "Ada");
clearTimeout(id);
```

Return values: browsers return a **number**; Node returns a **Timeout object** (with `ref()`, `unref()`, `refresh()`, `hasRef()`, and `[Symbol.toPrimitive]`).

## `ms` is a minimum, not a promise

```js
const t0 = performance.now();
setTimeout(() => console.log(performance.now() - t0), 100);
// 100.3 normally; much more if the thread is busy
```

The callback runs only when:

1. the delay has elapsed, **and**
2. the call stack is empty, **and**
3. the loop reaches the timers queue (earlier tasks and microtasks run first)

```js
setTimeout(() => console.log("timer"), 0);
const end = Date.now() + 1000;
while (Date.now() < end) {}          // blocks: "timer" fires only after ~1 s
```

## Delay clamping

| Rule | Detail |
|------|--------|
| `0` or missing delay | treated as `0`, but browsers clamp to ≥ 1 ms in practice |
| Nesting level > 5 (browsers) | clamped to ≥ **4 ms** per hop |
| Node | minimum 1 ms (`setTimeout(fn, 0)` is `1`) |
| Delay > 2³¹ − 1 ms (~24.8 days) | overflows: fires after 1 ms (Node warns) |
| Hidden / background tab (browsers) | throttled to ≥ 1000 ms; "intensive throttling" can run timers only once per minute |
| Negative or `NaN` | treated as `0` |

For "next tick-ish" work with no clamping, use `queueMicrotask`, `MessageChannel`, or `scheduler.postTask`.

```js
const { port1, port2 } = new MessageChannel();
port1.onmessage = () => console.log("macrotask without timer clamping");
port2.postMessage(null);
```

## Timer ordering

Timers with the same delay fire in creation order. Different delays fire in due-time order.

```js
setTimeout(() => console.log("A"), 10);
setTimeout(() => console.log("B"), 0);
setTimeout(() => console.log("C"), 0);
// B, C, A
```

Between timer callbacks, microtasks drain:

```js
setTimeout(() => { console.log("t1"); Promise.resolve().then(() => console.log("m1")); }, 0);
setTimeout(() => console.log("t2"), 0);
// t1, m1, t2
```

## `setInterval` problems

```js
setInterval(async () => {
  await slowRequest();       // may take longer than the interval
}, 1000);
```

- Intervals **do not wait** for async work: calls can overlap
- Missed ticks are not queued up forever: browsers drop or coalesce when delayed
- Drift accumulates because each delay starts relative to scheduling

Prefer a self-scheduling loop:

```js
async function loop(signal) {
  while (!signal.aborted) {
    const started = performance.now();
    try { await tick(); } catch (e) { report(e); }
    const elapsed = performance.now() - started;
    await sleep(Math.max(0, 1000 - elapsed), { signal });   // keeps a steady cadence without overlap
  }
}
```

```js
// recursive setTimeout variant
function schedule() {
  setTimeout(async () => { await work(); schedule(); }, 1000);
}
```

## Timekeeping: use timestamps

Timers drift and are throttled; **compute elapsed time from a clock**.

```js
const start = Date.now();
const id = setInterval(() => {
  const seconds = Math.floor((Date.now() - start) / 1000);   // correct even if ticks are late
  render(seconds);
}, 250);
```

| Clock | Use |
|-------|-----|
| `performance.now()` | durations and benchmarks (monotonic, high resolution) |
| `Date.now()` | wall-clock timestamps (can jump when the clock changes) |
| `process.hrtime.bigint()` (Node) | nanosecond monotonic time |

## `clearTimeout` details

```js
const id = setTimeout(fn, 1000);
clearTimeout(id);                    // safe even if already fired or undefined
clearTimeout(undefined);             // no-op
```

Always clear timers in cleanup paths (component unmount, request end, shutdown) to avoid leaks and callbacks firing against dead state.

## Node-specific behavior

```js
const t = setInterval(poll, 1000);
t.unref();                           // do not keep the process alive just for this timer
t.ref();                             // opposite
t.refresh();                         // restart the timer with the same duration
```

An active ref'd timer prevents Node from exiting; use `unref()` for housekeeping timers.

```js
import { setTimeout as sleep, setInterval as every } from "node:timers/promises";
await sleep(500);
for await (const _ of every(1000, undefined, { signal })) { await tick(); }   // async iterator interval
```

### `setImmediate` vs `setTimeout(0)` vs `process.nextTick`

| | Runs | Notes |
|---|------|-------|
| `process.nextTick` | before the loop continues, before promise microtasks | can starve I/O |
| `Promise.then` / `queueMicrotask` | after the current operation | microtask queue |
| `setTimeout(fn, 0)` | timers phase of the next iteration (≥ 1 ms) | order vs `setImmediate` varies in the main module |
| `setImmediate` | check phase, after poll | deterministic after an I/O callback |

## Browser-only: animation and idle

```js
// Smooth animation: sync to the display, pause in background tabs
function frame(timestamp) {
  update(timestamp);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

// Low-priority work when idle (feature-detect: not in Safari)
requestIdleCallback((deadline) => {
  while (deadline.timeRemaining() > 0 && queue.length) doWork(queue.shift());
  if (queue.length) requestIdleCallback(arguments.callee);
}, { timeout: 2000 });

// Prioritized scheduling (newer browsers; check support)
await scheduler.postTask(() => work(), { priority: "background" });
await scheduler.yield?.();       // yield to the event loop, then continue with priority
```

Do not use `setTimeout`-based animation loops; they do not align with frames.

## Debounce and throttle built on timers

```js
function debounce(fn, ms) {
  let id;
  return (...args) => { clearTimeout(id); id = setTimeout(() => fn(...args), ms); };
}

function throttle(fn, ms) {
  let last = 0, id;
  return (...args) => {
    const now = Date.now();
    const remaining = ms - (now - last);
    if (remaining <= 0) { last = now; fn(...args); }
    else if (!id) id = setTimeout(() => { last = Date.now(); id = null; fn(...args); }, remaining);
  };
}
```

## Testing with timers

```js
import { vi, test, expect } from "vitest";

test("debounce", () => {
  vi.useFakeTimers();
  const fn = vi.fn();
  const d = debounce(fn, 100);
  d(); d(); d();
  vi.advanceTimersByTime(100);
  expect(fn).toHaveBeenCalledTimes(1);
  vi.useRealTimers();
});
```

Fake timers (Vitest, Jest, Sinon) make timer logic deterministic and fast.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Treating delay as exact | Late callbacks | Timestamps for elapsed time |
| `setInterval` with async work | Overlap, drift | Self-scheduling loop with `await` |
| Not clearing timers | Leaks, zombie callbacks, process never exits | `clearTimeout`, `unref()` |
| `setTimeout` in loops with `var` | Shared variable | `let`, or pass args |
| Passing a string to `setTimeout` | Acts like `eval`, blocked by CSP | Pass a function |
| `setTimeout(fn(), ms)` | Calls `fn` immediately, schedules its return value | `setTimeout(fn, ms)` or `() => fn()` |
| Passing a method without binding (`setTimeout(obj.method, 0)`) | `this` lost | Arrow wrapper / `bind` |
| Animation with timers | Janky, wastes work in hidden tabs | `requestAnimationFrame` |
| Delays above 2³¹ − 1 ms | Fires immediately | Chain timers, or schedule with a date check |
| Assuming ordering between `setTimeout(0)` and `setImmediate` in the main module | Nondeterministic | Don't rely on it |

## Key takeaways

- Timers are macrotasks with a **minimum** delay, affected by clamping, throttling and a busy stack
- Prefer self-scheduling loops over `setInterval` for async work
- Measure time with `performance.now()`/`Date.now()` instead of counting ticks
- Clear timers during cleanup; use `requestAnimationFrame` for visuals and `queueMicrotask` for "right after this"

**Next:** [Task Ordering](./04_task-ordering.md)
