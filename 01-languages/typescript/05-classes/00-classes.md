# Classes in TypeScript

> Classes combine object-oriented encapsulation, prototype inheritance, and structural type compatibility with static and instance type bifurcation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [03-objects.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/03-objects.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.0+ / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `exactOptionalPropertyTypes: true`, `useDefineForClassFields: true` |

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
- **Problem Statement:** Large-scale enterprise systems require domain encapsulation, lifecycle state tracking, and reusable behavior. While JavaScript supports ES2015 classes, plain JavaScript provides zero static verification for property types, uninitialized fields, or member visibility.
- **Historical Context:** TypeScript introduced class declarations in version 0.9, years before ES2015 standardized classes in JavaScript. TypeScript's class system introduces dual symbol tables (instance type vs constructor function), property initializers, access modifiers, and structural subtyping.
- **Real-World Relevance:** Classes remain foundational in backend frameworks (NestJS, Fastify controllers, TypeORM/Prisma models, MikrORM), Angular, state management stores, and domain-driven design (DDD) entities.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Dual Type Nature: A `class C` declaration introduces TWO distinct symbols: (1) a type named `C` representing instance shapes, and (2) a value named `C` representing the constructor function in value space.
  - Structural Assignability: Classes are compared structurally. If class `A` and class `B` have identical public shapes, they are assignable to each other unless they contain private or protected fields.
  - Definite Assignment Checking: Under `strictPropertyInitialization: true`, every declared instance property must be initialized in its declaration or within the constructor.
  - `useDefineForClassFields: true`: Modern class fields emit `Object.defineProperty()` adhering to ECMAScript standard field semantics rather than prototype assignment.
- **Mental Model:** A class is both a factory (the constructor function in value space) and a blueprint (the interface describing instances in type space).
- **Soundness vs Ergonomics:** Private fields (`private x` or `#x`) break pure structural assignability: two classes with identical private fields from different declarations are NEVER assignable to each other.

---

## 3. How It Works Internally
- **Symbol Table Allocation:** The binder generates a `Symbol` with both `SymbolFlags.Class` and `SymbolFlags.Interface`. The static members live on the constructor symbol; the instance members live on the instance type symbol.
- **Field Initialization Order:**
  1. Base class parameter properties and field initializers.
  2. Base class constructor body.
  3. Derived class field initializers.
  4. Derived class constructor body.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                     Dual Type Space of a TypeScript Class                |
|                                                                          |
|                           class Engine { ... }                           |
|                                     │                                    |
|              ┌──────────────────────┴──────────────────────┐             |
|              ▼                                             ▼             |
|        [ VALUE SPACE ]                               [ TYPE SPACE ]      |
|   Identifier: Engine                            Type Identifier: Engine  |
|   ├── Prototype object                          ├── Represents instance  |
|   ├── Static methods & properties               ├── Instance members     |
|   └── Constructor function (callable via new)   └── Checked structurally |
|                                                                          |
|   const eng: Engine = new Engine();             type Ctor = typeof Engine|
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Class Declaration Syntax:**
  ```typescript
  class Account {
    readonly id: string;
    public balance: number;
    protected isFrozen: boolean = false;

    constructor(id: string, initialBalance: number) {
      this.id = id;
      this.balance = initialBalance;
    }

    deposit(amount: number): void {
      this.balance += amount;
    }
  }
  ```
- **Constructor Type Reference:**
  ```typescript
  type AccountConstructor = typeof Account; // new (id: string, initialBalance: number) => Account
  ```
- **tsconfig Dependencies:**
  - `strictPropertyInitialization: true` (under `strict: true`): Enforces that all instance fields are initialized.
  - `useDefineForClassFields: true`: Uses `Object.defineProperty` for fields (default for `target: "ES2022"`+).

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Declare a domain model with strong property initialization.
- **Code:**
```typescript
export class UserEntity {
  public readonly id: string;
  public name: string;
  public email: string;
  private loginAttempts: number = 0;

  constructor(id: string, name: string, email: string) {
    this.id = id;
    this.name = name;
    this.email = email;
  }

  public recordFailedLogin(): void {
    this.loginAttempts++;
  }

  public isLockedOut(): boolean {
    return this.loginAttempts >= 5;
  }
}

const user = new UserEntity('u_101', 'Alice', 'alice@corp.com');
user.recordFailedLogin();
console.log(user.isLockedOut()); // false
```
- **Output / Type Evaluation:**
```typescript
// Fully typed class instance; loginAttempts cannot be accessed externally.
```
- **Explanation:** Initializers and constructor assignments satisfy strict property initialization checks.

### Example 2: Real-World Enterprise Pattern (Repository Pattern with Dual Types)
- **Goal:** Accept a class constructor token and return an instance of that class.
- **Code:**
```typescript
export class DatabaseContext {
  private tables = new Map<string, unknown[]>();

  getRepository<T extends object>(entityCtor: new (...args: any[]) => T): T[] {
    const tableName = entityCtor.name.toLowerCase();
    const records = this.tables.get(tableName) ?? [];
    return records as T[];
  }
}

class Customer {
  constructor(public id: string, public name: string) {}
}

const db = new DatabaseContext();
const customers: Customer[] = db.getRepository(Customer);
```
- **Output / Type Evaluation:**
```typescript
// customers is inferred as Customer[] using entityCtor constructor token.
```
- **Explanation:** Passing `Customer` (the value) satisfies the construct signature, allowing the return type `T[]` to infer `Customer` (the instance type).

### Example 3: Edge Case / Boundary Condition (Structural Subtyping Across Classes)
- **Goal:** Demonstrate structural equivalence between different classes.
- **Code:**
```typescript
class Point2D {
  constructor(public x: number, public y: number) {}
}

class Vector2D {
  constructor(public x: number, public y: number) {}
}

let p: Point2D = new Point2D(10, 20);
// Perfectly legal: Vector2D has identical public structure to Point2D!
p = new Vector2D(30, 40);

class SecretPoint {
  private id = 'secret';
  constructor(public x: number, public y: number) {}
}

// @ts-expect-error TS2322: Types have separate declarations of a private property 'id'.
p = new SecretPoint(50, 60);
```
- **Output / Type Evaluation:**
```typescript
// Structural subtyping applies to public fields; private fields enforce nominal identity.
```
- **Explanation:** Classes with identical public shapes are interchangeable in TypeScript, preserving structural typing unless private/protected fields are present.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler error under `strictPropertyInitialization`.
- **Code:**
```typescript
// tsconfig: { "strictPropertyInitialization": true }
class OrderRecord {
  orderId: string;

  // @ts-expect-error TS2564: Property 'total' has no initializer and is not definitely assigned in the constructor.
  total: number;

  constructor(orderId: string) {
    this.orderId = orderId;
    // Forgot to initialize this.total!
  }
}
```
- **Real Compiler Error:**
```
error TS2564: Property 'total' has no initializer and is not definitely assigned in the constructor.
```
- **Explanation:** The compiler checks constructor control flow paths to guarantee that every field receives a value before constructor completion.

### Example 5: Idiomatic Modern Usage (Definite Assignment Assertion)
- **Goal:** Safely handle fields initialized asynchronously or via dependency injection frameworks.
- **Code:**
```typescript
export class FastifyService {
  // Definite assignment assertion (!) informs TS that DI container will inject this:
  private logger!: { info(msg: string): void };

  public injectDependencies(logger: { info(msg: string): void }): void {
    this.logger = logger;
  }

  public handleRequest(): void {
    this.logger.info('Processing request');
  }
}

const service = new FastifyService();
service.injectDependencies(console);
service.handleRequest();
```
- **Output / Type Evaluation:**
```typescript
// Clean DI integration without disabling strict property initialization globally.
```
- **Explanation:** The `!` definite assignment assertion operator satisfies the compiler when fields are populated through external frameworks.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`useDefineForClassFields` Hazard:** Under ES2022 (`useDefineForClassFields: true`), class fields without initializers (`foo: string;`) emit `Object.defineProperty(this, "foo", { value: undefined })`. In derived classes, this will shadow and overwrite getters defined on base classes!
2. **Method Shorthand vs Arrow Property:** Methods declared as `greet() {}` live on the prototype and share a single function reference. Arrow properties `greet = () => {}` create a new bound closure per instance, increasing memory footprint.
3. **`instanceof` Across Bundles:** As with plain JS, `instanceof` checks can fail if two micro-frontends bundle duplicate copies of the same class definition.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Disabling `strictPropertyInitialization` | Silent `undefined` values in runtime fields | Keep enabled; use initializers or `!` | Prevents uninitialized field crashes |
| Arrow function properties for all methods | 10x memory bloat in high-volume instances | Prototype methods; bind in constructor | Optimizes V8 hidden classes and memory |
| Passing instance where constructor expected | `TS2345: Argument not assignable to construct signature` | Pass `typeof MyClass` | Distinguishes class value from instance |
| Assuming classes provide nominal type safety | Unrelated class assignable if public shapes match | Add private field or unique symbol brand | Enforces true nominal type identity |
| Overwriting base getter with uninitialized field | Base getter shadowed by `undefined` | Use `declare` modifier or constructor assign | Respects ES2022 `Object.defineProperty` |
| Calling methods before `super()` in derived class | `TS17009: 'super' must be called before accessing 'this'` | Call `super()` first in constructor | Adheres to ECMAScript class specification |

---

## 8. Performance & Type-Checker Complexity
- **Dual Symbol Checks:** The compiler allocates two distinct symbol nodes for every class. Checking constructor calls involves validating the constructor symbol; checking instances involves the interface symbol.
- **V8 Hidden Classes:** Consistent property initialization order inside constructors ensures V8 optimizes instances with monomorphic hidden classes.
- **Isolated Declarations:** Exported class members must have explicit type annotations under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** `ts.ClassDeclaration` (`SyntaxKind.ClassDeclaration`), `ts.ConstructorDeclaration` (`SyntaxKind.Constructor`), `ts.PropertyDeclaration` (`SyntaxKind.PropertyDeclaration`).
- **Binder Processing:** The binder allocates a class symbol and binds its members into both `symbol.members` (instance) and `symbol.exports` (static).
- **Emitter Processing:** Respects `target` version; targets below ES6 transpile classes into prototype-manipulating constructor functions.

---

## 10. Best Practices & Design Guidelines
- **Always Enable `strictPropertyInitialization`:** Enforce definite field assignment.
- **Use Prototype Methods for Reusable Behavior:** Keep memory footprint low.
- **Leverage Interfaces for Decoupling:** Program against interfaces (`implements Interface`) rather than concrete class inheritance.

---

## 11. Real-World Case Study
- **Scenario:** A gaming backend simulated 100,000 active player entities. The `Player` class declared 15 methods using arrow function properties: `updatePosition = () => { ... }`. Node.js crashed with `JavaScript heap out of memory` at 20,000 players because each player allocated 15 unique closure functions in memory.
- **Implementation:** Refactored arrow properties to prototype methods:
  ```typescript
  // Before (allocates 15 closures per player):
  class Player { updatePosition = () => { ... }; }

  // After (shares prototype methods across all 100,000 players):
  class Player { updatePosition(): void { ... } }
  ```
- **Outcome:** Reduced memory footprint per player from 2.8 KB to 140 bytes, allowing all 100,000 concurrent entities to execute smoothly on a standard 2 GB container.

---

## 12. Interview Questions

### Q1: What does it mean that a TypeScript class introduces both a type and a value?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When a class `User` is declared in TypeScript, the identifier `User` occupies both the type namespace and the value namespace:
1. **In Type Space:** `User` represents the instance type—the structural interface describing the properties and methods on an instantiated `User` object.
2. **In Value Space:** `User` represents the constructor function—the runtime value that can be called with `new User()` or used to read static properties. To refer to the constructor's type in type space, you write `typeof User`.
</details>

### Q2: Why are two classes with identical public properties structurally assignable to each other in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript uses a structural type system (duck typing). In structural typing, type compatibility is determined entirely by the members of the types, not by their declaration names or inheritance lineage. If `ClassA` and `ClassB` possess identical public members, an instance of `ClassA` satisfies every requirement of `ClassB`. However, if either class declares a `private` or `protected` field, structural compatibility is broken: private and protected fields must originate from the exact same class declaration.
</details>

### Q3: How does `useDefineForClassFields` change emitted JavaScript for class properties?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Historically, TypeScript emitted class field initializers using simple assignment (`this.foo = value`). ECMAScript 2022 standardized class fields to use `Object.defineProperty(this, "foo", { value, writable: true, enumerable: true, configurable: true })`. When `useDefineForClassFields: true` is set:
1. Fields are created using `Object.defineProperty`.
2. Existing getters or setters on parent prototypes are NOT triggered during initialization.
3. Uninitialized fields (`x: string;`) define the property as `undefined`, which can accidentally overwrite parent prototype accessors.
</details>

### Q4: What does `strictPropertyInitialization` enforce and how can you satisfy it?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`strictPropertyInitialization` ensures that every declared class property is definitely assigned a value by the time the constructor completes. You can satisfy it by:
1. Providing an inline initializer: `name: string = "default";`
2. Assigning the property inside the constructor body: `this.name = name;`
3. Using constructor parameter properties: `constructor(public name: string)`
4. Using the definite assignment assertion (`name!: string`) if initialized asynchronously or via dependency injection.
</details>

### Q5: What is the difference between prototype methods and arrow function properties on a class?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **Prototype Methods (`method() {}`):** Defined on the class prototype (`MyClass.prototype`). All instances share the exact same function object in memory. However, `this` is dynamic and will be lost if the method is detached from its object without binding.
- **Arrow Function Properties (`method = () => {}`):** Defined on the instance itself. A brand new closure function is allocated for every created instance, consuming significantly more memory. However, `this` is lexically bound to the instance, making it safe to pass as detached callbacks.
</details>

### Q6: Can a class implement another class?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. In TypeScript, `class A implements B` is legal. Because `B` introduces an instance type interface, `A` treats `B` as an interface, meaning `A` must implement all public properties and methods declared in `B`. However, `A` does not inherit any implementation or prototype logic from `B` (it does not call `super()`).
</details>

### Q7: Why does TypeScript require `super()` to be called before accessing `this` in a derived class?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In ECMAScript 2015+, derived class constructors do not create their own `this` object. Instead, the `this` binding is created by the base class constructor when `super()` is called. Attempting to read or assign to `this` prior to invoking `super()` throws a runtime `ReferenceError` in JavaScript and is flagged as `TS17009` by the TypeScript compiler.
</details>

### Q8: What is the `declare` property modifier on class fields?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The `declare` modifier on a class field (`declare foo: string;`) informs the compiler that the property exists on the instance type, but suppresses the emission of any field initialization code in the emitted JavaScript. This prevents `useDefineForClassFields` from defining `undefined` over a property managed by a parent class getter/setter.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Definite Initialized Counter (Easy)
- **Problem Statement:** Author a class `Counter` with private `count` initialized to 0, providing `increment()`, `decrement()`, and `value` getter.
- **Test Case:**
```typescript
const c = new Counter();
c.increment();
console.log(c.value); // 1
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Counter {
  private count: number = 0;

  increment(): void {
    this.count++;
  }

  decrement(): void {
    this.count--;
  }

  get value(): number {
    return this.count;
  }
}
```
</details>

### Exercise 2: Generic Stack Class (Easy)
- **Problem Statement:** Implement a generic class `Stack<T>` with `push(item: T)`, `pop(): T | undefined`, and `peek(): T | undefined`.
- **Test Case:**
```typescript
const s = new Stack<number>();
s.push(10);
s.push(20);
console.log(s.pop()); // 20
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Stack<T> {
  private items: T[] = [];

  push(item: T): void {
    this.items.push(item);
  }

  pop(): T | undefined {
    return this.items.pop();
  }

  peek(): T | undefined {
    return this.items[this.items.length - 1];
  }
}
```
</details>

### Exercise 3: Structural Compatibility Enforcer (Medium)
- **Problem Statement:** Write two classes `Square` and `Rectangle` where `Square` is assignable to `Rectangle`, but `Rectangle` is not assignable to `Square` due to extra dimensions.
- **Test Case:**
```typescript
let r: Rectangle = new Square(10); // OK
// let s: Square = new Rectangle(10, 20); // Error
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Rectangle {
  constructor(public width: number, public height: number) {}
}

class Square {
  public height: number;
  constructor(public width: number) {
    this.height = width;
  }
  public isSquare = true; // Extra property breaks assignability from Rectangle
}
```
</details>

### Exercise 4: Factory Pattern with Static Creator (Medium)
- **Problem Statement:** Create a class `UserSession` with a private constructor and a static `create(email: string): UserSession` factory that validates emails before creation.
- **Test Case:**
```typescript
const session = UserSession.create('user@corp.com');
// new UserSession('user@corp.com') should fail compilation!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class UserSession {
  private constructor(public readonly email: string, public readonly createdAt: Date) {}

  static create(email: string): UserSession {
    if (!email.includes('@')) {
      throw new Error('Invalid email');
    }
    return new UserSession(email, new Date());
  }
}
```
</details>

### Exercise 5: Compile-Time Class Instance Validator (Hard)
- **Problem Statement:** Write a type-level utility `IsSubclassOf<Derived, Base>` that verifies if class `Derived` extends or satisfies class `Base`.
- **Test Case:**
```typescript
class Animal {}
class Dog extends Animal {}
type T1 = IsSubclassOf<typeof Dog, typeof Animal>; // true
type T2 = IsSubclassOf<typeof Animal, typeof Dog>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type IsSubclassOf<D extends abstract new (...args: any) => any, B extends abstract new (...args: any) => any> =
  InstanceType<D> extends InstanceType<B> ? true : false;
```
</details>

### Exercise 6: Nominal Class Branding via Unique Symbol (Hard)
- **Problem Statement:** Build two classes with identical structural fields (`id`, `name`) that CANNOT be assigned to each other by implementing a unique symbol brand.
- **Test Case:**
```typescript
let cust: CustomerEntity = new VendorEntity('1', 'Corp'); // Must fail compilation!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
declare const CustomerBrand: unique symbol;
declare const VendorBrand: unique symbol;

class CustomerEntity {
  readonly [CustomerBrand] = true;
  constructor(public id: string, public name: string) {}
}

class VendorEntity {
  readonly [VendorBrand] = true;
  constructor(public id: string, public name: string) {}
}
```
</details>
