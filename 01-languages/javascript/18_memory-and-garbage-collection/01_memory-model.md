# Memory Model

Every value your program uses occupies memory. JavaScript hides allocation and deallocation, but the engine still follows a model: a **call stack** for execution bookkeeping and a **heap** for most values. Knowing which is which explains copying behavior, equality, mutation surprises, and memory costs.

## Stack and heap

| | Call stack | Heap |
|---|-----------|------|
| Holds | Stack frames: local bindings, return addresses | Objects, arrays, functions, closures, strings (large), `Map`/`Set`, etc. |
| Allocation | Automatic, very fast (push/pop) | Dynamic, managed by the GC |
| Lifetime | Until the function returns | Until no longer reachable |
| Size | Small, fixed limit (stack overflow if exceeded) | Large, grows up to a limit |
| Structure | Ordered, LIFO | Unordered graph of objects |

```js
function area(w, h) {        // frame pushed: w, h live here
  const result = w * h;      // result lives in this frame
  return result;             // frame popped: gone
}

const rect = { w: 3, h: 4 };       // the object lives on the heap; `rect` holds a reference
area(rect.w, rect.h);
```

The call stack is described in [Execution Context and Call Stack](../04_scope-and-execution/03_execution-context-and-call-stack.md).

### Engine reality

The stack/heap split is a **mental model**. Real engines are more subtle:

- V8 may keep small integers directly in a tagged value (no allocation), allocate other numbers (heap numbers) when needed, and optimize hot code so some objects never reach the heap (escape analysis)
- Variables captured by a closure live in a heap-allocated **context**, even though they are "locals"
- The specification says nothing about the stack or heap; it only describes values and lifetimes

Use the model to reason, and verify with tools when it matters.

## Primitives vs objects

| Primitives | Objects |
|-----------|---------|
| `number`, `bigint`, `string`, `boolean`, `undefined`, `null`, `symbol` | Plain objects, arrays, functions, `Date`, `Map`, `Set`, class instances, ... |
| Immutable values | Mutable (unless frozen) |
| Copied by value | Copied by **reference** |
| Compared by value | Compared by identity |

```js
let a = 10;
let b = a;          // copies the value
b = 20;
console.log(a);     // 10

const o1 = { n: 1 };
const o2 = o1;      // copies the reference: both point to the same object
o2.n = 2;
console.log(o1.n);  // 2

{ n: 1 } === { n: 1 };   // false: two different objects
o1 === o2;               // true: the same object
```

See [Copying and Cloning](../03_objects-and-arrays/05_copying-and-cloning.md).

## References and the object graph

A variable holds either a primitive or a **reference** (like a pointer) to an object. Objects reference other objects, forming a **graph**:

```js
const user = {
  name: 'Ada',
  address: { city: 'London' },
  friends: [],
};
user.friends.push(user);         // cycle: user → friends → user (fine for GC)
```

```
user ──▶ { name, address ─▶ { city }, friends ─▶ [ ──▶ user ] }
```

Passing an object to a function passes the reference **by value**: the function can mutate the object, but reassigning the parameter does not affect the caller.

```js
function rename(u) {
  u.name = 'Grace';     // mutates the shared object
  u = { name: 'Other' };  // rebinds the local parameter only
}
```

## Strings

Strings are primitives but can be large. Engines optimize them:

- Strings are immutable; `s + t` may create a **rope** (a tree of pieces) that is flattened lazily
- Substrings and slices can keep the original large string alive (`str.slice()` may retain the parent in some engines)
- Short strings can be **internalized** (deduplicated), so identical property names and literals share storage

```js
let s = '';
for (let i = 0; i < 1e5; i++) s += 'x';   // modern engines handle this well via ropes
```

A very large string that you slice small pieces from can keep the whole thing in memory. If you only need a small part of a huge string, consider copying it into a new string (for example by concatenation or an array/`Buffer` round-trip) and dropping the original, and verify with a heap snapshot that it matters.

## Numbers

- All `number`s are 64-bit IEEE 754 doubles semantically
- V8 stores **small integers (Smis)** inline in the tagged value (31 or 32 bits depending on pointer compression); other numbers are boxed as heap numbers unless optimized away
- `BigInt` values are heap-allocated and grow with magnitude

Practical effect: arrays of small integers are compact; arrays of mixed doubles and objects are less so.

## How V8 represents objects

V8 does not store each object as a dictionary. It uses **hidden classes** (also called shapes or maps): objects created with the same properties in the same order share a shape, and each object stores only its values.

```js
const p1 = { x: 1, y: 2 };    // shape A
const p2 = { x: 3, y: 4 };    // shape A (shared)
const p3 = { y: 5, x: 6 };    // shape B (different order → different shape)
```

Benefits:

- Compact objects: no per-object property-name storage
- Fast property access through inline caches

Things that push objects into slow **dictionary mode** (hash table, more memory, slower access):

- Adding many properties dynamically, or using objects as big hash maps
- Using `delete` on properties
- Defining properties in different orders across instances

```js
class Point {
  constructor(x, y) { this.x = x; this.y = y; }   // consistent shape for every instance
}

// Prefer
const cfg = { a: 1, b: null };   // initialize all fields up front, set b later

// Avoid
const o = {};
o.a = 1; if (cond) o.b = 2;      // shapes vary
delete o.a;                      // can trigger dictionary mode
```

For dynamic key-value data use `Map` instead of an object.

## Arrays and element kinds

V8 tracks the **elements kind** of an array and picks a compact layout:

| Kind | Holds | Notes |
|------|-------|-------|
| `PACKED_SMI_ELEMENTS` | Small integers only | Most compact and fastest |
| `PACKED_DOUBLE_ELEMENTS` | Numbers (including non-integers) | Unboxed doubles |
| `PACKED_ELEMENTS` | Anything | References |
| `HOLEY_*` | Same, with gaps | Slightly slower; checks for holes |

An array moves to a more general kind when you add an incompatible value, and it **never goes back**:

```js
const a = [1, 2, 3];     // PACKED_SMI
a.push(1.5);             // → PACKED_DOUBLE
a.push('x');             // → PACKED_ELEMENTS (permanently)

const b = new Array(1000);   // HOLEY: avoid when you can; fill with push or Array.from
```

Typed arrays (`Float64Array`, `Uint8Array`) store raw numbers contiguously with no per-element overhead; see [Memory Optimization](./04_memory-optimization.md).

## Functions and closures

A function value is an object on the heap. A **closure** also keeps a reference to the **lexical environment** it was created in, so captured variables stay alive as long as the function is reachable.

```js
function makeCounter() {
  let count = 0;                 // lives in a heap-allocated context
  return () => ++count;          // the returned function keeps it alive
}

const next = makeCounter();      // count survives after makeCounter returns
```

V8 only keeps **captured** variables in the context, but a context is shared by all closures created in the same scope. If any of them captures a large value, it stays alive for all of them. See [Closure Pitfalls](../06_closures/03_closure-pitfalls.md).

## Measuring memory

### In Node

```js
const m = process.memoryUsage();
// {
//   rss,           // total memory held by the process (resident set size)
//   heapTotal,     // V8 heap reserved
//   heapUsed,      // V8 heap in use
//   external,      // memory for C++ objects tied to JS objects (e.g. Buffers)
//   arrayBuffers   // memory for ArrayBuffers and Buffers
// }

const mb = (n) => (n / 1024 / 1024).toFixed(1) + ' MB';
console.log('heapUsed', mb(m.heapUsed));
```

```js
import v8 from 'node:v8';
v8.getHeapStatistics();          // heap size limit, used, available, ...
```

`heapUsed` excludes `Buffer` data (which lives off-heap), so watch `external` and `arrayBuffers` as well.

### In the browser

```js
performance.memory;                          // Chrome only, coarse (non-standard)
await performance.measureUserAgentSpecificMemory?.();   // needs cross-origin isolation, Chrome
```

For detailed analysis use the DevTools **Memory** panel: heap snapshots, allocation instrumentation, and allocation sampling (see [Memory Leaks](./03_memory-leaks.md)).

## Limits

| Limit | Notes |
|-------|-------|
| Call stack depth | About 10,000 or more frames, engine and frame-size dependent; exceeded → `RangeError: Maximum call stack size exceeded` |
| Node heap limit | Depends on version and system memory; raise with `node --max-old-space-size=4096 app.js` (MB) |
| Browser tab | Browser-dependent; large pages may be killed |
| Max array length | 2³² − 1 elements |
| Max string length | Engine-dependent (hundreds of MB; about 512 MB in V8) |
| `ArrayBuffer` size | Large but platform-limited |

Hitting the heap limit crashes Node with `FATAL ERROR: ... JavaScript heap out of memory`.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Thinking `b = a` copies an object | Both names share one object | `structuredClone`, spread (shallow), or immutable updates |
| Using objects as large dynamic maps | Dictionary mode, memory overhead | `Map` |
| Using `delete` in hot paths | Deoptimizes shapes | Set to `undefined`/`null`, or rebuild the object |
| Creating holey or mixed-type arrays | Slower, bigger | Keep arrays homogeneous; avoid `new Array(n)` |
| Assuming locals always vanish at return | Captured variables survive in closures | Avoid capturing large values unnecessarily |
| Ignoring off-heap memory (`Buffer`, `ArrayBuffer`) | `heapUsed` looks fine while `rss` grows | Watch `external` and `arrayBuffers` |
| Assuming the heap is unlimited | Process crashes at the limit | Monitor, stream, and set limits deliberately |
| Holding a slice of a giant string | Keeps the whole string alive | Copy the part you need |

## Key takeaways

- The stack holds call frames; the heap holds objects, arrays, functions, and closure contexts
- Primitives are copied by value; objects are shared by reference and compared by identity
- Memory forms a graph of references; what is reachable from roots stays alive
- V8 uses hidden classes and element kinds: consistent shapes and homogeneous arrays are cheaper
- Closures keep captured variables on the heap for as long as the function is reachable
- Measure with `process.memoryUsage()`, `v8.getHeapStatistics()`, and DevTools, and watch off-heap memory too

**Next:** [Garbage Collection](./02_garbage-collection.md)