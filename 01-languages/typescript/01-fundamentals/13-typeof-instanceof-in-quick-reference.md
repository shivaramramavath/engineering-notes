# Typeof, Instanceof, and In Quick Reference

> Runtime type guards—`typeof`, `instanceof`, and the `in` operator—form the foundational primitives of TypeScript Control Flow Analysis, bridging dynamic JavaScript inspections with compile-time type narrowing.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [12-truthiness-and-equality-narrowing.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/12-truthiness-and-equality-narrowing.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.0 (`typeof`, `instanceof`) / TS 2.7 (`in` operator) / TS 4.9 (`in` unconstrained narrowing) / TS 7.0 Native |
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
- **Problem Statement:** In JavaScript applications, functions frequently accept polymorphic arguments or handle heterogenous payloads. Without runtime introspection, code is vulnerable to `TypeError` when accessing properties or methods that exist on only one variant of an input.
- **Historical Context:** JavaScript has native runtime introspection operators: `typeof` (for primitives), `instanceof` (for prototypes), and `in` (for property existence). TypeScript took these runtime operators and gave them compile-time semantics: when written inside conditional statements, they act as "type guards."
- **Real-World Relevance:** Type guards eliminate unsafe type casting (`as Type`). By using standard JavaScript `typeof`, `instanceof`, and `in` checks, TypeScript automatically refines broad unions (`string | number | Date | { error: string }`) into exact types within specific branch blocks.

---

## 2. Core Concepts
- **Formal Invariants:**
  - `typeof x === "type"`: Narrows primitive types based on the 8 ECMAScript return strings (`"string"`, `"number"`, `"bigint"`, `"boolean"`, `"symbol"`, `"undefined"`, `"object"`, `"function"`).
  - `typeof null === "object"`: Historical JS quirk. TypeScript's CFA reflects this: `typeof x === "object"` narrows to `object | null`, NOT just `object`.
  - `x instanceof Constructor`: Narrows `x` to the instance type of `Constructor` by verifying that `Constructor.prototype` exists in the prototype chain of `x`.
  - `"prop" in x`: Narrows an object union to variants that possess the specified property key. In TS 4.9+, `in` works even when the narrowed object type is unconstrained (`object`, `unknown`).
- **Mental Model:** Treat these operators as filters. Each operator inspects a specific runtime characteristic (primitive tag, prototype link, or property presence) and instructs the type checker to discard incompatible types from the union.
- **Soundness vs Ergonomics:** `instanceof` checks can fail across execution realms (e.g. iframes, worker threads, or npm packages duplicated across node_modules) because prototypes reside in different global memory spaces.

---

## 3. How It Works Internally
- **Type Guard Recognition in CFA:** When the parser encounters an `if` statement, the binder tags expressions containing `typeof`, `instanceof`, or `in` as type-guard expressions.
- **Checker Narrowing Rules:**
  - For `typeof x === "string"`, the checker calls `narrowTypeByTypeof(type, "string")`.
  - For `x instanceof C`, the checker computes `narrowTypeByInstanceof(type, C)`.
  - For `"k" in x`, the checker calls `narrowTypeByInKeyword(type, "k")`.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                       Type Guard Narrowing Matrix                        |
|                                                                          |
|       Condition Expression             Narrowed Target Type              |
|   --------------------------------------------------------------------   |
|   typeof x === "string"        ───►    string                            |
|   typeof x === "number"        ───►    number                            |
|   typeof x === "function"      ───►    Function / (...args) => any       |
|   typeof x === "object"        ───►    object | null (requires != null!) |
|   x instanceof Date            ───►    Date                              |
|   "run" in x                   ───►    { run: unknown }                  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Syntax Forms:**
  ```typescript
  if (typeof val === 'string') { /* string */ }
  if (val instanceof Error) { /* Error */ }
  if ('statusCode' in response) { /* has statusCode property */ }
  ```
- **Type-Space `typeof` vs Value-Space `typeof`:**
  - In value space: `if (typeof x === "string")` (runtime operator).
  - In type space: `type T = typeof myConst;` (queries the compile-time type of a value identifier).
- **tsconfig Dependencies:**
  - `strictNullChecks: true`: Ensures `typeof x === "object"` correctly retains `null`.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Narrow a multi-primitive union using `typeof`.
- **Code:**
```typescript
function formatIdentifier(id: string | number | bigint): string {
  if (typeof id === 'string') {
    return id.trim().toLowerCase();
  } else if (typeof id === 'number') {
    return id.toFixed(0);
  } else {
    // id is narrowed to bigint!
    return `${id.toString()}n`;
  }
}

console.log(formatIdentifier('  USR-101  ')); // "usr-101"
console.log(formatIdentifier(42.89));         // "43"
console.log(formatIdentifier(9007199254740995n)); // "9007199254740995n"
```
- **Output / Type Evaluation:**
```typescript
// "usr-101", "43", "9007199254740995n"
```
- **Explanation:** Sequential `typeof` checks cleanly exhaust the union, narrowing the final `else` branch to `bigint`.

### Example 2: Real-World Enterprise Pattern (Error Subclass Narrowing via `instanceof`)
- **Goal:** Differentiate domain-specific errors in an HTTP middleware layer.
- **Code:**
```typescript
export class DatabaseError extends Error {
  constructor(message: string, public readonly query: string) {
    super(message);
    this.name = 'DatabaseError';
  }
}

export class ValidationError extends Error {
  constructor(message: string, public readonly fields: string[]) {
    super(message);
    this.name = 'ValidationError';
  }
}

export function handleControllerError(err: unknown): { status: number; body: object } {
  if (err instanceof ValidationError) {
    // Narrowed to ValidationError: fields property accessible
    return { status: 400, body: { error: err.message, invalidFields: err.fields } };
  } else if (err instanceof DatabaseError) {
    // Narrowed to DatabaseError: query property accessible
    return { status: 500, body: { error: 'Database fault', queryRef: err.query } };
  } else if (err instanceof Error) {
    // Standard generic Error
    return { status: 500, body: { error: err.message } };
  }
  return { status: 500, body: { error: 'Unknown internal fault' } };
}
```
- **Output / Type Evaluation:**
```typescript
// Precision narrowing across custom exception inheritance hierarchies.
```
- **Explanation:** `instanceof` inspects prototype chains, allowing typed access to custom error properties.

### Example 3: Edge Case / Boundary Condition (`in` Operator Narrowing with Unconstrained Types)
- **Goal:** Demonstrate TypeScript 4.9+ `in` operator narrowing on unconstrained objects.
- **Code:**
```typescript
type SuccessResponse = { status: 200; payload: { userId: string } };
type FailureResponse = { status: 500; error: string };
type ApiResponse = SuccessResponse | FailureResponse;

function handleApiResponse(response: ApiResponse): void {
  if ('payload' in response) {
    // response is narrowed to SuccessResponse!
    console.log('User ID:', response.payload.userId);
  } else {
    // response is narrowed to FailureResponse!
    console.error('Error message:', response.error);
  }
}
```
- **Output / Type Evaluation:**
```typescript
// Compile-time guaranteed access to payload vs error.
```
- **Explanation:** The `in` operator checks for property existence, partitioning the union without requiring a dedicated discriminator tag.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate `typeof null === "object"` bug and compiler enforcement.
- **Code:**
```typescript
function inspectObject(target: string | { name: string } | null): void {
  if (typeof target === 'object') {
    // target is narrowed to: { name: string } | null !

    // @ts-expect-error TS18047: 'target' is possibly 'null'.
    console.log(target.name);
  }

  // Proper defensive check:
  if (typeof target === 'object' && target !== null) {
    console.log(target.name); // Safe! Narrowed to { name: string }
  }
}
```
- **Real Compiler Error:**
```
error TS18047: 'target' is possibly 'null'.
```
- **Explanation:** TypeScript correctly models JavaScript's `typeof null === "object"` quirk. Checking only `typeof target === "object"` leaves `null` in the narrowed type!

### Example 5: Idiomatic Modern Usage (Symbol.hasInstance Custom Guard)
- **Goal:** Implement a cross-realm safe `instanceof` guard using `Symbol.hasInstance`.
- **Code:**
```typescript
export class SafeBuffer {
  static [Symbol.hasInstance](instance: unknown): boolean {
    return (
      typeof instance === 'object' &&
      instance !== null &&
      'isSafeBuffer' in instance &&
      (instance as { isSafeBuffer: unknown }).isSafeBuffer === true
    );
  }

  public readonly isSafeBuffer = true;
  constructor(public readonly buffer: Uint8Array) {}
}

const fakeBuffer = { isSafeBuffer: true, buffer: new Uint8Array([1, 2, 3]) };
console.log(fakeBuffer instanceof SafeBuffer); // true!
```
- **Output / Type Evaluation:**
```typescript
// Evaluates to true even across different npm package instances.
```
- **Explanation:** Implementing `Symbol.hasInstance` enables custom structural runtime validation under standard `instanceof` syntax.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The Cross-Realm `instanceof` Trap:** If an object is created inside an iframe, worker, or VM context, `obj instanceof Array` evaluates to `false` in the parent window because `window.Array !== iframe.window.Array`. Use `Array.isArray()` instead of `instanceof Array`.
2. **`in` Operator on Primitives:** Running `"prop" in x` throws a runtime `TypeError: Cannot use 'in' operator to search for 'prop' in <primitive>` if `x` is a primitive like a string or number. Ensure `x` is checked as `typeof x === "object" && x !== null` before applying `in`.
3. **`typeof` Misspellings:** Checking `typeof x === "strng"` (typo) produces a compile-time error in TypeScript: `TS2367: This comparison appears to be unintentional...`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| `typeof x === 'object'` without null check | `TypeError: Cannot read properties of null` | `typeof x === 'object' && x !== null` | Handles JavaScript's `typeof null === "object"` quirk |
| `x instanceof Array` | Returns false across iframes / worker realms | `Array.isArray(x)` | Guarantees realm-agnostic array validation |
| `"prop" in primitive` | Runtime `TypeError: Cannot use 'in' operator on primitive` | Check `typeof x === 'object' && x !== null` first | Prevents unhandled exceptions |
| Using `as` instead of type guard | Silent runtime bugs when data shape drifts | Use `typeof`, `instanceof`, or `in` | Guarantees runtime verification |
| Assuming `instanceof` works on interfaces | `TS2693: 'MyInterface' only refers to a type` | Interfaces have zero runtime emit; use class or `in` | Interfaces cannot be used with `instanceof` |
| `typeof` on class instances | Returns `"object"` for all classes | Use `instanceof MyClass` | Distinguishes class instances |

---

## 8. Performance & Type-Checker Complexity
- **Fast Primitives:** `typeof` narrowing is evaluated instantaneously in $O(1)$ operations via string comparison in `checker.ts`.
- **Prototype Chain Traversal:** `instanceof` requires walking the JavaScript prototype chain at runtime ($O(D)$ where $D$ is prototype depth).
- **Isolated Declarations:** Fully supported under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:**
  - `ts.TypeOfExpression` (`SyntaxKind.TypeOfExpression`).
  - `ts.BinaryExpression` with `SyntaxKind.InstanceOfKeyword` or `SyntaxKind.InKeyword`.
- **Checker Narrowing Routines:**
  - `narrowTypeByTypeof`
  - `narrowTypeByInstanceof`
  - `narrowTypeByInKeyword`
- **Type Flags:** Primitives map directly to `TypeFlags.String`, `TypeFlags.Number`, `TypeFlags.Boolean`, etc.

---

## 10. Best Practices & Design Guidelines
- **Use `typeof` for Primitives:** Always use `typeof` when discriminating strings, numbers, booleans, and bigints.
- **Use `instanceof` for Class Hierarchies:** Ideal for error handling, DOM elements (`instanceof HTMLElement`), and dates (`instanceof Date`).
- **Use `in` for Object Shape Discrimination:** Ideal for distinguishing between anonymous object types or payload schemas without nominal classes.

---

## 11. Real-World Case Study
- **Scenario:** A micro-frontend container communicated with host applications via `window.postMessage`. The message listener received messages and checked `event.data instanceof CustomEventPayload`. Because each micro-frontend bundle bundled its own duplicate copy of the `CustomEventPayload` class, `instanceof` failed 100% of the time, breaking inter-app communication.
- **Implementation:** Refactored the event listener to use the `in` operator:
  ```typescript
  // Before:
  if (event.data instanceof CustomEventPayload) { ... } // Failed across micro-apps

  // After:
  if (typeof event.data === 'object' && event.data !== null && 'payloadKind' in event.data) {
    if (event.data.payloadKind === 'USER_NAVIGATION') {
      handleNavigation(event.data);
    }
  }
  ```
- **Outcome:** The check succeeded across all independent bundle realms, resolving the inter-app messaging outage completely.

---

## 12. Interview Questions

### Q1: Why does `typeof x === "object"` not narrow out `null` in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, `typeof null` famously returns `"object"` due to a legacy design decision in the initial 1995 JS implementation. TypeScript faithfully models JavaScript's runtime behavior. When you write `if (typeof x === "object")`, the compiler narrows `x` to `object | null` (if `null` was present in the original union). To safely narrow to a non-null object, you must explicitly check `if (typeof x === "object" && x !== null)`.
</details>

### Q2: Why can you NOT use `instanceof` with TypeScript interfaces?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`instanceof` is a runtime JavaScript operator that inspects the `.prototype` property of a constructor function. TypeScript interfaces are purely compile-time constructs that are completely erased during compilation, leaving zero JavaScript runtime artifacts. Attempting `x instanceof MyInterface` produces compile error `TS2693: 'MyInterface' only refers to a type, but is being used as a value here`. Classes, however, emit constructor functions and can be used with `instanceof`.
</details>

### Q3: What is the cross-realm problem with `instanceof` and how is it solved?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In browsers, each iframe, worker, or popup has its own distinct global execution context and memory heap. A `Date` or `Array` created inside an iframe inherits from the iframe's `Date.prototype`. When passed to the host window, `date instanceof Date` evaluates to `false` because the host's `Date.prototype` does not match the iframe's prototype. Solutions include:
1. Using `Array.isArray()` for arrays.
2. Checking `Object.prototype.toString.call(val) === '[object Date]'`.
3. Implementing custom `Symbol.hasInstance` or structural property checks via the `in` operator.
</details>

### Q4: How did TypeScript 4.9 improve the `in` operator for narrowing?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Prior to TypeScript 4.9, writing `"prop" in x` required `x` to already be a known union of object types. If `x` was an unconstrained `object` or `unknown`, TypeScript raised an error. In TS 4.9+, `in` narrowing was expanded so that if `x` is `object`, `"prop" in x` narrows `x` to `object & Record<"prop", unknown>`, allowing safe inspection of untyped objects without assertions.
</details>

### Q5: What values can `typeof` return in modern JavaScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In modern ECMAScript, `typeof` returns one of exactly 8 strings:
1. `"undefined"`
2. `"boolean"`
3. `"number"`
4. `"bigint"`
5. `"string"`
6. `"symbol"`
7. `"function"`
8. `"object"` (for objects, arrays, regexes, dates, and null)
</details>

### Q6: What happens when you check `if (key in arr)` where `arr` is an array?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, arrays are objects whose indices are string keys. `0 in ['a', 'b']` evaluates to `true` (index 0 exists). `"length" in ['a', 'b']` also evaluates to `true`. However, checking `"a" in ['a', 'b']` evaluates to `false` because `in` checks for the existence of *keys/indices*, not *values*. To check for value presence in an array, use `arr.includes(val)`.
</details>

### Q7: Can `instanceof` be overridden or customized in JavaScript and TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. ECMAScript 2015 introduced the well-known symbol `Symbol.hasInstance`. A class or constructor can implement a static method `static [Symbol.hasInstance](instance: unknown): boolean`. Whenever `x instanceof MyClass` is executed, JavaScript invokes this method instead of performing the default prototype chain walk, allowing custom duck-typing or structural checks.
</details>

### Q8: What is the difference between `typeof` in type space vs value space?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- Value space: `if (typeof x === "number")` executes JavaScript's runtime operator, producing one of 8 strings.
- Type space: `type Config = typeof defaultConfig;` queries TypeScript's type checker to extract the static type of the identifier `defaultConfig` at compile time. It has zero runtime emission.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Primitive Sorter (Easy)
- **Problem Statement:** Write a function `sortMixed(items: (string | number)[]): { strings: string[]; numbers: number[] }` using `typeof` narrowing.
- **Test Case:**
```typescript
sortMixed(['a', 1, 'b', 2]); // { strings: ['a', 'b'], numbers: [1, 2] }
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function sortMixed(items: (string | number)[]): { strings: string[]; numbers: number[] } {
  const strings: string[] = [];
  const numbers: number[] = [];
  for (const item of items) {
    if (typeof item === 'string') {
      strings.push(item);
    } else {
      numbers.push(item);
    }
  }
  return { strings, numbers };
}
```
</details>

### Exercise 2: Date or Timestamp Normalizer (Easy)
- **Problem Statement:** Write a function `toEpochMs(time: Date | number): number` using `instanceof` narrowing.
- **Test Case:**
```typescript
toEpochMs(new Date(1000)); // 1000
toEpochMs(2000); // 2000
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function toEpochMs(time: Date | number): number {
  if (time instanceof Date) {
    return time.getTime();
  }
  return time;
}
```
</details>

### Exercise 3: Safe Property Extractor via In (Medium)
- **Problem Statement:** Write a function `extractErrorMessage(error: unknown): string` that safely checks for `message` using `in` and `instanceof`.
- **Test Case:**
```typescript
extractErrorMessage(new Error('fail')); // "fail"
extractErrorMessage({ message: 'custom' }); // "custom"
extractErrorMessage('simple string'); // "simple string"
extractErrorMessage(123); // "Unknown error"
```
<details>
<summary><strong>View Solution</strong></summary>

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
  return 'Unknown error';
}
```
</details>

### Exercise 4: Discriminated Event Dispatcher (Medium)
- **Problem Statement:** Model two event shapes without common tags and use the `in` operator to route them to separate handling logic.
- **Test Case:**
```typescript
type MouseClick = { clientX: number; clientY: number };
type KeyPress = { key: string; code: number };
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type MouseClick = { clientX: number; clientY: number };
type KeyPress = { key: string; code: number };
type UIEvent = MouseClick | KeyPress;

function handleUIEvent(event: UIEvent): string {
  if ('clientX' in event) {
    return `Click at (${event.clientX}, ${event.clientY})`;
  } else {
    return `Key: ${event.key} [${event.code}]`;
  }
}
```
</details>

### Exercise 5: Universal Realm-Safe Date Guard (Hard)
- **Problem Statement:** Implement a type guard `isDateObject(val: unknown): val is Date` that works across iframes, worker threads, and node VM contexts without relying purely on `instanceof Date`.
- **Test Case:**
```typescript
isDateObject(new Date()); // true
isDateObject({}); // false
isDateObject('2025-01-01'); // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function isDateObject(val: unknown): val is Date {
  return (
    typeof val === 'object' &&
    val !== null &&
    (val instanceof Date || Object.prototype.toString.call(val) === '[object Date]') &&
    !isNaN((val as Date).getTime())
  );
}
```
</details>

### Exercise 6: Dynamic Schema Property Validator (Hard)
- **Problem Statement:** Author a function `hasProperties<K extends string>(obj: unknown, ...keys: K[]): obj is Record<K, unknown>` that verifies and narrows all requested keys at runtime.
- **Test Case:**
```typescript
const data: unknown = { id: 'u1', count: 10 };
if (hasProperties(data, 'id', 'count')) {
  // data is narrowed to Record<'id' | 'count', unknown>
  console.log(data.id, data.count);
}
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function hasProperties<K extends string>(
  obj: unknown,
  ...keys: K[]
): obj is Record<K, unknown> {
  if (typeof obj !== 'object' || obj === null) {
    return false;
  }
  for (const k of keys) {
    if (!(k in obj)) {
      return false;
    }
  }
  return true;
}
```
</details>
