# JavaScript Function Composition & Pipelines: 'compose', 'pipe' & Flow Architectures

> Comprehensive guide to functional composition, comparing right-to-left mathematical composition (`compose`) with left-to-right flow pipelines (`pipe`), async pipelines, and pipeline operators.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02_functions/06_higher-order-functions.md](../02_functions/06_higher-order-functions.md), [07_functional-programming/01_pure-functions-and-side-effects.md](./01_pure-functions-and-side-effects.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 / ES2015 Functional Composition |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Compose vs Pipe](#2-core-concepts-compose-vs-pipe)
3. [How It Works: Mathematical Properties & Execution Flows](#3-how-it-works-mathematical-properties--execution-flows)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Call Stack Depth & Closure Pipelines](#9-engine-internals-v8-call-stack-depth--closure-pipelines)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Enterprise API Request/Response Sanitization Pipeline](#11-real-world-case-study-enterprise-api-requestresponse-sanitization-pipeline)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In software development, complex programs are ideally built by assembling small, simple, focused functions into cohesive workflows. In imperative programming, multi-step data processing requires intermediate variable assignments or deeply nested function calls:
```javascript
// Clumsy, unreadable nested calls (inside-out reading):
const result = format(sanitize(validate(extract(rawInput))));
```

**Function Composition** is the algebraic process of combining two or more functions to produce a new function. Executing $(f \circ g)(x)$ passes the result of $g(x)$ as the argument to $f$.

To make data transformations read naturally in the Western reading direction (left-to-right, top-to-bottom), functional JavaScript introduces the **`pipe`** utility. Pipelines eliminate intermediate variable pollution, enforce single-responsibility units, and make complex business workflows declarative and testable.

---

## 2. Core Concepts: Compose vs Pipe

```
                           Compose vs. Pipe
                                  │
         ┌────────────────────────┴────────────────────────┐
         ▼                                                 ▼
      compose                                             pipe
   Right-to-Left (Mathematical)                      Left-to-Right (Unix Pipeline)
   compose(f, g, h)(x)                               pipe(h, g, f)(x)
   Evaluates: f(g(h(x)))                             Evaluates: f(g(h(x)))
   Data flows: h ──► g ──► f                         Data flows: h ──► g ──► f
```

### The Unary Requirement
In pure functional composition, every intermediate step must accept exactly **one argument** (the return value of the previous step). If a function requires multiple arguments, it must be **curried** or **partially applied** before entering the pipeline.

---

## 3. How It Works: Mathematical Properties & Execution Flows

Function composition obeys the **Associative Law** from category theory:

$$f \circ (g \circ h) = (f \circ g) \circ h$$

This means you can group sub-pipelines into composite functions without altering the final computation result:
```javascript
const sanitizeAndValidate = pipe(sanitize, validate);
const fullPipeline = pipe(sanitizeAndValidate, format);
```

### Implementation Mechanics
- `compose(...fns)`: Uses `Array.prototype.reduceRight()` to fold functions from right to left.
- `pipe(...fns)`: Uses `Array.prototype.reduce()` to fold functions from left to right.

---

## 4. Syntax & API Reference

### 1. Synchronous Pipe and Compose Implementations
```javascript
// Left-to-Right Pipe
const pipe = (...fns) => initialVal =>
  fns.reduce((value, fn) => fn(value), initialVal);

// Right-to-Left Compose
const compose = (...fns) => initialVal =>
  fns.reduceRight((value, fn) => fn(value), initialVal);
```

### 2. Asynchronous Pipe Implementation
```javascript
// Left-to-Right Async Pipe handling Promises
const pipeAsync = (...fns) => initialVal =>
  fns.reduce(async (promise, fn) => fn(await promise), Promise.resolve(initialVal));
```

---

## 5. Practical Examples

### Example 1: Basic String Transformation Pipeline
```javascript
const trim = str => str.trim();
const toLower = str => str.toLowerCase();
const removePunctuation = str => str.replace(/[^a-z0-9\s]/g, "");
const splitWords = str => str.split(/\s+/);

// Assemble left-to-right processing pipeline:
const tokenize = pipe(
  trim,
  toLower,
  removePunctuation,
  splitWords
);

const rawText = "   Hello, Functional JavaScript World!!   ";
console.log(tokenize(rawText));
// Output: [ 'hello', 'functional', 'javascript', 'world' ]
```

### Example 2: Real-World Data Transformation with Partially Applied Steps
```javascript
// Utilities requiring configuration
const multiplyBy = factor => num => num * factor;
const add = addend => num => num + addend;
const clamp = (min, max) => num => Math.min(max, Math.max(min, num));

// Pipeline assembling curried functions
const calculateDiscountedPrice = pipe(
  multiplyBy(0.80), // 20% discount
  add(5.00),        // $5 shipping fee
  clamp(10, 100)    // Price bounds: $10 min, $100 max
);

console.log(calculateDiscountedPrice(50));  // (50 * 0.8) + 5 = 45 -> clamped: 45
console.log(calculateDiscountedPrice(2));   // (2 * 0.8) + 5 = 6.6 -> clamped: 10
console.log(calculateDiscountedPrice(200)); // (200 * 0.8) + 5 = 165 -> clamped: 100
```

### Example 3: Edge Case with Single-Function and Empty Pipelines
```javascript
// Robust pipe handling edge cases
const safePipe = (...fns) => initialVal => {
  if (fns.length === 0) return initialVal; // Identity function
  return fns.reduce((acc, fn) => {
    if (typeof fn !== "function") {
      throw new TypeError(`Expected function in pipeline, received: ${typeof fn}`);
    }
    return fn(acc);
  }, initialVal);
};

console.log(safePipe()(42)); // 42 (Identity)
console.log(safePipe(x => x * 2)(42)); // 84
```

### Example 4: Failure Mode with Multi-Argument Intermediate Functions
```javascript
// Multi-argument function:
const addNumbers = (a, b) => a + b;
const double = x => x * 2;

// BUG: addNumbers expects 2 arguments; previous step in pipe only outputs 1!
const brokenPipe = pipe(
  double,
  addNumbers // Throws or returns NaN because 'b' is undefined!
);

console.log(brokenPipe(5)); // NaN (addNumbers(10, undefined) -> NaN)

// FIX: Ensure all intermediate functions are UNARY (curried or accepting an object)
const curriedAdd = a => b => a + b;
const fixedPipe = pipe(
  double,
  curriedAdd(10) // Unary function expecting 1 argument
);
console.log(fixedPipe(5)); // 20
```

### Example 5: Idiomatic Asynchronous Pipeline with Validation
```javascript
const fetchUserData = async id => ({ id, rawName: "  grace hopper  ", role: "admin" });
const validateUser = async user => {
  if (!user.id) throw new Error("Missing ID");
  return user;
};
const normalizeName = async user => ({
  ...user,
  name: user.rawName.trim().toUpperCase()
});
const attachPermissions = async user => ({
  ...user,
  permissions: user.role === "admin" ? ["READ", "WRITE", "DELETE"] : ["READ"]
});

// Asynchronous pipe
const loadUserSession = pipeAsync(
  fetchUserData,
  validateUser,
  normalizeName,
  attachPermissions
);

loadUserSession("usr_99").then(session => {
  console.log("Hydrated Session:", session);
  /*
  {
    id: 'usr_99',
    rawName: '  grace hopper  ',
    role: 'admin',
    name: 'GRACE HOPPER',
    permissions: [ 'READ', 'WRITE', 'DELETE' ]
  }
  */
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Intermediate Functions Must Be Unary**: Every function in the pipeline (except potentially the very first function if invoked with multiple arguments) must accept exactly one argument.
2. **Debugging Inside Pipelines**: Inspecting intermediate values in a long pipeline can be difficult. Use a dedicated `tap(fn)` or `trace(label)` logging helper:
   ```javascript
   const trace = label => val => { console.log(`[${label}]:`, val); return val; };
   const pipeline = pipe(stepA, trace("After Step A"), stepB);
   ```
3. **Promise Rejections in `pipeAsync`**: If any step in an asynchronous pipeline rejects or throws, the entire pipeline immediately aborts and returns a rejected Promise.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Passing multi-arg functions | `pipe(f, (a, b) => a + b)` | Curry or pass an object `{ a, b }` | Pipe can only pass single return values between steps. |
| Inverting `compose` and `pipe` | Expecting `compose` to run left-to-right | Use `pipe` for left-to-right | `compose` executes right-to-left (reverse order). |
| Mutating values inside pipe | `pipe(u => { u.name = "X"; return u; })` | Return shallow copy `{ ...u, name: "X" }` | Pipeline steps must remain pure and free of side effects. |
| Deeply nesting `pipe` inside `pipe` | Excessive unreadable nesting | Flat composition with descriptive names | Deeply nested pipelines defeat readability goals. |
| Throwing inside async pipe | `throw Error` without catch | Catch at pipeline invocation | Unhandled rejection crashes Node.js processes. |
| Missing error handling boundary | Long pipeline with zero validation | Include validation step early | Bad data cascades through steps before failing cryptically. |

---

## 8. Performance Characteristics

- **Invocation Overhead**: A synchronous `pipe` of 5 functions involves 5 standard JavaScript function calls. In modern V8, TurboFan JIT inlines short pipeline functions into a single compiled machine code block, resulting in **zero performance overhead** compared to manual imperative code.
- **Allocation Footprint**: Functional pipelines that pass immutable objects allocate shallow copies between steps. For huge payloads, pass an accumulated context object or use transducers.

---

## 9. Engine Internals: V8 Call Stack Depth & Closure Pipelines

Inside Google V8:
1. When `pipe(f, g, h)(x)` is evaluated via `reduce`:
   - Each step in `reduce` evaluates iteratively within the same call frame.
   - Unlike recursive composition (`f(g(h(x)))`), which pushes $N$ stack frames onto the Call Stack, a `reduce`-based pipe runs in $O(1)$ stack frames!
2. If functions are small and have fixed monomorphic types, TurboFan performs **Inter-Procedural Optimization (IPO)** and inlines all functions into the loop body.

---

## 10. Best Practices & Design Guidelines

1. **Prefer `pipe` Over `compose` for Readability**: Unless adhering strictly to category-theory mathematical conventions, prefer `pipe`: reading operations top-to-bottom matches how developers think.
2. **Keep Steps Small & Single-Purpose**: Each function in a pipeline should do one thing well (e.g. `trim`, `validate`, `format`).
3. **Use `trace` Helpers for Observability**: Insert non-mutating logging taps between pipeline stages when debugging complex data transformations.

---

## 11. Real-World Case Study: Enterprise API Request/Response Sanitization Pipeline

A production microservice gateway processing incoming HTTP request payloads: validating JWT claims, sanitizing user inputs against XSS, normalizing emails, and attaching tenant context.

```javascript
class RequestPipeline {
  // Trace / Log tap helper
  static #trace(stage) {
    return data => {
      // console.log(`[Pipeline Stage: ${stage}]`);
      return data;
    };
  }

  // Pure step 1: Validate schema
  static #validateSchema(req) {
    if (!req.body || typeof req.body !== "object") {
      throw new Error("Invalid payload: body is required");
    }
    return req;
  }

  // Pure step 2: Normalize email
  static #normalizeEmail(req) {
    const rawEmail = req.body.email || "";
    return {
      ...req,
      body: {
        ...req.body,
        email: rawEmail.trim().toLowerCase()
      }
    };
  }

  // Pure step 3: Sanitize strings against HTML tags
  static #sanitizeStrings(req) {
    const sanitizedBody = Object.entries(req.body).reduce((acc, [k, v]) => {
      acc[k] = typeof v === "string" ? v.replace(/<[^>]*>/g, "") : v;
      return acc;
    }, {});

    return { ...req, body: sanitizedBody };
  }

  // Pure step 4: Inject tenant context
  static #injectTenant(req) {
    const tenantId = req.headers["x-tenant-id"] || "default-tenant";
    return {
      ...req,
      context: { tenantId, processedAt: new Date().toISOString() }
    };
  }

  // Assembled pipeline
  static process = pipe(
    RequestPipeline.#validateSchema,
    RequestPipeline.#trace("Validated"),
    RequestPipeline.#normalizeEmail,
    RequestPipeline.#trace("Normalized"),
    RequestPipeline.#sanitizeStrings,
    RequestPipeline.#trace("Sanitized"),
    RequestPipeline.#injectTenant
  );
}

// Verification
const incomingRequest = {
  headers: { "x-tenant-id": "acme-corp" },
  body: {
    name: "John <script>alert(1)</script>Doe",
    email: "   JOHN.DOE@Domain.COM   ",
    age: 30
  }
};

const processed = RequestPipeline.process(incomingRequest);
console.log("Processed Request:", processed);
/*
{
  headers: { 'x-tenant-id': 'acme-corp' },
  body: { name: 'John Doe', email: 'john.doe@domain.com', age: 30 },
  context: { tenantId: 'acme-corp', processedAt: '2026-...' }
}
*/
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the fundamental difference between <code>compose</code> and <code>pipe</code>?</strong></summary>

Both combine multiple functions into a single function. `compose` executes functions from right to left (following mathematical category theory $(f \circ g)(x) = f(g(x))$). `pipe` executes functions from left to right (following the Unix pipeline convention $x \to f \to g$), matching Western reading order.
</details>

<details>
<summary><strong>2. How does the Associative Property apply to function composition?</strong></summary>

Function composition is associative: $f \circ (g \circ h) \equiv (f \circ g) \circ h$. This allows developers to group and extract sub-pipelines into independent, reusable functions without altering the overall computation.
</details>

<details>
<summary><strong>3. How can you include a function that expects two arguments in a unary pipeline?</strong></summary>

You must convert the function into a unary function using **currying** or **partial application**. For instance, instead of `add(a, b)`, define `const add = a => b => a + b`. Calling `add(5)` returns a unary function `b => 5 + b` that fits seamlessly into the pipeline.
</details>

<details>
<summary><strong>4. How do you debug an intermediate value inside a functional pipeline?</strong></summary>

By inserting a **tap** or **trace** helper into the pipeline. A tap function accepts a value, performs a side effect (such as `console.log(value)`), and returns the unmodified value so the pipeline can proceed uninterrupted:
```javascript
const tap = fn => x => { fn(x); return x; };
const p = pipe(step1, tap(console.log), step2);
```
</details>

<details>
<summary><strong>5. What is the call stack footprint of a <code>reduce</code>-based pipe vs nested function calls?</strong></summary>

Nested function calls `f(g(h(x)))` push three nested stack frames onto the Call Stack simultaneously. A `pipe` implemented via `reduce` invokes each function sequentially in a loop, running with a maximum stack frame depth of 1 for the iteration.
</details>

<details>
<summary><strong>6. How does <code>pipeAsync</code> handle asynchronous errors?</strong></summary>

`pipeAsync` chains Promises via `Promise.resolve().then(...)`. If any step throws an error or returns a rejected Promise, the Promise chain is broken, skipping subsequent steps and propagating the rejection to the caller's `.catch()` block.
</details>

<details>
<summary><strong>7. What is the proposed ECMAScript Pipeline Operator (<code>\|></code>)?</strong></summary>

The Pipeline Operator (`|>`) is a TC39 Stage 2 proposal that introduces native syntax for pipelines. For example, `x |> f(%) |> g(%)` allows developers to pipe values without defining custom `pipe()` wrapper functions.
</details>

<details>
<summary><strong>8. Can an empty pipe function exist? What should it return?</strong></summary>

An empty pipe `pipe()` should act as the **Identity Function** ($I(x) = x$). When invoked with an argument `pipe()(val)`, it should return `val` unchanged.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Basic Math Pipe (Easy)
Build a pipeline `compute(x)` that adds 5, multiplies by 3, and subtracts 2 using `pipe`.

<details>
<summary>View Solution</summary>

```javascript
const add5 = x => x + 5;
const mult3 = x => x * 3;
const sub2 = x => x - 2;

const compute = pipe(add5, mult3, sub2);
console.log(compute(10)); // ((10 + 5) * 3) - 2 = 43
```
</details>

### Exercise 2: Implementing Right-to-Left Compose (Easy)
Implement `compose(...fns)` from scratch using `Array.prototype.reduceRight`.

<details>
<summary>View Solution</summary>

```javascript
function compose(...fns) {
  return function (initialValue) {
    return fns.reduceRight((acc, fn) => fn(acc), initialValue);
  };
}

const shout = str => str.toUpperCase();
const exclaim = str => `${str}!`;

const excited = compose(exclaim, shout);
console.log(excited("hello")); // "HELLO!"
```
</details>

### Exercise 3: Debugging Tap Helper (Medium)
Create a `tap(label)` helper that logs the current state and passes it along without modification.

<details>
<summary>View Solution</summary>

```javascript
const tap = label => val => {
  console.log(`[TAP: ${label}]`, JSON.stringify(val));
  return val;
};

const pipeline = pipe(
  x => x + 10,
  tap("After Addition"),
  x => x * 2,
  tap("After Multiplication")
);

pipeline(5);
// Logs:
// [TAP: After Addition] 15
// [TAP: After Multiplication] 30
```
</details>

### Exercise 4: Object Pipeline Modifier (Medium)
Create a pipeline of 3 functions that processes a configuration object: adding defaults, freezing it, and verifying a required property.

<details>
<summary>View Solution</summary>

```javascript
const addDefaults = cfg => ({ port: 8080, host: "localhost", ...cfg });
const validate = cfg => {
  if (!cfg.apiKey) throw new Error("Missing apiKey");
  return cfg;
};
const freeze = cfg => Object.freeze({ ...cfg });

const prepareConfig = pipe(addDefaults, validate, freeze);

const config = prepareConfig({ apiKey: "sec_123" });
console.log(config); // { port: 8080, host: 'localhost', apiKey: 'sec_123' }
console.log("Is frozen:", Object.isFrozen(config)); // true
```
</details>

### Exercise 5: Asynchronous Pipe with Fallback Error Handler (Hard)
Build a `pipeAsyncWithFallback(fallbackFn, ...fns)` that executes an async pipeline and invokes `fallbackFn(error)` if any step fails.

<details>
<summary>View Solution</summary>

```javascript
function pipeAsyncWithFallback(fallbackFn, ...fns) {
  return async function (initialVal) {
    try {
      return await fns.reduce(
        async (promise, fn) => fn(await promise),
        Promise.resolve(initialVal)
      );
    } catch (err) {
      return fallbackFn(err);
    }
  };
}

const step1 = async n => n + 1;
const failingStep = async () => { throw new Error("Step failed"); };
const fallback = err => ({ recovered: true, error: err.message });

const safePipeline = pipeAsyncWithFallback(fallback, step1, failingStep);
safePipeline(10).then(res => console.log(res));
// { recovered: true, error: 'Step failed' }
```
</details>

### Exercise 6: Curried Predicate Filter Pipe (Hard)
Build a pipeline that takes a list of products, filters by price and category using curried predicates, and returns the sorted product names.

<details>
<summary>View Solution</summary>

```javascript
const filterBy = predicate => list => list.filter(predicate);
const mapTo = transformFn => list => list.map(transformFn);
const sortBy = compareFn => list => list.toSorted(compareFn);

const hasCategory = cat => item => item.category === cat;
const priceUnder = max => item => item.price < max;
const getName = item => item.name;

const getBudgetElectronics = pipe(
  filterBy(hasCategory("electronics")),
  filterBy(priceUnder(100)),
  sortBy((a, b) => a.price - b.price),
  mapTo(getName)
);

const items = [
  { name: "TV", category: "electronics", price: 500 },
  { name: "Mouse", category: "electronics", price: 25 },
  { name: "Shirt", category: "apparel", price: 20 },
  { name: "Keyboard", category: "electronics", price: 75 }
];

console.log(getBudgetElectronics(items));
// Output: [ 'Mouse', 'Keyboard' ]
```
</details>
