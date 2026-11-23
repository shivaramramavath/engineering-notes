# Senior & Staff Interview Guide: Advanced Type Metaprogramming & Inference

> Interview guide on advanced type-level metaprogramming: template literal types, `infer` mechanics, tuple manipulation, and recursive compiler constraints.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [10-advanced-types/00-conditional-types.md](../10-advanced-types/00-conditional-types.md), [10-advanced-types/02-template-literal-types.md](../10-advanced-types/02-template-literal-types.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [Core Architectural Concepts](#1-core-architectural-concepts)
2. [Deep-Dive Interview Questions & Answers](#2-deep-dive-interview-questions--answers)
3. [Output Prediction & Code Analysis Questions](#3-output-prediction--code-analysis-questions)
4. [Architecture & System Design Scenario](#4-architecture--system-design-scenario)
5. [Senior Technical Evaluation Rubric](#5-senior-technical-evaluation-rubric)

---

## 1. Core Architectural Concepts

Senior interview criteria:
- **`infer` Keyword**: Extracting inner type representations from function signatures, promises, and template strings.
- **Template Literal Types**: Pattern matching and string manipulation at compile time.
- **Recursive Conditional Types**: Accumulator patterns for tail-call optimization (TS 4.5+).
- **TypeScript Turing-Completeness**: Handling depth limits and compilation bounds.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: How does the `infer` keyword work in conditional types, and can multiple `infer` declarations be used simultaneously?</summary>

**Answer**:
The `infer` keyword introduces a temporary type variable within the `extends` clause of a conditional type. The compiler infers the variable's type based on pattern matching:
```typescript
type UnpackPromise<T> = T extends Promise<infer R> ? R : T;
```
Multiple `infer` declarations can be combined in a single statement. If the same inferred variable appears in multiple positions:
- In **covariant** positions (return values), the inferred type is a **union**:
  `type Union<T> = T extends { a: infer U, b: infer U } ? U : never;`
- In **contravariant** positions (function parameters), the inferred type is an **intersection**!
</details>

<details>
<summary>Q2: How can TypeScript parse URL query parameters entirely in the type system?</summary>

**Answer**:
Using recursive template literal pattern matching with `infer`:
```typescript
type ParseQuery<S extends string> =
  S extends `${infer Key}=${infer Val}&${infer Rest}`
    ? Record<Key, Val> & ParseQuery<Rest>
    : S extends `${infer Key}=${infer Val}`
      ? Record<Key, Val>
      : {};
```
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Problem: Covariant vs Contravariant `infer` Intersection
```typescript
type UnionToIntersection<U> =
  (U extends any ? (k: U) => void : never) extends (k: infer I) => void
    ? I
    : never;

type T = UnionToIntersection<{ a: string } | { b: number }>;
// What is type T?
```
**Evaluation & Explanation**:
- **Output**: `{ a: string } & { b: number }`
- **Explanation**: The type first distributes `U` into a union of function types `((k: { a: string }) => void) | ((k: { b: number }) => void)`. Then, matching against `(k: infer I) => void` places `I` in a **contravariant** parameter position. Multiple candidate types in contravariant positions are resolved as an **intersection**.

---

## 4. Architecture & System Design Scenario

**Scenario**: You are designing an event bus where events are formatted as `"namespace:action"`. You must enforce that listeners can subscribe to wildcards (e.g. `"user:*"`).

**Senior Solution**:
```typescript
type EventName = "user:login" | "user:logout" | "order:created" | "order:paid";

type Wildcard<T extends string> = T extends `${infer NS}:${string}` ? `${NS}:*` : never;

type SubscribableEvent = EventName | Wildcard<EventName>;
// SubscribableEvent is: "user:login" | "user:logout" | "order:created" | "order:paid" | "user:*" | "order:*"
```

---

## 5. Senior Technical Evaluation Rubric

| Level | Performance Characteristics |
| :--- | :--- |
| **Junior / Mid** | Struggles with `infer`; cannot manipulate string literal types. |
| **Senior** | Implements template literal parsers; writes `UnionToIntersection` fluently. |
| **Staff / Principal** | Understands V8 instantiation costs of recursive metaprogramming; establishes boundaries to prevent compiler slowdowns. |
