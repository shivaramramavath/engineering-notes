# Task Ordering

How to predict the exact order in which synchronous code, microtasks, timers and I/O callbacks run. This file is a practical **method plus worked puzzles**.

## The ordering recipe

1. Run all **synchronous** code of the current script top to bottom (including promise executors and the code before the first `await`)
2. Run **microtasks** until the queue is empty, in FIFO order (Node CommonJS: `process.nextTick` queue first)
3. Run the next **macrotask** (the earliest due timer, then others), then repeat step 2
4. Browsers may render between macrotasks

Priority at a glance:

```text
sync code  >  nextTick (Node CJS)  >  promise microtasks  >  timers / I/O / events  >  (browser: rAF before paint)
```

## Puzzle 1: the classic

```js
console.log("1");
setTimeout(() => console.log("2"), 0);
Promise.resolve().then(() => console.log("3"));
queueMicrotask(() => console.log("4"));
console.log("5");
```

**Output:** `1, 5, 3, 4, 2`

Sync: 1, 5. Microtasks in queue order: 3, 4. Then the timer: 2.

## Puzzle 2: microtasks between timers

```js
setTimeout(() => {
  console.log("t1");
  Promise.resolve().then(() => console.log("m1"));
}, 0);
setTimeout(() => console.log("t2"), 0);
```

**Output:** `t1, m1, t2`

Microtasks drain after **each** timer callback, before the next one.

## Puzzle 3: promise executors are synchronous

```js
console.log("a");
new Promise((resolve) => {
  console.log("b");
  resolve();
  console.log("c");
}).then(() => console.log("d"));
console.log("e");
```

**Output:** `a, b, c, e, d`

Everything inside the executor (even after `resolve()`) runs synchronously. Only `d` is a microtask.

## Puzzle 4: async/await

```js
async function a() {
  console.log("a1");
  await b();
  console.log("a2");
}
async function b() { console.log("b"); }

console.log("s");
a();
new Promise((resolve) => { console.log("p1"); resolve(); }).then(() => console.log("p2"));
console.log("e");
```

**Output:** `s, a1, b, p1, e, a2, p2`

`a()` runs synchronously until `await`; `b()` logs and returns a resolved promise, so the continuation (`a2`) is queued first. Then `p2` is queued. After the sync code ends, microtasks run in order: `a2`, `p2`.

## Puzzle 5: chained `then`s interleave

```js
Promise.resolve()
  .then(() => console.log("A1"))
  .then(() => console.log("A2"))
  .then(() => console.log("A3"));

Promise.resolve()
  .then(() => console.log("B1"))
  .then(() => console.log("B2"));
```

**Output:** `A1, B1, A2, B2, A3`

Each `then` handler is queued only after the previous one runs, so chains interleave one step at a time.

## Puzzle 6: returning a promise costs extra ticks

```js
Promise.resolve()
  .then(() => { console.log("A1"); return Promise.resolve("x"); })
  .then(() => console.log("A2"));

Promise.resolve()
  .then(() => console.log("B1"))
  .then(() => console.log("B2"))
  .then(() => console.log("B3"))
  .then(() => console.log("B4"));
```

**Output:** `A1, B1, B2, B3, A2, B4`

Returning a **promise** from a handler makes the outer promise wait for it, taking about **2 extra microtask turns** versus returning a plain value. This is why mixing promise styles can surprise you. Avoid relying on exact tick counts.

## Puzzle 7: `await` on a non-promise vs promise

```js
async function main() {
  console.log("1");
  await undefined;
  console.log("3");
}
main();
console.log("2");
```

**Output:** `1, 2, 3`

`await` always yields: even `await undefined` defers the rest to a microtask.

## Puzzle 8: timers with different delays

```js
setTimeout(() => console.log("A"), 100);
setTimeout(() => console.log("B"), 0);
setTimeout(() => console.log("C"), 50);
Promise.resolve().then(() => console.log("D"));
```

**Output:** `D, B, C, A`

## Puzzle 9: a busy thread delays everything

```js
const start = Date.now();
setTimeout(() => console.log("timeout", Date.now() - start >= 500), 0);
while (Date.now() - start < 500) {}     // block 500 ms
console.log("loop done");
// loop done, timeout true
```

## Puzzle 10: event handlers and microtasks

```html
<button id="b">click</button>
<script>
b.addEventListener("click", () => {
  Promise.resolve().then(() => console.log("micro 1"));
  console.log("listener 1");
});
b.addEventListener("click", () => console.log("listener 2"));
</script>
```

| Trigger | Output |
|---------|--------|
| User click | `listener 1, micro 1, listener 2` (stack empties between listeners) |
| `b.click()` from script | `listener 1, listener 2, micro 1` (one continuous stack) |

## Puzzle 11: Node ordering

```js
// CommonJS file
setTimeout(() => console.log("timeout"), 0);
setImmediate(() => console.log("immediate"));
process.nextTick(() => console.log("nextTick"));
Promise.resolve().then(() => console.log("promise"));
queueMicrotask(() => console.log("microtask"));
console.log("sync");
```

**Output (CommonJS):** `sync, nextTick, promise, microtask, timeout, immediate` (the last two may swap in the main module)

In an **ES module**: `sync, promise, microtask, nextTick, timeout/immediate` (top-level module code runs inside a promise job).

```js
// Inside an I/O callback the order is deterministic
fs.readFile(__filename, () => {
  setTimeout(() => console.log("timeout"), 0);
  setImmediate(() => console.log("immediate"));
  process.nextTick(() => console.log("nextTick"));
  Promise.resolve().then(() => console.log("promise"));
});
// nextTick, promise, immediate, timeout
```

## Puzzle 12: loops and closures with timers

```js
for (var i = 0; i < 3; i++) setTimeout(() => console.log(i), 0);   // 3, 3, 3
for (let j = 0; j < 3; j++) setTimeout(() => console.log(j), 0);   // 0, 1, 2
```

Timers run after the loop; `var` shares one binding, `let` creates one per iteration.

## Puzzle 13: async iteration

```js
async function* gen() { yield 1; yield 2; }
(async () => {
  for await (const x of gen()) console.log("item", x);
  console.log("done");
})();
console.log("sync");
// sync, item 1, item 2, done
```

## Predicting without running: a checklist

1. Mark every **synchronous** line, including promise executors and pre-`await` code
2. List `.then`/`await` continuations in **the order they become ready**, as a FIFO microtask queue
3. For each microtask that queues another, append it to the **end**
4. After the queue empties, pick the next **timer/I/O/event**
5. Repeat from step 2 after each macrotask

## Debugging ordering issues

```js
const log = (...a) => console.log(performance.now().toFixed(2), ...a);   // timestamps help
console.trace("who called me?");
```

In Chrome DevTools, the **Performance** panel shows tasks and microtask checkpoints; async stack traces show which `await` led to a continuation.

## Rules of thumb

| Statement | True? |
|-----------|-------|
| Promise reactions run before any `setTimeout` callback | Yes (if queued in the same turn) |
| `setTimeout(fn, 0)` runs immediately | No: after sync code, microtasks, and clamping |
| Code after `await` runs synchronously | No: always a microtask |
| `then` handlers can run synchronously if already resolved | No: always async |
| Two timers with the same delay run in creation order | Yes |
| Exact tick counts for promise chains are stable | No, avoid depending on them |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Depending on exact microtask tick counts | Fragile across engines and refactors | Use explicit sequencing (`await`) |
| Relying on `setTimeout(0)` vs `setImmediate` order in Node's main module | Nondeterministic | Don't depend on it |
| Assuming user and synthetic events behave the same | Different microtask timing | Test with real events |
| Using timers to "wait for" other code | Race conditions | Await promises or events |
| Treating `setTimeout(fn, 0)` as "after rendering" | Not guaranteed | `requestAnimationFrame` (+ timeout) for post-paint work |

## Key takeaways

- Order = sync code, then microtasks (FIFO, until empty), then one macrotask, repeat
- `await` and `then` continuations are always asynchronous microtasks
- Chains interleave step by step; returning a promise adds extra ticks
- In Node, `nextTick` runs before promise microtasks (CommonJS), and I/O callbacks make `setImmediate` beat timers

**Next:** [Event Loop Patterns](./05_event-loop-patterns.md)
