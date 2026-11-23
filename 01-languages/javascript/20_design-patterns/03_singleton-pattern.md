# The Singleton Pattern in JavaScript

> Single-instance guarantees, ES Module caching, class-based singletons, global symbols, testing challenges, and SSR hazards.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Classes](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/05_classes.md), [Modules](file:///e:/Learning/engineering-notes/01-languages/javascript/13_modules/01_module-systems-overview.md) |
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

In software engineering, certain resources must exist as exactly one instance throughout the entire lifecycle of an application. Examples include a database connection pool, a hardware configuration manager, a global event bus, or an in-memory cache. Creating multiple instances of these objects wastes system memory, creates conflicting state, and exhausts operating system socket handles.

The **Singleton Pattern** restricts the instantiation of a class or resource to a single instance and provides a global access point to it. In JavaScript, the pattern takes unique forms: while traditional OOP languages rely on private class constructors and static `getInstance()` methods, modern JavaScript frequently achieves singletons effortlessly through **ES Module evaluation caching**. However, improper singleton design can introduce severe state leakage, complicate unit testing, and cause catastrophic data collisions in Server-Side Rendering (SSR) environments.

---

## 2. Core Concepts

### Four Ways to Implement a Singleton in JavaScript
1. **ES Module Singleton**: The idiomatic JavaScript approach. In both ESM and CommonJS, module files are evaluated exactly once when first imported. Exporting an instantiated object produces a natural, engine-enforced singleton.
2. **Class-based Singleton (Static Factory / Constructor Interception)**: A class that intercepts `new ClassName()` to return the pre-existing instance if already constructed.
3. **Closure-based Singleton (IIFE)**: Encapsulating the instance inside a closure that exposes a `getInstance()` function.
4. **Cross-Realm / Global Symbol Singleton**: Attaching a singleton to `globalThis` using a unique `Symbol.for()` key, guaranteeing a single instance even across different npm package installations or iframe realms.

### The Problem with Singletons: Hidden Global State
Singletons are essentially object-oriented global variables. When multiple subsystems depend on a mutable singleton, they become tightly coupled through shared hidden state. In unit tests, mutations made by Test Suite A persist into Test Suite B, causing intermittent test failures (flakiness).

---

## 3. How It Works Internally

```
FIRST IMPORT / ACCESS:
Caller A: import config from './config.js';
   |
   v
[ Module System Evaluates config.js ]
   |
   +---> Creates Heap Instance: { dbHost: 'localhost', poolSize: 10 }
   |
   +---> Caches reference in Module Map (V8 Module Record)
   |
   +---> Returns Reference to Caller A

SUBSEQUENT IMPORTS:
Caller B: import config from './config.js';
   |
   v
[ Module System checks Module Map ] ---> MATCH FOUND!
   |
   +---> Returns EXACT SAME HEAP REFERENCE to Caller B (Zero re-evaluation)
```

---

## 4. Syntax & API Reference

### 1. The Idiomatic ES Module Singleton
```javascript
// db-connection.js
class DatabaseConnection {
  constructor() {
    this.connected = true;
    this.id = crypto.randomUUID();
  }
  query(sql) { /* ... */ }
}

// Exporting an INSTANCE guarantees a single instance across imports
export const db = new DatabaseConnection();
```

### 2. Class-Based Singleton (Constructor Interception)
```javascript
export class AppConfig {
  static #instance = null;

  constructor() {
    if (AppConfig.#instance) {
      return AppConfig.#instance; // Return existing instance
    }
    this.theme = 'dark';
    this.apiUrl = 'https://api.example.com';
    AppConfig.#instance = this;
  }

  static getInstance() {
    if (!AppConfig.#instance) {
      AppConfig.#instance = new AppConfig();
    }
    return AppConfig.#instance;
  }
}
```

---

## 5. Practical Examples

### Example 1: Robust Cross-Package Global Symbol Singleton
```javascript
// Guarantees a single instance even if dual npm versions or symlinks load the file twice!
const SINGLETON_KEY = Symbol.for('com.myapp.eventbus.instance');

class GlobalEventBus {
  constructor() {
    this.listeners = new Map();
  }

  on(event, handler) {
    if (!this.listeners.has(event)) this.listeners.set(event, new Set());
    this.listeners.get(event).add(handler);
  }

  emit(event, data) {
    if (this.listeners.has(event)) {
      this.listeners.get(event).forEach(fn => fn(data));
    }
  }
}

// Check global realm before creating
if (!globalThis[SINGLETON_KEY]) {
  globalThis[SINGLETON_KEY] = new GlobalEventBus();
}

export const eventBus = globalThis[SINGLETON_KEY];
```

### Example 2: Lazy-Initialized Database Connection Pool
```javascript
class ConnectionPool {
  #pool = null;

  async getClient() {
    if (!this.#pool) {
      console.log('Lazy initializing database connection pool...');
      this.#pool = await this.#initializePool();
    }
    return this.#pool.acquire();
  }

  async #initializePool() {
    // Heavy connection setup
    return { acquire: () => ({ id: Math.random() }) };
  }
}

// Singleton instance with lazy evaluation
export const poolManager = new ConnectionPool();
```

### Example 3: Testable Singleton with Reset Mechanics
```javascript
export class TelemetryService {
  static #instance = null;

  constructor() {
    if (TelemetryService.#instance) {
      return TelemetryService.#instance;
    }
    this.queue = [];
    TelemetryService.#instance = this;
  }

  track(event) {
    this.queue.push({ event, timestamp: Date.now() });
  }

  // Critical for unit tests: resets state between tests
  static _resetForTesting() {
    TelemetryService.#instance = null;
  }
}
```

### Example 4: Immutable Configuration Singleton
```javascript
class SystemSettings {
  constructor() {
    this.maxRetries = 3;
    this.timeoutMs = 5000;
    this.environment = process.env.NODE_ENV || 'development';
    // Freeze instance to prevent runtime tampering
    Object.freeze(this);
  }
}

export const settings = new SystemSettings();
```

### Example 5: Factory-Controlled Multiton (Keyed Singleton Pool)
```javascript
// Multiton: Guarantees exactly one instance PER KEY
export class TenantCache {
  static #instances = new Map();

  constructor(tenantId) {
    this.tenantId = tenantId;
    this.cache = new Map();
  }

  static getForTenant(tenantId) {
    if (!TenantCache.#instances.has(tenantId)) {
      TenantCache.#instances.set(tenantId, new TenantCache(tenantId));
    }
    return TenantCache.#instances.get(tenantId);
  }
}

// Tenant A and B have their own isolated singletons
const cacheA = TenantCache.getForTenant('tenant-100');
const cacheB = TenantCache.getForTenant('tenant-200');
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Dual-Package Split-Brain Hazard**: If your project or an npm dependency imports both a CommonJS build (`require('lib')`) and an ESM build (`import 'lib'`), Node.js treats them as separate modules. Each creates its own "singleton" instance in memory! Fix: Use the Global Symbol pattern (`Symbol.for()`) or point both builds to a shared state file.
2. **Server-Side Rendering (SSR) State Bleed**: In frameworks like Next.js, Nuxt, or Remix, a module-level singleton runs on the persistent Node.js server. If User A logs in and stores their user ID in a singleton store, User B's incoming HTTP request will see User A's data! **Never store request-scoped data in singletons on a server.**
3. **Subclassing Pitfalls**: If Class `B` extends Singleton Class `A`, invoking `new B()` returns the instance of `A`, completely breaking the derived class behavior.
4. **Test Pollution**: Singletons persist across unit test executions unless explicitly reset, causing random test failures depending on execution order.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Storing user auth or request state in a singleton | Leaks data across different user sessions in Node/SSR | Use AsyncLocalStorage or request contexts |
| Over-using singletons for everything | Tight coupling, untestable code, hidden dependencies | Pass dependencies via constructor parameters |
| Failing to provide a test reset hook | Test suites suffer from order-dependent state pollution | Provide a static `_resetForTesting()` method |
| Assuming ESM singletons survive dual CJS/ESM bundling | CJS and ESM module loaders create separate instances | Use `Symbol.for()` attached to `globalThis` |
| Calling `new ClassName()` expecting a new instance | Violates developer expectation of the `new` operator | Prefer static `getInstance()` or exported instance |

---

## 8. Performance Characteristics

- **Zero Instantiation Overhead**: After the initial instantiation, retrieving a singleton is an instantaneous $O(1)$ variable or property access.
- **Garbage Collection Immunity**: Because a singleton is referenced by a module record or global object, it is **never** collected by the garbage collector throughout the entire lifetime of the process.

---

## 9. Engine Internals

1. **V8 Module Map and Caching**:
   - In ECMAScript specification §16.2.1.5 (`Evaluate`), every module script has a status: `unlinked`, `linking`, `linked`, `evaluating`, `evaluated`.
   - Once a module reaches `evaluated`, the V8 engine stores its exports in a persistent `ModuleRecord` hash table.
   - Subsequent `import` statements across any file in the application bypass evaluation and return the existing exported instance reference directly.

---

## 10. Best Practices & Design Guidelines

- **Prefer ES Module Exports**: In modern JavaScript, `export const instance = new Service()` is cleaner and more idiomatic than complex Java-style class singletons.
- **Isolate Singletons to Stateless or Global Services**: Only use singletons for truly application-wide services (Logger, Metrics, Database Pool). Never use them for stateful user data.
- **Provide Dependency Injection as an Alternative**: Allow constructors to accept mock instances for testing rather than hardcoding singleton references.

---

## 11. Real-World Case Study

### Production WebSocket Client Singleton with Auto-Reconnect
```javascript
// Safe frontend WebSocket manager guaranteeing a single persistent socket connection
class WebSocketManager {
  static #instance = null;
  #socket = null;
  #url = null;
  #listeners = new Set();

  constructor(url) {
    if (WebSocketManager.#instance) {
      return WebSocketManager.#instance;
    }
    this.#url = url;
    this.#connect();
    WebSocketManager.#instance = this;
  }

  #connect() {
    this.#socket = new WebSocket(this.#url);

    this.#socket.onmessage = (event) => {
      const data = JSON.parse(event.data);
      for (const listener of this.#listeners) {
        listener(data);
      }
    };

    this.#socket.onclose = () => {
      console.warn('WebSocket closed. Reconnecting in 3s...');
      setTimeout(() => this.#connect(), 3000);
    };
  }

  subscribe(listener) {
    this.#listeners.add(listener);
    return () => this.#listeners.delete(listener);
  }

  send(payload) {
    if (this.#socket.readyState === WebSocket.OPEN) {
      this.#socket.send(JSON.stringify(payload));
    }
  }

  static getInstance(url = 'wss://api.example.com/live') {
    if (!WebSocketManager.#instance) {
      WebSocketManager.#instance = new WebSocketManager(url);
    }
    return WebSocketManager.#instance;
  }
}

export const wsManager = WebSocketManager.getInstance();
```

---

## 12. Interview Questions

<details>
<summary>1. How does the ES Module system inherently implement the Singleton pattern?</summary>
The ECMAScript specification dictates that every module is evaluated exactly once per realm when first imported. The engine caches the resulting module exports in its internal module registry. Any subsequent <code>import</code> from any file across the application resolves to the exact same cached instance in memory.
</details>

<details>
<summary>2. Why are singletons hazardous in Server-Side Rendered (SSR) applications like Next.js?</summary>
In an SSR environment, a single Node.js process persists across thousands of incoming HTTP requests from different users. If request-specific or user-specific data is stored in a module-level singleton, that state persists across requests, causing critical cross-user data leakage and security violations.
</details>

<details>
<summary>3. What is the Dual-Package Hazard in Node.js and how does it break singletons?</summary>
The Dual-Package Hazard occurs when an application or its dependencies inadvertently load both the CommonJS (<code>require</code>) and ES Module (<code>import</code>) builds of the same package. Node.js evaluates each package entry point independently, creating two distinct singleton instances in memory instead of one.
</details>

<details>
<summary>4. How can you ensure a singleton remains unified across dual-package or multi-realm environments?</summary>
By attaching the instance to <code>globalThis</code> using a well-known global symbol created via <code>Symbol.for('unique.identifier')</code>. Because the global symbol registry is shared across the entire JavaScript runtime process, both CJS and ESM wrappers will resolve to the same underlying instance.
</details>

<details>
<summary>5. How do singletons negatively impact automated unit testing?</summary>
Singletons introduce global shared mutable state. Modifications made to a singleton during Test A persist when Test B executes, violating test isolation and causing order-dependent test failures. To prevent this, singletons must expose explicit reset mechanisms (such as <code>_resetForTesting()</code>).
</details>

<details>
<summary>6. How can a class constructor intercept instantiation to enforce a singleton?</summary>
By storing the created instance in a private static field (e.g., <code>static #instance</code>) during the first call, and checking that field in the constructor. If the field already holds an instance, the constructor returns that existing instance instead of returning <code>this</code>.
</details>

<details>
<summary>7. What is a Multiton pattern?</summary>
A Multiton is a generalized extension of the Singleton pattern that manages a keyed map of singletons. Instead of maintaining a single global instance, it guarantees exactly one singleton instance <i>per unique key</i> (e.g., one connection pool per tenant ID).
</details>

<details>
<summary>8. Why is Object.freeze() recommended for configuration singletons?</summary>
By default, JavaScript objects are mutable. Without <code>Object.freeze()</code>, any part of the application can mutate configuration properties or overwrite methods on the singleton instance, compromising system stability.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Thread-Safe-Style In-Memory Lock Manager Singleton
Implement a singleton class `LockManager` providing `acquire(lockKey)` and `release(lockKey)` methods that ensures only one task can hold a named lock at a time, rejecting concurrent attempts on the same key.

<details>
<summary>View Solution</summary>

```javascript
export class LockManager {
  static #instance = null;
  #locks = new Set();

  constructor() {
    if (LockManager.#instance) {
      return LockManager.#instance;
    }
    LockManager.#instance = this;
  }

  acquire(lockKey) {
    if (this.#locks.has(lockKey)) {
      return false; // Lock already held
    }
    this.#locks.add(lockKey);
    return true; // Successfully acquired
  }

  release(lockKey) {
    return this.#locks.delete(lockKey);
  }

  static getInstance() {
    if (!LockManager.#instance) {
      LockManager.#instance = new LockManager();
    }
    return LockManager.#instance;
  }
}
```
</details>

### Exercise 2: Resettable Singleton with Snapshot & Restore
Implement a stateful configuration singleton `RuntimeConfig` that provides a `snapshot()` method to capture state and a `restore()` method to revert state, allowing unit tests to mutate configuration safely and restore it in `afterEach()`.

<details>
<summary>View Solution</summary>

```javascript
export class RuntimeConfig {
  static #instance = null;
  #config = new Map();
  #snapshot = null;

  constructor() {
    if (RuntimeConfig.#instance) {
      return RuntimeConfig.#instance;
    }
    RuntimeConfig.#instance = this;
  }

  set(key, value) {
    this.#config.set(key, value);
  }

  get(key) {
    return this.#config.get(key);
  }

  snapshot() {
    this.#snapshot = new Map(this.#config);
  }

  restore() {
    if (this.#snapshot) {
      this.#config = new Map(this.#snapshot);
    }
  }

  static getInstance() {
    if (!RuntimeConfig.#instance) {
      RuntimeConfig.#instance = new RuntimeConfig();
    }
    return RuntimeConfig.#instance;
  }
}
```
</details>
