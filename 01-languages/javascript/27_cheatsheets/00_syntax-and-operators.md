# Modern JavaScript Syntax & Operators Cheatsheet

> Quick-reference guide covering ECMAScript declarations, assignment expressions, logical operators, nullish coalescence, optional chaining, and bitwise arithmetic.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Level** | Reference / Quick Lookup |

---

## 1. Variable Declarations

| Keyword | Scope | Hoisted? | Reassignable? | Redeclarable? | TDZ? |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `const` | Block `{}` | Yes (Uninitialized) | No | No | Yes |
| `let` | Block `{}` | Yes (Uninitialized) | Yes | No | Yes |
| `var` | Function/Global | Yes (Initialized `undefined`) | Yes | Yes | No |

```javascript
const PI = 3.14159;          // Immutable binding
let count = 0; count++;      // Mutable binding
```

---

## 2. Modern Logical & Nullish Operators

| Operator | Name | Syntax | Evaluation Rule |
| :--- | :--- | :--- | :--- |
| `??` | Nullish Coalescing | `a ?? b` | Returns `b` **only** if `a` is `null` or `undefined` |
| `??=` | Logical Nullish Assignment | `a ??= b` | Assigns `b` to `a` only if `a` is `null` or `undefined` |
| `&&=` | Logical AND Assignment | `a &&= b` | Assigns `b` to `a` only if `a` is truthy |
| `\|\|=` | Logical OR Assignment | `a \|\|= b` | Assigns `b` to `a` only if `a` is falsy |
| `?.` | Optional Chaining | `obj?.prop`, `fn?.()` | Short-circuits to `undefined` if receiver is `null`/`undefined` |
| `?.[expr]` | Optional Bracket Access | `arr?.[index]` | Safely accesses index without throwing TypeError |

```javascript
// Examples:
const port = userPort ?? 8080;         // 0 is preserved! (unlike userPort || 8080)
config.timeout ??= 5000;              // Default if unset
const city = user?.address?.city;     // Safe deep navigation
const first = list?.[0];               // Safe element access
```

---

## 3. Comparison & Equality Operators

| Expression | Evaluates To | Specification Logic |
| :--- | :--- | :--- |
| `===` | Strict Equality | No coercion; returns `false` if types differ; `NaN === NaN` is `false` |
| `!==` | Strict Inequality | Inversion of `===` |
| `==` | Loose Equality | Abstract Equality with `ToNumber`/`ToPrimitive` coercion |
| `Object.is(a, b)` | SameValue | Identical to `===` except `Object.is(NaN, NaN) === true` and `Object.is(+0, -0) === false` |

---

## 4. Spread & Rest Syntax (`...`)

```javascript
// Rest Parameters (Function arguments into array)
function sum(multiplier, ...numbers) {
  return numbers.map((n) => n * multiplier);
}

// Spread Array (Expands iterable into elements)
const combined = [0, ...[1, 2, 3], 4]; // [0, 1, 2, 3, 4]

// Spread Object (Shallow property clone & merge)
const defaults = { theme: 'dark', notifications: true };
const userConfig = { ...defaults, notifications: false }; // Override
```

---

## 5. Destructuring Syntax

```javascript
// Object Destructuring with Renaming and Defaults
const { name: userName, role = 'guest', ...extra } = user;

// Array Destructuring with Skipping and Rest
const [first, , third, ...tail] = [10, 20, 30, 40, 50];

// Nested & Parameter Destructuring
function render({ user: { id, profile: { avatar } } }) {
  return `<img src="${avatar}" id="${id}" />`;
}
```

---

## 6. Bitwise & Math Operators

| Operator | Name | Syntax | Coercion Behavior |
| :--- | :--- | :--- | :--- |
| `&` | AND | `a & b` | Coerces to 32-bit signed integer |
| `\|` | OR | `a \| b` | Coerces to 32-bit signed integer |
| `^` | XOR | `a ^ b` | Coerces to 32-bit signed integer |
| `~` | NOT | `~a` | Inverts bits (`-(a + 1)`) |
| `<<` | Left Shift | `a << b` | Shifts bits left, fills with zeros |
| `>>` | Sign-propagating Right Shift | `a >> b` | Preserves sign bit |
| `>>>` | Zero-fill Right Shift | `a >>> b` | Unsigned 32-bit integer |
| `**` | Exponentiation | `2 ** 3` | Equivalent to `Math.pow(2, 3)` $\to 8$ |
| `**=` | Exponentiation Assignment | `x **= 2` | Square in-place |
