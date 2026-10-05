# Decorator Pattern

A decorator wraps an object or function and **adds behavior while keeping the same interface**. Callers can't tell the difference between the plain version and the decorated one, so decorators can be stacked and applied without changing the code that uses them.

Typical uses: logging, timing, caching, retrying, authorization checks, input validation.

**Prerequisites:** [Higher-Order Functions](../02_functions/06_higher-order-functions.md), [call, apply, bind](../05_this-and-oop/02_call-apply-bind.md), [Composition and Pipe](../07_functional-programming/04_composition-and-pipe.md)

> This note is about the **pattern**, which needs no special syntax. The `@decorator` syntax is a separate language feature (the TC39 decorators proposal). Check current engine and tooling support before relying on it in plain JavaScript.

---

## Function Decorators

A function decorator is a higher-order function: it takes a function and returns a function with the same signature.

```js
function withTiming(fn, report = console.log) {
  return function (...args) {
    const start = performance.now();
    try {
      return fn.apply(this, args);
    } finally {
      report(`${fn.name || "anonymous"} took ${(performance.now() - start).toFixed(1)}ms`);
    }
  };
}

const slowSum = (n) => { let s = 0; for (let i = 0; i < n; i++) s += i; return s; };
const timedSum = withTiming(slowSum);

timedSum(1e7); // same result as slowSum, plus a timing line
```

Points to notice:

- `function` (not an arrow) and `fn.apply(this, args)` so the decorator works on methods and forwards `this`.
- `try/finally` reports timing even if `fn` throws.
- The wrapper takes `...args` so it doesn't care about the arity of the original.

### Async functions

A wrapper around an async function must handle the promise, otherwise it measures only how long it took to *start* the work.

```js
function withTimingAsync(fn, report = console.log) {
  return async function (...args) {
    const start = performance.now();
    try {
      return await fn.apply(this, args);
    } finally {
      report(`${fn.name} took ${(performance.now() - start).toFixed(1)}ms`);
    }
  };
}
```

### Stacking

```js
const compose = (...decorators) => (fn) => decorators.reduceRight((f, d) => d(f), fn);

const hardened = compose(withLogging, withTimingAsync, withRetry)(fetchUser);
```

**Order matters.** With the line above, `withLogging` is outermost: it sees one call even if `withRetry` ran three attempts inside. Swap them and you log every attempt. Decide which behavior wraps which. For retry itself, see [Retry](../23_real-world-patterns/03_retry.md).

---

## Object Decorators

For objects, wrap one with another that **implements the same methods** and forwards to the original.

```js
class CachedUserRepo {
  #inner;
  #cache = new Map(); // id → Promise<user>

  constructor(inner) {
    this.#inner = inner;
  }

  findById(id) {
    if (!this.#cache.has(id)) {
      const p = this.#inner.findById(id).catch((err) => {
        this.#cache.delete(id); // don't cache failures
        throw err;
      });
      this.#cache.set(id, p);
    }
    return this.#cache.get(id);
  }
}

const repo = new CachedUserRepo(new DbUserRepo(db));
await repo.findById(1);
```

Why it caches the **promise** rather than the resolved value: two concurrent calls for the same id share one in-flight request instead of both hitting the database. Failed lookups are evicted so a transient error isn't remembered. This cache is unbounded and never expires. See [Caching](../23_real-world-patterns/08_caching.md) before using something like it in production.

Because `CachedUserRepo` has the same interface as `DbUserRepo`, the rest of the app doesn't change. Stack another one for logging if you like.

For wrapping *every* method automatically, a `Proxy` can do it in a few lines ([Proxy and Reflect](../09_built-in-objects/11_proxy-and-reflect.md)). The explicit class is easier to read and type for a small interface.

---

## Preserving Function Metadata

Wrappers are new functions, so `name` and `length` describe the wrapper. That makes stack traces and logs less useful.

```js
function withLogging(fn) {
  const wrapped = function (...args) {
    console.log(`→ ${fn.name}`, args);
    return fn.apply(this, args);
  };
  Object.defineProperty(wrapped, "name", { value: fn.name, configurable: true });
  return wrapped;
}
```

Do this if code relies on `fn.name` (logging, registries, debugging). Copying `length` the same way works too, but few things depend on it.

---

## Decorator vs Adapter vs Inheritance

- **Adapter** changes the interface ([Adapter](./07_adapter-pattern.md)). A decorator keeps it.
- **Inheritance** fixes the added behavior at class-definition time. Decorators are applied at runtime, in any combination, to any implementation of the interface.
- If you find yourself needing 2 decorators × 3 base classes, that's six subclasses with inheritance and five small pieces with decorators.

---

## Common Mistakes

- **Losing `this`** by using an arrow function wrapper or calling `fn(...args)` directly. Methods then break.
- **Forgetting to return the result** (or to `await`/return the promise). The caller gets `undefined`.
- **Changing the interface** by swallowing errors, changing return types, or adding required parameters. Then it's not a decorator anymore.
- **Wrong stacking order** (e.g. caching outside an authorization check, so unauthorized callers read cached data).
- **Losing metadata** (`name`) and then wondering why logs say `wrapped`.
- **Hidden state in the wrapper** (like the cache above) with no way to clear or bound it.

---

## Quick Summary

- Decorator = wrapper with the **same interface** that adds behavior.
- For functions: a higher-order function using `...args` and `apply(this, args)`.
- For objects: a class implementing the same methods and delegating to the inner one.
- Order of stacking changes behavior. Async wrappers must handle promises.
- No `@` syntax is required to use the pattern.

**Next:** [Dependency Injection](./09_dependency-injection.md)