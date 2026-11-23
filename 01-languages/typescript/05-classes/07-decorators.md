# Decorators in TypeScript

> TC39 Stage 3 standard decorators (TypeScript 5.0+) provide language-level metaprogramming for classes, methods, accessors, and fields, replacing legacy experimental decorators.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-classes.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/00-classes.md), [05-getters-and-setters.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/05-getters-and-setters.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0 (TC39 Stage 3 Decorators) / TS 5.2 (Metadata) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `target: "ES2022"`, `experimentalDecorators: false` |

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
- **Problem Statement:** Cross-cutting concerns—such as logging, execution profiling, input validation, dependency injection, and authorization checks—frequently pollute core business methods. Wrapping methods manually is error-prone, violates DRY, and obscures business logic.
- **Historical Context:** TypeScript supported an early, experimental decorator proposal for years via `"experimentalDecorators": true` (used heavily in Angular and NestJS). In 2022, TC39 reached Stage 3 consensus on a standardized Decorator specification. TypeScript 5.0 shipped full support for Stage 3 Decorators *without* requiring any compiler flags. TypeScript 5.2 added support for Decorator Metadata.
- **Real-World Relevance:** Stage 3 decorators are part of standard ECMAScript. They provide clean, declarative metaprogramming with strong type safety, context objects, and initializer hooks, obsoleting legacy `experimentalDecorators`.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Standard Function Signature: A Stage 3 decorator is a function taking two arguments: (1) `target` (the value being decorated: class, method, accessor, or undefined for fields), and (2) `context` (a strongly typed descriptor object).
  - Context Object Types:
    - `ClassDecoratorContext`
    - `ClassMethodDecoratorContext`
    - `ClassGetterDecoratorContext` / `ClassSetterDecoratorContext`
    - `ClassAutoAccessorDecoratorContext`
    - `ClassFieldDecoratorContext`
  - `addInitializer` Hook: Decorators can register callbacks via `context.addInitializer()` that run when instances are constructed.
  - Decorator Metadata (`context.metadata` in TS 5.2+): A shared metadata dictionary attached to `Symbol.metadata` on the constructor.
  - Evaluation Order: Decorator expressions are evaluated from top to bottom (like functions), but applied from bottom to top (inside-out).
- **Mental Model:** A decorator is a function that intercepts a class or member declaration at definition time, wrapping or augmenting it before the class is finalized.
- **Soundness vs Ergonomics:** Unlike legacy experimental decorators which required `reflect-metadata` runtime polyfills, Stage 3 decorators are native ECMAScript and carry first-class TypeScript context typing.

---

## 3. How It Works Internally
- **Standard Decorator Context:** The `context` object provides:
  - `kind`: `"class" | "method" | "getter" | "setter" | "accessor" | "field"`
  - `name`: string or symbol of the member.
  - `access`: object containing `get` and `set` functions to read or write the member dynamically.
  - `static`: boolean indicating if the member is static.
  - `private`: boolean indicating if the member is `#private`.
  - `addInitializer(fn)`: registers an initialization callback.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Stage 3 Decorator Execution Pipeline                    |
|                                                                          |
|   @logged                                                                |
|   executeTask(taskId: string): void { ... }                              |
|          │                                                               |
|          ▼                                                               |
|   Invokes: logged(originalMethod, context)                               |
|   ├── context.kind === "method"                                          |
|   ├── context.name === "executeTask"                                     |
|   ├── context.addInitializer(...)                                        |
|   └── Returns replacement wrapped method:                                |
|       function replacementMethod(this: any, ...args: any[]) {            |
|         console.log("Starting executeTask");                             |
|         return originalMethod.call(this, ...args);                       |
|       }                                                                  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Method Decorator:**
  ```typescript
  function logged<This, Args extends any[], Return>(
    target: (this: This, ...args: Args) => Return,
    context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Return>
  ) {
    const methodName = String(context.name);
    return function (this: This, ...args: Args): Return {
      console.log(`Entering ${methodName}`);
      const result = target.call(this, ...args);
      console.log(`Exiting ${methodName}`);
      return result;
    };
  }
  ```
- **Auto-Accessor Decorator:**
  ```typescript
  function loggedAccessor<This, Value>(
    target: ClassAccessorDecoratorTarget<This, Value>,
    context: ClassAutoAccessorDecoratorContext<This, Value>
  ): ClassAccessorDecoratorResult<This, Value> {
    return {
      get(this: This): Value {
        console.log(`Reading ${String(context.name)}`);
        return target.get.call(this);
      },
      set(this: This, val: Value): void {
        console.log(`Writing ${String(context.name)}: ${val}`);
        target.set.call(this, val);
      }
    };
  }
  ```
- **tsconfig Dependencies:**
  - Set `"experimentalDecorators": false` (or omit) to use modern Stage 3 decorators.
  - Set `target: "ES2022"` and `lib: ["ES2022", "DOM", "ESNext.Decorators"]`.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Author a method execution timing decorator using Stage 3 standards.
- **Code:**
```typescript
function timeExecution<This, Args extends any[], Return>(
  target: (this: This, ...args: Args) => Return,
  context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Return>
) {
  const name = String(context.name);

  return function (this: This, ...args: Args): Return {
    const start = performance.now();
    try {
      return target.call(this, ...args);
    } finally {
      const elapsed = performance.now() - start;
      console.log(`[BENCHMARK] ${name} took ${elapsed.toFixed(3)}ms`);
    }
  };
}

class ComputationalService {
  @timeExecution
  computePrimes(limit: number): number {
    let count = 0;
    for (let i = 2; i <= limit; i++) {
      let isPrime = true;
      for (let j = 2; j * j <= i; j++) {
        if (i % j === 0) { isPrime = false; break; }
      }
      if (isPrime) count++;
    }
    return count;
  }
}

const svc = new ComputationalService();
svc.computePrimes(10000);
```
- **Output / Type Evaluation:**
```typescript
// [BENCHMARK] computePrimes took 1.420ms
```
- **Explanation:** The method decorator wraps execution, timing the duration cleanly.

### Example 2: Real-World Enterprise Pattern (Method Autobind Decorator)
- **Goal:** Bind class methods to instance automatically, eliminating `.bind(this)` bugs.
- **Code:**
```typescript
function bound<This, Args extends any[], Return>(
  _target: (this: This, ...args: Args) => Return,
  context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Return>
) {
  const methodName = context.name;

  context.addInitializer(function (this: any) {
    // addInitializer runs when each instance is constructed!
    this[methodName] = this[methodName].bind(this);
  });
}

class ClickHandler {
  public count = 0;

  @bound
  handleClick(): void {
    this.count++;
    console.log('Clicked! Count:', this.count);
  }
}

const handler = new ClickHandler();
// Extract method without calling it on handler:
const detached = handler.handleClick;
detached(); // Works! Count: 1
detached(); // Works! Count: 2
```
- **Output / Type Evaluation:**
```typescript
// Clicked! Count: 1, Clicked! Count: 2
```
- **Explanation:** `context.addInitializer()` binds the method to each instance at instantiation time, allowing detached usage without arrow functions.

### Example 3: Edge Case / Boundary Condition (Stage 3 Decorator Metadata)
- **Goal:** Collect role-based access control (RBAC) metadata in TypeScript 5.2+.
- **Code:**
```typescript
function Roles(...roles: string[]) {
  return function <This, Args extends any[], Return>(
    _target: (this: This, ...args: Args) => Return,
    context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Return>
  ) {
    // context.metadata is available in TS 5.2+
    context.metadata[context.name] = roles;
  };
}

class AdminPanel {
  @Roles('ADMIN', 'SUPERUSER')
  deleteUser(userId: string): void {
    console.log('User deleted:', userId);
  }
}

// Inspect metadata from constructor Symbol.metadata:
const metadata = (AdminPanel as any)[Symbol.metadata];
console.log('deleteUser required roles:', metadata?.deleteUser); // ['ADMIN', 'SUPERUSER']
```
- **Output / Type Evaluation:**
```typescript
// ['ADMIN', 'SUPERUSER']
```
- **Explanation:** TypeScript 5.2 Decorator Metadata attaches metadata directly to `Symbol.metadata`, replacing external `reflect-metadata` packages.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate error when applying a method decorator to a non-matching signature.
- **Code:**
```typescript
function stringMethodOnly(
  target: (this: any, arg: string) => void,
  context: ClassMethodDecoratorContext<any, (this: any, arg: string) => void>
) {
  return target;
}

class WorkerService {
  // @ts-expect-error TS1270: Decorator function return type is not assignable to method type.
  @stringMethodOnly
  numericAction(count: number): void {
    console.log(count);
  }
}
```
- **Real Compiler Error:**
```
error TS1270: Decorator function return type is not assignable to method type.
  Types of parameters 'arg' and 'count' are incompatible.
    Type 'number' is not assignable to type 'string'.
```
- **Explanation:** Stage 3 decorators enforce strict type assignability between the target member and the decorator signature.

### Example 5: Idiomatic Modern Usage (Auto-Accessor Validation Decorator)
- **Goal:** Intercept and validate an auto-accessor property.
- **Code:**
```typescript
function positiveNumber<This>(
  target: ClassAccessorDecoratorTarget<This, number>,
  context: ClassAutoAccessorDecoratorContext<This, number>
): ClassAccessorDecoratorResult<This, number> {
  const propName = String(context.name);

  return {
    get(this: This): number {
      return target.get.call(this);
    },
    set(this: This, value: number): void {
      if (value < 0) {
        throw new RangeError(`Property '${propName}' must be non-negative`);
      }
      target.set.call(this, value);
    }
  };
}

class Inventory {
  @positiveNumber
  accessor stock: number = 10;
}

const inv = new Inventory();
inv.stock = 25; // OK
console.log('Stock:', inv.stock); // 25
```
- **Output / Type Evaluation:**
```typescript
// Intercepts reads and writes on auto-accessors with full type safety.
```
- **Explanation:** Auto-accessor decorators return replacement `get` and `set` functions, encapsulating validation logic cleanly.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Legacy vs Stage 3 Conflicts:** If `"experimentalDecorators": true` is set in `tsconfig.json`, TypeScript uses the legacy 2015 decorator spec (which passes 3 arguments `(target, propertyKey, descriptor)`). Ensure `experimentalDecorators` is `false` or omitted when writing Stage 3 decorators.
2. **Field Decorators Cannot Change Values Directly:** A field decorator returns an initializer function `(initialValue: T) => T`. It cannot wrap property access like an accessor decorator. Use `accessor` if you need getter/setter interception.
3. **Execution vs Application Order:** In `@A @B method()`, decorator expressions `A` and `B` evaluate top-down, but the decorators themselves wrap the method bottom-up (`A(B(method))`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Mixing legacy and Stage 3 signatures | Parameter type mismatch errors | Set `experimentalDecorators: false` | Aligns with standard ECMAScript |
| Using field decorators for validation | Cannot intercept subsequent assignments | Use `@decorator accessor prop` | Auto-accessors provide full read/write interception |
| Forgetting to call `target.call(this, ...args)` | Lost `this` context inside wrapped method | Always invoke via `target.call(this, ...args)` | Preserves correct instance binding |
| Heavy work during decorator evaluation | Freezes module evaluation on startup | Defer work to `addInitializer` or invocation | Keeps startup time fast |
| Relying on `reflect-metadata` with Stage 3 | Deprecated external dependency | Use `context.metadata` (TS 5.2+) | Native ECMAScript standard metadata |
| Returning new class with mismatched shape | TS1270: Decorator return type incompatible | Return compatible subclass | Preserves class structural contract |

---

## 8. Performance & Type-Checker Complexity
- **Definition-Time Overhead:** Stage 3 decorators run once when the class is defined at module evaluation time. Runtime method invocation overhead is limited to a single function call wrapper.
- **Type Checking Precision:** Stage 3 decorators are fully typed via generics (`This`, `Args`, `Return`), requiring zero `any` casts in the decorator implementation.
- **Isolated Declarations:** Exported decorated classes are fully supported under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** `ts.Decorator` (`SyntaxKind.Decorator`).
- **Standard Library Types:** `ClassDecoratorContext`, `ClassMethodDecoratorContext`, `ClassFieldDecoratorContext`, `ClassAutoAccessorDecoratorContext` defined in `lib.es2022.decorators.d.ts`.
- **Emitter Processing:** Under standard mode, emits standard Stage 3 decorator helper transforms (`__esDecorate`, `__runInitializers`).

---

## 10. Best Practices & Design Guidelines
- **Standardize on Stage 3 Decorators:** Migrate away from `experimentalDecorators` in all new projects.
- **Use Auto-Accessors (`accessor`) for Observable State:** Pair decorators with `accessor` for clean reactive properties.
- **Type `This` and Arguments Generically:** Preserve exact function signatures and avoid `any` in decorator declarations.

---

## 11. Real-World Case Study
- **Scenario:** A financial services API had 120 endpoints. Developers manually wrote audit logging in every controller method: `logger.log("Invoked", ...)`. Over time, 35% of endpoints lacked logging or logged incorrect parameter values, causing audit compliance failures during SOC2 certification.
- **Implementation:** Created a standardized Stage 3 `@AuditLog` decorator:
  ```typescript
  function AuditLog<This, Args extends any[], Return>(
    target: (this: This, ...args: Args) => Return,
    context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Return>
  ) {
    const action = String(context.name);
    return function (this: This, ...args: Args): Return {
      auditPipeline.emit({ action, timestamp: Date.now() });
      return target.call(this, ...args);
    };
  }

  class TransferController {
    @AuditLog
    transferFunds(from: string, to: string, amount: number): void { ... }
  }
  ```
- **Outcome:** Standardized audit logging across 100% of endpoints, reduced controller code by 1,800 lines, and passed the SOC2 audit with zero compliance exceptions.

---

## 12. Interview Questions

### Q1: What are the fundamental differences between legacy experimental decorators and TC39 Stage 3 decorators in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

1. **Specification Status:** Legacy decorators (`experimentalDecorators: true`) implemented an early 2015 proposal. Stage 3 decorators (`experimentalDecorators: false` in TS 5.0+) implement the finalized TC39 standard.
2. **Function Signatures:** Legacy method decorators took three arguments: `(target, propertyKey, descriptor)`. Stage 3 decorators take two arguments: `(target, context)`, where `context` is a strongly typed context object providing `kind`, `name`, `access`, `static`, `private`, and `addInitializer`.
3. **Parameter Decorators:** Legacy decorators supported parameter decorators. Stage 3 decorators do not currently support parameter decorators (deferred to a future proposal).
4. **Metadata:** Legacy decorators relied on `reflect-metadata` and `emitDecoratorMetadata`. Stage 3 decorators use standard `context.metadata` (TS 5.2+).
</details>

### Q2: What is `context.addInitializer()` and why is it useful?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`context.addInitializer()` is a method on the decorator context object that registers a callback to be executed during instance construction (for instance members) or class definition (for static members). It enables decorators to perform per-instance setup (such as binding methods to `this`, subscribing to event streams, or registering with dependency injection containers) without modifying the constructor body directly.
</details>

### Q3: How do Stage 3 decorators support ECMAScript private fields (`#private`)?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Stage 3 decorators natively support private fields and methods (`#field`). The decorator context object provides `context.private: true` and an `access` object containing `get` and `set` functions that have privileged access to read and mutate the private field on any instance, allowing decorators to inspect private state safely.
</details>

### Q4: What is the decorator evaluation order versus execution order?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **Evaluation Order (Outer to Inner / Top to Bottom):** When multiple decorators are applied (`@f @g method()`), the expressions evaluating to the decorator functions are evaluated from top to bottom.
- **Application Order (Inner to Outer / Bottom to Top):** The resulting decorator functions are executed in reverse order: `g` wraps the method first, and `f` wraps the result of `g` (`f(g(method))`).
</details>

### Q5: How do field decorators differ from auto-accessor decorators in Stage 3?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **Field Decorator (`@dec field = 1`):** Receives `undefined` as target. It can only return an initializer function `(initialValue: T) => T` to modify the initial value. It cannot intercept subsequent reads or writes.
- **Auto-Accessor Decorator (`@dec accessor field = 1`):** Receives a target with `get` and `set` functions. It can return replacement `get` and `set` functions, allowing full runtime interception of every property read and assignment.
</details>

### Q6: What does `Symbol.metadata` do in TypeScript 5.2+?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In TypeScript 5.2+, every class decorated with Stage 3 decorators receives a `[Symbol.metadata]` object on the constructor. Decorators write metadata to `context.metadata[key] = value`. The metadata object is inherited down the prototype chain, allowing reflection and runtime frameworks to inspect declared metadata without external polyfills.
</details>

### Q7: Can a class decorator replace the class constructor?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. A Stage 3 class decorator receives the class constructor as `target` and can return a new constructor function (typically a class that `extends target`). The returned class replaces the original constructor in value space.
</details>

### Q8: What compiler error occurs if a decorator return type is incompatible with the decorated member?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The compiler emits `TS1270: Decorator function return type is not assignable to method type`. Stage 3 decorators strictly validate that the function returned by the decorator satisfies the type contract of the target member.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Readonly Method Decorator (Easy)
- **Problem Statement:** Author a method decorator `freezeMethod` that prevents subclasses or external code from overwriting the method on the prototype.
- **Test Case:**
```typescript
class Greeter {
  @freezeMethod
  greet() { return 'Hello'; }
}
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function freezeMethod<This, Args extends any[], Return>(
  target: (this: This, ...args: Args) => Return,
  context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Return>
) {
  context.addInitializer(function () {
    Object.defineProperty(this, context.name, { writable: false, configurable: false });
  });
  return target;
}
```
</details>

### Exercise 2: UpperCase Field Initializer (Easy)
- **Problem Statement:** Write a field decorator `upperCase` that converts string initial field values to uppercase.
- **Test Case:**
```typescript
class User {
  @upperCase
  code = 'admin';
}
console.log(new User().code); // "ADMIN"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function upperCase(
  _target: undefined,
  _context: ClassFieldDecoratorContext<any, string>
) {
  return function (initialValue: string): string {
    return initialValue.toUpperCase();
  };
}
```
</details>

### Exercise 3: Safe Retry Decorator (Medium)
- **Problem Statement:** Implement a method decorator `retry(times: number)` that catches errors and retries execution up to `times` times.
- **Test Case:**
```typescript
class Api {
  @retry(3)
  fetchData() { /* ... */ }
}
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function retry(times: number) {
  return function <This, Args extends any[], Return>(
    target: (this: This, ...args: Args) => Promise<Return>,
    _context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Promise<Return>>
  ) {
    return async function (this: This, ...args: Args): Promise<Return> {
      let lastErr: unknown;
      for (let i = 0; i < times; i++) {
        try {
          return await target.call(this, ...args);
        } catch (err) {
          lastErr = err;
        }
      }
      throw lastErr;
    };
  };
}
```
</details>

### Exercise 4: Auto-Accessor Clamping Decorator (Medium)
- **Problem Statement:** Author an auto-accessor decorator `clamp(min: number, max: number)` that restricts values within bounds.
- **Test Case:**
```typescript
class Volume {
  @clamp(0, 100)
  accessor level = 50;
}
const v = new Volume();
v.level = 150;
console.log(v.level); // 100
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function clamp(min: number, max: number) {
  return function <This>(
    target: ClassAccessorDecoratorTarget<This, number>,
    _context: ClassAutoAccessorDecoratorContext<This, number>
  ): ClassAccessorDecoratorResult<This, number> {
    return {
      get(this: This): number {
        return target.get.call(this);
      },
      set(this: This, val: number): void {
        const clamped = Math.min(max, Math.max(min, val));
        target.set.call(this, clamped);
      }
    };
  };
}
```
</details>

### Exercise 5: Compile-Time Method Decorator Validator (Hard)
- **Problem Statement:** Write a type-level check `ValidMethodDecorator<T>` that verifies if a decorator function conforms to the Stage 3 method decorator signature.
- **Test Case:**
```typescript
type T1 = ValidMethodDecorator<typeof timeExecution>; // true
type T2 = ValidMethodDecorator<(a: number) => void>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type ValidMethodDecorator<D> =
  D extends (
    target: (this: any, ...args: any[]) => any,
    context: ClassMethodDecoratorContext<any, any>
  ) => any
    ? true
    : false;
```
</details>

### Exercise 6: Decorator Metadata Registry Container (Hard)
- **Problem Statement:** Build a route mapping framework using `@Get(path: string)` decorator with TS 5.2+ `context.metadata`, extracting a router map from the class constructor.
- **Test Case:**
```typescript
class Controller {
  @Get('/users')
  getUsers() {}
}
const routes = extractRoutes(Controller);
// [{ path: '/users', method: 'getUsers' }]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function Get(path: string) {
  return function <This, Args extends any[], Return>(
    _target: (this: This, ...args: Args) => Return,
    context: ClassMethodDecoratorContext<This, (this: This, ...args: Args) => Return>
  ) {
    if (!context.metadata.routes) {
      context.metadata.routes = [];
    }
    (context.metadata.routes as any[]).push({ path, method: context.name });
  };
}

function extractRoutes(ctor: any): { path: string; method: string }[] {
  return (ctor[Symbol.metadata]?.routes as any[]) ?? [];
}
```
</details>
