# JavaScript Functional Patterns: Functors, Monads & Railway-Oriented Programming

> In-depth guide to category theory patterns in JavaScript, covering Functor laws, Monadic composition (`flatMap`/`chain`), the Maybe/Either abstractions, and Railway-Oriented error handling.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [07_functional-programming/01_pure-functions-and-side-effects.md](./01_pure-functions-and-side-effects.md), [07_functional-programming/04_composition-and-pipe.md](./04_composition-and-pipe.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 / ES2015 Functional Architecture |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Functors & Monads](#2-core-concepts-functors--monads)
3. [How It Works: Category Theory Laws & Composition](#3-how-it-works-category-theory-laws--composition)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Object Allocations in Monadic Containers](#9-engine-internals-v8-object-allocations-in-monadic-containers)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Railway-Oriented E-Commerce Order Validation](#11-real-world-case-study-railway-oriented-e-commerce-order-validation)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In traditional imperative code, dealing with nullish values, network failures, and input errors leads to defensive code clutter: endless `if (x == null)` guards and sprawling `try/catch` blocks. Imperative exception throwing breaks referential transparency: an unhandled exception violently aborts the call stack, forcing callers to guess which functions might throw.

**Functional Design Patterns** borrowed from Category Theory—specifically **Functors** and **Monads**—solve these challenges by wrapping values inside context-aware container objects. Instead of accessing raw data directly, developers map pure functions over these containers.

These abstractions enable:
- **The Maybe Monad**: Eliminates `TypeError: Cannot read properties of undefined` without defensive `if` statements.
- **The Either Monad & Railway-Oriented Programming**: Models computations that can succeed (`Right`) or fail (`Left`) as first-class data values rather than throwing exceptions.
- **The IO Monad**: Defers side effects so that business workflows remain 100% pure until execution.

---

## 2. Core Concepts: Functors & Monads

```
                        The Container Hierarchy
                                   │
      ┌────────────────────────────┴────────────────────────────┐
      ▼                                                         ▼
   Functor                                                   Monad
   • A container implementing `map(fn)`                      • A Functor implementing `flatMap(fn)` (or `chain`)
   • Transforms wrapped value:                               • Flattens nested containers:
     Container(A) ──► Container(B)                             Container(Container(A)) ──► Container(A)
   • Examples: Array, Promise                                • Handles sequential dependent effects
```

### 1. Functor
A Functor is simply any data structure that implements a `map` method adhering to mathematical laws. JavaScript Arrays are native Functors: `[1, 2].map(x => x * 2)` transforms values while preserving the Array structure.

### 2. Monad
A Monad is an augmented Functor that provides a mechanism to unwrap and flatten nested containers (`flatMap` / `chain`). If a function takes a raw value and returns a Monad ($A \to M(B)$), standard `map` would produce $M(M(B))$. A Monad's `chain` operation flattens this back into $M(B)$.

---

## 3. How It Works: Category Theory Laws & Composition

To qualify as mathematical Functors and Monads, containers must satisfy formal algebraic laws:

```
Functor Laws:
1. Identity:      container.map(x => x) === container
2. Composition:   container.map(x => f(g(x))) === container.map(g).map(f)

Monad Laws:
1. Left Identity:   Monad.of(x).chain(f) === f(x)
2. Right Identity:  m.chain(Monad.of) === m
3. Associativity:   m.chain(f).chain(g) === m.chain(x => f(x).chain(g))
```

### The Railway-Oriented Model (The Either Monad)
Computations flow along a two-track railway:
- **Happy Path (Right)**: Success track. Functions transform data sequentially.
- **Error Path (Left)**: Failure track. If any step fails, the computation switches to the failure track, bypassing all remaining steps and delivering the error cleanly to the terminus.

```
Railway-Oriented Execution:
Input ──► [ Step 1: Validate ] ──(Success)──► [ Step 2: Charge ] ──(Success)──► Success Output
                   │ (Failure)                         │ (Failure)
                   ▼                                   ▼
             [ Error Track ] ────────────────────► [ Error Track ] ──────────► Failure Output
```

---

## 4. Syntax & API Reference

### 1. The Maybe / Option Monad
```javascript
class Maybe {
  #val;
  constructor(val) { this.#val = val; }

  static of(val) { return new Maybe(val); }
  static just(val) { return new Maybe(val); }
  static nothing() { return new Maybe(null); }

  isNothing() { return this.#val === null || this.#val === undefined; }

  map(fn) {
    return this.isNothing() ? Maybe.nothing() : Maybe.just(fn(this.#val));
  }

  chain(fn) {
    return this.isNothing() ? Maybe.nothing() : fn(this.#val);
  }

  getOrElse(fallback) {
    return this.isNothing() ? fallback : this.#val;
  }
}
```

### 2. The Either Monad (Left / Right)
```javascript
class Either {
  constructor(val) { this.val = val; }
  static right(val) { return new Right(val); }
  static left(val) { return new Left(val); }
}

class Right extends Either {
  map(fn) { return Either.right(fn(this.val)); }
  chain(fn) { return fn(this.val); }
  fold(leftFn, rightFn) { return rightFn(this.val); }
}

class Left extends Either {
  map(fn) { return this; }   // Bypass transformation
  chain(fn) { return this; } // Bypass chain
  fold(leftFn, rightFn) { return leftFn(this.val); }
}
```

---

## 5. Practical Examples

### Example 1: Safe Null Traversal with the Maybe Monad
```javascript
const userPayload = {
  id: 101,
  profile: {
    address: {
      street: "742 Evergreen Terrace",
      city: "Springfield"
    }
  }
};

// Safe property extraction pipeline
const getCityUppercase = user =>
  Maybe.of(user)
    .map(u => u.profile)
    .map(p => p.address)
    .map(a => a.city)
    .map(c => c.toUpperCase())
    .getOrElse("UNKNOWN CITY");

console.log(getCityUppercase(userPayload)); // "SPRINGFIELD"
console.log(getCityUppercase({}));          // "UNKNOWN CITY" (No crash!)
console.log(getCityUppercase(null));        // "UNKNOWN CITY" (No crash!)
```

### Example 2: Railway-Oriented Error Handling with the Either Monad
```javascript
function parseJson(rawString) {
  try {
    return Either.right(JSON.parse(rawString));
  } catch (err) {
    return Either.left(`Invalid JSON: ${err.message}`);
  }
}

function validatePayload(data) {
  if (!data.username) return Either.left("Validation Error: 'username' is required");
  if (!data.email) return Either.left("Validation Error: 'email' is required");
  return Either.right(data);
}

function normalizeUser(user) {
  return Either.right({
    ...user,
    email: user.email.trim().toLowerCase()
  });
}

// Composition pipeline
const processRawInput = raw =>
  parseJson(raw)
    .chain(validatePayload)
    .chain(normalizeUser);

// Test Success:
const good = processRawInput('{"username":"grace","email":"  GRACE@NAVY.MIL  "}');
good.fold(
  err => console.error("Error:", err),
  user => console.log("Success User:", user)
);
// Success User: { username: 'grace', email: 'grace@navy.mil' }

// Test Failure:
const bad = processRawInput('{"username":"grace"}');
bad.fold(
  err => console.error("Error:", err),
  user => console.log("Success User:", user)
);
// Error: Validation Error: 'email' is required
```

### Example 3: Edge Case with JavaScript Promises as Pragmatic Monads
```javascript
// JavaScript Promises act like Monads, but violate strict monadic laws!
// In a strict monad:
// Promise.resolve(Promise.resolve(42)) should be Promise<Promise<42>>
// BUT native Promises AUTO-FLATTEN nested promises inside .then():

const nested = Promise.resolve(Promise.resolve(42));
nested.then(val => {
  // val is 42, NOT Promise<42>!
  // Promise.prototype.then overloads both 'map' and 'flatMap' into one method!
  console.log("Promise auto-flattened value:", val); // 42
});
```

### Example 4: Failure Mode with Using `map` Instead of `chain`
```javascript
// Failure mode: Using map with a function that returns a Monad creates nested monads!
const safeSqrt = n => (n >= 0 ? Maybe.just(Math.sqrt(n)) : Maybe.nothing());

// BUG: Using .map() wraps the returned Maybe inside another Maybe!
const nestedMaybe = Maybe.just(16).map(safeSqrt);
console.log("Nested structure:", nestedMaybe); 
// Maybe { #val: Maybe { #val: 4 } } (Requires nested unwrapping!)

// FIX: Use .chain() (flatMap) to keep the structure flat!
const flatMaybe = Maybe.just(16).chain(safeSqrt);
console.log("Flat structure:", flatMaybe.getOrElse(0)); // 4
```

### Example 5: Idiomatic IO Monad Deferring Side Effects
```javascript
class IO {
  #effect;
  constructor(effect) { this.#effect = effect; }

  static of(val) { return new IO(() => val); }

  map(fn) {
    return new IO(() => fn(this.#effect()));
  }

  chain(fn) {
    return new IO(() => fn(this.#effect()).run());
  }

  // Pure until 'run()' is explicitly executed at the application boundary!
  run() {
    return this.#effect();
  }
}

// Side effect is defined purely as a recipe, NOT executed yet:
const readStorage = key => new IO(() => localStorageMock.get(key));
const writeLog = msg => new IO(() => console.log(`[Audit] ${msg}`));

const localStorageMock = new Map([["theme", "dark"]]);

const program = readStorage("theme")
  .map(theme => theme.toUpperCase())
  .chain(theme => writeLog(`Loaded theme: ${theme}`));

// Zero side effects have occurred up to this point!
console.log("Program defined. Executing now...");
program.run(); // Side effect happens here: "[Audit] Loaded theme: DARK"
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Promises Are Not Strict Monads**: Promises combine `map` and `flatMap` into `.then()`. If a function passed to `.then()` returns a primitive, it behaves like `map`; if it returns a Promise, it behaves like `flatMap`. Furthermore, Promises cannot wrap another Promise without auto-flattening.
2. **Memory Overhead of Monadic Chains**: In JavaScript, every `.map()` or `.chain()` allocates a new wrapper object instance. Deep chains on millions of records can generate substantial garbage collection pressure.
3. **Debugging Difficulty**: Inspecting the value inside a Monad requires calling `.map(tap(console.log))` or `.fold()`; setting standard breakpoints inside native container methods can be tedious.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using `map` when returning Monad | `maybe.map(x => Maybe.of(x))` | `maybe.chain(x => Maybe.of(x))` | Creates deeply nested `Maybe(Maybe(x))` wrappers. |
| Forgetting to unwrap Monad | Returning raw `Maybe` to caller | Unpack via `.getOrElse()` or `.fold()` | Exposes internal functional container to external callers. |
| Throwing inside Either pipeline | `if (!x) throw Error()` | `if (!x) return Either.left(err)` | Violates Railway-Oriented pattern and breaks pipeline flow. |
| Executing IO side effects early | Running DB queries inside `of()` | Wrap in lambda `() => db.query()` | Side effect runs eagerly instead of being deferred. |
| Over-abstracting simple logic | Wrapping single integer in Monad | Plain JavaScript conditionals | Introduces needless complexity when simple code suffices. |
| Mutating wrapped value in map | `maybe.map(o => { o.x = 1; return o; })` | `maybe.map(o => ({ ...o, x: 1 }))` | Violates immutability within the container. |

---

## 8. Performance Characteristics

- **Allocation Cost**: Each monadic step (`map`, `chain`) allocates a small JavaScript object on the heap.
- **Micro-Optimization**: For high-frequency loops processing millions of records per second, use imperative control flow (`if/else`) or transducers. For domain business logic, architecture boundaries, and user input validation, Monads provide unmatched reliability and safety.

---

## 9. Engine Internals: V8 Object Allocations in Monadic Containers

Inside Google V8:
1. `class Maybe` instances are allocated as small `JSObject` instances with in-object properties.
2. Because `Right` and `Left` inherit from `Either`, they share a common prototype root, but V8 assigns them **different Hidden Classes (Shapes)**.
3. In a polymorphic call site (`result.map(...)`), TurboFan creates a **Polymorphic Inline Cache (PIC)** with two entries (one for `Right` and one for `Left`), generating efficient conditional branches rather than deoptimizing.

---

## 10. Best Practices & Design Guidelines

1. **Use Either for Validation & Business Logic**: Replace sprawling `try/catch` and throwing errors in business services with the `Either` monad.
2. **Always Provide a Fallback (`getOrElse` / `fold`)**: Ensure that every monadic chain terminates with an explicit unwrap at the system boundary.
3. **Keep Monadic Containers Thin**: Do not bundle domain business logic inside the Monad classes themselves; keep them as generic mathematical data structures.

---

## 11. Real-World Case Study: Railway-Oriented E-Commerce Order Validation

A complete checkout order validation engine processing credit cards, inventory checks, and address verification via the `Either` monad.

```javascript
class OrderCheckoutPipeline {
  // Pure validation step 1: Validate Cart
  static validateCart(order) {
    if (!order.items || order.items.length === 0) {
      return Either.left({ code: "EMPTY_CART", message: "Cart contains no items" });
    }
    return Either.right(order);
  }

  // Pure validation step 2: Check Inventory
  static checkInventory(order) {
    const outOfStock = order.items.filter(item => item.inventoryCount <= 0);
    if (outOfStock.length > 0) {
      return Either.left({
        code: "OUT_OF_STOCK",
        message: `Items out of stock: ${outOfStock.map(i => i.name).join(", ")}`
      });
    }
    return Either.right(order);
  }

  // Pure validation step 3: Validate Payment Method
  static validatePayment(order) {
    if (!order.payment || !order.payment.token) {
      return Either.left({ code: "INVALID_PAYMENT", message: "Missing valid payment token" });
    }
    return Either.right(order);
  }

  // Pure transformation: Apply promotional discounts
  static applyDiscounts(order) {
    const total = order.items.reduce((sum, i) => sum + i.price * i.qty, 0);
    const discount = order.promoCode === "SAVE20" ? total * 0.20 : 0;
    return Either.right({
      ...order,
      subtotal: total,
      discount,
      finalTotal: total - discount
    });
  }

  // Composed Railway Pipeline
  static checkout(rawOrder) {
    return Either.right(rawOrder)
      .chain(OrderCheckoutPipeline.validateCart)
      .chain(OrderCheckoutPipeline.checkInventory)
      .chain(OrderCheckoutPipeline.validatePayment)
      .chain(OrderCheckoutPipeline.applyDiscounts);
  }
}

// Verification 1: Valid Order
const validOrder = {
  items: [
    { name: "Keyboard", price: 100, qty: 1, inventoryCount: 5 },
    { name: "Mouse", price: 50, qty: 1, inventoryCount: 2 }
  ],
  payment: { token: "tok_visa_123" },
  promoCode: "SAVE20"
};

const validResult = OrderCheckoutPipeline.checkout(validOrder);
validResult.fold(
  err => console.error("Checkout Failed:", err),
  success => console.log("Checkout Succeeded! Final Total: $" + success.finalTotal)
);
// Output: Checkout Succeeded! Final Total: $120

// Verification 2: Out of Stock Order
const badOrder = {
  items: [{ name: "Monitor", price: 300, qty: 1, inventoryCount: 0 }],
  payment: { token: "tok_visa_123" }
};

const badResult = OrderCheckoutPipeline.checkout(badOrder);
badResult.fold(
  err => console.log(`Checkout Failed [${err.code}]:`, err.message),
  success => console.log("Success:", success)
);
// Output: Checkout Failed [OUT_OF_STOCK]: Items out of stock: Monitor
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is a Functor in JavaScript?</strong></summary>

A Functor is any data structure that wraps a value and implements a `map` method adhering to the Identity and Composition laws. It allows you to apply a function to the wrapped value without altering the structure of the container itself (e.g. `[1, 2].map(x => x * 2)`).
</details>

<details>
<summary><strong>2. What is a Monad, and how does it differ from a Functor?</strong></summary>

A Monad is an extension of a Functor that, in addition to `map`, implements a `flatMap` (or `chain`) method. While `map` wraps the return value of a function in a new container ($A \to M(B)$ results in $M(M(B))$), `flatMap` unwraps and flattens nested containers, allowing sequential dependent operations to be chained together cleanly.
</details>

<details>
<summary><strong>3. What are the three Monad Laws?</strong></summary>

1. **Left Identity**: `Monad.of(a).chain(f)` must be identical to `f(a)`.
2. **Right Identity**: `m.chain(Monad.of)` must be identical to `m`.
3. **Associativity**: `m.chain(f).chain(g)` must be identical to `m.chain(x => f(x).chain(g))`.
</details>

<details>
<summary><strong>4. Why do JavaScript Promises violate strict mathematical Monad laws?</strong></summary>

Promises automatically flatten nested Promises inside `.then()`. In a strict Monad, mapping a value should preserve nested layers unless explicitly flattened. In addition, `.then()` serves as both `map` and `flatMap` dynamically based on whether the return value is a Promise or a primitive, violating categorical type separation.
</details>

<details>
<summary><strong>5. What is Railway-Oriented Programming?</strong></summary>

Railway-Oriented Programming is an error-handling architectural design pattern that models computations as parallel tracks: a "Green" success track and a "Red" failure track. Functions return an `Either` type (`Right` or `Left`). If any step encounters an error, execution switches to the Red track, bypassing all remaining steps and delivering the error directly to the end of the pipeline.
</details>

<details>
<summary><strong>6. What is the IO Monad, and why is it useful in a language without pure compile-time enforcement?</strong></summary>

The IO Monad wraps an impure side-effecting operation (such as reading a file or accessing `localStorage`) inside a lazy function. It allows side effects to be mapped, chained, and composed as pure, inert data structures. The actual side effect only executes when the caller explicitly triggers `.run()` at the application boundary.
</details>

<details>
<summary><strong>7. How does the Maybe Monad compare to modern optional chaining (<code>?.</code>) and nullish coalescing (<code>??</code>)?</strong></summary>

`?.` and `??` are native syntactic shortcuts that provide safe property traversal and fallbacks. The `Maybe` Monad is an algebraic abstraction that encapsulates nullability as a composable object, allowing it to be passed into generic functional pipelines, mapped with external functions, and integrated with other Monads.
</details>

<details>
<summary><strong>8. What is the purpose of the <code>fold</code> method on an Either Monad?</strong></summary>

`fold(leftFn, rightFn)` is the terminal extraction operation that forces the developer to handle both possible outcomes. If the instance is a `Left`, it invokes `leftFn(error)`; if it is a `Right`, it invokes `rightFn(value)`. This guarantees that errors cannot be silently ignored.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Maybe Safe Division (Easy)
Implement `safeDivide(numerator, denominator)` returning a `Maybe` instance that returns `Maybe.nothing()` if the denominator is 0.

<details>
<summary>View Solution</summary>

```javascript
function safeDivide(numerator, denominator) {
  if (denominator === 0) return Maybe.nothing();
  return Maybe.just(numerator / denominator);
}

console.log(safeDivide(10, 2).getOrElse("Div by zero")); // 5
console.log(safeDivide(10, 0).getOrElse("Div by zero")); // "Div by zero"
```
</details>

### Exercise 2: Functor Law Verification (Easy)
Write a test function verifying that a custom `Identity` functor satisfies both the Identity and Composition laws.

<details>
<summary>View Solution</summary>

```javascript
class Identity {
  constructor(val) { this.val = val; }
  static of(v) { return new Identity(v); }
  map(fn) { return Identity.of(fn(this.val)); }
}

const id = Identity.of(10);
// 1. Identity law
const law1 = id.map(x => x).val === id.val;

// 2. Composition law
const f = x => x + 2;
const g = x => x * 3;
const law2 = id.map(x => f(g(x))).val === id.map(g).map(f).val;

console.log("Identity Law holds:", law1);     // true
console.log("Composition Law holds:", law2);  // true
```
</details>

### Exercise 3: Safe Array Head / Tail Monad (Medium)
Create helper functions `head(array)` and `last(array)` that return a `Maybe` wrapping the element or `Nothing` if empty.

<details>
<summary>View Solution</summary>

```javascript
const head = arr => (arr && arr.length > 0 ? Maybe.just(arr[0]) : Maybe.nothing());
const last = arr => (arr && arr.length > 0 ? Maybe.just(arr[arr.length - 1]) : Maybe.nothing());

console.log(head([10, 20, 30]).getOrElse("Empty")); // 10
console.log(head([]).getOrElse("Empty"));           // "Empty"
console.log(last(["a", "b", "c"]).getOrElse("Empty")); // "c"
```
</details>

### Exercise 4: Validated Email Pipeline (Medium)
Build an input validator using the `Either` monad that verifies that an email contains an `@` sign and a valid domain extension.

<details>
<summary>View Solution</summary>

```javascript
function validateEmail(email) {
  if (!email || typeof email !== "string") {
    return Either.left("Email must be a string");
  }
  if (!email.includes("@")) {
    return Either.left("Email must contain an '@' symbol");
  }
  const parts = email.split("@");
  if (!parts[1].includes(".")) {
    return Either.left("Email domain must contain a dot extension");
  }
  return Either.right(email.trim().toLowerCase());
}

validateEmail("alice@domain.com").fold(
  err => console.log("Invalid:", err),
  valid => console.log("Valid email:", valid)
); // "Valid email: alice@domain.com"

validateEmail("bad-email").fold(
  err => console.log("Invalid:", err),
  valid => console.log("Valid:", valid)
); // "Invalid: Email must contain an '@' symbol"
```
</details>

### Exercise 5: Asynchronous Task Monad (Hard)
Implement a minimal `Task` monad that encapsulates an asynchronous operation and supports `map` and `chain` without executing until `.fork(reject, resolve)` is called.

<details>
<summary>View Solution</summary>

```javascript
class Task {
  #computation;
  constructor(computation) { this.#computation = computation; }

  static of(val) {
    return new Task((reject, resolve) => resolve(val));
  }

  static rejected(err) {
    return new Task((reject, resolve) => reject(err));
  }

  map(fn) {
    return new Task((reject, resolve) => {
      this.#computation(reject, val => {
        try { resolve(fn(val)); } catch (e) { reject(e); }
      });
    });
  }

  chain(fn) {
    return new Task((reject, resolve) => {
      this.#computation(reject, val => {
        try { fn(val).fork(reject, resolve); } catch (e) { reject(e); }
      });
    });
  }

  fork(reject, resolve) {
    this.#computation(reject, resolve);
  }
}

// Verification: Lazy execution
const fetchUserTask = id =>
  new Task((reject, resolve) => {
    setTimeout(() => resolve({ id, name: "Ada" }), 50);
  });

const pipeline = fetchUserTask(101)
  .map(user => user.name.toUpperCase())
  .map(name => `Welcome, ${name}!`);

pipeline.fork(
  err => console.error("Task Error:", err),
  res => console.log("Task Resolved:", res) // "Task Resolved: Welcome, ADA!"
);
```
</details>

### Exercise 6: Validation Applicative Functor (Hard)
Unlike `Either` (which fails fast on the first error), an **Applicative Validation** collects **all** validation errors in an array before returning. Implement `Validation.success` and `Validation.failure` that aggregate errors.

<details>
<summary>View Solution</summary>

```javascript
class Validation {
  constructor(isValid, value, errors = []) {
    this.isValid = isValid;
    this.value = value;
    this.errors = errors;
  }

  static success(value) {
    return new Validation(true, value, []);
  }

  static failure(error) {
    return new Validation(false, null, Array.isArray(error) ? error : [error]);
  }

  // Combine multiple validations
  and(otherValidation) {
    if (this.isValid && otherValidation.isValid) {
      return Validation.success([this.value, otherValidation.value]);
    }
    const combinedErrors = [...this.errors, ...otherValidation.errors];
    return Validation.failure(combinedErrors);
  }
}

// Validate fields independently
const checkName = name => (name.length >= 3 ? Validation.success(name) : Validation.failure("Name too short"));
const checkAge = age => (age >= 18 ? Validation.success(age) : Validation.failure("Must be 18+"));
const checkEmail = email => (email.includes("@") ? Validation.success(email) : Validation.failure("Invalid email"));

const result = checkName("Ed")
  .and(checkAge(16))
  .and(checkEmail("valid@test.com"));

console.log("Is valid?", result.isValid); // false
console.log("Collected errors:", result.errors);
// [ 'Name too short', 'Must be 18+' ] (Collected all errors!)
```
</details>
