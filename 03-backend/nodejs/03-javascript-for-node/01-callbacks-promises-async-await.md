# Callbacks, Promises & Async/Await

Three different syntaxes for the same underlying problem: doing something once an asynchronous operation finishes, without blocking everything else while it's in progress. Understanding all three matters because real codebases mix them, especially older libraries still using callbacks.

```
Callbacks    →  the original approach — a function passed in, called later
Promises      →  an object representing a future value — .then()/.catch()
async/await    →  syntax sugar over Promises, reads like synchronous code
```

---

## Callbacks — the original approach

```js
import fs from "node:fs";

fs.readFile("file.txt", "utf-8", (err, data) => {
  if (err) {
    console.error("Error:", err);
    return;
  }
  console.log(data);
});

console.log("This logs BEFORE the file content — readFile is async");
```

The **error-first callback** convention (`(err, data) => {...}`) is a Node-wide standard: the first argument is always an error (or `null` if none), the rest are the actual result.

### The problem: "callback hell"

```js
fs.readFile("a.txt", "utf-8", (err, a) => {
  if (err) return handleError(err);
  fs.readFile("b.txt", "utf-8", (err, b) => {
    if (err) return handleError(err);
    fs.readFile("c.txt", "utf-8", (err, c) => {
      if (err) return handleError(err);
      console.log(a + b + c);
    });
  });
});
```

Chaining multiple async steps with callbacks nests deeper with each step — hard to read, hard to handle errors consistently, hard to run steps in parallel cleanly. This exact pain point is what Promises were designed to fix.

---

## Promises

```js
import { readFile } from "node:fs/promises";

readFile("file.txt", "utf-8")
  .then((data) => console.log(data))
  .catch((err) => console.error("Error:", err));
```

A Promise represents a value that isn't available yet, but will be (resolved) or will fail (rejected) at some point.

### Chaining, flattened

```js
readFile("a.txt", "utf-8")
  .then((a) => readFile("b.txt", "utf-8").then((b) => a + b))
  .then((combined) => console.log(combined))
  .catch((err) => console.error(err));
```

Flatter than the callback version, and errors from _any_ step in the chain are caught by one `.catch()` at the end, rather than needing an error check after every single step.

### Running things in parallel

```js
const [a, b, c] = await Promise.all([
  readFile("a.txt", "utf-8"),
  readFile("b.txt", "utf-8"),
  readFile("c.txt", "utf-8"),
]);
```

`Promise.all` runs all three reads concurrently rather than one after another — a meaningful speed-up when operations don't depend on each other, and something callback-based code has to implement manually.

### `Promise.all` vs `Promise.allSettled` vs `Promise.race`

```js
Promise.all([...]);         // rejects immediately if ANY promise rejects
Promise.allSettled([...]);   // waits for all, gives you each result OR error individually
Promise.race([...]);          // resolves/rejects as soon as the FIRST one settles
```

Use `allSettled` when you need every result regardless of individual failures (e.g. sending emails to a list of users — one failure shouldn't stop the others from being reported).

---

## `async`/`await` — the modern default

```js
import { readFile } from "node:fs/promises";

async function loadFiles() {
  try {
    const a = await readFile("a.txt", "utf-8");
    const b = await readFile("b.txt", "utf-8");
    console.log(a + b);
  } catch (err) {
    console.error("Error:", err);
  }
}
```

`async`/`await` is syntax sugar over Promises — `loadFiles()` above still returns a Promise, and `await` still relies on the Promise machinery underneath. The difference is purely how it reads: sequential steps look like ordinary synchronous code, and `try/catch` handles errors the same way it does for synchronous code, rather than needing `.catch()` chains.

```js
async function loadFilesInParallel() {
  const [a, b] = await Promise.all([
    readFile("a.txt", "utf-8"),
    readFile("b.txt", "utf-8"),
  ]);
  return a + b;
}
```

`async`/`await` doesn't replace `Promise.all` — combine them when you want parallelism inside an otherwise sequential-looking function.

---

## Converting an old callback API to Promises

```js
import { promisify } from "node:util";
import { exec } from "node:child_process";

const execAsync = promisify(exec);

const { stdout } = await execAsync("git rev-parse HEAD");
```

`util.promisify` wraps any Node-style error-first callback function into one that returns a Promise — useful for older libraries that haven't been updated to offer a Promise-based API directly (many core modules now ship both, like `fs`/`fs/promises`).

---

## Comparing all three for the same task

```js
// Callback
fs.readFile("file.txt", "utf-8", (err, data) => {
  if (err) return console.error(err);
  console.log(data);
});

// Promise
fs.promises
  .readFile("file.txt", "utf-8")
  .then((data) => console.log(data))
  .catch((err) => console.error(err));

// async/await
try {
  const data = await fs.promises.readFile("file.txt", "utf-8");
  console.log(data);
} catch (err) {
  console.error(err);
}
```

All three do exactly the same thing. `async`/`await` is the standard default for new code — the earlier two forms matter mainly for reading existing code and understanding what `async`/`await` is actually doing underneath.

## Common mistakes

- **Forgetting `await`** — `const data = readFile(...)` (missing `await`) gives you a pending Promise object, not the resolved value, and is a very common source of confusing bugs for people new to `async`/`await`.
- **Forgetting `try/catch` around `await`** — an unhandled rejection from an `await`ed call propagates as an unhandled promise rejection (`process.on("unhandledRejection", ...)`, `02-core-modules/07-process.md`) rather than failing where you might expect.
- **Awaiting sequentially when parallel would work** — `await a(); await b();` when `a` and `b` don't depend on each other wastes time; use `Promise.all([a(), b()])` instead.
- **Mixing callback-style and Promise-style code carelessly** — e.g. calling an async function inside a callback without awaiting or handling its rejection.

## Quick summary

- Callbacks are the original pattern; nested callbacks for sequential async steps become hard to read and error-handle ("callback hell")
- Promises flatten chains and add `Promise.all`/`allSettled`/`race` for coordinating multiple async operations
- `async`/`await` is syntax sugar over Promises that reads like synchronous code, including `try/catch` for errors
- `util.promisify` bridges old callback-style APIs into Promise-based ones
- Default to `async`/`await` for new code; understanding the other two helps with existing code and libraries

## Next

**`02-event-loop.md`** covers the mechanism underneath all three of these — how Node actually schedules and runs async callbacks.
