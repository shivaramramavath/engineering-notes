# Generic Constraints and Defaults

> Generic constraints (`extends`) restrict type parameters to verified structural shapes, while type parameter defaults provide fallback substitutions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md), [01-generic-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/01-generic-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 2.3 (Generic Defaults) / TS 7.0 Native |
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
- **Problem Statement:** An unconstrained type parameter `<T>` assumes virtually nothing about the type (treating it as non-nullish `unknown` in strict mode). When an algorithm requires inspecting properties (e.g. `val.length`, `val.id`, or calling `val.toJSON()`), an unconstrained type parameter causes compile-time errors.
- **Historical Context:** TypeScript supported the `extends` constraint syntax from version 0.9. In TypeScript 2.3, generic type parameter defaults were added (`<T = DefaultType>`), allowing types and functions to provide fallback types when callers omit type arguments.
- **Real-World Relevance:** Constraints form the backbone of type-safe APIs, ensuring that functions can read mandatory properties while retaining the specific, narrow subtype of the caller.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Subtype Bounding: `<T extends BaseShape>` asserts that whatever type is passed or inferred for `T` must be assignable to `BaseShape`.
  - Property Accessibility: Inside the function or class body, all properties declared on `BaseShape` are statically accessible on values of type `T`.
  - Subtype Preservation: Unlike taking a parameter directly as `val: BaseShape`, constraining `T extends BaseShape` preserves the *exact specific subtype* returned to the caller.
  - Multiple Constraints via Intersection: To constrain `T` to multiple shapes, use intersection: `<T extends HasId & HasTimestamp>`.
  - Cross-Parameter Constraints: A type parameter can be constrained by another type parameter declared earlier in the same list: `<T, K extends keyof T>`.
  - Default Fallbacks: `<T = string>` specifies a fallback type if no type argument is provided and none can be inferred.
- **Mental Model:** A constraint is an entry requirement. "You may bring any type into this function, provided it satisfies this minimum structural checklist."
- **Soundness vs Ergonomics:** Constraining a generic parameter preserves full structural soundness while allowing callers to receive their exact input types back.

---

## 3. How It Works Internally
- **Constraint Solver:** In `checker.ts`, `getTypeParameterConstraint` retrieves the declared upper bound. During type argument resolution, `checkTypeRelatedTo` tests if the inferred or supplied type argument satisfies the constraint.
- **Subtype Retention:** If `T extends { id: string }`, and the caller passes `{ id: "1", name: "Alice", age: 30 }`, `T` is inferred as the full 3-property object literal, NOT truncated to `{ id: string }`.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Subtype Preservation vs Direct Parameter                |
|                                                                          |
|   Approach A: Direct Base Parameter                                      |
|   function save(item: HasId): HasId { return item; }                     |
|   const u = save({ id: "1", role: "admin" });                            |
|   u.role; // ERROR: 'role' does not exist on 'HasId' (Type truncated!)   |
|                                                                          |
|   Approach B: Generic Constraint                                         |
|   function save<T extends HasId>(item: T): T { return item; }            |
|   const u = save({ id: "1", role: "admin" });                            |
|   u.role; // SAFE: Returns exact type { id: string, role: string }!      |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Basic Constraint:**
  ```typescript
  function getLength<T extends { length: number }>(item: T): number {
    return item.length;
  }
  ```
- **Keyof Constraint:**
  ```typescript
  function getProperty<T, K extends keyof T>(obj: T, key: K): T[K] {
    return obj[key];
  }
  ```
- **Multiple Constraints with Defaults:**
  ```typescript
  interface Identifiable { id: string; }
  interface Auditable { createdAt: Date; }

  type EntityDTO<T extends Identifiable & Auditable = Identifiable & Auditable> = {
    entity: T;
    exportedAt: Date;
  };
  ```
- **tsconfig Dependencies:** Operates under standard configurations.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Author a function requiring objects to possess an `id` property while preserving extra fields.
- **Code:**
```typescript
interface HasId {
  id: string;
}

export function indexById<T extends HasId>(items: readonly T[]): Map<string, T> {
  const map = new Map<string, T>();
  for (const item of items) {
    map.set(item.id, item);
  }
  return map;
}

type User = { id: string; email: string; role: 'admin' | 'user' };
const users: User[] = [
  { id: 'u1', email: 'alice@corp.com', role: 'admin' },
  { id: 'u2', email: 'bob@corp.com', role: 'user' },
];

const userMap = indexById(users);
const alice = userMap.get('u1');
// alice is inferred as User | undefined; role and email accessible!
console.log(alice?.role); // "admin"
```
- **Output / Type Evaluation:**
```typescript
// userMap is Map<string, User>; exact fields preserved.
```
- **Explanation:** Constraining `T extends HasId` guarantees `item.id` exists while keeping the full `User` type intact.

### Example 2: Real-World Enterprise Pattern (Type-Safe Event Bus with Keyof Constraint)
- **Goal:** Ensure event names strictly match event payload signatures.
- **Code:**
```typescript
export class StronglyTypedEmitter<Events extends Record<string, any>> {
  private listeners: { [K in keyof Events]?: ((payload: Events[K]) => void)[] } = {};

  on<K extends keyof Events>(event: K, listener: (payload: Events[K]) => void): void {
    if (!this.listeners[event]) {
      this.listeners[event] = [];
    }
    this.listeners[event]!.push(listener);
  }

  emit<K extends keyof Events>(event: K, payload: Events[K]): void {
    const list = this.listeners[event];
    if (list) {
      for (const fn of list) fn(payload);
    }
  }
}

type AppEvents = {
  login: { userId: string; timestamp: number };
  logout: { userId: string };
};

const bus = new StronglyTypedEmitter<AppEvents>();
bus.on('login', (data) => console.log(data.userId, data.timestamp));
bus.emit('login', { userId: 'u_10', timestamp: Date.now() });
```
- **Output / Type Evaluation:**
```typescript
// End-to-end autocomplete and validation on event names and payloads.
```
- **Explanation:** The constraint `K extends keyof Events` couples the event name to its corresponding payload type in `Events[K]`.

### Example 3: Edge Case / Boundary Condition (Generic Defaults in Functional Options)
- **Goal:** Provide fallback type parameters for flexible API callers.
- **Code:**
```typescript
export interface QueryResponse<T = Record<string, unknown>> {
  data: T;
  executionMs: number;
}

// Caller 1: Uses default Record<string, unknown>
const untypedQuery: QueryResponse = {
  data: { raw: 'untyped' },
  executionMs: 12,
};

// Caller 2: Supplies specific DTO
type MetricData = { cpuUsage: number; memFree: number };
const typedQuery: QueryResponse<MetricData> = {
  data: { cpuUsage: 45.2, memFree: 1024 },
  executionMs: 8,
};
```
- **Output / Type Evaluation:**
```typescript
// untypedQuery.data is Record<string, unknown>; typedQuery.data is MetricData.
```
- **Explanation:** Defaults allow types to be used bare when payload shapes are dynamic, while supporting strict typing when contracts are known.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection when passing an argument that violates a constraint.
- **Code:**
```typescript
function printLength<T extends { length: number }>(val: T): void {
  console.log(val.length);
}

printLength('hello'); // OK: string has length
printLength([1, 2]);   // OK: array has length

// @ts-expect-error TS2345: Argument of type 'number' is not assignable to parameter of type '{ length: number; }'.
printLength(12345);
```
- **Real Compiler Error:**
```
error TS2345: Argument of type 'number' is not assignable to parameter of type '{ length: number; }'.
  Property 'length' is missing in type 'Number'.
```
- **Explanation:** Primitive numbers do not have a `length` property, violating the structural constraint.

### Example 5: Idiomatic Modern Usage (Immutable Patching with Key Constraints)
- **Goal:** Safely update an object property without losing immutability.
- **Code:**
```typescript
export function patchProperty<T extends object, K extends keyof T>(
  obj: T,
  key: K,
  value: T[K]
): T {
  return {
    ...obj,
    [key]: value,
  };
}

type Config = { host: string; port: number; secure: boolean };
const server: Config = { host: 'localhost', port: 8080, secure: false };

const updated = patchProperty(server, 'port', 9000);
console.log(updated.port); // 9000

// @ts-expect-error TS2345: Argument of type '"9000"' is not assignable to parameter of type 'number'.
patchProperty(server, 'port', '9000');
```
- **Output / Type Evaluation:**
```typescript
// TS2345 prevents assigning string to number property 'port'.
```
- **Explanation:** `value: T[K]` dynamically constrains the new value to the exact type of property `K` on object `T`.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Constraint Does Not Limit Fresh Return Properties:** If `T extends { id: string }`, returning `{ ...val, extra: true }` typed as `T` causes `TS2322: Type is not assignable to type 'T'`. The caller chose `T`, so the function cannot synthesize an arbitrary subtype of `T`.
2. **Keyof on Primitives:** Running `keyof number` produces methods on `Number.prototype` (`"toFixed" | "toExponential" | ...`). Ensure `T` is constrained with `T extends object` if only object keys are desired.
3. **Defaults Must Follow Required Type Parameters:** Declaring `<T = string, U>` causes `TS2706: Required type parameter may not follow an optional type parameter`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Truncating return types to constraint interface | Loses caller fields (returns `HasId` instead of `T`) | `<T extends HasId>(item: T): T` | Preserves caller subtype properties |
| Returning new object literal as `T` | `TS2322: Type '{ id }' not assignable to 'T'` | Return concrete type or cast | Callers define `T`; functions cannot fabricate `T` |
| `keyof T` without `T extends object` | Includes `toString`, `valueOf` on primitives | `<T extends object, K extends keyof T>` | Restricts keys to true object properties |
| Placing default before required generic | `TS2706: Required parameter after optional` | Move parameters with defaults to the end | Enforces positional type argument order |
| Over-constraining with multiple classes | Rejects valid structural shapes | Constrain with interfaces or object shapes | Preserves duck-typing flexibility |
| Forgetting to constrain key argument | `TS7053: No index signature` | Constrain key with `K extends keyof T` | Guarantees key exists on target object |

---

## 8. Performance & Type-Checker Complexity
- **Constraint Unification Depth:** When a generic function has multiple interconnected constraints (`T extends A, K extends keyof T, V extends T[K]`), the compiler solves constraints in topological order. Keep constraint chains under 3 levels to maintain fast compilation.
- **Isolated Declarations:** Full compatibility with `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Representation:** `ts.TypeParameterDeclaration` with `constraint` and `default` properties.
- **Constraint Resolution:** Handled in `checker.ts` by `getBaseConstraintOfType`.
- **Diagnostic Generation:** Flags `Diagnostics.Type_0_does_not_satisfy_the_constraint_1` (TS2344).

---

## 10. Best Practices & Design Guidelines
- **Constrain Generics to the Minimum Necessary Shape:** Only require properties your function actually uses (e.g. `{ id: string }`).
- **Use Cross-Parameter Constraints for Key Lookups:** Always pair `obj: T` with `key: K extends keyof T`.
- **Provide Defaults for Ergonomics:** Add sensible defaults to generic types when an obvious fallback exists.

---

## 11. Real-World Case Study
- **Scenario:** A core analytics framework collected user interaction metrics. A library author wrote a recording function: `function trackEvent(event: BaseEvent): BaseEvent`. When developers passed specialized events like `CheckoutEvent` (containing `cartTotal` and `items`), the function returned `BaseEvent`. Downstream pipelines lost all specialized fields and had to write unsafe `as CheckoutEvent` assertions, causing 3 production deployment failures when event structures were refactored.
- **Implementation:** Refactored the function to use generic constraints:
  ```typescript
  // Before (lost specialized fields):
  function trackEvent(event: BaseEvent): BaseEvent;

  // After (preserves exact caller event type):
  function trackEvent<T extends BaseEvent>(event: T): T {
    metricsEngine.record(event.name, event.timestamp);
    return event;
  }
  ```
- **Outcome:** Subtype fields were 100% preserved at all call sites without type casting, eliminating unverified assertions across 80+ frontend services.

---

## 12. Interview Questions

### Q1: Why does returning `{ ...val, extra: 1 }` from a function `function addExtra<T extends object>(val: T): T` cause a compile error?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In TypeScript, `T` is chosen by the **caller**, not by the callee. The caller could instantiate `T` with a type that has a property `extra: string` (e.g. `{ id: 1, extra: "text" }`). Returning an object with `extra: 1` (a number) would violate the caller's expectation of `extra` being a string. Furthermore, `T` could be a class instance with private fields or methods that the plain object literal does not possess. Therefore, TypeScript correctly rejects synthesizing a value of type `T` from a fresh object literal. To fix this, return an intersection (`T & { extra: number }`).
</details>

### Q2: How does `K extends keyof T` ensure type-safe property access?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`keyof T` produces a union of all known public property keys of `T`. Constraining `K extends keyof T` guarantees that:
1. The argument passed as `key` is statically verified to be a valid property name of `obj`.
2. The return type can be precisely indexed as `T[K]`, extracting the exact type of that specific property rather than a wide union of all property types on `T`.
</details>

### Q3: What is the difference between `<T extends Base>` and simply using `Base` as a parameter type?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **Using `Base` directly (`fn(item: Base): Base`):** Truncates the type. If the caller passes a derived subtype `Sub` with extra fields, the return type is widened back to `Base`, losing all extra subtype properties.
- **Using `T extends Base` (`fn<T extends Base>(item: T): T`):** Preserves the exact subtype. If the caller passes `Sub`, `T` is inferred as `Sub`, and the return type remains `Sub`.
</details>

### Q4: How do default type parameters behave in generic functions versus generic types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- In generic types (e.g. `type Result<T, E = Error>`), defaults apply when the consumer omits the type argument (`Result<string>` uses `Error` for `E`).
- In generic functions (e.g. `function fetch<T = unknown>()`), defaults apply when the caller does not supply an explicit type argument AND the compiler cannot infer `T` from any value parameters.
</details>

### Q5: Can a type parameter be constrained by an intersection of multiple types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. You can write `<T extends InterfaceA & InterfaceB>`. The argument passed for `T` must satisfy all members of `InterfaceA` and all members of `InterfaceB` simultaneously.
</details>

### Q6: What happens if an optional generic type parameter is followed by a required generic type parameter?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript reports compile-time error `TS2706: Required type parameter may not follow an optional type parameter`. Just like function parameter lists, all type parameters with defaults must be placed after all required type parameters.
</details>

### Q7: Why should `T` be constrained to `object` when using `keyof T` in generic functions?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

If `T` is unconstrained, callers can pass primitive types like `number` or `string`. In JavaScript, `keyof number` evaluates to methods on `Number.prototype` (`"toFixed"`, `"toPrecision"`, etc.). Constraining `T extends object` guarantees that `T` is a non-primitive object shape, preventing unintended prototype method key access.
</details>

### Q8: How can you constrain a type parameter to only accept constructor functions?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

By constraining `T` to a construct signature:
```typescript
type ConstructorConstraint<T> = new (...args: any[]) => T;
function createInstance<C extends new (...args: any[]) => any>(ctor: C): InstanceType<C> {
  return new ctor();
}
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe Max Element Finder (Easy)
- **Problem Statement:** Author a function `findMax<T extends { value: number }>(items: readonly T[]): T | null` returning the item with the highest `value`.
- **Test Case:**
```typescript
const items = [{ id: 'a', value: 10 }, { id: 'b', value: 20 }];
console.log(findMax(items)?.id); // "b"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function findMax<T extends { value: number }>(items: readonly T[]): T | null {
  if (items.length === 0) return null;
  let maxItem = items[0]!;
  for (let i = 1; i < items.length; i++) {
    if (items[i]!.value > maxItem.value) {
      maxItem = items[i]!;
    }
  }
  return maxItem;
}
```
</details>

### Exercise 2: Key Property Picker (Easy)
- **Problem Statement:** Write a function `pluck<T extends object, K extends keyof T>(arr: readonly T[], key: K): T[K][]`.
- **Test Case:**
```typescript
const names = pluck([{ name: 'Alice' }, { name: 'Bob' }], 'name'); // ["Alice", "Bob"]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function pluck<T extends object, K extends keyof T>(
  arr: readonly T[],
  key: K
): T[K][] {
  return arr.map((item) => item[key]);
}
```
</details>

### Exercise 3: Constrained Entity Merger (Medium)
- **Problem Statement:** Write a function `mergeEntities<A extends object, B extends object>(a: A, b: B): A & B` that merges two objects preserving exact intersection types.
- **Test Case:**
```typescript
const res = mergeEntities({ id: 1 }, { name: 'Alice' });
console.log(res.id, res.name); // 1, "Alice"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function mergeEntities<A extends object, B extends object>(
  a: A,
  b: B
): A & B {
  return { ...a, ...b };
}
```
</details>

### Exercise 4: Generic Comparator with Default (Medium)
- **Problem Statement:** Implement a function `sortObjects<T, K extends keyof T = keyof T>(arr: T[], key: K): T[]` that sorts objects by property `key`.
- **Test Case:**
```typescript
const sorted = sortObjects([{ rank: 3 }, { rank: 1 }], 'rank');
console.log(sorted[0]?.rank); // 1
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function sortObjects<T, K extends keyof T = keyof T>(
  arr: T[],
  key: K
): T[] {
  return [...arr].sort((a, b) => {
    if (a[key] < b[key]) return -1;
    if (a[key] > b[key]) return 1;
    return 0;
  });
}
```
</details>

### Exercise 5: Compile-Time Constraint Satisfier Checker (Hard)
- **Problem Statement:** Write a utility type `SatisfiesConstraint<T, Constraint>` that returns `true` if `T` satisfies `Constraint`, and a descriptive string error otherwise.
- **Test Case:**
```typescript
type T1 = SatisfiesConstraint<{ id: string }, { id: string }>; // true
type T2 = SatisfiesConstraint<number, { id: string }>; // "Error: Type does not satisfy constraint"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type SatisfiesConstraint<T, C> =
  T extends C ? true : 'Error: Type does not satisfy constraint';
```
</details>

### Exercise 6: Deep Key Property Extractor (Hard)
- **Problem Statement:** Author a function `getDeepProperty<T extends object, K1 extends keyof T, K2 extends keyof T[K1]>(obj: T, k1: K1, k2: K2): T[K1][K2]` that safely reads 2-level nested properties.
- **Test Case:**
```typescript
const conf = { db: { port: 5432 } };
const p = getDeepProperty(conf, 'db', 'port'); // 5432 (number)
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function getDeepProperty<
  T extends object,
  K1 extends keyof T,
  K2 extends keyof T[K1]
>(
  obj: T,
  k1: K1,
  k2: K2
): T[K1][K2] {
  return obj[k1][k2];
}
```
</details>
