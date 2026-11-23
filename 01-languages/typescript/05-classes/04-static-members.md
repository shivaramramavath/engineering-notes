# Static Members and Static Blocks

> Static properties, static methods, and ECMAScript static initialization blocks live on class constructor functions, providing state factories and initialization hooks.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-classes.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/00-classes.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 4.4 (Static Blocks) / TS 7.0 Native |
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
- **Problem Statement:** Classes frequently require state or helper functions that belong to the collection or type as a whole, rather than to any single instance (e.g. factory creators, configuration constants, connection pools). Prior to static blocks, initializing complex static state required running code outside the class body, exposing uninitialized state.
- **Historical Context:** TypeScript 0.9 supported `static` properties and methods. In TypeScript 4.4 (following ECMAScript 2022), static initialization blocks (`static { ... }`) were introduced.
- **Real-World Relevance:** Static members power Factory patterns (`User.create()`), Singleton instances (`Database.getInstance()`), and secure access bridges where private instance state is initialized through static blocks.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Value-Space Binding: Static members are attached directly to the constructor function (`MyClass.staticProp`), NOT to the prototype (`MyClass.prototype`).
  - Static Inheritance: Derived classes inherit static members through constructor prototype delegation (`Derived.__proto__ === Base`).
  - Access Modifiers on Static Members: Static members support `public`, `protected`, `private`, and `readonly`, as well as `#private` static fields (`static #secret`).
  - Static Initialization Blocks (`static {}`): Executed once when the class declaration is evaluated at runtime. Can contain statements, loops, and try/catch blocks, and have privileged access to private instance fields.
  - No Clashing with Function Built-ins: A class cannot declare static properties with names that clash with built-in properties of `Function.prototype` (e.g. `name`, `length`, `caller`, `bind`, `call`).
- **Mental Model:** Instance members describe an individual car; static members describe the factory that manufactures the cars.
- **Soundness vs Ergonomics:** Static blocks run in declaration order, guaranteeing that static configuration is fully locked before any instance can be instantiated.

---

## 3. How It Works Internally
- **Symbol Allocation:** Static members are stored in the `exports` table of the class's constructor symbol.
- **Evaluation Order:**
  1. Base class static fields and static blocks execute in declaration order.
  2. Derived class static fields and static blocks execute in declaration order.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Static Member Architecture in Memory                    |
|                                                                          |
|   class DatabaseConnection {                                             |
|     static defaultPort = 5432;                                           |
|     instanceId = Math.random();                                          |
|   }                                                                      |
|                                                                          |
|   Constructor Function (Value Space)      Prototype Object               |
|   ┌───────────────────────────────┐       ┌───────────────────────────┐  |
|   │ DatabaseConnection            │       │ DatabaseConnection.proto  │  |
|   │ ├── defaultPort = 5432        │       │ └── (instance methods)    │  |
|   │ └── prototype ────────────────┼──────►│                           │  |
|   └───────────────────────────────┘       └───────────────────────────┘  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Static Members:**
  ```typescript
  class MathUtils {
    public static readonly PI = 3.14159265359;
    public static square(n: number): number {
      return n * n;
    }
  }
  ```
- **Static Initialization Block (TS 4.4+):**
  ```typescript
  class Configuration {
    static #loaded: boolean = false;
    static apiEndpoint: string;

    static {
      try {
        Configuration.apiEndpoint = process.env.API_URL ?? 'https://api.local';
        Configuration.#loaded = true;
      } catch {
        Configuration.apiEndpoint = 'https://fallback.local';
      }
    }
  }
  ```
- **tsconfig Dependencies:**
  - Requires `target: "ES2022"`+ for native static blocks, or downlevels with TS 4.4+.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Declare static utility methods and constants.
- **Code:**
```typescript
export class UuidUtils {
  public static readonly VERSION = 'v4';

  public static generate(): string {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
      const r = (Math.random() * 16) | 0;
      const v = c === 'x' ? r : (r & 0x3) | 0x8;
      return v.toString(16);
    });
  }

  public static isValid(uuid: string): boolean {
    return /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(uuid);
  }
}

const id = UuidUtils.generate();
console.log(id, UuidUtils.isValid(id)); // true
```
- **Output / Type Evaluation:**
```typescript
// Static members called on UuidUtils constructor without instantiation.
```
- **Explanation:** Utility classes group related stateless helper functions into a clean namespace.

### Example 2: Real-World Enterprise Pattern (Singleton Connection Pool)
- **Goal:** Implement a thread-safe singleton connection manager.
- **Code:**
```typescript
export class DatabasePool {
  private static instance: DatabasePool | null = null;
  public readonly connectionString: string;

  private constructor(connStr: string) {
    this.connectionString = connStr;
    console.log('Database pool allocated.');
  }

  public static getInstance(): DatabasePool {
    if (DatabasePool.instance === null) {
      DatabasePool.instance = new DatabasePool('postgres://localhost:5432/main');
    }
    return DatabasePool.instance;
  }
}

const pool1 = DatabasePool.getInstance();
const pool2 = DatabasePool.getInstance();
console.log(pool1 === pool2); // true (singleton reference identity)
```
- **Output / Type Evaluation:**
```typescript
// Exactly one instance instantiated across application runtime.
```
- **Explanation:** A private constructor combined with a static factory method guarantees single-instance allocation.

### Example 3: Edge Case / Boundary Condition (Static Initialization Block with Private Access)
- **Goal:** Access private instance properties from an external friend function using static blocks.
- **Code:**
```typescript
let extractSecret: (obj: SecureContainer) => string;

export class SecureContainer {
  #secret: string;

  constructor(secret: string) {
    this.#secret = secret;
  }

  static {
    // Static blocks have privileged access to #private instance fields!
    extractSecret = (obj: SecureContainer) => obj.#secret;
  }
}

const container = new SecureContainer('MY_ENCRYPTED_DATA');
// Allowed through privileged bridge:
console.log(extractSecret!(container)); // "MY_ENCRYPTED_DATA"
```
- **Output / Type Evaluation:**
```typescript
// Static blocks can bridge private members to module-level friend functions.
```
- **Explanation:** ECMAScript static blocks possess lexical access to private instance fields declared within the class body.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate error when attempting to declare forbidden static property names.
- **Code:**
```typescript
class BadClass {
  // @ts-expect-error TS2699: Static property 'name' conflicts with built-in property 'Function.name' of constructor function.
  static name = 'CustomName';

  // @ts-expect-error TS2699: Static property 'length' conflicts with built-in property 'Function.length'.
  static length = 10;
}
```
- **Real Compiler Error:**
```
error TS2699: Static property 'name' conflicts with built-in property 'Function.name' of constructor function 'BadClass'.
error TS2699: Static property 'length' conflicts with built-in property 'Function.length' of constructor function 'BadClass'.
```
- **Explanation:** Built-in properties of `Function.prototype` cannot be shadowed by static class fields.

### Example 5: Idiomatic Modern Usage (Static Factory Pattern with Domain Validation)
- **Goal:** Enforce domain validation rules before constructing domain value objects.
- **Code:**
```typescript
export class EmailAddress {
  public readonly value: string;

  private constructor(email: string) {
    this.value = email;
  }

  public static create(raw: string): { ok: true; email: EmailAddress } | { ok: false; error: string } {
    const trimmed = raw.trim().toLowerCase();
    if (!trimmed.includes('@') || !trimmed.includes('.')) {
      return { ok: false, error: 'Invalid email syntax' };
    }
    return { ok: true, email: new EmailAddress(trimmed) };
  }
}

const res = EmailAddress.create('  ALICE@CORP.COM ');
if (res.ok) {
  console.log('Sanitized email:', res.email.value); // "alice@corp.com"
}
```
- **Output / Type Evaluation:**
```typescript
// Safe creation with zero unvalidated domain objects.
```
- **Explanation:** Static factories replace throwing constructors with type-safe Result return types.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`this` in Static Methods:** Inside a static method, `this` refers to the **constructor function**, NOT an instance of the class. Invoking `this.instanceMethod()` inside a static method throws a runtime `TypeError`.
2. **Inherited Static State is Shared:** If class `Base` has `static count = 0`, and `Derived extends Base`, reading `Derived.count` accesses `Base.count` unless `Derived` assigns its own static property. Modifying `Base.count` changes what `Derived.count` reads!
3. **Static Blocks Cannot Contain `return`:** A static initialization block cannot execute `return`; it is a block statement, not a function.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Naming static property `name` or `length` | `TS2699: Conflicts with built-in property` | Use `className` or `size` | Prevents corrupting Function.prototype |
| Accessing instance fields via `this` in static | `TS2339: Property does not exist on constructor` | Pass instance as argument | `this` in static refers to constructor |
| Initializing static state outside class body | State uninitialized if class referenced early | Use `static {}` blocks | Guarantees atomic static initialization |
| Mutating shared static properties across subclasses | Corrupts global state across sibling types | Declare static properties explicitly per class | Prevents cross-subclass static pollution |
| Accessing static members from instance (`this.PI`) | `TS2576: Property does not exist on instance` | Access via `ClassName.PI` | Enforces clean value-space constructor access |
| Calling `super()` in static block | Syntax Error | Use `super.staticMethod()` | Static blocks have no constructor call |

---

## 8. Performance & Type-Checker Complexity
- **Constructor Prototype Delegation:** In modern V8 engines, static inheritance links constructors via `Object.setPrototypeOf(Derived, Base)`. Method lookups are cached monomorphically.
- **Static Block Execution Cost:** Static blocks run once upon module evaluation, adding negligible one-time startup latency.
- **Isolated Declarations:** Exported static properties must have explicit type annotations under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:**
  - `ts.PropertyDeclaration` or `ts.MethodDeclaration` with `SyntaxKind.StaticKeyword`.
  - `ts.ClassStaticBlockDeclaration` (`SyntaxKind.ClassStaticBlockDeclaration`).
- **Symbol Storage:** Stored in `symbol.exports` on the class symbol.
- **Forbidden Name Check:** In `checkClassDeclaration`, the compiler flags any static member whose name matches `name`, `length`, `prototype`, `caller`, or `arguments`.

---

## 10. Best Practices & Design Guidelines
- **Use Static Factories for Complex Instantiation:** Replace error-prone constructors with descriptive static methods (`Order.fromDto()`, `Order.empty()`).
- **Use Static Blocks for Complex Configuration:** Encapsulate environment loading and resource pre-allocation cleanly inside the class.
- **Keep Static State Immutable:** Prefer `static readonly` to avoid shared mutable state concurrency bugs.

---

## 11. Real-World Case Study
- **Scenario:** A microservice initialized an encryption cipher class. A junior developer ran initialization code outside the class at the bottom of the file: `Cipher.initializeKey(process.env.KEY)`. When another module imported `Cipher` and called `Cipher.encrypt()` in top-level code, the external initialization line had not executed yet, causing runtime `NullPointerException` on undefined cipher keys.
- **Implementation:** Moved the key initialization into a static block:
  ```typescript
  export class CipherService {
    static #cipherKey: Buffer;

    static {
      const raw = process.env.ENCRYPTION_KEY;
      if (!raw) throw new Error('Missing ENCRYPTION_KEY environment variable');
      CipherService.#cipherKey = Buffer.from(raw, 'hex');
    }

    public static encrypt(data: string): string { ... }
  }
  ```
- **Outcome:** The static block guarantees that the cipher key is atomically loaded and validated the exact millisecond the class definition is parsed, completely eliminating uninitialized startup crashes.

---

## 12. Interview Questions

### Q1: What does `this` refer to inside a static method of a class?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Inside a static method, `this` refers to the **constructor function** of the class itself, NOT an instance of the class. If the class is `MyClass`, `this === MyClass`. If a subclass `SubClass` inherits the static method and calls it via `SubClass.method()`, `this` inside the method refers to `SubClass`. Instance fields and instance methods cannot be accessed via `this` inside static methods.
</details>

### Q2: How does static inheritance work in JavaScript and TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In ES2015+ and TypeScript, classes have a dual prototype chain:
1. The **instance prototype chain**: `Derived.prototype.__proto__ === Base.prototype`, allowing instances of `Derived` to inherit instance methods.
2. The **constructor prototype chain**: `Derived.__proto__ === Base`, allowing the derived constructor function to inherit static methods and properties from the base constructor function.
</details>

### Q3: What is a static initialization block (`static {}`) and when does it execute?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A static initialization block is a language feature introduced in ECMAScript 2022 and TypeScript 4.4. It allows arbitrary statements (loops, try/catch, variable declarations) to run inside the class definition scope. It executes exactly once when the class declaration is evaluated (at module evaluation time). It has privileged access to `#private` fields and methods of the class.
</details>

### Q4: Why can a class not declare `static name: string` in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, all function objects (including class constructor functions) have a built-in, configurable property named `name` inherited from `Function.prototype.name`, which holds the name of the function or class. Declaring `static name` clashes with this built-in property and causes compiler error `TS2699: Static property 'name' conflicts with built-in property 'Function.name'`.
</details>

### Q5: How can a static block be used to implement the "friend function" pattern from C++?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Because static blocks have privileged access to `#private` instance fields and methods within the class body, a static block can assign a closure to an external module-level variable:
```typescript
let privateReader: (inst: SecretBox) => string;
class SecretBox {
  #secret = '42';
  static {
    privateReader = (inst) => inst.#secret;
  }
}
```
This grants functions within the same file access to private state while keeping it completely sealed from outside importers.
</details>

### Q6: What is the difference between a static property and a module-level variable?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A module-level variable is scoped to the module file. A static property is attached to the class constructor function, allowing it to:
1. Be exported and referenced via `ClassName.prop` across different files.
2. Be inherited and overridden by derived subclasses.
3. Have visibility controlled by access modifiers (`private`, `protected`, `public`).
</details>

### Q7: Can a static method be marked as `abstract` in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. TypeScript does not permit `abstract static` methods. The `abstract` keyword only applies to instance members. Because static members reside on the constructor object and are resolved dynamically, mandating static overrides across subclasses is not supported by the type system.
</details>

### Q8: What happens if an error is thrown inside a static initialization block?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

If an uncaught exception is thrown inside a `static {}` block, module evaluation fails immediately with an unhandled error, and the class is not defined. This makes static blocks ideal for validating critical environment variables and invariants at startup.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Static Math Helper (Easy)
- **Problem Statement:** Author a class `Geometry` with static `circleArea(radius: number): number` and static `hypotenuse(a: number, b: number): number`.
- **Test Case:**
```typescript
Geometry.circleArea(10); // 314.159...
Geometry.hypotenuse(3, 4); // 5
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Geometry {
  static circleArea(radius: number): number {
    return Math.PI * radius * radius;
  }

  static hypotenuse(a: number, b: number): number {
    return Math.sqrt(a * a + b * b);
  }
}
```
</details>

### Exercise 2: Static Counter Tracker (Easy)
- **Problem Statement:** Implement a class `Ticket` that automatically increments a static counter `#nextId` on each instantiation, assigning an incremental ID.
- **Test Case:**
```typescript
const t1 = new Ticket(); // id: 1
const t2 = new Ticket(); // id: 2
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Ticket {
  static #nextId: number = 1;
  public readonly id: number;

  constructor() {
    this.id = Ticket.#nextId++;
  }
}
```
</details>

### Exercise 3: Static Block Configuration Loader (Medium)
- **Problem Statement:** Create a class `AppConfig` that loads an environment port in a static initialization block with a fallback to 3000.
- **Test Case:**
```typescript
console.log(AppConfig.PORT); // 3000
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class AppConfig {
  static readonly PORT: number;

  static {
    const envPort = typeof process !== 'undefined' ? process.env?.PORT : undefined;
    AppConfig.PORT = envPort ? parseInt(envPort, 10) : 3000;
  }
}
```
</details>

### Exercise 4: Static Factory with Private Constructor (Medium)
- **Problem Statement:** Write a class `IpAddress` with private constructor and static `parse(raw: string): IpAddress | null` validating 4 numeric octets.
- **Test Case:**
```typescript
const ip = IpAddress.parse('192.168.1.1'); // IpAddress
const invalid = IpAddress.parse('999.1.1.1'); // null
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class IpAddress {
  private constructor(public readonly octets: readonly [number, number, number, number]) {}

  static parse(raw: string): IpAddress | null {
    const parts = raw.split('.').map((p) => parseInt(p, 10));
    if (parts.length !== 4 || parts.some((p) => isNaN(p) || p < 0 || p > 255)) {
      return null;
    }
    return new IpAddress(parts as [number, number, number, number]);
  }
}
```
</details>

### Exercise 5: Compile-Time Static Method Verifier (Hard)
- **Problem Statement:** Write a type-level check `HasStaticMethod<C, MethodName>` that verifies if class constructor `C` possesses a specific static method.
- **Test Case:**
```typescript
class Svc { static ping() {} }
type T1 = HasStaticMethod<typeof Svc, 'ping'>; // true
type T2 = HasStaticMethod<typeof Svc, 'pong'>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type HasStaticMethod<C, M extends string> =
  M extends keyof C
    ? C[M] extends (...args: any[]) => any
      ? true
      : false
    : false;
```
</details>

### Exercise 6: Privileged Friend Bridge via Static Block (Hard)
- **Problem Statement:** Build a class `AccountSession` with private token, using a static initialization block to export a privileged extractor function to a dedicated validation helper.
- **Test Case:**
```typescript
const session = new AccountSession('my_secret_token');
console.log(verifyToken(session)); // "my_secret_token"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
let getTokenBridge: (session: AccountSession) => string;

class AccountSession {
  #token: string;

  constructor(token: string) {
    this.#token = token;
  }

  static {
    getTokenBridge = (session) => session.#token;
  }
}

function verifyToken(session: AccountSession): string {
  return getTokenBridge(session);
}
```
</details>
