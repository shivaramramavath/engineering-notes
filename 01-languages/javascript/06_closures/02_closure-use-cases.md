# Closure Use Cases

Closures solve practical problems: hiding data, configuring functions, caching results, running code once, and creating modules.

## 1. Data privacy (encapsulation)

Variables in the closure cannot be accessed from outside.

```js
function createStack() {
  const items = [];                        // private

  return {
    push(x) { items.push(x); },
    pop()   { return items.pop(); },
    peek()  { return items[items.length - 1]; },
    get size() { return items.length; },
  };
}

const stack = createStack();
stack.push(1);
stack.size;      // 1
stack.items;     // undefined (unreachable)
```

Modern alternative: class `#private` fields (`05_this-and-oop/07_private-fields.md`).

## 2. Factory functions

Create configured objects or functions without `new` or `this`.

```js
function createLogger(prefix, { level = "info" } = {}) {
  const levels = ["debug", "info", "warn", "error"];
  const min = levels.indexOf(level);

  return (lvl, msg) => {
    if (levels.indexOf(lvl) >= min) console.log(`[${prefix}] ${lvl}: ${msg}`);
  };
}

const log = createLogger("api", { level: "warn" });
log("info", "hidden");
log("error", "shown");
```

```js
const createUser = (name) => ({
  name,
  greet: () => `Hi, ${name}`,      // no `this` needed
});
```

## 3. Function factories and partial application

```js
const add = (a) => (b) => a + b;
const add10 = add(10);
add10(5);   // 15

const greet = (greeting) => (name) => `${greeting}, ${name}!`;
const hello = greet("Hello");
hello("Ada");   // "Hello, Ada!"

const withTax = (rate) => (price) => price * (1 + rate);
const withVat = withTax(0.2);
```

Currying builds on this: `06_closures` here, `07_functional-programming/05_currying-and-partial-application.md` later.

## 4. Memoization

Cache results in a private `Map`.

```js
function memoize(fn) {
  const cache = new Map();

  return function (...args) {
    const key = JSON.stringify(args);
    if (cache.has(key)) return cache.get(key);
    const result = fn.apply(this, args);
    cache.set(key, result);
    return result;
  };
}

const slowSquare = (n) => { for (let i = 0; i < 1e8; i++); return n * n; };
const fastSquare = memoize(slowSquare);
fastSquare(9);   // slow
fastSquare(9);   // instant
```

Caveat: `JSON.stringify` keys ignore functions and reorder nothing; for single primitive arguments use the argument itself as the key. Cap cache size for long-running apps (see pitfalls).

## 5. Run once

```js
function once(fn) {
  let called = false;
  let result;
  return (...args) => {
    if (!called) {
      called = true;
      result = fn(...args);
    }
    return result;
  };
}

const init = once(() => { console.log("setup"); return 42; });
init();   // logs "setup", returns 42
init();   // returns 42, no log
```

## 6. Debounce and throttle

```js
function debounce(fn, ms) {
  let timer;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), ms);
  };
}

function throttle(fn, ms) {
  let last = 0;
  return (...args) => {
    const now = Date.now();
    if (now - last >= ms) {
      last = now;
      fn(...args);
    }
  };
}

input.addEventListener("input", debounce(search, 300));
window.addEventListener("scroll", throttle(update, 100));
```

`timer` and `last` are private state shared by every call of the returned function.

## 7. Module pattern (pre-ES modules)

```js
const Counter = (() => {
  let count = 0;                     // module-private

  const step = () => 1;              // private helper

  return {
    inc: () => (count += step()),
    reset: () => { count = 0; },
    get value() { return count; },
  };
})();

Counter.inc();
Counter.value;   // 1
```

ES modules give the same privacy per file: unexported names stay private.

## 8. Event handlers and callbacks with context

```js
function attach(button, label) {
  let clicks = 0;
  button.addEventListener("click", () => {
    clicks++;
    console.log(`${label} clicked ${clicks} times`);
  });
}
```

Each call of `attach` gets its own `clicks`.

## 9. Iterators and generators of state

```js
function idGenerator(prefix = "id") {
  let n = 0;
  return () => `${prefix}-${++n}`;
}
const nextId = idGenerator("user");
nextId();   // "user-1"
nextId();   // "user-2"

function makeRange(from, to) {
  let current = from;
  return {
    next: () => current <= to ? { value: current++, done: false } : { value: undefined, done: true },
    [Symbol.iterator]() { return this; },
  };
}
```

## 10. Configuration and dependency injection

```js
function createApi({ baseUrl, token }) {
  const headers = { Authorization: `Bearer ${token}` };

  return {
    get: (path) => fetch(`${baseUrl}${path}`, { headers }).then((r) => r.json()),
    post: (path, body) =>
      fetch(`${baseUrl}${path}`, { method: "POST", headers, body: JSON.stringify(body) }),
  };
}

const api = createApi({ baseUrl: "https://api.example.com", token: "abc" });
```

## 11. Rate limiting and counters

```js
function limit(fn, max) {
  let calls = 0;
  return (...args) => {
    if (calls >= max) throw new Error("limit reached");
    calls++;
    return fn(...args);
  };
}
```

## 12. Lazy evaluation

```js
function lazy(compute) {
  let value, done = false;
  return () => {
    if (!done) { value = compute(); done = true; }
    return value;
  };
}
const config = lazy(() => loadHugeConfig());   // computed on first use only
```

## 13. Function composition

```js
const pipe = (...fns) => (x) => fns.reduce((acc, fn) => fn(acc), x);
const slug = pipe(
  (s) => s.trim(),
  (s) => s.toLowerCase(),
  (s) => s.replace(/\s+/g, "-"),
);
```

## When to use a closure vs a class

| Choose closures for | Choose classes for |
|---------------------|--------------------|
| Few instances, private state | Many instances (shared prototype methods) |
| Small utilities (`once`, `debounce`) | Rich types with inheritance |
| Avoiding `this` | `instanceof`, tooling, decorators |
| Configuring behavior | Long-lived domain models |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Unbounded memoize cache | Memory leak | LRU cache, `WeakMap` for object keys |
| Closure-private state impossible to test | Hard to inspect | Expose a small test hook or test behavior |
| Debounce timers not cleared on teardown | Fires after unmount | Return a `cancel()` |
| Overusing closures for large object graphs | Per-instance function memory | Classes with prototype methods |
| Shared state between callers unintentionally | Surprising coupling | Create a new closure per consumer |

## Key takeaways

- Closures give private state, configuration and memory across calls
- Factories, memoization, `once`, `debounce`, modules are all closure patterns
- Each call to the outer function makes an independent instance
- Add `cancel`/`clear` handles for timers and caches

**Next:** [Closure Pitfalls](./03_closure-pitfalls.md)
