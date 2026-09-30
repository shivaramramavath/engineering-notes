# Scope

**Scope** decides where a variable is visible. JavaScript uses **lexical (static) scope**: visibility is determined by where code is **written**, not where it is called.

```
global scope
 └─ function scope
     └─ block scope
         └─ inner block scope
```

An inner scope can read outer variables. An outer scope cannot read inner ones.

## Kinds of scope

| Scope | Created by | Holds |
|-------|-----------|-------|
| **Global** | the script itself | top-level `var` and functions (scripts), built-ins |
| **Module** | each ES module file | top-level names, private to the file |
| **Function** | every function call | parameters, `var`, `let`, `const`, inner functions |
| **Block** | `{ }` in `if`, `for`, `while`, `switch`, bare blocks | `let`, `const`, `class` |
| **Catch** | `catch (e)` clause | the error variable |

## Global scope

```js
var a = 1;      // in a classic script: becomes window.a
let b = 2;      // global, but NOT a property of window
const c = 3;

globalThis.a;   // 1 (script)  |  undefined (module)
globalThis.b;   // undefined
```

In **ES modules** and Node files, top-level declarations are **module scoped**, not global.

## Function scope

```js
function demo() {
  var x = 1;
  let y = 2;
}
console.log(typeof x);   // "undefined"
console.log(typeof y);   // "undefined"
```

## Block scope

`let`, `const` and `class` respect blocks. `var` ignores them.

```js
if (true) {
  var v = "var";
  let l = "let";
  const c = "const";
}
console.log(v);   // "var"
console.log(l);   // ReferenceError
```

Loops create a fresh block scope per iteration for `let`:

```js
for (let i = 0; i < 3; i++) {
  setTimeout(() => console.log(i));   // 0 1 2
}
```

## Shadowing

An inner variable can hide an outer one with the same name.

```js
const x = "outer";
function f() {
  const x = "inner";       // shadows
  console.log(x);          // "inner"
}
f();
console.log(x);            // "outer"
```

Rules:

| Case | Allowed? |
|------|----------|
| `let` in inner block shadowing outer `let` | Yes |
| `var` shadowing a `let` in an inner **function** | Yes |
| `var` inside a block when the outer scope has a `let` of the same name | **No** (SyntaxError) |
| Redeclaring `let` in the same scope | **No** (SyntaxError) |

```js
let a = 1;
{
  var a = 2;   // SyntaxError: 'a' has already been declared
}
```

Shadowing is legal but often confusing. Linters can warn (`no-shadow`).

## Lexical scope in action

```js
const name = "global";

function show() {
  console.log(name);       // uses where show was WRITTEN
}

function run() {
  const name = "local";
  show();                  // prints "global"
}
run();
```

## Scope in closures

Inner functions keep access to their outer scope even after the outer function returns.

```js
function counter() {
  let n = 0;
  return () => ++n;
}
const next = counter();
next();   // 1
next();   // 2
```

Full treatment in `06_closures/01_closures.md`.

## Scope and `switch`

All cases share **one** block. Wrap cases in braces to isolate declarations.

```js
switch (kind) {
  case "a": {
    const msg = "A";
    break;
  }
  case "b": {
    const msg = "B";   // no clash thanks to braces
    break;
  }
}
```

## Scope and `eval` / `with` (avoid)

`eval` and `with` change scope resolution dynamically. They defeat optimizations and are forbidden in strict mode (`with`) or discouraged (`eval`).

## Avoiding global pollution

| Technique | Notes |
|-----------|-------|
| ES modules | Best default |
| Block or function scope | Keep names local |
| `const`/`let` instead of `var` | No accidental global properties |
| Strict mode | Assigning an undeclared name throws |
| IIFE | Legacy approach |

```js
function leak() {
  oops = 5;          // sloppy mode: creates a global! strict mode: ReferenceError
}
```

## Scope pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `var` in loops with callbacks | One shared variable | `let` |
| Implicit globals | Hard-to-trace bugs | Strict mode, always declare |
| Assuming `var` is block scoped | Leaks out of blocks | `let` / `const` |
| Deep shadowing | Reads the wrong variable | Distinct names |
| Declaring inside `switch` cases without braces | Shared scope, TDZ errors | Braces per case |
| Relying on globals for state | Coupling, testing pain | Modules and parameters |

## Key takeaways

- JavaScript scope is lexical: decided by where code is written
- `var` is function scoped; `let`, `const`, `class` are block scoped
- Modules have their own top-level scope
- Inner scopes see outer variables, never the reverse
- Always declare variables and use strict mode

**Next:** [Lexical Environment](./02_lexical-environment.md)
