# JavaScript Classes: ES6+ Syntax, Desugaring & Class Fields

> In-depth exploration of ECMAScript classes, constructor desugaring, prototype method non-enumerability, public/static fields, and ES2022 static initialization blocks.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [05_this-and-oop/03_prototypes-and-prototype-chain.md](./03_prototypes-and-prototype-chain.md), [05_this-and-oop/04_constructor-functions.md](./04_constructor-functions.md) |
| **Time to Read** | 23 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Classes) / ES2022 (Public Fields & Static Blocks) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Class Anatomy & Syntax](#2-core-concepts-class-anatomy--syntax)
3. [How It Works: Engine Desugaring to Prototype Mechanics](#3-how-it-works-engine-desugaring-to-prototype-mechanics)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Class Boilerplates & Call Enforcements](#9-engine-internals-v8-class-boilerplates--call-enforcements)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Enterprise Event-Driven Pub/Sub Channel](#11-real-world-case-study-enterprise-event-driven-pubsub-channel)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Introduced in ECMAScript 2015 (ES6), the `class` keyword provides a formal, standardized syntax for creating object blueprints and establishing prototypal inheritance. 

Prior to ES6, object-oriented JavaScript required manual constructor wiring, prototype assignments, and boilerplates (`Parent.call(this)`, `Object.create()`, restoring `constructor`). While ECMAScript classes are often described as "syntactic sugar" over prototypes, they introduce critical language-level semantics that cannot be fully replicated with plain functions—such as mandatory `new` enforcement, non-enumerable prototype methods, and Temporal Dead Zone evaluation.

---

## 2. Core Concepts: Class Anatomy & Syntax

```
                                Anatomy of a Modern ES Class
┌────────────────────────────────────────────────────────────────────────────┐
│ class DataService {                                                        │
│   // 1. Public Instance Field (ES2022)                                     │
│   endpoint = "https://api.domain.com";                                     │
│                                                                            │
│   // 2. Static Field (ES2022)                                              │
│   static DEFAULT_TIMEOUT = 5000;                                           │
│                                                                            │
│   // 3. Static Initialization Block (ES2022)                               │
│   static {                                                                 │
│     // Complex static configuration logic runs once at class evaluation    │
│   }                                                                        │
│                                                                            │
│   // 4. Constructor Method                                                 │
│   constructor(apiKey) {                                                    │
│     this.apiKey = apiKey;                                                  │
│   }                                                                        │
│                                                                            │
│   // 5. Prototype Method (shared via DataService.prototype)                │
│   fetchData() { /* ... */ }                                                │
│                                                                            │
│   // 6. Accessor Properties (Getters / Setters)                            │
│   get isReady() { return Boolean(this.apiKey); }                           │
│ }                                                                          │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. How It Works: Engine Desugaring to Prototype Mechanics

When the engine evaluates a class declaration, it builds standard constructor and prototype objects with strict runtime semantics:

```
Class Declaration:
class User {
  constructor(name) { this.name = name; }
  sayHi() { return this.name; }
}

Desugars conceptually into:
1. function User(name) {
     if (!new.target) throw new TypeError("Class constructor cannot be invoked without 'new'");
     this.name = name;
   }
2. Object.defineProperty(User.prototype, "sayHi", {
     value: function() { return this.name; },
     writable: true,
     enumerable: false, // Class methods are NON-ENUMERABLE!
     configurable: true
   });
```

### The 5 Essential Differences Between Classes and Functions
1. **Mandatory `new`**: Calling `User()` without `new` throws `TypeError: Class constructor User cannot be invoked without 'new'`.
2. **Temporal Dead Zone**: Class declarations are not hoisted like function declarations; accessing a class before its declaration line throws `ReferenceError`.
3. **Automatic Strict Mode**: The entire class body is executed in strict mode automatically.
4. **Non-Enumerable Methods**: All methods defined in a class body have `enumerable: false`. `Object.keys(instance)` will never list prototype methods.
5. **No `prototype` on Methods**: Class methods do not have a `.prototype` property and cannot be called with `new`.

---

## 4. Syntax & API Reference

### Declarations vs. Expressions
```javascript
// 1. Class Declaration
class Box {
  constructor(size) { this.size = size; }
}

// 2. Named or Anonymous Class Expression
const DynamicService = class ServiceClass {
  identify() { return ServiceClass.name; }
};
```

### Static Initialization Blocks (ES2022)
Evaluated once when the class is defined, allowing multi-statement initialization of static state, private static access, or external registration:
```javascript
class Database {
  static connection;
  static {
    try {
      this.connection = "ActivePool";
    } catch {
      this.connection = "FallbackPool";
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Class with Accessors & Static Members
```javascript
class Temperature {
  #celsius = 0; // Private field

  constructor(celsius) {
    this.#celsius = celsius;
  }

  // Getter / Setter
  get fahrenheit() {
    return (this.#celsius * 9) / 5 + 32;
  }

  set fahrenheit(f) {
    this.#celsius = ((f - 32) * 5) / 9;
  }

  // Static Factory Method
  static fromFahrenheit(f) {
    const t = new Temperature(0);
    t.fahrenheit = f;
    return t;
  }
}

const boiling = new Temperature(100);
console.log("Boiling in F:", boiling.fahrenheit); // 212

const freezing = Temperature.fromFahrenheit(32);
console.log("Freezing in C:", freezing.fahrenheit); // 32
```

### Example 2: Real-World Public Class Fields vs Prototype Methods
```javascript
class EventHandler {
  // Public field assigned as arrow function -> Auto-bound to this instance!
  handleEvent = (event) => {
    console.log(`Handled ${event} on instance ${this.id}`);
  };

  constructor(id) {
    this.id = id;
  }

  // Standard prototype method -> Shared across all instances
  describe() {
    return `EventHandler ID: ${this.id}`;
  }
}

const handler = new EventHandler("EH_01");

// Detached callback execution works cleanly because handleEvent is auto-bound:
const detached = handler.handleEvent;
detached("click"); // "Handled click on instance EH_01"

// Prototype inspection:
console.log(Object.hasOwn(handler, "handleEvent")); // true (own property)
console.log(Object.hasOwn(handler, "describe"));    // false (on prototype)
```

### Example 3: Edge Case with Method Non-Enumerability
```javascript
class Product {
  constructor(title, price) {
    this.title = title;
    this.price = price;
  }

  getSummary() {
    return `${this.title}: $${this.price}`;
  }
}

const item = new Product("Laptop", 1200);

// Class methods are NOT enumerable:
console.log(Object.keys(item)); 
// Output: ['title', 'price'] (getSummary is omitted!)

// for...in loops skip class methods:
const keysInLoop = [];
for (const k in item) keysInLoop.push(k);
console.log(keysInLoop); // ['title', 'price']
```

### Example 4: Failure Mode with Invoking Class Without 'new'
```javascript
class Server {
  constructor(port) {
    this.port = port;
  }
}

try {
  // Failure mode: Calling class like a regular function
  // @ts-ignore
  Server(8080);
} catch (err) {
  console.log("Caught expected error:", err.message);
  // Class constructor Server cannot be invoked without 'new'
}
```

### Example 5: Idiomatic Static Block Initializing Private State
```javascript
let grantAccess;

class SecretVault {
  #secretKey;

  constructor(key) {
    this.#secretKey = key;
  }

  // Static initialization block exposes private access to privileged module scope
  static {
    grantAccess = (vaultInstance) => vaultInstance.#secretKey;
  }
}

const vault = new SecretVault("classified_encryption_token");
// External module can access via bridge without exposing public field:
console.log("Privileged extraction:", grantAccess(vault));
// "classified_encryption_token"
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Class Declarations Are Subject to TDZ**: Unlike function declarations, classes are not initialized upon hoisting:
   ```javascript
   // Throws ReferenceError: Cannot access 'Car' before initialization
   // const c = new Car();
   class Car {}
   ```
2. **Computed Method Names**: Method names can be computed at runtime using square brackets:
   ```javascript
   const methodKey = "dynamicAction";
   class Action {
     [methodKey]() { return "Executed dynamic action"; }
   }
   ```
3. **No Commas Allowed in Class Bodies**: Unlike object literals, class members are separated by newlines or semicolons, **not commas**. Putting a comma between methods throws a `SyntaxError`.
4. **`this` in Static Methods**: Inside a `static` method, `this` refers to the **class constructor itself**, not an instance of the class.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Commas between class methods | `class C { a() {}, b() {} }` | `class C { a() {} b() {} }` | Throws `SyntaxError: Unexpected token ','`. |
| Invoking class without `new` | `const c = Client();` | `const c = new Client();` | Throws `TypeError: cannot be invoked without 'new'`. |
| Over-using arrow class fields | `class C { m = () => {} }` | Prototype method `m() {}` | Wastes memory by creating duplicate closures per instance. |
| Expecting class hoisting | Using class before line | Declare class before use | Classes are in the TDZ and throw `ReferenceError`. |
| Using `this` in static method for state | `static fn() { return this.name; }` | Use static fields | `this` in static methods points to the constructor function. |
| Reassigning class inside method | `class C { f() { C = 1; } }` | Do not reassign class binding | Class name binding is `const`-like inside class body. |

---

## 8. Performance Characteristics

- **Instantiation**: Modern V8 TurboFan allocates class instances via optimized constructor stubs identical to native constructors.
- **Method Invocation**: Prototype methods use V8's monomorphic method inlining, executing at near C++ direct call speeds.
- **Class Fields Performance**: Public class fields (`x = 1`) are assigned during the constructor run. If defined consistently, all instances share the exact same V8 Hidden Class (`Map`).

---

## 9. Engine Internals: V8 Class Boilerplates & Call Enforcements

Inside Google V8:
1. **Class Boilerplate**: When V8 parses a class, it constructs a `ClassBoilerplate` object that pre-allocates the constructor function object and its associated `prototype` object.
2. **`[[IsClassConstructor]]` Internal Bit**: V8 sets a flag `is_class_constructor = true` on the `SharedFunctionInfo` structure of the class.
3. When the function is called, Ignition's call prologue checks this bit:
   - If invoked via `Call`: V8 immediately throws `TypeError: Class constructor ... cannot be invoked without 'new'`.
   - If invoked via `Construct`: Execution proceeds to the constructor body.

---

## 10. Best Practices & Design Guidelines

1. **Use Classes for Stateful Entities**: Use classes when modeling entities with lifecycle, identity, and shared behavior.
2. **Prefer Standard Prototype Methods for Shared Behavior**: Avoid turning every method into an arrow class field (`method = () => {}`); reserve arrow fields for callbacks passed across framework boundaries (like React event handlers).
3. **Use Static Factory Methods for Complex Instantiation**: Provide static methods like `fromJSON()` or `fromConfig()` to clarify construction logic.

---

## 11. Real-World Case Study: Enterprise Event-Driven Pub/Sub Channel

An enterprise messaging channel implementing subscription management, payload broadcasting, and clean disposal semantics using modern class architecture.

```javascript
class MessageChannel {
  // Static configuration
  static MAX_LISTENERS = 50;

  // Public instance fields
  channelName;
  #subscriptions = new Set();

  constructor(channelName) {
    if (!channelName || typeof channelName !== "string") {
      throw new TypeError("Channel name must be a non-empty string");
    }
    this.channelName = channelName;
  }

  // Subscribe method returning an unsubscribe callback
  subscribe(listener) {
    if (typeof listener !== "function") {
      throw new TypeError("Listener must be a function");
    }
    if (this.#subscriptions.size >= MessageChannel.MAX_LISTENERS) {
      console.warn(`[${this.channelName}] Warning: Max listeners exceeded`);
    }

    this.#subscriptions.add(listener);
    return () => this.unsubscribe(listener);
  }

  unsubscribe(listener) {
    return this.#subscriptions.delete(listener);
  }

  publish(message) {
    const envelope = {
      channel: this.channelName,
      payload: message,
      timestamp: new Date().toISOString()
    };

    for (const listener of this.#subscriptions) {
      try {
        listener(envelope);
      } catch (err) {
        console.error(`[${this.channelName}] Error in subscriber:`, err);
      }
    }
  }

  get subscriberCount() {
    return this.#subscriptions.size;
  }
}

// Verification
const telemetry = new MessageChannel("telemetry.metrics");
const unsubscribe = telemetry.subscribe(evt => {
  console.log(`Received on ${evt.channel}:`, evt.payload);
});

telemetry.publish({ cpu: 82.5, memoryMb: 1024 });
console.log("Subscribers:", telemetry.subscriberCount); // 1
unsubscribe();
console.log("Subscribers after unsubscribe:", telemetry.subscriberCount); // 0
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. Why can an ES6 class not be invoked without the <code>new</code> operator?</strong></summary>

The ECMAScript specification explicitly sets the `[[IsClassConstructor]]` internal slot to `true` on class constructors. When a function call is evaluated, if `[[IsClassConstructor]]` is `true` and the call is not a `[[Construct]]` invocation, the engine must throw a `TypeError`. This prevents accidental pollution of `this` and enforces clean object-oriented instantiation.
</details>

<details>
<summary><strong>2. How does method enumerability differ between ES6 classes and ES5 constructors?</strong></summary>

In ES5 constructor functions, attaching methods to the prototype manually (`User.prototype.sayHi = fn`) creates properties that are **enumerable by default** (`enumerable: true`). In ES6 classes, all methods, getters, and setters defined in the class body are created with `enumerable: false`, ensuring they do not appear in `Object.keys()` or `for...in` loops.
</details>

<details>
<summary><strong>3. Are ES6 class declarations hoisted?</strong></summary>

Yes, they are hoisted into the enclosing Lexical Environment during the Creation Phase, but they are left **uninitialized** in the Temporal Dead Zone (TDZ). Attempting to instantiate a class before its declaration line results in a `ReferenceError`.
</details>

<details>
<summary><strong>4. What is the difference between a class method and an arrow property in a class?</strong></summary>

A class method (`sayHi() {}`) is defined once on `Class.prototype` and shared across all instances via prototypal delegation. An arrow property (`sayHi = () => {}`) is an instance field evaluated in the constructor for every single instance created, creating a separate closure function on every object.
</details>

<details>
<summary><strong>5. What is the purpose of Static Initialization Blocks (ES2022)?</strong></summary>

Static initialization blocks (`static { ... }`) allow complex, multi-statement initialization logic for static class fields. They evaluate once at class definition time and have access to private static fields, enabling private state sharing with module-scoped helper functions.
</details>

<details>
<summary><strong>6. What is the binding of <code>this</code> inside a static method?</strong></summary>

Inside a static method, `this` refers to the class constructor function itself (the object on which the static method was called), not an instance of the class.
</details>

<details>
<summary><strong>7. Can a class name be reassigned inside the class body?</strong></summary>

No. Inside the class body, the class identifier acts as an immutable `const` binding. Attempting to reassign it within a method throws a `TypeError: Assignment to constant variable`.
</details>

<details>
<summary><strong>8. What does <code>Object.getPrototypeOf(Class.prototype)</code> evaluate to for a base class?</strong></summary>

For a base class that does not use `extends`, `Object.getPrototypeOf(Class.prototype)` evaluates to `Object.prototype`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Class Desugaring (Easy)
Convert the following ES6 class into equivalent ES5 constructor and prototype code, including non-enumerable method descriptors and a `new.target` check:

```javascript
class Counter {
  constructor(initial = 0) {
    this.count = initial;
  }
  increment() {
    return ++this.count;
  }
}
```

<details>
<summary>View Solution</summary>

```javascript
function Counter(initial) {
  if (!new.target) {
    throw new TypeError("Class constructor Counter cannot be invoked without 'new'");
  }
  this.count = initial !== undefined ? initial : 0;
}

Object.defineProperty(Counter.prototype, "increment", {
  value: function () {
    return ++this.count;
  },
  writable: true,
  enumerable: false, // Class methods are non-enumerable
  configurable: true
});

const c = new Counter(5);
console.log(c.increment()); // 6
console.log(Object.keys(c)); // ['count']
```
</details>

### Exercise 2: Static Factory Method (Easy)
Create a class `Point` with `x` and `y` fields, and add a static factory method `fromArray([x, y])` that returns a new `Point`.

<details>
<summary>View Solution</summary>

```javascript
class Point {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }

  static fromArray([x, y]) {
    return new Point(x, y);
  }
}

const p = Point.fromArray([10, 20]);
console.log(p.x, p.y); // 10, 20
```
</details>

### Exercise 3: Singleton Pattern via Class and Static Block (Medium)
Implement a thread-safe `DatabasePool` class that enforces a strict singleton pattern using ES2022 static blocks and private constructors.

<details>
<summary>View Solution</summary>

```javascript
class DatabasePool {
  static #instance = null;

  constructor() {
    if (DatabasePool.#instance !== null) {
      throw new Error("Cannot instantiate singleton directly. Use DatabasePool.getInstance()");
    }
  }

  static {
    // Lazily or eagerly initialize singleton instance
    DatabasePool.#instance = new DatabasePool();
  }

  static getInstance() {
    return DatabasePool.#instance;
  }

  query(sql) {
    return `Executing: ${sql}`;
  }
}

const pool1 = DatabasePool.getInstance();
const pool2 = DatabasePool.getInstance();
console.log(pool1 === pool2); // true
// new DatabasePool(); // Throws Error
```
</details>

### Exercise 4: Observable Property Class (Medium)
Create a class `ObservableModel` where setting any property via standard setter triggers an `onChange` event callback.

<details>
<summary>View Solution</summary>

```javascript
class ObservableModel {
  #data = {};
  #listeners = [];

  onChange(fn) {
    this.#listeners.push(fn);
  }

  set(key, value) {
    const oldValue = this.#data[key];
    this.#data[key] = value;
    for (const listener of this.#listeners) {
      listener(key, value, oldValue);
    }
  }

  get(key) {
    return this.#data[key];
  }
}

const model = new ObservableModel();
model.onChange((k, newVal, oldVal) => {
  console.log(`Property '${k}' changed from ${oldVal} to ${newVal}`);
});
model.set("theme", "dark");
```
</details>

### Exercise 5: Dynamic Class Generator (Hard)
Write a function `createRecordClass(...fieldNames)` that dynamically generates an ES6 class with typed getters, setters, and an immutable snapshot generator.

<details>
<summary>View Solution</summary>

```javascript
function createRecordClass(...fieldNames) {
  return class DynamicRecord {
    constructor(...values) {
      for (let i = 0; i < fieldNames.length; i++) {
        this[fieldNames[i]] = values[i];
      }
      Object.freeze(this); // Immutability guarantee
    }

    toJSON() {
      return fieldNames.reduce((acc, k) => {
        acc[k] = this[k];
        return acc;
      }, {});
    }
  };
}

const UserRecord = createRecordClass("id", "username", "role");
const user = new UserRecord(101, "ada_l", "admin");
console.log(user.toJSON()); // { id: 101, username: 'ada_l', role: 'admin' }
```
</details>

### Exercise 6: Class Method Call Interceptor (Hard)
Build a class utility `profileMethods(TargetClass)` that wraps all prototype methods of `TargetClass` with execution time loggers while preserving `enumerable: false` descriptor semantics.

<details>
<summary>View Solution</summary>

```javascript
function profileMethods(TargetClass) {
  const proto = TargetClass.prototype;
  for (const name of Object.getOwnPropertyNames(proto)) {
    if (name === "constructor") continue;
    const desc = Object.getOwnPropertyDescriptor(proto, name);
    if (typeof desc.value === "function") {
      const originalMethod = desc.value;
      Object.defineProperty(proto, name, {
        ...desc,
        value: function (...args) {
          const start = performance.now();
          try {
            return originalMethod.apply(this, args);
          } finally {
            console.log(`[Metric] ${TargetClass.name}.${name} took ${(performance.now() - start).toFixed(3)}ms`);
          }
        }
      });
    }
  }
}

class ComputationalTask {
  heavyWork(iterations) {
    let sum = 0;
    for (let i = 0; i < iterations; i++) sum += i;
    return sum;
  }
}

profileMethods(ComputationalTask);
const task = new ComputationalTask();
task.heavyWork(1_000_000);
```
</details>
