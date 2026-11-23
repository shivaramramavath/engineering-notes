# Type Widening, Literal Widening, and Contextual Typing in TypeScript

> Comprehensive guide to type widening: `let` vs `const`, literal widening vs non-widening types, contextual typing in callbacks, fresh object widening, and `as const` assertions.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type System Internals |
| **Difficulty** | Advanced |
| **Prerequisites** | [01-fundamentals/00-types-and-inference.md](../01-fundamentals/00-types-and-inference.md), [14-type-system-internals/01-assignability-and-subtyping.md](./01-assignability-and-subtyping.md) |
| **TypeScript Baseline** | TS 3.4+ (`const` assertions), TS 5.0+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

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
When you declare a variable without an explicit type annotation (e.g. `const x = "GET"` vs `let y = "GET"`), TypeScript must decide what type to infer. If the type is too specific (literal type `"GET"` for `let y`), you cannot reassign `y = "POST"` later. If it is too broad (inferring `any`), type safety is lost.

This balancing act is governed by **Type Widening** and **Contextual Typing**. Understanding how literal types widen to base primitives (`string`, `number`), how object properties widen, and how contextual typing flows downwards from expected types into function arguments is essential for writing clean, idiomatic TypeScript without redundant type annotations.

---

## 2. Core Concepts
- **Literal Widening (`let` vs `const`)**:
  - `const x = "hello"` infers the literal type `"hello"`.
  - `let y = "hello"` widens the literal to the base primitive type `string`.
- **Property Widening**: Object literals assigned to variables have their properties widened:
  ```typescript
  const req = { method: 'GET' }; // Inferred as { method: string }, NOT { method: 'GET' }
  ```
- **`as const` (Const Assertions)**: Introduced in TS 3.4 to prevent widening, locking data structures into deeply readonly literal types.
- **Contextual Typing (Bottom-Up vs Top-Down Inference)**:
  - Usually, inference flows bottom-up (expression defines the type).
  - In contextual typing, inference flows **top-down** from the target context into the expression (e.g. parameter types of callback functions passed to `array.map()` or event listeners).
- **Non-Widening Literal Types**: Literal types created via explicit type annotations (`const x: 'GET' = 'GET'`) or `as const` that resist widening even when assigned to mutable variables.

---

## 3. How It Works Internally

```
                         Variable Declaration
                                   |
              +--------------------+--------------------+
              |                                         |
         const x = "GET"                           let y = "GET"
              |                                         |
              v                                         v
     Literal Type: "GET"                       Initial: Fresh "GET"
     (Immutable binding)                                |
                                                        v
                                                 Type Checker applies
                                                 getBaseTypeOfLiteralType()
                                                        |
                                                        v
                                                Widened Type: string
```

In `checker.ts`, literal types carry a `TypeFlags.FreshLiteral` flag. When checking an assignment to a mutable declaration (`let` or object property), the compiler calls `getBaseTypeOfLiteralType(type)`. If the literal is fresh, it is replaced with its widened primitive base type (`StringKeyword`, `NumberKeyword`).

---

## 4. Syntax & API Reference

```typescript
// 1. Literal vs Widened
const a = 42;       // Type: 42
let b = 42;         // Type: number

// 2. Object property widening
const req1 = { method: 'GET' };          // { method: string }
const req2 = { method: 'GET' as const }; // { readonly method: 'GET' }

// 3. Contextual Typing in callback
const numbers = [1, 2, 3];
numbers.forEach(num => {
  // num is contextually inferred as number because of Array<number>.forEach!
  console.log(num.toFixed(2));
});
```

---

## 5. Practical Examples

### Example 1: The HTTP Request Method Widening Trap
```typescript
type HttpMethod = 'GET' | 'POST';

function makeRequest(url: string, method: HttpMethod) {
  console.log(`${method} ${url}`);
}

const config = {
  url: 'https://api.example.com',
  method: 'GET', // Inferred as string, not 'GET'!
};

// @ts-expect-error TS2345: Argument of type 'string' is not assignable to parameter of type 'HttpMethod'.
makeRequest(config.url, config.method);

// Solution 1: as const
const safeConfig = {
  url: 'https://api.example.com',
  method: 'GET' as const,
};
makeRequest(safeConfig.url, safeConfig.method); // Safe
```

### Example 2: Contextual Typing in Event Listeners
```typescript
interface ButtonProps {
  onClick: (event: { x: number; y: number }) => void;
}

const button: ButtonProps = {
  // event is contextually typed; no need for '(event: { x: number; y: number })' annotation!
  onClick: event => {
    console.log(event.x, event.y);
  },
};
```

### Example 3: Edge Case - `null` and `undefined` Widening
```typescript
// Under strictNullChecks:
let x = null;      // Type: any! (Widened to any because initial value is null and can be reassigned)
let y = undefined; // Type: any!

x = 'string'; // Legal because x is any
x = 123;      // Legal

// When const:
const cNull = null;           // Type: null (Not widened)
const cUndefined = undefined; // Type: undefined (Not widened)
```

### Example 4: Compiler Failure Case - Loss of Contextual Typing
```typescript
// When moving a callback to an un-annotated variable, contextual typing is lost:
const handleClick = (e) => {
  // @ts-expect-error TS7006: Parameter 'e' implicitly has an 'any' type.
  console.log(e);
};

const btn: ButtonProps = {
  onClick: handleClick,
};
```

### Example 5: Idiomatic Tuple Widening Prevention
```typescript
// Inferred as number[]:
const coordsWidened = [10, 20];

// Inferred as readonly [10, 20] (Tuple, non-widened):
const coordsLocked = [10, 20] as const;

function plot([x, y]: readonly [number, number]) {}
plot(coordsLocked);
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`null` and `undefined` Widen to `any`**: In `let x = null;`, TypeScript infers `x` as `any` (even under `strictNullChecks`!), assuming the variable will be reassigned later. If you want a variable that only holds null, annotate explicitly: `let x: null = null;`.
2. **Empty Array Widening**: `let arr = [];` infers `any[]`. Adding an element (`arr.push(1)`) mutates the inferred type evolvingly to `number[]` until function exit.
3. **Contextual Typing Does Not Cross Intermediate Variables**: Extracting an inline callback into an un-annotated separate variable severs the contextual flow, resulting in `implicit any` parameter errors.
4. **Refactorings Break Literal Inferences**: Changing `const x = "GET"` to `let x = "GET"` silently widens `"GET"` to `string`, causing downstream type errors at call sites.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `{ method: 'GET' }` passed to union parameter | **TS2345**: String not assignable to `'GET' \| 'POST'` | Add `as const` to object or property |
| `let x = null;` expecting strict null | `x` is typed as `any`; completely un-typechecked | Annotate `let x: string \| null = null;` |
| Redundantly annotating contextual callback params | Cluttered, verbose code | Rely on contextual typing: `(e) => ...` |
| `let arr = []` used across function boundaries | Implicit `any[]` leaks into exports | Annotate: `let arr: string[] = [];` |
| Forgetting `as const` on coordinate tuples | Inferred as array `number[]` instead of tuple | Use `[x, y] as const` |

---

## 8. Performance & Type-Checker Complexity
- Contextual typing is highly optimized: it avoids performing complex constraint solving by using the expected parameter type directly as the candidate type.
- Literal widening is $\mathcal{O}(1)$ (checking a flag on the literal symbol and swapping with the base primitive type).

---

## 9. Compiler Internals
In TypeScript's `checker.ts`:
- `checkExpressionForMutableLocation` evaluates whether an expression appears in a location that requires widening (such as the initializer of a `let` statement or property of a non-readonly object literal).
- `getContextualType(node)` inspects parent AST nodes (e.g. `CallExpression` arguments) to push expected types down to child expressions.

---

## 10. Best Practices & Design Guidelines
- Use `as const` for configuration objects and options containing literal string values.
- Trust contextual typing in inline arrow functions: do not write redundant type annotations on callbacks.
- Always annotate variables initialized to `null` or `[]` to prevent silent widening to `any`.
- Keep `const` wherever re-assignment is not required to preserve precise literal types.
- Use named tuple types when returning coordinate pairs from helper functions.

---

## 11. Real-World Case Study
A financial charting dashboard defined charting series configurations:
```typescript
interface SeriesOptions {
  type: 'line' | 'bar' | 'scatter';
  color: string;
}
```
A developer refactored series generation into a loop:
```typescript
const defaultSeries = {
  type: 'line',
  color: '#3b82f6',
};
```
When passed to the chart library, `defaultSeries.type` was inferred as `string`, failing compilation. The developer cast `defaultSeries as any` to bypass the error. Weeks later, someone changed `'line'` to `'linear'` (a typo); because `any` silenced the type checker, the chart broke in production with a blank canvas.

**Solution:**
Replaced `as any` with `as const`:
```typescript
const defaultSeries = {
  type: 'line',
  color: '#3b82f6',
} as const;
```
Full compile-time safety and literal validation were restored.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between type inference for `const` vs `let` in TypeScript?</summary>

`const` declares an immutable binding, so TypeScript infers the narrowest literal type (e.g. `const x = "GET"` infers `"GET"`). `let` declares a mutable binding, so TypeScript widens the literal to its base primitive type (e.g. `let x = "GET"` infers `string`).
</details>

<details>
<summary>2. What is contextual typing?</summary>

Contextual typing is top-down type inference where the type of an expression is informed by its location or container (e.g. the parameter types of a callback function are inferred from the function signature expected by the caller).
</details>

<details>
<summary>3. Why does `{ method: 'GET' }` have its property widened to `string` even if declared with `const`?</summary>

Because while the variable reference itself cannot be re-assigned, its internal properties are mutable in JavaScript (`config.method = 'POST'`). TypeScript widens the property type to `string` to permit reassignment unless `as const` is used.
</details>

<details>
<summary>4. What does `as const` do?</summary>

Introduced in TypeScript 3.4, `as const` prevents literal widening, marks all object properties as `readonly`, and transforms array literals into readonly tuples.
</details>

<details>
<summary>5. What type is inferred for `let x = null;` under `strictNullChecks`?</summary>

`any`. TypeScript treats an un-annotated variable initialized to `null` as an evolving/widened `any` type, expecting it to be assigned a real value later.
</details>

<details>
<summary>6. How does contextual typing improve developer ergonomics in array methods?</summary>

In `[1, 2, 3].map(x => x * 2)`, TypeScript contextually types `x` as `number` because `Array<T>.map` expects `(value: T, index: number, array: T[]) => U`. Developers don't need to write `(x: number) => ...`.
</details>

<details>
<summary>7. What happens when an un-annotated empty array `let arr = [];` is declared?</summary>

It starts as `any[]`. As values are pushed into it within the same function scope, TypeScript evolves its element type to the union of pushed values.
</details>

<details>
<summary>8. How can you prevent an object property from widening without freezing the whole object?</summary>

By asserting only that specific property:
```typescript
const config = {
  method: 'GET' as const,
  timeout: 5000,
};
```
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Narrow Literal**: Fix `let method = 'GET'` so that `method` has type `'GET'` and cannot be widened to `string`.
   <details>
   <summary>Solution</summary>

   ```typescript
   let method: 'GET' = 'GET';
   ```
   </details>

2. **Lock Tuple**: Prevent `const pair = [1, 'one']` from widening into `(string | number)[]`.
   <details>
   <summary>Solution</summary>

   ```typescript
   const pair = [1, 'one'] as const;
   ```
   </details>

### Medium
3. **Contextual Typing Fix**: Move a callback `(err, data) => void` from an inline position to a variable without losing parameter types.
   <details>
   <summary>Solution</summary>

   ```typescript
   type NodeCallback = (err: Error | null, data?: string) => void;

   const handler: NodeCallback = (err, data) => {
     if (err) console.error(err);
   };
   ```
   </details>

4. **Strict HTTP Config Builder**: Write a function `createRoute<T extends string>(path: string, method: T)` that returns an object where `method` is preserved as a literal type `T` rather than widened to `string`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function createRoute<const T extends string>(path: string, method: T): { path: string; method: T } {
     return { path, method };
   }
   ```
   </details>

### Hard
5. **Generic Deep Non-Widening Transformer**: Write a type-level utility `DeepNarrow<T>` that recursively converts all primitive properties into literal types matching their initial values.
   <details>
   <summary>Solution</summary>

   ```typescript
   export type DeepNarrow<T> =
     T extends Function ? T :
     T extends Array<infer U> ? ReadonlyArray<DeepNarrow<U>> :
     T extends object ? { readonly [K in keyof T]: DeepNarrow<T[K]> } :
     T;
   ```
   </details>

6. **Contextual Type Inspector Utility**: Using the TypeScript Compiler API, write a script that inspects a call expression and logs whether an argument expression was contextually typed using `checker.getContextualType`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function auditContextualTyping(code: string): void {
     const sourceFile = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);
     const defaultHost = ts.createCompilerHost({});
     const program = ts.createProgram(['test.ts'], { strict: true }, {
       ...defaultHost,
       getSourceFile: (name) => name === 'test.ts' ? sourceFile : defaultHost.getSourceFile(name, ts.ScriptTarget.Latest),
     });

     const checker = program.getTypeChecker();

     ts.forEachChild(sourceFile, node => {
       if (ts.isCallExpression(node)) {
         node.arguments.forEach((arg, idx) => {
           const contextualType = checker.getContextualType(arg);
           if (contextualType) {
             console.log(`Arg ${idx} has contextual type: ${checker.typeToString(contextualType)}`);
           }
         });
       }
     });
   }
   ```
   </details>
