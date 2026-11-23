# Common JavaScript Gotchas & Pitfalls Cheatsheet

> Quick-reference guide covering the 15 most notorious JavaScript edge cases, unexpected runtime behaviors, Automatic Semicolon Insertion quirks, and defensive patterns.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Level** | Reference / Quick Lookup |

---

## 1. Top 15 JavaScript Gotchas at a Glance

| # | Gotcha Expression | Observed Behavior | Underlying Specification Cause | Defensive Fix |
| :--- | :--- | :--- | :--- | :--- |
| **1** | `[10, 2, 5].sort()` | `[10, 2, 5]` | Default sort converts elements to strings | Always pass comparator: `(a, b) => a - b` |
| **2** | `Math.min() > Math.max()` | `true` | `Math.min() === +Infinity`, `Math.max() === -Infinity` | Verify input array length first |
| **3** | `return \n { a: 1 }` | Returns `undefined` | Automatic Semicolon Insertion (ASI) inserts `;` after `return` | Place opening brace on same line: `return {` |
| **4** | `typeof null` | `'object'` | Legacy 1995 3-bit type tag bug (`000` tag) | Use `val === null` for null check |
| **5** | `NaN === NaN` | `false` | IEEE 754 float rule: NaN is unequal to all values | Use `Number.isNaN(val)` or `Object.is(val, NaN)` |
| **6** | `new Date(2026, 1, 1)` | February 1, 2026 | Months are 0-indexed (`0 = Jan`, `1 = Feb`) | Use ISO date strings: `new Date('2026-02-01')` |
| **7** | `0.1 + 0.2 === 0.3` | `false` | IEEE 754 binary floating-point mantissa rounding | Use `Math.abs(a - b) < Number.EPSILON` |
| **8** | `parseInt("08")` | `8` (in modern ES5+; was `0` in ES3) | Legacy engines interpreted leading zero as octal | Always provide explicit radix: `parseInt(str, 10)` |
| **9** | `Array(3).map(fn)` | `[ <3 empty items> ]` | `Array(n)` creates empty holes; `map` ignores holes | Use `Array.from({ length: 3 }, fn)` |
| **10**| `JSON.stringify({ a: undefined })`| `"{}"` | `JSON.stringify` omits `undefined`, functions, symbols | Use `null` instead of `undefined` for JSON payloads |
| **11**| `finally { return 42; }` | Overrides try return | A `finally` return or throw preempts pending try returns | Avoid `return` inside `finally` blocks |
| **12**| `arr.length = 0` | Wipes array in-place | Writing to `length` directly mutates internal storage | Useful for in-place truncation, but beware shared refs |
| **13**| `["10", "10", "10"].map(parseInt)` | `[10, NaN, 2]` | `map` passes `(element, index)`; `parseInt` uses index as radix | Use unary arrow: `map((x) => parseInt(x, 10))` |
| **14**| `{ [a]: 1, [b]: 2 }` | Key collision `"[object Object]"` | Plain objects coerce to string `"[object Object]"` | Use `Map` if keys are non-primitive objects |
| **15**| `1 < 2 < 3` vs `3 > 2 > 1` | `true`, but `false` | Evaluated left-to-right: `(3 > 2) > 1` $\to$ `true > 1` $\to$ `1 > 1` $\to$ `false` | Never chain inequalities; use `a > b && b > c` |

---

## 2. Deep Dive: Automatic Semicolon Insertion (ASI) Pitfall

```javascript
// WRONG: Returns undefined because of ASI
function getUser() {
  return
  {
    name: 'Alice'
  };
}

// CORRECT: Opening brace must reside on the same line
function getUserCorrect() {
  return {
    name: 'Alice'
  };
}
```

---

## 3. Deep Dive: `parseInt` with Array.prototype.map

```javascript
// BUG: map passes 3 arguments: (item, index, array)
// parseInt(string, radix) interprets index as radix!
['10', '10', '10'].map(parseInt);
// Call 1: parseInt('10', 0) -> 10 (radix 0 defaults to 10)
// Call 2: parseInt('10', 1) -> NaN (radix 1 is invalid)
// Call 3: parseInt('10', 2) -> 2 (binary '10' is 2)

// SOLUTION:
['10', '10', '10'].map(Number); // [10, 10, 10]
['10', '10', '10'].map((val) => parseInt(val, 10)); // [10, 10, 10]
```

---

## 4. Deep Dive: `try / catch / finally` Control Flow

```javascript
function dangerousReturn() {
  try {
    return 1;
  } finally {
    return 2; // DANGER: Overrides the return value from the try block!
  }
}

console.log(dangerousReturn()); // 2
```
