# JavaScript Proxy & Reflect: Metaprogramming, Traps, Invariants & Reactivity Engines

> Comprehensive technical guide to ECMAScript `Proxy` and `Reflect`, the 13 fundamental language traps, invariant enforcement, the `receiver` argument, private field gotchas, and V8 `JSProxy` deoptimization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Metaprogramming |
| **Prerequisites** | [03_objects-and-arrays/01_object-basics.md](../03_objects-and-arrays/01_object-basics.md), [05_this-and-oop/03_prototypes-and-prototype-chain.md](../05_this-and-oop/03_prototypes-and-prototype-chain.md) |
| **Time to Read** | 28 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Proxy & Reflect) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Traps, Handlers & Reflect Parity](#2-core-concepts-traps-handlers--reflect-parity)
3. [How It Works: Engine Dispatch Table & Invariant Enforcement](#3-how-it-works-engine-dispatch-table--invariant-enforcement)
4. [The 13 Traps & Reflect API Reference](#4-the-13-traps--reflect-api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: Proxy vs In-Object Access](#8-performance-characteristics-proxy-vs-in-object-access)
9. [Engine Internals: V8 JSProxy Architecture & IC Deoptimization](#9-engine-internals-v8-jsproxy-architecture--ic-deoptimization)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Building a Reactive State Store (Vue 3 Model)](#11-real-world-case-study-building-a-reactive-state-store-vue-3-model)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6), JavaScript had no true metaprogramming or virtualization capabilities. Developers could define getters and setters using `Object.defineProperty()`, but this was strictly limited:
- You could only intercept properties that had **already been defined**.
- You could not intercept property deletions (`delete obj.x`), property enumerations (`key in obj`), method invocations, or the addition of completely new properties.

**`Proxy`** and **`Reflect`** fundamentally changed JavaScript:
- **`Proxy`:** Allows you to wrap any target object (or function) and intercept all 13 internal language operations (called **traps**).
- **`Reflect`:** A built-in namespace object that provides the default implementation for every single trap, returning clean booleans instead of throwing errors.

Together, `Proxy` and `Reflect` power modern frontend frameworks (e.g., Vue 3 Reactivity), RPC client virtualization, runtime schema validators, and transparent data-layer mocks.

---

## 2. Core Concepts: Traps, Handlers & Reflect Parity

### 2.1 Anatomy of a Proxy
A Proxy is created with two objects:
```javascript
const proxy = new Proxy(target, handler);
```
1. **Target:** The original object (or function) being wrapped.
2. **Handler:** An object whose methods define the **traps** that intercept operations.
3. If a trap is not defined in the handler, the operation falls through transparently to the target.

```
 Client Operation: proxy.user = "Ada"
               |
               v
 Handler Trap: handler.set(target, "user", "Ada", receiver)
               |
               +---> Intercepted! Custom logic (validation, telemetry, triggers)
               |
               v
 Reflect Delegate: Reflect.set(target, "user", "Ada", receiver)
               |
               v
 Final Execution: Target object updated!
```

### 2.2 Why Reflect is Essential
Whenever you author a Proxy trap, you should **almost always delegate back using `Reflect`**, not raw object access:
- `Reflect.set()` returns `true` or `false` indicating success, rather than throwing in strict mode.
- `Reflect.get(target, prop, receiver)` correctly passes the `receiver` argument, ensuring that getters inside the target execute with `this` pointing to the Proxy, preserving reactivity.

---

## 3. How It Works: Engine Dispatch Table & Invariant Enforcement

### 3.1 Proxy Invariants
To prevent proxies from completely corrupting ECMAScript language integrity, the specification mandates **Proxy Invariants**. If a trap tries to violate an invariant, the engine throws a `TypeError`:
1. **Non-Configurable Properties:** If `target.x` is non-configurable and non-writable, a `get` trap **must** return the exact same value. It cannot lie and return a different value.
2. **Non-Extensible Objects:** If `Object.preventExtensions(target)` has been called, an `ownKeys` trap cannot return keys that do not exist on the target.

```javascript
const target = {};
Object.defineProperty(target, "immutable", {
  value: 42,
  writable: false,
  configurable: false
});

const proxy = new Proxy(target, {
  get(t, prop) {
    if (prop === "immutable") return 999; // Lying trap!
    return Reflect.get(...arguments);
  }
});

// Throws TypeError: 'get' on proxy: property 'immutable' is a read-only and
// non-configurable data property on the proxy target but the proxy reported a different value
// console.log(proxy.immutable);
```

---

## 4. The 13 Traps & Reflect API Reference

There is an exact 1-to-1 correspondence between the 13 Proxy traps and the static methods on `Reflect`:

| # | Proxy Trap | Reflect Method | Intercepted Operation |
| :--- | :--- | :--- | :--- |
| 1 | `get(t, prop, receiver)` | `Reflect.get(t, prop, receiver)` | `proxy[prop]`, `proxy.prop` |
| 2 | `set(t, prop, val, receiver)`| `Reflect.set(t, prop, val, receiver)` | `proxy[prop] = val` |
| 3 | `has(t, prop)` | `Reflect.has(t, prop)` | `prop in proxy` |
| 4 | `deleteProperty(t, prop)` | `Reflect.deleteProperty(t, prop)` | `delete proxy[prop]` |
| 5 | `apply(t, thisArg, args)` | `Reflect.apply(t, thisArg, args)` | `proxy(...args)`, `fn.apply()` |
| 6 | `construct(t, args, newTarget)`| `Reflect.construct(t, args, newTarget)`| `new proxy(...args)` |
| 7 | `getPrototypeOf(t)` | `Reflect.getPrototypeOf(t)` | `Object.getPrototypeOf(proxy)` |
| 8 | `setPrototypeOf(t, proto)` | `Reflect.setPrototypeOf(t, proto)` | `Object.setPrototypeOf(proxy, proto)`|
| 9 | `isExtensible(t)` | `Reflect.isExtensible(t)` | `Object.isExtensible(proxy)` |
| 10| `preventExtensions(t)` | `Reflect.preventExtensions(t)` | `Object.preventExtensions(proxy)`|
| 11| `getOwnPropertyDescriptor(t, p)`| `Reflect.getOwnPropertyDescriptor(t, p)`| `Object.getOwnPropertyDescriptor()`|
| 12| `defineProperty(t, p, desc)` | `Reflect.defineProperty(t, p, desc)` | `Object.defineProperty(proxy, p, desc)`|
| 13| `ownKeys(t)` | `Reflect.ownKeys(t)` | `Object.keys()`, `Reflect.ownKeys()`|

---

## 5. Practical Examples

### Example 1: Negative Array Indexing via Proxy
In standard JavaScript, `arr[-1]` returns `undefined`. We can virtualize negative indices to access elements from the end:
```javascript
function createSmartArray(...items) {
  const target = [...items];

  return new Proxy(target, {
    get(arr, prop, receiver) {
      if (typeof prop === "string") {
        const index = Number(prop);
        if (Number.isInteger(index) && index < 0) {
          // Wrap negative index:
          const wrapped = arr.length + index;
          return Reflect.get(arr, wrapped, receiver);
        }
      }
      return Reflect.get(arr, prop, receiver);
    },
    set(arr, prop, value, receiver) {
      if (typeof prop === "string") {
        const index = Number(prop);
        if (Number.isInteger(index) && index < 0) {
          const wrapped = arr.length + index;
          return Reflect.set(arr, wrapped, value, receiver);
        }
      }
      return Reflect.set(arr, prop, value, receiver);
    }
  });
}

const list = createSmartArray("a", "b", "c", "d");
console.log(list[-1]); // "d"
console.log(list[-2]); // "c"
list[-1] = "FINAL";
console.log(list[3]);  // "FINAL"
```

### Example 2: Dynamic Fluent RPC / API Client Virtualization
```javascript
function createApiClient(baseUrl) {
  function makeProxy(pathParts = []) {
    return new Proxy(() => {}, {
      get(target, prop) {
        // Build nested path segments:
        return makeProxy([...pathParts, prop]);
      },
      apply(target, thisArg, args) {
        const path = pathParts.join("/");
        const [params] = args;
        return `[HTTP GET] ${baseUrl}/${path}?${new URLSearchParams(params)}`;
      }
    });
  }

  return makeProxy();
}

const api = createApiClient("https://api.github.com");
// Transparently virtualizes nested property paths into API endpoints!
console.log(api.repos.octocat.helloWorld({ page: 1, limit: 10 }));
// Output: "[HTTP GET] https://api.github.com/repos/octocat/helloWorld?page=1&limit=10"
```

### Example 3: Schema Validation & Type Enforcement
```javascript
function createValidatedRecord(schema, initialValues = {}) {
  const target = {};

  const validator = {
    set(obj, prop, value, receiver) {
      const typeDef = schema[prop];
      if (!typeDef) {
        throw new Error(`Property "${prop}" is not allowed in schema.`);
      }

      if (typeof value !== typeDef.type) {
        throw new TypeError(`Expected ${prop} to be a ${typeDef.type}, got ${typeof value}`);
      }

      if (typeDef.validator && !typeDef.validator(value)) {
        throw new RangeError(`Value ${value} failed validation for ${prop}`);
      }

      return Reflect.set(obj, prop, value, receiver);
    }
  };

  const proxy = new Proxy(target, validator);
  Object.assign(proxy, initialValues);
  return proxy;
}

const userSchema = {
  name: { type: "string" },
  age: { type: "number", validator: n => n >= 18 && n <= 120 }
};

const user = createValidatedRecord(userSchema);
user.name = "Ada";
user.age = 25; // Valid

// user.age = 15; // Throws RangeError!
// user.gender = "f"; // Throws Error: Property "gender" is not allowed
```

### Example 4: Revocable Security Proxy via `Proxy.revocable`
```javascript
const sensitiveData = { token: "secret_xyz_9901", balance: 50000 };

const { proxy, revoke } = Proxy.revocable(sensitiveData, {
  get(target, prop, receiver) {
    return Reflect.get(target, prop, receiver);
  }
});

console.log("Token:", proxy.token); // "secret_xyz_9901"

// Revoke access when task finishes:
revoke();

// Subsequent calls throw TypeError: Cannot perform 'get' on a proxy that has been revoked
// console.log(proxy.token);
```

### Example 5: Deep Read-Only Proxy
```javascript
function deepFreezeProxy(target) {
  if (typeof target !== "object" || target === null) return target;

  return new Proxy(target, {
    set(t, prop) {
      throw new TypeError(`Cannot modify read-only property "${prop}".`);
    },
    deleteProperty(t, prop) {
      throw new TypeError(`Cannot delete read-only property "${prop}".`);
    },
    get(t, prop, receiver) {
      const value = Reflect.get(t, prop, receiver);
      return deepFreezeProxy(value); // Recursively wrap children
    }
  });
}

const config = deepFreezeProxy({ database: { port: 5432 } });
console.log(config.database.port); // 5432
// config.database.port = 3306; // Throws TypeError: Cannot modify read-only property
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Private Class Fields (`#field`) Incompatibility
Private identifiers in ECMAScript classes use internal engine slots that require the exact class instance receiver. When a method accesses `#privateField` on a Proxy, V8 throws `TypeError: Cannot read private member from an object whose class did not declare it`:
```javascript
class BankAccount {
  #balance = 1000;

  getBalance() {
    return this.#balance; // 'this' will be the Proxy!
  }
}

const account = new BankAccount();
const proxy = new Proxy(account, {});

// Throws TypeError!
// proxy.getBalance();

// Fix: Bind methods to the original target when retrieved:
const safeProxy = new Proxy(account, {
  get(target, prop, receiver) {
    const val = Reflect.get(target, prop, receiver);
    if (typeof val === "function") {
      return val.bind(target); // Bind 'this' to target, NOT proxy!
    }
    return val;
  }
});
console.log(safeProxy.getBalance()); // 1000 (Safe!)
```

### 6.2 The Importance of the `receiver` Parameter
When an object has a getter that references `this`, failing to pass `receiver` to `Reflect.get()` breaks prototype inheritance and reactivity:
```javascript
const target = {
  _val: 10,
  get val() { return this._val; }
};

const proxy = new Proxy(target, {
  get(t, prop, receiver) {
    // WRONG: return t[prop]; (Bypasses receiver!)
    // RIGHT:
    return Reflect.get(t, prop, receiver);
  }
});
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `target[prop]` inside traps** | `set(t, p, v) { t[p] = v; return true; }` | `return Reflect.set(t, p, v, receiver)` | Bypasses getters/setters and drops the `receiver` binding. |
| **Omitting Boolean return in `set` trap** | `set(t, p, v) { Reflect.set(...); }` | Always return boolean (`return Reflect.set(...)`) | In strict mode, returning falsy/undefined causes a `TypeError`. |
| **Accessing Private Fields via Proxy** | `proxy.methodAccessingPrivate()` | Bind method closures to `target` in `get` | Private fields (`#`) require the true internal brand instance. |
| **Overriding Invariant Properties** | Returning forged values for non-configurable fields | Check property descriptors before forging | Violating engine invariants throws `TypeError`. |
| **Using Proxies in Extreme Inner Loops** | Wrapping 10,000,000 array elements in Proxies | Use plain arrays/TypedArrays for hot math | Proxies bypass TurboFan Inline Caches (ICs), causing $10\times$ slowdowns. |
| **Identity Comparison Mismatch** | `if (proxy === target)` | Compare against known target references | A Proxy and its Target are two distinct object references (`proxy !== target`). |

---

## 8. Performance Characteristics: Proxy vs In-Object Access

1. **Inline Cache (IC) Deoptimization:**
   - On standard objects, V8 caches property offsets directly in assembly instructions (Monomorphic IC). Lookups take $\approx 1\text{ ns}$.
   - On a `Proxy`, V8 cannot predict the result of dynamic handler traps. Every property access must miss the IC and jump into the C++ `Runtime_ProxyGet` engine dispatch table. Lookups take $\approx 15\text{–}30\text{ ns}$ ($15\times$ slower).
2. **Allocation Overhead:**
   - Instantiating a `new Proxy()` allocates a `JSProxy` wrapper object (~40 bytes) referencing both target and handler.
3. **Architectural Sweet Spot:**
   - Proxies are outstanding for **macro-architectural boundaries** (Reactivity systems, ORM queries, Dependency Injection containers, API mocks). They should **not** be placed in tight rendering loops or 60 FPS animation updates.

---

## 9. Engine Internals: V8 JSProxy Architecture & IC Deoptimization

In V8, every JavaScript object inherits from `v8::internal::JSReceiver`.
- Plain objects are `v8::internal::JSObject`.
- Proxies are `v8::internal::JSProxy`.

```
                  +-----------------------------------+
                  |      v8::internal::JSProxy        |
                  +-----------------------------------+
                  | Shape / Map (Always Proxy Map)    |
                  | target: HeapObjectRef             | ---> Points to Target JSObject
                  | handler: HeapObjectRef            | ---> Points to Handler JSObject
                  +-----------------------------------+
```

### Trap Invocation Flow:
1. When TurboFan encounters a property read on a `JSProxy`, it cannot emit a direct memory load.
2. It calls `v8::internal::JSProxy::GetProperty()`.
3. The C++ runtime checks if the handler defines the `"get"` trap.
4. If found, V8 switches execution context and invokes the JavaScript trap function with `(target, property, receiver)`.
5. After the trap returns, V8 runs **`CheckGetTrapResult()`** to verify that no non-configurable invariants were violated before returning the value to the caller.

---

## 10. Best Practices & Design Guidelines

1. **Always Return Booleans in `set` and `deleteProperty`:** Ensure traps return `true` on success and `false` on failure to maintain strict mode compatibility.
2. **Forward All Traps with `Reflect`:** Use `Reflect[trapName](...arguments)` as the canonical fallback.
3. **Handle Methods Accessing Private Fields:** If wrapping class instances with private fields (`#`), bind methods to `target` inside the `get` trap.
4. **Use Revocable Proxies for Security Boundaries:** When handing data to untrusted third-party plugins, wrap it in `Proxy.revocable()` and invoke `revoke()` when the plugin turn ends.

---

## 11. Real-World Case Study: Building a Reactive State Store (Vue 3 Model)

### Problem
A component library required automatic DOM re-rendering whenever state mutated. Previous implementations used `Object.defineProperty()`, which failed to detect newly added properties (`state.newProp = 10`) or array modifications (`state.list.push(item)`), requiring clunky helpers like `Vue.set()`.

### Solution
Implement a clean, recursive reactivity engine modeled after Vue 3 using `Proxy`, `Reflect`, and a `WeakMap` dependency tracker:

```javascript
// Dependency tracking mechanics:
let activeEffect = null;
const targetMap = new WeakMap();

function track(target, key) {
  if (!activeEffect) return;
  let depsMap = targetMap.get(target);
  if (!depsMap) {
    depsMap = new Map();
    targetMap.set(target, depsMap);
  }
  let dep = depsMap.get(key);
  if (!dep) {
    dep = new Set();
    depsMap.set(key, dep);
  }
  dep.add(activeEffect);
}

function trigger(target, key) {
  const depsMap = targetMap.get(target);
  if (!depsMap) return;
  const dep = depsMap.get(key);
  if (dep) {
    dep.forEach(effect => effect());
  }
}

// Reactive Proxy Factory:
function reactive(target) {
  if (typeof target !== "object" || target === null) return target;

  return new Proxy(target, {
    get(t, prop, receiver) {
      track(t, prop);
      const res = Reflect.get(t, prop, receiver);
      return typeof res === "object" && res !== null ? reactive(res) : res; // Deep reactive
    },
    set(t, prop, value, receiver) {
      const oldValue = t[prop];
      const result = Reflect.set(t, prop, value, receiver);
      if (oldValue !== value) {
        trigger(t, prop);
      }
      return result;
    }
  });
}

function watchEffect(effectFn) {
  activeEffect = effectFn;
  effectFn(); // Run once to register dependencies
  activeEffect = null;
}

// Testing the Reactivity Engine:
const state = reactive({ count: 0, user: { name: "Alan" } });

watchEffect(() => {
  console.log(`[UI Auto-Render] Count: ${state.count}, User: ${state.user.name}`);
});

// Mutating state triggers automatic re-renders!
state.count = 1;        // Logs: [UI Auto-Render] Count: 1, User: Alan
state.user.name = "Ada"; // Logs: [UI Auto-Render] Count: 1, User: Ada
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why is it important to pass the <code>receiver</code> argument to <code>Reflect.get()</code> inside a Proxy trap?</strong></summary>

The `receiver` argument sets the value of `this` when a property getter is invoked. If an object has a getter `get fullName() { return this.first + " " + this.last; }`, passing `receiver` ensures that `this` points to the `Proxy` rather than the underlying `target`. This is crucial for reactivity systems, allowing the reactive proxy to track accesses to `this.first` and `this.last`.
</details>

<details>
<summary><strong>Q2: What happens if a Proxy trap violates an ECMAScript invariant?</strong></summary>

The engine throws a `TypeError`. Invariants are inviolable rules enforced by the specification to prevent proxies from breaking fundamental language invariants (such as reporting that a non-configurable, non-writable property has changed, or reporting that an extensible object is non-extensible).
</details>

<details>
<summary><strong>Q3: Why do private class fields (<code>#private</code>) fail when invoked on a Proxy?</strong></summary>

Private fields in ECMAScript use internal brand verification: the engine verifies that the underlying C++ heap instance was instantiated by that specific class constructor. A `Proxy` is an instance of `JSProxy`, not the target class. When a method executes `this.#field`, V8 inspects the `this` receiver (which is the Proxy) and fails the brand check, throwing `TypeError`.
</details>

<details>
<summary><strong>Q4: What is a revocable proxy, and how is it used?</strong></summary>

A revocable proxy is created using `Proxy.revocable(target, handler)`. It returns an object `{ proxy, revoke }`. When `revoke()` is called, the proxy is permanently disconnected from the target. Any subsequent attempt to access or mutate properties on the proxy throws a `TypeError`. It is used to enforce zero-trust security boundaries when passing data to third-party modules.
</details>

<details>
<summary><strong>Q5: Can a Proxy intercept the <code>typeof</code> operator?</strong></summary>

No. `typeof` is not an interceptable trap. If the target is a function, `typeof proxy` evaluates to `"function"`; if the target is an object, it evaluates to `"object"`. The engine never invokes a trap for `typeof`.
</details>

<details>
<summary><strong>Q6: How does <code>Reflect.set()</code> improve upon standard assignment <code>target[prop] = val</code>?</strong></summary>

Standard assignment throws a `TypeError` in strict mode if the assignment fails (e.g., if the property is read-only). `Reflect.set()` does not throw; it returns a boolean (`true` if the assignment succeeded, `false` otherwise), simplifying defensive programming.
</details>

<details>
<summary><strong>Q7: Why do Proxies have lower performance than standard objects in V8?</strong></summary>

Standard objects use V8's Inline Caches (ICs) that compile property offsets directly into machine assembly instructions. Because Proxies have dynamic trap handlers that can execute arbitrary code, property lookups cannot be cached via monomorphic ICs. Every access must miss the IC and invoke the engine runtime C++ dispatch table.
</details>

<details>
<summary><strong>Q8: What is the <code>ownKeys</code> trap used for, and what must it return?</strong></summary>

The `ownKeys` trap intercepts `Object.keys()`, `Object.getOwnPropertyNames()`, `Object.getOwnPropertySymbols()`, and `Reflect.ownKeys()`. It must return an array of strings and symbols representing the object's own property keys.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Default Dict Implementation via Proxy
Implement a Python-style `defaultdict(defaultFactory)` in JavaScript using `Proxy`. When an unassigned property is accessed, it automatically initializes the property using `defaultFactory()`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function defaultDict(defaultFactory) {
  return new Proxy({}, {
    get(target, prop, receiver) {
      if (!(prop in target) && typeof prop === "string") {
        target[prop] = defaultFactory();
      }
      return Reflect.get(target, prop, receiver);
    }
  });
}

const counts = defaultDict(() => 0);
counts.apples += 1;
counts.bananas += 5;

console.log(counts.apples);  // 1
console.log(counts.bananas); // 5
console.log(counts.oranges); // 0 (Auto-initialized!)
```
</details>

### Exercise 2: Change-Tracking Dirty Proxy
Create an `auditProxy(target)` that records every mutation made to an object, maintaining a `changelog` array containing `{ prop, oldValue, newValue, timestamp }`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function auditProxy(target) {
  const changelog = [];

  const proxy = new Proxy(target, {
    set(t, prop, newValue, receiver) {
      const oldValue = t[prop];
      changelog.push({
        prop,
        oldValue,
        newValue,
        timestamp: Date.now()
      });
      return Reflect.set(t, prop, newValue, receiver);
    }
  });

  return { proxy, changelog };
}

const { proxy, changelog } = auditProxy({ title: "Draft" });
proxy.title = "Review";
proxy.title = "Published";

console.log("Current Title:", proxy.title);
console.log("Changelog entries:", changelog.length); // 2
console.log(changelog[0]); // { prop: 'title', oldValue: 'Draft', newValue: 'Review', ... }
```
</details>

### Exercise 3: Safe Method Binder for Private Fields
Implement a proxy wrapper that transparently allows class instances with private fields to have their methods invoked without throwing `TypeError`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function makePrivateSafeProxy(instance, handler = {}) {
  return new Proxy(instance, {
    ...handler,
    get(target, prop, receiver) {
      const value = Reflect.get(target, prop, receiver);
      // If retrieved value is a method, bind it explicitly to target:
      if (typeof value === "function") {
        return value.bind(target);
      }
      if (handler.get) {
        return handler.get(target, prop, receiver);
      }
      return value;
    }
  });
}

class SecretVault {
  #code = "1234-OPEN";
  reveal() {
    return `Vault unlocked with ${this.#code}`;
  }
}

const vault = new SecretVault();
const safeVault = makePrivateSafeProxy(vault);

console.log(safeVault.reveal()); // "Vault unlocked with 1234-OPEN" (Zero TypeError!)
```
</details>
