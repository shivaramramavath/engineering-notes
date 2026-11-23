# Function Overloads

> Function overloads establish multiple public callable signatures for a single unified implementation, enabling distinct return types conditioned upon specific argument combinations.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-function-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/00-function-types.md), [01-parameters-and-return-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/01-parameters-and-return-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
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
- **Problem Statement:** In JavaScript, a single function often changes its behavior and return type depending on the arguments provided (e.g. `document.createElement("div")` returns `HTMLDivElement`, while `document.createElement("canvas")` returns `HTMLCanvasElement`). Using simple union types (`string | number`) creates loose relationships where the return type is not linked to the specific argument passed.
- **Historical Context:** TypeScript introduced function overloading from version 0.9. Unlike languages like Java or C++ where each overload is a separate compiled function, JavaScript only has one runtime function. In TypeScript, overloads exist exclusively in type-space; the single runtime implementation must handle all declared signatures.
- **Real-World Relevance:** Overloads power DOM APIs, Node.js core libraries (`fs.readFileSync`), and enterprise framework APIs, ensuring exact return type mapping without forcing callers to cast.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Overload Signatures: One or more declared function headers defining public call contracts.
  - Implementation Signature: The final function header containing the actual body. It is completely HIDDEN from callers.
  - Implementation Compatibility: The implementation signature MUST be general enough to accept all overload signatures simultaneously.
  - Call Resolution: The compiler resolves overloads sequentially from top to bottom, picking the first signature that matches the call arguments.
- **Mental Model:** Treat overload signatures as the public API catalog displayed to users, and treat the implementation signature as the private kitchen that must fulfill any order on the menu.
- **Soundness vs Ergonomics:** Overloads can be fragile when callers pass union types. If overload 1 expects `string` and overload 2 expects `number`, passing `string | number` fails unless a union overload is explicitly declared.

---

## 3. How It Works Internally
- **Signature List in Symbol Table:** In the binder, successive `FunctionDeclaration` nodes with the same identifier are grouped into a single `Symbol` with multiple `declarations`.
- **Overload Resolution Algorithm:** In `checker.ts`, `resolveCall` iterates through the overload signatures in source code order. The first signature whose parameters are assignable from the arguments is chosen. If no overload matches, the compiler reports an error against the last signature.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                     Overload Resolution Pipeline                         |
|                                                                          |
|   1. Declared Overload 1:  function format(ts: number): string;          |
|   2. Declared Overload 2:  function format(date: Date): string;          |
|   3. Implementation:       function format(val: number | Date): string { |
|                                                                          |
|   Caller Invocation: format(new Date())                                  |
|          │                                                               |
|          ▼                                                               |
|   Check Overload 1: Date assignable to number? ──► NO                    |
|          │                                                               |
|          ▼                                                               |
|   Check Overload 2: Date assignable to Date?   ──► YES (Match Selected!) |
|          │                                                               |
|          ▼                                                               |
|   Return Type evaluated as: string                                       |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Function Declaration Overloading:**
  ```typescript
  function parse(input: string): object;
  function parse(input: number): string;
  function parse(input: string | number): object | string {
    if (typeof input === 'string') return JSON.parse(input);
    return input.toString();
  }
  ```
- **Interface Call Signature Overloading:**
  ```typescript
  interface Creator {
    (tag: 'div'): HTMLDivElement;
    (tag: 'span'): HTMLSpanElement;
    (tag: string): HTMLElement;
  }
  ```
- **tsconfig Dependencies:** Operates under standard configurations.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Link return type to input argument type.
- **Code:**
```typescript
function convert(input: string): number;
function convert(input: number): string;
function convert(input: string | number): number | string {
  if (typeof input === 'string') {
    return parseInt(input, 10);
  }
  return input.toString();
}

const num = convert('123'); // Inferred as: number
const str = convert(456);   // Inferred as: string
```
- **Output / Type Evaluation:**
```typescript
// num: number, str: string (exact return types inferred!)
```
- **Explanation:** Callers receive exact return types matching their argument type without type casting.

### Example 2: Real-World Enterprise Pattern (Database Query Overload)
- **Goal:** Return a single entity when an ID is passed, or an array when an options filter is passed.
- **Code:**
```typescript
type User = { id: string; name: string };
type QueryOptions = { limit?: number; offset?: number };

export class UserRepository {
  async find(id: string): Promise<User | null>;
  async find(options: QueryOptions): Promise<User[]>;
  async find(query: string | QueryOptions): Promise<User | null | User[]> {
    if (typeof query === 'string') {
      // Find single user by ID
      return { id: query, name: 'Alice' };
    }
    // Find multiple users by filter
    return [{ id: '1', name: 'Alice' }, { id: '2', name: 'Bob' }];
  }
}

const repo = new UserRepository();
const singleUser = await repo.find('usr_1'); // Inferred as: User | null
const userList = await repo.find({ limit: 10 }); // Inferred as: User[]
```
- **Output / Type Evaluation:**
```typescript
// singleUser: Promise<User | null>, userList: Promise<User[]>
```
- **Explanation:** Callers receive precise return contracts based on parameter shape.

### Example 3: Edge Case / Boundary Condition (Union Argument Resolution Pitfall)
- **Goal:** Demonstrate why passing a union to overloaded functions fails without a union signature.
- **Code:**
```typescript
function getLength(x: any[]): number;
function getLength(x: string): number;
function getLength(x: any[] | string): number {
  return x.length;
}

const arg: string | string[] = Math.random() > 0.5 ? 'hello' : ['a', 'b'];

// @ts-expect-error TS2769: No overload matches this call.
getLength(arg);

// Solution: Add a union overload signature:
function getLengthSafe(x: any[]): number;
function getLengthSafe(x: string): number;
function getLengthSafe(x: any[] | string): number; // Explicit union overload
function getLengthSafe(x: any[] | string): number {
  return x.length;
}

getLengthSafe(arg); // OK!
```
- **Output / Type Evaluation:**
```typescript
// TS2769 occurs because neither overload 1 nor 2 individually accepts string | string[].
```
- **Explanation:** Overloads are checked individually. If an argument is a union, it must satisfy one single overload signature, unless a union signature is explicitly declared.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection when implementation signature is incompatible.
- **Code:**
```typescript
function process(x: string): string;
function process(x: number): number;

// @ts-expect-error TS2394: This overload signature is not compatible with its implementation signature.
function process(x: string): string {
  return x;
}
```
- **Real Compiler Error:**
```
error TS2394: This overload signature is not compatible with its implementation signature.
```
- **Explanation:** The implementation signature only accepts `string`, failing to accommodate the `number` overload signature.

### Example 5: Idiomatic Modern Usage (Replacing Overloads with Conditional Types)
- **Goal:** Replace brittle overloads with a clean, single generic conditional type.
- **Code:**
```typescript
// Modern alternative to multiple overloads:
export function formatData<T extends string | number>(
  val: T
): T extends string ? { text: string } : { count: number } {
  if (typeof val === 'string') {
    return { text: val } as any;
  }
  return { count: val } as any;
}

const r1 = formatData('hello'); // Inferred as: { text: string }
const r2 = formatData(42);      // Inferred as: { count: number }
```
- **Output / Type Evaluation:**
```typescript
// Exact return type evaluated cleanly without multiple overload headers.
```
- **Explanation:** Generic conditional types often provide a more maintainable alternative to overloads, automatically handling union inputs.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Ordering Matters:** The compiler checks overloads from top to bottom. If a broader signature is placed before a more specific signature, the broader signature matches first, shadowing the specific overload. Always order from most specific to least specific.
2. **Implementation Signature is Inaccessible:** Callers cannot call the function using the implementation signature's parameters unless that signature is also duplicated as an overload header.
3. **Overloads vs Union Types:** Overloads should only be used when the return type changes based on arguments. If the return type is identical across all parameter types, use union parameter types instead.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Overloading when return type is identical | Unnecessary boilerplate | `fn(x: string | number): void` | Simplifies signature maintenance |
| Ordering general overload before specific | Specific overload never matches (shadowed) | Place specific overloads first | Ensures correct signature selection |
| Forgetting union overload for union callers | `TS2769: No overload matches this call` | Add union overload or use generics | Supports union arguments from callers |
| Implementation incompatible with overloads | `TS2394: Signature not compatible with implementation` | Widen implementation parameters | Fulfills implementation contract |
| Attempting overloads on arrow functions | Arrow functions cannot use overload syntax | Use function declaration or interface | Conforms to TypeScript syntax rules |
| Exporting implementation without all overloads | Missing type definitions in `.d.ts` | Export all overload headers together | Preserves public API surface |

---

## 8. Performance & Type-Checker Complexity
- **Sequential Resolution Cost:** If a function has 10+ overloads, the compiler evaluates every call against signatures sequentially until a match is found ($O(M)$ where $M$ is signature count).
- **Generic Overloads Overhead:** Generic overloads require type argument inference on each attempt, increasing compilation time.
- **Isolated Declarations:** All overload signatures must be fully typed and exported together under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Representation:** Multiple `FunctionDeclaration` nodes sharing identical names within the same container scope.
- **Symbol Merging:** The binder combines overload declarations into a single `Symbol` having `SymbolFlags.Function`.
- **Signature Resolution:** `resolveCall` in `checker.ts` evaluates candidates using `checkApplicableSignature`.

---

## 10. Best Practices & Design Guidelines
- **Prefer Union Types When Return Type Does Not Vary:** If `f(x: string)` and `f(x: number)` both return `void`, write `f(x: string | number): void`.
- **Order Overloads from Most Specific to Most General:** Place literal overloads (`tag: 'div'`) before general overloads (`tag: string`).
- **Consider Conditional Types for Complex Relationships:** Conditional types often handle unions better than overloads.

---

## 11. Real-World Case Study
- **Scenario:** A core state management library exposed an `action` creator:
  ```typescript
  function createAction(type: string): Action;
  function createAction(type: string, payload: any): PayloadAction;
  ```
  When developers invoked `createAction(actionName, maybePayload)` where `maybePayload` was `Payload | undefined`, TypeScript threw `TS2769: No overload matches this call` because neither overload matched an optional second parameter.
- **Implementation:** Added an explicit optional overload:
  ```typescript
  function createAction<P = void>(type: string, payload?: P): Action<P>;
  function createAction(type: string, payload?: unknown): Action<unknown> {
    return { type, payload };
  }
  ```
- **Outcome:** Simplified the API into a single generic signature, eliminating overload matching errors for 80+ engineering teams.

---

## 12. Interview Questions

### Q1: Why is the implementation signature of an overloaded function not callable from outside?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In TypeScript, overload signatures define the public API contract, while the implementation signature exists solely to provide the runtime body and type check internal logic. The implementation signature is intentionally excluded from the public signature list of the symbol. If callers could invoke the implementation signature directly, it would bypass the specific contracts declared by the overload signatures. To make the implementation signature callable, its signature must be explicitly declared as one of the overload headers.
</details>

### Q2: Why does passing a union of types to an overloaded function often cause TS2769?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript resolves overloads by testing each overload signature individually against the arguments provided. It does NOT synthesize a combined union signature from all overloads. If Overload 1 accepts `string` and Overload 2 accepts `number`, an argument of type `string | number` fails Overload 1 (because `string | number` is not assignable to `string`) and fails Overload 2 (because `string | number` is not assignable to `number`). To support union arguments, an explicit overload accepting the union (`x: string | number`) must be provided, or a generic conditional type should be used instead.
</details>

### Q3: Why does the order of overload signatures matter?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript evaluates overload signatures in the exact order they are declared in the source file. It selects the FIRST signature that satisfies parameter assignability. If a general signature (e.g. `tag: string`) is placed before a specific literal signature (e.g. `tag: "div"`), any call with `"div"` will match the first `string` overload and return the generic type, effectively shadowing the specific overload. Therefore, overloads must always be ordered from most specific to least specific.
</details>

### Q4: When should you prefer union types over function overloads?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

You should always prefer union types when the return type of the function does not depend on which parameter type is passed. For example, `function log(x: string | number): void` is vastly superior to declaring two overloads returning `void`. Overloads should be reserved for cases where different argument types produce different return types.
</details>

### Q5: Can arrow functions have overloads?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Arrow function declarations cannot directly define overload headers in their standard syntax (`const fn = ...`). However, an arrow function can implement an overloaded contract if it is assigned to an interface or type alias that declares multiple call signatures:
```typescript
type Overloaded = {
  (x: string): number;
  (x: number): string;
};
const fn: Overloaded = ((x: any) => ...) as Overloaded;
```
</details>

### Q6: What is the compiler error TS2394?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`TS2394: This overload signature is not compatible with its implementation signature` occurs when one of the declared overload signatures has parameters or a return type that cannot be assigned to the parameters or return type of the implementation signature. The implementation signature must be general enough to encompass all overload signatures.
</details>

### Q7: How do conditional types compare with function overloads?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Conditional types allow mapping parameter types to return types programmatically (`fn<T>(x: T): T extends A ? R1 : R2`). Unlike overloads, conditional types naturally handle union arguments without requiring extra signatures, and they work cleanly on arrow functions. However, complex conditional types can produce harder-to-read hover tooltips compared to clean overload signatures.
</details>

### Q8: How are overloads represented in emitted JavaScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Overload signatures have zero JavaScript representation; they are completely erased during compilation. Only the single implementation signature and its function body are emitted in the generated JavaScript file.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: String or Number Formatter Overload (Easy)
- **Problem Statement:** Declare and implement an overloaded function `formatValue` that accepts `string` and returns `string`, or accepts `number` and returns `number`.
- **Test Case:**
```typescript
const s = formatValue('hello'); // string
const n = formatValue(42);      // number
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function formatValue(val: string): string;
function formatValue(val: number): number;
function formatValue(val: string | number): string | number {
  return val;
}
```
</details>

### Exercise 2: Date Constructor Overload (Easy)
- **Problem Statement:** Create an overloaded function `createDate` that accepts a timestamp (`number`) or an ISO string (`string`) or no arguments, returning a `Date`.
- **Test Case:**
```typescript
const d1 = createDate(); // Date
const d2 = createDate(1000); // Date
const d3 = createDate('2025-01-01'); // Date
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function createDate(): Date;
function createDate(timestamp: number): Date;
function createDate(isoString: string): Date;
function createDate(arg?: number | string): Date {
  return arg !== undefined ? new Date(arg) : new Date();
}
```
</details>

### Exercise 3: Type-Safe Event Listener Overload (Medium)
- **Problem Statement:** Implement an overloaded event emitter `on` method that returns specific event payloads for `"status"` vs `"data"`.
- **Test Case:**
```typescript
emitter.on('status', (s) => console.log(s.code));
emitter.on('data', (d) => console.log(d.bytes));
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
interface Emitter {
  on(event: 'status', cb: (payload: { code: number }) => void): void;
  on(event: 'data', cb: (payload: { bytes: Uint8Array }) => void): void;
  on(event: string, cb: (payload: any) => void): void;
}
```
</details>

### Exercise 4: DOM Element Selector Overload (Medium)
- **Problem Statement:** Write overloads for a custom query selector that maps tag names `"button"` and `"input"` to their HTML element types.
- **Test Case:**
```typescript
const btn = queryElement('button'); // HTMLButtonElement | null
const input = queryElement('input'); // HTMLInputElement | null
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function queryElement(tag: 'button'): HTMLButtonElement | null;
function queryElement(tag: 'input'): HTMLInputElement | null;
function queryElement(tag: string): HTMLElement | null;
function queryElement(tag: string): HTMLElement | null {
  return document.querySelector(tag);
}
```
</details>

### Exercise 5: Compile-Time Overload Count Detector (Hard)
- **Problem Statement:** Write a type utility that verifies if a function type has multiple overloaded call signatures.
- **Test Case:**
```typescript
type Overloaded = { (a: string): number; (a: number): string };
type Single = (a: string) => number;
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
// Intersections of call signatures allow detecting overload presence:
type HasMultipleCallSignatures<T> =
  T extends { (...args: any[]): any; (...args: any[]): any } ? true : false;
```
</details>

### Exercise 6: Curried Overload Generator (Hard)
- **Problem Statement:** Write overloads for a function `slice` that can be called with 3 arguments `slice(arr, start, end)` or in curried fashion with 2 arguments `slice(start, end)(arr)`.
- **Test Case:**
```typescript
const r1 = slice([1, 2, 3], 0, 2); // [1, 2]
const r2 = slice(0, 2)([1, 2, 3]); // [1, 2]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function slice<T>(arr: T[], start: number, end: number): T[];
function slice<T>(start: number, end: number): (arr: T[]) => T[];
function slice<T>(
  first: T[] | number,
  second: number,
  third?: number
): T[] | ((arr: T[]) => T[]) {
  if (Array.isArray(first)) {
    return first.slice(second, third);
  }
  return (arr: T[]) => arr.slice(first, second);
}
```
</details>
