# JavaScript Constructor Functions & The 'new' Operator: Mechanics & Prototypal Wiring

> Comprehensive exploration of JavaScript constructor functions, the 5-step `new` operator algorithm, return overrides, `new.target` introspection, and pre-ES6 prototypal inheritance.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [05_this-and-oop/01_this.md](./01_this.md), [05_this-and-oop/03_prototypes-and-prototype-chain.md](./03_prototypes-and-prototype-chain.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES1 (1997) / ES2015 (`new.target`) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Constructors & Prototypes](#2-core-concepts-constructors--prototypes)
3. [How It Works: The 5-Step 'new' Operator Execution Algorithm](#3-how-it-works-the-5-step-new-operator-execution-algorithm)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 ConstructStub & Hidden Class Initialization](#9-engine-internals-v8-constructstub--hidden-class-initialization)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: State Machine Workflow Engine via Constructors](#11-real-world-case-study-state-machine-workflow-engine-via-constructors)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6) introduced the formal `class` keyword, object-oriented programming in JavaScript relied on **Constructor Functions**. A constructor function is a standard JavaScript function invoked with the **`new`** operator.

The `new` operator automates object instantiation: allocating memory, linking prototypes, setting execution contexts, and returning instances. While modern code frequently favors the `class` syntax, understanding constructor functions is crucial because ES6 classes are fundamentally "syntactic sugar" built on top of this exact constructor and prototype wiring model.

---

## 2. Core Concepts: Constructors & Prototypes

```
Constructor Function Prototype Relationship
┌──────────────────────────────┐
│     function Person(name)    │
│  prototype property ─────────┼───────────────────────────┐
└──────────────────────────────┘                           │
                                                           ▼
┌──────────────────────────────┐          ┌──────────────────────────────┐
│  const alice = new Person()  │          │       Person.prototype       │
│  [[Prototype]] Pointer ──────┼─────────►│  constructor: Person         │
│  Own Properties: { name }    │          │  sayHello: function()        │
└──────────────────────────────┘          └──────────────────────────────┘
```

1. **Constructor Function**: By convention, named in PascalCase (e.g. `User`, `EventEmitter`). Holds instance property assignments (`this.name = name`).
2. **`Constructor.prototype`**: Holds shared methods (`User.prototype.login = ...`). Methods defined here are shared by all instances, avoiding redundant memory duplication.
3. **`instance.constructor`**: A reference pointing back to the constructor function that instantiated the object.

---

## 3. How It Works: The 5-Step 'new' Operator Execution Algorithm

When JavaScript evaluates `new Constructor(arg1, arg2)`:

```
Step 1: Allocate a fresh, empty plain object in memory (`{}`).
Step 2: Link `instance.[[Prototype]] = Constructor.prototype`.
Step 3: Bind `this` to the newly allocated object.
Step 4: Execute `Constructor(arg1, arg2)` with `this` as context.
Step 5: Check Return Value:
        - If Constructor returns an Object, return THAT object.
        - Otherwise, return `this` (the object created in Step 1).
```

---

## 4. Syntax & API Reference

### 1. Basic Constructor Definition
```javascript
function Vehicle(make, model) {
  this.make = make;
  this.model = model;
}

// Attach shared methods to prototype
Vehicle.prototype.getDetails = function () {
  return `${this.make} ${this.model}`;
};
```

### 2. Modern Introspection: `new.target`
Introduced in ES2015, `new.target` detects whether a function was invoked with the `new` operator:
- When called with `new`: `new.target` references the constructor function.
- When called without `new`: `new.target` evaluates to `undefined`.

---

## 5. Practical Examples

### Example 1: Basic Constructor with Prototype Methods
```javascript
function BankAccount(accountHolder, initialBalance) {
  // Validate that constructor was called with 'new'
  if (!new.target) {
    throw new TypeError("BankAccount must be instantiated with 'new'");
  }

  this.accountHolder = accountHolder;
  this.balance = initialBalance;
}

// Shared method on prototype (allocated once)
BankAccount.prototype.deposit = function (amount) {
  if (amount <= 0) throw new RangeError("Deposit must be positive");
  this.balance += amount;
  return this.balance;
};

const account = new BankAccount("Ada", 1000);
console.log(account.deposit(250)); // 1250
console.log(account instanceof BankAccount); // true
```

### Example 2: Real-World Pre-ES6 Prototypal Inheritance
```javascript
// Base Constructor
function Employee(name, id) {
  this.name = name;
  this.id = id;
}
Employee.prototype.getBadge = function () {
  return `[${this.id}] ${this.name}`;
};

// Derived Constructor
function Manager(name, id, department) {
  // Step 1: Call super constructor (borrowing this)
  Employee.call(this, name, id);
  this.department = department;
}

// Step 2: Wire prototype chain (Manager delegates to Employee)
Manager.prototype = Object.create(Employee.prototype);

// Step 3: Repair constructor pointer
Manager.prototype.constructor = Manager;

// Step 4: Add derived methods
Manager.prototype.getDeptInfo = function () {
  return `${this.getBadge()} oversees ${this.department}`;
};

const mgr = new Manager("Grace Hopper", "M-101", "Systems Architecture");
console.log(mgr.getDeptInfo()); // "[M-101] Grace Hopper oversees Systems Architecture"
console.log(mgr instanceof Manager);  // true
console.log(mgr instanceof Employee); // true
```

### Example 3: Edge Case with Constructor Return Overrides
```javascript
// Rule: Returning a primitive is ignored; returning an object overrides 'this'

function PrimitiveReturn() {
  this.status = "valid";
  return 42; // Primitive: IGNORED!
}

function ObjectReturn() {
  this.status = "ignored";
  return { custom: "override" }; // Object: OVERRIDES 'this'!
}

console.log(new PrimitiveReturn()); // { status: 'valid' }
console.log(new ObjectReturn());    // { custom: 'override' }
```

### Example 4: Failure Mode with Missing 'new'
```javascript
function Point(x, y) {
  // Buggy code without new check:
  this.x = x;
  this.y = y;
}

// Calling without 'new' in sloppy mode pollutes globalThis!
// Point(10, 20); // Pollutes globalThis.x and globalThis.y

// Self-invoking constructor pattern (pre-ES6 defensive check):
function SafePoint(x, y) {
  if (!(this instanceof SafePoint)) {
    return new SafePoint(x, y); // Auto-instantiate
  }
  this.x = x;
  this.y = y;
}

const p = SafePoint(10, 20); // Works even without 'new'!
console.log(p.x, p.y); // 10, 20
```

### Example 5: Idiomatic Simulation of the 'new' Operator
```javascript
function simulateNew(Constructor, ...args) {
  if (typeof Constructor !== "function") {
    throw new TypeError("Constructor must be a function");
  }

  // Step 1 & 2: Allocate object and link prototype
  const instance = Object.create(Constructor.prototype);

  // Step 3 & 4: Execute constructor with instance as 'this'
  const result = Constructor.apply(instance, args);

  // Step 5: Check return value
  const isObject = result !== null && (typeof result === "object" || typeof result === "function");
  return isObject ? result : instance;
}

function User(name) {
  this.name = name;
}
User.prototype.getName = function () { return this.name; };

const simulatedUser = simulateNew(User, "Alan");
console.log(simulatedUser.getName()); // "Alan"
console.log(simulatedUser instanceof User); // true
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Arrow Functions Cannot Be Constructors**: Calling `new (() => {})` throws a `TypeError: is not a constructor`. Arrow functions lack a `[[Construct]]` method and do not have a `prototype` property.
2. **Forgetting to Repair `prototype.constructor`**: When writing `Child.prototype = Object.create(Parent.prototype)`, `Child.prototype.constructor` is overwritten with `Parent`. Always manually restore `Child.prototype.constructor = Child`.
3. **Defining Methods Inside the Constructor Body**: Writing `this.sayHi = function() {}` inside the constructor allocates a distinct function on every instantiation. Define methods on `Constructor.prototype` instead.
4. **`instanceof` Can Be Spoofed**: `account instanceof BankAccount` simply checks if `BankAccount.prototype` exists in `account`'s prototype chain. It does not guarantee internal state encapsulation.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Defining methods inside constructor | `this.run = function() {}` | `Constructor.prototype.run = ...` | Allocates duplicate function instances on the heap. |
| Forgetting `new` keyword | `const u = User("name")` | Check `new.target` | Invokes default binding; `this` is undefined or global. |
| Using `Child.prototype = Parent.prototype` | Direct prototype assignment | `Object.create(Parent.prototype)` | Mutating Child's prototype pollutes Parent's prototype. |
| Using `new Parent()` for inheritance | `Child.prototype = new Parent()` | `Object.create(Parent.prototype)` | Executes parent constructor early with dummy parameters. |
| Overwriting prototype after instantiation | `C.prototype = { newMethod() {} }` | Mutate existing prototype object | Existing instances remain linked to the old prototype. |
| Returning primitive from constructor | `return false;` to indicate failure | Throw an `Error` | Primitive return is ignored; returns newly created `this`. |

---

## 8. Performance Characteristics

- **Constructor Instantiation**: Fast $O(1)$ allocation in V8.
- **Hidden Class (Shape) Stability**: If properties are initialized in the exact same order in the constructor, all instances share the same V8 `Map`, enabling high-performance inline caches.
- **Prototype Method Execution**: Monomorphic method dispatch via prototype inline caches is as fast as direct own-method invocation.

---

## 9. Engine Internals: V8 ConstructStub & Hidden Class Initialization

Inside Google V8:
1. **`[[Construct]]` Internal Method**: Functions that can be called with `new` possess the `[[Construct]]` internal slot. Built-ins like `Math` or arrow functions do not.
2. **ConstructStub**: When `new Constructor()` is invoked, V8 calls a specialized assembly routine called `ConstructStub`.
   - `ConstructStub` allocates the `JSObject` memory block with an initial pre-allocated `Map` pointing to `Constructor.prototype`.
   - It sets the receiver register to this object.
   - It executes the constructor body.
   - If the constructor returns a pointer to a heap object, that pointer is returned; otherwise, the newly allocated receiver is returned.

---

## 10. Best Practices & Design Guidelines

1. **Guard Constructors with `new.target`**: Always throw an error if `!new.target` to prevent accidental function calls without `new`.
2. **Adopt ES6 `class` Syntax for Modern Applications**: Use `class` syntax for new code; it handles constructor checks, prototype wiring, and `super` calls automatically.
3. **Keep Property Initialization Predictable**: Initialize all instance fields in the constructor body in a fixed, consistent order to preserve V8 hidden classes.

---

## 11. Real-World Case Study: State Machine Workflow Engine via Constructors

A workflow state machine designed with constructor functions and shared prototype transitions for processing order fulfillment events.

```javascript
function OrderWorkflow(orderId) {
  if (!new.target) {
    throw new TypeError("OrderWorkflow must be constructed with 'new'");
  }

  this.orderId = orderId;
  this.state = "PENDING";
  this.history = [];
  this.#recordTransition("INITIALIZED");
}

// Private helper via symbol
const RECORD_FN = Symbol("recordTransition");
OrderWorkflow.prototype[RECORD_FN] = function (action) {
  this.history.push({
    state: this.state,
    action,
    timestamp: new Date().toISOString()
  });
};

// Transition methods on prototype
OrderWorkflow.prototype.approve = function () {
  if (this.state !== "PENDING") {
    throw new Error(`Cannot approve order in state: ${this.state}`);
  }
  this.state = "APPROVED";
  this[RECORD_FN]("APPROVED");
  return this;
};

OrderWorkflow.prototype.ship = function () {
  if (this.state !== "APPROVED") {
    throw new Error(`Cannot ship order in state: ${this.state}`);
  }
  this.state = "SHIPPED";
  this[RECORD_FN]("SHIPPED");
  return this;
};

// Verification
const order = new OrderWorkflow("ORD-8831");
order.approve().ship();

console.log("Current state:", order.state); // "SHIPPED"
console.log("Transition history:", order.history.length); // 3
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What exact 5 steps occur when a constructor function is invoked with <code>new</code>?</strong></summary>

1. A brand new plain object is created in memory (`{}`).
2. The newly created object's internal `[[Prototype]]` is set to the constructor function's `prototype` object.
3. The constructor function's `this` context is bound to this newly created object.
4. The constructor function executes with the provided arguments.
5. If the constructor returns an object, that object is returned. Otherwise, the newly created object from Step 1 is returned.
</details>

<details>
<summary><strong>2. What happens if a constructor function explicitly returns a primitive value vs an object?</strong></summary>

If a constructor returns a primitive value (e.g. `return 42;`, `return "error";`, `return false;`), the return value is completely ignored, and the newly constructed `this` instance is returned. If it returns an object or a function (e.g. `return { a: 1 };`), the newly constructed `this` instance is discarded, and that returned object is returned instead.
</details>

<details>
<summary><strong>3. What is <code>new.target</code>, and how does it behave in ES6?</strong></summary>

`new.target` is a meta-property available in all functions. If the function was invoked via `new`, `new.target` contains a reference to the constructor function. If the function was invoked as a normal function call, `new.target` is `undefined`. In class inheritance hierarchies, `new.target` in a superclass constructor points to the derived class that was instantiated.
</details>

<details>
<summary><strong>4. Why is <code>Child.prototype = new Parent()</code> an anti-pattern for inheritance?</strong></summary>

Calling `new Parent()` executes the `Parent` constructor at definition time. If `Parent` requires mandatory arguments, makes network requests, or sets instance-specific state, those actions execute prematurely with missing parameters, polluting the prototype with unwanted instance properties. The correct pre-ES6 pattern is `Child.prototype = Object.create(Parent.prototype)`.
</details>

<details>
<summary><strong>5. Why is <code>Child.prototype.constructor = Child</code> necessary after wiring prototypes?</strong></summary>

When setting `Child.prototype = Object.create(Parent.prototype)`, `Child.prototype` inherits `constructor` from `Parent.prototype`. As a result, `(new Child()).constructor` incorrectly evaluates to `Parent`. Manually restoring `Child.prototype.constructor = Child` fixes introspection and constructor reflection.
</details>

<details>
<summary><strong>6. Can you invoke <code>new</code> on an arrow function? Why or why not?</strong></summary>

No. Invoking `new` on an arrow function throws `TypeError: ... is not a constructor`. Arrow functions lack the internal `[[Construct]]` method, have lexical `this` (which cannot be rebound to a new object), and do not have a `prototype` property.
</details>

<details>
<summary><strong>7. How does V8 optimize constructor functions?</strong></summary>

V8 creates a dedicated `Map` (Hidden Class) for the constructor. If all instance properties are assigned in the exact same order in the constructor, V8 can reuse the transition tree and pre-allocate fixed in-object property offsets, avoiding runtime property table lookups.
</details>

<details>
<summary><strong>8. What is the difference between defining a method in a constructor vs on its prototype?</strong></summary>

Methods defined inside the constructor (`this.method = function() {}`) are re-created as new function instances on the heap for every single object created. Methods defined on the prototype (`Constructor.prototype.method = function() {}`) exist once in memory and are shared across all instances via prototype delegation.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Constructor Validation (Easy)
Write a constructor `StrictUser(username)` that throws an error if called without `new`.

<details>
<summary>View Solution</summary>

```javascript
function StrictUser(username) {
  if (!new.target) {
    throw new TypeError("Must use 'new' operator to instantiate StrictUser");
  }
  this.username = username;
}

const u = new StrictUser("grace");
console.log(u.username); // "grace"
// StrictUser("grace"); // Throws TypeError
```
</details>

### Exercise 2: Auto-New Constructor (Easy)
Write a constructor `FlexiblePoint(x, y)` that works identically whether called with `new` or without `new`.

<details>
<summary>View Solution</summary>

```javascript
function FlexiblePoint(x, y) {
  if (!new.target) {
    return new FlexiblePoint(x, y);
  }
  this.x = x;
  this.y = y;
}

const p1 = new FlexiblePoint(1, 2);
const p2 = FlexiblePoint(1, 2);
console.log(p1 instanceof FlexiblePoint); // true
console.log(p2 instanceof FlexiblePoint); // true
```
</details>

### Exercise 3: Complete Pre-ES6 Inheritance Helper (Medium)
Implement a utility function `inherit(ChildConstructor, ParentConstructor)` that sets up prototype delegation and repairs the constructor pointer.

<details>
<summary>View Solution</summary>

```javascript
function inherit(Child, Parent) {
  Child.prototype = Object.create(Parent.prototype);
  Object.defineProperty(Child.prototype, "constructor", {
    value: Child,
    enumerable: false,
    writable: true,
    configurable: true
  });
}

function Shape() {}
Shape.prototype.draw = function () { return "Drawing Shape"; };

function Circle() {}
inherit(Circle, Shape);

const c = new Circle();
console.log(c.draw()); // "Drawing Shape"
console.log(c.constructor === Circle); // true
```
</details>

### Exercise 4: Simulating 'new' with Primitive/Object Returns (Medium)
Implement `customNew(constructorFn, ...args)` that replicates the exact return override behavior of the native `new` operator.

<details>
<summary>View Solution</summary>

```javascript
function customNew(Constructor, ...args) {
  const obj = Object.create(Constructor.prototype);
  const result = Constructor.apply(obj, args);
  return (result !== null && (typeof result === "object" || typeof result === "function"))
    ? result
    : obj;
}

function RetNum() { this.x = 1; return 100; }
function RetObj() { this.x = 1; return { custom: true }; }

console.log(customNew(RetNum)); // { x: 1 }
console.log(customNew(RetObj)); // { custom: true }
```
</details>

### Exercise 5: Constructor Call Hierarchy Tracer (Hard)
Write an abstract base constructor `AuditedEntity()` that uses `new.target` to log the exact derived constructor name when instantiated.

<details>
<summary>View Solution</summary>

```javascript
function AuditedEntity() {
  if (!new.target) {
    throw new Error("Must use 'new'");
  }
  console.log(`[Audit] Instantiating concrete entity: ${new.target.name}`);
  this.createdAt = Date.now();
}

function Customer(name) {
  AuditedEntity.call(this);
  this.name = name;
}
Customer.prototype = Object.create(AuditedEntity.prototype);
Customer.prototype.constructor = Customer;

const cust = new Customer("Alice");
console.log(cust.name);
```
</details>

### Exercise 6: Polymorphic Shape Benchmark Simulator (Hard)
Write a benchmark comparing the execution speed of prototype methods vs. in-constructor methods across 100,000 instances to measure heap and invocation differences.

<details>
<summary>View Solution</summary>

```javascript
function benchmarkInstantiation() {
  function InConstructor(val) {
    this.val = val;
    this.getVal = function () { return this.val; };
  }

  function OnPrototype(val) {
    this.val = val;
  }
  OnPrototype.prototype.getVal = function () { return this.val; };

  const N = 100_000;

  console.time("InConstructor Instantiation");
  const list1 = new Array(N);
  for (let i = 0; i < N; i++) list1[i] = new InConstructor(i);
  console.timeEnd("InConstructor Instantiation");

  console.time("OnPrototype Instantiation");
  const list2 = new Array(N);
  for (let i = 0; i < N; i++) list2[i] = new OnPrototype(i);
  console.timeEnd("OnPrototype Instantiation");
}

benchmarkInstantiation();
```
</details>
