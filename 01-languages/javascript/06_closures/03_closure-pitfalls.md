# Closure Pitfalls

Closures are powerful but cause a predictable set of bugs: shared loop variables, stale values, and memory retention.

## 1. The `var` loop trap

```js
for (var i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 0);
}
// 3 3 3
```

All three callbacks close over the **same** `i`. By the time they run, the loop has finished and `i` is `3`.

### Fixes

```js
// 1. let (a fresh binding per iteration)
for (let i = 0; i < 3; i++) setTimeout(() => console.log(i), 0);   // 0 1 2

// 2. IIFE that copies the value (legacy)
for (var i = 0; i < 3; i++) {
  ((n) => setTimeout(() => console.log(n), 0))(i);
}

// 3. Pass the value as an argument
for (var i = 0; i < 3; i++) setTimeout(console.log, 0, i);

// 4. Iterate with forEach / for...of
[0, 1, 2].forEach((i) => setTimeout(() => console.log(i), 0));
```

## 2. Stale closures

A closure keeps the value it saw **when the variable was last reassigned**, but a callback that captured an **old copy** never sees new data. Common in React hooks and long-lived listeners.

```js
function setup() {
  let count = 0;
  const snapshot = count;                     // copied once

  return {
    inc: () => count++,
    logLive: () => console.log(count),        // sees updates
    logStale: () => console.log(snapshot),    // always 0
  };
}
```

```js
// React-style stale state
function Counter() {
  const [count, setCount] = useState(0);
  useEffect(() => {
    const id = setInterval(() => console.log(count), 1000);   // count is always 0
    return () => clearInterval(id);
  }, []);                                                     // missing dependency
}
// Fix: add count to deps, use a ref, or a functional update: setCount(c => c + 1)
```

Rule: a closure sees variables **from its creation render/call**. Recreate it when inputs change, or read from a mutable reference.

## 3. Unexpected shared state

```js
function makeHandlers() {
  let clicks = 0;                           // shared by both handlers
  return { a: () => ++clicks, b: () => ++clicks };
}
const h = makeHandlers();
h.a(); h.b();   // clicks is 2 (both share one variable)
```

Create separate closures (call the factory twice) when you need independent state.

## 4. Memory leaks

A closure keeps its **whole captured environment** reachable. Long-lived closures can pin big data.

```js
function attach() {
  const bigData = new Array(1_000_000).fill("x");

  document.addEventListener("click", () => {
    console.log(bigData.length);     // bigData retained as long as the listener exists
  });
}
attach();
```

### Common leak sources

| Source | Why |
|--------|-----|
| Event listeners never removed | The listener closure stays alive |
| `setInterval` never cleared | Callback and its scope stay alive forever |
| Global or module-level caches | Grow without bound |
| Closures stored in long-lived objects | Keep their environments alive |
| Detached DOM nodes referenced by handlers | Node and subtree retained |
| Promises that never settle | Callbacks retained |

### Fixes

```js
// Remove listeners
const handler = () => { ... };
el.addEventListener("click", handler);
el.removeEventListener("click", handler);

// AbortController for many listeners
const ctrl = new AbortController();
el.addEventListener("click", handler, { signal: ctrl.signal });
window.addEventListener("resize", onResize, { signal: ctrl.signal });
ctrl.abort();                               // removes both

// Clear timers
const id = setInterval(tick, 1000);
clearInterval(id);

// Drop references you no longer need
function process() {
  let data = loadHuge();
  const summary = summarize(data);
  data = null;                               // free before returning the closure
  return () => summary;
}

// Capture only what you need
const len = bigData.length;
el.addEventListener("click", () => console.log(len));   // bigData can be collected

// Weak references for caches keyed by objects
const cache = new WeakMap();
```

Engines only keep variables that some closure actually references, but if **any** closure in the scope references a variable, **all** closures from that scope keep it alive.

```js
function leaky() {
  const big = new Array(1e6).fill(0);
  const small = 1;
  const useBig = () => big.length;       // references big
  return () => small;                    // shares the environment: big may stay alive (engine dependent)
}
```

Details in `18_memory-and-garbage-collection/03_memory-leaks.md`.

## 5. Accidental capture of `this`

```js
class Widget {
  constructor() { this.data = new Array(1e6).fill(0); }

  start() {
    this.timer = setInterval(() => {
      this.tick();                       // arrow captures `this`, which holds data
    }, 1000);
  }
  stop() { clearInterval(this.timer); }  // forgetting this keeps Widget alive
}
```

## 6. Performance costs

| Cost | Detail |
|------|--------|
| Function per instance | Closures created in constructors/factories duplicate method code objects per instance |
| Allocation in hot loops | Creating closures inside tight loops adds GC pressure |
| Deopts | Very dynamic capture patterns can limit optimizations |

```js
// Costly: new closure for every element each render
items.map((item) => () => select(item.id));

// Cheaper: define once, pass data
const handleClick = (event) => select(event.currentTarget.dataset.id);
```

Measure before optimizing: closures are cheap enough for most code.

## 7. Confusing closures with `this`

Closures use **lexical** variable lookup, while `this` is **call-based** (except arrows).

```js
const obj = {
  name: "Ada",
  regular() { return function () { return this?.name; }; },   // this lost
  arrow()   { return () => this.name; },                       // this captured lexically
};
obj.regular()();   // undefined
obj.arrow()();     // "Ada"
```

## 8. Mutating captured variables from async code

```js
let total = 0;
await Promise.all(ids.map(async (id) => {
  const price = await fetchPrice(id);
  total += price;                        // fine in JS (single thread) but order is nondeterministic
}));
```

Reads-then-writes across an `await` can lose updates:

```js
let counter = 0;
async function bump() {
  const current = counter;               // read
  await delay(10);
  counter = current + 1;                 // write based on stale read
}
await Promise.all([bump(), bump()]);     // counter is 1, not 2
```

Fix: read and write together (`counter++` with no `await` in between) or serialize with a queue.

## 9. Debugging closure problems

| Symptom | Likely cause | Check |
|---------|--------------|-------|
| Callback sees final loop value | `var` in loop | Switch to `let` |
| Callback sees old data | Stale closure | Recreate callback or use a ref |
| Memory climbs over time | Listeners/timers/caches | DevTools Memory → heap snapshot, look at "Closure" retainers |
| Handler fires after teardown | Missing cleanup | Track and remove listeners/timers |
| Two features interfere | Shared captured variable | Separate factory calls |

DevTools tips: the **Scope** panel lists closure variables; the heap snapshot **Retainers** view shows what keeps an object alive.

## Pitfall summary

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `var` in loops | Shared variable | `let` / `const` |
| Stale captured state | Old values used | Fresh closures, refs, functional updates |
| Never-removed listeners and timers | Leaks and ghost callbacks | `AbortController`, cleanup functions |
| Capturing large objects | Memory retained | Capture only needed primitives |
| Unbounded closure-held caches | Memory growth | LRU, `WeakMap`, size limits |
| Read-modify-write across `await` | Lost updates | Atomic updates, queues |
| Creating closures in hot loops | GC pressure | Hoist functions |

## Key takeaways

- Loops with `var` share one variable; use `let`
- Closures reference variables, so watch for stale and shared state
- Anything a live closure can reach cannot be garbage collected: clean up listeners, timers and caches
- Capture the smallest amount of data needed
- Diagnose with the DevTools Scope panel and heap snapshots

**Next:** [Functional Programming](../07_functional-programming/00_README.md)
