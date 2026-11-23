# Access Modifiers and Privacy

> Access modifiers—public, protected, and private—govern compile-time visibility, while ECMAScript private fields (`#private`) enforce hard runtime encapsulation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-classes.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/00-classes.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 (`public`, `private`) / TS 1.3 (`protected`) / TS 3.8 (`#private`) / TS 7.0 Native |
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
- **Problem Statement:** Software components need to hide internal state and implementation details from outside consumers. Exposing raw fields directly allows external code to mutate state unpredictably, violating invariants and preventing safe refactoring.
- **Historical Context:** TypeScript 0.9 provided `public` and `private` keywords, and TS 1.3 added `protected`. However, TypeScript's `private` was purely a compile-time restriction; at JavaScript runtime, the property was emitted as a normal public property, accessible via `obj["privateField"]`. ECMAScript 2020 introduced native private fields using the hash prefix (`#field`), which TypeScript adopted in version 3.8.
- **Real-World Relevance:** Choosing between TypeScript `private` (soft, compile-time privacy) and ECMAScript `#private` (hard, runtime-enforced privacy) is a critical security and architectural decision in modern libraries and frameworks.

---

## 2. Core Concepts
- **Formal Invariants:**
  - `public` (Default): Accessible from anywhere (internally, derived subclasses, external callers).
  - `protected`: Accessible only within the declaring class and its derived subclasses. Not accessible externally on instance references.
  - `private` (TypeScript Keyword): Accessible only within the declaring class body. Inaccessible in derived subclasses. Erased at runtime to a standard public property.
  - `#private` (ECMAScript Private Field): Enforced at runtime by the JavaScript engine using private names / WeakMaps. Completely inaccessible outside the class syntax body, even via bracket reflection (`obj["#field"]` throws syntax error).
  - Nominal Subtyping Effect: Two classes with identical `private` or `protected` fields are NOT structurally compatible unless they inherit from the exact same class declaration.
- **Mental Model:**
  - TypeScript `private`: "Please don't touch this; the compiler will yell if you do, but JS won't stop you."
  - ECMAScript `#private`: "Fort Knox. The JavaScript engine forbids access under all circumstances."
- **Soundness vs Ergonomics:** TypeScript `private` allows unit tests to bypass privacy using bracket access (`obj["field"]`), whereas `#private` prevents even test reflection without dedicated test helpers.

---

## 3. How It Works Internally
- **Emit for TypeScript `private`:** Emits standard property assignments (`this.secret = val`).
- **Emit for ECMAScript `#private`:**
  - Modern targets (`target: "ES2022"`+): Emits native `#secret` fields.
  - Older targets (`target: "ES2015"`): Emits a file-scoped `WeakMap` (`_secret = new WeakMap()`) to guarantee hard privacy in legacy engines.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  TypeScript private vs ECMAScript #private               |
|                                                                          |
|   Feature                  TypeScript private          ECMAScript #field |
|   ---------------------------------------------------------------------- |
|   Enforcement              Compile-time ONLY           Compile + RUNTIME |
|   Derived subclass access  NO                          NO                |
|   Bracket access obj["x"]  Permitted (bypasses check)  SYNTAX ERROR      |
|   Object.keys() visible    YES                         NO (Invisible)    |
|   JSON.stringify() emit    YES (Serialized)            NO (Ignored)      |
|   Runtime Mechanism        Standard property           Private Name Slot |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **TypeScript Access Modifiers:**
  ```typescript
  class Repository {
    public endpoint: string = 'https://api.example.com';
    protected retryCount: number = 3;
    private authToken: string = 'secret_key';
  }
  ```
- **ECMAScript Hard Private Syntax:**
  ```typescript
  class SecureVault {
    #encryptionKey: string;

    constructor(key: string) {
      this.#encryptionKey = key;
    }

    #computeHash(): string {
      return this.#encryptionKey.split('').reverse().join('');
    }
  }
  ```
- **tsconfig Dependencies:**
  - Requires `target: "ES2015"` or higher for `#private` fields.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Contrast `protected` and `private` across inheritance.
- **Code:**
```typescript
class BaseWorker {
  private id = 'worker_01';      // Only BaseWorker can access
  protected taskQueue: string[] = []; // Subclasses can access
  public isRunning = false;       // Anyone can access

  getId(): string {
    return this.id;
  }
}

class QueueWorker extends BaseWorker {
  processNext(): void {
    // Legal: subclasses can read protected fields!
    const task = this.taskQueue.shift();
    console.log('Running task:', task);

    // @ts-expect-error TS2341: Property 'id' is private and only accessible within class 'BaseWorker'.
    console.log(this.id);
  }
}

const worker = new QueueWorker();
// @ts-expect-error TS2445: Property 'taskQueue' is protected and only accessible within class 'BaseWorker' and its subclasses.
worker.taskQueue.push('job1');
```
- **Output / Type Evaluation:**
```typescript
// TS2341 prevents subclass from reading private field; TS2445 blocks external access to protected field.
```
- **Explanation:** `protected` establishes an inheritance contract, while `private` seals fields strictly to the declaring class.

### Example 2: Real-World Enterprise Pattern (Hard Encapsulation with #private)
- **Goal:** Protect critical cryptographic keys from leakage via serialization or memory inspection.
- **Code:**
```typescript
export class PaymentSigner {
  #privateKey: string;
  public readonly keyId: string;

  constructor(keyId: string, privateKey: string) {
    this.keyId = keyId;
    this.#privateKey = privateKey;
  }

  signPayload(payload: string): string {
    return `SIG(${payload}):${this.#privateKey.slice(0, 4)}...`;
  }
}

const signer = new PaymentSigner('key_prod_01', 'SUPER_SECRET_PAYMENT_KEY');

// JSON.stringify ignores #private fields completely!
console.log(JSON.stringify(signer)); // '{"keyId":"key_prod_01"}'

// Reflection cannot read #private:
// @ts-expect-error TS18013: Property '#privateKey' is not accessible outside class 'PaymentSigner' because it has a private identifier.
console.log(signer.#privateKey);
```
- **Output / Type Evaluation:**
```typescript
// JSON output: {"keyId":"key_prod_01"}
```
- **Explanation:** `#private` fields are completely omitted from `JSON.stringify()` and cannot be enumerated by `Object.keys()`.

### Example 3: Edge Case / Boundary Condition (Subtyping with Private Fields)
- **Goal:** Demonstrate how private fields break structural compatibility between unrelated classes.
- **Code:**
```typescript
class FileLogger {
  private logPath = '/var/log/app.log';
  write(msg: string): void { console.log(msg); }
}

class ConsoleLogger {
  private logPath = '/var/log/app.log';
  write(msg: string): void { console.log(msg); }
}

let logger: FileLogger = new FileLogger();

// @ts-expect-error TS2322: Type 'ConsoleLogger' is not assignable to type 'FileLogger'.
// Types have separate declarations of a private property 'logPath'.
logger = new ConsoleLogger();
```
- **Output / Type Evaluation:**
```typescript
// TS2322 enforces nominal identity because logPath is private!
```
- **Explanation:** In TypeScript, private and protected fields enforce nominal typing: instances are only assignable if the private fields originate from the exact same class declaration.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate error when attempting to override private field in subclass.
- **Code:**
```typescript
class ParentService {
  private status = 'INITIAL';
}

class ChildService extends ParentService {
  // @ts-expect-error TS2415: Class 'ChildService' incorrectly extends base class 'ParentService'.
  // Types have separate declarations of a private property 'status'.
  private status = 'RUNNING';
}
```
- **Real Compiler Error:**
```
error TS2415: Class 'ChildService' incorrectly extends base class 'ParentService'.
  Types have separate declarations of a private property 'status'.
```
- **Explanation:** A subclass cannot re-declare a private field that already exists on its base class.

### Example 5: Idiomatic Modern Usage (Readonly Public + Private Mutator)
- **Goal:** Expose public read-only properties backed by private mutation logic.
- **Code:**
```typescript
export class StopWatch {
  #elapsedMs: number = 0;
  #intervalId: ReturnType<typeof setInterval> | null = null;

  public get elapsedMs(): number {
    return this.#elapsedMs;
  }

  public start(): void {
    if (this.#intervalId !== null) return;
    this.#intervalId = setInterval(() => {
      this.#elapsedMs += 10;
    }, 10);
  }

  public stop(): void {
    if (this.#intervalId !== null) {
      clearInterval(this.#intervalId);
      this.#intervalId = null;
    }
  }
}

const timer = new StopWatch();
timer.start();
console.log('Elapsed:', timer.elapsedMs);
```
- **Output / Type Evaluation:**
```typescript
// External callers can inspect elapsedMs but cannot mutate it directly.
```
- **Explanation:** Combining a getter with a private backing field enforces pure encapsulation.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **TypeScript `private` Is Not Secure at Runtime:** Writing `(obj as any).privateField` or `obj["privateField"]` successfully accesses the field at JavaScript runtime. Never rely on `private` to protect security secrets from malicious runtime scripts; use `#private`.
2. **`#private` Fields Cannot Be Re-declared in Subclasses:** Subclasses cannot define a `#field` with the same name if declared in the base class.
3. **Protected Constructors:** A class with a `protected constructor` cannot be instantiated externally via `new Base()`, but CAN be instantiated via subclasses (`class Sub extends Base { constructor() { super(); } }`). This is the idiomatic pattern for abstract base classes that should not be instantiated directly.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Relying on TS `private` for cryptographic keys | Key leaked via `JSON.stringify()` | Use ECMAScript `#privateKey` | Guarantees true runtime data security |
| Overriding a `private` property in a subclass | `TS2415: Separate declarations of private property` | Make property `protected` in base class | Allows safe inheritance and extension |
| Bypassing privacy with `obj["field"]` in prod | Silent breakage during internal refactoring | Use public APIs or getters | Preserves encapsulation boundaries |
| Using `#field` when targeting ES5 without polyfill | Downlevel emit overhead via WeakMaps | Target ES2022+ in modern Node/browsers | Preserves native engine performance |
| Declaring `public` redundantly on every member | Code noise | Omit `public` (it is the default) | Keeps class syntax clean and readable |
| Making constructors public on singleton classes | Multiple instances created accidentally | Make constructor `private` | Enforces singleton pattern integrity |

---

## 8. Performance & Type-Checker Complexity
- **Native Private Field Performance:** V8 allocates private brand symbols in hidden classes. Native `#field` access has near-identical performance to standard property access in modern engines.
- **WeakMap Downlevel Emit:** If compiling `#field` to ES5/ES2015, the compiler generates `WeakMap` lookups. This introduces $O(1)$ map lookup overhead and increases bundle size.
- **Isolated Declarations:** Private members do not require explicit type annotations under `--isolatedDeclarations` because they are not part of the public `.d.ts` interface.

---

## 9. Compiler Internals
- **AST Nodes:**
  - `ts.Modifier` with `SyntaxKind.PublicKeyword`, `SyntaxKind.ProtectedKeyword`, `SyntaxKind.PrivateKeyword`.
  - `ts.PrivateIdentifier` (`SyntaxKind.PrivateIdentifier`) for `#field`.
- **Nominal Identity Check:** In `checker.ts`, `isRelatedTo` checks:
  ```typescript
  if (sourceProp.flags & SymbolFlags.Private || sourceProp.flags & SymbolFlags.Protected) {
      if (sourceProp !== targetProp) {
          return false; // Private members must originate from identical symbol!
      }
  }
  ```

---

## 10. Best Practices & Design Guidelines
- **Use `#private` for True Encapsulation:** Always prefer `#field` for sensitive state, database credentials, and critical invariants.
- **Use `protected` for Template Method Patterns:** Mark extension points as `protected` to allow subclasses to customize behavior without exposing fields publicly.
- **Keep Internal State Minimal:** Expose derived data through public getters rather than granting direct access to state fields.

---

## 11. Real-World Case Study
- **Scenario:** A healthcare SDK handled HIPAA-compliant patient health records. The `PatientRecord` class typed medical diagnoses as `private diagnosis: string`. A telemetry logging service serialized API responses with `JSON.stringify(response)`. Because TypeScript's `private` keyword does not affect JavaScript runtime object keys, unencrypted medical diagnoses were serialized and logged in plain text, causing a critical data privacy audit failure.
- **Implementation:** Migrated to ECMAScript `#private` fields:
  ```typescript
  // Before (leaked during JSON.stringify):
  class PatientRecord {
    private ssn: string;
    private diagnosis: string;
  }

  // After (completely omitted from JSON.stringify):
  class PatientRecord {
    #ssn: string;
    #diagnosis: string;
    constructor(ssn: string, diag: string) {
      this.#ssn = ssn;
      this.#diagnosis = diag;
    }
  }
  ```
- **Outcome:** `JSON.stringify()` automatically stripped all `#private` fields, immediately ensuring zero medical data leaks across logging and analytics pipelines.

---

## 12. Interview Questions

### Q1: What is the fundamental difference between TypeScript's `private` keyword and ECMAScript's `#private` field syntax?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **TypeScript `private`:** A purely compile-time annotation. It is completely erased during compilation, emitting a standard public JavaScript property. It can be accessed at runtime via bracket syntax (`obj["privateField"]`) or `(obj as any).privateField`, and it is visible to `Object.keys()` and `JSON.stringify()`.
- **ECMAScript `#private`:** A native runtime language feature enforced by the JavaScript engine. It is impossible to access from outside the class body—bracket syntax (`obj["#field"]`) throws a syntax error, and `Object.keys()` cannot see it.
</details>

### Q2: Why does adding a `private` or `protected` field break structural assignability between two classes with identical shapes?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript uses structural typing for public members, but nominal typing for private and protected members. For two classes `A` and `B` to be compatible when private members are present, the private members must originate from the **exact same declaration** (i.e. one class must inherit from the other). This guarantees that an external class cannot masquerade as an instance of a class that encapsulates private state.
</details>

### Q3: How does `protected` differ from `private` in class hierarchies?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `private` members are accessible ONLY within the body of the class where they are declared. Subclasses cannot read or call them.
- `protected` members are accessible within the declaring class AND within any derived subclasses. However, they remain inaccessible to external callers outside the inheritance hierarchy.
</details>

### Q4: What is the use case for a `protected constructor`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A class with a `protected constructor` cannot be instantiated directly with `new Base()`. However, it can be subclassed, and the subclass constructor can legally call `super()`. This pattern is used to declare base classes that are intended solely to provide shared implementation to derived subclasses without allowing standalone instantiation.
</details>

### Q5: Can a subclass access a protected member of another instance of the base class?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In TypeScript, a subclass can only access a protected member on another instance if that instance's type is known to be the subclass type (or a derived type of that subclass). Accessing a protected member on an instance typed strictly as the base class is disallowed to prevent cross-hierarchy access between sibling subclasses.
</details>

### Q6: How do `#private` methods work in modern JavaScript and TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

You can prefix class methods with `#` (e.g. `#validateSession() {}`). Like private fields, private methods can only be invoked from within other methods of the same class. They are not stored on `prototype` where external code could inspect or overwrite them.
</details>

### Q7: What happens if you inspect an object with `#private` fields using `Object.getOwnPropertyNames()`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`Object.getOwnPropertyNames()` returns an array of string property keys. Because ECMAScript private fields are stored in private internal slots (not as string or symbol properties), they do not appear in `Object.getOwnPropertyNames()`, `Object.getOwnPropertySymbols()`, or `Reflect.ownKeys()`.
</details>

### Q8: What is the downside of compiling `#private` fields to `target: "ES5"`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When compiling to targets that lack native private field support (ES2021 and below), TypeScript emulates `#private` fields using module-scoped `WeakMap` instances. For every private field, a WeakMap is allocated, and property reads/writes are transformed into `_fieldName.get(this)` and `_fieldName.set(this, value)`. This increases memory usage, slows down execution, and inflates bundle size.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Encapsulated Bank Account (Easy)
- **Problem Statement:** Author a class `BankAccount` with a hard private `#balance` initialized via constructor, providing `deposit(amt)` and a read-only `balance` getter.
- **Test Case:**
```typescript
const acc = new BankAccount(100);
acc.deposit(50);
console.log(acc.balance); // 150
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class BankAccount {
  #balance: number;

  constructor(initialBalance: number) {
    this.#balance = initialBalance;
  }

  deposit(amount: number): void {
    if (amount <= 0) throw new Error('Invalid deposit amount');
    this.#balance += amount;
  }

  get balance(): number {
    return this.#balance;
  }
}
```
</details>

### Exercise 2: Protected Template Method (Easy)
- **Problem Statement:** Build a `DataParser` base class with a `public parse(data: string)` method that delegates to a `protected abstract parseContent(raw: string): object` method.
- **Test Case:**
```typescript
class JSONParser extends DataParser {
  protected parseContent(raw: string) { return JSON.parse(raw); }
}
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
abstract class DataParser {
  public parse(data: string): object {
    console.log('Sanitizing input...');
    return this.parseContent(data.trim());
  }

  protected abstract parseContent(raw: string): object;
}
```
</details>

### Exercise 3: Protected Constructor Singleton Factory (Medium)
- **Problem Statement:** Implement a base class `Service` with a protected constructor, and a derived class `AppService` with a static `getInstance()` method.
- **Test Case:**
```typescript
const svc = AppService.getInstance();
// new AppService() should be protected or private!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Service {
  protected constructor(public readonly name: string) {}
}

class AppService extends Service {
  private static instance: AppService | null = null;

  private constructor() {
    super('AppService');
  }

  public static getInstance(): AppService {
    if (!this.instance) {
      this.instance = new AppService();
    }
    return this.instance;
  }
}
```
</details>

### Exercise 4: Token Signer with Private Methods (Medium)
- **Problem Statement:** Author a `TokenManager` class using `#hashKey` private method that cannot be accessed or overridden.
- **Test Case:**
```typescript
const tm = new TokenManager('secret');
const token = tm.generateToken('user1');
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class TokenManager {
  #secret: string;

  constructor(secret: string) {
    this.#secret = secret;
  }

  #hashKey(val: string): string {
    return `${val}_${this.#secret}`;
  }

  generateToken(userId: string): string {
    return this.#hashKey(userId);
  }
}
```
</details>

### Exercise 5: Nominal Entity Verifier (Hard)
- **Problem Statement:** Prove nominal subtyping rejection by creating two classes `Order` and `Invoice` with identical fields but private identifiers, demonstrating compiler rejection on assignment.
- **Test Case:**
```typescript
let order: Order = new Invoice('INV-1', 100); // Compile Error!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Order {
  private _orderBrand = true;
  constructor(public id: string, public amount: number) {}
}

class Invoice {
  private _invoiceBrand = true;
  constructor(public id: string, public amount: number) {}
}

// let o: Order = new Invoice('INV-1', 100);
// Fails TS2322: Types have separate declarations of a private property.
```
</details>

### Exercise 6: Deep Private State Clone (Hard)
- **Problem Statement:** Author a class `SecureEntity` with `#secret` that provides a `clone()` method capable of copying private fields between instances of the same class.
- **Test Case:**
```typescript
const e1 = new SecureEntity('my-secret');
const e2 = e1.clone();
console.log(e2.verify('my-secret')); // true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class SecureEntity {
  #secret: string;

  constructor(secret: string) {
    this.#secret = secret;
  }

  verify(attempt: string): boolean {
    return this.#secret === attempt;
  }

  clone(): SecureEntity {
    // Other instances of the same class CAN access #secret inside class methods!
    const copy = new SecureEntity('');
    copy.#secret = this.#secret;
    return copy;
  }
}
```
</details>
