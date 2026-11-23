# Senior & Staff Interview Guide: TypeScript Basics and Type System Fundamentals

> Comprehensive technical interview mastery guide covering core TypeScript fundamentals: type annotations, type inference, primitives vs objects, and JavaScript runtime interactions.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [01-fundamentals/01-primitive-types.md](../01-fundamentals/01-primitive-types.md) |
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

Interviewers evaluating senior engineers on TypeScript basics look for a clear understanding of:
- **Structural Subtyping (Duck Typing)** vs. Nominal Subtyping.
- **Type Erasure**: Complete separation between static compile-time algebra and V8 runtime execution.
- **Top and Bottom Types**: The roles of `any`, `unknown`, `never`, and `void`.
- **Literal Widening**: How `const` vs `let` initializes immutable singleton types vs widened primitives.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: How does TypeScript's type system differ fundamentally from nominal type systems like Java or C++?</summary>

**Answer**:
TypeScript uses a **structural type system** (often called compile-time duck typing). In nominal systems (Java, C++, C#), two types are only assignable if they share an explicit named declaration hierarchy (e.g. `class Dog extends Animal`).

In TypeScript, assignability is determined exclusively by the shape (structure) of the data. If object $A$ has all the properties required by type $B$ with compatible types, $A$ is assignable to $B$, regardless of whether $A$ was declared as a class, an interface, or an anonymous literal:
```typescript
interface Point2D { x: number; y: number; }
interface Vector2D { x: number; y: number; }

const p: Point2D = { x: 10, y: 20 };
const v: Vector2D = p; // Valid! Structural equivalence.
```
This design was chosen to naturally model idiomatic JavaScript, where objects are constructed dynamically.
</details>

<details>
<summary>Q2: What are Top and Bottom types, and how do `any`, `unknown`, and `never` fit into the type lattice?</summary>

**Answer**:
In type theory:
1. **Top Type ($\top$)**: A type that is a supertype of every type in the system. Every value is assignable to the Top type.
   - `unknown`: Safe Top type. You can assign anything to `unknown`, but you cannot perform any operations on it without narrowing.
   - `any`: Escape hatch Top type that simultaneously acts as a Bottom type (bidirectional assignability), effectively disabling the type checker.
2. **Bottom Type ($\bot$)**: A type that has no values and is a subtype of every type in the system.
   - `never`: Represents values that can never occur (e.g., functions that throw or enter infinite loops, or exhaustive switch branches).
</details>

<details>
<summary>Q3: Explain the difference between `interface` and `type` alias in modern TypeScript.</summary>

**Answer**:
In TypeScript 5.x, `interface` and `type` are mostly interchangeable, but maintain key architectural distinctions:
1. **Declaration Merging**: Multiple `interface` declarations with the same name in the same scope merge automatically. `type` aliases throw a duplicate identifier error.
2. **Compiler Symbol Caching**: Interfaces create a stable `ts.Symbol` in the type checker, allowing $O(1)$ identity checks during assignability tests. Complex intersections (`A & B`) synthesized with `type` cannot be cached by identity.
3. **Capabilities**: `type` can declare unions (`A | B`), primitives (`type ID = string`), tuples, and mapped types. `interface` can only describe object shapes and callable signatures.
</details>

<details>
<summary>Q4: What is Literal Widening and how does `as const` prevent it?</summary>

**Answer**:
When a variable is declared with `let`, TypeScript infers a widened mutable primitive type:
```typescript
let x = "https"; // Inferred as: string
```
When declared with `const`, TypeScript infers the exact literal type:
```typescript
const y = "https"; // Inferred as: "https"
```
However, object properties are always widened even when assigned to `const`:
```typescript
const config = { protocol: "https" }; // protocol is string!
```
Adding `as const` recursively marks all properties as `readonly` and prevents literal widening:
```typescript
const config = { protocol: "https" } as const; // protocol is "https"
```
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Problem 1: Excess Property Checking Trap
```typescript
interface User {
  name: string;
  age: number;
}

function printUser(u: User) {
  console.log(u.name, u.age);
}

// Will Code A compile?
// printUser({ name: "Alice", age: 30, role: "admin" });

// Will Code B compile?
const person = { name: "Alice", age: 30, role: "admin" };
printUser(person);
```
**Evaluation & Explanation**:
- **Code A Fails**: Direct object literal assignments trigger **Excess Property Checking (EPC)**, raising `TS2353: Object literal may only specify known properties`.
- **Code B Compiles Cleanly**: `person` is an intermediate variable evaluated under standard **structural subtyping**. Because `person` contains `name` and `age`, the extra property `role` is permitted.

---

## 4. Architecture & System Design Scenario

**Scenario**: You are the lead architect for an SDK team. External clients complain that your public functions accept raw strings for `userId` and `organizationId`, leading to bugs where developers swap the two parameters: `assignUserToOrg(orgId, userId)`.

**Senior Solution**:
Implement **Nominal Branding**:
```typescript
declare const __brand: unique symbol;
export type Brand<T, B extends string> = T & { readonly [__brand]: B };

export type UserId = Brand<string, "UserId">;
export type OrganizationId = Brand<string, "OrganizationId">;

export function assignUserToOrg(user: UserId, org: OrganizationId): void {
  // Safe! Compiler prevents swapping parameters.
}
```

---

## 5. Senior Technical Evaluation Rubric

| Level | Performance Characteristics |
| :--- | :--- |
| **Junior / Mid** | Explains basic syntax; confuses `any` with `unknown`; unaware of Excess Property Checking. |
| **Senior** | Articulates structural subtyping, literal widening, type lattice (`unknown`/`never`), and compiler caching. |
| **Staff / Principal** | Explains Turing-completeness, assignability engine internals, AST symbol allocation, and SDK nominal branding patterns. |
