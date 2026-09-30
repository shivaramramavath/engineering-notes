# Currying and Partial Application

Both techniques turn a multi-argument function into something you can **configure step by step**. They are related but different.

| | Currying | Partial application |
|---|----------|--------------------|
| Definition | Transform `f(a, b, c)` into `f(a)(b)(c)` | Fix some arguments now, supply the rest later |
| Each call takes | exactly **one** argument | any number |
| Result | chain of unary functions | function of the remaining arguments |

```js
const add = (a, b, c) => a + b + c;

const curried = (a) => (b) => (c) => a + b + c;
curried(1)(2)(3);                    // 6

const add1 = add.bind(null, 1);      // partial application
add1(2, 3);                          // 6
```

## Manual currying

```js
const multiply = (a) => (b) => a * b;

const double = multiply(2);
const triple = multiply(3);
[1, 2, 3].map(double);               // [2, 4, 6]
```

## Generic `curry`

```js
function curry(fn) {
  return function curried(...args) {
    if (args.length >= fn.length) return fn.apply(this, args);
    return (...next) => curried.apply(this, [...args, ...next]);
  };
}

const add3 = curry((a, b, c) => a + b + c);
add3(1)(2)(3);       // 6
add3(1, 2)(3);       // 6
add3(1)(2, 3);       // 6
add3(1, 2, 3);       // 6
```

It uses `fn.length` (the declared parameter count), so it does **not** work with default or rest parameters.

## Partial application helper

```js
const partial = (fn, ...preset) => (...rest) => fn(...preset, ...rest);
const partialRight = (fn, ...preset) => (...rest) => fn(...rest, ...preset);

const greet = (greeting, name) => `${greeting}, ${name}!`;
const hello = partial(greet, "Hello");
hello("Ada");                        // "Hello, Ada!"
```

Alternatives: `fn.bind(null, ...preset)` or a plain arrow `(name) => greet("Hello", name)`.

## Why it is useful

```js
// Reusable, specialized functions
const log = (level) => (message) => console.log(`[${level}] ${message}`);
const info = log("info");
const error = log("error");

// Data-last helpers that compose well
const map = (fn) => (list) => list.map(fn);
const prop = (key) => (obj) => obj[key];
const names = map(prop("name"));
names(users);

// Event handlers with context
const handleClick = (id) => (event) => select(id, event);
list.forEach((item) => el.addEventListener("click", handleClick(item.id)));

// Configuration once, use many times
const fetchJson = (baseUrl) => (path) => fetch(baseUrl + path).then((r) => r.json());
const api = fetchJson("https://api.example.com");
api("/users");
```

## Argument order matters

Put the arguments that change **least** first and the **data** last.

```js
const filterBy = (pred) => (list) => list.filter(pred);     // configuration first, data last
const isAdult = (u) => u.age >= 18;
const adults = filterBy(isAdult);
adults(users);
```

Compare to `filter(list, pred)`, which cannot be partially applied by config easily.

## Flip and other adapters

```js
const flip = (fn) => (a, b, ...rest) => fn(b, a, ...rest);
const unary = (fn) => (x) => fn(x);
const nAry = (n, fn) => (...args) => fn(...args.slice(0, n));

["1", "2", "3"].map(unary(parseInt));    // [1, 2, 3]  (fixes the index trap)
```

## Placeholders

Some libraries (Ramda `R.__`, lodash `_`) let you skip arguments.

```js
const _ = Symbol("placeholder");
function partialP(fn, ...preset) {
  return (...later) => {
    let i = 0;
    const args = preset.map((p) => (p === _ ? later[i++] : p));
    return fn(...args, ...later.slice(i));
  };
}
const sub = (a, b) => a - b;
partialP(sub, _, 10)(50);   // 40
```

## Currying vs default parameters vs options objects

| Need | Best tool |
|------|-----------|
| Reuse a function with fixed first args | Partial application / currying |
| Optional settings | Default parameters or options object |
| Many named parameters | Options object |
| Composition-friendly unary functions | Currying |

Currying shines in pipelines; options objects shine for public APIs with many settings. Do not force currying onto APIs where callers will pass all arguments at once.

## Performance notes

- Each curried call creates closures: negligible for normal use, noticeable in tight loops
- Generic `curry` adds overhead; write critical hot paths by hand
- Partial application via `bind` is fast and simple

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `curry` on functions with defaults/rest | `fn.length` is wrong | Explicit arity: `curry(fn, arity)` |
| Passing extra args to curried fn | Result is called earlier than expected | Wrap with `unary` |
| Confusing currying and partial application | Wrong mental model | Currying = one arg at a time |
| Bad argument order | Cannot partially apply usefully | Config first, data last |
| Over-currying public APIs | Awkward `f(a)(b)(c)` for callers | Offer both forms or keep normal signatures |
| Currying methods that use `this` | Context lost | Use `apply(this, ...)` (as in `curry` above) or arrows |
| Debugging deeply nested closures | Stack traces less clear | Name inner functions |

## Key takeaways

- Currying: `f(a, b, c)` becomes `f(a)(b)(c)`; partial application fixes some arguments
- Order arguments so configuration comes first and data comes last
- Use them to create specialized functions and composable pipelines
- Keep public APIs simple; curry internally where it helps

**Next:** [Point-Free Style](./06_point-free.md)
