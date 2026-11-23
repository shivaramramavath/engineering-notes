# The Module and Revealing Module Pattern in JavaScript

> Encapsulation, private state via lexical closures, the Revealing Module pattern, and modernization with ES Modules.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Closures](file:///e:/Learning/engineering-notes/01-languages/javascript/06_closures/01_closures.md), [Scope and Execution](file:///e:/Learning/engineering-notes/01-languages/javascript/04_scope-and-execution/01_scope.md), [IIFE](file:///e:/Learning/engineering-notes/01-languages/javascript/02_functions/07_iife-and-function-properties.md) |
| **Time to read** | 22 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js & Evergreen Browsers |

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

Before ES2015 (ES6) introduced native modules (`import`/`export`) and classes with `#private` fields, JavaScript provided no built-in mechanism for information hiding, access modifiers, or data encapsulation. Any variable declared in a script was exposed directly on the global scope (`window`), causing namespace collisions and fragile coupling.

The **Module Pattern** was the foundational architectural pattern that transformed JavaScript into an enterprise-grade language. Pioneered by Douglas Crockford and expanded by Christian Heilmann as the **Revealing Module Pattern**, it leverages Immediately Invoked Function Expressions (IIFE) and lexical closures to create private execution state while selectively exposing a public API. Even in modern JavaScript with native ESM, this pattern remains vital for library bundling, per-instance state isolation, and dependency encapsulation.

---

## 2. Core Concepts

### Information Hiding via Lexical Scope
In JavaScript, variable scope is bound to functions and blocks. Variables declared with `let`, `const`, or `var` inside a function are completely unreachable from outer code. By returning an object literal containing functions that close over those private variables, we create a public API with exclusive access to the encapsulated state.

### The Classic Module Pattern vs The Revealing Module Pattern
- **Classic Module Pattern**: Functions and properties are defined directly on the returned object literal. Public methods reference private functions, but public-to-public method overrides can break internal consistency.
- **Revealing Module Pattern**: All variables and functions are defined purely as private declarations in the function body. The return statement simply exposes pointers to the chosen private functions using an object literal mapping. This establishes a clean, self-documenting public interface.

### Modern Equivalence: ES Modules vs Closures
While native ES Modules provide file-level singleton encapsulation, the closure-based Module Pattern provides **instance-level** encapsulation: calling a module factory function produces multiple independent state containers, each with its own private variables.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| GLOBAL / CALLER SCOPE                                                    |
| const cart = createCart();                                               |
| cart.addItem(...)       cart.getTotal()         cart.items (UNDEFINED)   |
+-----------------------------+--------------------------------------------+
                              | Public Method Calls
                              v
+--------------------------------------------------------------------------+
| RETURNED PUBLIC API OBJECT                                                |
| { addItem: [Function], getTotal: [Function], clear: [Function] }         |
+-----------------------------+--------------------------------------------+
                              | [[Environment]] Pointer
                              v
+--------------------------------------------------------------------------+
| LEXICAL CLOSURE CONTEXT (Heap Context)                                   |
| - let items = [];                    <--- COMPLETELY PRIVATE             |
| - let taxRate = 0.08;                <--- COMPLETELY PRIVATE             |
| - function recalculateTaxes() {...}  <--- PRIVATE HELPER                 |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Classic Module Pattern (IIFE Singleton)
```javascript
const CartModule = (() => {
  // Private state
  const items = [];
  const taxRate = 0.08;

  // Public API
  return {
    addItem(name, price) {
      items.push({ name, price });
    },
    getTotal() {
      const subtotal = items.reduce((sum, item) => sum + item.price, 0);
      return subtotal * (1 + taxRate);
    },
  };
})();
```

### Revealing Module Pattern (Factory Instance)
```javascript
function createStore(initialState = {}) {
  // Private variables
  let state = { ...initialState };
  const listeners = new Set();

  // Private methods
  function getState() {
    return Object.freeze({ ...state });
  }

  function setState(updates) {
    state = { ...state, ...updates };
    notify();
  }

  function subscribe(fn) {
    listeners.add(fn);
    return () => listeners.delete(fn);
  }

  function notify() {
    listeners.forEach((listener) => listener(state));
  }

  // Revealing Public API
  return Object.freeze({
    getState,
    setState,
    subscribe,
  });
}
```

---

## 5. Practical Examples

### Example 1: Robust Banking Account with Strict Encapsulation
```javascript
export function createBankAccount(accountNumber, initialBalance = 0) {
  let balance = initialBalance;
  const transactionLedger = [];

  function recordTransaction(type, amount) {
    transactionLedger.push({
      id: crypto.randomUUID(),
      timestamp: new Date().toISOString(),
      type,
      amount,
      balanceAfter: balance,
    });
  }

  function deposit(amount) {
    if (typeof amount !== 'number' || amount <= 0) {
      throw new TypeError('Deposit amount must be a positive number');
    }
    balance += amount;
    recordTransaction('DEPOSIT', amount);
    return balance;
  }

  function withdraw(amount) {
    if (typeof amount !== 'number' || amount <= 0) {
      throw new TypeError('Withdrawal amount must be a positive number');
    }
    if (amount > balance) {
      throw new RangeError('Insufficient funds');
    }
    balance -= amount;
    recordTransaction('WITHDRAWAL', amount);
    return balance;
  }

  function getBalance() {
    return balance;
  }

  function getLedger() {
    // Return deep copy so callers cannot mutate internal ledger
    return transactionLedger.map(tx => ({ ...tx }));
  }

  return Object.freeze({
    accountNumber,
    deposit,
    withdraw,
    getBalance,
    getLedger,
  });
}
```

### Example 2: Configurable HTTP Client with Private Auth Interceptor
```javascript
export function createApiClient(baseUrl) {
  let authToken = null;
  let retryCount = 3;

  function setAuthToken(token) {
    authToken = token;
  }

  async function request(endpoint, options = {}) {
    const url = `${baseUrl}${endpoint}`;
    const headers = {
      'Content-Type': 'application/json',
      ...(options.headers || {}),
    };

    if (authToken) {
      headers['Authorization'] = `Bearer ${authToken}`;
    }

    let attempts = 0;
    while (attempts < retryCount) {
      try {
        const response = await fetch(url, { ...options, headers });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await response.json();
      } catch (err) {
        attempts++;
        if (attempts >= retryCount) throw err;
      }
    }
  }

  function get(endpoint) {
    return request(endpoint, { method: 'GET' });
  }

  function post(endpoint, data) {
    return request(endpoint, { method: 'POST', body: JSON.stringify(data) });
  }

  return Object.freeze({
    setAuthToken,
    get,
    post,
  });
}
```

### Example 3: Augmenting / Extending a Module (Sub-Module Pattern)
```javascript
// Base Module
const CoreApp = (() => {
  const version = '2.4.0';
  function getVersion() { return version; }
  return { getVersion };
})();

// Extension Module (Augmentation)
const ExtendedApp = ((parent) => {
  const plugins = new Map();

  parent.registerPlugin = function(name, pluginFn) {
    plugins.set(name, pluginFn);
  };

  parent.runPlugins = function(context) {
    for (const [name, fn] of plugins) {
      fn(context);
    }
  };

  return parent;
})(CoreApp || {});
```

### Example 4: The Namespace Injection Pattern
```javascript
// Safely attaching modules to a unified application namespace
const App = globalThis.App || {};

((namespace) => {
  const cache = new Map();

  function save(key, value) {
    cache.set(key, value);
  }

  function load(key) {
    return cache.get(key);
  }

  // Inject into namespace
  namespace.Cache = Object.freeze({ save, load });
})(App);
```

### Example 5: Testing Module Pattern Private Logic with Test Injections
```javascript
export function createOrderProcessor(testHooks = {}) {
  const orders = [];

  function calculateTax(subtotal) {
    return subtotal * 0.10;
  }

  function placeOrder(item, price) {
    const tax = calculateTax(price);
    const order = { id: orders.length + 1, item, total: price + tax };
    orders.push(order);
    return order;
  }

  // Expose private methods exclusively in test environments
  if (process.env.NODE_ENV === 'test') {
    testHooks._calculateTax = calculateTax;
    testHooks._orders = orders;
  }

  return { placeOrder };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Shared State Across Factory Instances**: Declaring a variable outside the factory function shares that variable across all created module instances, inadvertently creating a singleton leak:
   ```javascript
   let sharedMistake = []; // BUG: shared across all instances!
   function createModule() {
     return { add: (x) => sharedMistake.push(x) };
   }
   ```
2. **Loss of `this` Context**: One of the greatest advantages of the Revealing Module Pattern is that its methods do not rely on `this`. They reference private functions lexically. Destructuring `{ deposit } = account` works seamlessly without `.bind()`.
3. **Immutability of the Returned API**: If the returned object is not frozen via `Object.freeze()`, external consumers can overwrite public methods (`cart.addItem = null`), breaking client code.
4. **Leaking Internal References**: Returning private arrays or objects directly allows outside callers to mutate private state:
   ```javascript
   return { getItems: () => items }; // BAD: external code can call getItems().length = 0
   return { getItems: () => [...items] }; // GOOD: shallow copy protects state
   ```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Returning direct reference to private arrays | External callers can mutate internal state via `.push()` or `.splice()` | Return shallow/deep clone or frozen view |
| Forgetting `Object.freeze()` on returned API | External code can overwrite or delete public methods | Wrap returned object in `Object.freeze()` |
| Placing instance variables outside factory | Shares state across all instances (unintended singleton) | Move state declarations inside factory |
| Referencing public methods via `this` | Destructuring breaks `this` binding | Use Revealing Module pattern with direct lexical references |
| Massive monolithic modules (> 1000 lines) | Violates Single Responsibility; increases memory footprint | Split into smaller composable modules |
| Using `eval` or `with` inside module scope | Breaks lexical scope analysis in V8 | Keep module scope strictly standard |

---

## 8. Performance Characteristics

- **Memory Overhead per Instance**: Every factory invocation creates a new V8 `Context` object on the heap and a fresh set of function closures. For 1,000,000 instances, ES6 classes with prototype methods consume ~4x less memory than closure-based module factories.
- **Method Execution Speed**: Invoking closure-based module methods is blisteringly fast because it avoids prototype chain lookups and polymorphic `this` inline cache checks.

---

## 9. Engine Internals

1. **V8 Context Allocation**:
   - When a function defines inner functions that outlive its execution, V8 allocates a heap `Context` struct.
   - Variables captured by closures are moved out of the CPU stack frame and stored in the heap `Context` table.
   - Pointers to this context are stored in each returned function's `[[Environment]]` internal slot.

---

## 10. Best Practices & Design Guidelines

- **Use Revealing Module for Small Services**: Ideal for services, utilities, and stores where instance counts are low ($\le 1000$) and strict private encapsulation is paramount.
- **Use ES6 Classes with `#private` Fields for High-Volume Entities**: If creating millions of entity instances (e.g., game particles or database rows), use ES6 classes with `#` private fields to leverage prototype method sharing.
- **Always Freeze the Interface**: Use `Object.freeze(publicApi)` to guarantee immutability of the exported contract.

---

## 11. Real-World Case Study

### Production Feature Flag Manager
```javascript
export function createFeatureFlagManager(initialFlags = {}) {
  // Encapsulated private state
  const flags = new Map(Object.entries(initialFlags));
  const changeListeners = new Set();

  function isEnabled(flagKey, userContext = {}) {
    if (!flags.has(flagKey)) return false;
    const rule = flags.get(flagKey);

    if (typeof rule === 'boolean') return rule;
    if (typeof rule === 'function') return Boolean(rule(userContext));
    return false;
  }

  function setFlag(flagKey, rule) {
    flags.set(flagKey, rule);
    notify(flagKey);
  }

  function onFlagChange(listener) {
    changeListeners.add(listener);
    return () => changeListeners.delete(listener);
  }

  function notify(flagKey) {
    for (const listener of changeListeners) {
      try {
        listener(flagKey);
      } catch (err) {
        console.error('Feature flag listener error:', err);
      }
    }
  }

  return Object.freeze({
    isEnabled,
    setFlag,
    onFlagChange,
  });
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between the Classic Module Pattern and the Revealing Module Pattern?</summary>
In the Classic Module Pattern, public methods are defined directly as functions on the returned object literal. In the Revealing Module Pattern, all variables and functions are defined privately in the function body, and the return statement simply maps public property names to those private functions. This keeps the syntax consistent and prevents public method overriding from breaking internal references.
</details>

<details>
<summary>2. Why does the Module Pattern provide better encapsulation than pre-ES2022 ES6 classes?</summary>
Prior to ES2022 private fields (<code>#field</code>), all class properties and methods were public. Developers relied on naming conventions like <code>_privateVar</code>, which could still be inspected, modified, or overwritten by external code. The Module Pattern utilizes lexical closures, making private variables physically inaccessible from outside the closure.
</details>

<details>
<summary>3. Why are methods in a Revealing Module immune to "this" context loss?</summary>
Because Revealing Module methods do not use the <code>this</code> keyword to reference state or sibling methods; they access private variables directly via lexical scope. Therefore, destructuring methods (e.g., <code>const { getCount } = counter;</code>) can be executed without losing context or calling <code>.bind()</code>.
</details>

<details>
<summary>4. What is the memory trade-off between the Module Pattern and ES6 Prototype Classes?</summary>
ES6 classes store methods on a single shared <code>prototype</code> object, regardless of how many instances are created. The closure-based Module Pattern instantiates a new set of closure functions and a V8 heap <code>Context</code> for every single instance created, consuming significantly more memory when millions of instances exist.
</details>

<details>
<summary>5. How does the IIFE (Immediately Invoked Function Expression) enable the singleton module pattern?</summary>
The IIFE executes immediately upon definition, creating a private scope for its variables. It returns the public API object once, which is assigned to a variable, effectively creating a single, globally accessible singleton instance without polluting the global scope.
</details>

<details>
<summary>6. How can external callers accidentally break a module's encapsulation?</summary>
If the module returns a direct reference to an internal mutable array or object, the external caller can mutate that data structure directly (e.g., calling <code>.push()</code> or altering properties). Modules must always return shallow or deep copies of internal data.
</details>

<details>
<summary>7. What role does Object.freeze() play when returning a module's public API?</summary>
<code>Object.freeze()</code> prevents external code from adding new properties, deleting existing properties, or reassigning public methods on the returned interface, guaranteeing the integrity of the module's public contract.
</details>

<details>
<summary>8. How do ES Modules (ESM) compare with the closure-based Module Pattern?</summary>
ES Modules provide file-level static encapsulation natively supported by engines and bundlers, with tree-shaking and asynchronous loading. However, ESM creates module singletons per file. The closure-based Module Pattern is still required when multiple isolated stateful instances need to be generated dynamically at runtime.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build an Auto-Expiring Key-Value Store Module
Implement a factory function `createExpiringCache()` using the Revealing Module Pattern that allows setting key-value pairs with a TTL (Time To Live in milliseconds), supporting `get(key)`, `set(key, val, ttlMs)`, and automatic cleanup of expired keys.

<details>
<summary>View Solution</summary>

```javascript
export function createExpiringCache() {
  const store = new Map();

  function set(key, value, ttlMs = 1000) {
    const expiresAt = Date.now() + ttlMs;
    store.set(key, { value, expiresAt });
  }

  function get(key) {
    const entry = store.get(key);
    if (!entry) return null;

    if (Date.now() > entry.expiresAt) {
      store.delete(key);
      return null;
    }

    return entry.value;
  }

  function clearExpired() {
    const now = Date.now();
    for (const [key, entry] of store.entries()) {
      if (now > entry.expiresAt) {
        store.delete(key);
      }
    }
  }

  function size() {
    clearExpired();
    return store.size;
  }

  return Object.freeze({
    set,
    get,
    clearExpired,
    size,
  });
}
```
</details>

### Exercise 2: Secure Pub-Sub Event Bus Module
Implement a closure-based Pub-Sub event bus module `createEventBus()` that allows registering event listeners, publishing payloads safely without leaking listeners to subscribers, and returning an unsubscribe function.

<details>
<summary>View Solution</summary>

```javascript
export function createEventBus() {
  const registry = new Map();

  function subscribe(event, callback) {
    if (typeof callback !== 'function') {
      throw new TypeError('Subscriber callback must be a function');
    }

    if (!registry.has(event)) {
      registry.set(event, new Set());
    }

    const subscribers = registry.get(event);
    subscribers.add(callback);

    // Return unsubscription function
    return function unsubscribe() {
      subscribers.delete(callback);
      if (subscribers.size === 0) {
        registry.delete(event);
      }
    };
  }

  function publish(event, data) {
    if (!registry.has(event)) return;

    // Shallow copy subscribers to prevent mutation issues during dispatch
    const subscribers = Array.from(registry.get(event));
    for (const callback of subscribers) {
      try {
        callback(data);
      } catch (err) {
        console.error(`Error in event listener for [${event}]:`, err);
      }
    }
  }

  return Object.freeze({
    subscribe,
    publish,
  });
}
```
</details>
