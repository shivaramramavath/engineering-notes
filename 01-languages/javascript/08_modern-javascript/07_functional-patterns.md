# Functional Patterns

Reusable structures from functional programming: **functors**, **Maybe**, **Either**, **monads**, plus a few more practical patterns. This is an introduction, aimed at seeing patterns you already use every day.

## Functor: a container you can `map` over

A **functor** is a value in a context that supports `map`, applying a function to the inside without leaving the context.

```js
[1, 2, 3].map((n) => n + 1);          // Array is a functor
Promise.resolve(1).then((n) => n + 1);// (then) behaves like map/flatMap
```

Laws (so `map` behaves predictably):

- **Identity**: `x.map((a) => a)` equals `x`
- **Composition**: `x.map((a) => f(g(a)))` equals `x.map(g).map(f)`

Build your own:

```js
class Box {
  constructor(value) { this.value = value; }
  static of(value) { return new Box(value); }
  map(fn) { return Box.of(fn(this.value)); }
  fold(fn) { return fn(this.value); }
}

Box.of("  Hello ")
  .map((s) => s.trim())
  .map((s) => s.toUpperCase())
  .fold((s) => s);                    // "HELLO"
```

## Maybe: handling missing values

Encode "value or nothing" so that `map` skips work on empty values (no `null` checks scattered everywhere).

```js
class Maybe {
  static of(value) { return value == null ? new Nothing() : new Just(value); }
}
class Just {
  constructor(v) { this.value = v; }
  map(fn) { return Maybe.of(fn(this.value)); }
  chain(fn) { return fn(this.value); }
  getOrElse() { return this.value; }
  fold(_onNothing, onJust) { return onJust(this.value); }
}
class Nothing {
  map() { return this; }
  chain() { return this; }
  getOrElse(fallback) { return fallback; }
  fold(onNothing) { return onNothing(); }
}

const city = Maybe.of(user)
  .map((u) => u.address)
  .map((a) => a.city)
  .getOrElse("Unknown");
```

In modern JS, optional chaining covers the common case:

```js
const city = user?.address?.city ?? "Unknown";
```

Use `Maybe` when you want to **compose** possibly-missing computations as values.

## Either / Result: errors as values

Represent success (`Right`) or failure (`Left`) explicitly instead of throwing.

```js
const Right = (value) => ({
  isRight: true,
  map: (fn) => Right(fn(value)),
  chain: (fn) => fn(value),
  fold: (_l, r) => r(value),
});
const Left = (error) => ({
  isRight: false,
  map: () => Left(error),
  chain: () => Left(error),
  fold: (l) => l(error),
});

const tryCatch = (fn) => {
  try { return Right(fn()); } catch (e) { return Left(e); }
};

const parseJson = (text) => tryCatch(() => JSON.parse(text));

parseJson('{"a":1}')
  .map((o) => o.a)
  .fold(
    (err) => `failed: ${err.message}`,
    (a) => `a is ${a}`,
  );                                  // "a is 1"
```

Plain object alternative (no classes):

```js
const ok = (value) => ({ ok: true, value });
const fail = (error) => ({ ok: false, error });
const result = divide(4, 0);
if (!result.ok) handle(result.error);
```

## Monad: `map` plus `chain` (flatMap)

A **monad** adds `chain` (also `flatMap`, `bind`) so functions that themselves return a wrapped value can be sequenced without nesting.

```js
const safeDivide = (a, b) => (b === 0 ? Left("div by zero") : Right(a / b));

Right(100)
  .chain((n) => safeDivide(n, 5))     // Right(20)
  .chain((n) => safeDivide(n, 0))     // Left("div by zero")
  .chain((n) => safeDivide(n, 2))     // skipped
  .fold(String, String);              // "div by zero"
```

Without `chain` you would get `Right(Right(Right(...)))`.

Monads you already know:

| Type | `map` | `chain` / flatten |
|------|-------|-------------------|
| Array | `map` | `flatMap` |
| Promise | `then(fn)` returning a value | `then(fn)` returning a promise (auto-flattened) |
| Iterator helpers / generators | `map` | `flatMap` |
| Maybe / Either | shown above | `chain` |

Laws (informal): wrapping a value then chaining equals just calling the function (left identity), chaining `of` changes nothing (right identity), and chain order can be regrouped (associativity).

## Applicative (brief)

Apply a wrapped function to wrapped values, useful for combining independent results such as validations.

```js
const validate = (v) => (cond, msg) => (cond ? Right(v) : Left([msg]));
// libraries (fp-ts, folktale, Ramda) provide ap / sequence for collecting all errors
```

`Promise.all` is the applicative pattern for promises: combine independent effects.

## Lazy evaluation and thunks

Wrap computation in a function to delay it.

```js
const thunk = (fn, ...args) => () => fn(...args);
const lazyValue = thunk(expensive, 42);
lazyValue();                          // runs now

function* naturals() { let n = 1; while (true) yield n++; }
const firstFive = naturals().take(5).toArray();   // iterator helpers (ES2025)
```

## Transducers (idea)

Compose `map`/`filter` steps into **one pass** without intermediate arrays.

```js
const mapT = (fn) => (step) => (acc, x) => step(acc, fn(x));
const filterT = (pred) => (step) => (acc, x) => (pred(x) ? step(acc, x) : acc);
const composeT = (...ts) => (step) => ts.reduceRight((s, t) => t(s), step);

const xform = composeT(
  filterT((n) => n % 2 === 0),
  mapT((n) => n * 10),
);
[1, 2, 3, 4].reduce(xform((acc, x) => (acc.push(x), acc)), []);   // [20, 40]
```

Lazy iterator helpers and generators often cover the same need with simpler code.

## Recursion and trampolines, memoization

Covered in `02_functions/08_recursion.md` and `06_closures/02_closure-use-cases.md`: immutability-friendly loops and caching of pure functions.

## Algebraic data types (tagged unions)

```js
const Shape = {
  circle: (r) => ({ type: "circle", r }),
  rect: (w, h) => ({ type: "rect", w, h }),
};

const area = (s) => {
  switch (s.type) {
    case "circle": return Math.PI * s.r ** 2;
    case "rect": return s.w * s.h;
    default: throw new Error(`Unknown shape ${s.type}`);
  }
};
```

Model states explicitly:

```js
const state = { status: "loading" };
// { status: "success", data } | { status: "error", error }
```

## Lenses (idea)

A **lens** bundles a getter and an immutable setter for nested data.

```js
const lens = (get, set) => ({ get, set });
const prop = (k) => lens((o) => o[k], (v, o) => ({ ...o, [k]: v }));
const compose2 = (a, b) => lens((o) => b.get(a.get(o)), (v, o) => a.set(b.set(v, a.get(o)), o));

const addressCity = compose2(prop("address"), prop("city"));
addressCity.set("Paris", user);      // new user object with updated city
```

Libraries: Ramda `lens`, `optics-ts`, Immer for a simpler approach.

## Pattern summary

| Pattern | Solves |
|---------|--------|
| Functor | Transform a value inside a context |
| Maybe | Missing values without null checks |
| Either / Result | Errors as data, composable failure |
| Monad (`chain`) | Sequencing steps that return wrapped values |
| Applicative | Combining independent wrapped values |
| Thunk / lazy | Delay expensive work |
| Transducer | Fuse `map`/`filter` into one pass |
| Tagged union | Explicit, exhaustive states |
| Lens | Immutable updates on nested data |

## Practical guidance

- You do not need a full FP library to benefit: **Result objects**, **pure functions**, **immutable updates** and **pipelines** cover most cases
- Reach for `fp-ts`, `Effect`, `Ramda`, or `neverthrow` when a codebase is committed to the style
- Prefer language features first (`?.`, `??`, `Promise`, iterators) before custom monads
- Keep abstractions small and documented; teammates must be able to read them

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Building monad towers for simple code | Over-engineering | Plain functions, optional chaining |
| Mixing thrown errors and `Either` | Two error channels | Choose one per boundary |
| Forgetting to `fold`/unwrap at the edge | Wrapped values leak everywhere | Unwrap in the impure shell |
| Mutable data inside "functional" wrappers | Breaks the laws | Keep values immutable |
| Custom abstractions without tests | Subtle law violations | Test identity/composition laws |
| Heavy FP libraries for tiny projects | Learning cost | Adopt gradually |

## Key takeaways

- Functors map, monads map and chain; arrays and promises are everyday examples
- Maybe and Either make missing values and errors explicit and composable
- Tagged unions and pure pipelines model state clearly
- Use the smallest abstraction that solves the problem

**Next:** [Modern JavaScript](../08_modern-javascript/00_README.md)
