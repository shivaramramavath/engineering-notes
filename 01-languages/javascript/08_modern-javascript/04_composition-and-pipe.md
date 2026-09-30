# Composition and Pipe

**Function composition** combines small functions so that the output of one becomes the input of the next.

```
compose(f, g)(x)  =  f(g(x))      // right to left (math order)
pipe(f, g)(x)     =  g(f(x))      // left to right (reading order)
```

## Manual composition

```js
const trim = (s) => s.trim();
const lower = (s) => s.toLowerCase();
const dash = (s) => s.replace(/\s+/g, "-");

const slug = (s) => dash(lower(trim(s)));      // nested calls: reads inside-out
slug("  Hello World  ");                        // "hello-world"
```

## `compose` and `pipe`

```js
const compose = (...fns) => (x) => fns.reduceRight((acc, fn) => fn(acc), x);
const pipe    = (...fns) => (x) => fns.reduce((acc, fn) => fn(acc), x);

const slugify = pipe(trim, lower, dash);
slugify("  Hello World  ");                     // "hello-world"

compose(dash, lower, trim)("  Hello World  "); // same result, reversed order
```

Prefer `pipe`: steps read in execution order.

Multiple arguments for the first function:

```js
const pipeMulti = (first, ...rest) => (...args) => rest.reduce((acc, fn) => fn(acc), first(...args));
const total = pipeMulti((a, b) => a + b, (n) => n * 2, String);
total(1, 2);   // "6"
```

## Composition requirements

| Rule | Reason |
|------|--------|
| Each function takes **one** argument (after the first) | Output feeds a single input |
| Types must line up | Output of step n matches input of step n+1 |
| Functions should be pure | Predictable pipelines |
| Data goes **last** in shared utilities | Enables partial application (data-last) |

Use currying or wrappers to reduce multi-argument functions to unary:

```js
const add = (a) => (b) => a + b;
const multiply = (a) => (b) => a * b;

const calc = pipe(add(1), multiply(10));       // (x + 1) * 10
calc(4);                                       // 50
```

## Data-last arguments

```js
const map = (fn) => (list) => list.map(fn);
const filter = (pred) => (list) => list.filter(pred);
const reduce = (fn, init) => (list) => list.reduce(fn, init);

const totalPaid = pipe(
  filter((o) => o.status === "paid"),
  map((o) => o.total),
  reduce((a, b) => a + b, 0),
);

totalPaid(orders);
```

Libraries built around this: **Ramda**, **lodash/fp**, **fp-ts**.

## Debugging pipelines with `tap`

```js
const tap = (fn) => (x) => (fn(x), x);

const process = pipe(
  trim,
  tap((v) => console.log("after trim:", v)),
  lower,
  tap((v) => console.log("after lower:", v)),
  dash,
);
```

## Async pipelines

```js
const pipeAsync = (...fns) => (x) => fns.reduce((p, fn) => p.then(fn), Promise.resolve(x));

const loadProfile = pipeAsync(
  fetchUser,
  (user) => fetchPosts(user.id),
  (posts) => posts.filter((p) => p.public),
);
await loadProfile(42);
```

## Composing predicates and comparators

```js
const and = (...preds) => (x) => preds.every((p) => p(x));
const or  = (...preds) => (x) => preds.some((p) => p(x));
const not = (pred) => (x) => !pred(x);

const isAdult = (u) => u.age >= 18;
const isActive = (u) => u.active;

users.filter(and(isAdult, isActive));
users.filter(not(isAdult));
```

```js
const by = (key) => (a, b) => (a[key] > b[key]) - (a[key] < b[key]);
const thenBy = (...cmps) => (a, b) => cmps.reduce((r, cmp) => r || cmp(a, b), 0);

users.toSorted(thenBy(by("role"), by("name")));
```

## Composition with methods

```js
const result = [" a ", "b "]
  .map((s) => s.trim())
  .filter(Boolean)
  .map((s) => s.toUpperCase());       // method chaining is composition for a fixed type
```

Method chaining is convenient but limited to the methods the object has. `pipe` works with any functions.

## Pipeline operator (proposal)

```js
// TC39 proposal (not standard yet): hack pipes
const result = "  Hello World " |> trim(%) |> lower(%) |> dash(%);
```

Until it ships, use `pipe`.

## Algebra of composition

- **Associative**: `compose(f, compose(g, h))` equals `compose(compose(f, g), h)`
- **Identity**: `const id = (x) => x;` `compose(f, id)` equals `f`

These laws let you regroup and refactor pipelines safely.

## Practical example

```js
const parseCsvLine = (line) => line.split(",").map((c) => c.trim());
const toRecord = ([name, age, city]) => ({ name, age: Number(age), city });
const isValid = (r) => r.name && !Number.isNaN(r.age);

const parseCsv = pipe(
  (text) => text.split("\n"),
  (lines) => lines.slice(1),                 // drop header
  (lines) => lines.filter(Boolean),
  (lines) => lines.map(parseCsvLine),
  (rows) => rows.map(toRecord),
  (records) => records.filter(isValid),
);
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Multi-argument functions in a pipe | Later steps get only one value | Curry or wrap in arrows |
| Mixed types between steps | Runtime surprises | Name steps, type-check with TS/JSDoc |
| Impure steps hidden in the pipeline | Hard to reason about | Isolate effects, use `tap` deliberately |
| `compose` vs `pipe` confusion | Steps run in reverse | Pick `pipe`, be consistent |
| Very long anonymous pipelines | Hard to debug | Name intermediate functions |
| Async function in a sync `pipe` | Promise passed along | `pipeAsync` |
| `map(fn)` where `fn` takes extra params | Index/array passed unexpectedly | `map((x) => fn(x))` |

## Key takeaways

- Composition builds big behavior from small unary functions
- `pipe` reads left to right; `compose` right to left
- Put data last and curry to make functions composable
- Use `tap` to debug and `pipeAsync` for promises

**Next:** [Currying and Partial Application](./05_currying-and-partial-application.md)
