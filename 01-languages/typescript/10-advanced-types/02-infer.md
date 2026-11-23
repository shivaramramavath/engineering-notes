# The infer Keyword

> The `infer` keyword introduces pattern matching and type-level destructuring within conditional types, unlocking recursive unboxing, tuple destructuring, and AST extraction.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-conditional-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/00-conditional-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.8 / TS 4.7 (constraints) / TS 7.0 Native |
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
In functional programming, pattern matching allows destructuring complex data structures to extract nested values.

Before TypeScript 2.8, TypeScript had no way to extract inner types from generic wrappers. If a function returned `Promise<User>`, there was no way to write a type that extracted `User`.

The **`infer`** keyword introduces pattern matching to TypeScript:
- It declares a fresh type variable dynamically inside the `extends` clause of a conditional type.
- The compiler deduces the identity of that type variable by matching against the candidate structure.

---

## 2. Core Concepts

### Pattern Matching Syntax
```typescript
type UnpackPromise<T> = T extends Promise<infer U> ? U : T;

type R1 = UnpackPromise<Promise<string>>; // string
type R2 = UnpackPromise<number>;          // number
```

### Constrained `infer` (TS 4.7+)
Starting in TypeScript 4.7, `infer` declarations can include explicit `extends` constraints directly:
```typescript
type FirstString<T> = T extends [infer S extends string, ...unknown[]] ? S : never;

type A = FirstString<["hello", 123]>; // "hello"
type B = FirstString<[100, 200]>;     // never (100 does not extend string)
```

---

## 3. How It Works Internally

```
Target Type:   (x: string, y: number) => boolean
Pattern:       (...args: any[]) => infer R
                          |
                          v
Matching:      Return position matches 'boolean'
Inferred:      R = boolean
Result:        boolean
```

During type assignability checking:
1. The compiler creates an `InferenceContext`.
2. It sets up inference candidates for every `infer` node.
3. If an inferred variable appears in **covariant positions** (return types), multiple candidates are merged into a **union**.
4. If an inferred variable appears in **contravariant positions** (parameter types), multiple candidates are merged into an **intersection**.

---

## 4. Syntax & API Reference

### Multiple `infer` Variables
```typescript
type UnpackEntry<T> = T extends [infer Key, infer Value] ? { key: Key; value: Value } : never;
type Entry = UnpackEntry<["id", 42]>; // { key: "id"; value: 42 }
```

---

## 5. Practical Examples

### 5.1 Extracting Constructor Parameters
```typescript
type ConstructorArgs<T> =
  T extends new (...args: infer P) => any ? P : never;

class Server {
  constructor(port: number, host: string) {}
}

type ServerArgs = ConstructorArgs<typeof Server>; // [port: number, host: string]
```

### 5.2 Real-World Application: Type-Safe Event Bus Payload Extractor
```typescript
interface AppHandlers {
  onLogin(user: { id: string }): void;
  onLogout(): void;
  onError(err: Error, fatal: boolean): void;
}

type HandlerPayload<T extends (...args: any[]) => void> =
  T extends (...args: infer P) => void ? P : never;

type ErrorArgs = HandlerPayload<AppHandlers["onError"]>; // [err: Error, fatal: boolean]
```

### 5.3 Edge Case: Tuple Rest Infer
```typescript
type Pop<T extends readonly unknown[]> =
  T extends readonly [...infer Rest, infer Last] ? Rest : [];

type ShortList = Pop<[1, 2, 3]>; // [1, 2]
```

### 5.4 Failure Case: `infer` in False Branch
```typescript
// @ts-expect-error TS2304: Cannot find name 'U'.
type BadInfer<T> = T extends Promise<infer U> ? U : U;
// Inferred variables are ONLY in scope inside the TrueBranch!
```

### 5.5 Idiomatic Pattern: Recursive String Splitter
```typescript
type Split<S extends string, Delim extends string> =
  S extends `${infer Head}${Delim}${infer Tail}`
    ? [Head, ...Split<Tail, Delim>]
    : [S];

type Parts = Split<"users/profile/edit", "/">;
// ["users", "profile", "edit"]
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Scope Limit**: The variable introduced by `infer U` is **only in scope inside the true branch** of the conditional type. It cannot be used in the false branch or outside the ternary.
2. **Contravariant Intersections**: If `infer T` is in a function parameter position across overloaded signatures, it infers the **intersection** of parameter types.
3. **Cannot Be Used in Type Aliases Directly**: You cannot write `type X = infer U;`. It must reside inside the `extends` clause of a conditional type.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Referencing `infer` variable in false branch | Variable is out of scope in false branch | Restructure ternary or use fallback type | `TS2304: Cannot find name 'U'` |
| Using `infer` outside conditional extends | Only valid inside conditional `extends` clause | Wrap in `T extends Pattern<infer U> ? ...` | `TS1338: 'infer' declarations are only permitted...` |
| Unconstrained `infer` matching `unknown` | Type checker cannot verify downstream properties | Constrain: `infer U extends Expected` | `TS2344: Type does not satisfy constraint` |
| Expecting union from parameter positions | Function parameters are contravariant | Inferred parameter types produce intersections | Unexpected `never` / intersection |
| Recursive `infer` without base case | Exceeds compiler recursion stack | Provide non-matching terminating branch | `TS2589: Type instantiation is excessively deep` |
| Over-inferring deeply nested tuples | Large tuple expansions slow compilation | Limit tuple parsing depth | Slow compilation |

---

## 8. Performance & Type-Checker Complexity
- Simple inference (`ReturnType`, `Parameters`) is $O(1)$.
- Recursive string or tuple destructuring with `infer` runs in $O(N)$ where $N$ is recursion depth (capped at ~1,000 steps with tail-call optimization).

---

## 9. Compiler Internals
- `InferTypeNode` (`kind: SyntaxKind.InferType`) contains a `typeParameter`.
- In `src/compiler/checker.ts`, `createInferenceContext` tracks inferred type candidates.
- Evaluated during subtyping via `inferTypes(context.inferences, source, target)`.

---

## 10. Best Practices & Design Guidelines
- Use constrained `infer` (`infer U extends string`) in TypeScript 4.7+ to avoid redundant downstream conditional checks.
- Keep recursive `infer` types tail-recursive by passing accumulator parameters.
- Provide sensible fallback types in the false branch.

---

## 11. Real-World Case Study
### Type-Safe SQL Parameter Parser
```typescript
type ExtractSqlParams<T extends string> =
  T extends `${infer _}$: ${infer Param} ${infer Rest}`
    ? Param | ExtractSqlParams<` ${Rest}`>
    : T extends `${infer _}$: ${infer Param}`
    ? Param
    : never;

type Query = "SELECT * FROM users WHERE id = :userId AND tenant = :tenantId";
type Params = ExtractSqlParams<Query>; // "userId" | "tenantId"
```

---

## 12. Interview Questions

<details>
<summary>1. What does the <code>infer</code> keyword do in TypeScript?</summary>

It introduces an inferred type parameter inside the `extends` clause of a conditional type, allowing the compiler to pattern-match against complex types and extract inner components (like return types, element types, or arguments).
</details>

<details>
<summary>2. Where is an <code>infer</code> variable in scope?</summary>

It is strictly in scope only within the **true branch** of the conditional type declaration. It cannot be accessed in the false branch.
</details>

<details>
<summary>3. What happens when the same <code>infer</code> variable appears in multiple covariant vs contravariant positions?</summary>

In covariant positions (like return types), multiple candidates are merged into a **union**. In contravariant positions (like function arguments), multiple candidates are merged into an **intersection**.
</details>

<details>
<summary>4. What feature was added to <code>infer</code> in TypeScript 4.7?</summary>

Type parameter constraints: `infer T extends Constraint`, allowing developers to immediately bound the inferred type.
</details>

<details>
<summary>5. How does <code>ReturnType&lt;T&gt;</code> use <code>infer</code>?</summary>

```typescript
type ReturnType<T extends (...args: any) => any> =
  T extends (...args: any) => infer R ? R : any;
```
</details>

<details>
<summary>6. How can <code>infer</code> be used to extract the head and tail of a tuple?</summary>

```typescript
type HeadTail<T> = T extends [infer Head, ...infer Tail] ? { head: Head; tail: Tail } : never;
```
</details>

<details>
<summary>7. Can <code>infer</code> be used in template literal types?</summary>

Yes. It matches substrings: `T extends `${infer A}/${infer B}` ? [A, B] : [T]`.
</details>

<details>
<summary>8. What error is raised if <code>infer</code> is written in an interface declaration?</summary>

`TS1338: 'infer' declarations are only permitted in the 'extends' clause of a conditional type.`
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Extract Promise Type (Easy)
Extract `T` from `Promise<T>` using `infer`.
<details>
<summary>Solution</summary>

```typescript
type Unwrap<T> = T extends Promise<infer U> ? U : T;
type Res = Unwrap<Promise<string>>; // string
```
</details>

### Exercise 2: First Tuple Element (Easy)
Extract the first element of a tuple type using `infer`.
<details>
<summary>Solution</summary>

```typescript
type First<T extends readonly unknown[]> =
  T extends readonly [infer F, ...unknown[]] ? F : never;

type F = First<[10, 20, 30]>; // 10
```
</details>

### Exercise 3: Last Element of Tuple (Medium)
Extract the last element of a non-empty tuple.
<details>
<summary>Solution</summary>

```typescript
type Last<T extends readonly unknown[]> =
  T extends readonly [...unknown[], infer L] ? L : never;

type L = Last<["a", "b", "c"]>; // "c"
```
</details>

### Exercise 4: Function Parameter at Index (Medium)
Extract the parameter at index 1 from a function signature.
<details>
<summary>Solution</summary>

```typescript
type SecondParam<T extends (...args: any[]) => any> =
  T extends (p0: any, p1: infer P1, ...rest: any[]) => any ? P1 : never;

function log(msg: string, code: number, flag: boolean) {}
type CodeType = SecondParam<typeof log>; // number
```
</details>

### Exercise 5: Trim String Left (Hard)
Implement `TrimLeft<S>` that recursively strips leading whitespace using `infer`.
<details>
<summary>Solution</summary>

```typescript
type Whitespace = " " | "	" | "
";
type TrimLeft<S extends string> =
  S extends `${Whitespace}${infer Rest}` ? TrimLeft<Rest> : S;

type Trimmed = TrimLeft<"   hello world">; // "hello world"
```
</details>

### Exercise 6: Parse Function Signature Details (Hard)
Extract an object containing parameter tuple, return type, and boolean indicating if return is a Promise.
<details>
<summary>Solution</summary>

```typescript
type InspectFunction<T> = T extends (...args: infer P) => infer R
  ? {
      parameters: P;
      returnType: R;
      isAsync: R extends Promise<any> ? true : false;
    }
  : never;

async function fetchUser(id: string) { return { id }; }
type Info = InspectFunction<typeof fetchUser>;
// { parameters: [id: string]; returnType: Promise<{ id: string }>; isAsync: true }
```
</details>
