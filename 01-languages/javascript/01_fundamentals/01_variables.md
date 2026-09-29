# Variables

A variable is a **named binding** to a value. JavaScript has three ways to declare one: `var`, `let` and `const`.

```
name  ─────►  value
"age"         36
```

## The three declarations

|                                                      | `var`                           | `let`               | `const`             |
| ---------------------------------------------------- | ------------------------------- | ------------------- | ------------------- |
| Scope                                                | Function                        | Block               | Block               |
| Reassign                                             | Yes                             | Yes                 | **No**              |
| Redeclare in same scope                              | Yes                             | No                  | No                  |
| Hoisted                                              | Yes, initialized to `undefined` | Yes, but in the TDZ | Yes, but in the TDZ |
| Becomes property of global object (top-level script) | Yes                             | No                  | No                  |
| Needs an initial value                               | No                              | No                  | **Yes**             |

```js
var a = 1;
let b = 2;
const c = 3;

b = 20; // fine
c = 30; // TypeError: Assignment to constant variable
```

## Default rule

1. Use **`const`** by default
2. Use **`let`** when the value must change
3. Avoid **`var`** in new code

## `const` does not mean immutable

`const` locks the **binding**, not the value it points to.

```js
const user = { name: "Ada" };
user.name = "Grace"; // allowed, the object is mutated
user = {}; // TypeError, the binding cannot change

const frozen = Object.freeze({ name: "Ada" });
frozen.name = "Grace"; // silently ignored (throws in strict mode)
```

`Object.freeze` is shallow. Nested objects stay mutable.

## Block scope vs function scope

```js
if (true) {
  var x = 1;
  let y = 2;
}
console.log(x); // 1  (var ignores blocks)
console.log(y); // ReferenceError
```

The classic loop trap:

```js
for (var i = 0; i < 3; i++) setTimeout(() => console.log(i)); // 3 3 3
for (let j = 0; j < 3; j++) setTimeout(() => console.log(j)); // 0 1 2
```

`let` creates a fresh binding per iteration. `var` shares one.

## Hoisting and the temporal dead zone

Declarations are registered before code runs.

```js
console.log(a); // undefined  (var is hoisted and initialized)
var a = 5;

console.log(b); // ReferenceError: Cannot access 'b' before initialization
let b = 5;
```

The time between entering the scope and reaching the `let`/`const` line is the **temporal dead zone (TDZ)**. Details are in `04_scope-and-execution/04_hoisting-and-tdz.md`.

## Naming rules

- Letters, digits, `_` and `$`; cannot start with a digit
- Case sensitive (`user` and `User` differ)
- Cannot be a reserved word (`class`, `return`, `let` in some contexts, ...)
- Unicode letters are allowed, but stay with ASCII for readability

| Convention         | Used for                                     |
| ------------------ | -------------------------------------------- |
| `camelCase`        | variables, functions                         |
| `PascalCase`       | classes, constructors                        |
| `UPPER_SNAKE_CASE` | true constants (`MAX_RETRIES`)               |
| `_name` / `#name`  | "private" by convention / real private field |

## Multiple declarations and shadowing

```js
let a = 1,
  b = 2; // legal, but one per line reads better

let x = 10;
{
  let x = 20; // shadows the outer x
  console.log(x); // 20
}
console.log(x); // 10
```

## Accidental globals

```js
function leak() {
  total = 5; // no declaration: creates a global in sloppy mode
}
```

In strict mode (and in ES modules) this throws a `ReferenceError`. Always declare.

## Variable pitfalls

| Pitfall                                | Why it hurts                        | Better                            |
| -------------------------------------- | ----------------------------------- | --------------------------------- |
| Using `var` in loops                   | Shared binding, surprising closures | `let`                             |
| Assuming `const` = frozen              | Objects still mutate                | `Object.freeze` or copy on change |
| Reading before declaration             | TDZ `ReferenceError`                | Declare at the top of the scope   |
| Implicit globals                       | Hard-to-trace bugs                  | `"use strict"` or modules         |
| Reusing one variable for many meanings | Confusing, type changes             | One purpose per variable          |

## Key takeaways

- `const` first, `let` when needed, `var` almost never
- `let` and `const` are block scoped and live in the TDZ before their declaration
- `const` protects the binding, not the object's contents
- Declare everything; never rely on implicit globals

**Next:** [Data Types](./02_data-types.md)
