# Data Types

JavaScript is **dynamically typed**: variables have no type, **values** do. The same variable can hold a number now and a string later.

```
primitives (immutable, copied by value)     objects (mutable, copied by reference)
string number bigint boolean                {} [] function Date Map Set RegExp ...
undefined null symbol
```

## The 8 types

| Type                 | Example                         | `typeof`                  |
| -------------------- | ------------------------------- | ------------------------- |
| String               | `"hi"`, `'hi'`, `` `hi` ``      | `"string"`                |
| Number               | `42`, `3.14`, `NaN`, `Infinity` | `"number"`                |
| BigInt               | `9007199254740993n`             | `"bigint"`                |
| Boolean              | `true`, `false`                 | `"boolean"`               |
| Undefined            | `undefined`                     | `"undefined"`             |
| Null                 | `null`                          | `"object"` (historic bug) |
| Symbol               | `Symbol("id")`                  | `"symbol"`                |
| Object               | `{}`, `[]`, `new Date()`        | `"object"`                |
| Function (an object) | `() => {}`                      | `"function"`              |

## `typeof` and its gaps

```js
typeof "a"; // "string"
typeof null; // "object"   (bug kept for compatibility)
typeof []; // "object"
typeof NaN; // "number"
typeof undeclaredVar; // "undefined" (does not throw)
```

Better checks:

```js
value === null; // null
Array.isArray(value); // arrays
Number.isFinite(value); // real numbers only
Object.prototype.toString.call(value); // "[object Date]", "[object Map]", ...
value instanceof Date; // class-based check
```

## Numbers

All numbers are **64-bit IEEE 754 floats**.

```js
0.1 + 0.2; // 0.30000000000000004
Math.abs(0.3 - (0.1 + 0.2)) < Number.EPSILON; // true, safe comparison

Number.MAX_SAFE_INTEGER; // 9007199254740991
Number.MAX_SAFE_INTEGER + 2; // 9007199254740992 (precision lost)

1 / 0; // Infinity
0 / 0; // NaN
NaN === NaN; // false
Number.isNaN(NaN); // true (prefer over global isNaN)
```

Money: store **integers in the smallest unit** (cents) or use a decimal library.

## BigInt

For integers beyond the safe range.

```js
const big = 9007199254740993n;
big + 1n; // 9007199254740994n
big + 1; // TypeError: cannot mix BigInt and Number
typeof big; // "bigint"
```

## Strings

Immutable sequences of UTF-16 code units.

```js
const s = "hello";
s[0] = "H"; // ignored, strings never change
s.toUpperCase(); // returns a NEW string
"😀".length; // 2 (two code units), use [..."😀"].length for 1
```

## `undefined` vs `null`

|                  | `undefined`                                           | `null`                |
| ---------------- | ----------------------------------------------------- | --------------------- |
| Meaning          | "no value assigned yet"                               | "intentionally empty" |
| Set by           | the engine                                            | the programmer        |
| Examples         | uninitialized variable, missing property, no `return` | `let user = null;`    |
| `==` each other  | `true`                                                | `true`                |
| `===` each other | `false`                                               | `false`               |

## Symbols

Unique, non-string keys.

```js
const id = Symbol("id");
const a = { [id]: 1 };
Symbol("x") === Symbol("x"); // false
```

Covered in `08_modern-javascript/03_symbols.md`.

## Value vs reference

Primitives are copied. Objects share a reference.

```js
let a = 1;
let b = a;
b = 2;
console.log(a);         // 1

const o1 = { n: 1 };
const o2 = o1;          // same object
o2.n = 2;
console.log(o1.n);      // 2

{} === {};              // false (different references)
```

To copy an object, see `03_objects-and-arrays/05_copying-and-cloning.md`.

## Wrapper objects

```js
"abc".length; // primitives borrow methods via temporary wrappers
typeof new String("abc"); // "object"  (avoid: new String, new Number, new Boolean)
```

## Data type pitfalls

| Pitfall                      | Why it hurts                | Better                         |
| ---------------------------- | --------------------------- | ------------------------------ |
| `typeof null === "object"`   | Null checks pass as objects | Check `=== null` first         |
| Float math for money         | Rounding errors             | Integers in cents              |
| Mixing BigInt and Number     | `TypeError`                 | Convert explicitly             |
| Comparing objects with `===` | Compares references         | Compare fields or use a helper |
| `new Boolean(false)`         | The object is truthy        | Use primitives                 |

## Key takeaways

- 7 primitives plus objects; functions and arrays are objects
- `typeof` is fine for primitives but wrong for `null` and arrays
- Numbers are floats: never trust `0.1 + 0.2`, and mind `MAX_SAFE_INTEGER`
- Primitives copy by value, objects by reference

**Next:** [Type Conversion and Equality](./03_type-conversion-and-equality.md)
