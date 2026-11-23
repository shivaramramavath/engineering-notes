# JavaScript Symbols: Unique Identifiers, Well-Known Symbols & Metaprogramming

> Comprehensive technical reference on ECMAScript Symbols, primitive uniqueness, private object properties, the Global Symbol Registry, well-known symbols, and metaprogramming hooks.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Metaprogramming |
| **Prerequisites** | [01_fundamentals/02_data-types.md](../01_fundamentals/02_data-types.md), [03_objects-and-arrays/01_object-basics.md](../03_objects-and-arrays/01_object-basics.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Symbols, Registry, Well-Known Symbols) / ES2019 (Symbol.prototype.description) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Uniqueness, Immutability & Registry](#2-core-concepts-uniqueness-immutability--registry)
3. [How It Works: Engine Symbol Storage & Property Lookups](#3-how-it-works-engine-symbol-storage--property-lookups)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Symbol Representation & Name Dictionaries](#9-engine-internals-v8-symbol-representation--name-dictionaries)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Cross-Library Metadata & Protocol Implementation](#11-real-world-case-study-cross-library-metadata--protocol-implementation)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6), all object property keys were strictly strings. This created a persistent problem for library authors, framework maintainers, and large codebases: **property name collisions**.

If two independent libraries attached metadata or helper methods to a shared object (such as a DOM node or a global configuration object), they risked overwriting each other's properties:
```javascript
// Pre-ES6 collision risk:
domNode._customCache = { count: 1 };
thirdPartyLib.attachState(domNode); // Overwrites _customCache with a boolean!
```

**Symbols** introduced a new primitive data type designed specifically for guaranteed-unique property keys and engine-level protocol hooks:
1. **Guaranteed Uniqueness:** Every symbol created via `Symbol()` is globally unique, even if given identical descriptions.
2. **Hidden by Default:** Symbols do not appear in `for...in` loops, `Object.keys()`, or `JSON.stringify()`, preventing unintended serialization and iteration pollution.
3. **Language Extensibility (Metaprogramming):** Through **well-known symbols** (e.g., `Symbol.iterator`, `Symbol.toPrimitive`, `Symbol.hasInstance`), developers can customize core engine behaviors without breaking existing string-based object semantics.

---

## 2. Core Concepts: Uniqueness, Immutability & Registry

### 2.1 Primitive Identity
A Symbol is an immutable, primitive value. It is not an object, cannot be instantiated with `new Symbol()` (which throws `TypeError`), and has no literal syntax:
```javascript
const s1 = Symbol('config');
const s2 = Symbol('config');

console.log(s1 === s2); // false
console.log(typeof s1); // "symbol"
```

### 2.2 Global Symbol Registry
While local symbols are always unique, applications often need to share symbols across different runtime contexts (such as between multiple micro-frontends, iframes, web workers, or Node.js VM contexts). The **Global Symbol Registry** provides a shared runtime dictionary:
- `Symbol.for(key)`: Looks up `key` in the global registry. If found, returns the existing symbol; if not, creates a new symbol, registers it under `key`, and returns it.
- `Symbol.keyFor(sym)`: Returns the string key for a symbol registered in the global registry, or `undefined` if the symbol was created locally.

```
+-------------------------------------------------------------+
|                   Global Symbol Registry                    |
|  "app.tenant"   --->  Symbol(app.tenant) [Shared across VMs] |
|  "app.metrics"  --->  Symbol(app.metrics)                   |
+-------------------------------------------------------------+
                               ^
                     Symbol.for("app.tenant")
                               |
   [Frame 1 / Script A] <------+------> [Frame 2 / Script B]
```

### 2.3 Well-Known Symbols
ECMAScript standardizes built-in symbols as properties on the `Symbol` constructor. These act as extension points for core language protocols:
- `Symbol.iterator`: Defines default iteration behavior for `for...of` and spread syntax.
- `Symbol.asyncIterator`: Defines async iteration behavior for `for await...of`.
- `Symbol.hasInstance`: Customizes `instanceof` operator evaluations.
- `Symbol.toPrimitive`: Controls object-to-primitive type coercion (`default`, `number`, `string`).
- `Symbol.toStringTag`: Customizes the string returned by `Object.prototype.toString()`.
- `Symbol.species`: Specifies the constructor used to create derived objects in built-ins (e.g., `Array.prototype.map`).
- `Symbol.isConcatSpreadable`: Determines if `Array.prototype.concat` flattens an object.

---

## 3. How It Works: Engine Symbol Storage & Property Lookups

When an object has symbol-keyed properties, the engine separates property storage into string keys and symbol keys within its internal property descriptors:

```
               +--------------------------------------+
               |             JavaScript Object        |
               +--------------------------------------+
               | Hidden Class (Shape / Map pointer)   |
               +--------------------------------------+
               | String Key Properties:               |
               |   - "id": 101                        |
               |   - "name": "Telemetry"              |
               +--------------------------------------+
               | Symbol Key Properties:               |
               |   - Symbol(token): "x87fa9"          |
               |   - Symbol.iterator: [Function]      |
               +--------------------------------------+
                                  |
              Object.keys()       |   Object.getOwnPropertySymbols()
              Returns strings     |   Returns symbols only
                                  v
                        Reflect.ownKeys()
                        Returns [strings, symbols]
```

### Property Enumeration Invariants:
1. `Object.keys()`, `Object.values()`, `Object.entries()`, and `for...in` **ignore** symbol keys completely.
2. `JSON.stringify()` **ignores** symbol keys and symbol values.
3. `Object.getOwnPropertySymbols(obj)` returns an array containing only the symbol keys owned by `obj`.
4. `Reflect.ownKeys(obj)` returns all keys: strings first (ordered by index then insertion), followed by symbols in insertion order.

---

## 4. Syntax & API Reference

### 4.1 Creating Symbols
```javascript
// Local Symbol with optional description:
const sym = Symbol("description");

// Reading description (ES2019):
console.log(sym.description); // "description"

// Conversion rules:
console.log(String(sym));      // "Symbol(description)"
console.log(sym.toString());   // "Symbol(description)"
// console.log(sym + "");      // TypeError: Cannot convert a Symbol value to a string
// console.log(Number(sym));   // TypeError: Cannot convert a Symbol value to a number
console.log(Boolean(sym));     // true (all symbols are truthy)
```

### 4.2 Global Registry API
```javascript
// Global symbol creation / retrieval:
const shared = Symbol.for("core.logger");
const sameShared = Symbol.for("core.logger");
console.log(shared === sameShared); // true

// Retrieve key from symbol:
console.log(Symbol.keyFor(shared)); // "core.logger"

const local = Symbol("core.logger");
console.log(Symbol.keyFor(local));  // undefined (local symbols are unregistered)
```

### 4.3 Well-Known Symbols API Reference
| Symbol | Method Signature Hook | Purpose |
| :--- | :--- | :--- |
| `Symbol.iterator` | `[Symbol.iterator](): Iterator` | Defines iterable protocol |
| `Symbol.asyncIterator` | `[Symbol.asyncIterator](): AsyncIterator` | Defines async iterable protocol |
| `Symbol.hasInstance` | `static [Symbol.hasInstance](instance): boolean` | Overrides `instanceof` check |
| `Symbol.toPrimitive` | `[Symbol.toPrimitive](hint: "number"\|"string"\|"default"): primitive` | Overrides type coercion |
| `Symbol.toStringTag` | `get [Symbol.toStringTag](): string` | Customizes `[object <Tag>]` output |
| `Symbol.isConcatSpreadable`| `[Symbol.isConcatSpreadable]: boolean` | Controls flattening in `Array#concat` |
| `Symbol.species` | `static get [Symbol.species]()` | Constructor for derived objects |
| `Symbol.dispose` (ES2025) | `[Symbol.dispose](): void` | Explicit resource management (`using`) |
| `Symbol.asyncDispose` (ES2025)| `[Symbol.asyncDispose](): Promise<void>` | Async resource cleanup (`await using`) |

---

## 5. Practical Examples

### Example 1: Hidden Object Metadata & Non-Enumerable Properties
```javascript
const METADATA_KEY = Symbol("entity.metadata");

class Entity {
  constructor(id, data) {
    this.id = id;
    this.data = data;
    // Attach hidden internal metadata
    this[METADATA_KEY] = {
      createdAt: Date.now(),
      dirty: false,
      version: 1
    };
  }

  markDirty() {
    this[METADATA_KEY].dirty = true;
    this[METADATA_KEY].version++;
  }

  getMetadata() {
    return { ...this[METADATA_KEY] };
  }
}

const user = new Entity(42, { name: "Grace" });
user.markDirty();

// Regular serialization ignores the symbol:
console.log(JSON.stringify(user));
// Output: {"id":42,"data":{"name":"Grace"}}

// Keys iteration ignores the symbol:
console.log(Object.keys(user)); // ["id", "data"]
```

### Example 2: Custom Coercion via `Symbol.toPrimitive`
```javascript
class Money {
  constructor(amount, currency = "USD") {
    this.amount = amount;
    this.currency = currency;
  }

  [Symbol.toPrimitive](hint) {
    switch (hint) {
      case "number":
        return this.amount;
      case "string":
        return `${this.amount.toFixed(2)} ${this.currency}`;
      case "default":
      default:
        return this.amount; // Used in addition (+) and equality (==)
    }
  }
}

const price = new Money(49.99, "EUR");
console.log(+price);          // 49.99 (number hint)
console.log(`${price}`);       // "49.99 EUR" (string hint)
console.log(price + 10);      // 59.99 (default hint coerced to number)
```

### Example 3: Metaprogramming Type Checks with `Symbol.hasInstance`
```javascript
class IntegerRange {
  constructor(min, max) {
    this.min = min;
    this.max = max;
  }

  // Custom instanceof semantics
  static [Symbol.hasInstance](value) {
    return typeof value === "number" &&
           Number.isInteger(value) &&
           value >= 1 &&
           value <= 100;
  }
}

console.log(42 instanceof IntegerRange);   // true
console.log(105 instanceof IntegerRange);  // false
console.log(3.14 instanceof IntegerRange); // false
console.log("50" instanceof IntegerRange); // false
```

### Example 4: Custom Object Identification with `Symbol.toStringTag`
```javascript
class ApiResponse {
  constructor(status, payload) {
    this.status = status;
    this.payload = payload;
  }

  get [Symbol.toStringTag]() {
    return "ApiResponse";
  }
}

const res = new ApiResponse(200, { ok: true });
console.log(Object.prototype.toString.call(res)); // "[object ApiResponse]"
```

### Example 5: Resource Cleanup with Explicit Resource Management (`Symbol.dispose`)
```javascript
// TC39 Stage 3 / ES2025 Resource Management Protocol
class DatabaseConnection {
  constructor(connectionString) {
    this.connStr = connectionString;
    this.active = true;
    console.log(`Connected to ${this.connStr}`);
  }

  query(sql) {
    if (!this.active) throw new Error("Connection closed");
    return `Results for: ${sql}`;
  }

  [Symbol.dispose]() {
    this.active = false;
    console.log(`Connection to ${this.connStr} closed successfully.`);
  }
}

// Emulating `using` statement scope:
function executeTransaction() {
  const db = new DatabaseConnection("postgres://localhost:5432/test");
  try {
    console.log(db.query("SELECT * FROM users"));
  } finally {
    db[Symbol.dispose](); // In ES2025: handled automatically via `using db = ...`
  }
}

executeTransaction();
// Outputs:
// Connected to postgres://localhost:5432/test
// Results for: SELECT * FROM users
// Connection to postgres://localhost:5432/test closed successfully.
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Symbols Are NOT Truly Private
A common misconception is that Symbols enforce private state. While they do not appear in `for...in` or `Object.keys()`, any code with access to the object can retrieve all symbol keys using `Object.getOwnPropertySymbols()` or `Reflect.ownKeys()`:
```javascript
const secretKey = Symbol("apiKey");
const config = { [secretKey]: "super-secret-1234" };

// Introspection breaches "privacy":
const symbols = Object.getOwnPropertySymbols(config);
console.log(config[symbols[0]]); // "super-secret-1234"
```
For true hard private encapsulation, use ECMAScript Private Class Fields (`#privateField`).

### 6.2 Implicit String Coercion Throws TypeError
Unlike primitives such as numbers and booleans, Symbols cannot be implicitly coerced to strings. This design intentionally prevents symbols from being accidentally coerced into string dictionary keys:
```javascript
const id = Symbol("user_id");

// Throws TypeError:
// const message = "ID is " + id; 

// Explicit conversion required:
const message = `ID is ${String(id)}`; // "ID is Symbol(user_id)"
```

### 6.3 Object Property Collision in Symbol.for()
The Global Symbol Registry is shared across the entire JavaScript runtime process. If two separate modules use generic keys with `Symbol.for()`, they will point to the exact same symbol instance:
```javascript
// Module A
const cacheA = Symbol.for("cache");

// Module B (completely unrelated)
const cacheB = Symbol.for("cache");

console.log(cacheA === cacheB); // true! Potential collision
```
**Fix:** Namespace your global symbol keys using reverse-domain or package naming (e.g., `Symbol.for("myorg.pkg.cache")`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `new Symbol()`** | `const s = new Symbol("id");` | `const s = Symbol("id");` | `Symbol` is a primitive factory, not a constructor; `new` throws `TypeError`. |
| **Relying on Symbols for Security** | Storing passwords on objects via Symbol keys | Use ECMAScript `#private` fields or `WeakMap` | Symbol keys can be inspected via `Object.getOwnPropertySymbols()`. |
| **Un-namespaced Global Symbols** | `Symbol.for("state")` | `Symbol.for("@org/module:state")` | Prevents naming collisions between independent third-party libraries. |
| **Implicit Coercion in Logging** | `console.log("Token: " + tokenSym)` | `console.log("Token:", tokenSym.description)` | Implicit string coercion throws `TypeError`. |
| **Missing `[Symbol.iterator]` return** | Returning non-iterable object from `[Symbol.iterator]` | Return an object implementing `{ next(): { value, done } }` | Violates the Iterator protocol contract. |
| **Serializing Objects with Symbols** | Expecting `JSON.stringify` to capture Symbol properties | Provide a custom `toJSON()` method returning string keys | `JSON.stringify` ignores symbol-keyed properties by specification. |

---

## 8. Performance Characteristics

1. **Property Lookup Speed:**
   - In modern V8, accessing symbol-keyed properties on monomorphic objects has identical execution speed to accessing string-keyed properties (both resolve via constant offsets in the V8 Shape/Hidden Class).
2. **Memory Footprint:**
   - A unique `Symbol` allocated via `Symbol()` is an immutable heap primitive (~24–32 bytes in 64-bit V8).
   - Local symbols are garbage-collected when no longer referenced.
3. **Global Registry Retention:**
   - Symbols created via `Symbol.for()` are anchored in the runtime's Global Symbol Registry table. They persist for the lifetime of the process/isolate and are **not garbage-collected**. Use them sparingly.

---

## 9. Engine Internals: V8 Symbol Representation & Name Dictionaries

Inside the V8 engine, strings and symbols are unified under the internal C++ class `v8::internal::Name`.

```
                  +--------------------------+
                  |  v8::internal::Name      |
                  +--------------------------+
                  |  raw_hash_field()        |
                  +-------------+------------+
                                |
          +---------------------+---------------------+
          |                                           |
+---------------------+                     +---------------------+
| v8::internal::String|                     | v8::internal::Symbol|
+---------------------+                     +---------------------+
| char contents       |                     | flags (well-known)  |
| length              |                     | description (String)|
+---------------------+                     +---------------------+
```

### Key V8 Mechanics:
1. **Hash Generation:** When a `Symbol` is allocated, V8 generates a random 32-bit hash immediately and stores it in the `Name` base header. This guarantees instantaneous hash-table lookups without string hashing overhead.
2. **V8 Shape Transitions:** When an object has a symbol property added:
   ```javascript
   const obj = {};
   const sym = Symbol("feature");
   obj[sym] = true;
   ```
   V8 transitions `obj` to a new `Map` (Hidden Class) whose descriptor array maps the specific `Symbol` pointer to the in-object property slot.
3. **Private Name Symbols:** V8 internally implements JavaScript private fields (`#field`) using internal `PrivateName` symbols. Unlike public symbols, `PrivateName` symbols have an internal flag that hides them from `Object.getOwnPropertySymbols()` and `Reflect.ownKeys()`.

---

## 10. Best Practices & Design Guidelines

1. **Use Symbols for Cross-Cutting Protocol Hooks:** Implement `Symbol.iterator`, `Symbol.toPrimitive`, and `Symbol.toStringTag` to make domain models work naturally with language idioms.
2. **Namespace Registry Keys:** Always use prefix notations like `Symbol.for("domain.package.key")` to prevent cross-library collisions.
3. **Prefer `#private` for True Encapsulation:** If properties must be genuinely inaccessible from outside class methods, use `#privateField`, not Symbols.
4. **Export Symbols for Controlled Public APIs:** If a library needs to provide an extensible plugin hook without polluting the string autocompletion namespace, export a dedicated Symbol:
   ```javascript
   export const PLUGIN_HOOK = Symbol("app.pluginHook");
   ```

---

## 11. Real-World Case Study: Cross-Library Metadata & Protocol Implementation

### Problem
A multi-tier monitoring system collects telemetry across multiple decoupled plugins. Each plugin can attach metrics to standard JavaScript Error objects. String keys like `err.metadata` frequently collided between the logging framework, error tracking (e.g., Sentry), and APM tools.

### Solution
Implement a standardized, namespaced symbol protocol that plugins use to attach and read structured diagnostics without modifying string keys:

```javascript
// telemetry-protocol.js
const DIAGNOSTICS = Symbol.for("@company/telemetry:diagnostics");

function attachDiagnostics(error, metadata) {
  if (typeof error !== "object" || error === null) return error;
  
  const existing = error[DIAGNOSTICS] || [];
  error[DIAGNOSTICS] = [
    ...existing,
    {
      timestamp: Date.now(),
      source: metadata.source,
      context: metadata.context
    }
  ];
  return error;
}

function extractDiagnostics(error) {
  return error?.[DIAGNOSTICS] ?? [];
}

// Service A (Auth Plugin)
const authError = new Error("Invalid Bearer Token");
attachDiagnostics(authError, {
  source: "AuthService",
  context: { tenantId: "tenant_892" }
});

// Service B (Rate Limiter Plugin)
attachDiagnostics(authError, {
  source: "RateLimiter",
  context: { remainingQuota: 0 }
});

// Centralized APM Logger
function logErrorSafely(err) {
  // Stringified error preserves standard Node/Browser JSON output:
  console.log("Serialized payload:", JSON.stringify({ message: err.message }));
  
  // Telemetry consumer extracts structured records cleanly:
  const reports = extractDiagnostics(err);
  console.log(`Extracted ${reports.length} diagnostics events:`, reports);
}

logErrorSafely(authError);
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What are the primary differences between <code>Symbol()</code> and <code>Symbol.for()</code>?</strong></summary>

- `Symbol(desc)` creates a completely unique, locally scoped symbol every time it is invoked. Two calls with the same description are never equal (`Symbol("a") !== Symbol("a")`).
- `Symbol.for(key)` accesses the runtime-wide Global Symbol Registry. If a symbol with `key` exists, it returns it; otherwise it creates and registers one. Two calls with the same key return the exact same symbol (`Symbol.for("a") === Symbol.for("a")`).
- Symbols created with `Symbol()` are garbage collected when unreferenced; symbols in the registry persist for the lifetime of the engine isolate.
</details>

<details>
<summary><strong>Q2: Can Symbols be used to create truly private object properties? Why or why not?</strong></summary>

No. Symbols provide **name-spacing and property hiding**, not security or private encapsulation. Although symbol properties are omitted by `Object.keys()` and `for...in`, any consumer with a reference to the object can retrieve them using `Object.getOwnPropertySymbols(obj)` or `Reflect.ownKeys(obj)`. True privacy requires JavaScript `#private` fields or `WeakMap`.
</details>

<details>
<summary><strong>Q3: How does <code>Symbol.toPrimitive</code> interact with JavaScript's type coercion algorithms?</strong></summary>

When an object is coerced to a primitive (via mathematical operations, string templates, or comparisons), JavaScript checks for the presence of `[Symbol.toPrimitive]`. If present, it calls this method with one of three string "hints":
1. `"number"`: triggered by unary `+`, math operators (`-`, `*`, `/`), or `Math.round()`.
2. `"string"`: triggered by template literals or `String()`.
3. `"default"`: triggered by binary `+` (which supports both numbers and strings) and loose equality `==`.
If `Symbol.toPrimitive` is defined, the engine bypasses `valueOf()` and `toString()`.
</details>

<details>
<summary><strong>Q4: Why does <code>JSON.stringify()</code> omit Symbol properties and Symbol values?</strong></summary>

The JSON data interchange format (RFC 8259) strictly specifies that object keys must be double-quoted strings and values must be valid JSON data types (strings, numbers, objects, arrays, booleans, or null). Because symbols are ECMAScript-specific primitives with no standardized JSON representation, `JSON.stringify()` deliberately ignores symbol-keyed properties and converts symbol values in arrays to `null`.
</details>

<details>
<summary><strong>Q5: What is the purpose of <code>Symbol.species</code> in classes that extend built-ins like <code>Array</code>?</strong></summary>

`Symbol.species` allows a subclass to define which constructor should be used when built-in methods (like `map()`, `filter()`, or `slice()`) return a new instance. By default, `SubArray.prototype.map()` creates an instance of `SubArray`. By overriding `static get [Symbol.species]() { return Array; }`, the developer can force derived operations to return a plain `Array` instead of the subclass.
</details>

<details>
<summary><strong>Q6: How does <code>Symbol.hasInstance</code> alter the behavior of the <code>instanceof</code> operator?</strong></summary>

When `a instanceof B` is executed, the engine invokes `B[Symbol.hasInstance](a)`. By implementing `static [Symbol.hasInstance](instance)` on a class or function, you can provide custom verification logic (e.g., structural type checking or range checking) rather than the standard prototype chain traversal.
</details>

<details>
<summary><strong>Q7: What happens if you try to use a Symbol in template interpolation without explicit conversion?</strong></summary>

Template literal interpolation `${sym}` invokes JavaScript's internal `ToString` conversion on the value. For Symbols, the specification mandates that implicit `ToString` conversions throw a `TypeError`. You must explicitly call `sym.toString()` or `sym.description`.
</details>

<details>
<summary><strong>Q8: How does V8 optimize property access when using Symbols as object keys?</strong></summary>

V8 treats Symbols as `Name` instances alongside Strings. When an object property is keyed by a symbol, V8's Hidden Class (Shape) allocates an internal slot transition mapped directly to that Symbol's pointer address. Monomorphic inline caches (ICs) cache the offset directly, resulting in identical lookup speeds to string-keyed properties.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement an Iterable Matrix using `Symbol.iterator`
Create a `Matrix` class that stores rows and columns of numbers and allows iterating over all elements row-by-row using `for...of`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
class Matrix {
  constructor(grid) {
    this.grid = grid;
  }

  *[Symbol.iterator]() {
    for (const row of this.grid) {
      for (const cell of row) {
        yield cell;
      }
    }
  }
}

const m = new Matrix([
  [1, 2, 3],
  [4, 5, 6]
]);

console.log([...m]); // [1, 2, 3, 4, 5, 6]
```
</details>

### Exercise 2: Safe Object Deep Clone Preserving Symbols
Write a `deepCloneWithSymbols(obj)` function that creates a deep copy of an object while preserving all symbol-keyed properties.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function deepCloneWithSymbols(target, map = new WeakMap()) {
  if (typeof target !== "object" || target === null) return target;
  if (map.has(target)) return map.get(target);

  const clone = Array.isArray(target) ? [] : {};
  map.set(target, clone);

  // Reflect.ownKeys retrieves both string and symbol keys:
  for (const key of Reflect.ownKeys(target)) {
    clone[key] = deepCloneWithSymbols(target[key], map);
  }

  return clone;
}

const sym = Symbol("test");
const original = { a: 1, [sym]: { nested: "val" } };
const cloned = deepCloneWithSymbols(original);

console.log(cloned[sym].nested); // "val"
console.log(cloned[sym] !== original[sym]); // true (deeply cloned)
```
</details>

### Exercise 3: Structural Subtyping Hook via `Symbol.hasInstance`
Create an interface simulator `Interface` such that `obj instanceof Streamable` returns `true` if `obj` implements `read()` and `write()` methods.

<details>
<summary><strong>Solution</strong></summary>

```javascript
class Streamable {
  static [Symbol.hasInstance](instance) {
    return (
      instance !== null &&
      (typeof instance === "object" || typeof instance === "function") &&
      typeof instance.read === "function" &&
      typeof instance.write === "function"
    );
  }
}

const validStream = {
  read() { return "data"; },
  write(chunk) { console.log(chunk); }
};

const invalidStream = {
  read() { return "data"; }
};

console.log(validStream instanceof Streamable);   // true
console.log(invalidStream instanceof Streamable); // false
```
</details>
