# Any and Unknown

> `any` completely disables TypeScript's type-checker for a value, introducing unchecked runtime vulnerabilities, while `unknown` represents a type-safe top type requiring explicit narrowing or validation before any operation is permitted.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [01-primitive-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/01-primitive-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 3.0 (`unknown`) / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true`, `noUncheckedIndexedAccess: true` |

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
- **Problem Statement:** Real-world applications ingest unverified data from external boundaries — HTTP requests, WebSocket messages, LocalStorage, third-party JavaScript libraries. In early TypeScript, developers typed unverified inputs as `any`, which turned off compiler checking and allowed unverified property access, causing `TypeError: Cannot read properties of undefined` in production.
- **Historical Context:** TypeScript 1.0 launched with `any` as an escape hatch for migrating untyped JavaScript. Recognizing that `any` leaks unchecked operations transitively through codebases, TypeScript 3.0 introduced `unknown` as a type-safe counterpart.
- **Real-World Relevance:** In modern production systems, `any` is treated as a code smell (flagged by typescript-eslint `@typescript-eslint/no-explicit-any`). `unknown` enforces boundary validation: data cannot be consumed until it passes a runtime type guard, Zod/ArkType schema, or control flow check.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Both `any` and `unknown` are top types in the TypeScript type lattice; every type is assignable to both `any` and `unknown`.
  - `any` is also a bottom type (except for `never`); `any` is assignable to almost every other type, subverting subtyping rules.
  - `unknown` is strictly a top type; it is assignable to nothing except `unknown` and `any` without explicit narrowing.
- **Mental Model:** Think of `any` as a wildcard that says: "Trust me, turn off the type checker." Think of `unknown` as a sealed black box: "This value exists, but you are not allowed to inspect its properties, call it, or construct it until you inspect its shape at runtime."
- **Soundness vs Ergonomics:** `any` provides maximum migration ergonomics at the cost of zero soundness. `unknown` preserves 100% mathematical soundness by forcing developers to write runtime guards before operating on values.

---

## 3. How It Works Internally
- **Type Checker Traversal:** When encountering `any`, the type checker's `isTypeAssignableTo` returns `true` immediately for any destination type. When encountering `unknown`, assignability fails unless the destination type is `unknown` or `any`.
- **Control Flow Analysis (CFA):** Narrowing checks (`typeof`, `instanceof`, custom type predicates) refine an `unknown` variable to a specific subtype within the current control flow block.
- **Architectural Flow Diagram:**

```
+-------------------------------------------------------------------------+
|                    Top Types in the TypeScript Lattice                  |
|                                                                         |
|                [ any ]                       [ unknown ]                |
|            (Escape Hatch)                  (Safe Top Type)              |
|             /          \                          |                     |
|            /            \                         ▼                     |
|    Assignable To:    Permits:              Assignable ONLY To:          |
|    ALL Types         ALL Operations        `unknown` & `any`            |
|    (Bypasses type    (Silently propagates        │                      |
|     checker)          runtime errors)            ▼                      |
|                                            Requires Narrowing           |
|                                            (typeof, instanceof, guards) |
|                                                  │                      |
|                                                  ▼                      |
|                                            Refined Concrete Type        |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Declaration:**
  ```typescript
  let untyped: any;
  let safeInput: unknown;
  ```
- **Union & Intersection Behavior:**
  - Union with `unknown`: `T | unknown` evaluates to `unknown` (absorptive top type).
  - Intersection with `unknown`: `T & unknown` evaluates to `T` (identity element in intersection).
  - Union with `any`: `T | any` evaluates to `any` (absorptive).
  - Intersection with `any`: `T & any` evaluates to `any` (infectious).
- **tsconfig Dependencies:**
  - `noImplicitAny: true`: Mandates explicit type annotations whenever the compiler cannot infer a non-any type.
  - `useUnknownInCatchVariables: true` (included in `strict: true` since TS 4.4): Types `catch(e)` clause error variables as `unknown` rather than `any`.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compare operations permitted on `any` vs `unknown`.
- **Code:**
```typescript
function handleAny(input: any): void {
  // Unchecked access - compiles without warning, may crash at runtime!
  console.log(input.name.toUpperCase());
  input.executeOperation();
}

function handleUnknown(input: unknown): void {
  // Type narrowing required:
  if (typeof input === 'object' && input !== null && 'name' in input) {
    const candidate = input as { name: unknown };
    if (typeof candidate.name === 'string') {
      console.log(candidate.name.toUpperCase());
    }
  }
}
```
- **Output / Type Evaluation:**
```typescript
// handleUnknown guarantees zero runtime TypeErrors on unexpected inputs.
```
- **Explanation:** `unknown` enforces structural verification before property traversal.

### Example 2: Real-World Enterprise Pattern (Type-Safe JSON Parser)
- **Goal:** Author a production-grade JSON parsing utility that avoids `any` leaks.
- **Code:**
```typescript
export function parseSafeJSON(jsonString: string): unknown {
  return JSON.parse(jsonString);
}

// User domain schema
type UserPayload = {
  id: string;
  email: string;
};

function isUserPayload(value: unknown): value is UserPayload {
  return (
    typeof value === 'object' &&
    value !== null &&
    'id' in value &&
    typeof (value as Record<string, unknown>).id === 'string' &&
    'email' in value &&
    typeof (value as Record<string, unknown>).email === 'string'
  );
}

function processUserData(rawJson: string): UserPayload | null {
  const parsed = parseSafeJSON(rawJson);
  if (isUserPayload(parsed)) {
    return parsed; // Successfully narrowed from unknown to UserPayload
  }
  return null;
}
```
- **Output / Type Evaluation:**
```typescript
// Returns verified UserPayload with zero unsafe assertions.
```
- **Explanation:** Standard `JSON.parse()` returns `any` in TypeScript standard library definitions. Wrapping it to return `unknown` protects downstream business logic.

### Example 3: Edge Case / Boundary Condition (Catch Clause with useUnknownInCatchVariables)
- **Goal:** Safely handle exceptions in try/catch blocks without assuming `Error` instances.
- **Code:**
```typescript
function extractErrorMessage(error: unknown): string {
  if (error instanceof Error) {
    return error.message;
  }
  if (typeof error === 'string') {
    return error;
  }
  if (typeof error === 'object' && error !== null && 'message' in error) {
    return String((error as Record<string, unknown>).message);
  }
  return 'Unknown runtime fault occurred';
}

async function executeNetworkCall(): Promise<void> {
  try {
    throw { code: 504, message: 'Gateway Timeout' };
  } catch (err: unknown) {
    const message = extractErrorMessage(err);
    console.error('Handled fault:', message);
  }
}
```
- **Output / Type Evaluation:**
```typescript
// Handled fault: Gateway Timeout
```
- **Explanation:** In JavaScript, any value can be thrown (`throw "string"`, `throw 404`, `throw null`). Typing catch variables as `unknown` prevents assuming `err.message` exists.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection when attempting unverified access on `unknown`.
- **Code:**
```typescript
const rawPayload: unknown = { token: 'secret_123', count: 42 };

// @ts-expect-error TS2571: Object is of type 'unknown'.
console.log(rawPayload.token);

// @ts-expect-error TS2571: Object is of type 'unknown'.
rawPayload();

// @ts-expect-error TS2322: Type 'unknown' is not assignable to type 'string'.
const tokenString: string = rawPayload;
```
- **Real Compiler Error:**
```
error TS2571: 'rawPayload' is of type 'unknown'.
error TS2571: 'rawPayload' is of type 'unknown'.
error TS2322: Type 'unknown' is not assignable to type 'string'.
```
- **Explanation:** The compiler strictly blocks property access, invocation, and assignment on `unknown` until refined.

### Example 5: Idiomatic Modern Usage (Generic Boundary Function with Type Predicate)
- **Goal:** Implement an unverified payload validator using modern TypeScript generics and custom type guards.
- **Code:**
```typescript
export function assertShape<T>(
  value: unknown,
  guard: (val: unknown) => val is T,
  errorMessage: string
): asserts value is T {
  if (!guard(value)) {
    throw new TypeError(errorMessage);
  }
}

function isValidConfig(val: unknown): val is { port: number; host: string } {
  return (
    typeof val === 'object' &&
    val !== null &&
    'port' in val &&
    typeof (val as Record<string, unknown>).port === 'number' &&
    'host' in val &&
    typeof (val as Record<string, unknown>).host === 'string'
  );
}

function bootService(rawConfig: unknown): void {
  assertShape(rawConfig, isValidConfig, 'Invalid server configuration format');
  // rawConfig is narrowed to { port: number; host: string }
  console.log(`Server starting at ${rawConfig.host}:${rawConfig.port}`);
}
```
- **Output / Type Evaluation:**
```typescript
// Verified compilation and narrowing via assertion signature.
```
- **Explanation:** Assertion functions allow transitioning cleanly from `unknown` to concrete domain types without type pollution.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Transitive Infection of `any`:** Passing an `any` into a generic function often causes the return type to be inferred as `any`, silently contaminating downstream functions.
2. **`any` Masks Typos in Property Names:** Accessing `user.prfile` on an `any` produces no compiler warning, deferring discovery to production runtime.
3. **Double Assertions (`as unknown as T`):** When two types do not overlap, TypeScript rejects `x as T`. Developers frequently work around this via `x as unknown as T`. This bypasses compiler verification and should be restricted to verified low-level primitives or unit tests.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using `any` for API response payloads | Runtime `TypeError: Cannot read properties of undefined` | Use `unknown` + Zod / TypeGuard | Guarantees external data matches expected schema |
| Casting directly with `data as SpecificType` | Bypasses compiler checking; crashes if shape changes | Type predicate function (`val is T`) | Validates shape at runtime before execution |
| `catch (err: any)` in exception handlers | `TypeError: Cannot read property 'message' of undefined` | `catch (err: unknown)` + `instanceof Error` | Safely handles non-Error thrown values |
| Leaving parameters untyped with `noImplicitAny: false` | Silent compiler degradation across whole project | Enable `noImplicitAny: true` | Enforces complete type coverage across modules |
| Using `any` in unit tests for mock objects | Mock drift; tests pass while production code fails | Use `Partial<T>` or type-safe mockers | Keeps test mocks aligned with actual contracts |
| Intersecting `T & any` | Silently degrades entire type to `any` | Intersect with `Record<string, unknown>` | Retains structural constraints |

---

## 8. Performance & Type-Checker Complexity
- **Fast-Path Bailout on `any`:** The TypeScript checker evaluates operations on `any` using an instant short-circuit. While this speeds up compilation marginally, it eliminates all safety guarantees.
- **Narrowing Overhead on `unknown`:** Flow-sensitive narrowing of `unknown` variables creates control-flow nodes in the checker. This has $O(1)$ complexity per branch and introduces negligible compilation cost.
- **Isolated Declarations:** Exported functions returning `any` or `unknown` are trivially compatible with `--isolatedDeclarations` because their types require zero cross-file inference.

---

## 9. Compiler Internals
- **Type Flags:**
  - `any` is represented by `TypeFlags.Any`.
  - `unknown` is represented by `TypeFlags.Unknown`.
- **Top-Type Semantics:** In `src/compiler/checker.ts`, the checker function `isTypeRelatedTo` checks:
  ```typescript
  if (target.flags & TypeFlags.AnyOrUnknown) {
      return true; // Any source type is assignable to unknown or any
  }
  if (source.flags & TypeFlags.Any) {
      return true; // any source is assignable to almost any target!
  }
  ```
- **AST Representation:** Parsed as `ts.KeywordTypeNode` with `SyntaxKind.AnyKeyword` or `SyntaxKind.UnknownKeyword`.

---

## 10. Best Practices & Design Guidelines
- **Ban Explicit `any`:** Enable ESLint rule `@typescript-eslint/no-explicit-any: "error"`.
- **Default Boundary Inputs to `unknown`:** All external I/O (network responses, message queues, JSON parses) should enter your application typed as `unknown`.
- **Narrow at Boundaries, Consume Concrete Types Internally:** Perform validation immediately upon ingestion so internal domain functions only deal with strict, concrete types.

---

## 11. Real-World Case Study
- **Scenario:** A healthcare analytics dashboard ingested patient telemetry via WebSocket streams. The message handler used `event: any`. A backend schema change renamed `bloodPressure` to `systolicBloodPressure`. The dashboard rendered blank cards, and analytics pipelines recorded `NaN`, resulting in undetected monitoring outages.
- **Implementation:** Re-architected ingestion using `unknown` and a strict runtime validator:
  ```typescript
  function handleTelemetryStream(rawMessage: unknown): void {
    if (!isTelemetryEvent(rawMessage)) {
      telemetryAlertPipeline.reportCorruptFrame(rawMessage);
      return;
    }
    // Safe and fully typed telemetry event processing
    renderVitalSigns(rawMessage.vitals);
  }
  ```
- **Outcome:** The schema regression was caught immediately in staging tests with clear schema error logs; zero silent `NaN` corrupted frames reached production databases.

---

## 12. Interview Questions

### Q1: Why is `unknown` called a type-safe top type while `any` is not?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Both `any` and `unknown` are top types because any value can be assigned to them. However, `any` acts simultaneously as a bottom type (it can be assigned to any destination without checks) and disables the compiler's safety checks, allowing arbitrary property accesses and calls. `unknown` is type-safe because nothing can be done with an `unknown` value until it is narrowed through runtime type checks or assertions, preventing unsafe operations.
</details>

### Q2: How does the type checker resolve `any & unknown` vs `any | unknown`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `any | unknown` evaluates to `any`. Because `any` is infectious, in a union between `any` and `unknown`, the compiler preserves the permissive behavior of `any`.
- `any & unknown` also evaluates to `any`. Since `unknown` is the identity type for intersections (`T & unknown === T`), substituting `any` for `T` yields `any`.
</details>

### Q3: When is the use of `any` ever justified in modern TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Valid uses of `any` are rare and specialized:
1. Migration of massive legacy JavaScript codebases where typing everything upfront is economically infeasible.
2. Writing compiler tests or negative unit tests explicitly verifying compiler diagnostic generation.
3. Complex generic conditional types where intermediate inference produces unavoidable compiler limitations, though `any` should be constrained within the internal implementation and never exposed in the public signature.
</details>

### Q4: Why did TypeScript 4.4 switch catch variables from `any` to `unknown` under `strict`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, any expression can be thrown: `throw "error"`, `throw 500`, or `throw undefined`. Typing catch variables as `any` led developers to write `catch(e) { console.log(e.message) }`, causing unhandled exceptions when non-Error primitives were thrown. Under `useUnknownInCatchVariables`, `catch(e)` is `unknown`, forcing developers to verify `e instanceof Error` or narrow before reading properties.
</details>

### Q5: What is the difference between `unknown` and `never`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`unknown` is the top type of the type hierarchy (the set of all possible JavaScript values); every value is assignable to `unknown`. `never` is the bottom type (the empty set containing zero values); no value can be assigned to `never`, but `never` is assignable to every other type.
</details>

### Q6: Can you index an object with an `unknown` key?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. Object keys in JavaScript must be `string`, `number`, or `symbol`. Attempting `obj[key]` where `key` is of type `unknown` produces `TS2538: Type 'unknown' cannot be used as an index type`. The key must first be narrowed to `string | number | symbol`.
</details>

### Q7: How does `unknown` behave in generic constraints (`<T extends unknown>`)?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In TSX files, writing `<T>(x: T) => x` causes syntax ambiguity with JSX elements. Writing `<T extends unknown>(x: T) => x` clarifies to the parser that `<T...>` is a generic type parameter rather than an opening JSX tag. Since every type extends `unknown`, the constraint imposes no operational restrictions on `T`.
</details>

### Q8: What is the risk of using `as unknown as TargetType` (double assertion)?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript normally forbids casting between types that have no structural overlap (e.g. `string as number`). Casting to `unknown` first strips all structural constraints, allowing a cast to any arbitrary target type. This completely breaks compile-time guarantees, creating silent runtime failures if the underlying memory representation does not match the target type.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Property Extraction (Easy)
- **Problem Statement:** Write a function `getOptionalString(obj: unknown, key: string): string | undefined` that safely reads a string property from an unverified object.
- **Test Case:**
```typescript
getOptionalString({ name: 'Alice' }, 'name'); // "Alice"
getOptionalString({ name: 123 }, 'name'); // undefined
getOptionalString(null, 'name'); // undefined
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function getOptionalString(obj: unknown, key: string): string | undefined {
  if (typeof obj === 'object' && obj !== null && key in obj) {
    const val = (obj as Record<string, unknown>)[key];
    if (typeof val === 'string') {
      return val;
    }
  }
  return undefined;
}
```
</details>

### Exercise 2: Safe Array Element Filter (Easy)
- **Problem Statement:** Implement a function `filterNumbers(arr: unknown): number[]` that accepts an unknown input and returns an array of numbers if the input is an array, filtering out non-number elements.
- **Test Case:**
```typescript
filterNumbers([1, 'a', 2, null, 3]); // [1, 2, 3]
filterNumbers('not an array'); // []
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function filterNumbers(arr: unknown): number[] {
  if (!Array.isArray(arr)) return [];
  return arr.filter((item): item is number => typeof item === 'number');
}
```
</details>

### Exercise 3: Strict Deep Unknown Normalizer (Medium)
- **Problem Statement:** Write a type `DeepUnknown<T>` that converts all primitive properties of a type to `unknown`, while preserving object and array structural nesting.
- **Test Case:**
```typescript
type User = { id: string; meta: { tags: string[]; age: number } };
type UncheckedUser = DeepUnknown<User>;
// { id: unknown; meta: { tags: unknown[]; age: unknown } }
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type DeepUnknown<T> = T extends Function
  ? unknown
  : T extends Array<infer _U>
  ? DeepUnknown<_U>[]
  : T extends object
  ? { [K in keyof T]: DeepUnknown<T[K]> }
  : unknown;
```
</details>

### Exercise 4: Type-Safe Request Dispatcher (Medium)
- **Problem Statement:** Build a request router that takes an `unknown` message payload and dispatches it to registered typed route handlers based on a `type` string discriminant.
- **Test Case:**
```typescript
type Handler<T> = (data: T) => void;
const router = new SafeRouter();
router.register('PING', (data: { timestamp: number }) => console.log(data.timestamp));
router.dispatch({ type: 'PING', data: { timestamp: Date.now() } });
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class SafeRouter {
  private handlers = new Map<string, (data: unknown) => void>();

  register<T>(type: string, handler: (data: T) => void): void {
    this.handlers.set(type, handler as (data: unknown) => void);
  }

  dispatch(raw: unknown): boolean {
    if (typeof raw === 'object' && raw !== null && 'type' in raw && 'data' in raw) {
      const type = String((raw as Record<string, unknown>).type);
      const handler = this.handlers.get(type);
      if (handler) {
        handler((raw as Record<string, unknown>).data);
        return true;
      }
    }
    return false;
  }
}
```
</details>

### Exercise 5: Compile-Time `IsAny<T>` Detector (Hard)
- **Problem Statement:** Write a utility type `IsAny<T>` that evaluates to `true` if `T` is `any`, and `false` for all other types including `unknown` and `never`.
- **Test Case:**
```typescript
type T1 = IsAny<any>; // true
type T2 = IsAny<unknown>; // false
type T3 = IsAny<never>; // false
type T4 = IsAny<string>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
// Relies on the unique behavior: 0 extends (1 & any) evaluates to true
type IsAny<T> = 0 extends 1 & T ? true : false;
```
</details>

### Exercise 6: Compile-Time `IsUnknown<T>` Detector (Hard)
- **Problem Statement:** Write a utility type `IsUnknown<T>` that evaluates to `true` if and only if `T` is `unknown`, returning `false` for `any`, `never`, and other types.
- **Test Case:**
```typescript
type R1 = IsUnknown<unknown>; // true
type R2 = IsUnknown<any>; // false
type R3 = IsUnknown<never>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type IsUnknown<T> = IsAny<T> extends true
  ? false
  : [unknown] extends [T]
  ? true
  : false;
```
</details>
