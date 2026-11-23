# Exclude, Extract, and NonNullable

> Union filtering utilities in TypeScript use distributive conditional types to narrow, prune, and assert members within discriminated or primitive type unions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [05-generic-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/05-generic-inference.md), [01-primitive-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/01-primitive-types.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.8 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `strictNullChecks: true` |

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
TypeScript represents complex states, event types, and API responses as **unions** (e.g. `A | B | C`). In many architectural layers, code only cares about a specific sub-slice of a union:
- Handling only error responses in an error handler.
- Filtering out null and undefined values from optional inputs.
- Stripping internal action types before broadcasting to a WebSocket client.

To operate on union types mathematically (set difference and set intersection), TypeScript provides **`Exclude<T, U>`**, **`Extract<T, U>`**, and **`NonNullable<T>`**.

---

## 2. Core Concepts

### `Exclude<T, U>`: Set Difference
`Exclude<T, U>` removes from `T` all union members that are assignable to `U`:
```typescript
type AllEvents = "click" | "hover" | "scroll" | "keydown";
type MouseEvents = "click" | "hover";

type NonMouseEvents = Exclude<AllEvents, MouseEvents>;
// "scroll" | "keydown"
```

### `Extract<T, U>`: Set Intersection
`Extract<T, U>` extracts from `T` all union members that are assignable to `U`:
```typescript
type SharedEvents = Extract<AllEvents, "click" | "focus" | "blur">;
// "click"
```

### `NonNullable<T>`: Nullability Pruning
`NonNullable<T>` removes `null` and `undefined` from type `T`:
```typescript
type MaybeUser = string | null | undefined;
type ValidUser = NonNullable<MaybeUser>;
// string
```

---

## 3. How It Works Internally

```
Type Definitions in lib.es5.d.ts:

type Exclude<T, U> = T extends U ? never : T;

type Extract<T, U> = T extends U ? T : never;

type NonNullable<T> = T & {}; // Modern TS 4.8+ definition
// (Historically: T extends null | undefined ? never : T)
```

### Distribution Over Unions
Because `T` is a bare type parameter in `T extends U`, the compiler evaluates each union branch individually:
```
Exclude<"a" | "b", "a">
=> ("a" extends "a" ? never : "a") | ("b" extends "a" ? never : "b")
=> never | "b"
=> "b"  (never is eliminated from unions)
```

---

## 4. Syntax & API Reference

### Discriminated Union Filtering
```typescript
type Result =
  | { success: true; data: string[] }
  | { success: false; error: Error };

// Extract success variant
type SuccessResult = Extract<Result, { success: true }>;
// { success: true; data: string[] }

// Exclude success variant to get failures
type FailureResult = Exclude<Result, { success: true }>;
// { success: false; error: Error }
```

### Modern `NonNullable<T>` with `T & {}`
In TypeScript 4.8+, `NonNullable<T>` was upgraded to use intersection with empty object `T & {}`. In TypeScript, `{}` matches any value *except* `null` and `undefined`, yielding cleaner intersection simplifications.

---

## 5. Practical Examples

### 5.1 Basic Union Pruning
```typescript
type Status = "draft" | "review" | "published" | "archived";
type ActiveStatus = Exclude<Status, "archived">; // "draft" | "review" | "published"
```

### 5.2 Real-World Application: Strict Discriminated Event Router
```typescript
type DomainEvent =
  | { kind: "USER_REGISTERED"; userId: string; email: string }
  | { kind: "PASSWORD_CHANGED"; userId: string }
  | { kind: "ORDER_PLACED"; orderId: string; amount: number };

function handleUserEvents(event: Extract<DomainEvent, { userId: string }>): void {
  console.log("Processing user event for:", event.userId);
}

handleUserEvents({ kind: "USER_REGISTERED", userId: "u_1", email: "a@b.com" });
// handleUserEvents({ kind: "ORDER_PLACED", orderId: "o_1", amount: 100 }); // Error!
```

### 5.3 Edge Case: Primitive Filtering
```typescript
type MixedUnion = string | number | (() => void) | boolean;

type FunctionsOnly = Extract<MixedUnion, Function>; // () => void
type Serializable = Exclude<MixedUnion, Function>;   // string | number | boolean
```

### 5.4 Failure Case: Passing Incompatible Shapes
```typescript
type Action = { type: "LOAD" } | { type: "SAVE" };

// @ts-expect-error TS2322: Type '{ type: "UNKNOWN"; }' is not assignable to type 'Action'.
const a: Extract<Action, { type: "UNKNOWN" }> = { type: "UNKNOWN" }; // Evaluates to never!
```

### 5.5 Idiomatic Pattern: Type-Safe Array Compact
```typescript
function compactArray<T>(items: Array<T>): Array<NonNullable<T>> {
  return items.filter((x): x is NonNullable<T> => x !== null && x !== undefined);
}

const mixed = [1, null, "two", undefined, 3];
const clean = compactArray(mixed); // Array<string | number>
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Never Output**: When `Extract` finds no matches, it evaluates to `never`. Assigning any value to `never` fails.
2. **Object Types with `Extract`**: `Extract<T, Pattern>` uses assignability. If `Pattern` has fewer properties than `T`, any variant containing those properties matches.
3. **`NonNullable` does not remove empty string or 0**: It only removes `null` and `undefined`, not falsy primitives like `""` or `0` or `NaN`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Expecting `NonNullable` to prune empty strings | Only strips `null` and `undefined` | Use `Exclude<T, "" | 0 | false>` | Falsy values persist |
| Applying `Exclude` to object interfaces | `Exclude` filters unions, not object properties | Use `Omit<T, K>` for properties | Evaluates to `never` |
| `Extract` with too broad pattern | Matches more union variants than intended | Use exact literal property match | Accidental union expansion |
| Using `== null` check without updating types | Runtime filter does not narrow array type | Use custom type guard (`x is NonNullable<T>`) | `TS2322: Type 'undefined' not assignable...` |
| Over-filtering to `never` | Consuming code becomes unreachable | Ensure union includes fallback branch | Dead code / `never` leak |
| Expecting deep null stripping | Only removes top-level `null` | Author recursive `DeepNonNullable` | Nested `null` remains |

---

## 8. Performance & Type-Checker Complexity
- Distributive conditional types scale linearly with union size ($O(N)$).
- Avoid applying `Exclude` over unions with thousands of literal members (e.g. huge enum unions) in recursive loops.

---

## 9. Compiler Internals
- `Exclude` and `Extract` parse as conditional types (`ConditionalTypeNode`).
- Handled by `getTypeFromConditionalType` in `src/compiler/checker.ts`.
- Because the check type is a distributive type parameter, the compiler iterates over the constituent union members and invokes `checkTypeRelatedTo(member, target)`.

---

## 10. Best Practices & Design Guidelines
- Use `Extract` to derive specific payloads from discriminated unions.
- Use `Exclude` to author transition guards and state machines (e.g. `Exclude<State, "TERMINATED">`).
- Combine `NonNullable<T>` with user-defined type guards when filtering arrays.

---

## 11. Real-World Case Study
### State Transition Guard in Financial Transaction Engine
```typescript
type TxState =
  | "PENDING_AUTH"
  | "AUTHORIZED"
  | "CAPTURING"
  | "SETTLED"
  | "FAILED"
  | "REFUNDED";

// Transactions that can still be cancelled
type CancellableState = Exclude<TxState, "SETTLED" | "FAILED" | "REFUNDED">;

function cancelTransaction(state: CancellableState, txId: string): void {
  console.log(`Cancelling transaction ${txId} in state ${state}`);
}

cancelTransaction("PENDING_AUTH", "tx_01"); // OK
cancelTransaction("AUTHORIZED", "tx_02");   // OK
// cancelTransaction("SETTLED", "tx_03");   // Error: TS2345
```

---

## 12. Interview Questions

<details>
<summary>1. How do <code>Exclude</code> and <code>Extract</code> differ?</summary>

Both operate on union types via distributive conditional types. `Exclude<T, U>` removes union members of `T` that are assignable to `U` (difference). `Extract<T, U>` keeps only the union members of `T` that are assignable to `U` (intersection).
</details>

<details>
<summary>2. Why does <code>Exclude&lt;T, U&gt;</code> evaluate to <code>never</code> when all members match?</summary>

Because matching members evaluate to `never` (`T extends U ? never : T`). In TypeScript, `never` in a union represents an empty set and is eliminated (`never | never === never`).
</details>

<details>
<summary>3. How does modern TypeScript (4.8+) define <code>NonNullable&lt;T&gt;</code>?</summary>

TypeScript 4.8+ defines `NonNullable<T> = T & {}`. Because `{}` matches any non-nullish value, intersecting `T` with `{}` eliminates `null` and `undefined` directly.
</details>

<details>
<summary>4. What is the difference between <code>Exclude</code> and <code>Omit</code>?</summary>

`Exclude<T, U>` filters **members of a union type**. `Omit<T, K>` filters **property keys from an object type**.
</details>

<details>
<summary>5. Can <code>Extract</code> be used to narrow a discriminated union by a single property?</summary>

Yes: `Extract<Union, { type: "SPECIFIC" }>` isolates only union members that have `type: "SPECIFIC"`.
</details>

<details>
<summary>6. Does <code>NonNullable&lt;T&gt;</code> remove empty objects or empty arrays?</summary>

No. Empty objects `{}` and empty arrays `[]` are valid truthy objects in JavaScript and are not `null` or `undefined`.
</details>

<details>
<summary>7. What happens when you apply <code>Exclude&lt;string | number, boolean&gt;</code>?</summary>

Since neither `string` nor `number` is assignable to `boolean`, neither member is excluded. The result is `string | number`.
</details>

<details>
<summary>8. How do you extract only function properties from an interface?</summary>

Combine `keyof` with `Extract` or conditional mapping:
```typescript
type FunctionKeys<T> = { [K in keyof T]: T[K] extends Function ? K : never }[keyof T];
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Filter Nullable Elements (Easy)
Filter `null` and `undefined` from `string | number | null | undefined`.
<details>
<summary>Solution</summary>

```typescript
type Raw = string | number | null | undefined;
type Clean = NonNullable<Raw>; // string | number
```
</details>

### Exercise 2: Extract Error Actions (Easy)
Extract all error action shapes from a Redux action union.
<details>
<summary>Solution</summary>

```typescript
type Action =
  | { type: "FETCH_START" }
  | { type: "FETCH_SUCCESS"; data: string }
  | { type: "FETCH_ERROR"; error: Error };

type ErrorAction = Extract<Action, { error: any }>;
// { type: "FETCH_ERROR"; error: Error }
```
</details>

### Exercise 3: Filter Primitives from Union (Medium)
Create a utility `FilterPrimitives<T>` that excludes all primitive types (`string`, `number`, `boolean`, `symbol`, `bigint`).
<details>
<summary>Solution</summary>

```typescript
type Primitive = string | number | boolean | symbol | bigint | null | undefined;
type FilterPrimitives<T> = Exclude<T, Primitive>;

type Mixed = string | { id: number } | number | [boolean];
type ObjectsOnly = FilterPrimitives<Mixed>; // { id: number } | [boolean]
```
</details>

### Exercise 4: Discriminated Union Event Handler (Medium)
Write a function that accepts only non-terminal status transitions from a union.
<details>
<summary>Solution</summary>

```typescript
type JobStatus = "queued" | "running" | "completed" | "failed" | "cancelled";

type TerminalStatus = "completed" | "failed" | "cancelled";
type ActiveJobStatus = Exclude<JobStatus, TerminalStatus>;

function notifyActive(status: ActiveJobStatus): void {
  console.log("Job still in progress:", status);
}

notifyActive("running"); // OK
// notifyActive("completed"); // Compile error
```
</details>

### Exercise 5: Deep NonNullable (Hard)
Implement a recursive `DeepNonNullable<T>` that recursively strips `null` and `undefined` from all nested object properties and array elements.
<details>
<summary>Solution</summary>

```typescript
type DeepNonNullable<T> = T extends (...args: any[]) => any
  ? T
  : T extends Array<infer U>
  ? Array<DeepNonNullable<NonNullable<U>>>
  : T extends object
  ? { [K in keyof T]: DeepNonNullable<NonNullable<T[K]>> }
  : NonNullable<T>;

type Messy = {
  a?: string | null;
  b: Array<number | null | undefined>;
};
type Pristine = DeepNonNullable<Messy>;
// { a: string; b: Array<number> }
```
</details>

### Exercise 6: Invert Union Extraction (Hard)
Write a utility `Difference<A, B>` that computes symmetric difference between two unions (members in A or B, but not both).
<details>
<summary>Solution</summary>

```typescript
type Difference<A, B> = Exclude<A, B> | Exclude<B, A>;

type U1 = "a" | "b" | "c";
type U2 = "b" | "c" | "d";

type SymDiff = Difference<U1, U2>; // "a" | "d"
```
</details>
