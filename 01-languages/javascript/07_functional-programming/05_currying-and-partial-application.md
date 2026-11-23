# JavaScript Currying & Partial Application: Mathematical Foundations & Arity Mechanics

> In-depth exploration of function currying and partial application in JavaScript, arity reduction algorithms, auto-curry implementations with placeholders, and `fn.length` edge cases.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [02_functions/03_parameters-and-arguments.md](../02_functions/03_parameters-and-arguments.md), [02_functions/06_higher-order-functions.md](../02_functions/06_higher-order-functions.md), [06_closures/01_closures.md](../06_closures/01_closures.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 / ES2015 Functions & Closures |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Currying vs Partial Application](#2-core-concepts-currying-vs-partial-application)
3. [How It Works: Arity Reduction & Accumulation Mechanics](#3-how-it-works-arity-reduction--accumulation-mechanics)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Call Sites & Arity Inlining](#9-engine-internals-v8-call-sites--arity-inlining)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Enterprise Audit Event Logger Factory](#11-real-world-case-study-enterprise-audit-event-logger-factory)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In everyday programming, functions often require several parameters to complete a computation. Frequently, some of those parameters are known early (e.g. configuration, environment, database connection), while others arrive much later during program execution (e.g. user payload, transaction ID).

**Currying** and **Partial Application** are two closely related functional techniques used to separate the delivery of arguments over time and space:
- **Partial Application**: Fixes a subset of a function's arguments upfront, returning a new function that expects the remaining arguments.
- **Currying**: Deconstructs an $N$-ary function into a strict sequence of $N$ unary functions (each accepting exactly one argument).

Named after logician Haskell Curry, currying allows functions to be seamlessly adapted into unary pipelines, customized without repetitive boilerplate, and composed with maximum modularity.

---

## 2. Core Concepts: Currying vs Partial Application

```
                    Currying vs. Partial Application
                                   │
      ┌────────────────────────────┴────────────────────────────┐
      ▼                                                         ▼
   Currying                                              Partial Application
   f(a, b, c) ──► f(a)(b)(c)                             f(a, b, c) ──► f(a)(b, c)
   • Strict unary transformation                         • Arbitrary arity reduction
   • Returns a function until ALL                        • Binds 1 or more arguments upfront
     arguments are supplied 1-by-1                       • Produces a function expecting remainder
```

### Arity
The **arity** of a function is the number of arguments it formally declares:
- Unary ($1$ argument)
- Binary ($2$ arguments)
- Ternary ($3$ arguments)
- $N$-ary ($N$ arguments)
- Variadic (arbitrary number of arguments via rest parameters)

---

## 3. How It Works: Arity Reduction & Accumulation Mechanics

An automated currying function inspects the formal parameter count of the target function via `fn.length`:

```
Execution Flow of Auto-Curried Function: const curried = curry(add3)
Step 1: curried(1)
        args received: [1]
        Length check: 1 < 3 -> Return intermediate closure!

Step 2: curried(1)(2)
        args accumulated: [1, 2]
        Length check: 2 < 3 -> Return intermediate closure!

Step 3: curried(1)(2)(3)
        args accumulated: [1, 2, 3]
        Length check: 3 >= 3 -> Invoke add3(1, 2, 3) -> Return 6!
```

---

## 4. Syntax & API Reference

### 1. Manual Arrow Currying
```javascript
// Binary curried function
const add = a => b => a + b;

// Ternary curried function
const formatMessage = level => timestamp => msg =>
  `[${level}] [${timestamp}] ${msg}`;
```

### 2. Auto-Curry Implementation
```javascript
function curry(fn) {
  return function curried(...args) {
    if (args.length >= fn.length) {
      return fn.apply(this, args);
    }
    return function (...nextArgs) {
      return curried.apply(this, [...args, ...nextArgs]);
    };
  };
}
```

### 3. Native Partial Application via `bind`
```javascript
function sendEmail(apiKey, to, subject, body) { /* ... */ }

// Partially apply apiKey
const sendFromMyAccount = sendEmail.bind(null, "API_KEY_123");
sendFromMyAccount("user@test.com", "Welcome", "Hello!");
```

---

## 5. Practical Examples

### Example 1: Basic Currying for Filtering Sequences
```javascript
const filter = predicate => array => array.filter(predicate);
const map = transformFn => array => array.map(transformFn);

const isEven = n => n % 2 === 0;
const double = n => n * 2;

// Specialized reusable transformations
const filterEvens = filter(isEven);
const doubleAll = map(double);

const numbers = [1, 2, 3, 4, 5, 6];
console.log(filterEvens(numbers)); // [2, 4, 6]
console.log(doubleAll(numbers));   // [2, 4, 6, 8, 10, 12]
```

### Example 2: Real-World HTTP Header Builder
```javascript
// Curried request builder configuring authorization, contentType, and endpoint
const createApiCaller = baseUrl => authToken => contentType => (endpoint, body) => {
  return {
    url: `${baseUrl}${endpoint}`,
    headers: {
      Authorization: `Bearer ${authToken}`,
      "Content-Type": contentType
    },
    body: JSON.stringify(body)
  };
};

// 1. Configure organization base URL
const githubClient = createApiCaller("https://api.github.com");

// 2. Configure authentication token
const authenticatedClient = githubClient("ghp_secretToken12345");

// 3. Configure JSON Content Type
const jsonClient = authenticatedClient("application/json");

// 4. Reuse configured client across different endpoints
const userRequest = jsonClient("/user", { name: "Ada" });
console.log(userRequest);
/*
{
  url: 'https://api.github.com/user',
  headers: {
    Authorization: 'Bearer ghp_secretToken12345',
    'Content-Type': 'application/json'
  },
  body: '{"name":"Ada"}'
}
*/
```

### Example 3: Edge Case with Default Parameters and `fn.length`
```javascript
// TRAP: Default parameters do NOT count towards fn.length!
function multiply(a, b = 2, c = 3) {
  return a * b * c;
}

console.log("multiply.length:", multiply.length); // 1! (Halted at first default parameter!)

const brokenCurry = curry(multiply);
// Since fn.length is 1, brokenCurry evaluates on the very first argument:
console.log(brokenCurry(5)); // 5 * 2 * 3 = 30!
// Attempting to supply b fails:
// brokenCurry(5)(10) -> TypeError: brokenCurry(...) is not a function
```

### Example 4: Failure Mode with Variadic Rest Parameters
```javascript
// TRAP: Rest parameters have length 0!
function sumAll(...numbers) {
  return numbers.reduce((a, b) => a + b, 0);
}

console.log("sumAll.length:", sumAll.length); // 0!

const brokenSum = curry(sumAll);
// Calling with 0 arguments triggers evaluation immediately!
console.log(brokenSum()); // 0
```

### Example 5: Idiomatic Auto-Curry with Placeholder Support (Ramda/Lodash Style)
```javascript
const _ = Symbol("curry.placeholder");

function curryWithPlaceholders(fn, arity = fn.length) {
  return function curried(...args) {
    // Count real arguments (non-placeholders)
    const effectiveArgs = args.filter(arg => arg !== _);

    // If we have filled all arity slots without placeholders
    if (args.length >= arity && !args.slice(0, arity).includes(_)) {
      return fn.apply(this, args.slice(0, arity));
    }

    return function (...nextArgs) {
      // Merge nextArgs into placeholder slots
      const mergedArgs = args.map(arg => (arg === _ && nextArgs.length > 0 ? nextArgs.shift() : arg));
      return curried.apply(this, [...mergedArgs, ...nextArgs]);
    };
  };
}

const divide = (a, b) => a / b;
const curriedDivide = curryWithPlaceholders(divide);

// Use placeholder to partially apply the second argument (the divisor):
const halve = curriedDivide(_, 2);
console.log(halve(10)); // 5 (10 / 2)
console.log(halve(100)); // 50 (100 / 2)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`fn.length` Stops at Default Parameters**: `function f(a, b = 1, c)` has `fn.length === 1`. Auto-currying stops gathering arguments at index 0. Always specify explicit arity when currying functions with default arguments: `curry(fn, 3)`.
2. **Rest Parameters Have Arity 0**: `function f(...args)` has `fn.length === 0`. Standard auto-curry evaluates immediately.
3. **Loss of `this` Context**: In naive arrow-curried functions, `this` is lexically bound to the declaring scope. If methods require dynamic `this`, explicit `Function.prototype.apply` forwarding is mandatory.
4. **Memory Retention of Intermediate Closures**: Long-lived intermediate curried functions retain all previously passed arguments in heap `Context` objects.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Currying functions with defaults | `curry((a, b = 1) => ...)` | `curry((a, b) => ..., 2)` | `fn.length` treats default params as arity boundary. |
| Currying rest parameters | `curry((...nums) => ...)` | Specify explicit arity | `fn.length === 0`; evaluates before receiving args. |
| Over-currying simple utilities | `const add = a => b => a + b` | `(a, b) => a + b` unless composing | Unnecessary closure allocation and call frame overhead. |
| Missing `this` forwarding | Arrow curry for object methods | Forward `this` via `.apply()` | Method loses access to instance receiver. |
| Assuming partial application is curry | Calling `curried(1, 2)` when expecting `f(1)(2)` | Understand arity requirements | Full currying expects unary chain; partial application allows multi-args. |
| Modifying preset arguments in closure | Mutating captured argument array | Treat arguments as immutable | Corrupts future invocations of the partially applied function. |

---

## 8. Performance Characteristics

- **Call Frame Overhead**: Invoking a 3-stage curried function (`f(a)(b)(c)`) involves 3 distinct function calls and 3 stack frames, compared to a single call frame for `f(a, b, c)`.
- **Memory Allocation**: Each intermediate stage allocates an inner function and a heap `Context` object. In ultra-high-throughput loops ($>100,000\text{ iterations/sec}$), prefer uncurried $N$-ary functions.
- **JIT Inlining**: TurboFan JIT inlines small curried closures effectively once the call-site becomes monomorphic.

---

## 9. Engine Internals: V8 Call Sites & Arity Inlining

Inside Google V8:
1. When Ignition compiles a function call, it checks the target function's parameter count against the arguments provided in bytecode registers.
2. If arguments match `fn.length`, V8 emits direct register parameter passing.
3. If arguments are missing, V8 pushes `undefined` constants for unsupplied slots.
4. When TurboFan JIT compiles an auto-curried chain whose argument types and counts remain stable, it flattens the nested closure invocations into a single native C++ call frame.

---

## 10. Best Practices & Design Guidelines

1. **Put Data Last (Data-Last Architecture)**: When designing curried utilities (like Ramda or Lodash/fp), place configuration parameters first and the target data last: `const filter = predicate => data => data.filter(predicate)`. This enables effortless pipeline composition.
2. **Explicitly Specify Arity for Auto-Currying**: If a function uses defaults or rest parameters, pass the target arity explicitly: `curry(fn, expectedArity)`.
3. **Use Partial Application for Context Injection**: Pre-bind configuration or authentication tokens once at application bootstrap, passing the specialized function to business modules.

---

## 11. Real-World Case Study: Enterprise Audit Event Logger Factory

A distributed logging pipeline that uses currying to enforce structured audit logging across multiple microservices with consistent metadata.

```javascript
class AuditLoggerFactory {
  // Pure curried logger builder: Environment -> Service -> Action -> Payload
  static createLogger = env => service => action => (details, metadata = {}) => {
    return Object.freeze({
      environment: env,
      serviceName: service,
      actionType: action,
      details,
      metadata: {
        ...metadata,
        timestamp: new Date().toISOString(),
        host: "node-worker-01"
      }
    });
  };
}

// 1. Stage 1: Configure Environment at App Startup
const prodLogger = AuditLoggerFactory.createLogger("production");

// 2. Stage 2: Configure Service at Module Load
const paymentLogger = prodLogger("PaymentGateway");

// 3. Stage 3: Configure Action Handlers
const logChargeSuccess = paymentLogger("PAYMENT_SUCCESS");
const logChargeFailure = paymentLogger("PAYMENT_FAILED");

// 4. Stage 4: Trigger Audit Events at Runtime
const audit1 = logChargeSuccess({ transactionId: "tx_101", amount: 49.99 });
const audit2 = logChargeFailure({ transactionId: "tx_102", reason: "CARD_EXPIRED" });

console.log("Success Audit:", audit1);
console.log("Failure Audit:", audit2);
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the difference between Currying and Partial Application?</strong></summary>

- **Currying** transforms a function of arity $N$ into a strict chain of $N$ unary functions (each accepting exactly one argument).
- **Partial Application** fixes a specific number of arguments (one or more) on a function of arity $N$, returning a function that accepts the remaining arguments all at once.
</details>

<details>
<summary><strong>2. How does <code>Function.prototype.length</code> affect automated currying?</strong></summary>

`fn.length` reports the number of formal parameters declared in the function's definition. Automated currying utilities compare `args.length >= fn.length` to determine when to evaluate the underlying function. However, `fn.length` halts counting at the first parameter with a default value and excludes rest parameters (`...args`), which can break naive auto-currying.
</details>

<details>
<summary><strong>3. What is the "Data-Last" convention in functional JavaScript libraries?</strong></summary>

The data-last convention structures curried functions so that configuration arguments come first and the primary data structure being operated on comes last:
```javascript
const map = fn => array => array.map(fn);
```
This enables curried functions to be partially applied without mentioning the data, making them drop-in compatible with `pipe()` and `compose()`.
</details>

<details>
<summary><strong>4. How do placeholders work in advanced currying libraries like Ramda or Lodash?</strong></summary>

Placeholders (e.g. `_` or `R.__`) act as sentinel values indicating that an argument slot should be skipped for now and filled by an argument passed in a subsequent function call. This allows developers to partially apply arguments in any order (such as fixing argument 2 while leaving argument 1 open).
</details>

<details>
<summary><strong>5. Can an arrow function be curried?</strong></summary>

Yes. Arrow functions are particularly well-suited for manual currying due to their concise syntax:
```javascript
const add = a => b => c => a + b + c;
```
</details>

<details>
<summary><strong>6. What are the memory and performance costs of currying?</strong></summary>

Currying introduces multiple nested function calls and allocates a closure context on the heap for each intermediate step. While negligible for standard business logic, calling curried functions inside hot inner loops running millions of times per second can cause GC churn and CPU overhead compared to plain $N$-ary functions.
</details>

<details>
<summary><strong>7. How does <code>Function.prototype.bind</code> relate to Partial Application?</strong></summary>

`fn.bind(thisArg, ...presetArgs)` is a built-in form of partial application. It pre-binds `this` and any number of arguments, returning a new bound function that accepts the remaining arguments when called later.
</details>

<details>
<summary><strong>8. What happens if you pass too many arguments to an auto-curried function?</strong></summary>

A robust `curry` implementation checks `args.length >= fn.length`. If more arguments than `fn.length` are supplied, it evaluates the function immediately, forwarding all arguments (including extras) via `fn.apply(this, args)`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Manual Currying (Easy)
Write a curried function `volume(length)(width)(height)` that calculates rectangular prism volume.

<details>
<summary>View Solution</summary>

```javascript
const volume = l => w => h => l * w * h;

console.log(volume(2)(3)(4)); // 24
const baseArea10 = volume(5)(2); // l=5, w=2
console.log(baseArea10(3));     // 30
```
</details>

### Exercise 2: Partial Application Helper (Easy)
Write a utility `partial(fn, ...presetArgs)` that partially applies arguments to a function.

<details>
<summary>View Solution</summary>

```javascript
function partial(fn, ...presetArgs) {
  return function (...remainingArgs) {
    return fn.apply(this, [...presetArgs, ...remainingArgs]);
  };
}

const greet = (salutation, name) => `${salutation}, ${name}!`;
const sayHello = partial(greet, "Hello");

console.log(sayHello("Alice")); // "Hello, Alice!"
```
</details>

### Exercise 3: Auto-Curry with Explicit Arity (Medium)
Implement `curryWithArity(fn, arity)` that correctly curries functions containing default parameters by taking an explicit arity parameter.

<details>
<summary>View Solution</summary>

```javascript
function curryWithArity(fn, arity = fn.length) {
  return function curried(...args) {
    if (args.length >= arity) {
      return fn.apply(this, args);
    }
    return function (...nextArgs) {
      return curried.apply(this, [...args, ...nextArgs]);
    };
  };
}

// Function with default param: fn.length is 1, but arity is 2
function addWithDefault(a, b = 10) {
  return a + b;
}

const curriedAdd = curryWithArity(addWithDefault, 2);
console.log(curriedAdd(5)(20)); // 25
console.log(curriedAdd(5, 20)); // 25
```
</details>

### Exercise 4: Object-Field Extractor Curry (Medium)
Create a curried utility `prop(key)(obj)` that extracts a property from an object, and use it with `map` to extract user names.

<details>
<summary>View Solution</summary>

```javascript
const prop = key => obj => obj?.[key];

const users = [{ name: "Alice" }, { name: "Bob" }, { name: "Carol" }];
const getNames = users.map(prop("name"));

console.log(getNames); // ['Alice', 'Bob', 'Carol']
```
</details>

### Exercise 5: Variadic Sum with Uncurry-On-Empty-Call (Hard)
Implement a curried summing function `sum(n)` that accumulates numbers infinitely until called with zero arguments: `sum(1)(2)(3)() === 6`.

<details>
<summary>View Solution</summary>

```javascript
function sum(initialVal) {
  if (initialVal === undefined) return 0;
  let total = initialVal;

  function accumulator(nextVal) {
    if (nextVal === undefined) {
      return total; // Termination trigger
    }
    total += nextVal;
    return accumulator;
  }

  return accumulator;
}

console.log(sum(1)(2)(3)(4)()); // 10
console.log(sum(10)(20)());     // 30
console.log(sum());             // 0
```
</details>

### Exercise 6: Complete Auto-Curry with Multi-Placeholder Reordering (Hard)
Build a production-grade `curry` function supporting multiple placeholders (`_`) that correctly tracks unresolved argument indices across multiple calls.

<details>
<summary>View Solution</summary>

```javascript
const _ = Symbol("placeholder");

function advancedCurry(fn, arity = fn.length) {
  return function curried(...args) {
    // Determine if all required positional slots are filled with non-placeholders
    const isComplete = args.length >= arity && !args.slice(0, arity).includes(_);

    if (isComplete) {
      return fn.apply(this, args);
    }

    return function (...nextArgs) {
      // Merge nextArgs into placeholder slots or append
      const merged = args.map(arg => (arg === _ && nextArgs.length > 0 ? nextArgs.shift() : arg));
      return curried.apply(this, [...merged, ...nextArgs]);
    };
  };
}

const formatCoords = (x, y, z) => `Point(${x}, ${y}, ${z})`;
const curriedCoords = advancedCurry(formatCoords);

// Pre-fill y and z, leaving x open:
const setX = curriedCoords(_, 20, 30);
console.log(setX(10)); // "Point(10, 20, 30)"

// Fill x and z, leaving y open:
const setY = curriedCoords(10, _, 30);
console.log(setY(99)); // "Point(10, 99, 30)"
```
</details>
