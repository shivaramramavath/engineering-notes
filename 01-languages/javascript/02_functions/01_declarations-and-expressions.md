# Function Declarations and Expressions

A function is a reusable block of code that takes input (parameters), does work, and gives back a result.

```
call site  ──arguments──►  function(parameters)  ──return value──►  call site
```

## Ways to define a function

| Form                      | Example                                          |
| ------------------------- | ------------------------------------------------ |
| Declaration               | `function add(a, b) { return a + b; }`           |
| Expression                | `const add = function (a, b) { return a + b; };` |
| Named expression          | `const add = function sum(a, b) { ... };`        |
| Arrow                     | `const add = (a, b) => a + b;`                   |
| Method shorthand          | `const o = { add(a, b) { return a + b; } };`     |
| Constructor               | `new Function("a", "b", "return a + b")` (avoid) |
| Class / generator / async | `class`, `function*`, `async function`           |

## Declaration

```js
console.log(square(4)); // 16, works before the line (hoisted)

function square(n) {
  return n * n;
}
```

Declarations are **hoisted with their body**, so you can call them earlier in the same scope.

## Expression

```js
console.log(cube(2)); // TypeError: cube is not a function (var) / ReferenceError (const)
const cube = function (n) {
  return n ** 3;
};
```

Function expressions follow the hoisting rules of the variable that holds them: `const` and `let` are in the TDZ, `var` is `undefined`.

## Declaration vs expression

|                         | Declaration         | Expression                           |
| ----------------------- | ------------------- | ------------------------------------ |
| Hoisted with body       | Yes                 | No                                   |
| Needs a name            | Yes                 | Optional                             |
| Can be an IIFE directly | No (needs wrapping) | Yes                                  |
| Typical use             | top-level helpers   | callbacks, assignments, conditionals |

## Named function expressions

The name is visible **only inside** the function. It helps stack traces and recursion.

```js
const fact = function factorial(n) {
  return n <= 1 ? 1 : n * factorial(n - 1);
};
fact(5); // 120
typeof factorial; // "undefined" outside
```

## Functions are values

```js
function greet() {
  return "hi";
}

const alias = greet; // store
const list = [greet, Math.max]; // put in arrays
const obj = { greet }; // property
run(greet); // pass as argument
function makeGreeter() {
  return greet;
} // return
```

## Function scope and block-level functions

Parameters and inner `let`/`const` belong to the function's scope.

```js
function demo() {
  const inside = 1;
}
console.log(typeof inside); // "undefined"
```

Avoid declaring functions inside `if` blocks. Behavior differs between sloppy and strict mode. Use an expression instead:

```js
let handler;
if (fast) {
  handler = () => quick();
} else {
  handler = () => slow();
}
```

## Naming functions

| Convention               | Example                                        |
| ------------------------ | ---------------------------------------------- |
| Verb first               | `getUser`, `calculateTotal`, `parseDate`       |
| Booleans as questions    | `isValid`, `hasAccess`, `canEdit`              |
| Handlers                 | `handleClick`, `onSubmit`                      |
| Factories / constructors | `createStore`, `User` (PascalCase for classes) |

## Method shorthand

```js
const counter = {
  count: 0,
  inc() {
    this.count++;
  }, // shorthand method
  dec: function () {
    this.count--;
  },
};
```

## Function declaration pitfalls

| Pitfall                                    | Why it hurts                           | Better                             |
| ------------------------------------------ | -------------------------------------- | ---------------------------------- |
| Calling a `const` function before its line | TDZ `ReferenceError`                   | Define first, or use a declaration |
| Functions declared inside blocks           | Inconsistent semantics                 | Function expressions               |
| Anonymous callbacks everywhere             | Poor stack traces                      | Name important functions           |
| `new Function(...)`                        | Like `eval`, security and speed issues | Regular functions                  |
| Huge functions doing many jobs             | Hard to test                           | Small single-purpose functions     |

## Key takeaways

- Declarations are hoisted with their body; expressions follow variable rules
- Functions are first-class values
- Name functions clearly, verb first
- Prefer expressions or arrows for callbacks and conditional definitions

**Next:** [Arrow Functions](./02_arrow-functions.md)
