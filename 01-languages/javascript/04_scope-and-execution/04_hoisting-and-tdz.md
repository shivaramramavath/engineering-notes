# Hoisting and the Temporal Dead Zone

**Hoisting** describes the effect of the creation phase: declarations are registered **before** any code in their scope runs. Code is not physically moved.

## What happens to each declaration

| Declaration | Hoisted? | Initial value | Usable before its line? |
|-------------|----------|---------------|-------------------------|
| `var x` | Yes | `undefined` | Yes (reads `undefined`) |
| `function f() {}` | Yes | the whole function | **Yes** |
| `let` / `const` | Yes | uninitialized (TDZ) | **No**, `ReferenceError` |
| `class C {}` | Yes | uninitialized (TDZ) | **No**, `ReferenceError` |
| `import` | Yes | live binding | Yes (imports are hoisted) |
| Function expression (`const f = function`) | Follows the variable | | No |
| Arrow function in a variable | Follows the variable | | No |

## `var` hoisting

```js
console.log(a);   // undefined
var a = 5;
console.log(a);   // 5
```

Behaves as if written:

```js
var a;
console.log(a);
a = 5;
```

Only the **declaration** is hoisted, not the assignment.

## Function hoisting

```js
sayHi();                    // works
function sayHi() { console.log("hi"); }

sayBye();                   // TypeError: sayBye is not a function
var sayBye = function () {};

sayLater();                 // ReferenceError (const is in the TDZ)
const sayLater = () => {};
```

If a function and a `var` share a name, the function declaration wins at creation time, then the `var` assignment (if any) overwrites it when executed.

## The Temporal Dead Zone (TDZ)

The TDZ is the time between **entering a scope** and **reaching the `let`/`const`/`class` line**. Accessing the name in that period throws.

```js
{
  // TDZ for `x` starts here
  console.log(x);   // ReferenceError: Cannot access 'x' before initialization
  let x = 10;       // TDZ ends
  console.log(x);   // 10
}
```

The TDZ is about **time**, not position:

```js
function read() { return value; }   // fine to define
// read();                          // would throw here (TDZ)
const value = 1;
read();                             // 1
```

## Proof that `let` is hoisted

```js
let x = "outer";
{
  console.log(x);   // ReferenceError, NOT "outer"
  let x = "inner";  // this inner x is hoisted to the top of the block
}
```

If `let` were not hoisted, the first `console.log` would see the outer `x`.

## `typeof` is not always safe

```js
typeof undeclared;      // "undefined"
typeof tdzVar;          // ReferenceError if tdzVar is in its TDZ
let tdzVar = 1;
```

## Default parameters have a TDZ too

```js
function f(a = b, b = 2) {}   // ReferenceError: b used before initialization
f();
```

## Class hoisting

```js
new Person();            // ReferenceError
class Person {}

const p = new Person();  // fine after the declaration
```

## Hoisting order in one scope

1. Function declarations (fully initialized)
2. `var` names (`undefined`)
3. `let`/`const`/`class` names (TDZ)
4. Then code runs top to bottom

## Practical rules

| Rule | Why |
|------|-----|
| Declare variables at the top of their scope | Removes TDZ surprises |
| Use `const`/`let`, never `var` | Block scope, TDZ catches mistakes |
| Define functions before use, or as declarations | Clear reading order |
| Put imports at the top | Conventional and lint-friendly |
| Enable `no-use-before-define` in ESLint | Catches early access |

## Interview classics

```js
var a = 1;
function f() {
  console.log(a);   // undefined (local var a is hoisted)
  var a = 2;
}
f();

console.log(typeof foo);   // "function"
var foo = 1;
function foo() {}
console.log(typeof foo);   // "number"
```

## Hoisting pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Reading `var` before assignment | Silent `undefined` | `let`/`const` |
| Calling a function expression early | `TypeError` / TDZ | Define before use, or use a declaration |
| Assuming `let` is not hoisted | Wrong mental model, confusing errors | Remember the TDZ |
| Function declarations inside blocks | Engine-dependent hoisting in sloppy mode | Function expressions |
| Circular module imports with TDZ | Early access errors | Restructure dependencies |
| Class used before definition | `ReferenceError` | Order declarations |

## Key takeaways

- Every declaration is registered before code runs; initialization differs by kind
- `var` starts as `undefined`, functions are fully available, `let`/`const`/`class` sit in the TDZ
- The TDZ is temporal: it ends when execution reaches the declaration
- Prefer `const`/`let` and declare before use

**Next:** [Strict Mode](./05_strict-mode.md)
