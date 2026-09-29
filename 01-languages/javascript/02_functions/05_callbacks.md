# Callbacks

A **callback** is a function passed to another function to be called later (or repeatedly) by it.

```
you write  callback  ──passed to──►  host function  ──calls it──►  when ready
```

## Synchronous callbacks

Called immediately, during the host function's execution.

```js
[1, 2, 3].map((n) => n * 2);

function repeat(times, fn) {
  for (let i = 0; i < times; i++) fn(i);
}
repeat(3, (i) => console.log("step", i));
```

## Asynchronous callbacks

Called **after** the current code finishes.

```js
setTimeout(() => console.log("later"), 1000);

button.addEventListener("click", (event) => console.log(event.type));

console.log("first"); // runs before "later"
```

| Kind  | When it runs             | Examples                                  |
| ----- | ------------------------ | ----------------------------------------- |
| Sync  | during the call          | `map`, `filter`, `sort`, `forEach`        |
| Async | later via the event loop | timers, events, I/O, `fetch` (older APIs) |

## Callback arguments

Array methods pass `(value, index, array)`.

```js
["a", "b"].forEach((value, index) => console.log(index, value));
```

Trap: passing a function that takes extra parameters.

```js
["1", "2", "3"].map(parseInt); // [1, NaN, NaN]  (parseInt receives the index as radix)
["1", "2", "3"].map(Number); // [1, 2, 3]
["1", "2", "3"].map((s) => parseInt(s, 10));
```

## Error-first callbacks (Node style)

```js
import { readFile } from "node:fs";

readFile("data.txt", "utf8", (err, data) => {
  if (err) return console.error(err);
  console.log(data);
});
```

Convention: first argument is the error (or `null`), the rest is the result.

## Callback hell

```js
getUser(id, (err, user) => {
  getOrders(user, (err, orders) => {
    getItems(orders[0], (err, items) => {
      // deeper and deeper
    });
  });
});
```

Fixes:

1. Name and extract functions
2. Use **Promises**: see `11_asynchronous-javascript/03_promises.md`
3. Use **async/await**

```js
const user = await getUser(id);
const orders = await getOrders(user);
const items = await getItems(orders[0]);
```

## Converting callbacks to promises

```js
import { promisify } from "node:util";
const readFileAsync = promisify(readFile);

const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
await wait(500);
```

## `this` inside callbacks

```js
class Counter {
  count = 0;
  start() {
    setInterval(() => this.count++, 1000); // arrow keeps `this`
  }
}
```

Or bind explicitly: `element.addEventListener("click", this.handle.bind(this))`.

## Designing APIs that take callbacks

```js
function fetchWithRetry(task, { retries = 3, onRetry = () => {} } = {}) { ... }
```

- Give optional callbacks a **no-op default**
- Document when and how often the callback is called
- Do not mix sync and async invocation of the same callback (it causes race bugs)
- Always call it once (or document repetition)

## Callback pitfalls

| Pitfall                         | Why it hurts                          | Better                         |
| ------------------------------- | ------------------------------------- | ------------------------------ |
| `map(parseInt)`                 | Extra arguments change behavior       | Wrap in an arrow               |
| Ignoring the error argument     | Silent failures                       | Handle `err` first             |
| Deep nesting                    | Callback hell                         | Promises, async/await          |
| Calling a callback twice        | Duplicate effects                     | Return after calling           |
| Losing `this`                   | Wrong context                         | Arrows or `bind`               |
| Throwing inside async callbacks | Cannot be caught by outer `try/catch` | Handle inside, or use promises |

## Key takeaways

- A callback is just a function passed as an argument
- Sync callbacks run now, async callbacks run via the event loop
- Node callbacks are error-first
- Prefer promises and async/await to avoid nesting

**Next:** [Higher-Order Functions](./06_higher-order-functions.md)
