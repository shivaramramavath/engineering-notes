# Decorator Pattern

A **decorator** wraps an object or function to **add behavior** while keeping the **same interface**. The caller uses the decorated version exactly like the original, but gets extra features: logging, caching, retries, timing, validation, authorization.

Decorators can be stacked, so you compose small behaviors instead of building one giant class or creating a subclass for every combination.

See also: [Higher-Order Functions](../02_functions/06_higher-order-functions.md), [Closures](../06_closures/00_README.md), [Composition and Pipe](../07_functional-programming/04_composition-and-pipe.md), [Mixins and Composition](../05_this-and-oop/08_mixins-and-composition.md).

> Not the same as the `@decorator` **syntax** in TypeScript and the newer JavaScript decorators proposal. That syntax is a convenient way to apply the *pattern* to classes and methods (covered near the end of this file).

## The problem: subclass explosion

You have a `DataSource` and want optional encryption, compression, and logging. With inheritance you need a class for every combination:

```
DataSource
├── EncryptedDataSource
├── CompressedDataSource
├── LoggedDataSource
├── EncryptedCompressedDataSource
├── EncryptedLoggedDataSource
├── CompressedLoggedDataSource
└── EncryptedCompressedLoggedDataSource    ← and so on, 2ⁿ classes
```

Decorators let you combine behaviors at runtime instead: `logged(compressed(encrypted(source)))`.

## Function decorators (higher-order functions)

A function decorator takes a function and returns a new function with extra behavior:

```js
function withLogging(fn) {
  return function (...args) {
    console.log(`Calling ${fn.name} with`, args);
    const result = fn.apply(this, args);
    console.log(`${fn.name} returned`, result);
    return result;
  };
}

function add(a, b) { return a + b; }

const loggedAdd = withLogging(add);
loggedAdd(2, 3);
// Calling add with [2, 3]
// add returned 5
```

Details that matter:

- Forward **all arguments** (`...args`) and **`this`** (`fn.apply(this, args)`), and **return** the result
- The decorated function keeps the same signature, so it can replace the original anywhere

### Common function decorators

```js
// Timing
const timed = (fn) => async function (...args) {
  const start = performance.now();
  try {
    return await fn.apply(this, args);
  } finally {
    console.log(`${fn.name} took ${(performance.now() - start).toFixed(1)} ms`);
  }
};

// Memoization (for pure functions)
const memoized = (fn) => {
  const cache = new Map();
  return function (...args) {
    const key = JSON.stringify(args);
    if (!cache.has(key)) cache.set(key, fn.apply(this, args));
    return cache.get(key);
  };
};

// Retry
const retrying = (fn, { retries = 3, delayMs = 200 } = {}) => async function (...args) {
  for (let attempt = 0; ; attempt++) {
    try {
      return await fn.apply(this, args);
    } catch (err) {
      if (attempt >= retries) throw err;
      await new Promise((r) => setTimeout(r, delayMs * 2 ** attempt));
    }
  }
};

// Timeout
const withTimeout = (fn, ms) => function (...args) {
  return Promise.race([
    fn.apply(this, args),
    new Promise((_, reject) => setTimeout(() => reject(new Error(`Timed out after ${ms} ms`)), ms)),
  ]);
};

// Ensure only the first call runs
const once = (fn) => {
  let called = false, value;
  return function (...args) {
    if (!called) { called = true; value = fn.apply(this, args); }
    return value;
  };
};
```

### Stacking decorators

```js
const fetchUser = async (id) => { /* network call */ };

const robustFetchUser = timed(retrying(withTimeout(fetchUser, 2000), { retries: 2 }));
```

Order matters: the outermost decorator runs first on the way in and last on the way out. Here `timed` measures the whole thing including retries, and each attempt is limited to 2 seconds.

Use a `compose`/`pipe` helper to read it more easily:

```js
const decorate = (fn, ...decorators) => decorators.reduceRight((f, d) => d(f), fn);

const robust = decorate(
  fetchUser,
  (f) => timed(f),
  (f) => retrying(f, { retries: 2 }),
  (f) => withTimeout(f, 2000),
);
```

## Object decorators (wrapping)

For objects with several methods, wrap the object and delegate:

```js
// Interface: store.get(key), store.set(key, value)
const memoryStore = {
  data: new Map(),
  get(key) { return this.data.get(key); },
  set(key, value) { this.data.set(key, value); },
};

function withLoggingStore(store) {
  return {
    get(key) {
      console.log('get', key);
      return store.get(key);
    },
    set(key, value) {
      console.log('set', key, value);
      return store.set(key, value);
    },
  };
}

function withTtlStore(store, ttlMs) {
  const expiry = new Map();
  return {
    get(key) {
      if (expiry.get(key) < Date.now()) {
        expiry.delete(key);
        return undefined;
      }
      return store.get(key);
    },
    set(key, value) {
      expiry.set(key, Date.now() + ttlMs);
      return store.set(key, value);
    },
  };
}

const store = withLoggingStore(withTtlStore(memoryStore, 60_000));
```

The decorators share the interface, so code that uses `store` cannot tell how many layers wrap it.

### Class-based decorators

```js
class Coffee {
  cost() { return 3; }
  description() { return 'Coffee'; }
}

class MilkDecorator {
  #inner;
  constructor(inner) { this.#inner = inner; }
  cost() { return this.#inner.cost() + 0.5; }
  description() { return `${this.#inner.description()}, milk`; }
}

class SugarDecorator {
  #inner;
  constructor(inner) { this.#inner = inner; }
  cost() { return this.#inner.cost() + 0.2; }
  description() { return `${this.#inner.description()}, sugar`; }
}

const order = new SugarDecorator(new MilkDecorator(new Coffee()));
order.description();   // 'Coffee, milk, sugar'
order.cost();          // 3.7
```

### Delegating the rest with `Proxy`

Writing a forwarding method for every member is tedious. A `Proxy` can decorate **all** methods at once:

```js
function traceAll(target, log = console.log) {
  return new Proxy(target, {
    get(obj, prop, receiver) {
      const value = Reflect.get(obj, prop, receiver);
      if (typeof value !== 'function') return value;
      return function (...args) {
        log(`${String(prop)}(`, ...args, ')');
        return value.apply(obj, args);       // bind to the real object (important for private fields)
      };
    },
  });
}

const traced = traceAll(new Map());
traced.set('a', 1);        // set( a 1 )
traced.get('a');           // get( a )
```

Calling `value.apply(obj, args)` (not `receiver`) matters for built-ins and classes with `#private` fields, which fail if `this` is the proxy. See [Proxy and Reflect](../09_built-in-objects/11_proxy-and-reflect.md).

## Middleware: decorators in a chain

Express, Koa, Redux, and many frameworks use the idea as **middleware**: each layer wraps the next handler.

```js
const logger = (next) => async (req) => {
  console.log('→', req.url);
  const res = await next(req);
  console.log('←', res.status);
  return res;
};

const auth = (next) => async (req) => {
  if (!req.headers.authorization) return { status: 401, body: 'Unauthorized' };
  return next(req);
};

const handler = async (req) => ({ status: 200, body: 'Hello' });

const app = logger(auth(handler));
await app({ url: '/', headers: {} });      // → /  ← 401
```

Each middleware may run code before, after, short-circuit, or pass through. Compare with the [composition](../07_functional-programming/04_composition-and-pipe.md) idea.

## Decorators vs inheritance

| | Inheritance | Decorator |
|---|-------------|-----------|
| When combined | At design time (class hierarchy) | At runtime (wrapping) |
| Number of classes for n features | Up to 2ⁿ | n |
| Add or remove behavior later | Hard | Easy |
| Coupling | Tight to parent | Loose: depends only on the interface |
| Order | Fixed by hierarchy | Chosen by the wrapper order |

## The `@decorator` syntax (TypeScript and JavaScript)

Modern JavaScript (ECMAScript decorators, Stage 3, supported by TypeScript 5+ and some toolchains; check your target) gives special syntax for decorating classes and class members. A **method decorator** receives the original method and returns a replacement:

```js
function logged(originalMethod, context) {
  const name = String(context.name);
  return function (...args) {
    console.log(`→ ${name}`, args);
    const result = originalMethod.apply(this, args);
    console.log(`← ${name}`, result);
    return result;
  };
}

class Calculator {
  @logged
  add(a, b) { return a + b; }
}

new Calculator().add(1, 2);
```

This is the same wrap-and-delegate idea. Without the syntax, you apply it manually:

```js
class Calculator {
  add(a, b) { return a + b; }
}
Calculator.prototype.add = logged(Calculator.prototype.add, { name: 'add' });
```

Notes:

- Syntax and semantics differ between the **legacy** (TypeScript `experimentalDecorators`) and the **standard** decorators; they are not compatible. Check your tooling's configuration
- Decorators hide behavior behind annotations, which makes control flow less visible; use them for cross-cutting concerns (logging, validation, authorization, DI registration), not core logic
- Frameworks such as Angular and NestJS rely on them heavily

## Preserving metadata

Wrappers should look like the original when it matters:

```js
function withLogging(fn) {
  const wrapped = function (...args) { /* ... */ return fn.apply(this, args); };

  Object.defineProperty(wrapped, 'name', { value: fn.name, configurable: true });
  Object.defineProperty(wrapped, 'length', { value: fn.length, configurable: true });
  return wrapped;
}
```

Debugging (stack traces, `fn.name`) and libraries that inspect arity (`fn.length`) benefit from this. Copy custom properties too if the original function carries any.

## When to use it

| Cross-cutting concern | Decorator example |
|----------------------|-------------------|
| Logging and tracing | `withLogging`, request logging middleware |
| Performance | Timing, metrics, memoization, caching |
| Resilience | Retry, timeout, circuit breaker, rate limit |
| Security | Authentication and authorization checks |
| Validation | Check inputs before calling the real function |
| Transformation | Serialize/deserialize, compress, encrypt |
| Debounce/throttle | Control how often a function runs |
| Feature flags | Run the new or old implementation |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Dropping `this` or arguments in the wrapper | Broken behavior in methods | `fn.apply(this, args)` with `...args` |
| Forgetting to return the result (or the promise) | Callers get `undefined`; errors lost | Always return; use `async`/`await` consistently |
| Mixing sync and async wrappers | A sync wrapper breaks an async function's error handling | Make the wrapper `async` when the target may be async |
| Wrong order of stacking | Caching outside auth, retry outside timeout, etc. give different results | Decide order deliberately and test it |
| Changing the interface | Callers break; it is no longer a decorator | Keep the same signature and return type |
| Wrapping with `Proxy` and calling methods on the proxy | Private fields throw (`#x` on a proxy) | Call with the real target as `this` |
| Memoizing impure functions or using unbounded caches | Wrong results, memory growth | Only pure functions; bound the cache |
| Hidden behavior from stacked `@decorators` | Hard to debug | Keep decorators few, documented, and orthogonal |
| Lost function metadata (`name`, `length`) | Confusing stack traces | Copy metadata when it matters |
| Decorating where a plain function call would do | Needless indirection | Add behavior directly if it is used once |
| Mixing legacy and standard decorator syntax | Build or runtime errors | Configure the toolchain consistently |

## Key takeaways

- A decorator wraps an object or function to add behavior while keeping the same interface
- In JavaScript, function decorators are just higher-order functions: forward `this`, arguments, and return values
- Stack decorators to combine behaviors at runtime and avoid subclass explosions; order matters
- Wrap objects by delegating methods, or use a `Proxy` to decorate many methods at once
- Middleware chains (Express, Koa, Redux) apply the same idea to request handlers
- The `@decorator` syntax is sugar for applying the pattern to classes and methods, and has legacy and standard variants
- Use decorators for cross-cutting concerns such as logging, caching, retry, timeout, and auth, not for core logic

**Next:** [Dependency Injection](./09_dependency-injection.md)
