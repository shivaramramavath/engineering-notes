# Getters and Setters

> Accessors (getters and setters) intercept property reads and writes, executing validation and computed transformations while preserving standard field syntax.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-classes.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/00-classes.md), [01-access-modifiers.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/01-access-modifiers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 4.3 (Separate Getter/Setter Types) / TS 5.0 (`accessor` keyword) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `target: "ES2022"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
- **Problem Statement:** Direct field assignment exposes object state to invalid mutations (e.g. assigning negative account balances, unformatted phone numbers, or invalid timestamps). Replacing fields with explicit methods (`getBalance()`, `setBalance()`) clutters syntax and breaks existing public API contracts.
- **Historical Context:** TypeScript supported `get` and `set` from version 0.9. Prior to TypeScript 4.3, a setter's parameter type had to match the getter's return type exactly. TypeScript 4.3 permitted setters to accept a broader type than the getter returns (e.g. setter accepts `string | number`, getter always returns `number`). TypeScript 5.0 introduced the `accessor` keyword for Stage 3 decorators.
- **Real-World Relevance:** Accessors power computed properties in domain models, lazy evaluation, reactive signals (MobX, Vue, Solid), and defensive input validation while maintaining clean dot-notation property access (`user.fullName`).

---

## 2. Core Concepts
- **Formal Invariants:**
  - Prototype Definition: Accessors are defined on the class `prototype` using `Object.defineProperty()`. They are NOT own properties of the instance.
  - Read-Only Property: If a property has a `get` accessor but no `set` accessor, TypeScript infers the property as `readonly`.
  - Type Relationship (TS 4.3+): A setter parameter type must be assignable to the getter return type, OR the getter return type must be assignable to the setter parameter type.
  - Auto-Accessors (`accessor` in TS 5.0+): The `accessor` keyword generates a private backing field with automatic getter and setter pairs for Stage 3 decorators.
- **Mental Model:** A getter is an invisible zero-argument method that disguises itself as a property read. A setter is an invisible unary method that disguises itself as an assignment.
- **Soundness vs Ergonomics:** Allowing setters to accept broader input types than getters return (TS 4.3) enables ergonomic input coercions (accepting strings or numbers while normalizing to numbers).

---

## 3. How It Works Internally
- **Prototype Property Descriptors:** Emits an accessor property descriptor on `Constructor.prototype`:
  ```javascript
  Object.defineProperty(User.prototype, "fullName", {
    get: function() { return this.first + " " + this.last; },
    enumerable: false,
    configurable: true
  });
  ```
- **Type Checking:** The type checker resolves property accesses on accessors by dispatching reads to the `get` signature and writes to the `set` signature.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Getter / Setter Interception Pipeline                   |
|                                                                          |
|   Assignment:  box.width = "400px";                                      |
|                     │                                                    |
|                     ▼ (Invokes Setter)                                   |
|   set width(val: string | number) {                                      |
|     this.#width = typeof val === "string" ? parseInt(val, 10) : val;     |
|   }                                                                      |
|                     │                                                    |
|                     ▼ (Normalizes state to #width: 400)                  |
|   Read:        console.log(box.width);                                   |
|                     │                                                    |
|                     ▼ (Invokes Getter)                                   |
|   get width(): number { return this.#width; }  ──► Returns 400 (number)  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Standard Getter and Setter:**
  ```typescript
  class Rectangle {
    #width: number = 0;

    get width(): number {
      return this.#width;
    }

    set width(val: number) {
      if (val < 0) throw new RangeError('Width cannot be negative');
      this.#width = val;
    }
  }
  ```
- **Different Getter/Setter Types (TS 4.3+):**
  ```typescript
  class Dimension {
    #meters: number = 0;

    get meters(): number {
      return this.#meters;
    }

    set meters(value: string | number) {
      this.#meters = typeof value === 'string' ? parseFloat(value) : value;
    }
  }
  ```
- **Auto-Accessors (TS 5.0+):**
  ```typescript
  class SignalEntity {
    accessor count: number = 0; // Auto-generates backing field & getter/setter
  }
  ```
- **tsconfig Dependencies:**
  - Requires `target: "ES5"` or higher (ES3 did not support `Object.defineProperty`).

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compute derived property dynamically without storing redundant state.
- **Code:**
```typescript
export class Person {
  constructor(public firstName: string, public lastName: string) {}

  public get fullName(): string {
    return `${this.firstName} ${this.lastName}`.trim();
  }

  public set fullName(name: string) {
    const parts = name.split(' ');
    this.firstName = parts[0] ?? '';
    this.lastName = parts.slice(1).join(' ');
  }
}

const p = new Person('John', 'Doe');
console.log(p.fullName); // "John Doe"

p.fullName = 'Jane Smith';
console.log(p.firstName, p.lastName); // "Jane", "Smith"
```
- **Output / Type Evaluation:**
```typescript
// Two-way synchronization between fullName and firstName/lastName.
```
- **Explanation:** Accessors encapsulate split and join string operations transparently.

### Example 2: Real-World Enterprise Pattern (Input Normalization with TS 4.3+ Type Mismatch)
- **Goal:** Accept strings or numbers in a setter while always returning a normalized number in the getter.
- **Code:**
```typescript
export class FinancialAmount {
  #cents: number = 0;

  public get amount(): number {
    return this.#cents / 100;
  }

  // Setter accepts currency string or number:
  public set amount(value: string | number) {
    if (typeof value === 'string') {
      const cleaned = value.replace(/[^0-9.-]+/g, '');
      this.#cents = Math.round(parseFloat(cleaned) * 100);
    } else {
      this.#cents = Math.round(value * 100);
    }
  }
}

const item = new FinancialAmount();
item.amount = '$49.99'; // Assign string!
console.log(item.amount); // 49.99 (returns number!)

item.amount = 12.5;     // Assign number!
console.log(item.amount); // 12.5
```
- **Output / Type Evaluation:**
```typescript
// Setter type: string | number; Getter type: number
```
- **Explanation:** In TypeScript 4.3+, setters can accept broader types than getters return, facilitating input sanitization.

### Example 3: Edge Case / Boundary Condition (Read-Only Inferred Property)
- **Goal:** Create a read-only property using a getter without a setter.
- **Code:**
```typescript
class ServerNode {
  readonly #bootTime = new Date();

  public get uptimeMs(): number {
    return Date.now() - this.#bootTime.getTime();
  }
}

const node = new ServerNode();
console.log(node.uptimeMs);

// @ts-expect-error TS2540: Cannot assign to 'uptimeMs' because it is a read-only property.
node.uptimeMs = 1000;
```
- **Output / Type Evaluation:**
```typescript
// TS2540 prevents assignment to a property that defines only a getter.
```
- **Explanation:** A getter without a corresponding setter is inferred by the compiler as `readonly`.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection when setter parameter is incompatible with getter.
- **Code:**
```typescript
class BadAccessors {
  // @ts-expect-error TS2380: The return type of a 'get' accessor must be assignable to its 'set' accessor type.
  get status(): number {
    return 1;
  }

  set status(val: string) {
    // string and number have no overlap!
  }
}
```
- **Real Compiler Error:**
```
error TS2380: The return type of a 'get' accessor must be assignable to its 'set' accessor type.
```
- **Explanation:** While TS 4.3 relaxed getter/setter types, they must still have an assignability relationship.

### Example 5: Idiomatic Modern Usage (Auto-Accessors with Stage 3 Decorators in TS 5.0+)
- **Goal:** Use modern `accessor` keyword to declare reactive observable state.
- **Code:**
```typescript
export class ObservableModel {
  // Modern auto-accessor keyword (TS 5.0+)
  accessor title: string = 'Initial';
  accessor count: number = 0;
}

const model = new ObservableModel();
model.title = 'Updated Title';
console.log(model.title); // "Updated Title"
```
- **Output / Type Evaluation:**
```typescript
// Emits clean private backing storage with standard accessor dispatch.
```
- **Explanation:** The `accessor` keyword reduces boilerplate when integrating with Stage 3 field decorators.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Getter Side Effects:** Getters should remain pure and fast. Triggering network requests, heavy computation, or state mutations inside a getter violates consumer expectations and slows down debugging.
2. **Infinite Recursion:** Writing `get name() { return this.name; }` or `set name(val) { this.name = val; }` causes an infinite loop and `RangeError: Maximum call stack size exceeded`. Always store state in a distinct backing field (`#name` or `_name`).
3. **Loss of Enumerable Keys:** Accessors defined on a class prototype are NOT enumerable by default (`enumerable: false`). Running `Object.keys(instance)` or `{ ...instance }` will NOT include accessor properties!

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Reading/writing the accessor inside itself | `RangeError: Maximum call stack size exceeded` | Use private backing field (`#field`) | Prevents infinite recursion |
| Expecting accessors in `{ ...instance }` spread | Spread ignores prototype accessors (empty fields) | Manually copy or serialize | Prototype properties are not own-enumerable |
| Heavy I/O or side effects inside getter | Non-deterministic performance; debugger freezes | Use explicit async method (`loadData()`) | Getters must be pure and instantaneous |
| Setter parameter incompatible with getter | `TS2380: Return type of get must be assignable` | Align types or make setter a superset | Preserves type coherence |
| Async getters: `async get prop()` | `TS1059: A 'get' accessor cannot be async` | Return `Promise<T>` or use method | ECMAScript syntax forbids async accessors |
| Manually writing backing fields with decorators | Verbose boilerplate | Use `accessor` keyword (TS 5.0+) | Leverages TC39 Stage 3 auto-accessors |

---

## 8. Performance & Type-Checker Complexity
- **JIT Deoptimization:** Accessor property reads require function invocation overhead in V8 compared to raw field reads. If an accessor is placed in a hot loop (millions of calls), V8 must inline the getter to avoid performance penalties.
- **Type Checker Caching:** The checker records both get-signature and set-signature on a single `PropertySymbol` with `SymbolFlags.Accessor`.
- **Isolated Declarations:** Exported accessors must explicitly annotate getter return types and setter parameter types under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** `ts.GetAccessorDeclaration` (`SyntaxKind.GetAccessor`), `ts.SetAccessorDeclaration` (`SyntaxKind.SetAccessor`), `ts.PropertyDeclaration` with `SyntaxKind.AccessorKeyword`.
- **Symbol Flags:** Tagged with `SymbolFlags.GetAccessor` and `SymbolFlags.SetAccessor`.
- **Assignability Checking:** `checkAccessorDeclaration` validates relationship rules between getter and setter.

---

## 10. Best Practices & Design Guidelines
- **Always Use Private Backing Fields:** Back accessors with ECMAScript `#private` fields to prevent external circumvention.
- **Keep Getters Pure and Fast:** Never perform asynchronous work or mutations inside getters.
- **Widen Setters Responsibly (TS 4.3+):** Accept forgiving input types in setters, but always normalize to a strict, predictable getter return type.

---

## 11. Real-World Case Study
- **Scenario:** An e-commerce pricing engine stored product prices in dollars as numbers: `product.price = 19.99`. Due to floating point imprecision, calculations like `19.99 * 3` produced `59.970000000000006`. Furthermore, developers frequently passed raw string inputs from forms (`"19.99"`), causing string concatenation bugs like `"19.99" + 5 = "19.995"`.
- **Implementation:** Re-engineered `Product` with integer-cent storage and accessors:
  ```typescript
  export class Product {
    #cents: number = 0;

    public get price(): number {
      return this.#cents / 100;
    }

    public set price(val: string | number) {
      const num = typeof val === 'string' ? parseFloat(val) : val;
      if (isNaN(num) || num < 0) throw new TypeError('Invalid price');
      this.#cents = Math.round(num * 100);
    }
  }
  ```
- **Outcome:** Eliminated 100% of floating-point rounding errors and string concatenation bugs while preserving clean `product.price` access across the entire frontend.

---

## 12. Interview Questions

### Q1: What happens under the hood when you define a getter and setter in a TypeScript class?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When a getter and setter are defined on a class, they are not created on the instance itself. Instead, the compiler emits a call to `Object.defineProperty()` targeting `ClassName.prototype`. The property descriptor specifies the getter function as `get` and the setter function as `set`, with `enumerable: false` and `configurable: true`. When a caller accesses `instance.prop`, JavaScript walks the prototype chain, finds the descriptor on the prototype, and invokes the function.
</details>

### Q2: Why are accessors not copied when using the object spread operator (`{ ...instance }`)?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The object spread operator (`{ ...obj }`) and `Object.assign()` only copy **own, enumerable** properties directly residing on the source object. Because class getters and setters are defined on the class's **prototype** (and are non-enumerable by default), the spread operator completely ignores them, resulting in an object missing all accessor properties.
</details>

### Q3: How did TypeScript 4.3 relax the relationship between getters and setters?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Prior to TypeScript 4.3, a setter's parameter type was required to be strictly identical to the getter's return type. TypeScript 4.3 permitted getters and setters to declare different types, provided that either:
1. The getter's return type is assignable to the setter's parameter type, OR
2. The setter's parameter type is assignable to the getter's return type.
This allows writing setters that accept broader, unnormalized input types (e.g. `string | number`) while the getter returns a strictly normalized type (e.g. `number`).
</details>

### Q4: Can a getter be declared as `async` in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. In ECMAScript and TypeScript, an accessor cannot be marked with the `async` keyword (`async get data()`). Attempting to do so triggers compile error `TS1059: A 'get' accessor cannot be 'async'`. A getter can return a `Promise<T>` (`get data(): Promise<T>`), but it must be invoked synchronously, and callers must await the returned property (`await obj.data`).
</details>

### Q5: What is the `accessor` keyword introduced in TypeScript 5.0?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The `accessor` keyword is part of the TC39 Stage 3 Decorators proposal supported in TypeScript 5.0+. Writing `accessor foo: string = "bar";` is an auto-accessor. The compiler automatically creates a private backing storage slot (`#foo`) and generates a corresponding getter and setter. This allows Stage 3 decorators to wrap or intercept property reads and writes directly.
</details>

### Q6: What happens if a class defines a getter without a setter?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

If a property has a getter but no setter, TypeScript automatically infers the property as `readonly`. Any attempt by callers to assign a value to that property (`instance.prop = val`) causes compile-time error `TS2540: Cannot assign to 'prop' because it is a read-only property`.
</details>

### Q7: Why does writing `set age(val: number) { this.age = val; }` cause a runtime crash?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Inside the setter `set age(val)`, executing `this.age = val` is an assignment to `this.age`. Because `age` is governed by the setter, this assignment invokes the setter itself. The setter calls the setter, leading to infinite recursion until the JavaScript engine throws `RangeError: Maximum call stack size exceeded`. Setters must always store values in a separate private backing field (e.g. `this.#age = val`).
</details>

### Q8: Can accessors be declared on an interface?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In an interface, you cannot write `get` or `set` keywords. Instead, you declare the property normally:
- If read-only: `readonly count: number;`
- If read-write: `count: number;`
A class implementing that interface can satisfy `readonly count` using a getter, and can satisfy read-write `count` using both a getter and a setter.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Temperature Converter Accessor (Easy)
- **Problem Statement:** Author a `Temperature` class with `#celsius` backing storage, providing a `fahrenheit` getter and setter converting between Celsius and Fahrenheit.
- **Test Case:**
```typescript
const t = new Temperature();
t.fahrenheit = 212;
console.log(t.celsius); // 100
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Temperature {
  #celsius: number = 0;

  get celsius(): number {
    return this.#celsius;
  }

  set celsius(c: number) {
    this.#celsius = c;
  }

  get fahrenheit(): number {
    return (this.#celsius * 9) / 5 + 32;
  }

  set fahrenheit(f: number) {
    this.#celsius = ((f - 32) * 5) / 9;
  }
}
```
</details>

### Exercise 2: Trimmed String Property (Easy)
- **Problem Statement:** Create a class `SanitizedInput` where setter for `value` strips leading and trailing whitespace.
- **Test Case:**
```typescript
const input = new SanitizedInput();
input.value = '   hello   ';
console.log(input.value); // "hello"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class SanitizedInput {
  #val: string = '';

  get value(): string {
    return this.#val;
  }

  set value(v: string) {
    this.#val = v.trim();
  }
}
```
</details>

### Exercise 3: Flexible Date Accessor (Medium)
- **Problem Statement:** Implement a `Schedule` class with a `startDate` property whose setter accepts `Date | string | number`, but whose getter always returns a `Date`.
- **Test Case:**
```typescript
const s = new Schedule();
s.startDate = '2025-01-01';
console.log(s.startDate instanceof Date); // true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Schedule {
  #date: Date = new Date();

  get startDate(): Date {
    return this.#date;
  }

  set startDate(val: Date | string | number) {
    this.#date = val instanceof Date ? val : new Date(val);
  }
}
```
</details>

### Exercise 4: Validated Range Accessor (Medium)
- **Problem Statement:** Build a class `Percentage` whose setter throws `RangeError` if value is not between 0 and 100.
- **Test Case:**
```typescript
const p = new Percentage();
p.value = 50; // OK
// p.value = 150 throws RangeError!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Percentage {
  #pct: number = 0;

  get value(): number {
    return this.#pct;
  }

  set value(n: number) {
    if (n < 0 || n > 100) {
      throw new RangeError('Percentage must be between 0 and 100');
    }
    this.#pct = n;
  }
}
```
</details>

### Exercise 5: Compile-Time Readonly Property Detector (Hard)
- **Problem Statement:** Write a utility type `IsReadonlyProperty<T, K extends keyof T>` that evaluates to `true` if property `K` lacks a setter.
- **Test Case:**
```typescript
class TestClass {
  get ro(): number { return 1; }
  get rw(): number { return 2; }
  set rw(v: number) {}
}
type T1 = IsReadonlyProperty<TestClass, 'ro'>; // true
type T2 = IsReadonlyProperty<TestClass, 'rw'>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type IfEquals<X, Y, A, B> =
  (<T>() => T extends X ? 1 : 2) extends <T>() => T extends Y ? 1 : 2 ? A : B;

type IsReadonlyProperty<T, K extends keyof T> =
  IfEquals<{ [P in K]: T[P] }, { -readonly [P in K]: T[P] }, false, true>;
```
</details>

### Exercise 6: Auto-Caching Computed Accessor (Hard)
- **Problem Statement:** Author a class with an expensive computed accessor that caches the computed result until an underlying dependency property is modified.
- **Test Case:**
```typescript
const analyzer = new LogAnalyzer();
analyzer.addLog('e1');
console.log(analyzer.summary); // Computes
console.log(analyzer.summary); // Returns cached
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class LogAnalyzer {
  private logs: string[] = [];
  private cachedSummary: string | null = null;

  addLog(entry: string): void {
    this.logs.push(entry);
    this.cachedSummary = null; // Invalidate cache
  }

  get summary(): string {
    if (this.cachedSummary === null) {
      console.log('Computing expensive summary...');
      this.cachedSummary = `Total logs: ${this.logs.length}`;
    }
    return this.cachedSummary;
  }
}
```
</details>
