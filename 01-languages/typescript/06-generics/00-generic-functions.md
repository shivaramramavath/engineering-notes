# Generic Functions

> Generic functions parameterize callable logic over types, enabling reusable algorithms that preserve exact type relationships between inputs and outputs without type assertions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-function-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/00-function-types.md), [01-parameters-and-return-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/01-parameters-and-return-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
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
- **Problem Statement:** Reusable functions (such as collection mappers, identity helpers, API request clients, or cache wrappers) operate on data of arbitrary shapes. Without generics, developers must choose between: (1) typing inputs as `any` (destroying all type safety), (2) typing inputs as `unknown` (forcing callers to cast return values), or (3) authoring redundant, duplicated functions for every domain type.
- **Historical Context:** TypeScript has supported generics since version 0.9. Inspired by C# and Java generics, TypeScript's generic functions operate through compile-time unification and type argument inference, leaving zero runtime overhead in emitted JavaScript.
- **Real-World Relevance:** Generic functions power utility libraries (Lodash, Ramda), asynchronous data fetching (Axios, Fetch wrappers, TanStack Query), array transforms (`map`, `filter`, `reduce`), and functional pipelines.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Type Parameters: Identifiers enclosed in angle brackets (`<T>`) that act as placeholders for concrete types provided at the call site.
  - Type Argument Inference: In the vast majority of calls, TypeScript automatically infers the concrete type of `T` from the arguments passed to the function without requiring explicit annotation (`identity(10)` infers `T = number`).
  - Input-Output Relationship: A generic type parameter is only meaningful if it establishes a relationship between two or more values (e.g. connecting an argument to the return type, or connecting two arguments to each other).
  - Explicit Type Arguments: Callers can override inference by passing explicit type arguments (`fetchData<User>('/api/user')`).
- **Mental Model:** A standard function takes values as arguments and returns a value. A generic function takes *types* as arguments (compile-time) and *values* as arguments (runtime).
- **Soundness vs Ergonomics:** Type inference allows developers to write clean, idiomatic JavaScript while the compiler tracks exact type flow under the hood.

---

## 3. How It Works Internally
- **Type Argument Synthesis:** When a call expression is checked, `checkCallExpression` instantiates the generic signature. It collects type candidates from argument expressions and solves for the best common type.
- **Instantiation Cache:** The type checker memoizes generic function instantiations based on argument types to avoid re-evaluating the signature body on repeated calls.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Generic Function Type Parameter Inference               |
|                                                                          |
|   Signature: function wrap<T>(value: T): { data: T }                     |
|                                                                          |
|   Call Site: wrap("hello")                                               |
|          │                                                               |
|          ▼                                                               |
|   Argument passed: "hello" (Type: string)                                |
|          │                                                               |
|          ▼                                                               |
|   Type Checker Solver:                                                   |
|   ├── Maps argument position 0 to parameter 'value: T'                   |
|   ├── Candidate for T = string                                           |
|   └── Unifies T -> string                                                |
|          │                                                               |
|          ▼                                                               |
|   Instantiated Return Type: { data: string }                             |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Function Declaration:**
  ```typescript
  function identity<T>(arg: T): T {
    return arg;
  }
  ```
- **Arrow Function Syntax:**
  ```typescript
  // In .ts files:
  const wrap = <T>(val: T): { data: T } => ({ data: val });

  // In .tsx files (extends unknown prevents JSX element parsing collision):
  const wrapTsx = <T extends unknown>(val: T): { data: T } => ({ data: val });
  ```
- **Multiple Type Parameters:**
  ```typescript
  function pair<A, B>(first: A, second: B): [A, B] {
    return [first, second];
  }
  ```
- **tsconfig Dependencies:** Operates under standard configurations.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Author an array element swapper preserving element types.
- **Code:**
```typescript
export function swapFirstTwo<T>(arr: readonly T[]): T[] {
  if (arr.length < 2) return [...arr];
  const copy = [...arr];
  const temp = copy[0]!;
  copy[0] = copy[1]!;
  copy[1] = temp;
  return copy;
}

const numbers = swapFirstTwo([1, 2, 3]); // Inferred as: number[]
const strings = swapFirstTwo(['a', 'b', 'c']); // Inferred as: string[]
console.log(numbers); // [2, 1, 3]
console.log(strings); // ["b", "a", "c"]
```
- **Output / Type Evaluation:**
```typescript
// Exact array types preserved without casting.
```
- **Explanation:** Generic parameter `T` ensures that passing `number[]` returns `number[]`, and passing `string[]` returns `string[]`.

### Example 2: Real-World Enterprise Pattern (Type-Safe HTTP Client Wrapper)
- **Goal:** Author an API fetch wrapper that binds response payloads to caller DTO contracts.
- **Code:**
```typescript
export type ApiResponse<T> = {
  data: T;
  status: number;
  headers: Record<string, string>;
};

export async function getJson<TResponse>(
  url: string,
  options?: RequestInit
): Promise<ApiResponse<TResponse>> {
  const res = await fetch(url, options);
  if (!res.ok) {
    throw new Error(`HTTP error ${res.status}: ${res.statusText}`);
  }
  const data = (await res.json()) as TResponse;
  return {
    data,
    status: res.status,
    headers: Object.fromEntries(res.headers.entries()),
  };
}

// Consuming DTO
type UserDto = { id: string; email: string };

// Call site provides explicit type argument:
const response = await getJson<UserDto>('/api/users/me');
console.log(response.data.email);
```
- **Output / Type Evaluation:**
```typescript
// response.data is typed as UserDto with full autocomplete.
```
- **Explanation:** In I/O boundary functions where types cannot be inferred from arguments, callers provide explicit type arguments (`<UserDto>`).

### Example 3: Edge Case / Boundary Condition (Generic Array Spreading and Union Types)
- **Goal:** Understand how the type checker infers unions when arguments have disparate types.
- **Code:**
```typescript
function mergeArrays<T>(arr1: readonly T[], arr2: readonly T[]): T[] {
  return [...arr1, ...arr2];
}

const nums = [1, 2];
const strs = ['a', 'b'];

// Type checker computes common supertype: string | number
const combined = mergeArrays<string | number>(nums, strs);
console.log(combined); // [1, 2, "a", "b"]
```
- **Output / Type Evaluation:**
```typescript
// combined is typed as (string | number)[]
```
- **Explanation:** Passing incompatible arrays forces callers to provide the union type argument explicitly.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler error on superfluous or useless generic parameters.
- **Code:**
```typescript
// Anti-pattern: T is only used once in the return position!
// It provides zero relationship and forces callers to cast.
function createInvalid<T>(): T {
  // @ts-expect-error TS2322: Type '{}' is not assignable to type 'T'.
  return {} as any;
}

function processLength<T>(val: T): number {
  // @ts-expect-error TS2339: Property 'length' does not exist on type 'T'.
  return val.length;
}
```
- **Real Compiler Error:**
```
error TS2339: Property 'length' does not exist on type 'T'.
```
- **Explanation:** Unconstrained generic type parameters treat `T` as unknown/any-object; accessing properties without a constraint (`T extends { length: number }`) is rejected.

### Example 5: Idiomatic Modern Usage (Generic Predicate Filter)
- **Goal:** Create a type-safe filter function that refines array element types.
- **Code:**
```typescript
export function filterByType<T, S extends T>(
  items: readonly T[],
  guard: (item: T) => item is S
): S[] {
  const result: S[] = [];
  for (const item of items) {
    if (guard(item)) {
      result.push(item);
    }
  }
  return result;
}

const mixed: (string | number)[] = ['alpha', 10, 'beta', 20];
const onlyStrings = filterByType(mixed, (x): x is string => typeof x === 'string');
console.log(onlyStrings); // ["alpha", "beta"]
```
- **Output / Type Evaluation:**
```typescript
// onlyStrings is typed as string[]; numbers excluded completely.
```
- **Explanation:** Combining generic functions with type predicates refines polymorphic collections safely.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The "Golden Rule of Generics":** If a type parameter appears only once in a function signature, it is probably useless. A generic type parameter should establish a relationship between two or more values (e.g. parameter and return type, or two parameters).
2. **JSX Parsing Collision in TSX Files:** In `.tsx` files, writing `const fn = <T>(x: T) => x;` causes the parser to mistake `<T>` for an unclosed JSX element. Fix by writing `<T extends unknown>` or `<T,>` (with a trailing comma).
3. **Type Argument Inference Failure:** If multiple arguments reference `T` with incompatible types (`fn(1, "a")`), TypeScript raises an error rather than silently widening `T` to `string | number` unless explicitly typed.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using generic parameter only once in return | Forces caller to cast (`fn<T>()`) | Return concrete type or use argument | Prevents unsafe phantom generics |
| Unconstrained generic property access | `TS2339: Property 'x' does not exist on type 'T'` | Add constraint `<T extends HasX>` | Guarantees required properties exist |
| Over-specifying explicit type arguments | Redundant boilerplate; fragile refactoring | Let compiler infer type arguments | Keeps call sites clean and idiomatic |
| Writing `<T>(x: T)` in TSX file | `TS1005: JSX element has no closing tag` | Write `<T extends unknown>` or `<T,>` | Resolves parser ambiguity with JSX |
| Using `any` inside generic implementation | Silently breaks type safety internally | Use proper narrowing or constraints | Preserves internal correctness |
| Forgetting to return `T` in chaining | Drops concrete subclass/derived type | Return `T` or `this` | Preserves fluid method chaining |

---

## 8. Performance & Type-Checker Complexity
- **Type Argument Deduction Cost:** The solver unifies argument types against parameter signatures. Functions with 5+ generic parameters increase compilation time exponentially. Keep type parameters minimal (1–3).
- **Instantiation Memoization:** The compiler caches instantiations. Calling `identity(number)` 1,000 times reuses the memoized signature in $O(1)$ time.
- **Isolated Declarations:** Exported generic functions must have explicit return types if they cannot be trivially inferred under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Representation:** `ts.FunctionDeclaration` with `typeParameters` array containing `ts.TypeParameterDeclaration` nodes.
- **Type Checker Logic:** `inferTypeArguments` solves constraints using inference contexts. Candidates are collected and unified using `getCommonSuperType`.

---

## 10. Best Practices & Design Guidelines
- **Push Type Parameters Down:** Push generics as close to the call site as possible to maximize inference.
- **Never Use Generics When Not Relating Types:** If a function just accepts an object with an `id`, write `fn(obj: { id: string })`, NOT `fn<T extends { id: string }>(obj: T)`.
- **Prefer Inference Over Explicit Annotations:** Allow TypeScript to infer `T` from arguments whenever possible.

---

## 11. Real-World Case Study
- **Scenario:** A legacy microservice utility library defined an in-memory caching wrapper: `function getCached(key: string): any`. Developers had to write `const user = getCached('u1') as User` at 400 call sites. When the `User` schema was updated, outdated casts caused runtime null-pointer exceptions in production.
- **Implementation:** Refactored to a type-safe generic client:
  ```typescript
  export class TypedCache {
    private store = new Map<string, unknown>();

    get<T>(key: string): T | undefined {
      return this.store.get(key) as T | undefined;
    }

    set<T>(key: string, value: T): void {
      this.store.set(key, value);
    }
  }
  ```
- **Outcome:** Provided full autocomplete and compile-time contract verification across all cache consumers, catching 42 broken schema access sites during compilation.

---

## 12. Interview Questions

### Q1: What is the "Golden Rule of Generics" in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The Golden Rule of Generics states: **A type parameter should relate two or more values or signatures together.** If a type parameter appears only once in a function signature (for example, only in the parameter list: `function log<T>(item: T): void`, or only in the return type: `function create<T>(): T`), generics are usually being misused.
- If it only appears once in parameters, use a concrete type or interface (`item: unknown`).
- If it only appears once in the return type, it is a phantom type that forces callers to cast unsafely.
Generics are valuable when they connect an input to an output (`function identity<T>(x: T): T`) or connect multiple inputs together (`function merge<T>(a: T, b: T): T`).
</details>

### Q2: Why does `<T>(x: T) => x` fail to parse in a `.tsx` file?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In `.tsx` files, the TypeScript parser must distinguish between JSX element tags (`<div />`, `<Component />`) and generic type parameters. When the parser encounters `<T>`, it assumes it is the opening tag of a JSX element named `T` and expects a closing tag `</T>`. To disambiguate, you can:
1. Add an extends clause: `<T extends unknown>(x: T) => x`
2. Add a trailing comma: `<T,>(x: T) => x`
Both inform the parser that `<T...>` is a type parameter list rather than a JSX element.
</details>

### Q3: How does TypeScript infer generic type arguments from multiple function parameters?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When multiple parameters reference the same type variable `T` (e.g. `function choose<T>(a: T, b: T): T`), TypeScript collects candidate types from each argument passed. It then runs a unification algorithm to compute the "best common type" (the common supertype of all candidates). If no common supertype exists, or if strict literal types conflict, the compiler reports an error unless an explicit type argument is provided.
</details>

### Q4: What happens if you call a generic function without providing type arguments?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript runs type argument inference. It inspects the types of the values passed as arguments and attempts to synthesize the type arguments. If an argument provides no information (or if the generic parameter is unreferenced in parameters), the type parameter defaults to its declared default (if specified with `<T = Default>`), its constraint (if `<T extends Constraint>`), or `unknown`.
</details>

### Q5: Can a generic function have default type parameters?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. You can specify default type parameters:
```typescript
function request<T = Record<string, unknown>>(url: string): Promise<T> { ... }
```
If the caller does not supply an explicit type argument and the compiler cannot infer one, `T` defaults to `Record<string, unknown>`.
</details>

### Q6: How do generic functions interact with callbacks?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Generic functions can pass inferred type parameters into callback signatures:
```typescript
function map<T, U>(arr: T[], fn: (item: T) => U): U[] { ... }
```
TypeScript infers `T` from `arr`, uses `T` to contextually type the callback's `item` parameter, and infers `U` from what the callback returns.
</details>

### Q7: What is the difference between `Array<any>` and `Array<T>` in a generic function?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- `Array<any>`: Tells the compiler to accept an array of anything, discarding all type information about the elements. Operations on the elements will not be type-checked.
- `Array<T>`: Tells the compiler to capture the exact element type of the array into the type variable `T`, preserving it for use in return types or downstream parameters.
</details>

### Q8: What is a phantom type parameter in a generic function?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A phantom type parameter is a generic type parameter that is declared on a function or class but is never referenced in any runtime value parameter or property. It exists purely at the type level, typically used for nominal branding or tracking state machine transitions at compile time.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Generic Identity Pair (Easy)
- **Problem Statement:** Author a function `makePair<A, B>(first: A, second: B): readonly [A, B]` returning an immutable tuple.
- **Test Case:**
```typescript
const p = makePair('id', 100); // readonly ["id", 100]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function makePair<A, B>(first: A, second: B): readonly [A, B] {
  return [first, second] as const;
}
```
</details>

### Exercise 2: Safe Head Element (Easy)
- **Problem Statement:** Write a function `head<T>(arr: readonly T[]): T | undefined` returning the first element.
- **Test Case:**
```typescript
const h = head([1, 2, 3]); // number | undefined
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function head<T>(arr: readonly T[]): T | undefined {
  return arr[0];
}
```
</details>

### Exercise 3: Generic Zip Function (Medium)
- **Problem Statement:** Implement a function `zip<A, B>(a: readonly A[], b: readonly B[]): [A, B][]` that combines elements into pairs up to the shorter array length.
- **Test Case:**
```typescript
const zipped = zip(['a', 'b'], [1, 2, 3]); // [["a", 1], ["b", 2]]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function zip<A, B>(a: readonly A[], b: readonly B[]): [A, B][] {
  const len = Math.min(a.length, b.length);
  const result: [A, B][] = [];
  for (let i = 0; i < len; i++) {
    result.push([a[i]!, b[i]!]);
  }
  return result;
}
```
</details>

### Exercise 4: Dictionary Inverter (Medium)
- **Problem Statement:** Author a function `invert<K extends string, V extends string>(obj: Record<K, V>): Record<V, K>`.
- **Test Case:**
```typescript
const inv = invert({ a: '1', b: '2' }); // { "1": "a", "2": "b" }
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function invert<K extends string, V extends string>(
  obj: Record<K, V>
): Record<V, K> {
  const result = {} as Record<V, K>;
  for (const [key, value] of Object.entries(obj)) {
    result[value as V] = key as K;
  }
  return result;
}
```
</details>

### Exercise 5: Compile-Time Single-Usage Generic Detector (Hard)
- **Problem Statement:** Write a type utility that verifies if a function signature has generic parameters that only appear in the return position.
- **Test Case:**
```typescript
type Bad = () => string;
// Analyze parameter tuple length against return type
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type HasNoParamInputs<F> =
  F extends (...args: infer P) => any
    ? P['length'] extends 0
      ? true
      : false
    : false;
```
</details>

### Exercise 6: Curried Binary Function Transformer (Hard)
- **Problem Statement:** Author a generic function `curry2<A, B, R>(fn: (a: A, b: B) => R): (a: A) => (b: B) => R` preserving strict type inference across both stages.
- **Test Case:**
```typescript
const add = (a: number, b: number) => a + b;
const curried = curry2(add);
const add5 = curried(5);
console.log(add5(10)); // 15
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function curry2<A, B, R>(
  fn: (a: A, b: B) => R
): (a: A) => (b: B) => R {
  return (a: A) => (b: B): R => fn(a, b);
}
```
</details>
