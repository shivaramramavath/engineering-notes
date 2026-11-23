# The Decorator Pattern in JavaScript

> Dynamically extending behavior, Higher-Order Function decorators, TC39 Stage 3 decorators, and composition over subclassing.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Higher-Order Functions](file:///e:/Learning/engineering-notes/01-languages/javascript/02_functions/06_higher-order-functions.md), [Classes](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/05_classes.md) |
| **Time to read** | 24 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, TC39 Stage 3 Decorators (Node 20+, Modern Browsers) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In software development, you frequently need to add cross-cutting responsibilities—such as telemetry logging, performance execution timing, input validation, authentication checks, or retry resilience—to existing functions or classes.

Attempting to add these features through class inheritance creates a **Class Explosion Anti-Pattern**: if you have a `Coffee` class and want optional `Milk`, `Sugar`, and `Caramel`, subclassing requires `CoffeeWithMilk`, `CoffeeWithMilkAndSugar`, `CoffeeWithCaramelAndMilk`, etc., leading to dozens of permutations.

The **Decorator Pattern** dynamically attaches additional responsibilities to an object or function without modifying its underlying source code. It wraps the target component inside a decorator object or function that exposes the same interface, executing auxiliary behavior before or after delegating to the wrapped component.

---

## 2. Core Concepts

### Composition Over Inheritance
Rather than hardcoding behavior in subclasses, decorators wrap objects at runtime. Multiple decorators can be stacked and composed in any sequence:
```javascript
const query = withLogging(withMetrics(withRetry(rawDatabaseQuery)));
```

### Two Styles of Decorators in JavaScript
1. **Higher-Order Function (HoF) Decorators**: The idiomatic, universal JavaScript approach. A function that accepts a target function, wraps it in a closure, and returns an enhanced function. Works everywhere without build tools.
2. **TC39 Class Decorators (Stage 3 Standard)**: Native language syntax (`@decorator`) that decorates class declarations, methods, getters/setters, and class fields.

### The Open-Closed Principle
Decorators adhere strictly to the Open-Closed Principle: the core business function remains completely untouched and closed for modification, while its behavior is open for extension via decorators.

---

## 3. How It Works Internally

```
CALLING CODE: decoratedService.fetchData(101)
                    |
                    v
+--------------------------------------------------------------------------+
| DECORATOR 1: withLogging                                                 |
| - console.log('Starting fetchData for 101')                              |
| - Calls inner function ->                                                |
+-------------------+------------------------------------------------------+
                    |
                    v
+--------------------------------------------------------------------------+
| DECORATOR 2: withTiming                                                  |
| - const t0 = performance.now()                                           |
| - Calls inner function ->                                                |
+-------------------+------------------------------------------------------+
                    |
                    v
+--------------------------------------------------------------------------+
| TARGET CORE FUNCTION: rawFetchData(101)                                  |
| - Executes actual database query or network request                      |
| - Returns result                                                         |
+-------------------+------------------------------------------------------+
                    |
                    | Resolves value back up the pipeline
                    v
| DECORATOR 2: Logs `Duration: ${performance.now() - t0}ms`
                    v
| DECORATOR 1: Logs `Completed fetchData successfully`
                    v
RETURN TO CALLER
```

---

## 4. Syntax & API Reference

### Higher-Order Function Decorator Pattern
```javascript
function withLogging(fn) {
  return function (...args) {
    console.log(`[CALL] ${fn.name} with arguments:`, args);
    const result = fn.apply(this, args);
    console.log(`[RESULT] ${fn.name} returned:`, result);
    return result;
  };
}

// Wrapping a target function
const add = (a, b) => a + b;
const loggedAdd = withLogging(add);
```

### Modern TC39 Stage 3 Method Decorator
```javascript
// Method Decorator signature in TC39 Stage 3:
// (targetMethod, context: { kind, name, static, private, addInitializer }) => replacementMethod
function logged(targetMethod, context) {
  const methodName = String(context.name);

  return function (...args) {
    console.log(`Executing ${methodName}...`);
    const result = targetMethod.call(this, ...args);
    console.log(`Finished ${methodName}.`);
    return result;
  };
}

class UserService {
  @logged
  getUser(id) {
    return { id, name: 'Grace Hopper' };
  }
}
```

---

## 5. Practical Examples

### Example 1: Execution Timing Decorator (`withTiming`)
```javascript
import { performance } from 'node:perf_hooks';

export function withTiming(fn, label = fn.name || 'Anonymous') {
  return async function (...args) {
    const start = performance.now();
    try {
      return await fn.apply(this, args);
    } finally {
      const duration = (performance.now() - start).toFixed(2);
      console.log(`[BENCHMARK] ${label} took ${duration}ms`);
    }
  };
}

// Usage:
const fetchUsers = async () => (await fetch('https://api.example.com/users')).json();
const timedFetchUsers = withTiming(fetchUsers, 'User Fetch Service');
```

### Example 2: Automatic Retry Decorator with Exponential Backoff
```javascript
export function withRetry(fn, { maxRetries = 3, baseDelayMs = 200 } = {}) {
  return async function (...args) {
    let attempt = 0;
    while (true) {
      try {
        return await fn.apply(this, args);
      } catch (err) {
        attempt++;
        if (attempt >= maxRetries) {
          throw err;
        }
        const delay = baseDelayMs * Math.pow(2, attempt - 1);
        console.warn(`[RETRY] ${fn.name || 'Operation'} failed. Retrying in ${delay}ms...`);
        await new Promise(r => setTimeout(r, delay));
      }
    }
  };
}
```

### Example 3: In-Memory Memoization Decorator
```javascript
export function memoize(fn, keyResolver = (...args) => JSON.stringify(args)) {
  const cache = new Map();

  const memoized = function (...args) {
    const key = keyResolver(...args);
    if (cache.has(key)) {
      return cache.get(key);
    }
    const result = fn.apply(this, args);
    cache.set(key, result);
    return result;
  };

  memoized.clearCache = () => cache.clear();
  return memoized;
}

// Usage for expensive CPU calculation
const fib = memoize((n) => (n <= 1 ? n : fib(n - 1) + fib(n - 2)));
```

### Example 4: Classic Object-Level Decorator (Coffee Shop Permutations)
```javascript
// Base Component
class SimpleCoffee {
  cost() { return 2.0; }
  description() { return 'Simple Coffee'; }
}

// Decorator Base
class CoffeeDecorator {
  constructor(coffee) {
    this.coffee = coffee;
  }
  cost() { return this.coffee.cost(); }
  description() { return this.coffee.description(); }
}

// Concrete Decorator A: Milk
class WithMilk extends CoffeeDecorator {
  cost() { return super.cost() + 0.5; }
  description() { return `${super.description()}, Milk`; }
}

// Concrete Decorator B: Vanilla Syrup
class WithVanilla extends CoffeeDecorator {
  cost() { return super.cost() + 0.75; }
  description() { return `${super.description()}, Vanilla Syrup`; }
}

// Compose decorators dynamically at runtime
let myOrder = new SimpleCoffee();
myOrder = new WithMilk(myOrder);
myOrder = new WithVanilla(myOrder);

console.log(myOrder.description()); // Simple Coffee, Milk, Vanilla Syrup
console.log(myOrder.cost());        // 3.25
```

### Example 5: TC39 Stage 3 Deprecation Warning Decorator
```javascript
function deprecated(targetMethod, context) {
  const name = String(context.name);
  let warned = false;

  return function (...args) {
    if (!warned) {
      console.warn(`[DEPRECATION WARNING] Method "${name}" is deprecated and will be removed in a future release.`);
      warned = true;
    }
    return targetMethod.call(this, ...args);
  };
}

class LegacyApi {
  @deprecated
  oldEndpoint() {
    return 'legacy result';
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Loss of Function Metadata (`fn.name` and `fn.length`)**: When wrapping a function in a decorator, the returned wrapper function typically has `length = 0` and `name = ""` or `"wrapper"`. If other libraries inspect `fn.length` for arity checks, this can break behavior. Workaround: Use `Object.defineProperty(wrapper, 'name', { value: fn.name })`.
2. **Preserving `this` Context**: In function decorators, you **must** use standard `function (...args)` and call `fn.apply(this, args)`. Using an arrow function `(...args) => fn(...args)` loses the caller's `this` context, breaking method decorators on class instances.
3. **Async vs Sync Function Mismatches**: Wrapping a synchronous function in an asynchronous decorator (e.g., using `async function (...args)`) forces the return value into a Promise. Callers expecting synchronous returns will receive an unresolved Promise instead.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Using arrow functions for method decorators | Loses caller `this` binding | Use standard `function (...args)` with `fn.apply(this, args)` |
| Wrapping sync functions in async decorators | Unintentionally turns return value into a Promise | Keep decorator synchronously transparent if target is sync |
| Forgetting to return the inner function's result | Caller receives `undefined` | Always return the evaluated result of `fn.apply()` |
| Subclassing for every optional feature combination | Causes Class Explosion anti-pattern | Use runtime decorator composition |
| Stacking decorators in arbitrary order when order matters | E.g. caching before auth allows unauthenticated cache reads | Carefully sequence decorators (Auth -> Cache -> Execute) |

---

## 8. Performance Characteristics

- **Invocation Latency**: Wrapping a function in a closure decorator adds $< 5$ nanoseconds of call stack overhead.
- **TurboFan Inlining**: Small decorators (e.g. logging or timing) that wrap monomorphic functions are frequently inlined by TurboFan, resulting in zero performance penalty in production.

---

## 9. Engine Internals

1. **Closure Scopes and Environment Records**:
   - Each HoF decorator call allocates a small lexical `Context` holding references to the wrapped `fn` and decorator parameters.
   - Calling the decorated function dereferences `fn` from the `[[Environment]]` slot in $O(1)$ constant time.

---

## 10. Best Practices & Design Guidelines

- **Order Matters in Stacking**: When composing decorators, order is critical. Security/auth decorators must execute *first*, followed by rate limiting, caching, and finally execution timing.
- **Keep Decorators Single-Purpose**: Each decorator should do exactly one thing (Single Responsibility Principle).
- **Forward All Arguments**: Always use rest parameters `(...args)` and `fn.apply(this, args)` to maintain transparency.

---

## 11. Real-World Case Study

### Production API Route Protection & Telemetry Decorator Pipeline
```javascript
// Composing a production-ready API route pipeline without frameworks
function withAuthentication(handler) {
  return async function (req, res) {
    const authHeader = req.headers['authorization'];
    if (!authHeader?.startsWith('Bearer valid-token')) {
      res.writeHead(401, { 'Content-Type': 'application/json' });
      return res.end(JSON.stringify({ error: 'Unauthorized' }));
    }
    return handler.call(this, req, res);
  };
}

function withRateLimit(handler, maxPerMinute = 60) {
  const requests = new Map();

  return async function (req, res) {
    const ip = req.socket.remoteAddress || 'unknown';
    const now = Date.now();
    const timestamps = (requests.get(ip) || []).filter(t => now - t < 60_000);

    if (timestamps.length >= maxPerMinute) {
      res.writeHead(429, { 'Content-Type': 'application/json' });
      return res.end(JSON.stringify({ error: 'Rate limit exceeded' }));
    }

    timestamps.push(now);
    requests.set(ip, timestamps);
    return handler.call(this, req, res);
  };
}

// Composition pipeline utility
const compose = (...decorators) => (baseFn) =>
  decorators.reduceRight((acc, decorator) => decorator(acc), baseFn);

// Protected Core Handler
async function getSecretDataHandler(req, res) {
  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ secret: 'Top secret data' }));
}

// Assemble decorated endpoint
export const protectedEndpoint = compose(
  withRateLimit,
  withAuthentication
)(getSecretDataHandler);
```

---

## 12. Interview Questions

<details>
<summary>1. What problem does the Decorator Pattern solve and how does it compare to subclassing?</summary>
The Decorator Pattern solves the Class Explosion problem by dynamically adding behavior to objects or functions at runtime using composition rather than static inheritance. Subclassing requires compiling a unique class for every possible combination of features, whereas decorators can be stacked and composed flexibly in any order.
</details>

<details>
<summary>2. Why must arrow functions NOT be used when writing higher-order function decorators?</summary>
Arrow functions capture <code>this</code> lexically from their enclosing scope. When decorating class methods or object methods, using an arrow function prevents the decorator from receiving the actual instance context (<code>this</code>) of the caller, causing method calls on <code>this</code> to fail.
</details>

<details>
<summary>3. What is the execution order when multiple decorators are stacked?</summary>
Decorators are composed from the inside out (or bottom to top). In syntax like <code>@A @B method()</code> or <code>compose(A, B)(fn)</code>, Decorator <b>B</b> wraps the target first, and Decorator <b>A</b> wraps B. When the method is called, Decorator A executes its pre-processing first, delegates to B, which delegates to the method.
</details>

<details>
<summary>4. What are TC39 Stage 3 Decorators and how do they differ from legacy Babel/TypeScript experimental decorators?</summary>
TC39 Stage 3 decorators are the official, standardized ECMAScript decorator specification. Unlike legacy experimental decorators (which passed raw property descriptors and constructor prototypes), Stage 3 decorators receive a target value and a standardized <code>context</code> object containing metadata (kind, name, static, private, addInitializer).
</details>

<details>
<summary>5. How does the Decorator pattern differ from the Proxy pattern?</summary>
While both patterns wrap an underlying target, their intent differs:
- <b>Decorator</b>: Designed to dynamically add new behaviors, responsibilities, or enrichments.
- <b>Proxy</b>: Designed to control, intercept, or guard access to the underlying object (e.g. lazy loading, security gatekeeper, virtual DOM tracking) without adding new domain features.
</details>

<details>
<summary>6. How do you preserve the original function's name and length when decorating it?</summary>
By copying the metadata properties explicitly onto the returned wrapper function using <code>Object.defineProperty(wrapper, 'name', { value: fn.name })</code> and <code>Object.defineProperty(wrapper, 'length', { value: fn.length })</code>.
</details>

<details>
<summary>7. What happens if an async decorator wraps a synchronous function?</summary>
The decorator will return a Promise. Any client code that called the synchronous function expecting an immediate return value will now receive a Promise, which can cause unexpected truthy checks (e.g. <code>if (decoratedFn())</code> evaluates true because a Promise is truthy).
</details>

<details>
<summary>8. How does middleware in Express or Koa relate to the Decorator pattern?</summary>
Express and Koa middleware pipelines are architectural variants of the Decorator and Chain of Responsibility patterns: each middleware wraps request/response handling, performing auxiliary tasks (logging, parsing, authentication) before delegating to the next handler via <code>next()</code>.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Concurrency Throttling Decorator
Implement a function decorator `withConcurrencyLimit(fn, limit = 2)` that wraps an asynchronous function and ensures that no more than `limit` concurrent executions of `fn` can run simultaneously, queueing subsequent calls.

<details>
<summary>View Solution</summary>

```javascript
export function withConcurrencyLimit(fn, limit = 2) {
  let activeCount = 0;
  const queue = [];

  function next() {
    if (activeCount < limit && queue.length > 0) {
      activeCount++;
      const { args, resolve, reject, context } = queue.shift();

      Promise.resolve(fn.apply(context, args))
        .then(resolve)
        .catch(reject)
        .finally(() => {
          activeCount--;
          next();
        });
    }
  }

  return function (...args) {
    return new Promise((resolve, reject) => {
      queue.push({ args, resolve, reject, context: this });
      next();
    });
  };
}
```
</details>

### Exercise 2: Input Argument Type-Checking Decorator
Implement a decorator `withTypeCheck(fn, typeGuards)` that accepts an array of type validation functions `(val) => boolean` corresponding to expected arguments, throwing a `TypeError` if any parameter fails validation before executing `fn`.

<details>
<summary>View Solution</summary>

```javascript
export function withTypeCheck(fn, typeGuards = []) {
  return function (...args) {
    for (let i = 0; i < typeGuards.length; i++) {
      const guard = typeGuards[i];
      if (typeof guard === 'function' && !guard(args[i])) {
        throw new TypeError(`Invalid argument at index ${i}: received ${typeof args[i]} (${args[i]})`);
      }
    }
    return fn.apply(this, args);
  };
}

// Usage
const isNumber = n => typeof n === 'number' && !isNaN(n);
const safeMultiply = withTypeCheck((a, b) => a * b, [isNumber, isNumber]);

safeMultiply(10, 5); // 50
// safeMultiply(10, 'five'); // Throws TypeError!
```
</details>
