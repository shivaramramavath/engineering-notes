# Senior & Staff Interview Guide: Generics and Variance Mastery

> Technical interview guide on advanced generic engineering: generic constraints, contextual typing, and variance (covariance, contravariance, invariance, and bivariance).

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [14-compiler-internals/02-variance.md](../14-compiler-internals/02-variance.md) |
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

Interviewers evaluating senior candidates on generics probe:
- **Variance Subtyping Rules**: Covariance (output), Contravariance (input), Invariance (bidirectional), and Method Bivariance.
- **Explicit Variance Annotations (TS 4.7+)**: The `in` and `out` keywords on type parameters.
- **Generic Constraints**: `T extends U` and distributive property preservation.
- **Unconstrained Generic Pitfalls**: When generics are overused where union types or interfaces suffice.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: Explain Covariance, Contravariance, and Invariance with practical examples in TypeScript.</summary>

**Answer**:
Variance describes how subtyping between complex types relates to subtyping between their component types:
1. **Covariance (`out`)**: Preserves subtyping direction. If `Dog extends Animal`, then `Getter<Dog>` is assignable to `Getter<Animal>`. Typical for return values and readonly data.
2. **Contravariance (`in`)**: Reverses subtyping direction. If `Dog extends Animal`, then `Consumer<Animal>` is assignable to `Consumer<Dog>`! (A function that can accept *any* Animal can safely accept a Dog). Typical for function parameter inputs.
3. **Invariance**: Subtypes must match exactly. If a structure is both read from and written to (e.g. `Box<T>` with get and set), `Box<Dog>` is NOT assignable to `Box<Animal>`.
</details>

<details>
<summary>Q2: What is Method Bivariance in TypeScript and why does it exist?</summary>

**Answer**:
Under `"strictFunctionTypes": true`, function parameters are strictly **contravariant**. 
However, **method declarations** in interfaces and classes remain deliberately **bivariant** (both covariant and contravariant):
```typescript
interface Animal { name: string; }
interface Dog extends Animal { bark(): void; }

// Method syntax: Bivariant!
interface Comparer {
  compare(a: Animal): number;
}
// Property syntax: Strictly contravariant!
interface StrictComparer {
  compare: (a: Animal) => number;
}
```
This was a pragmatic design concession to allow `Array<T>` methods (like `arr.push(dog)`) to remain assignable in legacy JavaScript frameworks.
</details>

<details>
<summary>Q3: Why did TypeScript 4.7 introduce explicit variance annotations (`in` and `out`)?</summary>

**Answer**:
Prior to TS 4.7, TypeScript calculated variance structurally by recursively inspecting every member of a generic type, which caused massive compiler slowdowns on deeply nested generic libraries.
The `in` (contravariant) and `out` (covariant) markers allow developers to declare variance explicitly:
```typescript
interface Producer<out T> {
  make(): T;
}
interface Consumer<in T> {
  consume(item: T): void;
}
```
The compiler verifies variance at declaration time and performs instant $O(1)$ assignability checks without traversing internal object hierarchies.
</details>

<details>
<summary>Q4: What is the "Golden Rule of Generics" regarding unnecessary type parameters?</summary>

**Answer**:
If a type parameter appears only **once** in a function signature, it is probably unnecessary and should be replaced with a concrete type or union. Generics are intended to correlate two or more parameters or relate a parameter to the return type:
```typescript
// BAD: T is used only once (no correlation!)
function logMessage<T extends string>(msg: T): void { ... }

// GOOD: Plain string is cleaner and faster
function logMessage(msg: string): void { ... }

// LEGITIMATE GENERIC: Correlates input to output
function first<T>(arr: T[]): T | undefined { ... }
```
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Problem: Contravariant Function Assignment
```typescript
class Animal { name = "Animal"; }
class Dog extends Animal { bark() {} }

let handleAnimal: (a: Animal) => void = (a) => console.log(a.name);
let handleDog: (d: Dog) => void = (d) => d.bark();

// Will Line A compile under strictFunctionTypes?
// handleAnimal = handleDog; // Line A

// Will Line B compile under strictFunctionTypes?
// handleDog = handleAnimal; // Line B
```
**Evaluation & Explanation**:
- **Line A Fails**: `(d: Dog) => void` is NOT assignable to `(a: Animal) => void`. If permitted, the caller could pass a `Cat` into `handleAnimal`, which would call `d.bark()` on a cat, causing a runtime crash.
- **Line B Compiles Cleanly**: Function parameters are contravariant. `handleAnimal` can accept any Animal, so it is 100% safe to pass a `Dog` to it.

---

## 4. Architecture & System Design Scenario

**Scenario**: You are designing a generic Pipeline / Middleware architecture for an API framework where middleware functions transform context objects sequentially.

**Senior Solution**:
```typescript
export type Middleware<TIn, TOut> = (ctx: TIn) => Promise<TOut>;

export class Pipeline<TCtx> {
  private fns: Middleware<any, any>[] = [];

  public use<TNext>(fn: Middleware<TCtx, TNext>): Pipeline<TNext> {
    const next = new Pipeline<TNext>();
    next.fns = [...this.fns, fn];
    return next;
  }

  public async execute(initial: TCtx): Promise<any> {
    let curr = initial;
    for (const fn of this.fns) {
      curr = await fn(curr);
    }
    return curr;
  }
}
```

---

## 5. Senior Technical Evaluation Rubric

| Level | Performance Characteristics |
| :--- | :--- |
| **Junior / Mid** | Uses `<T>` everywhere; struggles to explain why function parameters cannot accept wider types. |
| **Senior** | Understands covariance vs contravariance; avoids unnecessary generic type parameters. |
| **Staff / Principal** | Leverages TS 4.7 `in`/`out` variance annotations for compiler optimization; navigates method bivariance tradeoffs. |
