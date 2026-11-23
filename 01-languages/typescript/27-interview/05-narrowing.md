# Senior & Staff Interview Guide: Narrowing, Control Flow & Type Guards

> Technical interview guide on TypeScript control flow analysis: discriminated unions, type predicates (`is`), assertion signatures (`asserts`), and `in` narrowing.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [03-unions-and-narrowing/00-discriminated-unions.md](../03-unions-and-narrowing/00-discriminated-unions.md), [03-unions-and-narrowing/02-type-guards.md](../03-unions-and-narrowing/02-type-guards.md) |
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

Interview evaluation areas:
- **Control Flow Analysis (CFA)**: How TypeScript traces execution branches, returns, and throws to prune union types.
- **Type Predicates (`val is T`)**: Custom user-defined type guards.
- **Assertion Signatures (`asserts val is T`)**: Narrowing via assertion functions.
- **Exhaustive Default Checks**: Utilizing `never` to guarantee zero unhandled states.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: How does TypeScript's Control Flow Analysis (CFA) narrow types within conditional statements?</summary>

**Answer**:
CFA analyzes the graph of basic blocks in the AST. It tracks variable assignments, type checks (`typeof`, `instanceof`, `in`), truthiness, and branch exits (`return`, `throw`).
At each node, CFA creates a type flow graph. Inside an `if (typeof x === "string")` block, all non-string variants are subtracted from the union. In the `else` block, `string` is subtracted.
</details>

<details>
<summary>Q2: What is the difference between a Type Predicate (`val is T`) and an Assertion Signature (`asserts val is T`)?</summary>

**Answer**:
- **Type Predicate (`val is T`)**: Returns a boolean. If the function returns `true`, the parameter is narrowed within the calling block:
  ```typescript
  function isString(x: unknown): x is string { return typeof x === "string"; }
  ```
- **Assertion Signature (`asserts val is T`)**: Returns `void` (or throws). If the function returns normally without throwing, TypeScript narrows the variable for the entire remaining scope:
  ```typescript
  function assertIsString(x: unknown): asserts x is string {
    if (typeof x !== "string") throw new Error("Not a string");
  }
  ```
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Problem: Variable Re-assignment Narrowing Loss
```typescript
function test(x: string | number) {
  if (typeof x === "string") {
    console.log(x.toUpperCase()); // x is string
    x = 42;
    // What is the type of x here?
    console.log(x.toFixed(2));
  }
}
```
**Evaluation & Explanation**:
- **Output Compiles Cleanly**: TypeScript CFA tracks re-assignments! Once `x = 42` is executed, CFA re-infers `x` as `number` for the remainder of the block.

---

## 4. Architecture & System Design Scenario

**Scenario**: You are designing a financial transactions processor that receives payment events (`CREDIT`, `DEBIT`, `REVERSAL`). You must guarantee that if a junior engineer adds a new event type to the union, the application will not compile until a handler is written.

**Senior Solution**:
```typescript
type Event =
  | { type: "CREDIT"; amount: number }
  | { type: "DEBIT"; amount: number }
  | { type: "REVERSAL"; txnId: string };

function processEvent(ev: Event): void {
  switch (ev.type) {
    case "CREDIT": break;
    case "DEBIT": break;
    case "REVERSAL": break;
    default: {
      const _unreachable: never = ev;
      throw new Error(`Unhandled event: ${_unreachable}`);
    }
  }
}
```

---

## 5. Senior Technical Evaluation Rubric

| Level | Performance Characteristics |
| :--- | :--- |
| **Junior / Mid** | Uses type assertions (`as`) instead of type guards; forgets exhaustive checking. |
| **Senior** | Writes clean user-defined type predicates; enforces exhaustive checks via `never`. |
| **Staff / Principal** | Designs assertion pipelines that enforce zero-trust data ingestion across microservices. |
