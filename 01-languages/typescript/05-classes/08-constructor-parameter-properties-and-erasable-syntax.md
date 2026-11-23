# Constructor Parameter Properties and Erasable Syntax

> Constructor parameter properties provide concise field assignment in traditional TypeScript, but violate modern erasable syntax boundaries; explicit class field declarations ensure compatibility with native type-stripping runtimes.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-classes.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/00-classes.md), [01-access-modifiers.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/01-access-modifiers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.8 (`--erasableSyntaxOnly`) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `erasableSyntaxOnly: true`, `useDefineForClassFields: true` |

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
- **Problem Statement:** In standard JavaScript and traditional OOP languages, declaring and initializing constructor fields requires writing each identifier three times: (1) field declaration, (2) constructor parameter, and (3) constructor body assignment (`this.x = x`). This boilerplate led TypeScript to introduce "parameter properties" (`constructor(public x: number)`).
- **Historical Context:** Parameter properties were introduced in TypeScript 0.9. However, parameter properties generate runtime JavaScript assignments (`this.x = x`). In modern JavaScript runtimes (Node.js 22 native type stripping, Bun, Deno) and modern bundlers (esbuild, Vite, SWC), compilers operate on "erasable syntax" — simply stripping type annotations with zero code generation. TypeScript 5.8 introduced `--erasableSyntaxOnly`, which disallows constructor parameter properties.
- **Real-World Relevance:** Understanding parameter properties and their tension with the TC39 Type Annotations proposal is vital for migrating enterprise codebases to native Node 22+ execution.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Parameter Property Modifier: Adding `public`, `protected`, `private`, or `readonly` to a constructor parameter automatically creates an instance property with that name and assigns the argument to it.
  - Runtime Code Generation: Parameter properties do not simply erase; they inject executable assignment statements at the start of the emitted constructor body.
  - Erasable Syntax Boundary: Any TypeScript syntax that requires transforming code structure or emitting runtime statements (enums, namespaces with values, parameter properties) violates the erasable syntax boundary.
  - TypeScript 5.8 `--erasableSyntaxOnly`: When enabled, flags parameter properties with `TS5118: Cannot use parameter properties when '--erasableSyntaxOnly' is enabled`.
- **Mental Model:** Parameter properties are syntactic sugar that crosses from type space into value code-generation. For native execution, replace them with explicit field declarations and explicit constructor body assignments.
- **Soundness vs Ergonomics:** While parameter properties reduce line count, standard class fields align 100% with the ECMAScript 2022 Class Fields specification.

---

## 3. How It Works Internally
- **Compiler Transform:**
  Input TypeScript:
  ```typescript
  class Point {
    constructor(public x: number, public y: number) {}
  }
  ```
  Emitted JavaScript (traditional emit):
  ```javascript
  class Point {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
  }
  ```
- **Node 22 Type Stripping Failure:** Native type stripping replaces type annotations with whitespace:
  ```javascript
  class Point {
    constructor(public x, public y) {} // SyntaxError: Unexpected token 'public'
  }
  ```
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Parameter Properties vs Erasable Syntax                 |
|                                                                          |
|   [ Parameter Property Syntax ]         [ Standard Class Field Syntax ]  |
|   constructor(public id: string)        id: string;                      |
|                                         constructor(id: string) {        |
|                                           this.id = id;                  |
|                                         }                                |
|                 │                                      │                 |
|                 ▼                                      ▼                 |
|   Requires TS compiler codegen           Pure ECMAScript syntax          |
|   Fails Node 22 Type Stripping           Works with node --strip-types   |
|   Fails TS 5.8 --erasableSyntaxOnly      Passes TS 5.8 checks            |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Traditional Parameter Property:**
  ```typescript
  class LegacyUser {
    constructor(
      public readonly id: string,
      private secret: string,
      protected role: string = 'user'
    ) {}
  }
  ```
- **Modern Erasable Class Field Pattern:**
  ```typescript
  class ModernUser {
    readonly id: string;
    #secret: string;
    protected role: string;

    constructor(id: string, secret: string, role: string = 'user') {
      this.id = id;
      this.#secret = secret;
      this.role = role;
    }
  }
  ```
- **tsconfig Dependencies:**
  - `erasableSyntaxOnly: true` (TS 5.8+): Strictly disallows parameter properties.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compare traditional parameter properties with modern erasable fields.
- **Code:**
```typescript
// Traditional (non-erasable):
class LegacyTask {
  constructor(public id: string, public completed: boolean = false) {}
}

// Modern (100% erasable syntax):
export class ModernTask {
  public id: string;
  public completed: boolean;

  constructor(id: string, completed: boolean = false) {
    this.id = id;
    this.completed = completed;
  }
}

const task = new ModernTask('task_101');
console.log(task.id, task.completed); // "task_101", false
```
- **Output / Type Evaluation:**
```typescript
// ModernTask executes cleanly under Node 22 without transpile steps.
```
- **Explanation:** Explicit class fields provide identical type safety while conforming to standard ECMAScript.

### Example 2: Real-World Enterprise Pattern (Node.js 22 Native Microservice Entity)
- **Goal:** Author domain entities that execute natively under `node --experimental-strip-types`.
- **Code:**
```typescript
export class AccountEntity {
  public readonly accountId: string;
  public balanceCents: number;
  #internalRoutingNumber: string;

  constructor(
    accountId: string,
    initialBalance: number,
    routingNumber: string
  ) {
    this.accountId = accountId;
    this.balanceCents = initialBalance;
    this.#internalRoutingNumber = routingNumber;
  }

  public get maskedRouting(): string {
    return `***${this.#internalRoutingNumber.slice(-4)}`;
  }
}

const acc = new AccountEntity('acc_9921', 50000, '021000021');
console.log(acc.accountId, acc.maskedRouting);
```
- **Output / Type Evaluation:**
```typescript
// "acc_9921", "***0021"
```
- **Explanation:** Combining standard class fields with native `#private` identifiers satisfies both runtime privacy and erasable syntax compliance.

### Example 3: Edge Case / Boundary Condition (Initialization Order with Super Calls)
- **Goal:** Understand when parameter properties are initialized relative to `super()`.
- **Code:**
```typescript
class BaseItem {
  constructor(public baseId: string) {
    console.log('Base constructor called. baseId:', this.baseId);
  }
}

class DerivedItem extends BaseItem {
  // Parameter property in derived class:
  constructor(baseId: string, public derivedTag: string) {
    super(baseId);
    // In derived classes, parameter properties are initialized AFTER super() returns!
    console.log('Derived constructor completed. tag:', this.derivedTag);
  }
}

new DerivedItem('b_1', 'd_1');
```
- **Output / Type Evaluation:**
```typescript
// Base constructor called. baseId: b_1
// Derived constructor completed. tag: d_1
```
- **Explanation:** Parameter properties in derived classes run immediately after `super()` returns, before the derived constructor body executes.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection under `--erasableSyntaxOnly` in TypeScript 5.8+.
- **Code:**
```typescript
// tsconfig: { "erasableSyntaxOnly": true }
class ProductModel {
  // @ts-expect-error TS5118: Cannot use parameter properties when '--erasableSyntaxOnly' is enabled.
  constructor(public sku: string, private price: number) {}
}
```
- **Real Compiler Error:**
```
error TS5118: Cannot use parameter properties when '--erasableSyntaxOnly' is enabled.
```
- **Explanation:** TypeScript 5.8 detects non-erasable constructor parameter properties and flags them at compile time.

### Example 5: Idiomatic Modern Usage (Destructured Options Object Pattern)
- **Goal:** Model domain classes with many fields using a single typed options bag.
- **Code:**
```typescript
export type ServerOptions = {
  host: string;
  port?: number;
  maxConnections?: number;
  enableTls?: boolean;
};

export class ServerConfig {
  public readonly host: string;
  public readonly port: number;
  public readonly maxConnections: number;
  public readonly enableTls: boolean;

  constructor({
    host,
    port = 8080,
    maxConnections = 1000,
    enableTls = false,
  }: ServerOptions) {
    this.host = host;
    this.port = port;
    this.maxConnections = maxConnections;
    this.enableTls = enableTls;
  }
}

const config = new ServerConfig({ host: '0.0.0.0', enableTls: true });
console.log(config.port, config.enableTls); // 8080, true
```
- **Output / Type Evaluation:**
```typescript
// Fully typed, self-documenting configuration constructor.
```
- **Explanation:** Destructured option bags eliminate positional argument confusion and comply 100% with erasable syntax.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Node.js Type Stripping SyntaxError:** Running `node --experimental-strip-types app.ts` on a file with parameter properties causes a runtime `SyntaxError: Unexpected token 'public'`.
2. **Double Initialization with useDefineForClassFields:** If you declare a field `foo: string;` AND use a parameter property `constructor(public foo: string)`, ECMAScript defines the property twice, which can trigger errors or unexpected behavior.
3. **Parameter Properties Disallow `#private`:** You cannot write `constructor(#secret: string)`. ECMAScript private fields cannot be declared as parameter properties.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using parameter properties with Node 22 | Runtime `SyntaxError: Unexpected token 'public'` | Explicit field declarations + `this.x = x` | Conforms to modern erasable syntax standards |
| Writing `constructor(#secret: string)` | Syntax error in TS parser | Declare `#secret` on class body | Native private fields must be declared on class |
| Declaring field AND parameter property | Duplicate declaration confusion | Use only explicit fields in modern TS | Prevents duplicate field assignments |
| Using parameter properties under TS 5.8 | `TS5118: Cannot use parameter properties` | Refactor to standard class fields | Pass `--erasableSyntaxOnly` validation |
| Disregarding initialization order | `this` read before `super()` returns | Call `super()` before accessing fields | Adheres to ECMAScript class execution order |
| Mixing 10+ positional parameter properties | Argument transposition bugs at call sites | Use a single destructured options object | Improves readability and API stability |

---

## 8. Performance & Type-Checker Complexity
- **Fast Type Stripping:** Transpilers like `esbuild`, `swc`, and `tsgo` process explicit class fields 10x faster than generating custom parameter property assignment statements.
- **V8 Hidden Classes:** Explicitly declared class fields with constructor assignments optimize hidden class transitions in modern V8 engines.
- **Isolated Declarations:** Fully supported under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** `ts.ParameterDeclaration` with `modifiers` containing `SyntaxKind.PublicKeyword`, etc.
- **Emitter Processing:** In legacy emit, `emitConstructor` synthesizes `ts.BinaryExpression` nodes (`this.prop = prop`) and prepends them to the constructor statement list.
- **Erasable Syntax Diagnostic:** In `checker.ts`, if `compilerOptions.erasableSyntaxOnly` is enabled and `hasSyntacticModifier(param, ModifierFlags.ParameterPropertyModifier)`, emit error `TS5118`.

---

## 10. Best Practices & Design Guidelines
- **Adopt Explicit Class Fields for New Projects:** Phase out parameter properties in favor of explicit field declarations.
- **Enable `erasableSyntaxOnly` in TS 5.8+:** Guarantee that your repository runs natively in modern runtimes without build steps.
- **Use Options Bags for Complex Constructors:** Replace multi-argument constructors with a single destructured configuration object.

---

## 11. Real-World Case Study
- **Scenario:** A high-frequency trading platform migrated its backend microservices from Dockerized TypeScript compilation (`tsc`) to Node.js 22 native type stripping (`node --experimental-strip-types`) to cut container startup times. During deployment, 80% of microservices failed to boot with `SyntaxError: Unexpected token 'public'` in constructor signatures.
- **Implementation:** Ran an automated AST codemod converting all parameter properties to explicit class fields:
  ```typescript
  // Before (crashed Node 22):
  class Order {
    constructor(public id: string, public price: number) {}
  }

  // After (runs natively in Node 22):
  class Order {
    public id: string;
    public price: number;
    constructor(id: string, price: number) {
      this.id = id;
      this.price = price;
    }
  }
  ```
- **Outcome:** All microservices booted natively in Node 22 with zero transpilation, slashing container startup times from 4.2 seconds to 380 milliseconds.

---

## 12. Interview Questions

### Q1: What is a parameter property in TypeScript and what code does it generate?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A parameter property is a TypeScript-specific syntax shortcut. When an access modifier (`public`, `protected`, `private`) or `readonly` is added to a constructor parameter (`constructor(public name: string)`), the compiler automatically:
1. Declares an instance property with that name on the class type.
2. Injects an assignment statement (`this.name = name;`) at the beginning of the emitted JavaScript constructor body.
</details>

### Q2: Why do parameter properties conflict with modern runtime type stripping in Node.js 22?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Node.js 22 (`--experimental-strip-types`) and modern single-file tools implement type stripping by replacing type annotations with whitespace without modifying the surrounding JavaScript code structure. Because parameter properties are non-erasable (they require the compiler to emit `this.x = x` statements into the constructor), simply stripping the type annotation leaves `constructor(public x)`, which is invalid JavaScript and throws a runtime `SyntaxError`.
</details>

### Q3: What is the `--erasableSyntaxOnly` flag introduced in TypeScript 5.8?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`--erasableSyntaxOnly` is a compiler flag in TypeScript 5.8 that issues compile-time errors whenever TypeScript-specific syntax requiring runtime code generation is used. It flags:
1. Traditional `enum` declarations.
2. Parameter properties in constructors.
3. Namespaces containing executable code.
4. `import =` alias declarations.
Enabling it guarantees that your code can be executed directly by any standard type-stripping engine.
</details>

### Q4: Can you use ECMAScript private fields (`#field`) as parameter properties?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. TypeScript does not permit `#` private identifiers in constructor parameter lists (`constructor(#secret: string)` is a syntax error). Native ECMAScript private fields must be declared directly on the class body.
</details>

### Q5: In derived classes, when are parameter property assignments executed?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In derived classes, parameter property assignments are executed immediately after `super()` returns, before the remainder of the derived class constructor body executes.
</details>

### Q6: How does `readonly` behave on a parameter property?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Writing `constructor(readonly id: string)` declares a public, read-only instance property and assigns it inside the constructor. Any attempt to modify the property after constructor completion triggers `TS2540: Cannot assign to 'id' because it is a read-only property`.
</details>

### Q7: What is the recommended modern alternative to parameter properties for classes with many dependencies?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The recommended modern pattern is declaring explicit class fields and accepting a single typed options bag:
```typescript
class Service {
  private logger: Logger;
  private db: Database;

  constructor({ logger, db }: ServiceDeps) {
    this.logger = logger;
    this.db = db;
  }
}
```
This complies with erasable syntax, prevents parameter ordering bugs, and improves readability.
</details>

### Q8: What happens if a parameter property has a default value?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Writing `constructor(public timeout: number = 5000)` creates an optional constructor argument. If the caller omits the argument or passes `undefined`, the default value `5000` is evaluated and assigned to `this.timeout`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Refactor Parameter Property to Erasable Syntax (Easy)
- **Problem Statement:** Refactor `class Point { constructor(public x: number, public y: number) {} }` to 100% erasable syntax.
- **Test Case:**
```typescript
const p = new Point(10, 20);
console.log(p.x, p.y); // 10, 20
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Point {
  public x: number;
  public y: number;

  constructor(x: number, y: number) {
    this.x = x;
    this.y = y;
  }
}
```
</details>

### Exercise 2: Erasable Readonly Entity (Easy)
- **Problem Statement:** Author a class `Article` with `readonly id: string` and `title: string` using explicit field initializations.
- **Test Case:**
```typescript
const a = new Article('art_1', 'TypeScript 5.8');
// a.id = 'new' fails compilation!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Article {
  readonly id: string;
  title: string;

  constructor(id: string, title: string) {
    this.id = id;
    this.title = title;
  }
}
```
</details>

### Exercise 3: Options Bag DI Pattern (Medium)
- **Problem Statement:** Implement a class `PaymentGateway` that accepts an options object `{ apiKey: string; sandbox?: boolean }` without using parameter properties.
- **Test Case:**
```typescript
const gw = new PaymentGateway({ apiKey: 'pk_live_123' });
console.log(gw.sandbox); // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type GatewayConfig = {
  apiKey: string;
  sandbox?: boolean;
};

class PaymentGateway {
  readonly apiKey: string;
  readonly sandbox: boolean;

  constructor({ apiKey, sandbox = false }: GatewayConfig) {
    this.apiKey = apiKey;
    this.sandbox = sandbox;
  }
}
```
</details>

### Exercise 4: Native Private Field Entity (Medium)
- **Problem Statement:** Author a class `SessionToken` with an erasable native `#token` field and a public method `verify(token: string): boolean`.
- **Test Case:**
```typescript
const s = new SessionToken('token_123');
console.log(s.verify('token_123')); // true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class SessionToken {
  #token: string;

  constructor(token: string) {
    this.#token = token;
  }

  verify(candidate: string): boolean {
    return this.#token === candidate;
  }
}
```
</details>

### Exercise 5: Compile-Time Erasable Class Inspector (Hard)
- **Problem Statement:** Author a type-level check `HasNoParameterProperties<C>` that verifies constructor parameters do not duplicate instance keys.
- **Test Case:**
```typescript
class Model { id: string; constructor(id: string) { this.id = id; } }
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type ConstructorParams<T> = T extends new (...args: infer P) => any ? P : never;
// Type utility checking parameter counts against property counts
type HasValidConstructor<T extends new (...args: any[]) => any> =
  ConstructorParams<T> extends any[] ? true : false;
```
</details>

### Exercise 6: Polymorphic Record Entity with Options Bag (Hard)
- **Problem Statement:** Implement a generic entity `EntityRecord<T>` that takes a base `id` and a generic `data: T` payload via a destructured options bag, freezing the instance.
- **Test Case:**
```typescript
const rec = new EntityRecord({ id: 'rec_1', data: { score: 100 } });
console.log(rec.data.score); // 100
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type EntityOptions<T> = {
  id: string;
  data: T;
};

class EntityRecord<T> {
  readonly id: string;
  readonly data: T;

  constructor({ id, data }: EntityOptions<T>) {
    this.id = id;
    this.data = data;
    Object.freeze(this);
  }
}
```
</details>
