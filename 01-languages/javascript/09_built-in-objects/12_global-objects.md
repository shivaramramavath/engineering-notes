# Global Objects

Globals are names available without importing. Some are part of the **language** (ECMAScript), others come from the **host** (browser, Node, Deno, Bun) or web platform standards.

## `globalThis`

The portable way to reach the global object.

| Environment | Global object |
|-------------|---------------|
| Browser main thread | `window` (also `self`, `frames`) |
| Web Worker | `self` |
| Node.js | `global` |
| Everywhere (ES2020) | **`globalThis`** |

```js
globalThis.setTimeout === setTimeout;   // true
typeof globalThis.document;             // "object" in browsers, "undefined" in Node
```

Top-level `var` and function declarations in classic scripts become properties of the global object; `let`, `const` and module-level declarations do not.

## Language-level globals (ECMAScript)

| Group | Names |
|-------|-------|
| Values | `undefined`, `NaN`, `Infinity`, `globalThis` |
| Functions | `parseInt`, `parseFloat`, `isNaN`, `isFinite`, `eval`, `encodeURIComponent`, `decodeURIComponent`, `encodeURI`, `decodeURI` |
| Fundamental objects | `Object`, `Function`, `Symbol`, `Error` and subclasses, `AggregateError` |
| Numbers and math | `Number`, `BigInt`, `Math` |
| Text | `String`, `RegExp` |
| Collections | `Array`, typed arrays, `Map`, `Set`, `WeakMap`, `WeakSet`, `WeakRef` |
| Structured data | `JSON`, `ArrayBuffer`, `SharedArrayBuffer`, `DataView`, `Atomics` |
| Control abstraction | `Promise`, `Iterator`, generators |
| Reflection | `Proxy`, `Reflect` |
| Other | `Date`, `Intl`, `FinalizationRegistry` |

Avoid `eval`, `escape`, `unescape` and `with`.

## URI encoding

```js
encodeURIComponent("a b&c=d/é");    // "a%20b%26c%3Dd%2F%C3%A9"  (for a query value or path segment)
encodeURI("https://x.dev/a b");      // keeps structural characters like : / ? &
decodeURIComponent("%C3%A9");        // "é"
```

Prefer `URL` and `URLSearchParams` for building URLs.

## Structured clone

```js
const copy = structuredClone({ date: new Date(), map: new Map([[1, { a: 1 }]]) });

structuredClone(obj, { transfer: [buffer] });   // move ArrayBuffers instead of copying
```

Handles cycles, `Date`, `Map`, `Set`, `RegExp`, typed arrays, `Error`. Throws on functions, DOM nodes, and drops class prototypes and getters.

## Scheduling

| API | Runs | Notes |
|-----|------|-------|
| `queueMicrotask(fn)` | end of the current task, before the next macrotask | same queue as promise callbacks |
| `setTimeout(fn, ms)` / `clearTimeout` | macrotask after ≥ `ms` | `ms` is a minimum, browsers clamp nested timers to ≥ 4ms |
| `setInterval(fn, ms)` / `clearInterval` | repeated macrotask | drifts, prefer recursive `setTimeout` |
| `setImmediate(fn)` (Node) | after I/O callbacks | not in browsers |
| `process.nextTick(fn)` (Node) | before promise microtasks | Node-only |
| `requestAnimationFrame(fn)` (browser) | before next paint | animations |
| `requestIdleCallback(fn)` (browser) | when idle | background work |
| `scheduler.postTask` (newer browsers) | prioritized tasks | check support |

```js
console.log("A");
setTimeout(() => console.log("timeout"), 0);
queueMicrotask(() => console.log("microtask"));
Promise.resolve().then(() => console.log("promise"));
console.log("B");
// A, B, microtask, promise, timeout
```

Details in `12_event-loop/`.

## Web-platform globals available in browsers and modern Node/Deno/Bun

| Global | Purpose |
|--------|---------|
| `fetch`, `Request`, `Response`, `Headers` | HTTP |
| `URL`, `URLSearchParams` | URL parsing and building |
| `AbortController`, `AbortSignal` | cancellation (`AbortSignal.timeout(ms)`, `AbortSignal.any([...])`) |
| `TextEncoder`, `TextDecoder` | UTF-8 encoding |
| `Blob`, `File` (Node 20+), `FormData` | binary and form data |
| `ReadableStream`, `WritableStream`, `TransformStream` | Web Streams |
| `EventTarget`, `Event`, `CustomEvent` | event system |
| `performance` (`now`, `mark`, `measure`) | high-resolution timing |
| `crypto` (`randomUUID`, `getRandomValues`, `subtle`) | cryptography |
| `atob`, `btoa` | Base64 for binary strings |
| `structuredClone` | deep clone |
| `BroadcastChannel`, `MessageChannel`, `MessagePort` | messaging |
| `console` | logging |
| `reportError(e)` | report an uncaught-style error |
| `navigator` | environment info (browser, partly Node) |

```js
const url = new URL("https://x.dev/search?q=js&page=2");
url.searchParams.get("q");           // "js"
url.searchParams.set("page", "3");
url.href;

const ctrl = new AbortController();
fetch(u, { signal: AbortSignal.any([ctrl.signal, AbortSignal.timeout(5000)]) });

crypto.randomUUID();
performance.now();
```

## Browser-only globals

`window`, `document`, `location`, `history`, `navigator`, `localStorage`, `sessionStorage`, `indexedDB`, `alert`, `confirm`, `prompt`, `customElements`, `getComputedStyle`, `matchMedia`, `IntersectionObserver`, `MutationObserver`, `ResizeObserver`, `Worker`, `Notification`, `screen`, `devicePixelRatio`.

## Node-only globals

`process`, `Buffer`, `__dirname` and `__filename` (CommonJS only), `require`, `module`, `exports` (CommonJS), `setImmediate`, `global`. In ES modules use `import.meta.url`, `import.meta.dirname` / `import.meta.filename` (recent Node versions).

## Detecting environment and features

```js
const isBrowser = typeof window !== "undefined" && typeof window.document !== "undefined";
const isNode = typeof process !== "undefined" && !!process.versions?.node;
const isWorker = typeof WorkerGlobalScope !== "undefined" && self instanceof WorkerGlobalScope;

if (typeof structuredClone === "function") { /* use it */ }
if ("IntersectionObserver" in globalThis) { /* use it */ }
```

Prefer **feature detection** over user-agent sniffing.

## Creating and polluting globals

```js
globalThis.appConfig = { debug: true };   // possible, but shared by everything
```

Guidelines:

- Avoid adding globals; use modules and explicit imports
- Namespaces reduce collisions: a single `window.MyApp`
- Use `Symbol.for("app.key")` for cross-realm shared keys
- Freeze or protect intentional globals (`Object.freeze`)
- In strict mode, assigning to an undeclared name throws instead of creating a global

## Realms

Each iframe, worker or `vm` context has its **own set of built-ins**. `instanceof Array` can fail across realms; use `Array.isArray` and duck typing.

```js
Array.isArray(iframe.contentWindow.Array.of(1));   // true
iframe.contentWindow.Array.of(1) instanceof Array; // false
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `window` in code that also runs in Node/workers | `ReferenceError` | `globalThis` and feature checks |
| Implicit globals (undeclared assignments) | Leaks, bugs | Strict mode, modules |
| Relying on `var` globals | Collisions | `const` / modules |
| `setInterval` for precise timing | Drift, overlap | Recursive `setTimeout`, timestamps |
| `setTimeout(fn, 0)` expecting immediate | ≥ 1 to 4 ms, after microtasks | `queueMicrotask` for "soon after this task" |
| `alert`/`prompt` for UX | Blocks, unavailable outside browsers | UI components |
| Assuming a global exists in old runtimes | Crashes | Feature detection, polyfills |
| Overwriting built-ins (`Array.prototype.x`) | Conflicts and future breakage | Helper functions |
| `instanceof` across realms | False negatives | `Array.isArray`, duck typing |

## Key takeaways

- Use `globalThis` for portable global access and prefer modules over globals
- `structuredClone`, `queueMicrotask`, `URL`, `AbortController`, `crypto` and `fetch` are available in modern runtimes
- Timers are macrotasks; microtasks run before them
- Detect features instead of environments, and avoid polluting the global scope

**Next:** [Error Handling](../10_error-handling/00_README.md)
