# JavaScript Class Inheritance: 'extends', 'super' & Dual Prototype Chains

> In-depth exploration of ECMAScript class inheritance, constructor execution rules with `super()`, method delegation via `[[HomeObject]]`, static inheritance, and built-in subclassing.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | [05_this-and-oop/03_prototypes-and-prototype-chain.md](./03_prototypes-and-prototype-chain.md), [05_this-and-oop/05_classes.md](./05_classes.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (ES6) Class Inheritance Specification |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: The 'extends' and 'super' Keywords](#2-core-concepts-the-extends-and-super-keywords)
3. [How It Works: The Dual Prototype Chain Architecture](#3-how-it-works-the-dual-prototype-chain-architecture)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 [[HomeObject]] & Derived Constructor Stubs](#9-engine-internals-v8-homeobject--derived-constructor-stubs)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Enterprise HTTP Error Hierarchy](#11-real-world-case-study-enterprise-http-error-hierarchy)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Inheritance allows developers to create specialized derived classes that reuse, extend, and customize the behavior of parent base classes. Prior to ECMAScript 2015, subclassing in JavaScript required complex multi-step prototype manipulations:
```javascript
// Pre-ES6 clumsy inheritance wiring:
function Child() { Parent.call(this); }
Child.prototype = Object.create(Parent.prototype);
Child.prototype.constructor = Child;
Object.setPrototypeOf(Child, Parent); // For static inheritance
```

With ES2015, the **`extends`** and **`super`** keywords standardized class inheritance. Unlike pre-ES6 constructor functions—where the child constructor allocates `this` and passes it to the parent constructor—in ES6 derived classes, **the parent constructor allocates `this`**, and derived classes cannot access `this` until calling `super()`.

---

## 2. Core Concepts: The 'extends' and 'super' Keywords

```
                              ES6 Subclassing
                                     │
         ┌───────────────────────────┴───────────────────────────┐
         ▼                                                       ▼
   The 'extends' Keyword                                   The 'super' Keyword
   • Wires prototype chain for instances                   • In constructor: calls parent constructor
   • Wires prototype chain for static methods              • In methods: invokes parent method
   • Enforces derived constructor rules                    • Statically bound via [[HomeObject]]
```

### 1. `extends`
The `extends` keyword establishes inheritance between a derived class and a base class (or any constructor expression).

### 2. `super(...)` in Constructors
In a derived constructor, calling `super(...args)` invokes the superclass constructor. Calling `super()` is **mandatory** before any access to `this` or `return`.

### 3. `super.method()` in Methods
In derived methods, `super.methodName()` delegates to the parent class prototype's version of that method, enabling method extension and polymorphic override.

---

## 3. How It Works: The Dual Prototype Chain Architecture

When you write `class Child extends Parent {}`, the JavaScript engine wires **two distinct prototype chains**:

```
                       The Dual Prototype Chains
                       
1. Instance Method Chain (Delegation for Instances):
┌─────────────────────────┐          ┌─────────────────────────┐
│     Child.prototype     │ ───────► │    Parent.prototype     │ ───────► Object.prototype
└─────────────────────────┘          └─────────────────────────┘

2. Static Property Chain (Delegation for Constructors):
┌─────────────────────────┐          ┌─────────────────────────┐
│     Child Class (Fn)    │ ───────► │    Parent Class (Fn)    │ ───────► Function.prototype
└─────────────────────────┘          └─────────────────────────┘
```

1. **Instance Method Chain**: Any method called on an instance of `Child` (`child.method()`) walks `Child.prototype` ──► `Parent.prototype` ──► `Object.prototype`.
2. **Static Method Chain**: Any static method called on the `Child` constructor (`Child.staticMethod()`) walks `Child` ──► `Parent` ──► `Function.prototype`. This enables static methods to be inherited automatically!

---

## 4. Syntax & API Reference

### Derived Class Syntax
```javascript
class Parent {
  constructor(name) { this.name = name; }
  speak() { return `${this.name} speaks`; }
  static createDefault() { return new this("Default"); }
}

class Child extends Parent {
  constructor(name, age) {
    super(name); // MUST be called before using 'this'
    this.age = age;
  }

  // Override method with super delegation
  speak() {
    return `${super.speak()} with high energy!`;
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Inheritance & Super Delegation
```javascript
class Logger {
  constructor(prefix) {
    this.prefix = prefix;
  }

  log(message) {
    return `[${this.prefix}] ${message}`;
  }
}

class TimestampedLogger extends Logger {
  constructor(prefix) {
    super(prefix); // Forward prefix to Logger constructor
  }

  // Polymorphic method override
  log(message) {
    const timestamp = new Date().toISOString();
    // Delegate to parent Logger.prototype.log via super
    return `${timestamp} ${super.log(message)}`;
  }
}

const logger = new TimestampedLogger("AUTH");
console.log(logger.log("Token validated"));
// Output: 2026-03-31T... [AUTH] Token validated
```

### Example 2: Real-World Subclassing Built-In Error
```javascript
class DatabaseError extends Error {
  constructor(message, query, code = "DB_ERR") {
    // 1. Invoke Error constructor
    super(message);

    // 2. Fix Error name and add domain metadata
    this.name = this.constructor.name;
    this.query = query;
    this.code = code;

    // 3. Capture stack trace omitting constructor
    if (Error.captureStackTrace) {
      Error.captureStackTrace(this, this.constructor);
    }
  }
}

try {
  throw new DatabaseError("Connection timeout", "SELECT * FROM users", "ERR_CONN_TIMEOUT");
} catch (err) {
  console.log(`Caught: [${err.name}] Code: ${err.code}`);
  console.log("Query:", err.query);
  console.log(err instanceof DatabaseError); // true
  console.log(err instanceof Error);         // true
}
```

### Example 3: Edge Case with 'this' Before 'super()'
```javascript
class Base {
  constructor() {
    this.initialized = true;
  }
}

class Derived extends Base {
  constructor() {
    // Failure mode: accessing 'this' before super() throws ReferenceError!
    try {
      // @ts-ignore
      console.log(this);
    } catch (err) {
      console.log("Caught expected TDZ error on 'this':", err.message);
      // Must call super constructor in derived class before accessing 'this'
    }

    super(); // Allocates 'this'
    console.log("this is now valid:", this.initialized); // true
  }
}

new Derived();
```

### Example 4: Failure Mode with Abstract Base Classes
```javascript
// Simulating abstract classes using new.target
class AbstractRepository {
  constructor() {
    if (new.target === AbstractRepository) {
      throw new TypeError("Cannot instantiate abstract class AbstractRepository directly");
    }
  }

  // Abstract method contract
  findById(id) {
    throw new Error("Method 'findById()' must be implemented by derived class");
  }
}

class SqlUserRepository extends AbstractRepository {
  findById(id) {
    return { id, username: "admin" };
  }
}

// Concrete instantiation works:
const repo = new SqlUserRepository();
console.log(repo.findById(10)); // { id: 10, username: 'admin' }

// Direct instantiation fails:
try {
  new AbstractRepository();
} catch (err) {
  console.log("Abstract prevention works:", err.message);
}
```

### Example 5: Idiomatic Static Inheritance & Factory Methods
```javascript
class BaseModel {
  constructor(attributes) {
    this.attributes = { ...attributes };
  }

  // Static factory uses 'new this' to construct the DERIVED class!
  static fromJSON(jsonString) {
    const parsed = JSON.parse(jsonString);
    return new this(parsed);
  }
}

class UserProfile extends BaseModel {
  get displayName() {
    return `${this.attributes.firstName} ${this.attributes.lastName}`;
  }
}

// UserProfile inherits static method from BaseModel:
const profile = UserProfile.fromJSON('{"firstName":"Grace","lastName":"Hopper"}');
console.log(profile instanceof UserProfile); // true
console.log(profile.displayName);            // "Grace Hopper"
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Derived Constructor Without Constructor**: If a derived class omits `constructor`, JavaScript automatically inserts:
   ```javascript
   constructor(...args) {
     super(...args);
   }
   ```
2. **Returning an Object from Derived Constructor**: If a derived constructor returns an object explicitly, that object becomes the instance, even if `super()` was never called! However, returning a primitive without calling `super()` still throws a `ReferenceError`.
3. **`super` is Statically Bound via `[[HomeObject]]`**: If you copy a method that uses `super` to another object, `super` **still resolves to the original parent class**, because `[[HomeObject]]` is set at method definition time.
4. **Subclassing Arrays**: Subclassing `Array` produces true exotic array instances whose `length` property synchronizes automatically.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Accessing `this` before `super()` | `this.id = 1; super();` | `super(); this.id = 1;` | Throws `ReferenceError: Must call super constructor`. |
| Missing `super()` in derived class | Derived constructor without `super` | Always invoke `super()` | Derived class cannot initialize `this` without `super()`. |
| Copying method with `super` | Detaching method using `super` | Keep method on prototype | `super` relies on static `[[HomeObject]]` binding. |
| Deep inheritance hierarchies | Class tree $>4$ levels deep | Favor composition & mixins | Fragile base class problem; hard to reason about changes. |
| Forgetting `Error.name` in custom errors | `class MyError extends Error {}` | Set `this.name = this.constructor.name` | Custom error will format stack traces with `"Error:"`. |
| Mutating parent prototype directly | `Child.prototype.__proto__.m = ...` | Override on `Child.prototype` | Mutates shared base class across unrelated children. |

---

## 8. Performance Characteristics

- **Static Super Resolution**: TurboFan JIT statically resolves `super.method()` at compile time using the method's `[[HomeObject]]`, eliminating prototype chain traversal.
- **Instance Prototype Depth**: Walking 1–2 prototype levels has identical performance to own property access due to V8's Inline Caches.
- **Subclassing Built-ins**: In modern V8, subclassing `Array` or `Map` uses native C++ layout allocation, achieving parity with native built-in instances.

---

## 9. Engine Internals: V8 [[HomeObject]] & Derived Constructor Stubs

Inside Google V8:
1. **`[[ConstructorKind]]`**: Every function has an internal slot `[[ConstructorKind]]` set to either `"base"` or `"derived"`.
2. **Derived Allocation**: Base constructors allocate `this` on entry. Derived constructors enter with `this` uninitialized (`the_hole`).
3. Calling `super()` invokes the parent constructor stub and stores the resulting object in the derived constructor's `this` register.
4. **`[[HomeObject]]`**: Every method defined with object method shorthand syntax (`method() {}`) gets an internal slot `[[HomeObject]]` pointing to the prototype where it was defined. When `super.foo()` is executed, V8 evaluates `Object.getPrototypeOf([[HomeObject]]).foo.call(this)`.

---

## 10. Best Practices & Design Guidelines

1. **Favor Composition Over Inheritance**: Use inheritance strictly when modeling a true "is-a" relationship (e.g. `DatabaseError is-an Error`). For sharing capabilities across unrelated classes, use composition or mixins.
2. **Limit Inheritance Depth to 2–3 Levels**: Deep inheritance trees make state mutation tracking and unit testing difficult.
3. **Always Call `super()` with Exact Forwarded Arguments**: If overriding a constructor, forward the arguments expected by the base class.

---

## 11. Real-World Case Study: Enterprise HTTP Error Hierarchy

A standardized HTTP REST error hierarchy used in production web APIs to map domain exceptions to HTTP status codes, error payloads, and audit logs.

```javascript
class HttpError extends Error {
  constructor(message, status = 500, details = null) {
    super(message);
    this.name = this.constructor.name;
    this.status = status;
    this.details = details;
    this.timestamp = new Date().toISOString();

    if (Error.captureStackTrace) {
      Error.captureStackTrace(this, this.constructor);
    }
  }

  toResponse() {
    return {
      error: {
        type: this.name,
        message: this.message,
        status: this.status,
        details: this.details,
        timestamp: this.timestamp
      }
    };
  }
}

class BadRequestError extends HttpError {
  constructor(message = "Bad Request", validationErrors = null) {
    super(message, 400, validationErrors);
  }
}

class NotFoundError extends HttpError {
  constructor(resource, identifier) {
    super(`${resource} with identifier '${identifier}' not found`, 404);
    this.resource = resource;
    this.identifier = identifier;
  }
}

class UnauthorizedError extends HttpError {
  constructor(message = "Authentication required") {
    super(message, 401);
  }
}

// Verification in Router Middleware
function handleRequest(userId) {
  if (!userId) {
    throw new BadRequestError("User ID is required", [{ field: "userId", rule: "required" }]);
  }
  if (userId !== "usr_101") {
    throw new NotFoundError("User", userId);
  }
  return { id: "usr_101", name: "Ada Lovelace" };
}

try {
  handleRequest(null);
} catch (err) {
  if (err instanceof HttpError) {
    console.log(`HTTP ${err.status} Response:`, JSON.stringify(err.toResponse()));
  }
}
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. Why must <code>super()</code> be called before accessing <code>this</code> in a derived class constructor?</strong></summary>

In ES6 class inheritance, the responsibility for allocating the instance memory (`this`) belongs to the base class constructor. The derived class constructor does not instantiate `this`; it waits for the base class constructor to allocate the object and return it. Accessing `this` before calling `super()` attempts to read `this` while it is still an uninitialized TDZ binding, throwing a `ReferenceError`.
</details>

<details>
<summary><strong>2. What are the two prototype chains created when using <code>class Child extends Parent</code>?</strong></summary>

1. **Instance Prototype Chain**: `Child.prototype.[[Prototype]] === Parent.prototype`. This allows instances of `Child` to delegate to instance methods defined on `Parent.prototype`.
2. **Static Prototype Chain**: `Child.[[Prototype]] === Parent`. This allows `Child` to inherit static methods and properties defined on `Parent`.
</details>

<details>
<summary><strong>3. How does <code>super.method()</code> resolve which method to call?</strong></summary>

Every method created with shorthand syntax has an internal specification slot called `[[HomeObject]]` pointing to the object/prototype where the method was defined. When `super.method()` is evaluated, the engine looks up `Object.getPrototypeOf([[HomeObject]])` and retrieves `method` from that prototype, invoking it with `this` bound to the current instance.
</details>

<details>
<summary><strong>4. What happens if you define a derived class with no constructor?</strong></summary>

If a derived class does not explicitly define a constructor, the JavaScript engine synthesizes a default constructor:
```javascript
constructor(...args) {
  super(...args);
}
```
All incoming constructor arguments are automatically forwarded to the base class constructor.
</details>

<details>
<summary><strong>5. Can an ES6 class extend a traditional ES5 constructor function?</strong></summary>

Yes. ES6 classes can extend any callable constructor function whose `prototype` property is an object or `null`. However, a traditional ES5 constructor function *cannot* easily inherit from an ES6 class because class constructors require `new` and cannot be invoked via `Parent.call(this)`.
</details>

<details>
<summary><strong>6. How can you implement an Abstract Base Class in JavaScript?</strong></summary>

By checking `new.target` inside the base class constructor:
```javascript
class AbstractBase {
  constructor() {
    if (new.target === AbstractBase) {
      throw new TypeError("Cannot instantiate abstract class directly");
    }
  }
}
```
When `new Derived()` is called, `new.target` references `Derived`, so the check passes.
</details>

<details>
<summary><strong>7. What happens when subclassing the built-in <code>Array</code> class?</strong></summary>

Subclassing `Array` via `class MyArray extends Array` produces genuine exotic array instances. Methods like `push` and `pop` work natively, the `length` property automatically updates, and array-returning methods (like `slice` or `filter`) return instances of `MyArray` (via `Symbol.species`).
</details>

<details>
<summary><strong>8. What is <code>Symbol.species</code> used for in class inheritance?</strong></summary>

`Symbol.species` is a well-known symbol defining a getter property that constructor functions use to determine what constructor should be used to instantiate derived objects in methods like `map()`, `filter()`, or `slice()`. By overriding `static get [Symbol.species]() { return Array; }`, a derived array subclass can ensure that `map()` returns a plain `Array` rather than an instance of the subclass.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Basic Class Extension (Easy)
Create a class `Animal` with `name` and a `speak()` method. Create a derived class `Dog` that overrides `speak()` to return `"${name} barks"`.

<details>
<summary>View Solution</summary>

```javascript
class Animal {
  constructor(name) {
    this.name = name;
  }
  speak() {
    return `${this.name} makes a sound`;
  }
}

class Dog extends Animal {
  speak() {
    return `${this.name} barks`;
  }
}

const d = new Dog("Buddy");
console.log(d.speak()); // "Buddy barks"
```
</details>

### Exercise 2: Static Method Inheritance Verification (Easy)
Demonstrate that static methods are inherited by subclasses using `extends`.

<details>
<summary>View Solution</summary>

```javascript
class Factory {
  static create() {
    return "Created by Factory";
  }
}

class CustomFactory extends Factory {}

console.log(CustomFactory.create()); // "Created by Factory"
console.log(Object.getPrototypeOf(CustomFactory) === Factory); // true
```
</details>

### Exercise 3: Custom Subclass of Array (Medium)
Create a `Stack` class that extends `Array` and adds `peek()` and `isEmpty()` methods without mutating underlying array behaviors.

<details>
<summary>View Solution</summary>

```javascript
class Stack extends Array {
  peek() {
    return this.at(-1);
  }

  isEmpty() {
    return this.length === 0;
  }
}

const s = new Stack(10, 20, 30);
console.log("Peek:", s.peek());       // 30
console.log("Is empty?", s.isEmpty()); // false
s.pop();
console.log("Peek after pop:", s.peek()); // 20
```
</details>

### Exercise 4: Enforcing Abstract Interface Methods (Medium)
Write an abstract base class `StorageProvider` that throws an error if any derived class fails to implement `read(key)` and `write(key, val)`.

<details>
<summary>View Solution</summary>

```javascript
class StorageProvider {
  constructor() {
    if (new.target === StorageProvider) {
      throw new TypeError("Cannot instantiate abstract class StorageProvider");
    }
    // Verify interface contracts
    if (this.read === StorageProvider.prototype.read) {
      throw new Error(`Subclass ${new.target.name} must implement read()`);
    }
    if (this.write === StorageProvider.prototype.write) {
      throw new Error(`Subclass ${new.target.name} must implement write()`);
    }
  }

  read(key) { throw new Error("Not implemented"); }
  write(key, val) { throw new Error("Not implemented"); }
}

class MemoryStorage extends StorageProvider {
  #store = new Map();
  read(k) { return this.#store.get(k); }
  write(k, v) { this.#store.set(k, v); }
}

const mem = new MemoryStorage();
mem.write("token", "abc");
console.log(mem.read("token")); // "abc"
```
</details>

### Exercise 5: Symbol.species Subclass Customization (Hard)
Create an `AuditedArray` subclass that logs whenever elements are pushed, but configures `Symbol.species` so that `map()` and `filter()` return standard plain `Array` instances.

<details>
<summary>View Solution</summary>

```javascript
class AuditedArray extends Array {
  push(...items) {
    console.log(`[Audit] Pushed ${items.length} items`);
    return super.push(...items);
  }

  // Force derived operations to return standard Array instances
  static get [Symbol.species]() {
    return Array;
  }
}

const list = new AuditedArray(1, 2, 3);
list.push(4); // Logs audit message

const mapped = list.map(x => x * 10);
console.log("Mapped items:", mapped);
console.log("Is mapped an AuditedArray?", mapped instanceof AuditedArray); // false
console.log("Is mapped a standard Array?", mapped instanceof Array);        // true
```
</details>

### Exercise 6: Multi-Level Super Delegation Chain (Hard)
Construct a 3-tier inheritance hierarchy (`GrandParent` -> `Parent` -> `Child`) where a method in `Child` invokes `super.describe()` to aggregate descriptions from all levels without infinite loops.

<details>
<summary>View Solution</summary>

```javascript
class GrandParent {
  describe() {
    return "GrandParent";
  }
}

class Parent extends GrandParent {
  describe() {
    return `${super.describe()} -> Parent`;
  }
}

class Child extends Parent {
  describe() {
    return `${super.describe()} -> Child`;
  }
}

const c = new Child();
console.log("Aggregated description:", c.describe());
// Output: "GrandParent -> Parent -> Child"
```
</details>
