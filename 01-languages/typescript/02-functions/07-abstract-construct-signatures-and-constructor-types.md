# Abstract Construct Signatures and Constructor Types

> Constructor types and abstract construct signatures define object instantiation contracts, powering mixin architectures, dependency injection containers, and reflection registries.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-function-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/00-function-types.md), [01-parameters-and-return-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/01-parameters-and-return-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 4.2 (`abstract` construct signatures) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true` |

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
- **Problem Statement:** In enterprise TypeScript architectures, dependency injection (DI) frameworks (NestJS, InversifyJS, Tsyringe) and mixin composition patterns treat class constructors as first-class values. Prior to TypeScript 4.2, passing an `abstract class` to a function expecting a constructor failed because abstract classes cannot be directly instantiated with `new`.
- **Historical Context:** TypeScript 0.9 supported concrete construct signatures (`new (...args: any[]) => T`). TypeScript 4.2 introduced `abstract new (...args: any[]) => T`, allowing functions to accept both concrete and abstract constructors for mixins and type token lookups.
- **Real-World Relevance:** Constructor types enable type-safe dependency injection containers, database ORM entity registries (TypeORM, MikroORM), and composable mixin hierarchies without sacrificing static type checking.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Concrete Construct Signature: `new (...args: Args) => Instance` specifies a constructor that CAN be instantiated via the `new` keyword. Abstract classes are NOT assignable to concrete construct signatures.
  - Abstract Construct Signature: `abstract new (...args: Args) => Instance` specifies a constructor type that matches BOTH concrete classes and abstract classes.
  - Object Construct Signature: `{ new (...args: Args): Instance }` defines constructability within an object interface.
  - `InstanceType<T>`: Built-in utility type that extracts the instance type produced by a constructor type.
  - `ConstructorParameters<T>`: Extracts the parameter types of a constructor as a tuple.
- **Mental Model:** A regular function type describes a callable (`fn()`). A constructor type describes an instantiable entity (`new Constructor()`). Adding `abstract` widens the contract to accept class templates that cannot be directly instantiated.
- **Soundness vs Ergonomics:** Abstract construct signatures allow factory and mixin functions to extend abstract base classes safely.

---

## 3. How It Works Internally
- **Construct Signature Representation:** The compiler distinguishes between `CallSignature` and `ConstructSignature`. In `checker.ts`, construct signatures have the `SignatureFlags.Construct` bit set.
- **Abstract Class Assignability:** When checking `isTypeAssignableTo`, an `abstract class` constructor has `TypeFlags.Object` with construct signatures flagged as abstract. Concrete construct parameters reject abstract constructors, while abstract construct parameters accept both.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Construct Signature Subtyping Hierarchy                 |
|                                                                          |
|       abstract new (...args: any[]) => T   (Accepts Concrete & Abstract) |
|                        ▲                                                 |
|                        │ (Structural Subtype)                            |
|                        │                                                 |
|          new (...args: any[]) => T         (Accepts Concrete ONLY)       |
|                                                                          |
|   Assignment Check:                                                      |
|   ├── ConcreteClass assignable to 'new () => T'          ──► YES         |
|   ├── AbstractClass assignable to 'abstract new () => T' ──► YES         |
|   └── AbstractClass assignable to 'new () => T'          ──► NO (TS2511) |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Construct Type Expression:**
  ```typescript
  type ConcreteConstructor<T = object> = new (...args: any[]) => T;
  type AbstractConstructor<T = object> = abstract new (...args: any[]) => T;
  ```
- **Interface Construct Signature:**
  ```typescript
  interface Factory<T> {
    new (...args: any[]): T;
  }
  ```
- **Standard Library Utility Types:**
  - `InstanceType<T extends abstract new (...args: any) => any>`
  - `ConstructorParameters<T extends abstract new (...args: any) => any>`
- **tsconfig Dependencies:**
  - Requires TypeScript 4.2+ for the `abstract` constructor modifier.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compare concrete and abstract constructor assignments.
- **Code:**
```typescript
abstract class BaseController {
  abstract handle(): void;
}

class UserController extends BaseController {
  handle(): void {
    console.log('User request handled');
  }
}

type AnyConstructor = new (...args: any[]) => any;
type AnyAbstractConstructor = abstract new (...args: any[]) => any;

let concreteCons: AnyConstructor;
let abstractCons: AnyAbstractConstructor;

concreteCons = UserController; // Legal: concrete class
abstractCons = UserController; // Legal: concrete satisfies abstract

abstractCons = BaseController; // Legal: abstract class satisfies abstract construct signature!

// @ts-expect-error TS2511: Cannot assign an abstract constructor to a non-abstract constructor type.
concreteCons = BaseController;
```
- **Output / Type Evaluation:**
```typescript
// TS2511 prevents assigning BaseController to a type that can be instantiated with 'new'.
```
- **Explanation:** Abstract construct signatures allow passing abstract classes to registries and mixins safely.

### Example 2: Real-World Enterprise Pattern (Type-Safe Mixin Pattern)
- **Goal:** Compose cross-cutting concerns (timestamping, logging) onto classes via mixins.
- **Code:**
```typescript
export type GConstructor<T = object> = new (...args: any[]) => T;

export function Timestamped<TBase extends GConstructor>(Base: TBase) {
  return class extends Base {
    public readonly createdAt = new Date();

    getAgeMs(): number {
      return Date.now() - this.createdAt.getTime();
    }
  };
}

class DocumentRecord {
  constructor(public title: string) {}
}

const TimestampedDocument = Timestamped(DocumentRecord);
const doc = new TimestampedDocument('Architecture Spec');

console.log(doc.title); // "Architecture Spec"
console.log(doc.createdAt.toISOString());
```
- **Output / Type Evaluation:**
```typescript
// doc has both DocumentRecord properties (title) and Timestamped properties (createdAt)!
```
- **Explanation:** Mixin functions take a constructor type and return a dynamically extended derived class with merged instance properties.

### Example 3: Edge Case / Boundary Condition (Abstract Mixin with TS 4.2+)
- **Goal:** Apply a mixin to an abstract base class.
- **Code:**
```typescript
type AbstractGConstructor<T = object> = abstract new (...args: any[]) => T;

abstract class ServiceNode {
  abstract execute(): Promise<void>;
}

function WithLogging<TBase extends AbstractGConstructor<ServiceNode>>(Base: TBase) {
  abstract class LoggedService extends Base {
    async runWithLog(): Promise<void> {
      console.log('Starting execution...');
      await this.execute();
      console.log('Execution finished.');
    }
  }
  return LoggedService;
}

const LoggedBase = WithLogging(ServiceNode);

class DatabaseBackupService extends LoggedBase {
  async execute(): Promise<void> {
    console.log('Backing up database...');
  }
}

const svc = new DatabaseBackupService();
await svc.runWithLog();
```
- **Output / Type Evaluation:**
```typescript
// Full mixin composition over abstract classes enabled by abstract construct signatures!
```
- **Explanation:** In TypeScript 4.2+, `abstract new (...args: any[]) => T` permits mixins to target abstract classes.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate error when attempting to instantiate an abstract constructor type.
- **Code:**
```typescript
type AbstractToken<T> = abstract new (...args: any[]) => T;

function createInstance<T>(token: AbstractToken<T>): T {
  // @ts-expect-error TS2511: Cannot create an instance of an abstract class.
  return new token();
}
```
- **Real Compiler Error:**
```
error TS2511: Cannot create an instance of an abstract class.
```
- **Explanation:** The compiler strictly blocks invoking `new` on an abstract construct signature.

### Example 5: Idiomatic Modern Usage (Type-Safe DI Container Token Registry)
- **Goal:** Build an IoC / DI container that accepts class constructors as service injection tokens.
- **Code:**
```typescript
type ServiceToken<T> = abstract new (...args: any[]) => T;

export class DependencyContainer {
  private instances = new Map<ServiceToken<any>, any>();

  register<T>(token: ServiceToken<T>, instance: T): void {
    this.instances.set(token, instance);
  }

  resolve<T>(token: ServiceToken<T>): T {
    const instance = this.instances.get(token);
    if (!instance) {
      throw new Error(`No service registered for token: ${token.name}`);
    }
    return instance as T;
  }
}

abstract class CacheService {
  abstract get(key: string): string | null;
}

class RedisCacheService extends CacheService {
  get(key: string): string | null {
    return 'cached_val';
  }
}

const container = new DependencyContainer();
container.register(CacheService, new RedisCacheService());

const cache = container.resolve(CacheService);
console.log(cache.get('token')); // "cached_val"
```
- **Output / Type Evaluation:**
```typescript
// cache is inferred as CacheService; abstract token resolves concrete instance!
```
- **Explanation:** Using `abstract new` allows registering and resolving by abstract class tokens without runtime errors.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`InstanceType` on Non-Constructors:** Passing a regular function or interface to `InstanceType<T>` causes `TS2344: Type does not satisfy constraint 'abstract new (...args: any) => any'`.
2. **Private / Protected Constructors:** A class with a `private` or `protected` constructor cannot be assigned to `new (...args: any[]) => T`. This correctly prevents external instantiation of singletons or base classes.
3. **Constructor Overloading:** If a class has constructor overloads, `ConstructorParameters<typeof MyClass>` resolves to the parameters of the LAST overload signature.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using `Function` as a DI token | Loss of instance type inference | `abstract new (...args: any[]) => T` | Preserves 100% type safety on resolved instances |
| Instantiating an abstract constructor | `TS2511: Cannot create instance of abstract class` | Restrict factory to `new () => T` | Prevents runtime instantiation exceptions |
| Using concrete constructor for abstract mixins | `TS2345: Argument not assignable to constructor` | Use `abstract new (...args: any[]) => T` | Enables mixins on abstract base classes |
| Omitting rest parameters on mixin constructor | Subclass arguments rejected | `new (...args: any[]) => T` | Accommodates arbitrary subclass constructor arguments |
| Passing instance to constructor parameter | Mismatched value space and type space | Pass `typeof MyClass` | Distinguishes class value from instance type |
| Forgetting `extends` constraint in mixin return | Lost prototype chain inheritance | `return class extends Base { ... }` | Preserves `instanceof` checks on mixin instances |

---

## 8. Performance & Type-Checker Complexity
- **Mixin Recursion:** Deeply nested mixin chains (e.g. `A(B(C(D(Base))))`) can cause compiler memory pressure and slow down declaration emit. Keep mixin compositions shallow (2–3 levels).
- **Type Flattening in DI:** Container maps using constructor tokens perform $O(1)$ reference lookups in JavaScript.
- **Isolated Declarations:** Exported mixin factory functions require explicit return types under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:**
  - `ts.ConstructorTypeNode` (`SyntaxKind.ConstructorType`).
  - Marked with `SyntaxKind.AbstractKeyword` if declared as abstract.
- **Symbol & Construction:** A class identifier has two types: the instance type (`MyClass`) and the constructor type (`typeof MyClass`).
- **Checker Logic:** In `checkInstantiateExpression`, the compiler ensures the target type does NOT have `TypeFlags.Abstract` set.

---

## 10. Best Practices & Design Guidelines
- **Use Abstract Construct Signatures for DI Tokens:** Maximize flexibility by allowing both abstract contracts and concrete implementations.
- **Use Generic Constraints for Mixin Base Classes:** Constrain `TBase extends GConstructor<ExpectedInterface>` to verify prerequisites.
- **Leverage `InstanceType<typeof Class>`:** Extract instance contracts dynamically without redundant interface definitions.

---

## 11. Real-World Case Study
- **Scenario:** A microservice framework used an Inversion of Control (IoC) container. Handlers declared dependencies using abstract class contracts: `container.get(AbstractLogger)`. Because the container was typed with `token: new (...args: any[]) => T`, developers encountered compile errors whenever they passed abstract classes as tokens, prompting them to cast with `as any` across 200+ service definitions.
- **Implementation:** Upgraded to TypeScript 4.2+ abstract constructor tokens:
  ```typescript
  // Before:
  function getService<T>(token: new (...args: any[]) => T): T;

  // After:
  function getService<T>(token: abstract new (...args: any[]) => T): T;
  ```
- **Outcome:** Eliminated all `as any` casts across the codebase, allowing abstract classes to serve as first-class dependency injection tokens.

---

## 12. Interview Questions

### Q1: What is the difference between `MyClass` and `typeof MyClass` in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `MyClass` (in type space) represents the **instance type** of the class—the shape of the object created when you execute `new MyClass()`. It contains instance properties and methods.
- `typeof MyClass` (in type space) represents the **constructor type** of the class—the shape of the constructor function object itself. It contains static properties, static methods, and the construct signature (`new (...args: any[]) => MyClass`).
</details>

### Q2: Why did TypeScript 4.2 introduce the `abstract` modifier for constructor type signatures?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Prior to TypeScript 4.2, construct signatures could only describe concrete constructors (`new (...args: any[]) => T`). Because abstract classes cannot be directly instantiated, they were not assignable to standard construct signatures. This prevented abstract classes from being used in mixin factory functions or as dependency injection tokens. TypeScript 4.2 added `abstract new (...args: any[]) => T`, which describes any constructor (concrete or abstract), while preventing direct instantiation via `new` on that type.
</details>

### Q3: How does the TypeScript mixin pattern work using constructor types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A mixin is a function that takes a base class constructor and returns a new class that extends that base class:
```typescript
type Constructor<T = {}> = new (...args: any[]) => T;

function Activatable<TBase extends Constructor>(Base: TBase) {
  return class extends Base {
    isActive = false;
    activate() { this.isActive = true; }
  };
}
```
The generic parameter `TBase` captures the base class instance type, and the returned class dynamically merges the base members with the new mixin members.
</details>

### Q4: How do `InstanceType<T>` and `ConstructorParameters<T>` extract types from constructors?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Both use conditional types with the `infer` keyword:
```typescript
type InstanceType<T extends abstract new (...args: any) => any> =
  T extends abstract new (...args: any) => infer R ? R : any;

type ConstructorParameters<T extends abstract new (...args: any) => any> =
  T extends abstract new (...args: infer P) => any ? P : never;
```
They pattern match against the construct signature to extract the instance return type `R` and parameter tuple `P`.
</details>

### Q5: Can an abstract class implement an interface?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. An abstract class can implement an interface. It is permitted to implement all interface members, or declare some members as `abstract`, delegating their concrete implementation to derived subclasses.
</details>

### Q6: What happens if a class constructor is marked `private`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A class with a `private` constructor cannot be instantiated with `new` outside the class body, nor can it be extended by subclasses. It is not assignable to `new (...args: any[]) => T`. This pattern is typically used to implement Singletons or utility classes with only static methods.
</details>

### Q7: Why does `new token()` fail when `token` is typed as `abstract new (...args: any[]) => T`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The `abstract` modifier explicitly informs the compiler that the constructor may be an abstract class, which has incomplete or missing method implementations. Permitting `new token()` would violate type soundness by creating instances of abstract classes at runtime. To instantiate, the constructor type must omit `abstract`.
</details>

### Q8: How can a mixin require that its base class implement a specific interface?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

By constraining the generic constructor parameter to produce that interface:
```typescript
type HasId = { id: string };
type ConstructorWithId = abstract new (...args: any[]) => HasId;

function WithTracking<TBase extends ConstructorWithId>(Base: TBase) {
  return class extends Base {
    track() { console.log('Tracking entity:', this.id); }
  };
}
```
Any base class that does not have an `id: string` property will be rejected at compile time.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Instance Factory Function (Easy)
- **Problem Statement:** Write a generic factory function `instantiate<T>(ctor: new () => T): T` that creates an instance using `new`.
- **Test Case:**
```typescript
class Item { name = 'default'; }
const item = instantiate(Item); // Item
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function instantiate<T>(ctor: new () => T): T {
  return new ctor();
}
```
</details>

### Exercise 2: Constructor Parameters Logger (Easy)
- **Problem Statement:** Write a function `inspectConstructor<T extends abstract new (...args: any) => any>(ctor: T): string` returning the constructor name.
- **Test Case:**
```typescript
class User { constructor(public id: number) {} }
inspectConstructor(User); // "User"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function inspectConstructor<T extends abstract new (...args: any) => any>(ctor: T): string {
  return ctor.name;
}
```
</details>

### Exercise 3: Disposable Mixin Implementation (Medium)
- **Problem Statement:** Implement a mixin `DisposableMixin` that adds `isDisposed: boolean` and `dispose(): void` to any base class.
- **Test Case:**
```typescript
class Resource { name = 'File'; }
const DisposableResource = DisposableMixin(Resource);
const r = new DisposableResource();
r.dispose();
console.log(r.isDisposed); // true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type AnyConstructor = new (...args: any[]) => object;

function DisposableMixin<TBase extends AnyConstructor>(Base: TBase) {
  return class extends Base {
    public isDisposed = false;

    dispose(): void {
      this.isDisposed = true;
    }
  };
}
```
</details>

### Exercise 4: Type-Safe Registry with Constructor Keys (Medium)
- **Problem Statement:** Build a class registry that maps constructor tokens to singleton instances.
- **Test Case:**
```typescript
const reg = new Registry();
reg.set(User, new User(1));
const u = reg.get(User); // User
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Registry {
  private map = new Map<abstract new (...args: any[]) => any, any>();

  set<T>(token: abstract new (...args: any[]) => T, instance: T): void {
    this.map.set(token, instance);
  }

  get<T>(token: abstract new (...args: any[]) => T): T | undefined {
    return this.map.get(token);
  }
}
```
</details>

### Exercise 5: Compile-Time Constructor Extractor (Hard)
- **Problem Statement:** Author a utility type `MatchConstructor<C, Instance>` that validates if constructor `C` instantiates `Instance`.
- **Test Case:**
```typescript
class A { a = 1; }
class B { b = 2; }
type T1 = MatchConstructor<typeof A, A>; // true
type T2 = MatchConstructor<typeof A, B>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type MatchConstructor<C, Instance> =
  C extends abstract new (...args: any[]) => infer R
    ? [R] extends [Instance]
      ? [Instance] extends [R]
        ? true
        : false
      : false
    : false;
```
</details>

### Exercise 6: Constrained Abstract Mixin Pipeline (Hard)
- **Problem Statement:** Implement a multi-stage abstract mixin that requires the base class to implement `Identifiable` and `Auditable`, composing a `SecuredEntity` class.
- **Test Case:**
```typescript
abstract class Model {
  abstract id: string;
  abstract auditedAt: Date;
}
const SecuredModel = WithSecurity(Model);
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
interface Identifiable { id: string; }
interface Auditable { auditedAt: Date; }

type RequiredBase = abstract new (...args: any[]) => Identifiable & Auditable;

function WithSecurity<TBase extends RequiredBase>(Base: TBase) {
  abstract class Secured extends Base {
    public isLocked = false;

    lock(): void {
      this.isLocked = true;
      console.log(`Secured entity ${this.id} audited on ${this.auditedAt.toISOString()}`);
    }
  }
  return Secured;
}
```
</details>
