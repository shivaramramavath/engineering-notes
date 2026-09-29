# IIFE and Function Properties

Functions are objects, so they have properties and methods. An **IIFE** is a function that runs the moment it is defined.

## IIFE (Immediately Invoked Function Expression)

```js
(function () {
  const secret = "private";
  console.log("runs immediately");
})();

(() => {
  console.log("arrow IIFE");
})();

const result = (function (a, b) {
  return a + b;
})(2, 3); // 5
```

The parentheses turn the `function` keyword into an **expression**.

## Why IIFEs existed

| Use                                     | Today's alternative                  |
| --------------------------------------- | ------------------------------------ |
| Avoid global variables (function scope) | `let`/`const` in a block, ES modules |
| Module pattern (private state)          | ES modules, classes, closures        |
| Run async setup                         | top-level `await` in modules         |

```js
const counter = (() => {
  let count = 0;
  return { inc: () => ++count, get: () => count };
})();
```

Async IIFE (for scripts without top-level `await`):

```js
(async () => {
  const data = await load();
  console.log(data);
})();
```

## Built-in function properties

| Property     | Meaning                                               | Example                           |
| ------------ | ----------------------------------------------------- | --------------------------------- |
| `name`       | function name                                         | `add.name` → `"add"`              |
| `length`     | number of parameters before the first default or rest | `((a, b = 1) => {}).length` → `1` |
| `prototype`  | object used by `new` (regular functions and classes)  |                                   |
| `toString()` | source text                                           | `add.toString()`                  |

```js
function add(a, b) {
  return a + b;
}
add.name; // "add"
add.length; // 2

const sub = (a, b) => a - b;
sub.name; // "sub" (inferred from the variable)
```

## Custom properties

Functions can hold data.

```js
function visit() {
  visit.count++;
}
visit.count = 0;
visit();
visit();
visit.count; // 2
```

Useful for simple counters or caches, but prefer closures for real state.

## `call`, `apply`, `bind`

```js
function intro(greeting, punct) {
  return `${greeting}, ${this.name}${punct}`;
}
const user = { name: "Ada" };

intro.call(user, "Hi", "!"); // arguments listed
intro.apply(user, ["Hi", "!"]); // arguments as an array
const bound = intro.bind(user, "Hi");
bound("?"); // "Hi, Ada?"
```

Full treatment in `05_this-and-oop/02_call-apply-bind.md`.

## Constructor calls and `new.target`

```js
function Person(name) {
  if (!new.target) return new Person(name); // safe to call without new
  this.name = name;
}
```

## Getting function info at runtime

```js
typeof fn === "function";
fn instanceof Function;
fn.constructor === Function; // true for normal functions
Object.getPrototypeOf(async function () {}).constructor.name; // "AsyncFunction"
```

## Pitfalls

| Pitfall                                                 | Why it hurts                  | Better                  |
| ------------------------------------------------------- | ----------------------------- | ----------------------- |
| Missing leading `;` before an IIFE in no-semicolon code | Previous line merges with `(` | Use semicolons          |
| Using IIFEs for scoping in modules                      | Unnecessary                   | Block scope and modules |
| Relying on `fn.length` with defaults/rest               | Counts fewer parameters       | Do not depend on it     |
| Storing state on function properties                    | Hidden global-like state      | Closures or classes     |
| Depending on `name` after minification                  | Names change                  | Never use it for logic  |

## Key takeaways

- IIFEs run immediately and create a private scope; modules and blocks mostly replace them
- Functions are objects with `name`, `length`, `call`, `apply`, `bind`
- Use closures or classes for state instead of properties on functions

**Next:** [Recursion](./08_recursion.md)
