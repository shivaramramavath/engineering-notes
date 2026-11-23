# Function Types

> Function types define callable contracts in TypeScript, encompassing arrow signatures, object call signatures, callable objects with properties, and strict parameter contravariance.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [04-type-aliases.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/04-type-aliases.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 2.6 (`strictFunctionTypes`) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `strictFunctionTypes: true`, `noImplicitAny: true` |

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
- **Problem Statement:** In JavaScript, functions are first-class values: passed as callbacks, returned from higher-order functions, and augmented with attached properties. In untyped systems, passing functions with incompatible argument counts or incorrect parameter expectations causes runtime exceptions.
- **Historical Context:** TypeScript supported function types from version 0.9. Prior to TypeScript 2.6, function parameters were checked bivariantly (both covariant and contravariant) for ergonomic reasons, which allowed unsafe assignments. TypeScript 2.6 introduced `strictFunctionTypes` (now part of `strict: true`), enforcing mathematical contravariance for function parameters.
- **Real-World Relevance:** Modern architectures rely heavily on callbacks, middleware pipelines, and event listeners. Strongly typing function contracts ensures that handlers only receive verified arguments and prevents callers from invoking functions with missing or mismatched data.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Function Type Expressions: `(a: A, b: B) => R` defines the parameter types and return type.
  - Call Signatures: `{ (a: A, b: B): R }` defines callable contracts within object shapes, allowing functions to carry static properties.
  - Parameter Contravariance (`strictFunctionTypes`): A function type `(x: Sub) => void` is NOT assignable to `(x: Super) => void`. The parameter subtyping direction is inverted: `(x: Super) => void` IS assignable to `(x: Sub) => void`.
  - Return Type Covariance: Return types follow standard covariance: `() => Sub` IS assignable to `() => Super`.
  - Parameter Arity Flexibility: A function with fewer parameters is assignable to a callback expecting more parameters (`(a: number) => void` satisfies `(a: number, b: string) => void`).
- **Mental Model:** A function caller guarantees the inputs it passes, but the function implementation requires certain inputs. If the caller provides a `Dog`, the function cannot demand a `GermanShepherd`; it can only demand an `Animal` or a `Dog`.
- **Soundness vs Ergonomics:** Method shorthand syntax in interfaces (`foo(x: string): void`) remains checked bivariantly by design (to support `Array<Derived>` assignable to `Array<Base>`), while function property syntax (`foo: (x: string) => void`) enforces strict contravariance.

---

## 3. How It Works Internally
- **Assignability Checking:** During function assignability checks, `checkTypeRelatedTo` compares each parameter. For parameter $i$, it tests if $TargetParam_i$ is assignable to $SourceParam_i$ (inverting the check direction: contravariance).
- **Contextual Typing:** When a lambda or arrow function is assigned to a typed function slot, the checker propagates types downward, inferring argument types without explicit annotations.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Function Variance in TypeScript 2.6+                    |
|                                                                          |
|       Supertype: Animal  ◄──────── Subtype: Dog                          |
|                                                                          |
|   1. Return Types are COVARIANT (Preserve direction):                    |
|      () => Dog  ──────────────►  () => Animal   (Legal!)                 |
|                                                                          |
|   2. Parameters are CONTRAVARIANT (Invert direction):                    |
|      (a: Animal) => void ─────►  (d: Dog) => void  (Legal!)              |
|      (d: Dog) => void   ───X──►  (a: Animal) => void (REJECTED TS2322)   |
|      (Animal handler safely accepts Dog; Dog handler crashes on Cat!)    |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Function Type Expression:**
  ```typescript
  type BinaryOp = (x: number, y: number) => number;
  ```
- **Object Call Signature (Function with Properties):**
  ```typescript
  type CallableWithState = {
    (query: string): string[];
    cacheSize: number;
    clearCache(): void;
  };
  ```
- **Construct Signatures:**
  ```typescript
  type Constructor<T> = {
    new (...args: any[]): T;
  };
  ```
- **tsconfig Dependencies:**
  - `strictFunctionTypes: true` (included in `strict: true`): Enforces contravariance for function parameters.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Declare and consume function type aliases and contextual typing.
- **Code:**
```typescript
type Formatter = (value: number, currency: string) => string;

// Contextual typing: 'val' and 'curr' automatically inferred as number and string!
const formatPrice: Formatter = (val, curr) => {
  return `${curr} ${val.toFixed(2)}`;
};

console.log(formatPrice(49.9, 'USD')); // "USD 49.90"
```
- **Output / Type Evaluation:**
```typescript
// formatPrice is verified; val and curr require zero manual annotations.
```
- **Explanation:** Function types eliminate boilerplate by contextually typing implementation parameters.

### Example 2: Real-World Enterprise Pattern (Callable Object with Attached Telemetry)
- **Goal:** Model an invokable API client function that also carries metadata and statistics.
- **Code:**
```typescript
export type Fetcher = {
  (url: string, init?: RequestInit): Promise<Response>;
  readonly callCount: number;
  resetTelemetry(): void;
};

export function createMonitoredFetcher(): Fetcher {
  let count = 0;

  const fetcher = async (url: string, init?: RequestInit): Promise<Response> => {
    count++;
    return fetch(url, init);
  };

  return Object.assign(fetcher, {
    get callCount() {
      return count;
    },
    resetTelemetry() {
      count = 0;
    },
  });
}
```
- **Output / Type Evaluation:**
```typescript
// Returns an entity that is both callable and holds properties.
```
- **Explanation:** Call signatures in object types allow modeling JavaScript functions with properties (like Express middleware, Axios, or lodash).

### Example 3: Edge Case / Boundary Condition (Contravariance in Action)
- **Goal:** Demonstrate why parameter contravariance is necessary for type soundness.
- **Code:**
```typescript
class Animal { name = 'Animal'; }
class Dog extends Animal { bark() { console.log('Woof'); } }
class Cat extends Animal { meow() { console.log('Meow'); } }

type AnimalHandler = (a: Animal) => void;
type DogHandler = (d: Dog) => void;

const handleAnimal: AnimalHandler = (a) => console.log(a.name);
const handleDog: DogHandler = (d) => d.bark();

// Legal: AnimalHandler accepts Dog, because a Dog is an Animal!
const dHandler: DogHandler = handleAnimal; // OK!

// Dangerous: If this were legal, we could pass a Cat to handleDog!
// @ts-expect-error TS2322: Type 'AnimalHandler' is not assignable to type 'DogHandler'.
const aHandler: AnimalHandler = handleDog;
```
- **Output / Type Evaluation:**
```typescript
// TS2322 prevents passing a Cat to a handler that expects a Dog!
```
- **Explanation:** Under `strictFunctionTypes`, parameter checking is contravariant: handlers expecting broad types can handle specific types, but not vice-versa.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection when passing excess or incompatible arguments.
- **Code:**
```typescript
type LogFn = (message: string) => void;

function invokeLogger(logger: LogFn): void {
  logger('Operational event');
}

// @ts-expect-error TS2322: Type '(a: number) => void' is not assignable to type 'LogFn'.
const badLogger: LogFn = (msg: number) => console.log(msg);

// @ts-expect-error TS2345: Argument of type 'string' is not assignable to parameter of type 'number'.
badLogger(100);
```
- **Real Compiler Error:**
```
error TS2322: Type '(msg: number) => void' is not assignable to type 'LogFn'.
  Types of parameters 'msg' and 'message' are incompatible.
    Type 'string' is not assignable to type 'number'.
```
- **Explanation:** Incompatible parameter types are rejected during function assignability checks.

### Example 5: Idiomatic Modern Usage (Generic Pipe & Middleware Pipeline)
- **Goal:** Construct a strictly typed middleware pipeline composing function types.
- **Code:**
```typescript
export type Middleware<TContext> = (
  ctx: TContext,
  next: () => Promise<void>
) => Promise<void>;

export class Pipeline<TContext> {
  private stack: Middleware<TContext>[] = [];

  use(fn: Middleware<TContext>): this {
    this.stack.push(fn);
    return this;
  }

  async execute(context: TContext): Promise<void> {
    const run = async (index: number): Promise<void> => {
      if (index >= this.stack.length) return;
      const middleware = this.stack[index]!;
      await middleware(context, () => run(index + 1));
    };
    await run(0);
  }
}
```
- **Output / Type Evaluation:**
```typescript
// End-to-end type safety across asynchronous middleware composition.
```
- **Explanation:** Clean function types power flexible, composable asynchronous architecture without class inheritance.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Method Syntax vs Function Property Syntax:**
   ```typescript
   interface InvariantA { method(x: Dog): void; }   // Bivariant parameter checking!
   interface InvariantB { prop: (x: Dog) => void; } // Strict contravariant checking!
   ```
   Method declarations in interfaces bypass `strictFunctionTypes`. Always use property syntax (`prop: (arg: T) => void`) for strict parameter safety.
2. **Discarding Arguments (Arity Mismatch):** In TypeScript, `(x: number) => void` is assignable to `(x: number, y: string) => void`. This is intentional so that `[1, 2].map(x => x * 2)` is valid without forcing developers to accept all 3 parameters of the map callback (`value`, `index`, `array`).
3. **The `Function` Type Anti-Pattern:** The global type `Function` represents any callable object but accepts `any` arguments and returns `any`. It disables all compile-time checks. Always use `(...args: any[]) => any` or specific signatures.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using the global `Function` type | Complete loss of type safety (`any` returns) | Specific function type `() => void` | Enforces parameter and return contracts |
| Using method syntax in interfaces for callbacks | Bypasses `strictFunctionTypes` (bivariant) | Use property syntax `cb: (x: T) => void` | Enforces mathematical parameter contravariance |
| Re-annotating parameters on typed lambdas | Redundant boilerplate | Rely on contextual typing | Keeps code clean and DRY |
| Expecting exact parameter count matches | Runtime surprises when callbacks ignore args | Document optional arguments explicitly | Aligns with JavaScript callback semantics |
| Assigning `() => Dog` to `() => Cat` | `TS2322: Type 'Dog' not assignable to 'Cat'` | Return correct subtype | Guarantees return type covariance |
| Ignoring Promise return in void callback | Unhandled async rejection in `forEach` | Type async callbacks as `() => Promise<void>` | Ensures async operations are awaited |

---

## 8. Performance & Type-Checker Complexity
- **Arity Truncation Checks:** Comparing function arities is an $O(N)$ comparison across parameter lists.
- **Contextual Typing Caching:** TypeScript caches contextual signatures during AST visitor passes, speeding up nested callback resolution.
- **Isolated Declarations:** Exported functions require explicit parameter and return type annotations under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Node Representation:**
  - Function type expression is `ts.FunctionTypeNode` (`SyntaxKind.FunctionType`).
  - Call signature in object literal is `ts.CallSignatureDeclaration` (`SyntaxKind.CallSignature`).
- **Symbol & Signature:** The type checker creates a `Signature` object in `checker.ts` containing `parameters: Symbol[]` and `resolvedReturnType: Type`.
- **Contravariance Logic:** In `checkSignaturesRelatedTo`, parameter comparisons pass `RelationFlags.Contravariant` when `strictFunctionTypes` is enabled.

---

## 10. Best Practices & Design Guidelines
- **Always Use Property Syntax for Function Contracts:** Prefer `handler: (event: E) => void` over `handler(event: E): void` in interfaces to preserve contravariance.
- **Leverage Contextual Typing:** Do not manually annotate callback parameters when assigning to typed function signatures.
- **Avoid the Global `Function` Type:** Never use `Function` as a type annotation; use `(...args: unknown[]) => unknown` if modeling an arbitrary callable.

---

## 11. Real-World Case Study
- **Scenario:** An analytics pipeline collected event listeners for user click events. An interface defined `interface Listener { onEvent(e: MouseClickEvent): void }`. An engineer passed a general handler `onEvent(e: KeyboardEvent)`. Because method syntax in interfaces uses bivariant checking, TypeScript compiled without error. In production, reading `e.clientX` crashed background worker threads with `TypeError: Cannot read properties of undefined`.
- **Implementation:** Refactored the contract to function property syntax:
  ```typescript
  // Before (bivariant method syntax):
  interface Listener {
    onEvent(e: MouseClickEvent): void;
  }

  // After (strict contravariant property syntax):
  interface Listener {
    readonly onEvent: (e: MouseClickEvent) => void;
  }
  ```
- **Outcome:** The compiler immediately raised `TS2322: Type '(e: KeyboardEvent) => void' is not assignable to type '(e: MouseClickEvent) => void'`, catching the event mismatch during compilation.

---

## 12. Interview Questions

### Q1: What is the difference between covariance and contravariance in function types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **Covariance (Return Types):** Types maintain their subtyping direction. If `Dog` extends `Animal`, then a function returning a `Dog` (`() => Dog`) is assignable to a function returning an `Animal` (`() => Animal`). The caller expects an `Animal`, and a `Dog` satisfies all properties of `Animal`.
- **Contravariance (Parameter Types):** Types invert their subtyping direction. A function accepting an `Animal` (`(a: Animal) => void`) is assignable to a function accepting a `Dog` (`(d: Dog) => void`). The caller passes a `Dog`, which the `Animal` handler can safely process. However, a function accepting only a `Dog` CANNOT be assigned to an `Animal` parameter, because the caller might pass a `Cat`.
</details>

### Q2: Why does TypeScript check interface methods bivariantly while function properties are checked contravariantly?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When TypeScript 2.6 introduced `strictFunctionTypes`, applying contravariance to interface methods broke widespread JavaScript idioms—most notably `Array<Derived>` being assignable to `Array<Base>`. In `Array<T>`, methods like `push(...items: T[]): number` have `T` in parameter positions. If methods were contravariant, `Array<Dog>` could not be assigned to `Array<Animal>`. To balance soundness with real-world JavaScript ergonomics, method shorthand syntax (`method(): void`) remains bivariant, while property syntax (`prop: () => void`) enforces strict contravariance.
</details>

### Q3: Why does TypeScript permit passing a function with fewer parameters to a callback expecting more parameters?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

JavaScript functions regularly ignore unused arguments. For example, `Array.prototype.forEach` passes three arguments to its callback: `(item, index, array)`. If TypeScript enforced strict parameter arity matching, writing `[1, 2, 3].forEach(x => console.log(x))` would fail compilation because `x => console.log(x)` only accepts one argument. Allowing functions with fewer parameters preserves idiomatic JavaScript while remaining sound, as ignoring passed arguments causes no runtime harm.
</details>

### Q4: How do you type a function that has static properties attached to it?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Using an object type with a call signature:
```typescript
type CounterFunction = {
  (): number;
  count: number;
  reset(): void;
};
```
This describes a value that can be called like a function (`fn()`) and has properties accessed via dot notation (`fn.count`, `fn.reset()`).
</details>

### Q5: What is the difference between a function type expression and a call signature?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- Function type expression: `type Fn = (x: number) => string;` (arrow syntax, cannot hold extra object properties).
- Call signature: `type Fn = { (x: number): string; };` (declared inside curly braces, can be combined with other properties or additional call signatures to model overloading).
</details>

### Q6: What does `strictFunctionTypes` do in `tsconfig.json`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

It enables strict contravariant parameter checking for function types. When disabled (or for method declarations), function parameters are checked bivariantly, which can allow unsound assignments. It is automatically enabled when `strict: true` is set.
</details>

### Q7: Why is the global type `Function` considered harmful in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The global `Function` interface accepts any arguments (`...args: any`) and returns `any`. Using it as a type annotation turns off all compile-time argument checks and treats the return value as unverified `any`, reintroducing runtime errors.
</details>

### Q8: How does contextual typing work when passing an inline arrow function?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When an expression appears in a location that has an expected type (the "contextual type"), TypeScript infers the types of the parameters from that context. For example, in `window.addEventListener('click', e => ...)`, the second parameter of `addEventListener` is typed as `EventListener`, contextually typing `e` as `MouseEvent`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Contextual Math Function (Easy)
- **Problem Statement:** Declare a function type `Reducer<T>` that takes an accumulator and a current value of type `T` and returns a new accumulator of type `T`.
- **Test Case:**
```typescript
const sumReducer: Reducer<number> = (acc, val) => acc + val;
sumReducer(10, 5); // 15
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type Reducer<T> = (accumulator: T, currentValue: T) => T;
```
</details>

### Exercise 2: Callable With Property Implementation (Easy)
- **Problem Statement:** Implement a function matching `type TaggedLogger = { (msg: string): void; prefix: string; }`.
- **Test Case:**
```typescript
const log = createLogger('[APP]');
log('Booted'); // "[APP] Booted"
console.log(log.prefix); // "[APP]"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type TaggedLogger = {
  (msg: string): void;
  prefix: string;
};

function createLogger(prefix: string): TaggedLogger {
  const fn = (msg: string) => console.log(`${prefix} ${msg}`);
  fn.prefix = prefix;
  return fn;
}
```
</details>

### Exercise 3: Strict Contravariant Dispatcher (Medium)
- **Problem Statement:** Create an event dispatcher that stores handlers in an array using contravariant property syntax to prevent assigning incompatible handlers.
- **Test Case:**
```typescript
type EventListener<E> = { readonly handle: (event: E) => void };
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type EventListener<E> = {
  readonly handle: (event: E) => void;
};

class EventBus<E> {
  private listeners: EventListener<E>[] = [];

  subscribe(listener: EventListener<E>): void {
    this.listeners.push(listener);
  }

  publish(event: E): void {
    for (const l of this.listeners) {
      l.handle(event);
    }
  }
}
```
</details>

### Exercise 4: Async Callback Pipeline Composer (Medium)
- **Problem Statement:** Author a function `composeAsync` that takes an array of unary async functions `(val: T) => Promise<T>` and returns a single function running them sequentially.
- **Test Case:**
```typescript
const pipeline = composeAsync<number>([
  async (x) => x + 1,
  async (x) => x * 2,
]);
await pipeline(5); // 12
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function composeAsync<T>(
  fns: ((val: T) => Promise<T>)[]
): (initial: T) => Promise<T> {
  return async (initial: T) => {
    let current = initial;
    for (const fn of fns) {
      current = await fn(current);
    }
    return current;
  };
}
```
</details>

### Exercise 5: Compile-Time Arity Extractor (Hard)
- **Problem Statement:** Write a utility type `ParametersCount<T>` that extracts the exact number of required parameters of a function as a literal number.
- **Test Case:**
```typescript
type F1 = (a: string, b: number) => void;
type C1 = ParametersCount<F1>; // 2
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type ParametersCount<T extends (...args: any[]) => any> =
  Parameters<T>['length'];
```
</details>

### Exercise 6: Curried Signature Generator (Hard)
- **Problem Statement:** Write a type `Curry2<T>` that takes a binary function `(a: A, b: B) => R` and transforms it into `(a: A) => (b: B) => R`.
- **Test Case:**
```typescript
type Add = (a: number, b: number) => number;
type CurriedAdd = Curry2<Add>; // (a: number) => (b: number) => number
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type Curry2<Fn> = Fn extends (a: infer A, b: infer B) => infer R
  ? (a: A) => (b: B) => R
  : never;
```
</details>
