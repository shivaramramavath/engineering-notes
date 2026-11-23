# Truthiness and Equality Narrowing

> Control Flow Analysis leverages JavaScript truthiness and equality checks (`===`, `!==`, `==`, `!=`) to systematically narrow union types and eliminate impossible execution branches.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [08-null-and-undefined.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/08-null-and-undefined.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.0 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `strictNullChecks: true`, `noImplicitReturns: true` |

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
- **Problem Statement:** In dynamically typed JavaScript, conditional branching frequently checks boolean truthiness (`if (x)`) or value equality (`if (x === "active")`). A type system that ignores these conditional expressions forces developers to repeatedly write unsafe type assertions (`as string`).
- **Historical Context:** TypeScript 2.0 pioneered Control Flow Analysis (CFA). CFA tracks how statements, assignments, and control branches change the type of a variable at each specific program point.
- **Real-World Relevance:** Truthiness and equality narrowing make standard JavaScript idioms 100% type-safe. Writing `if (user !== null)` or `switch (event.type)` refines complex polymorphic union types into concrete shapes without external libraries or runtime wrappers.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Truthiness checks (`if (x)`) narrow out falsy values: `false`, `0`, `-0`, `0n`, `""`, `null`, `undefined`, and `NaN`.
  - Strict equality (`x === y`): When two variables are compared with `===`, TypeScript narrows `x` to the intersection of the types of `x` and `y`.
  - Loose equality (`x == null`): In JavaScript, `x == null` checks for BOTH `null` and `undefined`. TypeScript narrows out both `null` and `undefined` simultaneously in the positive branch.
  - Discriminated Union Narrowing: Comparing a common literal discriminant property (`shape.kind === "circle"`) narrows the parent object union to the exact matching variant.
- **Mental Model:** CFA acts as a symbolic execution engine. At every branch fork (`if`, `switch`, ternary), it splits the type lattice. At branch merges, it takes the union of surviving types.
- **Soundness vs Ergonomics:** Truthiness narrowing is convenient but dangerous when dealing with numbers or strings where `0` or `""` are legitimate valid inputs.

---

## 3. How It Works Internally
- **Flow Graph Construction:** The binder creates a directed graph of `FlowCondition` nodes representing basic blocks, branching points, and jumps (`return`, `throw`, `break`).
- **Flow Type Evaluation:** When the type checker evaluates the type of an identifier at an expression node, it traverses the flow graph backward to determine the narrowed type at that exact location.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                     Control Flow Analysis (CFA) Graph                    |
|                                                                          |
|                  Initial Type: [ string | number | null ]                |
|                                     │                                    |
|                                     ▼                                    |
|                             [ if (x != null) ]                           |
|                                /          \                              |
|                              TRUE        FALSE                           |
|                              /              \                           |
|                             ▼                ▼                           |
|                  Type: [ string | number ]  Type: [ null ]               |
|                             │                                            |
|                             ▼                                            |
|                  [ if (typeof x === "string") ]                          |
|                     /                     \                             |
|                   TRUE                   FALSE                           |
|                   /                         \                           |
|                  ▼                           ▼                           |
|          Type: [ string ]             Type: [ number ]                   |
|          Safe: x.toUpperCase()        Safe: x.toFixed(2)                 |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Conditional Branching Forms:**
  - `if (val)` / `if (!val)` (Truthiness)
  - `if (a === b)` / `if (a !== b)` (Strict equality / inequality)
  - `if (a == null)` / `if (a != null)` (Nullish loose equality)
  - `switch (obj.tag) { case 'A': ... }` (Discriminated union matching)
  - `const res = cond ? a : b` (Ternary narrowing)
- **tsconfig Dependencies:**
  - `strictNullChecks: true`: Mandatory; without it, null and undefined are not tracked as distinct types by CFA.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compare truthiness vs explicit null checks for primitive values.
- **Code:**
```typescript
function printCount(count: number | undefined): void {
  // Truthiness trap: 0 is falsy!
  if (count) {
    console.log(`Count is ${count}`);
  } else {
    console.log('No count or zero'); // Reaches here for count === 0!
  }

  // Safe narrowing:
  if (count !== undefined) {
    console.log(`Precise count is ${count}`); // Correct for count === 0
  }
}

printCount(0);
```
- **Output / Type Evaluation:**
```typescript
// "No count or zero" followed by "Precise count is 0"
```
- **Explanation:** Truthiness filters out `0`, which is frequently a legitimate numerical value. Explicit equality preserves valid falsy data.

### Example 2: Real-World Enterprise Pattern (Discriminated Union Switch Narrowing)
- **Goal:** Exhaustively process telemetry packets using discriminant equality.
- **Code:**
```typescript
type TelemetryPacket =
  | { kind: 'GPS'; latitude: number; longitude: number }
  | { kind: 'Battery'; millivolts: number; charging: boolean }
  | { kind: 'Heartbeat'; uptimeSeconds: number };

export function processPacket(packet: TelemetryPacket): string {
  switch (packet.kind) {
    case 'GPS':
      // Narrowed to GPS packet
      return `GPS: ${packet.latitude.toFixed(4)}, ${packet.longitude.toFixed(4)}`;
    case 'Battery':
      // Narrowed to Battery packet
      return `Battery: ${packet.millivolts}mV (Charging: ${packet.charging})`;
    case 'Heartbeat':
      // Narrowed to Heartbeat packet
      return `Uptime: ${packet.uptimeSeconds}s`;
  }
}
```
- **Output / Type Evaluation:**
```typescript
// Type checker verifies all variants and properties without assertions.
```
- **Explanation:** CFA matches string literal types in `case` statements, refining the object to the exact member of the union.

### Example 3: Edge Case / Boundary Condition (Loose Equality Null Checking)
- **Goal:** Narrow both `null` and `undefined` using a single loose equality check.
- **Code:**
```typescript
function processInput(val: string | number | null | undefined): string {
  // In JavaScript: val == null is true for both null and undefined
  if (val == null) {
    // val is narrowed to: null | undefined
    return 'DEFAULT';
  }

  // In the else branch, both null and undefined are eliminated:
  // val is narrowed to: string | number
  return typeof val === 'string' ? val.toUpperCase() : val.toFixed(2);
}
```
- **Output / Type Evaluation:**
```typescript
// Clean elimination of nullish types with one equality comparison.
```
- **Explanation:** TypeScript's CFA incorporates ECMAScript loose equality semantics for `== null` and `!= null`.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection when comparing types with zero overlap.
- **Code:**
```typescript
type Role = 'ADMIN' | 'USER';

function checkPermission(role: Role): void {
  // @ts-expect-error TS2367: This comparison appears to be unintentional because the types 'Role' and '"SUPERADMIN"' have no overlap.
  if (role === 'SUPERADMIN') {
    console.log('Access granted');
  }
}
```
- **Real Compiler Error:**
```
error TS2367: This comparison appears to be unintentional because the types 'Role' and '"SUPERADMIN"' have no overlap.
```
- **Explanation:** TypeScript rejects equality checks between types that have no possible common values, catching typos and obsolete checks at compile time.

### Example 5: Idiomatic Modern Usage (Boolean Filter Narrowing with Predicate)
- **Goal:** Safely filter out falsy values from an array without losing type precision.
- **Code:**
```typescript
// Standard Boolean(x) filter often drops type precision in older TS.
// Idiomatic type-safe truthiness filter:
export function isTruthy<T>(val: T): val is NonNullable<T> {
  return Boolean(val);
}

const mixed = ['Alice', null, 'Bob', undefined, 'Charlie'];
const validUsers: string[] = mixed.filter(isTruthy);
```
- **Output / Type Evaluation:**
```typescript
// validUsers is typed as string[], null and undefined stripped cleanly.
```
- **Explanation:** A custom truthiness predicate bridges JavaScript runtime boolean conversion with TypeScript's non-nullable type system.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The Empty String Trap:** `if (text)` evaluates to `false` when `text === ""`. If an empty string is a valid input (e.g. empty user comment), truthiness checks will misclassify it.
2. **Boolean Constructor Type Inference:** In TypeScript, `arr.filter(Boolean)` narrows out `null` and `undefined` starting in TypeScript 4.1+, but may strip `0` or `false` from unions unexpectedly.
3. **Comparing Objects by Value:** Checking `if (objA === objB)` tests reference identity, not structural equality. Two distinct objects with identical fields evaluate to `false`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| `if (user.age)` to check existence | Drops `0` (e.g. newborn infant age) | `if (user.age !== undefined)` | Prevents data loss on valid numeric zeros |
| `if (text)` to validate string presence | Fails on empty string `""` | `if (typeof text === 'string')` | Correctly distinguishes empty strings from null/undefined |
| `if (role === 'admin')` when type is `'ADMIN'` | `TS2367: Types have no overlap` | Use correct case or const enum | Catches silent dead-code branching |
| Reassigning narrowed variable inside block | CFA widens type back to union | Keep variables `const` | Prevents invalidation of narrowing |
| Checking `typeof null === 'object'` | Truthy for `null`, crashes on property access | `val !== null && typeof val === 'object'` | Avoids runtime `TypeError` |
| `switch` without exhaustiveness assertion | New variants unhandled silently | `default: assertNever(val)` | Guarantees compile-time exhaustiveness |

---

## 8. Performance & Type-Checker Complexity
- **Control Flow Splitting:** CFA branches create fork points. In deeply nested conditions (50+ nested `if`/`else` chains), the number of flow paths can grow exponentially, increasing type-checking time. Keep conditional trees flat.
- **Literal Equality Intersections:** Computing intersections between literal types is an $O(1)$ symbol ID equality check.
- **Isolated Declarations:** Full compatibility with `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** `ts.IfStatement` (`SyntaxKind.IfStatement`), `ts.SwitchStatement` (`SyntaxKind.SwitchStatement`), `ts.BinaryExpression` (`SyntaxKind.BinaryExpression`).
- **Flow Nodes:** Represented in `checker.ts` by `FlowNode` structures: `FlowCondition`, `FlowSwitchClause`, `FlowAssignment`.
- **Narrowing Algorithm:** `narrowTypeByEquality` inspects operator tokens (`SyntaxKind.EqualsEqualsEqualsToken`, `SyntaxKind.ExclamationEqualsEqualsToken`).

---

## 10. Best Practices & Design Guidelines
- **Use Strict Equality (`===`):** Standardize on `===` everywhere, reserving `== null` exclusively for concise nullish checks.
- **Prefer Explicit Checks Over Truthiness for Numbers and Booleans:** Never check numbers with `if (num)` unless zero is intentionally rejected.
- **Leverage Discriminated Unions for Complex State:** Design domain states with a single `kind` or `status` property to maximize CFA efficiency.

---

## 11. Real-World Case Study
- **Scenario:** A banking onboarding portal collected applicants' credit history scores. An applicant with a score of `0` (unscored credit) submitted the form. The validation function checked `if (data.creditScore)`. Because `0` is falsy, the system evaluated the field as missing and defaulted it to `650` (fair credit), issuing unvetted credit cards and incurring $420,000 in bad-debt defaults.
- **Implementation:** Refactored the validation logic to use explicit nullish checks:
  ```typescript
  // Before:
  const score = data.creditScore ? data.creditScore : 650;

  // After:
  const score = data.creditScore ?? null;
  if (score === null) {
    throw new ValidationError('Credit score cannot be null or omitted');
  }
  ```
- **Outcome:** The zero credit score was accurately preserved and routed to specialized manual review, preventing unapproved credit issuance.

---

## 12. Interview Questions

### Q1: What are all seven falsy values in JavaScript and how does TypeScript narrow them?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The seven falsy values in modern JavaScript are:
1. `false` (boolean)
2. `0` and `-0` (number)
3. `0n` (bigint)
4. `""` (empty string)
5. `null`
6. `undefined`
7. `NaN` (number)

When a variable of union type (e.g. `string | null | undefined`) enters an `if (x)` block, TypeScript's CFA eliminates `null`, `undefined`, and the empty string literal `""`, narrowing `x` to non-empty `string`. In the `else` branch, `x` is narrowed to `"" | null | undefined`.
</details>

### Q2: Why does TypeScript emit TS2367 when comparing `if (x === "hello")` if `x` is typed as `number`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript enforces strict type safety during equality comparisons. If the types of the two operands have zero structural overlap (meaning there is no possible runtime JavaScript value that can satisfy both types simultaneously), the comparison will always evaluate to `false`. TypeScript treats this as a likely developer error (e.g. comparing wrong variables, typos, or outdated assumptions) and raises `TS2367: This comparison appears to be unintentional because the types have no overlap`.
</details>

### Q3: How does loose equality `x == null` differ from strict equality `x === null` in CFA narrowing?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `x === null`: Narrows `x` strictly to `null`. If `x` was `string | null | undefined`, in the `else` branch `x` remains `string | undefined`.
- `x == null`: Leverages JavaScript's loose equality coercion rules where `null == undefined` evaluates to `true`. In TypeScript, `if (x == null)` narrows `x` to `null | undefined` in the true branch, and completely removes BOTH `null` and `undefined` in the false branch, leaving `string`.
</details>

### Q4: What is the risk of truthiness narrowing on numbers?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, the number `0` is falsy. If a function accepts `number | undefined` representing an index, temperature, amount, or count, checking `if (val)` evaluates to `false` when `val === 0`. This causes the program to take the fallback or error branch even though a valid, non-nullish numerical value was supplied. Always use `if (val !== undefined)` or `val ?? defaultVal`.
</details>

### Q5: Can TypeScript narrow based on equality between two variables of union types (`if (a === b)`)?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. If `a: "x" | "y"` and `b: "y" | "z"`, inside `if (a === b)`, TypeScript computes the intersection of both types: `("x" | "y") & ("y" | "z")`. Because the only overlapping literal is `"y"`, both `a` and `b` are narrowed to `"y"` inside that conditional block.
</details>

### Q6: How does `switch (true)` pattern matching interact with TypeScript's type narrowing?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

While `switch (true)` is valid JavaScript, TypeScript's CFA does not track condition expressions across `case` statements in `switch (true)` as effectively as it does in sequential `if / else if` blocks or standard discriminated `switch (x.kind)`. Sequential `if` statements or standard discriminated unions are preferred for predictable type narrowing.
</details>

### Q7: Why does mutating a variable invalidate previous narrowing?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript's CFA tracks variable types based on linear control flow. When a variable declared with `let` is reassigned (`x = getNewValue()`), the compiler resets the variable's type at that program point to the declared or inferred type of the new assignment expression, discarding previous narrowing from preceding `if` checks.
</details>

### Q8: How does `NaN` behave in equality and truthiness narrowing?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`NaN` has the type `number` in TypeScript. In JavaScript, `NaN === NaN` is always `false`. Therefore, you cannot narrow to `NaN` using `===`. Truthiness narrowing treats `NaN` as falsy. To explicitly check for `NaN`, `Number.isNaN(val)` must be used.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Number Coalesce (Easy)
- **Problem Statement:** Write a function `getQuantity(input: number | undefined): number` that returns `input` if provided, correctly preserving `0`, defaulting to `1` only if `undefined`.
- **Test Case:**
```typescript
getQuantity(0); // 0
getQuantity(5); // 5
getQuantity(undefined); // 1
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function getQuantity(input: number | undefined): number {
  return input !== undefined ? input : 1;
}
```
</details>

### Exercise 2: Strict Nullish Check (Easy)
- **Problem Statement:** Write a function `hasValue<T>(val: T | null | undefined): val is T` using loose equality narrowing.
- **Test Case:**
```typescript
hasValue('test'); // true
hasValue(null); // false
hasValue(undefined); // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function hasValue<T>(val: T | null | undefined): val is T {
  return val != null;
}
```
</details>

### Exercise 3: Discriminated State Processor (Medium)
- **Problem Statement:** Model a 3-state request workflow (`loading`, `success`, `failure`) and write a processor using strict equality switch narrowing that returns formatted status strings.
- **Test Case:**
```typescript
type Req =
  | { status: 'loading' }
  | { status: 'success'; data: string }
  | { status: 'failure'; error: string };
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type Req =
  | { status: 'loading' }
  | { status: 'success'; data: string }
  | { status: 'failure'; error: string };

function formatRequest(req: Req): string {
  switch (req.status) {
    case 'loading': return 'In Flight';
    case 'success': return `OK: ${req.data}`;
    case 'failure': return `ERR: ${req.error}`;
  }
}
```
</details>

### Exercise 4: Truthy Array Filter (Medium)
- **Problem Statement:** Author a function `compact<T>(arr: readonly (T | null | undefined | false | 0 | '')[]): T[]` that filters out all falsy values while retaining valid truthy elements.
- **Test Case:**
```typescript
compact(['a', 0, 'b', null, false, 'c']); // ['a', 'b', 'c']
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function compact<T>(arr: readonly (T | null | undefined | false | 0 | '')[]): T[] {
  return arr.filter((x): x is T => Boolean(x));
}
```
</details>

### Exercise 5: Compile-Time Overlap Asserter (Hard)
- **Problem Statement:** Write a type-level check `HasOverlap<A, B>` that evaluates to `true` if `A` and `B` share at least one common value in their domain, or `false` if equality comparison would trigger TS2367.
- **Test Case:**
```typescript
type T1 = HasOverlap<'a' | 'b', 'b' | 'c'>; // true
type T2 = HasOverlap<'a', 'b'>; // false
type T3 = HasOverlap<number, string>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type HasOverlap<A, B> = [A & B] extends [never] ? false : true;
```
</details>

### Exercise 6: Flow-Sensitive Variable Cache (Hard)
- **Problem Statement:** Write a class `GuardedSlot<T>` that stores an optional value, providing an `isLoaded()` type guard method that narrows the instance type so that `getValue()` returns `T` without `undefined`.
- **Test Case:**
```typescript
const slot = new GuardedSlot<string>();
if (slot.isLoaded()) {
  const val: string = slot.getValue(); // Safe!
}
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class GuardedSlot<T> {
  private value: T | undefined;

  set(val: T): void {
    this.value = val;
  }

  isLoaded(): this is this & { getValue(): T } {
    return this.value !== undefined;
  }

  getValue(): T | undefined {
    return this.value;
  }
}
```
</details>
