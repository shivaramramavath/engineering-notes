# Global Objects

Node makes a set of objects and functions available in every file, with no `import`/`require` needed. This file covers what's actually there, and a few CommonJS-only globals that trip people up in ES Modules.

## The `global` object

```js
console.log(typeof global); // "object"
```

`global` is Node's equivalent of `window` in a browser — the top-level object everything global attaches to. You'll almost never reference `global` directly, but it's worth knowing it exists, since it's the reason things like `console` and `setTimeout` work without an import.

```js
global.myValue = 42;
console.log(myValue); // 42 — accessible anywhere, generally best avoided
```

Attaching your own values to `global` works but is considered poor practice — it's the Node equivalent of polluting the browser's `window` object, making dependencies between files implicit and hard to trace.

---

## `console`

```js
console.log("info");
console.error("errors, printed to stderr not stdout");
console.warn("warnings");
console.table([
  { a: 1, b: 2 },
  { a: 3, b: 4 },
]); // renders a formatted table
console.time("operation");
// ... some work ...
console.timeEnd("operation"); // logs elapsed time
```

`console.log`/`console.info` write to **stdout**; `console.error`/`console.warn` write to **stderr** — a meaningful distinction when piping or redirecting output (`node app.js > out.log 2> err.log`), and relevant to logging setups (`14-logging-observability/01-pino-and-structured-logging.md`) that treat error-level output differently.

---

## Timers

```js
setTimeout(() => console.log("later"), 1000);
setInterval(() => console.log("repeating"), 5000);
setImmediate(() => console.log("after current operation completes"));

clearTimeout(timeoutId);
clearInterval(intervalId);
```

These behave largely like their browser equivalents, but Node adds `setImmediate` — schedules a callback to run right after the current event loop phase completes, distinct from `setTimeout(fn, 0)` in subtle ordering ways covered in `03-javascript-for-node/02-event-loop.md`.

---

## `process`

```js
console.log(process.env.NODE_ENV);
console.log(process.argv);
```

Covered in full in `02-core-modules/07-process.md` — mentioned here because it's globally available like `console`, with no import required, unlike almost every other Node capability (`fs`, `http`, etc.), which must be explicitly imported.

---

## `Buffer`

```js
const buf = Buffer.from("hello");
```

Also globally available without an import — covered fully in `02-core-modules/06-buffer.md`.

---

## Modern Web APIs, now also global in Node

Since Node 18+, several APIs standard in browsers are available globally in Node too, without any import:

```js
const res = await fetch("https://api.example.com/data");
const data = await res.json();

const url = new URL("https://example.com/path?query=1");
console.log(url.searchParams.get("query"));

const controller = new AbortController();
setTimeout(() => controller.abort(), 5000);
fetch(url, { signal: controller.signal });
```

- `fetch` — making HTTP requests, the same API browsers have (superseding most direct use of `http.request`, see `02-core-modules/03-http.md`)
- `URL`/`URLSearchParams` — parsing and building URLs
- `AbortController` — canceling an in-progress async operation (a fetch request, for example)

This convergence is deliberate — it means a growing amount of code (especially around `fetch`) runs identically in Node and in a browser with no adaptation needed.

---

## CommonJS-only globals: `__dirname`, `__filename`, `require`, `module`

```js
// only available in CommonJS files
console.log(__dirname); // absolute path to the current file's directory
console.log(__filename); // absolute path to the current file
console.log(module.exports); // this file's exports object
```

These are **not actually globals** in the strict sense — they're injected per-file by Node's CommonJS module wrapper, which is why they behave like globals in CommonJS but don't exist at all in ES Modules:

```js
// in an ES Module (.mjs, or "type": "module"), this throws:
console.log(__dirname);
// ReferenceError: __dirname is not defined
```

The ESM replacement is covered fully in `03-module-system.md` — this is one of the most common points of confusion when a project switches from CommonJS to ES Modules.

---

## `globalThis`: the standard, environment-agnostic global

```js
console.log(globalThis === global); // true, in Node
```

`globalThis` is the standard JavaScript way to reference "whatever the global object is" — `window` in a browser, `global` in Node, `self` in a Web Worker. Code intended to run in multiple environments should prefer `globalThis` over the Node-specific `global` or browser-specific `window`.

## Quick summary

- `global` is Node's top-level object, roughly equivalent to a browser's `window` — avoid attaching your own values to it
- `console`, timers, `process`, and `Buffer` are all available without an import
- Since Node 18, `fetch`, `URL`, and `AbortController` are global too, matching browser JavaScript
- `__dirname`/`__filename`/`require`/`module` are CommonJS-only — they don't exist in ES Modules, which need a different approach (`03-module-system.md`)
- `globalThis` is the standard, environment-agnostic way to reference the global object

## Next

**`03-module-system.md`** covers CommonJS vs ES Modules — including that `__dirname` gap and how to work around it.
