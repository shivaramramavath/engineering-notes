# Recursive Types

> Recursive type definitions enable TypeScript to model self-referential tree structures, linked lists, deeply nested JSON documents, and unbounded hierarchical domain models.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-conditional-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/00-conditional-types.md), [03-mapped-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/03-mapped-types.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 3.7 / TS 4.1 / TS 7.0 Native |
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
Real-world data structures are frequently self-referential:
- File systems: A directory contains files and subdirectories.
- DOM / HTML: An element contains child nodes, which in turn contain child nodes.
- JSON: A JSON value can be an array of JSON values or an object mapping keys to JSON values.
- Abstract Syntax Trees (ASTs): An expression node contains left and right sub-expressions.

Before TypeScript 3.7, defining recursive type aliases required cumbersome interface wrapping workarounds. TypeScript 3.7 introduced native support for **Recursive Type Aliases**, and TypeScript 4.1 added support for recursive conditional types.

---

## 2. Core Concepts

### Direct Recursive Type Aliases
Type aliases can directly reference themselves within their own definitions:
```typescript
type JsonValue =
  | string
  | number
  | boolean
  | null
  | JsonValue[]
  | { [key: string]: JsonValue };
```

### Recursive Tree Nodes
```typescript
interface TreeNode<T> {
  value: T;
  left?: TreeNode<T>;
  right?: TreeNode<T>;
}
```

### Recursive Conditional Types (TS 4.1+)
Conditional types can invoke themselves recursively:
```typescript
type DeepFlatten<T> = T extends Array<infer U> ? DeepFlatten<U> : T;
type Res = DeepFlatten<number[][][]>; // number
```

---

## 3. How It Works Internally

```
Type Evaluation: JsonValue = string | ... | JsonValue[]
                   |
                   v
   Compiler defers recursive expansion:
   Instead of infinitely evaluating JsonValue,
   it creates a lazy TypeReference node.
                   |
                   v
   Expanded only when a specific property/element is accessed:
   const x: JsonValue = { a: [1, 2, "3"] };
   Checking 'x.a': Resolves JsonValue[] lazily!
```

Inside `checker.ts`:
1. When checking a self-referential type alias, the compiler assigns it a `TypeReference` marker.
2. It defers expanding the type body until an operation (such as property access or subtyping check) requires resolution.
3. For recursive conditional types, the compiler tracks call stack depth, preventing runaway recursion beyond internal limits.

---

## 4. Syntax & API Reference

### Tail-Call Optimized Recursion (TS 4.5+)
When a recursive conditional type evaluates its recursive call in **tail position**, the compiler eliminates the call stack frame:
```typescript
// Tail-recursive: Recursion limit is ~1,000 steps
type Reverse<T extends any[], Acc extends any[] = []> =
  T extends [infer Head, ...infer Tail]
    ? Reverse<Tail, [Head, ...Acc]>
    : Acc;
```

---

## 5. Practical Examples

### 5.1 Nested Menu Navigation Structure
```typescript
interface MenuItem {
  title: string;
  path?: string;
  children?: MenuItem[];
}

const navMenu: MenuItem = {
  title: "Settings",
  children: [
    { title: "Profile", path: "/profile" },
    {
      title: "Security",
      children: [
        { title: "2FA", path: "/2fa" },
        { title: "Sessions", path: "/sessions" },
      ],
    },
  ],
};
```

### 5.2 Real-World Application: Fully Recursive Deep Partial
```typescript
type DeepPartial<T> = T extends Function
  ? T
  : T extends Array<infer U>
  ? Array<DeepPartial<U>>
  : T extends object
  ? { [K in keyof T]?: DeepPartial<T[K]> }
  : T;

interface OrgHierarchy {
  company: string;
  departments: Array<{
    name: string;
    manager: { name: string; email: string };
  }>;
}

const patch: DeepPartial<OrgHierarchy> = {
  departments: [{ manager: { email: "new@org.com" } }],
};
```

### 5.3 Edge Case: Self-Referential Linked List
```typescript
interface LinkedListNode<T> {
  value: T;
  next: LinkedListNode<T> | null;
}

function traverseList<T>(head: LinkedListNode<T>): T[] {
  const result: T[] = [];
  let curr: LinkedListNode<T> | null = head;
  while (curr) {
    result.push(curr.value);
    curr = curr.next;
  }
  return result;
}
```

### 5.4 Failure Case: Infinite Recursion Without Base Case
```typescript
// @ts-expect-error TS2589: Type instantiation is excessively deep and possibly infinite.
type Infinite<T> = [Infinite<T>];
```

### 5.5 Idiomatic Pattern: Bounded Recursion Depth Guard
```typescript
type Prev = [never, 0, 1, 2, 3, 4, 5];

type BoundedDeepReadonly<T, Depth extends number = 5> = [Depth] extends [0]
  ? T
  : T extends object
  ? { readonly [K in keyof T]: BoundedDeepReadonly<T[K], Prev[Depth]> }
  : T;
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Error TS2589**: Occurs when recursive conditional types exceed the compiler's instantiation depth limit (approx. 1,000 steps for tail recursion, ~50 for non-tail recursion).
2. **Circular Class References**: Classes can reference each other cyclically across files, but cyclic initialization order at runtime can yield `undefined`.
3. **Function Preservation in Deep Recursion**: Always guard with `T extends Function ? T : ...` to prevent mapping over function prototypes.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting terminating base case | Causes infinite compiler loop | Provide condition that returns base type | `TS2589: Type instantiation is excessively deep...` |
| Non-tail recursive type calls | Hits stack limit after ~50 iterations | Use accumulator in tail position | Stack overflow in checker |
| Mapping over functions in recursive types | Destroys function call signatures | Add `T extends Function ? T : ...` base case | Function becomes empty object |
| Not handling `Date` / `RegExp` in deep types | Turns built-in objects into empty mapped objects | Add explicit base case for built-ins | Missing prototype methods |
| Recursive object intersection accumulation | Intersecting in a loop causes $O(N^2)$ memory bloat | Use mapped types instead of intersections | Extreme compiler memory usage |
| Deep recursion over dynamic unbounded JSON | Can freeze IDE language server | Cap recursion depth with tuple counter | Slow IDE autocompletion |

---

## 8. Performance & Type-Checker Complexity
- Recursive type aliases use lazy evaluation and have minimal memory footprint when dormant.
- Recursive conditional types that run at compile time consume $O(N)$ type instantiations. Keep recursion bounds modest.

---

## 9. Compiler Internals
- Evaluated via `getTypeFromTypeReference` in `src/compiler/checker.ts`.
- The compiler maintains an `instantiationDepth` integer.
- If recursion exceeds internal caps, `error(node, Diagnostics.Type_instantiation_is_excessively_deep_and_possibly_infinite)` is raised.

---

## 10. Best Practices & Design Guidelines
- Provide depth counters on deep recursive utilities (e.g. `Depth extends number = 5`).
- Formulate recursive conditional types in tail-call position using accumulator tuples.
- Guard against built-in types (`Function`, `Date`, `RegExp`) before recursing into object keys.

---

## 11. Real-World Case Study
### Strongly Typed File System Tree Traversal
```typescript
type FileNode = { type: "file"; name: string; sizeBytes: number };
type DirectoryNode = { type: "dir"; name: string; entries: FsNode[] };
type FsNode = FileNode | DirectoryNode;

function calculateTotalSize(node: FsNode): number {
  if (node.type === "file") {
    return node.sizeBytes;
  }
  return node.entries.reduce((acc, entry) => acc + calculateTotalSize(entry), 0);
}

const rootDir: DirectoryNode = {
  type: "dir",
  name: "root",
  entries: [
    { type: "file", name: "index.html", sizeBytes: 500 },
    {
      type: "dir",
      name: "assets",
      entries: [{ type: "file", name: "logo.png", sizeBytes: 2400 }],
    },
  ],
};

console.log(calculateTotalSize(rootDir)); // 2900
```

---

## 12. Interview Questions

<details>
<summary>1. When did TypeScript introduce native support for recursive type aliases?</summary>

TypeScript 3.7 introduced native recursive type aliases (like `type Json = string | Json[]`) by deferring resolution until properties are accessed.
</details>

<details>
<summary>2. What is Tail-Call Optimization for recursive conditional types in TypeScript 4.5+?</summary>

When the recursive call of a conditional type is in tail position (directly returned without wrapping in an intermediate structure), the compiler reuses the evaluation stack frame, extending the supported recursion depth from ~50 to ~1,000 steps.
</details>

<details>
<summary>3. What causes error TS2589?</summary>

Exceeding the compiler's internal recursion depth limit during generic instantiation, typically caused by a recursive type that lacks a terminating base case or loops circularly.
</details>

<details>
<summary>4. How can you limit the recursion depth of a recursive type?</summary>

Using a tuple lookup array as a decrementing counter:
```typescript
type Prev = [never, 0, 1, 2, 3];
type Recurse<T, D extends number = 3> = [D] extends [0] ? T : Recurse<Next<T>, Prev[D]>;
```
</details>

<details>
<summary>5. Why must functions be guarded in recursive deep types?</summary>

In TypeScript, functions extend `object`. If a recursive mapped type does not guard against `Function`, it attempts to iterate over `keyof Function` and strips the call signature, turning the function into an uncallable object.
</details>

<details>
<summary>6. How do you define a recursive JSON value type?</summary>

```typescript
type Json = string | number | boolean | null | Json[] | { [key: string]: Json };
```
</details>

<details>
<summary>7. How does lazy evaluation work for recursive types?</summary>

The compiler creates an unevaluated `TypeReference` marker instead of evaluating the full recursive tree at declaration time. The recursive branch is only resolved when code explicitly accesses a child property.
</details>

<details>
<summary>8. Can interfaces be recursive in TypeScript?</summary>

Yes, interfaces have always supported recursive definitions since TypeScript 0.9 (e.g. `interface Node { next?: Node; }`).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Recursive String Array Flattener (Easy)
Write a recursive utility `NestedArray<T>` representing an arbitrarily nested array of `T`.
<details>
<summary>Solution</summary>

```typescript
type NestedArray<T> = Array<T | NestedArray<T>>;
const valid: NestedArray<number> = [1, [2, [3, [4]]]];
```
</details>

### Exercise 2: Count Depth of Nested Array (Easy)
Write a type `ArrayDepth<T>` that counts how many array dimensions surround `T`.
<details>
<summary>Solution</summary>

```typescript
type ArrayDepth<T, Acc extends any[] = []> =
  T extends readonly (infer U)[]
    ? ArrayDepth<U, [...Acc, any]>
    : Acc["length"];

type D3 = ArrayDepth<string[][][]>; // 3
```
</details>

### Exercise 3: Recursive Deep Readonly (Medium)
Implement `DeepReadonly<T>` with base-case guards for functions and primitives.
<details>
<summary>Solution</summary>

```typescript
type DeepReadonly<T> = T extends Function
  ? T
  : T extends readonly (infer U)[]
  ? ReadonlyArray<DeepReadonly<U>>
  : T extends object
  ? { readonly [K in keyof T]: DeepReadonly<T[K]> }
  : T;
```
</details>

### Exercise 4: Reverse Tuple Tail-Recursively (Medium)
Implement `Reverse<T>` for tuples using tail recursion.
<details>
<summary>Solution</summary>

```typescript
type Reverse<T extends readonly unknown[], Acc extends unknown[] = []> =
  T extends readonly [infer Head, ...infer Tail]
    ? Reverse<Tail, [Head, ...Acc]>
    : Acc;

type Rev = Reverse<[1, 2, 3, 4]>; // [4, 3, 2, 1]
```
</details>

### Exercise 5: Depth-Guarded Deep Required (Hard)
Implement `BoundedDeepRequired<T, Depth extends number = 3>`.
<details>
<summary>Solution</summary>

```typescript
type Prev = [never, 0, 1, 2, 3];

type BoundedDeepRequired<T, D extends number = 3> = [D] extends [0]
  ? T
  : T extends Function
  ? T
  : T extends Array<infer U>
  ? Array<BoundedDeepRequired<U, Prev[D]>>
  : T extends object
  ? { [K in keyof T]-?: BoundedDeepRequired<T[K], Prev[D]> }
  : T;
```
</details>

### Exercise 6: Parse Deep Path Value (Hard)
Given a recursive object structure, implement a strongly typed path accessor returning the nested property type.
<details>
<summary>Solution</summary>

```typescript
type PathValue<T, P extends string> =
  P extends `${infer Key}.${infer Rest}`
    ? Key extends keyof T
      ? PathValue<T[Key], Rest>
      : never
    : P extends keyof T
    ? T[P]
    : never;

interface Org {
  dept: { engineering: { lead: { name: string } } };
}

type LeadName = PathValue<Org, "dept.engineering.lead.name">; // string
```
</details>
