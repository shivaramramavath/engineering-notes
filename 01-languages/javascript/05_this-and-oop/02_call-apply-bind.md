# JavaScript Explicit Binding: call, apply, bind & Method Borrowing

> In-depth exploration of JavaScript's explicit binding trifecta (`call`, `apply`, `bind`), method borrowing mechanics, partial application, and full ECMAScript-compliant polyfills.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [02_functions/01_declarations-and-expressions.md](../02_functions/01_declarations-and-expressions.md), [05_this-and-oop/01_this.md](./01_this.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES3 (call/apply) / ES5 (bind) / ES2022 (`Object.hasOwn`) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: call vs apply vs bind](#2-core-concepts-call-vs-apply-vs-bind)
3. [How It Works: Method Execution & Context Injection](#3-how-it-works-method-execution--context-injection)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 BoundFunction Exotic Objects](#9-engine-internals-v8-boundfunction-exotic-objects)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Heterogeneous Data Sanitizer via Borrowed Methods](#11-real-world-case-study-heterogeneous-data-sanitizer-via-borrowed-methods)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, functions are first-class objects that exist independently of the objects on which they are invoked. To reuse a function across multiple objects without attaching it as a permanent property, JavaScript provides three explicit binding methods on `Function.prototype`: **`call`**, **`apply`**, and **`bind`**.

Historically, explicit binding was essential for:
1. **Method Borrowing**: Executing array methods (like `slice` or `join`) on array-like objects (`arguments`, `NodeList`).
2. **Safe Property Auditing**: Calling `Object.prototype.hasOwnProperty.call(obj, prop)` to prevent shadowing attacks.
3. **Hard Binding & Partial Application**: Pre-configuring arguments and locking execution contexts for asynchronous callbacks.

Understanding how to control `this` explicitly enables developers to design flexible, highly reusable functional and object-oriented abstractions.

---

## 2. Core Concepts: call vs apply vs bind

```
                      Explicit Binding Comparison
                                   │
      ┌────────────────────────────┴────────────────────────────┐
      ▼                                                         ▼
  Immediate Execution                                    Deferred Execution
  • Executes function right away                          • Returns a new bound function
  • Returns function's return value                       • Executes later when invoked
         │                                                       │
  ┌──────┴──────┐                                                │
  ▼             ▼                                                ▼
call()        apply()                                          bind()
Args passed   Args passed as                                   Pre-binds context +
individually  an Array/array-like                              preset arguments
fn.call(ctx,  fn.apply(ctx,                                    const b = fn.bind(ctx,
  1, 2)         [1, 2])                                          preset1)
```

### Key Differences
- **`call(thisArg, ...args)`**: Invokes the function immediately. Arguments are provided as a comma-separated list.
- **`apply(thisArg, [args])`**: Invokes the function immediately. Arguments are provided as a single array or array-like collection.
- **`bind(thisArg, ...presetArgs)`**: Does *not* invoke the function immediately. Returns a new exotic `BoundFunction` with `this` permanently set to `thisArg`.

---

## 3. How It Works: Method Execution & Context Injection

When `fn.call(ctx, a, b)` executes:
1. The engine checks if `fn` is a callable object (throws `TypeError` if not).
2. If in sloppy mode and `ctx` is `null` or `undefined`, `ctx` is coerced to `globalThis`. If `ctx` is a primitive (e.g. number), it is boxed into an object wrapper (`ToObject(ctx)`).
3. In strict mode, `ctx` is preserved exactly as passed (primitives remain primitives, `null`/`undefined` remain `null`/`undefined`).
4. An execution context is created where `this` is assigned to `ctx`.
5. The function body evaluates with arguments `(a, b)` and returns the resulting value.

---

## 4. Syntax & API Reference

### Signatures
```javascript
// Immediate execution with comma-separated arguments
fn.call(thisArg, arg1, arg2, /* ..., */ argN);

// Immediate execution with array of arguments
fn.apply(thisArg, [argsArray]);

// Deferred execution with optional partial application (currying)
const boundFn = fn.bind(thisArg, arg1, arg2, /* ..., */ argN);
```

### Modern Replacements in ES6+
- Replace `Math.max.apply(null, arr)` with `Math.max(...arr)` (Spread operator).
- Replace `[].slice.call(arguments)` with `Array.from(arguments)` or `[...arguments]`.
- Replace `Object.prototype.hasOwnProperty.call(obj, k)` with `Object.hasOwn(obj, k)` (ES2022).

---

## 5. Practical Examples

### Example 1: Basic Immediate Execution with `call` and `apply`
```javascript
function introduce(greeting, punctuation) {
  return `${greeting}, I am ${this.name}${punctuation}`;
}

const user = { name: "Alan Turing" };

// 1. Using call (individual arguments)
console.log(introduce.call(user, "Hello", "!"));
// Output: "Hello, I am Alan Turing!"

// 2. Using apply (array arguments)
console.log(introduce.apply(user, ["Greetings", "."]));
// Output: "Greetings, I am Alan Turing."
```

### Example 2: Real-World Partial Application via `bind`
```javascript
function log(level, timestamp, message) {
  console.log(`[${level.toUpperCase()}] [${timestamp}] ${message}`);
}

// Partially apply 'level' and 'timestamp'
const infoNow = log.bind(null, "INFO", new Date().toISOString());

infoNow("Database migration started");
infoNow("Indexes rebuilt successfully");
// Output: [INFO] [2026-...] Database migration started
// Output: [INFO] [2026-...] Indexes rebuilt successfully
```

### Example 3: Edge Case with Method Borrowing from Array.prototype
```javascript
// Array-like object lacking slice or join methods
const customList = {
  0: "First",
  1: "Second",
  2: "Third",
  length: 3
};

// Borrow join from Array.prototype:
const formatted = Array.prototype.join.call(customList, " -> ");
console.log("Borrowed join:", formatted);
// Output: "First -> Second -> Third"

// Borrow map to transform elements:
const uppercase = Array.prototype.map.call(customList, item => item.toUpperCase());
console.log("Borrowed map:", uppercase);
// Output: ['FIRST', 'SECOND', 'THIRD']
```

### Example 4: Failure Mode with Modern Arrow Functions
```javascript
// Arrow functions do NOT have their own this binding
const arrowGreeter = () => {
  return `User: ${this?.name ?? "Global/Undefined"}`;
};

const profile = { name: "Ada Lovelace" };

// Attempting explicit binding on arrow functions FAILS SILENTLY:
console.log(arrowGreeter.call(profile));  // Output: "User: Global/Undefined"
console.log(arrowGreeter.apply(profile)); // Output: "User: Global/Undefined"
const boundArrow = arrowGreeter.bind(profile);
console.log(boundArrow());               // Output: "User: Global/Undefined"
```

### Example 5: Idiomatic Safe Property Checking
```javascript
// An object created with Object.create(null) has NO prototype, lacking hasOwnProperty!
const dictionary = Object.create(null);
dictionary.key = "value";

try {
  // Throws TypeError: dictionary.hasOwnProperty is not a function
  dictionary.hasOwnProperty("key");
} catch (err) {
  console.log("Direct hasOwnProperty failed as expected");
}

// Pre-ES2022 method borrowing:
console.log(Object.prototype.hasOwnProperty.call(dictionary, "key")); // true

// Modern ES2022 standard alternative:
console.log(Object.hasOwn(dictionary, "key")); // true
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`new` Overrides `bind()`**: Calling `new` on a bound function ignores the hard-bound `thisArg` and instantiates a new object, though pre-bound arguments are still forwarded.
2. **Double Binding Does Not Work**: You cannot rebind an already-bound function:
   ```javascript
   function f() { return this.x; }
   const b1 = f.bind({ x: 1 });
   const b2 = b1.bind({ x: 2 }); // Ignored!
   console.log(b2()); // 1
   ```
3. **Primitive Boxing in Sloppy Mode**: In non-strict mode, `fn.call(42)` boxes `42` into `new Number(42)`. In strict mode (`"use strict"`), `typeof this` inside `fn` remains `"number"`.
4. **Length and Name of Bound Functions**: A bound function's `.length` is the target function's length minus the number of bound arguments (minimum 0). Its `.name` is prefixed with `"bound "` (`"bound originalName"`).

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Binding arrow functions | `arrowFn.bind(ctx)` | Use classic function | Arrow functions ignore `bind()`; `this` remains lexical. |
| Re-binding bound function | `fn.bind(a).bind(b)` | Bind the original function | Subsequent `bind()` calls can never override `this`. |
| `Math.max.apply` for massive lists | `Math.max.apply(null, hugeList)` | `hugeList.reduce(...)` | Exceeds call stack limit if array has $>65\text{k}$ elements. |
| Calling `obj.hasOwnProperty()` | `obj.hasOwnProperty(k)` | `Object.hasOwn(obj, k)` | Fails if property is shadowed or object has null prototype. |
| Using `apply` with non-array | `fn.apply(ctx, 123)` | `fn.apply(ctx, [123])` | Throws `TypeError: CreateListFromArrayLike called on non-object`. |
| Unnecessary binding in loops | `items.map(fn.bind(this))` | `items.map(x => this.fn(x))` | Allocates new bound function instance on every iteration. |

---

## 8. Performance Characteristics

- **Direct Call vs `call()`**: Modern V8 TurboFan compiles monomorphic `call()` invocations to direct assembly jumps with virtually zero overhead.
- **`apply()` Overhead**: If arguments are already packed in a JSArray, `apply()` is optimized via internal stack splats. Passing an array-like object requires an extra iteration pass.
- **`bind()` Overhead**: Allocates a heap object (`JSBoundFunction`). If created inside hot rendering loops, this creates unnecessary garbage collection churn.

---

## 9. Engine Internals: V8 BoundFunction Exotic Objects

Inside Google V8:
1. `Function.prototype.bind()` does not simply create an interpreted wrapper function. It allocates a specialized internal C++ heap entity: `JSBoundFunction`.
2. A `JSBoundFunction` contains three internal slots:
   - `[[BoundTargetFunction]]`: Pointer to the original underlying function.
   - `[[BoundThis]]`: The target receiver object.
   - `[[BoundArguments]]`: A fixed array containing preset arguments.
3. When invoked, V8's `Builtins::CallBoundFunction` extracts the preset arguments, prepends them to the call arguments, injects `[[BoundThis]]`, and invokes the target function without entering an intermediate JavaScript frame.

---

## 10. Best Practices & Design Guidelines

1. **Use ES2022 `Object.hasOwn`**: Replace all instances of `Object.prototype.hasOwnProperty.call()` with `Object.hasOwn()`.
2. **Prefer Arrow Functions Over `.bind()` in Classes**: Use arrow functions or explicit instance methods rather than scattering `.bind()` calls across consumer code.
3. **Use Rest/Spread for Variadic Calls**: Prefer `fn(...args)` over `fn.apply(ctx, args)` unless `this` rebinding is specifically necessary.

---

## 11. Real-World Case Study: Heterogeneous Data Sanitizer via Borrowed Methods

An enterprise ingestion pipeline processing mixed inputs (DOM NodeLists, function arguments, sets, custom generator records) by borrowing Array and String prototype methods to sanitize strings into a normalized format.

```javascript
class HeterogeneousSanitizer {
  // Borrow Array methods to process any array-like or iterable collection
  static sanitizeCollection(collection, sanitizerFn) {
    if (collection == null) return [];

    // Convert iterable or array-like to dense array using borrowed slice / Array.from
    const items = Array.isArray(collection)
      ? collection
      : Array.prototype.slice.call(collection);

    return Array.prototype.map.call(items, item => {
      if (typeof item === "string") {
        // Borrow String trim and replace
        return sanitizerFn(String.prototype.trim.call(item));
      }
      return item;
    });
  }

  // Pre-configured sanitizers using partial application (bind)
  static createTagNormalizer(prefix = "#") {
    // Partially apply prefix to internal tag generator
    return HeterogeneousSanitizer.#formatTag.bind(null, prefix);
  }

  static #formatTag(prefix, rawString) {
    const cleaned = rawString.toLowerCase().replace(/[^a-z0-9]/g, "");
    return `${prefix}${cleaned}`;
  }
}

// Verification
function processUserTags() {
  // 'arguments' is array-like, not a real array
  const normalizer = HeterogeneousSanitizer.createTagNormalizer("@topic/");
  return HeterogeneousSanitizer.sanitizeCollection(arguments, normalizer);
}

const tags = processUserTags("  JavaScript  ", "Node.JS!!", "  V8 Engine ");
console.log(tags);
// Output: [ '@topic/javascript', '@topic/nodejs', '@topic/v8engine' ]
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the fundamental difference between <code>call</code>, <code>apply</code>, and <code>bind</code>?</strong></summary>

`call` and `apply` invoke the function immediately with an explicit `thisArg`. `call` takes arguments individually, while `apply` takes arguments as an array. `bind` does not execute the function immediately; it returns a new bound function with `this` and preset arguments locked for future invocations.
</details>

<details>
<summary><strong>2. What does <code>fn.bind(ctx, 1, 2)</code> do to the <code>length</code> property of the resulting function?</strong></summary>

Under the ECMAScript specification, the `length` of a bound function is the target function's `length` minus the number of bound arguments, clamped to a minimum of 0. For example, if `fn` has 3 formal parameters and 2 arguments are preset via `bind`, `boundFn.length` will be $3 - 2 = 1$.
</details>

<details>
<summary><strong>3. Why does <code>Function.prototype.bind()</code> allow <code>new</code> to override the bound <code>this</code>?</strong></summary>

The specification explicitly designed `bind()` to support constructor currying. If you bind preset constructor arguments, you still need to be able to create fresh instances via `new`. The `[[Construct]]` internal method of a bound function recognizes `new` and binds `this` to the newly created instance while preserving preset arguments.
</details>

<details>
<summary><strong>4. Why did developers use <code>Object.prototype.hasOwnProperty.call(obj, prop)</code> instead of <code>obj.hasOwnProperty(prop)</code>?</strong></summary>

Two reasons:
1. `obj` might have been created via `Object.create(null)`, in which case it has no prototype and lacks `hasOwnProperty`.
2. `obj` might have an own property named `hasOwnProperty` (e.g. `{ hasOwnProperty: false }`), which shadows the prototype method and throws or misbehaves.
</details>

<details>
<summary><strong>5. What happens when you pass <code>null</code> or <code>undefined</code> as the first argument to <code>call</code> or <code>apply</code>?</strong></summary>

In sloppy mode, `null` or `undefined` is substituted with the global object (`window` or `globalThis`). In strict mode (`"use strict"`), `this` remains strictly `null` or `undefined`.
</details>

<details>
<summary><strong>6. Can you change the <code>this</code> binding of an arrow function using <code>call</code>, <code>apply</code>, or <code>bind</code>?</strong></summary>

No. Arrow functions establish their `this` binding lexically at creation time based on the enclosing scope. The ECMAScript specification specifies that the `[[ThisMode]]` of an arrow function is `lexical`. Calling `call`, `apply`, or `bind` simply ignores the passed `thisArg`.
</details>

<details>
<summary><strong>7. How does V8 optimize bound function calls?</strong></summary>

V8 allocates an exotic `JSBoundFunction` object. When TurboFan optimizes the call site, it bypasses the creation of an intermediate JavaScript execution frame, directly forwarding the preset and dynamic arguments to the underlying target function.
</details>

<details>
<summary><strong>8. What happens if you call <code>.bind()</code> multiple times on the same function?</strong></summary>

The first `bind()` establishes the permanent `this` context. Subsequent `.bind()` calls can append additional partially applied arguments, but cannot change `this`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Argument Format Converter (Easy)
Write a wrapper function `applyToCall(fn, thisArg, argsArray)` that executes `fn` using `.call()` instead of `.apply()` by spreading `argsArray`.

<details>
<summary>View Solution</summary>

```javascript
function applyToCall(fn, thisArg, argsArray) {
  return fn.call(thisArg, ...argsArray);
}

function sum(a, b, c) { return a + b + c; }
console.log(applyToCall(sum, null, [1, 2, 3])); // 6
```
</details>

### Exercise 2: Safe Property Checker Polyfill (Easy)
Implement a polyfill for `Object.hasOwn(obj, prop)` using `Object.prototype.hasOwnProperty.call`.

<details>
<summary>View Solution</summary>

```javascript
function myHasOwn(obj, prop) {
  if (obj == null) {
    throw new TypeError("Cannot convert undefined or null to object");
  }
  return Object.prototype.hasOwnProperty.call(Object(obj), prop);
}

const nullProto = Object.create(null);
nullProto.key = "exists";
console.log(myHasOwn(nullProto, "key")); // true
```
</details>

### Exercise 3: Full Custom Function.prototype.myCall Implementation (Medium)
Implement `Function.prototype.myCall` without using native `.call()` or `.apply()`.

<details>
<summary>View Solution</summary>

```javascript
Function.prototype.myCall = function (thisArg, ...args) {
  if (typeof this !== "function") {
    throw new TypeError("myCall must be invoked on a function");
  }

  // Handle null/undefined and primitive boxing
  const context = thisArg == null ? globalThis : Object(thisArg);
  const uniqueKey = Symbol("tempFn");

  // Temporarily attach function to context to trigger Implicit Binding
  context[uniqueKey] = this;

  try {
    return context[uniqueKey](...args);
  } finally {
    delete context[uniqueKey];
  }
};

function test(a, b) { return `${this.name}: ${a + b}`; }
console.log(test.myCall({ name: "Alice" }, 10, 20)); // "Alice: 30"
```
</details>

### Exercise 4: Custom Function.prototype.myApply Implementation (Medium)
Implement `Function.prototype.myApply` without using native `.apply()` or `.call()`.

<details>
<summary>View Solution</summary>

```javascript
Function.prototype.myApply = function (thisArg, argsArray) {
  if (typeof this !== "function") {
    throw new TypeError("myApply must be invoked on a function");
  }

  const context = thisArg == null ? globalThis : Object(thisArg);
  const uniqueKey = Symbol("tempFn");
  context[uniqueKey] = this;

  try {
    const args = Array.isArray(argsArray) || (argsArray && typeof argsArray === "object" && "length" in argsArray)
      ? Array.from(argsArray)
      : [];
    return context[uniqueKey](...args);
  } finally {
    delete context[uniqueKey];
  }
};

function greet(greeting, punctuation) { return `${greeting}, ${this.user}${punctuation}`; }
console.log(greet.myApply({ user: "Bob" }, ["Hi", "!"])); // "Hi, Bob!"
```
</details>

### Exercise 5: Specification-Compliant Function.prototype.myBind (Hard)
Implement `Function.prototype.myBind` handling both standard calls and `new` operator overrides.

<details>
<summary>View Solution</summary>

```javascript
Function.prototype.myBind = function (thisArg, ...boundArgs) {
  if (typeof this !== "function") {
    throw new TypeError("myBind must be called on a function");
  }

  const targetFn = this;

  function bound(...callArgs) {
    // If called with new, this is an instance of bound
    const isNew = this instanceof bound;
    const effectiveContext = isNew ? this : thisArg;
    return targetFn.apply(effectiveContext, [...boundArgs, ...callArgs]);
  }

  if (targetFn.prototype) {
    bound.prototype = Object.create(targetFn.prototype);
  }

  // Set bound function name and length
  Object.defineProperties(bound, {
    length: {
      value: Math.max(0, targetFn.length - boundArgs.length),
      configurable: true
    },
    name: {
      value: `bound ${targetFn.name || "anonymous"}`,
      configurable: true
    }
  });

  return bound;
};

function Example(x, y) { this.val = x + y; }
const BoundEx = Example.myBind(null, 5);
const obj = new BoundEx(10);
console.log(obj.val); // 15
console.log(BoundEx.name); // "bound Example"
console.log(BoundEx.length); // 1
```
</details>

### Exercise 6: Variadic Pipeline Composer via Bind (Hard)
Create a function `composeMethods(targetObject, ...methodNames)` that extracts a pipeline of methods from `targetObject`, hard-binding each to `targetObject` so they can be piped without losing context.

<details>
<summary>View Solution</summary>

```javascript
function composeMethods(targetObject, ...methodNames) {
  const boundMethods = methodNames.map(name => {
    const fn = targetObject[name];
    if (typeof fn !== "function") {
      throw new TypeError(`Property ${String(name)} is not a method on target object`);
    }
    return fn.bind(targetObject);
  });

  return initialInput => boundMethods.reduce((val, fn) => fn(val), initialInput);
}

class TextProcessor {
  #prefix = ">> ";

  trim(str) { return str.trim(); }
  shout(str) { return str.toUpperCase(); }
  affix(str) { return `${this.#prefix}${str}`; }
}

const proc = new TextProcessor();
const pipeline = composeMethods(proc, "trim", "shout", "affix");
console.log(pipeline("   hello world   ")); // ">> HELLO WORLD"
```
</details>
