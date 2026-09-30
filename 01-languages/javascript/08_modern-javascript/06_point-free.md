# Point-Free Style

**Point-free** (tacit) style defines functions **without naming their arguments** (the "points"). You build new functions by combining existing ones.

```js
// pointful: mentions the argument `x`
const isEven = (x) => x % 2 === 0;
const evens = (list) => list.filter((x) => isEven(x));

// point-free: no argument names
const evensPF = (list) => list.filter(isEven);
const lengths = (words) => words.map((w) => w.length);      // pointful
const lengthsPF = map(prop("length"));                       // point-free (with helpers)
```

## The basic move: remove the wrapper

If a function just forwards its argument, pass the inner function directly.

```js
users.map((u) => getName(u));    // wrapper
users.map(getName);              // point-free

promise.then((r) => parse(r));
promise.then(parse);

button.addEventListener("click", (e) => handle(e));
button.addEventListener("click", handle);
```

Careful: only safe when arities match (see the `map(parseInt)` trap).

## Building blocks

```js
const pipe = (...fns) => (x) => fns.reduce((v, f) => f(v), x);
const map = (fn) => (list) => list.map(fn);
const filter = (pred) => (list) => list.filter(pred);
const prop = (key) => (obj) => obj[key];
const not = (pred) => (x) => !pred(x);
const equals = (a) => (b) => a === b;
const gt = (n) => (x) => x > n;
const join = (sep) => (list) => list.join(sep);
const split = (sep) => (str) => str.split(sep);
const trim = (s) => s.trim();
const toLower = (s) => s.toLowerCase();
```

## Example

```js
// pointful
const activeAdultNames = (users) =>
  users
    .filter((u) => u.active)
    .filter((u) => u.age >= 18)
    .map((u) => u.name)
    .map((n) => n.toUpperCase());

// point-free
const activeAdultNamesPF = pipe(
  filter(prop("active")),
  filter(pipe(prop("age"), gt(17))),
  map(prop("name")),
  map((n) => n.toUpperCase()),
);
```

Notice the last step still uses a lambda: point-free is a spectrum, not a rule.

```js
const toUpper = (s) => s.toUpperCase();
const activeAdultNamesPF2 = pipe(
  filter(prop("active")),
  filter(pipe(prop("age"), gt(17))),
  map(pipe(prop("name"), toUpper)),
);
```

## Slug example

```js
const slugify = pipe(trim, toLower, split(/\s+/), join("-"));
slugify("  Hello Functional World ");   // "hello-functional-world"
```

Every step is a named, reusable, testable function.

## Advantages

| Benefit | Why |
|---------|-----|
| Less noise | No throwaway variable names |
| Focus on the **shape** of the transformation | Reads as a recipe |
| Encourages small reusable functions | Everything is a named building block |
| Easier composition | Unary functions plug together |

## Limits and readability

Point-free is a **means**, not a goal. Stop when it gets cryptic.

```js
// hard to read
const f = compose(map(pipe(prop("a"), add(1))), filter(pipe(prop("b"), gt(2), not)), sortBy(prop("c")));

// clearer, named steps
const bigEnough = pipe(prop("b"), gt(2));
const smallOnes = filter(not(bigEnough));
const incrementA = map(pipe(prop("a"), add(1)));
const process = pipe(smallOnes, sortBy(prop("c")), incrementA);
```

Good rules:

- Name intermediate functions for domain meaning (`isEligible`, `toInvoice`)
- Use pointful style when it is shorter or clearer
- Do not contort code to avoid a variable

## When point-free is awkward in JavaScript

| Problem | Reason |
|---------|--------|
| Methods (`str.trim()`) | Need wrappers like `trim` above |
| `this`-dependent code | Loses the receiver |
| Variadic callbacks (`map` passes 3 args) | Extra arguments leak in |
| Debugging | Anonymous compositions are harder to step through |
| Stack traces | Names come from where functions are defined |
| Multi-argument functions | Need currying first |

## Fixing arity problems

```js
["1", "2", "3"].map(parseInt);           // [1, NaN, NaN]
["1", "2", "3"].map(unary(parseInt));    // [1, 2, 3]
["1", "2", "3"].map(Number);             // [1, 2, 3]

const unary = (fn) => (x) => fn(x);
```

## Method extraction helpers

```js
const invoke = (method, ...args) => (obj) => obj[method](...args);

["a", "b"].map(invoke("toUpperCase"));                 // ["A", "B"]
["a,b", "c"].map(invoke("split", ","));                // [["a", "b"], ["c"]]
```

## Point-free vs pointful decision

| Situation | Prefer |
|-----------|--------|
| Function just forwards its argument | Point-free (`map(getName)`) |
| Combining 2+ unary functions | `pipe` with named functions |
| Needs multiple arguments in the middle | Pointful arrow |
| Team is new to FP | Pointful with small named helpers |
| Performance-critical loop | Plain loop or pointful code |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Chasing point-free purity | Cryptic code | Stop when it stops helping readability |
| Passing functions with mismatched arity | Extra args cause bugs | `unary`, arrow wrapper |
| Losing `this` when passing methods | Wrong context | Bind or use arrow |
| Deeply nested compositions | Hard to debug | Name intermediate functions, use `tap` |
| Hidden argument order assumptions | Wrong results | Document data-last convention |
| Point-free everything in a mixed team | Onboarding cost | Agree on a style guide |

## Key takeaways

- Point-free style omits argument names by composing and forwarding functions
- Start by replacing `x => f(x)` with `f`
- Curry and data-last helpers make it practical
- Prioritize clarity: name meaningful steps and stop when it gets cryptic

**Next:** [Functional Patterns](./07_functional-patterns.md)
