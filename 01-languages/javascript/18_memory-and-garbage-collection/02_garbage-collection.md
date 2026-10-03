# Garbage Collection

**Garbage collection (GC)** automatically reclaims memory held by objects the program can no longer reach. You never call `free`. Instead, the engine periodically finds unreachable objects and releases them. Understanding the rules tells you what the GC can and cannot do for you.

## Reachability

An object is **alive** if it can be reached from a **root** by following references. Roots include:

- Global object and global variables
- The current call stack: local variables and parameters of active functions
- Variables captured by reachable closures
- Pending timers, event listeners, and promise callbacks that reference objects
- Engine internals (handles held by native code)

Everything else is **garbage**.

```js
let user = { name: 'Ada' };     // reachable via `user`
user = null;                    // no references remain → collectable
```

```js
let a = { name: 'A' };
let b = { name: 'B', friend: a };
a = null;                       // the object is STILL reachable via b.friend
b = null;                       // now both are unreachable → collectable
```

The GC does not know whether you will use an object again; it only knows whether you **could**. An object that is reachable but never used again is a **leak** (see [Memory Leaks](./03_memory-leaks.md)).

## Reference counting (and why JS does not use it)

Reference counting frees an object when its count hits zero. It fails on cycles:

```js
function cycle() {
  const a = {};
  const b = { a };
  a.b = b;               // a ↔ b reference each other: counts never reach zero
}
cycle();                 // the objects are unreachable after return
```

Modern engines use **tracing** collectors (reachability from roots), which collect cycles correctly. Old browsers (Internet Explorer 6/7) used reference counting for DOM objects and leaked cycles; that is why you still see advice to break cycles, which is unnecessary today.

## Mark and sweep

The classic tracing algorithm:

1. **Mark**: start from the roots, follow every reference, and mark each visited object as alive
2. **Sweep**: scan the heap and free every unmarked object
3. (**Compact**): optionally move survivors together to remove fragmentation

```
Before:  [A][B][C][D][E][F]      roots → A → C → E
Mark:     ✓     ✓     ✓          (B, D, F unmarked)
Sweep:   [A][ ][C][ ][E][ ]      B, D, F freed
Compact: [A][C][E][ ][ ][ ]      survivors moved together, references updated
```

## V8's collector

V8 (Chrome, Node.js, Deno) uses a **generational**, **incremental**, **concurrent** garbage collector, often called **Orinoco**. These ideas rest on the **generational hypothesis**: most objects die young.

### Generations

| Space | Holds | Collected by | Frequency | Cost |
|-------|-------|--------------|-----------|------|
| **Young generation** (new space, a few MB to tens of MB) | Newly allocated objects | **Scavenger** (minor GC) | Very often | Fast (ms) |
| **Old generation** | Objects that survived two scavenges | **Mark-Sweep-Compact** (major GC) | Rare | Slower; mostly concurrent |
| Large object space | Very large objects | Major GC | Rare | Not moved |
| Code space | Compiled code | Major GC | Rare | |

```
new allocations ──▶ [ young: nursery → intermediate ] ──survive twice──▶ [ old generation ]
                       minor GC (scavenge): copy survivors,                major GC: mark, sweep,
                       free the rest instantly                              compact when needed
```

### Scavenge (minor GC)

The young generation is split into two halves (semi-spaces). A scavenge copies the **live** objects to the other half and discards everything left behind. Since most young objects are dead, little is copied, so it is fast. Objects that survive two scavenges are **promoted** to the old generation.

### Major GC

Marks live objects in the old generation, sweeps dead ones, and sometimes compacts. To avoid long pauses V8 uses:

| Technique | Idea |
|-----------|------|
| **Incremental marking** | Marking is split into small steps interleaved with JavaScript |
| **Concurrent marking** | Helper threads mark while JavaScript keeps running |
| **Parallel scavenging / compaction** | Several threads share the work during a pause |
| **Lazy / concurrent sweeping** | Freed memory is reclaimed in the background |
| **Idle-time GC** | Browsers run GC work when the page is idle |
| **Write barriers** | Track pointer changes made by the program while marking runs |

A "stop-the-world" pause still happens for parts of GC, but typically in the low-millisecond range for the young generation and short for the old.

## What triggers GC

- The young generation fills up (scavenge)
- The old generation grows past a threshold (major GC, which adapts to your heap size)
- External memory pressure (for example large `Buffer` or `ArrayBuffer` allocations)
- Low-memory notifications and idle time
- Explicit calls when exposed (`global.gc()` with `--expose-gc`): for testing only

You cannot trigger or control GC in normal code, and you should not try.

## Observing GC

### Node

```bash
node --trace-gc app.js                      # prints each GC event
node --trace-gc-verbose app.js
node --expose-gc app.js                     # exposes global.gc() for experiments
node --max-old-space-size=2048 app.js       # heap limit in MB
node --max-semi-space-size=64 app.js        # larger young generation (fewer minor GCs)
```

```js
import { PerformanceObserver } from 'node:perf_hooks';

const obs = new PerformanceObserver((list) => {
  for (const e of list.getEntries()) {
    console.log(`GC kind=${e.detail?.kind} duration=${e.duration.toFixed(2)}ms`);
  }
});
obs.observe({ entryTypes: ['gc'] });
```

```js
import v8 from 'node:v8';
console.log(v8.getHeapSpaceStatistics());   // per-space sizes: new_space, old_space, large_object_space, ...
```

### Browser

Use the DevTools **Performance** panel (look for GC events and the memory graph) and the **Memory** panel. In Chrome, `chrome://tracing` and the Task Manager show per-tab memory.

## Weak references

Normal references keep objects alive. **Weak** references let you refer to an object **without** preventing its collection.

### `WeakMap` and `WeakSet`

Keys (objects) are held weakly; when a key is otherwise unreachable, its entry disappears.

```js
const meta = new WeakMap();

function track(el) {
  meta.set(el, { clicks: 0 });      // metadata tied to el's lifetime
}

// when `el` is removed and unreferenced elsewhere, the entry is collected automatically
```

You cannot iterate or measure a `WeakMap`; that is deliberate, since contents depend on GC timing. See [WeakMap, WeakSet, WeakRef](../09_built-in-objects/07_weakmap-weakset-weakref.md).

### `WeakRef`

```js
let big = { data: new Array(1e6).fill(0) };
const ref = new WeakRef(big);

big = null;                          // only the weak ref remains

const maybe = ref.deref();           // the object, or undefined if it was collected
if (maybe) use(maybe);
```

Typical use: caches that may be dropped under memory pressure.

```js
class WeakCache {
  #map = new Map();                  // key → WeakRef(value)

  get(key) {
    const ref = this.#map.get(key);
    const value = ref?.deref();
    if (!value) this.#map.delete(key);
    return value;
  }

  set(key, value) {
    this.#map.set(key, new WeakRef(value));
  }
}
```

Caveats: results depend on GC timing and differ across engines and runs. Within one synchronous turn (job), `deref()` of an object that was alive stays alive. Do not build correctness on collection timing.

### `FinalizationRegistry`

Run a callback **after** an object is collected, for example to release an external resource keyed by a token.

```js
const registry = new FinalizationRegistry((heldValue) => {
  console.log('cleanup for', heldValue);   // may run late, or never
});

function open(resource) {
  const handle = { resource };
  registry.register(handle, resource.id);  // do not capture `handle` in the held value
  return handle;
}
```

Rules of thumb:

- Callbacks may be delayed or never run (for example when the process exits)
- Do **not** rely on them for essential cleanup: use explicit `close()` / `dispose` and `try/finally`
- Never put the target object itself in `heldValue` (it would keep it alive forever)

Explicit resource management (`using` / `await using` with `Symbol.dispose`) is the deterministic alternative where supported.

## Things the GC does not do for you

| Not freed automatically | Why | You must |
|-------------------------|-----|----------|
| Objects still reachable but unneeded | Reachable means "alive" | Drop references (`delete map[key]`, `set.delete`, `= null`, clear arrays) |
| Timers, intervals | The timer queue references the callback | `clearTimeout` / `clearInterval` |
| Event listeners | The target references the handler | `removeEventListener` / `off`, or `AbortSignal` |
| Open files, sockets, DB connections | OS resources, not heap memory | `close()`, `end()`, `destroy()` |
| Worker threads, child processes | Separate resources | `terminate()`, `kill()` |
| Native memory (`Buffer`, `ArrayBuffer`) | Counted as external; freed when the object is collected, but late | Drop references promptly; stream large data |

## GC cost and pauses

GC trades CPU and latency for convenience. Symptoms of GC pressure:

- Frame drops or jank in browsers (long main-thread pauses)
- Latency spikes in Node servers, visible as p99 outliers
- High CPU with little useful work (the GC is constantly running)
- Sawtooth heap graph that rises quickly and drops often (many short-lived allocations)

Reduce pressure by allocating less in hot paths (see [Memory Optimization](./04_memory-optimization.md)).

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Thinking `obj = null` always frees memory | Other references may still exist; memory is freed later, not instantly | Remove **all** references; trust the GC to run |
| Relying on `FinalizationRegistry` for cleanup | May never run | Explicit disposal |
| Using `WeakRef` for correctness | Timing is nondeterministic | Use it only for optional caches |
| Calling `global.gc()` in production | Pauses, defeats heuristics | Fix the retention instead |
| Forgetting timers and listeners | They keep objects reachable | Clean them up |
| Ignoring off-heap memory | GC may not run in time for large buffers | Stream, reuse buffers |
| Allocating heavily in hot loops | Constant minor GCs | Reuse objects, typed arrays |
| Assuming cycles leak | Tracing GC handles cycles | Only reachability from roots matters |

## Key takeaways

- An object is garbage only when **unreachable** from roots; cycles do not matter
- V8 uses a generational collector: fast scavenges for young objects, incremental and concurrent mark-sweep-compact for the old generation
- You cannot control when GC runs, and you should not depend on it for cleanup
- `WeakMap`, `WeakSet`, and `WeakRef` allow references that do not keep objects alive; `FinalizationRegistry` callbacks are unreliable
- Timers, listeners, and external resources are **not** freed by GC: release them explicitly
- Watch GC with `--trace-gc`, `PerformanceObserver`, and DevTools

**Next:** [Memory Leaks](./03_memory-leaks.md)