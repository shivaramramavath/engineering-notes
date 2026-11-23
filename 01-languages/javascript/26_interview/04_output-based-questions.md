# JavaScript Interview Mastery: Output-Based Tricky Questions & Riddles

> An elite collection of 15 high-frequency JavaScript code prediction puzzles, edge-case riddles, and engine execution traces with step-by-step mental models and runtime explanations.

| Metadata | Details |
| :--- | :--- |
| **Track** | JavaScript Interview Preparation (`26_interview`) |
| **Target Role** | Senior / Staff Frontend & Full-Stack Engineer |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Prerequisites** | [00_language-core-qa.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/00_language-core-qa.md), [03_async-and-event-loop-qa.md](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview/03_async-and-event-loop-qa.md) |

---

## Table of Contents
1. [Core Principles of Code Prediction](#1-core-principles-of-code-prediction)
2. [15 Tricky Output Challenges with Deep Explanations](#2-15-tricky-output-challenges-with-deep-explanations)
   - [Puzzle 1: Object Keys & Coercion Collisions](#puzzle-1-object-keys--coercion-collisions)
   - [Puzzle 2: Sparse Arrays & Iteration Holes](#puzzle-2-sparse-arrays--iteration-holes)
   - [Puzzle 3: Parameter Default Values & Scope Shadowing](#puzzle-3-parameter-default-values--scope-shadowing)
   - [Puzzle 4: Promise Thenable Chaining & Return Values](#puzzle-4-promise-thenable-chaining--return-values)
   - [Puzzle 5: Class Field vs Prototype Method Execution](#puzzle-5-class-field-vs-prototype-method-execution)
   - [Puzzle 6: Implicit Type Coercion in Equality](#puzzle-6-implicit-type-coercion-in-equality)
   - [Puzzle 7: Block-Scoped Functions in Sloppy vs Strict Mode](#puzzle-7-block-scoped-functions-in-sloppy-vs-strict-mode)
   - [Puzzle 8: Immediately Invoked Named Function Expressions](#puzzle-8-immediately-invoked-named-function-expressions)
   - [Puzzle 9: Async Function Immediate Return vs Await](#puzzle-9-async-function-immediate-return-vs-await)
   - [Puzzle 10: Array Length Truncation & Delete Operator](#puzzle-10-array-length-truncation--delete-operator)
   - [Puzzle 11: `this` inside Nested Arrow vs Method Call](#puzzle-11-this-inside-nested-arrow-vs-method-call)
   - [Puzzle 12: Float Rounding & Large Integer Overflow](#puzzle-12-float-rounding--large-integer-overflow)
   - [Puzzle 13: Regex Global Flag Statefulness](#puzzle-13-regex-global-flag-statefulness)
   - [Puzzle 14: Object Destructuring Default Aliases](#puzzle-14-object-destructuring-default-aliases)
   - [Puzzle 15: Generator Yield Return Interleaving](#puzzle-15-generator-yield-return-interleaving)
3. [Senior Problem-Solving Checklist](#3-senior-problem-solving-checklist)

---

## 1. Core Principles of Code Prediction

When senior interviewers present output-prediction puzzles, they assess your systematic debugging approach:
1. **Creation Phase vs Execution Phase**: Distinguish identifier allocation (hoisting) from synchronous assignment.
2. **Abstract Operations**: Trace `ToPrimitive`, `ToNumber`, and `ToPropertyKey` step-by-step.
3. **Queue Prioritization**: Delineate Call Stack $\to$ Microtasks $\to$ Rendering $\to$ Macrotasks.

---

## 2. 15 Tricky Output Challenges with Deep Explanations

### Puzzle 1: Object Keys & Coercion Collisions
```javascript
const a = {};
const b = { key: 'b' };
const c = { key: 'c' };

a[b] = 123;
a[c] = 456;

console.log(a[b]);
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
456
```
**Explanation:**
Object property keys in JavaScript must be either strings or symbols. When passing an object `b` as an accessor (`a[b]`), JavaScript invokes the `ToPropertyKey(b)` abstract operation, which calls `b.toString()`. This returns `"[object Object]"`. Both `a[b]` and `a[c]` write to the exact same key: `a["[object Object]"]`. The second write overwrites `123` with `456`.
</details>

---

### Puzzle 2: Sparse Arrays & Iteration Holes
```javascript
const arr = [1, 2, , 4];

console.log(arr.length);
console.log(arr.map((x) => x * 2));
console.log(arr.filter((x) => x === undefined));
console.log(arr.indexOf(undefined));
console.log(arr.includes(undefined));
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
4
[ 2, 4, <1 empty item>, 8 ]
[]
-1
true
```
**Explanation:**
Index `2` contains an empty hole (an unallocated slot), not `undefined`.
- `map()` and `filter()` skip empty slots entirely according to the ECMAScript specification.
- `indexOf()` performs strict equality (`===`) and ignores empty slots, returning `-1`.
- `includes()` uses the `SameValueZero` comparison algorithm, which considers holes equivalent to `undefined`, returning `true`.
</details>

---

### Puzzle 3: Parameter Default Values & Scope Shadowing
```javascript
let x = 1;

function foo(x, y = function () { return x; }) {
  var x = 2;
  console.log(y());
}

foo(3);
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
3
```
**Explanation:**
When function parameters have default expressions, ECMAScript creates an **intermediate parameter scope** between the outer scope and the function body scope.
- In parameter scope: `x = 3`, `y = fn`.
- The function body declares a local variable `var x = 2`, which lives in a separate body environment and does **not** overwrite the parameter scope's `x`.
- When `y()` executes, its lexical parent environment is the parameter scope where `x` is `3`.
</details>

---

### Puzzle 4: Promise Thenable Chaining & Return Values
```javascript
Promise.resolve(1)
  .then((x) => x + 1)
  .then((x) => { throw new Error('boom'); })
  .catch((err) => 100)
  .then((x) => x + 1)
  .then((x) => console.log(x));
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
101
```
**Explanation:**
A `.catch()` handler is simply a `.then(null, onRejected)` handler. When `.catch((err) => 100)` executes, returning `100` recovers the rejected promise chain into a fulfilled state! The subsequent `.then()` receives `100` and produces `101`.
</details>

---

### Puzzle 5: Class Field vs Prototype Method Execution
```javascript
class Base {
  name = 'Base';
  constructor() {
    this.print();
  }
  print() {
    console.log(this.name);
  }
}

class Derived extends Base {
  name = 'Derived';
  print() {
    console.log(this.name);
  }
}

new Derived();
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
undefined
```
**Explanation:**
1. `new Derived()` invokes `Base` constructor via `super()`.
2. Inside `Base` constructor, `this.name = 'Base'` initializes.
3. Next, `this.print()` is called. Because the dynamic receiver is `Derived`, virtual method dispatch calls `Derived.prototype.print()`.
4. However, `Derived`'s own field declarations (`name = 'Derived'`) have **not run yet** (they execute strictly after `super()` completes!).
5. Therefore, `this.name` on the instance at that instant is `undefined`.
</details>

---

### Puzzle 6: Implicit Type Coercion in Equality
```javascript
console.log([] == ![]);
console.log([] == 0);
console.log(['0'] == 0);
console.log([1, 2] == '1,2');
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
true
true
true
true
```
**Explanation:**
- `[] == ![]`: `![]` coerces to boolean `false`. So `[] == false`. `false` converts to number `0`. `[]` converts via `ToPrimitive` to `""`. `"" == 0` converts to `0 == 0` $\to$ `true`.
- `['0'] == 0`: `['0']` coerces via `ToPrimitive` to `"0"`, then `"0"` converts to number `0`. `0 == 0` $\to$ `true`.
- `[1, 2] == '1,2'`: `[1, 2]` coerces to string `'1,2'`. `'1,2' == '1,2'` $\to$ `true`.
</details>

---

### Puzzle 7: Block-Scoped Functions in Sloppy vs Strict Mode
```javascript
{
  function f() { return 1; }
  {
    function f() { return 2; }
  }
}
console.log(typeof f);
```
<details>
<summary><strong>Output & Explanation</strong></summary>

- In **Strict Mode** (`"use strict"`): `undefined` (functions are block-scoped).
- In **Sloppy Mode** (Web Legacy Semantics): `"function"`.
</details>

---

### Puzzle 8: Immediately Invoked Named Function Expressions
```javascript
var f = function foo() {
  foo = 42;
  console.log(typeof foo);
};
f();
console.log(typeof foo);
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
"function"
"undefined"
```
**Explanation:**
In a Named Function Expression (`const f = function foo() {}`), the identifier `foo` is bound exclusively within the function's own internal scope as an **immutable binding** (read-only).
- Attempting to assign `foo = 42` fails silently in sloppy mode (or throws `TypeError` in strict mode). `typeof foo` remains `"function"`.
- `foo` does not leak into the outer scope, so `typeof foo` in the global scope returns `"undefined"`.
</details>

---

### Puzzle 9: Async Function Immediate Return vs Await
```javascript
async function test() {
  return await Promise.resolve(42);
}

console.log(test());
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
Promise { <pending> }
```
**Explanation:**
Every `async` function always returns a Promise immediately upon invocation. The `await` keyword pauses execution inside the function, but the caller receives the wrapping Promise synchronously in the `<pending>` state.
</details>

---

### Puzzle 10: Array Length Truncation & Delete Operator
```javascript
const list = [10, 20, 30, 40];
delete list[1];
console.log(list.length);

list.length = 1;
console.log(list);
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
4
[ 10 ]
```
**Explanation:**
- `delete list[1]` removes the property at index `1`, leaving an empty hole. It does **not** alter the array's `length`, which remains `4`.
- Setting `list.length = 1` deletes all indices $\ge 1$ from the internal array store, truncating the array to `[ 10 ]`.
</details>

---

### Puzzle 11: `this` inside Nested Arrow vs Method Call
```javascript
const obj = {
  count: 10,
  doCount() {
    return {
      count: 20,
      getCount: () => this.count
    };
  }
};

console.log(obj.doCount().getCount());
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
10
```
**Explanation:**
The arrow function `getCount` lexically captures `this` from its enclosing function `doCount`. When `obj.doCount()` is invoked, `this` is `obj`. Therefore, `getCount` will always return `obj.count` (`10`), completely ignoring the nested object's `count: 20`.
</details>

---

### Puzzle 12: Float Rounding & Large Integer Overflow
```javascript
console.log(9007199254740991 + 1 === 9007199254740991 + 2);
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
true
```
**Explanation:**
`9007199254740991` is `Number.MAX_SAFE_INTEGER` ($2^{53} - 1$). Beyond this limit, IEEE 754 64-bit floats lack sufficient mantissa precision to represent every sequential integer, causing consecutive odd numbers to round to the nearest representable even float. Both calculations evaluate to `9007199254740992`.
</details>

---

### Puzzle 13: Regex Global Flag Statefulness
```javascript
const regex = /foo/g;
console.log(regex.test('foo'));
console.log(regex.test('foo'));
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
true
false
```
**Explanation:**
When a `RegExp` has the global (`g`) or sticky (`y`) flag, it stores mutable state in its `lastIndex` property.
1. First test: matches `foo` at index 0, advances `lastIndex` to `3`, returns `true`.
2. Second test: begins searching at index `3`. No subsequent match is found; resets `lastIndex` to `0`, returns `false`.
</details>

---

### Puzzle 14: Object Destructuring Default Aliases
```javascript
const { a: x = 10, b: y = 20 } = { a: null, b: undefined };
console.log(x, y);
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
null 20
```
**Explanation:**
Default values in destructuring are applied **only** when the resolved property is strictly `=== undefined`. Because `a` is `null`, the default is ignored and `x` becomes `null`. Because `b` is `undefined`, the default activates and `y` becomes `20`.
</details>

---

### Puzzle 15: Generator Yield Return Interleaving
```javascript
function* gen() {
  console.log('A');
  yield 1;
  console.log('B');
  return 2;
  console.log('C');
}

const g = gen();
console.log(g.next().value);
console.log(g.next().value);
console.log(g.next().value);
```
<details>
<summary><strong>Output & Explanation</strong></summary>

```
A
1
B
2
undefined
```
**Explanation:**
1. `g.next()`: runs to first `yield 1`, logs `'A'`, yields `{ value: 1, done: false }`.
2. `g.next()`: resumes, logs `'B'`, hits `return 2`, returns `{ value: 2, done: true }`.
3. Subsequent `g.next()`: generator is completed; `'C'` is unreachable; returns `{ value: undefined, done: true }`.
</details>

---

## 3. Senior Problem-Solving Checklist

When answering tricky output questions:
1. Don't rush into guessing: trace execution step-by-step.
2. Identify hoisting declarations first (`var`, `let`, `function`).
3. Check for implicit type coercion conversions.
4. Distinguish macro vs microtask queues.
5. Clarify strict mode vs sloppy mode assumptions.
