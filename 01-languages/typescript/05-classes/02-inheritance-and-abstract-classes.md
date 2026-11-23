# Inheritance and Abstract Classes

> Class inheritance models hierarchical object relationships via prototype delegation, while abstract classes define incomplete contracts mandating subclass implementation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-classes.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/00-classes.md), [01-access-modifiers.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/01-access-modifiers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 4.3 (`override` keyword) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitOverride: true` |

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
- **Problem Statement:** In large software architectures, related domain entities share core logic (e.g. lifecycle methods, event dispatching, database connectivity) while requiring specialized business rules for specific subtypes. Duplicating shared logic across every entity creates code bloat and synchronization bugs.
- **Historical Context:** TypeScript supported class inheritance (`extends`) from its inception. TypeScript 1.6 introduced `abstract class` and `abstract` methods. In TypeScript 4.3, the `override` keyword and `noImplicitOverride` compiler flag were added to prevent accidental method shadowing and broken overrides when base classes change.
- **Real-World Relevance:** Abstract classes provide template method patterns in backend frameworks, ORM base entities (e.g. `BaseEntity` in TypeORM), and extensible SDK plugins, ensuring that subclasses fulfill contract obligations at build time.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Prototype Delegation: `class Derived extends Base` wires `Derived.prototype.__proto__` to `Base.prototype`, and `Derived.__proto__` to `Base` (static inheritance).
  - `super()` Invariant: A derived class constructor MUST call `super()` before reading `this`.
  - Abstract Classes: Declared with `abstract class`. Cannot be directly instantiated via `new`.
  - Abstract Members: Declared with `abstract`. Have no implementation in the abstract class; every concrete subclass MUST provide a concrete implementation.
  - `override` Keyword (TS 4.3+): Marks a method as explicitly overriding a member defined in the base class. Under `noImplicitOverride: true`, omitting `override` on a method that shadows a base member is a compile error.
- **Mental Model:** An abstract class is an incomplete jigsaw puzzle. It provides the framework and shared pieces, but leaves specific empty slots that derived classes must fill before the puzzle can be assembled.
- **Soundness vs Ergonomics:** Unlike interfaces which are completely erased during compilation, abstract classes emit real JavaScript constructor functions and prototypes.

---

## 3. How It Works Internally
- **Prototype Linking:** In emitted JavaScript, TypeScript uses `Object.setPrototypeOf(Derived.prototype, Base.prototype)` and `Object.setPrototypeOf(Derived, Base)`.
- **Abstract Method Checking:** When compiling a concrete class that extends an abstract class, `checkClassDeclaration` walks the inheritance chain. For every symbol flagged with `SymbolFlags.Abstract`, it checks if the derived class provides a non-abstract member. If any abstract member is missing, it emits `TS2515`.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Prototype Chain & Abstract Class Inheritance            |
|                                                                          |
|      abstract class Shape                                                |
|      ├── area(): number (abstract)                                       |
|      └── print(): void  (concrete template method)                       |
|               ▲                                                          |
|               │ extends                                                  |
|      class Circle extends Shape                                          |
|      ├── radius: number                                                  |
|      └── area(): number (concrete implementation: Math.PI * r^2)         |
|                                                                          |
|   Runtime Prototype Chain:                                               |
|   circle ──► Circle.prototype ──► Shape.prototype ──► Object.prototype   |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Abstract Class & Method Syntax:**
  ```typescript
  abstract class BaseHttpService {
    constructor(protected baseUrl: string) {}

    abstract handleRequest(path: string): Promise<Response>;

    public logRequest(path: string): void {
      console.log(`Request to ${this.baseUrl}${path}`);
    }
  }
  ```
- **Override Keyword (TS 4.3+):**
  ```typescript
  class ApiService extends BaseHttpService {
    override async handleRequest(path: string): Promise<Response> {
      this.logRequest(path);
      return fetch(`${this.baseUrl}${path}`);
    }
  }
  ```
- **tsconfig Dependencies:**
  - `noImplicitOverride: true`: Enforces that any overridden method must be tagged with the `override` keyword.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Declare an abstract base class with a template method and implement a concrete subclass.
- **Code:**
```typescript
abstract class ReportGenerator {
  public generate(): string {
    const header = this.buildHeader();
    const body = this.buildBody();
    const footer = this.buildFooter();
    return `${header}
${body}
${footer}`;
  }

  protected buildHeader(): string {
    return '=== FINANCIAL REPORT ===';
  }

  // Abstract: Subclass MUST provide body formatting
  protected abstract buildBody(): string;

  protected buildFooter(): string {
    return '=== END OF REPORT ===';
  }
}

class CsvReportGenerator extends ReportGenerator {
  override buildBody(): string {
    return 'Item,Price,Quantity
Widget,$10.00,5';
  }
}

const report = new CsvReportGenerator();
console.log(report.generate());
```
- **Output / Type Evaluation:**
```typescript
// Full template method output combining concrete base methods and subclass implementation.
```
- **Explanation:** The abstract base class controls overall execution flow, while the subclass provides domain-specific formatting.

### Example 2: Real-World Enterprise Pattern (Database Entity Lifecycle Hook)
- **Goal:** Model persistent database entities with automatic audit timestamps.
- **Code:**
```typescript
export abstract class BaseEntity {
  public readonly id: string;
  public readonly createdAt: Date;
  public updatedAt: Date;

  constructor(id: string) {
    this.id = id;
    this.createdAt = new Date();
    this.updatedAt = new Date();
  }

  public touch(): void {
    this.updatedAt = new Date();
  }

  abstract validate(): void;
}

export class OrderEntity extends BaseEntity {
  constructor(id: string, public totalCents: number) {
    super(id);
  }

  override validate(): void {
    if (this.totalCents < 0) {
      throw new Error('Order total cannot be negative');
    }
  }
}

const order = new OrderEntity('ord_99', 4999);
order.validate(); // OK
order.touch();
```
- **Output / Type Evaluation:**
```typescript
// OrderEntity inherits BaseEntity audit fields while fulfilling validate contract.
```
- **Explanation:** Concrete subclasses inherit shared audit properties while being forced to implement entity-specific validation logic.

### Example 3: Edge Case / Boundary Condition (Constructor Execution Order)
- **Goal:** Demonstrate field initialization order between base and derived classes.
- **Code:**
```typescript
class Parent {
  name = 'Parent';

  constructor() {
    console.log('1. Parent constructor runs. Name:', this.name);
    this.printName(); // Calls overridden method!
  }

  printName(): void {
    console.log('2. Parent printName:', this.name);
  }
}

class Child extends Parent {
  override name = 'Child';

  constructor() {
    super();
    console.log('4. Child constructor runs. Name:', this.name);
  }

  override printName(): void {
    console.log('3. Child printName:', this.name);
  }
}

new Child();
```
- **Output / Type Evaluation:**
```typescript
// 1. Parent constructor runs. Name: Parent
// 3. Child printName: undefined  (Hazard! Child field initializers have not run yet!)
// 4. Child constructor runs. Name: Child
```
- **Explanation:** In JavaScript, calling virtual methods inside a parent constructor runs the child's method *before* the child's property initializers have executed, leading to `undefined` property bugs.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection when abstract methods are not implemented, or when `override` is missing.
- **Code:**
```typescript
abstract class MessageSender {
  abstract send(text: string): Promise<void>;
}

// @ts-expect-error TS2515: Non-abstract class 'EmailSender' does not implement inherited abstract member 'send' from class 'MessageSender'.
class EmailSender extends MessageSender {}

class SmsSender extends MessageSender {
  // @ts-expect-error TS4114: This member must have an 'override' modifier because it overrides a member in the base class.
  async send(text: string): Promise<void> {
    console.log('SMS sent:', text);
  }
}
```
- **Real Compiler Error:**
```
error TS2515: Non-abstract class 'EmailSender' does not implement inherited abstract member 'send'.
error TS4114: This member must have an 'override' modifier because it overrides a member in the base class.
```
- **Explanation:** Concrete classes must implement all abstract members, and `noImplicitOverride` forces explicit annotation.

### Example 5: Idiomatic Modern Usage (Abstract Class with Polymorphic This)
- **Goal:** Build an extensible SQL criteria builder using abstract classes and polymorphic `this`.
- **Code:**
```typescript
export abstract class QueryCriteria {
  protected conditions: string[] = [];

  abstract getTargetTable(): string;

  where(clause: string): this {
    this.conditions.push(clause);
    return this;
  }

  build(): string {
    const table = this.getTargetTable();
    const whereClause = this.conditions.length > 0 ? ` WHERE ${this.conditions.join(' AND ')}` : '';
    return `SELECT * FROM ${table}${whereClause}`;
  }
}

export class UserCriteria extends QueryCriteria {
  override getTargetTable(): string {
    return 'users';
  }

  activeOnly(): this {
    return this.where('is_active = true');
  }
}

const sql = new UserCriteria().activeOnly().where('age >= 18').build();
console.log(sql); // "SELECT * FROM users WHERE is_active = true AND age >= 18"
```
- **Output / Type Evaluation:**
```typescript
// Fully fluent chaining across base and derived methods.
```
- **Explanation:** Polymorphic `this` allows chaining methods declared in the abstract base class with specialized methods in the derived subclass.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Calling Virtual Methods in Constructors:** Calling an abstract or overridden method inside a base class constructor is an anti-pattern. The derived class's property initializers have not executed yet, causing runtime property reads to return `undefined`.
2. **Abstract Classes Cannot Be Instantiated Directly:** Attempting `new AbstractClass()` produces `TS2511: Cannot create an instance of an abstract class`.
3. **`noImplicitOverride` Prevents Accidental Shadowing:** If a base class adds a method named `render()` that coincides with an existing subclass method, `noImplicitOverride` immediately alerts the developer to the naming conflict.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Instantiating an abstract class directly | `TS2511: Cannot create instance of abstract class` | Subclass or use factory function | Respects abstract class semantics |
| Calling abstract method in base constructor | Accesses uninitialized child fields (`undefined`) | Initialize in subclass or factory | Eliminates constructor initialization race conditions |
| Missing `override` under `noImplicitOverride` | `TS4114: Must have an 'override' modifier` | Annotate with `override` keyword | Documents intent and catches renamed base methods |
| Deep inheritance trees (5+ levels) | Fragile base class problem; tight coupling | Favor composition over inheritance | Simplifies system maintenance |
| Missing `super()` call in derived constructor | `TS17009: 'super' must be called before this` | Call `super()` first in constructor | Aligns with ECMAScript constructor requirements |
| Forgetting to implement abstract method | `TS2515: Does not implement inherited member` | Provide concrete implementation | Fulfills base class contract |

---

## 8. Performance & Type-Checker Complexity
- **Prototype Chain Lookup:** Looking up inherited methods requires the JS engine to traverse the prototype chain. Modern V8 engines optimize this via inline caches (ICs), achieving direct-call speeds for monomorphic call sites.
- **Symbol Inheritance Traversal:** In `checker.ts`, checking a derived class traverses all base class members recursively, checking visibility and override compatibility.
- **Isolated Declarations:** Exported derived classes must have explicit types on all public members under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** `ts.ClassDeclaration` with `SyntaxKind.AbstractKeyword`, `ts.HeritageClause` with `SyntaxKind.ExtendsKeyword`.
- **Modifier Checking:** `hasSyntacticModifier(node, ModifierFlags.Override)`.
- **Diagnostic Generation:** Flags `Diagnostics.Non_abstract_class_0_does_not_implement_inherited_abstract_member_1_from_class_2` (TS2515).

---

## 10. Best Practices & Design Guidelines
- **Enable `noImplicitOverride: true`:** Always enable `noImplicitOverride` in `tsconfig.json` to guard against broken overrides.
- **Limit Inheritance Depth to 2–3 Levels:** Prefer composition and interface implementation over deep class hierarchies.
- **Never Call Overridden Methods from Base Constructors:** Prevent uninitialized child property access bugs.

---

## 11. Real-World Case Study
- **Scenario:** A core banking SDK maintained an abstract `PaymentGateway` class. The base class author refactored the signature of `processTransaction(req: Request)` to `processPayment(req: Request)`. Because subclasses did not use `override`, the compiler emitted no errors on the subclasses' existing `processTransaction()` methods. However, the concrete payment gateways were now missing the newly named `processPayment()` method, causing runtime crashes across 5 payment adapters during Friday deployment.
- **Implementation:** Enabled `noImplicitOverride: true` and tagged all subclass methods with `override`:
  ```typescript
  // In tsconfig.json:
  // "noImplicitOverride": true

  class StripeGateway extends PaymentGateway {
    // If processPayment is renamed in base class, this line immediately fails compilation!
    override async processPayment(req: PaymentRequest): Promise<PaymentResponse> {
      return this.sendToStripe(req);
    }
  }
  ```
- **Outcome:** Subclass overrides are now strictly bound to base class member names; any base class refactor immediately breaks the build with a descriptive error.

---

## 12. Interview Questions

### Q1: What is the exact difference between an abstract class and an interface in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

1. **Runtime Existence:** Interfaces are purely compile-time constructs that are completely erased during JavaScript emit. Abstract classes emit real JavaScript constructor functions and prototypes.
2. **Implementation:** Interfaces cannot contain any implementation code, initializers, or method bodies. Abstract classes can contain fully implemented methods, properties, and constructors alongside abstract declarations.
3. **Inheritance:** A class can only extend one abstract class (single inheritance), but can implement multiple interfaces (multiple interface implementation).
4. **`instanceof`:** You can check `obj instanceof AbstractClass` at runtime; you cannot use `instanceof` with interfaces.
</details>

### Q2: What problem does the `override` keyword solve in TypeScript 4.3+?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The `override` keyword prevents two major classes of bugs:
1. **Accidental Shadowing:** If a developer adds a method to a subclass with a common name (like `init()`), and the base class later adds a method with the same name, the subclass might accidentally override the base method without realizing it.
2. **Orphaned Overrides:** If a base class method is renamed or deleted during a refactor, an existing subclass method intended to override it silently becomes an uncalled standalone method. With `noImplicitOverride`, TypeScript throws an error if a method has `override` but does not match any base class member, and throws an error if a method overrides a base member without the `override` tag.
</details>

### Q3: Why is calling an abstract method inside a base class constructor dangerous?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, base class constructors execute *before* derived class constructors and before derived class field initializers run. If a base class constructor calls an abstract method implemented by a child class, that child method will execute while the child class's instance properties are still uninitialized (`undefined`). If the child method attempts to read any of its own fields, it will read `undefined` or cause runtime `TypeError` exceptions.
</details>

### Q4: Can an abstract class extend another abstract class?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. An abstract class can extend another abstract class. The derived abstract class can choose to implement some of the base abstract members, leave them abstract, or declare additional abstract members of its own.
</details>

### Q5: What is static inheritance in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In ECMAScript 2015+ and TypeScript, classes inherit not only instance methods through the prototype chain, but also static methods and static properties through the constructor prototype chain (`Derived.__proto__ === Base`). Calling `Derived.staticMethod()` will find and execute `Base.staticMethod()` if not overridden by the derived class.
</details>

### Q6: Can a class have an abstract constructor?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. A constructor cannot be marked with the `abstract` keyword in a class declaration. However, in type space, you can define an abstract construct signature: `type AbstractCtor<T> = abstract new (...args: any[]) => T;`.
</details>

### Q7: What does the `super` keyword do when used outside a constructor?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When used in a derived class method, `super.methodName()` invokes the base class implementation of that method on the current instance (`this`). It allows a subclass to augment or wrap base class behavior rather than replacing it entirely.
</details>

### Q8: What compiler error occurs if a concrete class extends an abstract class but fails to implement an abstract method?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The compiler emits `TS2515: Non-abstract class 'Derived' does not implement inherited abstract member 'methodName' from class 'Base'`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Abstract Shape with Area (Easy)
- **Problem Statement:** Author an `abstract class Shape` with `abstract area(): number`, and implement concrete `Circle` and `Rectangle` subclasses.
- **Test Case:**
```typescript
const c = new Circle(10);
console.log(c.area()); // 314.159...
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
abstract class Shape {
  abstract area(): number;
}

class Circle extends Shape {
  constructor(public radius: number) {
    super();
  }

  override area(): number {
    return Math.PI * this.radius * this.radius;
  }
}

class Rectangle extends Shape {
  constructor(public width: number, public height: number) {
    super();
  }

  override area(): number {
    return this.width * this.height;
  }
}
```
</details>

### Exercise 2: Database Migration Runner (Easy)
- **Problem Statement:** Create an `abstract class Migration` with abstract `up(): Promise<void>` and `down(): Promise<void>`, implementing a concrete migration.
- **Test Case:**
```typescript
const m = new AddUserTableMigration();
await m.up();
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
abstract class Migration {
  constructor(public readonly version: number) {}
  abstract up(): Promise<void>;
  abstract down(): Promise<void>;
}

class AddUserTableMigration extends Migration {
  constructor() {
    super(1);
  }

  override async up(): Promise<void> {
    console.log('CREATE TABLE users');
  }

  override async down(): Promise<void> {
    console.log('DROP TABLE users');
  }
}
```
</details>

### Exercise 3: Template Method Cache Handler (Medium)
- **Problem Statement:** Implement a `CacheHandler` abstract class with a concrete template method `getOrFetch(key, fetcher)` that delegates to abstract `readCache(key)` and `writeCache(key, val)`.
- **Test Case:**
```typescript
const cache = new MemoryCache();
const val = await cache.getOrFetch('user_1', async () => ({ name: 'Alice' }));
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
abstract class CacheHandler {
  async getOrFetch<T>(key: string, fetcher: () => Promise<T>): Promise<T> {
    const cached = await this.readCache(key);
    if (cached !== null) {
      return JSON.parse(cached) as T;
    }
    const fresh = await fetcher();
    await this.writeCache(key, JSON.stringify(fresh));
    return fresh;
  }

  protected abstract readCache(key: string): Promise<string | null>;
  protected abstract writeCache(key: string, value: string): Promise<void>;
}

class MemoryCache extends CacheHandler {
  private store = new Map<string, string>();

  protected override async readCache(key: string): Promise<string | null> {
    return this.store.get(key) ?? null;
  }

  protected override async writeCache(key: string, value: string): Promise<void> {
    this.store.set(key, value);
  }
}
```
</details>

### Exercise 4: Enforce Override Compliance (Medium)
- **Problem Statement:** Write a base class `Notification` and derived `PushNotification` where `send()` is overridden using the `override` keyword, verifying compliance with `noImplicitOverride`.
- **Test Case:**
```typescript
const pn = new PushNotification();
pn.send('Hello');
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Notification {
  send(message: string): void {
    console.log(`Base notification: ${message}`);
  }
}

class PushNotification extends Notification {
  override send(message: string): void {
    console.log(`Push notification: ${message}`);
  }
}
```
</details>

### Exercise 5: Compile-Time Abstract Class Detector (Hard)
- **Problem Statement:** Author a utility type `IsAbstractClass<T>` that evaluates to `true` if `T` is an abstract constructor and `false` if it is concrete.
- **Test Case:**
```typescript
abstract class Abs {}
class Con {}
type T1 = IsAbstractClass<typeof Abs>; // true
type T2 = IsAbstractClass<typeof Con>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type IsAbstractClass<T> =
  T extends abstract new (...args: any[]) => any
    ? T extends new (...args: any[]) => any
      ? false
      : true
    : false;
```
</details>

### Exercise 6: Polymorphic Chainable AST Builder (Hard)
- **Problem Statement:** Build an abstract query expression builder where subclasses (`SelectQuery`, `DeleteQuery`) inherit chainable base methods (`where`, `limit`) returning the exact subclass type via polymorphic `this`.
- **Test Case:**
```typescript
const q = new SelectQuery().from('orders').where('active = true').limit(10);
// q is SelectQuery!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
abstract class BaseQuery {
  protected clauses: string[] = [];
  protected limitCount?: number;

  abstract getQueryType(): 'SELECT' | 'DELETE';

  where(clause: string): this {
    this.clauses.push(clause);
    return this;
  }

  limit(count: number): this {
    this.limitCount = count;
    return this;
  }
}

class SelectQuery extends BaseQuery {
  private tableName = '';

  override getQueryType(): 'SELECT' {
    return 'SELECT';
  }

  from(table: string): this {
    this.tableName = table;
    return this;
  }
}
```
</details>
