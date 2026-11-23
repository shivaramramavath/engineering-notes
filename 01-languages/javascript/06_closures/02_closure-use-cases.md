# JavaScript Closure Design Patterns: Modules, Factories & Memoization

> Comprehensive guide to real-world closure patterns in modern JavaScript, covering the Revealing Module Pattern, private state factories, currying, function decorators, and high-performance memoization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [02_functions/06_higher-order-functions.md](../02_functions/06_higher-order-functions.md), [06_closures/01_closures.md](./01_closures.md) |
| **Time to Read** | 23 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 / ES2015 Modules & Closures |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Closure Architectural Patterns](#2-core-concepts-closure-architectural-patterns)
3. [How It Works: Encapsulation & Environment Life Cycles](#3-how-it-works-encapsulation--environment-life-cycles)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Context Lifetimes Across Multiple Closures](#9-engine-internals-v8-context-lifetimes-across-multiple-closures)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Production-Grade Debounce & Throttle Engine](#11-real-world-case-study-production-grade-debounce--throttle-engine)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Closures are not merely an academic language feature; they represent one of the most powerful and versatile architectural building blocks in JavaScript software design. 

Before modern ES modules and `#private` class fields existed, closures were the **only mechanism** in JavaScript for establishing true private state and data hiding. Today, closures remain the underlying engine powering:
- Function decorators and higher-order composition
- Currying and partial application pipelines
- Memoization caches and throttled event listeners
- React Hooks (`useState`, `useEffect`) and Vue composition APIs

Mastering these design patterns allows developers to choose the right abstraction between closures, classes, and plain objects.

---

## 2. Core Concepts: Closure Architectural Patterns

```
                          Core Closure Use Cases
                                     │
         ┌───────────────────────────┼───────────────────────────┐
         ▼                           ▼                           ▼
  1. Data Encapsulation       2. Function Transformation  3. State Preservation
  • Revealing Module Pattern  • Currying                  • Memoization Caches
  • Factory Functions         • Partial Application       • Debounce / Throttle
  • Private Methods/State     • Function Decorators       • React Hooks Engine
```

### 1. Data Encapsulation
By declaring variables inside a function and exposing only specific inner functions to the outside world, developers create clean public interfaces while hiding internal invariants.

### 2. Function Transformation
Closures allow a function to accept parameters in multiple stages, pre-configuring behavior (partial application) or decorating existing functions with telemetry, validation, or retries.

### 3. State Preservation Across Invocations
Closures allow asynchronous callbacks (such as HTTP handlers or DOM event listeners) to retain state across asynchronous time gaps without polluting global or parent objects.

---

## 3. How It Works: Encapsulation & Environment Life Cycles

```
The Revealing Module Pattern
┌────────────────────────────────────────────────────────┐
│ const CartModule = (function () {                      │
│   // 1. Private State in Heap Context                  │
│   let items = [];                                      │
│   function computeTax() { return 0.08; }               │
│                                                        │
│   // 2. Public Methods closing over private state      │
│   function addItem(item) { items.push(item); }         │
│   function getTotal() { /* uses items & computeTax */ }│
│                                                        │
│   // 3. Return Object revealing only public interface   │
│   return { addItem, getTotal };                        │
│ })();                                                  │
└────────────────────────────────────────────────────────┘
External World:
Can invoke CartModule.addItem() and CartModule.getTotal().
CANNOT read or mutate `items` array directly!
```

---

## 4. Syntax & API Reference

### Pattern Signatures
- **Factory Function**: `function createInstance(init) { let state = init; return { get, set }; }`
- **IIFE Module**: `const Module = (function() { let _priv; return { pub() {} }; })();`
- **Decorator / Wrapper**: `function withLogging(fn) { return function(...args) { return fn(...args); }; }`
- **Currying**: `const add = a => b => a + b;`

---

## 5. Practical Examples

### Example 1: The Revealing Module Pattern
```javascript
const UserAuthModule = (function () {
  // Private module state
  let #sessionToken = null; // conceptual privacy
  let activeUser = null;

  function hashCredentials(user, pass) {
    return `hash_${user}_${pass.length}`;
  }

  function login(username, password) {
    activeUser = username;
    #sessionToken = hashCredentials(username, password);
    console.log(`[Auth] User ${username} logged in.`);
  }

  function logout() {
    activeUser = null;
    #sessionToken = null;
    console.log("[Auth] User logged out.");
  }

  function isAuthenticated() {
    return activeUser !== null;
  }

  // Reveal public API
  return {
    login,
    logout,
    isAuthenticated,
    get username() { return activeUser; }
  };
})();

UserAuthModule.login("Grace", "superSecret123");
console.log("Is Authenticated?", UserAuthModule.isAuthenticated()); // true
console.log("Current User:", UserAuthModule.username);              // "Grace"
UserAuthModule.logout();
```

### Example 2: Real-World Parametric Currency Converter
```javascript
function createCurrencyConverter(exchangeRate, currencySymbol) {
  // exchangeRate and currencySymbol are permanently captured in closure
  return function convert(amount) {
    if (typeof amount !== "number" || amount < 0) {
      throw new TypeError("Amount must be a positive number");
    }
    const converted = amount * exchangeRate;
    return `${currencySymbol}${converted.toFixed(2)}`;
  };
}

const toEuros = createCurrencyConverter(0.92, "€");
const toPounds = createCurrencyConverter(0.79, "£");

console.log(toEuros(100));  // "€92.00"
console.log(toPounds(100)); // "£79.00"
```

### Example 3: Edge Case with Method Borrowing Breaking Closures
```javascript
function createStore(initial) {
  let state = initial;
  return {
    getState() { return state; },
    setState(val) { state = val; }
  };
}

const store1 = createStore("Store 1");
const store2 = createStore("Store 2");

// Attempting to borrow method does NOT borrow state:
const borrowedSet = store1.setState;
// Closures bind to lexical scope, NOT 'this'!
borrowedSet.call(store2, "Mutated!");

console.log("Store 1:", store1.getState()); // "Mutated!" (Store 1 was mutated!)
console.log("Store 2:", store2.getState()); // "Store 2" (Untouched!)
```

### Example 4: Failure Mode with Non-Primitive Memoization Keys
```javascript
function naiveMemoize(fn) {
  const cache = {}; // Object with string keys

  return function (...args) {
    // Failure mode: Objects coerce to "[object Object]"!
    const key = args.join(",");
    if (key in cache) return cache[key];
    const res = fn(...args);
    cache[key] = res;
    return res;
  };
}

const processUser = naiveMemoize(u => u.name.toUpperCase());
console.log(processUser({ name: "Alice" })); // "ALICE"
// Bug: { name: "Bob" } generates same key "[object Object]"!
console.log(processUser({ name: "Bob" }));   // "ALICE" (Incorrect cached result!)

// Fix: Use Map with reference keys or JSON serialization
```

### Example 5: Idiomatic Function Execution Guard (Once)
```javascript
function once(fn) {
  let executed = false;
  let memoizedResult;

  return function (...args) {
    if (!executed) {
      executed = true;
      memoizedResult = fn.apply(this, args);
      // Clean up fn reference to assist GC if fn holds heavy closures
      fn = null;
    }
    return memoizedResult;
  };
}

const initializeDb = once(endpoint => {
  console.log(`Connecting to ${endpoint}...`);
  return { connected: true, timestamp: Date.now() };
});

console.log(initializeDb("postgres://localhost:5432"));
console.log(initializeDb("postgres://localhost:5432")); // Returned from closure!
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Closures Do Not Obey `this` Rebinding**: Unlike object methods, a closure references identifiers in lexical scope. Calling `closure.call(anotherObj)` has zero effect on the enclosed variables.
2. **Memory Retention in Multi-Method Factories**: In factory functions returning multiple methods, all methods share the exact same `Context`. Retaining one method retains all variables in that context.
3. **Memoization Key Collisions**: Using naive string serialization for memoization keys fails on functions, circular objects, and symbols.
4. **Stale State in Asynchronous Closures**: If a closure reads a variable that is asynchronously modified in parallel, race conditions can occur.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Naive memoization key | `cache[args.join()]` | `Map` with composite key | Objects serialize to `"[object Object]"` causing collisions. |
| Retaining massive objects in factory | `let req = hugeReq; return () => req.id` | `let id = hugeReq.id; return () => id` | Prevents the entire 10MB request payload from being freed. |
| Using closures where plain object fits | Static configs in closure | Plain object literal | Closures incur heap context allocation overhead. |
| Forgetting to nullify once references | Leaving heavy function in `once` | `fn = null;` after run | Prevents underlying function closure tree from GC. |
| Re-creating factories inside loops | `arr.map(() => createFactory())` | Hoist factory outside | Creates hundreds of redundant heap contexts. |
| Expecting `this` rebinding in closure | `closure.call(other)` to change var | Use object properties if rebinding | Closures bind lexically, completely ignoring `call/apply`. |

---

## 8. Performance Characteristics

- **Factory Functions vs Classes**:
  - Class instances share a single prototype object ($O(1)$ memory per instance for methods).
  - Factory functions allocate new closure functions for every instance ($O(k)$ functions per instance).
  - For thousands of instances, classes are significantly more memory-efficient. For singletons or modules, factory closures are ideal.
- **Memoization Lookups**: $O(1)$ constant time lookup in a `Map`. Can improve $O(2^n)$ recursive algorithms to $O(n)$ linear time.

---

## 9. Engine Internals: V8 Context Lifetimes Across Multiple Closures

Inside Google V8:
1. When a function containing closures evaluates, V8 allocates a `Context` containing all captured variables.
2. If `createStore()` returns `{ getState, setState }`, both functions share the **same internal pointer** to that `Context` in their `[[Environment]]` slot.
3. As long as either `getState` OR `setState` is reachable from root references (e.g. attached to `window`), the entire `Context` remains alive.
4. Only when **both** functions are garbage-collected can V8 reclaim the `Context` memory.

---

## 10. Best Practices & Design Guidelines

1. **Choose Classes for High-Volume Objects, Closures for Singletons**: If you instantiate $>1,000$ objects, use classes with prototype methods. For singletons, middleware, and event handlers, use closures.
2. **Defend Memoization Caches Against Memory Leaks**: Always use an LRU (Least Recently Used) cache or a fixed-size buffer for long-running server processes to prevent unbounded heap growth.
3. **Keep Enclosed Scope Narrow**: Only capture the absolute minimum set of primitive values required for the closure to operate.

---

## 11. Real-World Case Study: Production-Grade Debounce & Throttle Engine

Production-grade debounce and throttle higher-order wrappers used in high-frequency event processing (scroll, resize, search autocomplete).

```javascript
class TimingUtilities {
  // Debounce: delays execution until 'waitMs' has elapsed since last call
  static debounce(fn, waitMs, { immediate = false } = {}) {
    let timeoutId = null;

    function debounced(...args) {
      const context = this;
      const callNow = immediate && !timeoutId;

      clearTimeout(timeoutId);

      timeoutId = setTimeout(() => {
        timeoutId = null;
        if (!immediate) {
          fn.apply(context, args);
        }
      }, waitMs);

      if (callNow) {
        fn.apply(context, args);
      }
    }

    // Cancellation hook
    debounced.cancel = () => {
      clearTimeout(timeoutId);
      timeoutId = null;
    };

    return debounced;
  }

  // Throttle: guarantees execution at most once every 'limitMs'
  static throttle(fn, limitMs) {
    let lastRan = 0;
    let timeoutId = null;

    return function throttled(...args) {
      const context = this;
      const now = Date.now();

      if (now - lastRan >= limitMs) {
        if (timeoutId) {
          clearTimeout(timeoutId);
          timeoutId = null;
        }
        fn.apply(context, args);
        lastRan = now;
      } else if (!timeoutId) {
        // Trailing execution
        timeoutId = setTimeout(() => {
          fn.apply(context, args);
          lastRan = Date.now();
          timeoutId = null;
        }, limitMs - (now - lastRan));
      }
    };
  }
}

// Verification
let auditLogs = [];
const recorder = msg => auditLogs.push({ msg, time: Date.now() });

const debouncedLog = TimingUtilities.debounce(recorder, 50);

debouncedLog("Call 1");
debouncedLog("Call 2");
debouncedLog("Call 3"); // Only Call 3 will execute after 50ms

setTimeout(() => {
  console.log("Debounced logs recorded:", auditLogs.length); // 1
  console.log("Recorded message:", auditLogs[0]?.msg);        // "Call 3"
}, 80);
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the Revealing Module Pattern, and why was it significant in JavaScript's evolution?</strong></summary>

The Revealing Module Pattern uses an Immediately Invoked Function Expression (IIFE) and closures to define private variables and functions, returning an object that exposes only the public interface. It was significant because it provided the first robust standard for modularity, encapsulation, and namespace protection prior to the introduction of ES Modules in ES6.
</details>

<details>
<summary><strong>2. How does currying use closures to transform functions?</strong></summary>

Currying transforms a function taking $N$ arguments into a sequence of $N$ unary functions (each taking 1 argument). Each nested function returned in the chain forms a closure over the previously passed arguments, accumulating them until all required parameters have been supplied, at which point the final computation evaluates.
</details>

<details>
<summary><strong>3. What are the memory trade-offs of Factory Functions vs ES6 Classes?</strong></summary>

- **ES6 Classes**: Method definitions reside on `Class.prototype`. All instances share references to the same functions. Instantiating 10,000 instances creates 10,000 light objects with pointer links ($O(1)$ memory per method).
- **Factory Functions**: Every instance creates a new set of inner closure functions and heap `Context` objects. Instantiating 10,000 instances creates 10,000 duplicate function instances on the heap, increasing memory consumption and GC overhead.
</details>

<details>
<summary><strong>4. Why does method borrowing fail on methods created inside closure factories?</strong></summary>

Object methods rely on dynamic `this` binding (`this.val`), which can be swapped via `call` or `apply`. In contrast, methods created inside closure factories resolve their state via lexical variable bindings in the `Context`. The lexical environment is fixed at definition time, so changing `this` via `.call()` has zero effect on the enclosed state.
</details>

<details>
<summary><strong>5. How does memoization prevent stack overflow in recursive algorithms?</strong></summary>

Memoization caches the results of function calls based on input arguments. In overlapping subproblem algorithms (such as the Fibonacci sequence), memoization eliminates redundant recursive branches, reducing time complexity from exponential $O(2^n)$ down to linear $O(n)$, drastically cutting call stack frame depth.
</details>

<details>
<summary><strong>6. How do React Hooks like <code>useState</code> rely on closures?</strong></summary>

React maintains an internal array of state cells for each component instance. When a component calls `useState(init)`, React returns the current state and a setter function. The setter function is a closure that captures the component's internal fiber index, ensuring that calling `setState` updates the exact state cell corresponding to that component.
</details>

<details>
<summary><strong>7. What is an LRU cache, and why is it recommended for closure memoization?</strong></summary>

An LRU (Least Recently Used) cache evicts the oldest, least accessed items when the cache reaches a predetermined size limit. Without an eviction policy, a closure memoization cache will grow indefinitely as new arguments are passed, leading to memory exhaustion and eventual application crashes.
</details>

<details>
<summary><strong>8. How does V8 handle garbage collection when multiple closures share an outer context?</strong></summary>

V8 allocates a single shared `Context` for all variables captured by any closure defined in that scope. The entire `Context` remains alive in heap memory as long as at least one closure referencing that `Context` is still reachable.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Configurable Tag Generator (Easy)
Write a function `makeTag(tag)` that returns a closure that wraps text in that HTML tag (e.g. `makeTag("h1")("Title")` -> `"<h1>Title</h1>"`).

<details>
<summary>View Solution</summary>

```javascript
function makeTag(tag) {
  return function (content) {
    return `<${tag}>${content}</${tag}>`;
  };
}

const h1 = makeTag("h1");
const p = makeTag("p");
console.log(h1("System Architecture")); // "<h1>System Architecture</h1>"
console.log(p("Overview text..."));      // "<p>Overview text...</p>"
```
</details>

### Exercise 2: Auto-Incrementing Key Generator (Easy)
Create a factory `createKeyGen(prefix)` that generates unique string keys on each call using closures.

<details>
<summary>View Solution</summary>

```javascript
function createKeyGen(prefix) {
  let counter = 0;
  return () => `${prefix}_${++counter}`;
}

const userKey = createKeyGen("usr");
console.log(userKey()); // "usr_1"
console.log(userKey()); // "usr_2"
```
</details>

### Exercise 3: Generic Curry Implementation (Medium)
Implement `curry(fn)` that transforms a function of arbitrary arity into a curried version.

<details>
<summary>View Solution</summary>

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

const sum3 = (a, b, c) => a + b + c;
const curriedSum = curry(sum3);

console.log(curriedSum(1)(2)(3)); // 6
console.log(curriedSum(1, 2)(3)); // 6
console.log(curriedSum(1)(2, 3)); // 6
```
</details>

### Exercise 4: LRU Memoization Cache (Medium)
Implement `memoizeLRU(fn, capacity = 3)` that caches results using closures and evicts the least recently used entry when capacity is exceeded.

<details>
<summary>View Solution</summary>

```javascript
function memoizeLRU(fn, capacity = 3) {
  const cache = new Map(); // Map preserves insertion order

  return function (...args) {
    const key = JSON.stringify(args);
    if (cache.has(key)) {
      const val = cache.get(key);
      cache.delete(key);
      cache.set(key, val); // Refresh order
      return val;
    }

    const result = fn.apply(this, args);
    if (cache.size >= capacity) {
      const oldestKey = cache.keys().next().value;
      cache.delete(oldestKey); // Evict LRU
    }
    cache.set(key, result);
    return result;
  };
}

const square = memoizeLRU(x => {
  console.log(`Computing ${x}^2...`);
  return x * x;
}, 2);

console.log(square(2)); // Computes 4
console.log(square(3)); // Computes 9
console.log(square(2)); // Cached!
console.log(square(4)); // Evicts 3!
```
</details>

### Exercise 5: Asynchronous Retry Wrapper with Backoff (Hard)
Create a function decorator `withRetry(asyncFn, maxRetries = 3, baseDelayMs = 100)` using closures to track attempt count and apply exponential backoff.

<details>
<summary>View Solution</summary>

```javascript
function withRetry(asyncFn, maxRetries = 3, baseDelayMs = 100) {
  return async function (...args) {
    let attempt = 0;
    while (true) {
      try {
        return await asyncFn.apply(this, args);
      } catch (err) {
        attempt++;
        if (attempt > maxRetries) {
          throw new Error(`Exceeded max retries (${maxRetries}). Last error: ${err.message}`);
        }
        const delay = baseDelayMs * Math.pow(2, attempt - 1);
        console.log(`[Retry] Attempt ${attempt} failed. Retrying in ${delay}ms...`);
        await new Promise(r => setTimeout(r, delay));
      }
    }
  };
}

let tries = 0;
const flakyNetwork = async () => {
  if (++tries < 3) throw new Error("Connection reset");
  return "Payload Delivered!";
};

const resilientFetch = withRetry(flakyNetwork, 3, 20);
resilientFetch().then(res => console.log("Final outcome:", res));
```
</details>

### Exercise 6: Complete Reactive Signal Engine (Hard)
Build a minimal reactive signal system (`createSignal`, `createEffect`) where signals use closures to track and notify dependent effect listeners automatically.

<details>
<summary>View Solution</summary>

```javascript
let activeEffect = null;

function createSignal(initialValue) {
  let value = initialValue;
  const subscribers = new Set();

  function read() {
    if (activeEffect !== null) {
      subscribers.add(activeEffect);
    }
    return value;
  }

  function write(newValue) {
    if (value !== newValue) {
      value = newValue;
      for (const effect of Array.from(subscribers)) {
        effect();
      }
    }
  }

  return [read, write];
}

function createEffect(fn) {
  const execute = () => {
    activeEffect = execute;
    try {
      fn();
    } finally {
      activeEffect = null;
    }
  };
  execute();
}

// Verification
const [count, setCount] = createSignal(0);

createEffect(() => {
  console.log(`[Effect Reaction] Count is now: ${count()}`);
});

setCount(1); // Logs: [Effect Reaction] Count is now: 1
setCount(2); // Logs: [Effect Reaction] Count is now: 2
```
</details>
