# Higher-Order Functions

A **higher-order function (HOF)** takes a function as an argument, returns a function, or both. This works because functions are first-class values.

```
HOF( fn ) ─────► result          (takes a function)
HOF( x )  ─────► fn              (returns a function)
```

## Built-in HOFs

| Method               | Purpose             |
| -------------------- | ------------------- |
| `map`                | transform each item |
| `filter`             | keep matching items |
| `reduce`             | fold into one value |
| `forEach`            | run side effects    |
| `some` / `every`     | test conditions     |
| `find` / `findIndex` | first match         |
| `sort(compareFn)`    | custom ordering     |
| `flatMap`            | map then flatten    |

```js
const users = [
  { name: "Ada", age: 36, active: true },
  { name: "Linus", age: 17, active: false },
];

const names = users.filter((u) => u.active).map((u) => u.name); // ["Ada"]
const totalAge = users.reduce((sum, u) => sum + u.age, 0); // 53
users.sort((a, b) => a.age - b.age);
```

## Functions that return functions

```js
const multiplyBy = (factor) => (n) => n * factor;

const double = multiplyBy(2);
double(21); // 42
```

This uses a **closure**: the inner function remembers `factor`.

## Writing your own HOFs

```js
function times(n, fn) {
  return Array.from({ length: n }, (_, i) => fn(i));
}
times(3, (i) => i * i); // [0, 1, 4]

function withLogging(fn) {
  return function (...args) {
    console.log("calling", fn.name, args);
    const result = fn(...args);
    console.log("returned", result);
    return result;
  };
}

const loggedAdd = withLogging((a, b) => a + b);
```

## Function decorators (wrappers)

```js
const once = (fn) => {
  let called = false,
    value;
  return (...args) => {
    if (!called) {
      called = true;
      value = fn(...args);
    }
    return value;
  };
};

const debounce = (fn, ms) => {
  let id;
  return (...args) => {
    clearTimeout(id);
    id = setTimeout(() => fn(...args), ms);
  };
};
```

More in `23_real-world-patterns/`.

## Composition preview

```js
const compose =
  (...fns) =>
  (x) =>
    fns.reduceRight((acc, fn) => fn(acc), x);
const pipe =
  (...fns) =>
  (x) =>
    fns.reduce((acc, fn) => fn(acc), x);

const slugify = pipe(
  (s) => s.trim(),
  (s) => s.toLowerCase(),
  (s) => s.replace(/\s+/g, "-"),
);
slugify("  Hello World "); // "hello-world"
```

Deeper coverage in `07_functional-programming/`.

## Sorting comparator rules

```js
[10, 9, 1].sort(); // [1, 10, 9]  (default: string order)
[10, 9, 1].sort((a, b) => a - b); // [1, 9, 10]
```

The comparator returns negative, zero, or positive.

## HOF vs loop

| Prefer HOFs when                   | Prefer a loop when              |
| ---------------------------------- | ------------------------------- |
| Expressing transformations clearly | Early `break` is needed         |
| Chaining steps                     | Performance-critical hot paths  |
| Reusing behavior                   | Complex state across iterations |

## HOF pitfalls

| Pitfall                              | Why it hurts            | Better                     |
| ------------------------------------ | ----------------------- | -------------------------- |
| `map` used for side effects          | Builds an unused array  | `forEach` or `for...of`    |
| `reduce` for everything              | Hard to read            | `map`/`filter` first       |
| Missing initial value in `reduce`    | Fails on empty arrays   | Always pass one            |
| Mutating inside `map`/`filter`       | Surprising side effects | Return new values          |
| `sort` without comparator on numbers | String ordering         | `(a, b) => a - b`          |
| Forgetting `sort` mutates            | Original array changes  | `toSorted()` or copy first |

## Key takeaways

- HOFs accept or return functions; closures make returned functions powerful
- Master `map`, `filter`, `reduce` and write small wrappers for reuse
- Use the right tool: `map` transforms, `forEach` acts
- `sort` needs a numeric comparator and mutates in place

**Next:** [IIFE and Function Properties](./07_iife-and-function-properties.md)
