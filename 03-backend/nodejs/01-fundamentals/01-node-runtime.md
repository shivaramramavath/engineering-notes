# The Node.js Runtime

Node.js lets you run JavaScript outside a browser. This file covers what that actually means, and how Node differs from the JavaScript environment you'd get in a browser tab.

## What Node actually is

```
Node.js = V8 (Chrome's JavaScript engine) + libuv (async I/O) + built-in APIs (fs, http, ...)
```

- **V8** — the same JavaScript engine that powers Google Chrome, responsible for actually parsing and executing your JavaScript
- **libuv** — a C library providing the event loop and asynchronous, non-blocking I/O (file access, networking) across operating systems
- **Built-in modules** — `fs`, `http`, `path`, and everything else covered in `02-core-modules/`, giving JavaScript capabilities it has no access to in a browser

None of this is magic beyond what's documented — Node is, fundamentally, V8 embedded in a program that also gives your JavaScript access to the file system, network, and operating system, none of which a browser allows (for good security reasons — a random website shouldn't be able to read your files).

---

## What Node can do that browser JavaScript can't

```js
import fs from "node:fs";
const data = fs.readFileSync("file.txt", "utf-8");
```

Browser JavaScript has no `fs` module at all — reading arbitrary files from a visited website would be a massive security hole. Node, running as a trusted program on your own machine or server, has no such restriction.

| Capability                  | Browser | Node                                |
| --------------------------- | ------- | ----------------------------------- |
| File system access          | No      | Yes (`fs`)                          |
| Creating a TCP/HTTP server  | No      | Yes (`http`, `net`)                 |
| Spawning other processes    | No      | Yes (`child_process`)               |
| DOM / `window` / `document` | Yes     | No — there is no page to manipulate |
| `fetch` for HTTP requests   | Yes     | Yes (built in since Node 18)        |

The DOM row matters more than it might seem: code written assuming a browser environment (anything touching `window`, `document`, or `localStorage`) simply won't run in Node without modification, and vice versa — Node-specific code using `fs` or `process` won't run in a browser.

---

## Single-threaded, event-driven

Node runs your JavaScript on a **single thread** — but handles many concurrent operations efficiently through its **event loop**, rather than by using multiple threads for each one.

```js
console.log("1. Start");

setTimeout(() => console.log("3. Timeout callback"), 0);

console.log("2. End");
```

```
1. Start
2. End
3. Timeout callback
```

Even with a `0ms` delay, the callback runs _after_ the synchronous code finishes — Node doesn't pause to wait for it. This single fact explains a huge amount of how Node code needs to be written and is covered in full in `03-javascript-for-node/02-event-loop.md`.

### Why single-threaded works well for I/O-bound work

Most server workloads spend most of their time _waiting_ — for a database query, a file read, a network response — rather than doing heavy CPU computation. Node hands that waiting off to the operating system (via libuv) and keeps the single JavaScript thread free to handle other requests in the meantime, rather than blocking a thread per request the way some other server models do.

### The trade-off: CPU-bound work blocks everything

```js
// blocks the entire process — no other request can be handled meanwhile
function heavySyncWork() {
  let total = 0;
  for (let i = 0; i < 10_000_000_000; i++) total += i;
  return total;
}
```

A long synchronous computation blocks the single thread entirely, freezing every other in-flight request until it finishes. This is exactly the problem `worker_threads` (`02-core-modules/10-cluster-and-worker-threads.md`) exists to solve.

---

## The Node.js process lifecycle

```js
console.log("Script starts running");

setTimeout(() => {
  console.log("Async work happens");
}, 1000);

console.log("Script finishes its synchronous portion");
```

Node keeps the process alive as long as there's pending work — a timer, an open server, a pending promise. Once nothing is left to do, the process exits naturally (see `process.exitCode` vs `process.exit()` in `02-core-modules/07-process.md`).

```bash
node script.js
```

A plain script with no server or pending timers runs top to bottom and exits immediately once finished. A running HTTP server (`http.createServer(...).listen(...)`) keeps the process alive indefinitely, since the server itself represents ongoing pending work.

---

## Versions and release lines

```bash
node --version
```

Node has two kinds of releases:

- **LTS (Long-Term Support)** — even-numbered major versions (18, 20, 22...), recommended for production, supported for years
- **Current** — the latest features, odd-numbered majors, shorter support window, mainly useful for trying out new features early

```bash
nvm install --lts   # covered in 00-setup/01-getting-started.md
```

For production apps, stick to LTS releases unless you have a specific reason to need a Current release's newer features.

## Quick summary

- Node = V8 (executes JS) + libuv (async I/O) + built-in modules (`fs`, `http`, etc.)
- Node has no DOM/`window`/`document`; browsers have no `fs`/`process`/`child_process` — code assuming one environment generally won't run unmodified in the other
- JavaScript execution is single-threaded; I/O is handled asynchronously without blocking that thread, but CPU-bound synchronous code still blocks everything
- A Node process stays alive as long as there's pending work (an open server, a timer); it exits naturally once there's nothing left
- Prefer LTS releases for production

## Next

**`02-global-objects.md`** covers what's available in every Node file without needing an import — `console`, `process`, timers, and more.
