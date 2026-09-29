# Return Values

Every function call is an expression that evaluates to the returned value.

## Basics

```js
function double(n) {
  return n * 2;
}

const x = double(4); // 8
```

`return` immediately exits the function.

```js
function check(n) {
  if (n < 0) return "negative"; // early exit
  return "ok";
}
```

## Implicit `undefined`

| Situation                             | Result                                        |
| ------------------------------------- | --------------------------------------------- |
| No `return` statement                 | `undefined`                                   |
| `return;`                             | `undefined`                                   |
| Arrow with block body and no `return` | `undefined`                                   |
| Constructor call with `new`           | the new object (unless an object is returned) |

```js
function noReturn() {}
noReturn(); // undefined
```

## The ASI trap

```js
function bad() {
  return;
  {
    ok: true;
  } // ASI inserts a semicolon: returns undefined
}
function good() {
  return { ok: true }; // keep the value on the same line
}
```

## Returning multiple values

A function returns **one** value. Bundle several in an array or object.

```js
function minMax(list) {
  return [Math.min(...list), Math.max(...list)];
}
const [min, max] = minMax([3, 1, 4]);

function parse(input) {
  return { value: Number(input), valid: !Number.isNaN(Number(input)) };
}
const { value, valid } = parse("42");
```

Use an **object** when the values have names, an **array** when position is natural.

## Returning functions

```js
function multiplier(factor) {
  return (n) => n * factor;
}
const triple = multiplier(3);
triple(5); // 15
```

## Returning promises and errors

```js
async function load() {
  return 42;
} // returns Promise<42>

function risky() {
  throw new Error("failed"); // throwing is a different exit path
}
```

Choose one strategy for failure: **throw**, **return a result object** (`{ ok, error }`), or **return `null`/`undefined`** for "not found". Document it and stay consistent.

## Guard clauses

```js
function getDiscount(user) {
  if (!user) return 0;
  if (!user.isMember) return 0;
  return user.years > 5 ? 0.2 : 0.1;
}
```

## Pure return vs side effects

Prefer functions that **compute and return** rather than mutate outside state.

```js
// side effect
let total = 0;
function addToTotal(n) {
  total += n;
}

// pure
const add = (total, n) => total + n;
```

## Return value pitfalls

| Pitfall                            | Why it hurts              | Better                             |
| ---------------------------------- | ------------------------- | ---------------------------------- |
| `return` then newline              | ASI returns `undefined`   | Same-line value                    |
| Forgetting `return` in arrow block | Silent `undefined`        | Concise arrow or explicit `return` |
| Different return types per path    | Callers need many checks  | Consistent shape                   |
| Returning huge mutable internals   | Callers can corrupt state | Return copies or frozen data       |
| Mixing throw and null for errors   | Confusing contracts       | One documented approach            |

## Key takeaways

- No `return` means `undefined`
- One return value: use arrays or objects for many
- Keep `return` and its value on the same line
- Be consistent about how failures are reported

**Next:** [Callbacks](./05_callbacks.md)
