# Senior & Staff Interview Guide: Utility Types & Mapped Transformation Mastery

> Technical interview guide on advanced utility types, mapped types, key remapping (`as`), and conditional type algebra.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [07-utility-types/00-partial-required-readonly.md](../07-utility-types/00-partial-required-readonly.md), [07-mapped-and-conditional-types/00-mapped-types.md](../07-mapped-and-conditional-types/00-mapped-types.md) |
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

Interviewers assessing utility type mastery look for:
- **Homomorphic vs Non-Homomorphic Mapped Types**: How `Partial`, `Readonly`, and `Record` treat property modifiers.
- **Key Remapping (`as`)**: Filtering keys using `as never` and renaming properties with template literals.
- **Distributive Conditional Types**: Why `T extends any` distributes over unions and how to prevent it using `[T]`.
- **Recursive Utility Types**: Writing `DeepPartial`, `DeepReadonly`, and understanding compiler recursion limits.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: What is a Homomorphic Mapped Type and why does `Record<K, T>` behave differently from `Partial<T>`?</summary>

**Answer**:
A **Homomorphic Mapped Type** is a mapped type of the form `[P in keyof T]`. Because it operates directly on `keyof T`, the compiler automatically preserves property modifiers (`readonly`, `?`) from the original type `T`:
```typescript
interface Original {
  readonly id: string;
  age?: number;
}
type MyPartial<T> = { [P in keyof T]?: T[P] }; // Homomorphic: retains readonly on id!
```
In contrast, `Record<K, V>` is defined as `[P in K]: V`. Because `K` is an independent type parameter (not `keyof T`), it is **non-homomorphic**: it strips all original property modifiers and produces default mutable, required properties.
</details>

<details>
<summary>Q2: How does key remapping with `as` allow filtering properties in mapped types?</summary>

**Answer**:
Introduced in TypeScript 4.1, the `as` clause allows remapping or filtering keys during mapped type iteration. If a key remaps to `never`, TypeScript completely excludes that property from the resulting object:
```typescript
// Filter out function properties:
export type NonFunctionKeys<T> = {
  [K in keyof T as T[K] extends Function ? never : K]: T[K];
};
```
</details>

<details>
<summary>Q3: What are Distributive Conditional Types and how can you turn off distribution?</summary>

**Answer**:
When a conditional type checks a naked type parameter (`T extends U`), it automatically **distributes** over union types:
```typescript
type ToArray<T> = T extends any ? T[] : never;
type Res = ToArray<string | number>; // string[] | number[] (Distributed!)
```
To prevent distribution and evaluate the union as a single monolithic type, wrap both sides in a single-element tuple:
```typescript
type NonDistributiveToArray<T> = [T] extends [any] ? T[] : never;
type Res2 = NonDistributiveToArray<string | number>; // (string | number)[]
```
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Problem: Conditional Distribution over `never`
```typescript
type Test<T> = T extends never ? true : false;

type Result = Test<never>;
// What is Result? true or false?
```
**Evaluation & Explanation**:
- **Result is `never`!** (Neither true nor false).
- **Explanation**: `never` is an empty union. Because `T` is a naked type parameter, the conditional type distributes over the empty union, running 0 times and yielding `never`.
- **Fix**: Wrap in tuple: `type SafeTest<T> = [T] extends [never] ? true : false;` (yields `true`).

---

## 4. Architecture & System Design Scenario

**Scenario**: You are building an ORM schema generator. Given an entity interface, you need a utility type `PatchUpdate<T>` that makes all fields optional except the primary key `id`, which must be forbidden from the update payload.

**Senior Solution**:
```typescript
export type PatchUpdate<T extends { id: any }> = {
  [K in keyof T as K extends "id" ? never : K]?: T[K];
};
```

---

## 5. Senior Technical Evaluation Rubric

| Level | Performance Characteristics |
| :--- | :--- |
| **Junior / Mid** | Uses built-in `Partial` and `Omit`; unaware of homomorphic preservation of readonly. |
| **Senior** | Implements custom mapped types with `as` filtering; understands distributive conditional types over unions. |
| **Staff / Principal** | Explains why `Test<never>` evaluates to `never`; architects recursive compile-time data transformers with zero type leaks. |
