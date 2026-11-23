# Types & Type Inference in TypeScript

> A deep architectural guide to TypeScript's structural type system, local and contextual type inference algorithms, literal widening, and `as const` immutability assertions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-setup/00-toolchain.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/00-toolchain.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0+ / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true`, `exactOptionalPropertyTypes: true` |

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

### Problem Statement
In dynamically typed JavaScript, variables hold arbitrary values whose structures mutate at runtime. Without a static type system, catching typos, invalid function arguments, and null pointer dereferences requires runtime execution. However, demanding manual type annotations on every variable declaration degrades developer velocity. A modern language requires an intelligent type inference engine that deduces precise types automatically.

### Historical Context
TypeScript introduced a **gradual, structural type system** (Duck Typing). Unlike nominal languages like Java or C# where an object must explicitly implement an interface, TypeScript verifies types purely based on their shape. In TS 3.4, `as const` assertions were introduced to prevent literal widening, followed by contextual inference improvements in TS 5.0+.

### Real-World Relevance
Type inference enables clean, readable code with zero boilerplate. Developers annotate function boundaries and public interfaces, while allowing the compiler to deduce local variable types, return types, and generic parameters automatically.

---

## 2. Core Concepts

### Structural Subtyping vs Nominal Typing
If object `A` has at least all the properties required by type `B` with compatible types, `A` is assignable to `B`, regardless of whether `A` explicitly declares a relationship with `B`.

```typescript
interface Point2D { x: number; y: number; }
interface NamedPoint { x: number; y: number; name: string; }

const p: NamedPoint = { x: 10, y: 20, name: 'Origin' };
const point: Point2D = p; // OK: Structural compatibility!
```

### Type Widening & Literal Types
When a variable is declared with `let`, TypeScript infers a widened mutable type (e.g., `string`). When declared with `const`, TypeScript infers the exact literal type (e.g., `'admin'`).

```typescript
let role = 'admin'; // Inferred as: string (widened)
const fixedRole = 'admin'; // Inferred as: 'admin' (literal)
```

---

## 3. How It Works Internally

### Contextual Typing & Bottom-Up vs Top-Down Flow
TypeScript uses two complementary inference strategies:
1. **Bottom-Up Inference (Synthesizing)**: Derives a type from the expression upwards (e.g. `1 + 2` $\to$ `number`).
2. **Top-Down Inference (Contextual Typing)**: The expected type from the surrounding context informs the type of an inner expression (e.g., inferring parameter types in an event handler callback).

```
Contextual Type Flow (Top-Down):
window.addEventListener('click', (event) => {
                                   ▲
                                   │ Contextually inferred as MouseEvent!
});
```

---

## 4. Syntax & API Reference

### Inference Modifiers

| Syntax | Behavior | Emitted JavaScript |
| :--- | :--- | :--- |
| `let x = val` | Widened primitive inference (`number`, `string`) | `let x = val;` |
| `const x = val` | Literal type inference (`10`, `"active"`) | `const x = val;` |
| `expr as const` | Deep readonly, narrowest literal inference | Stripped entirely |
| `satisfies T` | Validates against `T` without widening the inferred type | Stripped entirely |

---

## 5. Practical Examples

### Example 1: Contextual Typing in Array Callbacks
- **Goal:** Leverage contextual typing without writing explicit parameter types.
- **Code:**
```typescript
interface Task {
  id: string;
  priority: 'low' | 'high';
}

const tasks: Task[] = [
  { id: 't1', priority: 'high' },
  { id: 't2', priority: 'low' }
];

// 't' is contextually typed as Task; 'priority' is narrowed
const urgent = tasks.filter((t) => t.priority === 'high');
console.log('Urgent task count:', urgent.length);
```
- **Output:**
```
Urgent task count: 1
```

### Example 2: The `satisfies` Operator vs Type Annotation
- **Goal:** Ensure an object conforms to an interface while retaining exact literal property types.
- **Code:**
```typescript
type Palette = Record<string, string | number[]>;

// Using satisfies preserves RGB tuple structure:
const theme = {
  primary: '#3b82f6',
  accent: [59, 130, 246]
} satisfies Palette;

// 'theme.accent' retains tuple methods (map), not string | number[]!
const doubled = theme.accent.map((c) => c * 2);
console.log('RGB doubled:', doubled);
```
- **Output:**
```
RGB doubled: [ 118, 260, 492 ]
```

### Example 3: Deep Immutability via `as const`
- **Goal:** Freeze object shapes and prevent literal widening.
- **Code:**
```typescript
const AppConfig = {
  endpoint: 'https://api.internal.net',
  retryLimit: 3,
  headers: {
    'X-Client': 'web'
  }
} as const;

// AppConfig is:
// {
//   readonly endpoint: "https://api.internal.net";
//   readonly retryLimit: 3;
//   readonly headers: { readonly 'X-Client': "web" };
// }
console.log('Config endpoint:', AppConfig.endpoint);
```

### Example 4: Negative Test — Excess Property Checking
- **Goal:** Demonstrate compiler error when passing excess properties in object literals.
- **Code:**
```typescript
interface Point {
  x: number;
  y: number;
}

// @ts-expect-error TS2353: Object literal may only specify known properties
const p: Point = {
  x: 10,
  y: 20,
  z: 30
};
```
- **Real Compiler Error:**
```
error TS2353: Type '{ x: number; y: number; z: number; }' is not assignable to type 'Point'.
  Object literal may only specify known properties, and 'z' does not exist in type 'Point'.
```
- **Explanation:** Although TypeScript is structurally typed, fresh object literals undergo **Excess Property Checks** to prevent accidental misspellings.

### Example 5: Inferred Return Types vs Explicit Boundaries
- **Goal:** Demonstrate when to omit vs when to enforce explicit return types.
- **Code:**
```typescript
// Explicit boundary for exported public API
export function createSessionToken(userId: string): { token: string; expiresAt: number } {
  return {
    token: `tok_${userId}_${Math.random().toString(36).slice(2)}`,
    expiresAt: Date.now() + 3600000
  };
}

// Inferred internal helper:
function formatPrefix(prefix: string) {
  return `[SESSION-${prefix.toUpperCase()}]`;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Widening in Object Properties**: `const x = { role: 'admin' };` infers `role: string` (widened), NOT `'admin'`. If you need the literal, write `as const` or `: { role: 'admin' }`.
2. **Freshness Invalidation**: Excess property checking triggers **only on fresh object literals**. If you assign the literal to an intermediate variable first, excess property checking is bypassed!
3. **Empty Array Inference**: `const arr = [];` infers `any[]`. In strict mode (`noImplicitAny`), pushing items to it later may produce `TS7005: Variable 'arr' implicitly has an 'any[]' type`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Real Compiler Issue | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Over-annotating obvious locals (`const x: number = 10`) | Redundant code, visual clutter | Rely on type inference: `const x = 10` | Cleaner code; easier refactoring |
| Type asserting instead of `satisfies` (`x as Config`) | Disables excess property checks silently | Use `x satisfies Config` | Validates shape without losing literal types |
| Mutating an object inferred with `as const` | `TS2540: Cannot assign to 'x' because it is a read-only property` | Clone object or remove `as const` | Preserves immutability guarantees |
| Declaring empty arrays without type (`let arr = []`) | Inferred as `never[]` or `any[]` | Type explicitly: `let arr: string[] = []` | Prevents subsequent insertion type errors |
| Relying on nominal type safety | Allows unexpected object pass-through | Use Branded Types (`type USD = number & { __brand: 'USD' }`) | Enforces true semantic boundaries |

---

## 8. Performance & Type-Checker Complexity

- **Inference Cost**: Inferring return types across 10-level nested functions forces `tsc` to evaluate entire call trees before checking callers.
- **Explicit Return Types on Exports**: Providing explicit return types on all exported module functions dramatically accelerates `--isolatedDeclarations` and allows parallel type-checking across project boundaries.

---

## 9. Compiler Internals

Within V8 and the TypeScript Type Checker (`src/compiler/checker.ts`):
1. The function `checkExpressionWorker` evaluates the AST expression.
2. If checking an identifier declared with `const`, it assigns a `FreshLiteralType`.
3. If passed across an assignment, `widenType` converts `FreshLiteralType` to the base primitive `TypeFlags.String` or `TypeFlags.Number`.

---

## 10. Best Practices & Design Guidelines

1. **Annotate Module Boundaries, Infer Internals**: Place explicit types on exported function parameters and return types; let local variables infer.
2. **Use `satisfies` for Configuration Objects**: Keeps exact autocomplete paths while guaranteeing schema compliance.
3. **Avoid Unnecessary Type Assertions (`as`)**: Prefer type narrowing or `satisfies`.

---

## 11. Real-World Case Study

A high-traffic e-commerce checkout service used `const endpoints: Record<string, string>` for API routing. Developers frequently lost autocomplete for specific routes and suffered runtime crashes when accessing misspelled keys (`endpoints.checkuot`).

### Solution:
Replaced the annotation with `satisfies Record<string, string>`:
```typescript
const routes = {
  checkout: '/v1/checkout',
  payment: '/v1/payment'
} satisfies Record<string, string>;

// routes.checkout is inferred as literal "/v1/checkout", and routes.checkuot is a compile error!
```
**Outcome**: Eliminated 100% of route-typo bugs in the checkout pipeline.

---

## 12. Interview Questions

### Q1: What is structural subtyping and how does it differ from nominal subtyping?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In nominal subtyping (Java, C++), type compatibility is determined by explicit declarations: class `A` is only compatible with interface `B` if `A` explicitly states `implements B`.
In structural subtyping (TypeScript), compatibility is determined solely by the object's shape: if object `A` has all the properties and methods required by type `B`, `A` is assignable to `B` regardless of its origin.
</details>

### Q2: Why does TypeScript enforce excess property checking on object literals?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

While TypeScript is structural, passing an inline object literal with extra properties (`{ x: 1, y: 2, z: 3 }` to `Point2D { x, y }`) almost always indicates a developer mistake (e.g., a misspelled optional property). Excess property checking flags this mistake immediately. It only triggers on "fresh" object literals directly assigned to a type.
</details>

### Q3: What is the exact difference between `as const` and `Object.freeze()`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`Object.freeze()` is a JavaScript runtime function that shallowly freezes an object so properties cannot be added or mutated at runtime. It does not freeze nested objects and only types the top-level keys as readonly.
`as const` is a purely compile-time TypeScript assertion. It deeply marks all nested properties as `readonly`, infers literal types for all primitives, and is erased completely with zero runtime overhead.
</details>

### Q4: How does the `satisfies` operator differ from a standard type annotation?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A type annotation (`const x: T = ...`) forces the variable `x` to be typed as `T`, widening specific literal properties and discarding detailed inference.
The `satisfies` operator (`const x = ... satisfies T`) verifies that the expression matches type `T`, but allows `x` to retain its precise, narrow inferred type.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Fix Literal Widening
- **Problem Statement**: Prevent `config.env` from widening to `string`:
```typescript
const config = { env: 'production' };
```
- **Solution**:
```typescript
const config = { env: 'production' } as const;
```

### Exercise 2 (Medium): Validate Palette using `satisfies`
- **Problem Statement**: Ensure `colors` conforms to `Record<string, string>` while preserving hex string literals.
- **Solution**:
```typescript
const colors = {
  primary: '#1e40af',
  secondary: '#64748b'
} satisfies Record<string, string>;
```

### Exercise 3 (Hard): Emulate Nominal Typing via Brand Pattern
- **Problem Statement**: Author two nominal types: `UserId` and `OrderId` using branded numbers that cannot be passed to one another.
- **Solution**:
```typescript
export type UserId = number & { readonly __brand: unique symbol };
export type OrderId = number & { readonly __brand: unique symbol };

export function makeUserId(id: number): UserId { return id as UserId; }
export function makeOrderId(id: number): OrderId { return id as OrderId; }

const user = makeUserId(1);
const order = makeOrderId(1);

function shipOrder(targetOrder: OrderId) { /* ... */ }

shipOrder(order); // OK
// @ts-expect-error TS2345: Type 'UserId' is not assignable to 'OrderId'
shipOrder(user);
```
