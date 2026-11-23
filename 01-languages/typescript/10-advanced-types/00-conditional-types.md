# Conditional Types

> Conditional types select one of two types based on a subtyping relationship test, bringing boolean ternary logic and dynamic dispatch directly to the TypeScript type system.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md), [01-generic-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/01-generic-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.8 / TS 5.x / TS 7.0 Native |
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
In traditional static type systems, types are rigid and static: a generic parameter `T` cannot change its shape based on runtime argument inputs.

In JavaScript, functions dynamically adapt their return types depending on argument types:
```javascript
function process(input) {
  return typeof input === "string" ? input.toUpperCase() : input * 2;
}
```
Before TypeScript 2.8, modeling this required cumbersome function overloads that easily became desynchronized.

**Conditional Types** introduced ternary decision expressions into type space:
```typescript
T extends U ? TrueBranch : FalseBranch
```
If `T` is assignable to `U`, the type resolves to `TrueBranch`; otherwise, it resolves to `FalseBranch`.

---

## 2. Core Concepts

### Basic Conditional Type
```typescript
type IsString<T> = T extends string ? true : false;

type A = IsString<"hello">; // true
type B = IsString<123>;     // false
type C = IsString<any>;     // boolean (true | false)
```

### Deferred Conditional Types
When a conditional type depends on an uninstantiated generic type parameter, the compiler defers its evaluation until concrete type arguments are supplied:
```typescript
function processValue<T extends string | number>(val: T): T extends string ? string[] : number[] {
  // Return type is deferred until the call site!
  return (typeof val === "string" ? [val] : [val]) as any;
}

const resStr = processValue("hello"); // string[]
const resNum = processValue(42);      // number[]
```

---

## 3. How It Works Internally

```
Evaluating: T extends U ? X : Y
                  |
        Is T a concrete type?
             /         \
           Yes          No (Generic Type Parameter)
           /              \
  Does T extend U?       Defer evaluation;
     /        \          create DeferredType node
   Yes         No
   /            \
Return X      Return Y
```

When the type checker evaluates `T extends U ? X : Y`:
1. It tests assignability: `checkTypeRelatedTo(T, U, Relation.Assignable)`.
2. If `T` is completely assignable to `U`, `X` is evaluated.
3. If `T` is disjoint from `U`, `Y` is evaluated.
4. If `T` is a union, distributive rules are applied (unless disabled with tuples).

---

## 4. Syntax & API Reference

### Chained Conditional Types
Conditional types can be nested to form multi-branch decision trees:
```typescript
type TypeName<T> =
  T extends string ? "string" :
  T extends number ? "number" :
  T extends boolean ? "boolean" :
  T extends undefined ? "undefined" :
  T extends Function ? "function" :
  "object";

type T1 = TypeName<() => void>; // "function"
type T2 = TypeName<42>;          // "number"
```

---

## 5. Practical Examples

### 5.1 Basic Type Classifier
```typescript
type NonNullable<T> = T extends null | undefined ? never : T;

type Raw = string | null | undefined;
type Clean = NonNullable<Raw>; // string
```

### 5.2 Real-World Application: Dynamic API Client Response Decoupling
```typescript
interface ApiContract {
  "GET /users": { id: string; name: string }[];
  "GET /user/:id": { id: string; name: string };
  "DELETE /user/:id": { success: boolean };
}

type ApiResponse<T extends keyof ApiContract> =
  T extends `GET ${string}` ? ApiContract[T] :
  T extends `DELETE ${string}` ? { status: "deleted" } :
  never;

type UserList = ApiResponse<"GET /users">; // { id: string; name: string }[]
```

### 5.3 Edge Case: The `boolean` Split
```typescript
// In TypeScript, 'boolean' is defined as union: type boolean = true | false;
type TestBool<T> = T extends true ? "YES" : "NO";

type Res = TestBool<boolean>;
// Distributes over 'true | false': TestBool<true> | TestBool<false> => "YES" | "NO"!
```

### 5.4 Failure Case: Narrowing Return in Generic Function Body
```typescript
function badNarrow<T extends string | number>(val: T): T extends string ? string : number {
  if (typeof val === "string") {
    // @ts-expect-error TS2322: Type 'string' is not assignable to type 'T extends string ? string : number'.
    return val; // TypeScript cannot narrow deferred conditional return types in function bodies!
  }
  return val as any;
}
```

### 5.5 Idiomatic Pattern: Type Equality Checker
```typescript
// Tests whether X and Y are identically equal, including readonly and any flags:
export type IsIdentical<X, Y> =
  (<T>() => T extends X ? 1 : 2) extends
  (<T>() => T extends Y ? 1 : 2) ? true : false;

type T1 = IsIdentical<string, string>; // true
type T2 = IsIdentical<string, number>; // false
type T3 = IsIdentical<any, string>;    // false
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Deferred Return Types**: TypeScript does NOT narrow the return type of a function when checking against deferred conditional types. You must use an overload or type assertion inside the implementation body.
2. **`any` in Conditional Types**: `any extends string ? 1 : 2` evaluates to `1 | 2` (both branches are evaluated and combined into a union!).
3. **`never` Distribution**: When `never` is passed to a distributive conditional type, it evaluates to `never` without visiting either branch!

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Expecting function body to narrow conditional return | CFA cannot narrow deferred generic return types | Use function overloads or explicit assertion | `TS2322: Type 'X' is not assignable to...` |
| Passing `any` expecting one branch | Evaluates both branches into a union | Guard with `IsAny<T>` | Unexpected `BranchA | BranchB` union |
| Unbracketed `never` check | Distributive condition collapses to `never` | Use tuple check: `[T] extends [never]` | Unexpected `never` |
| Creating deeply nested 20-level ternaries | Triggers instantiation limits / unreadable | Break into modular sub-types | `TS2589: Type instantiation is excessively deep` |
| Forgetting base-case in recursive conditional | Infinite compiler loop | Add terminating branch | Compiler stack overflow |
| Using conditional type where interface extends works | Slower type checking | Use interface extension for simple subtyping | Slower build times |

---

## 8. Performance & Type-Checker Complexity
- Evaluating conditional types is fast, but nested chains that distribute over wide unions scale exponentially.
- Wrap operands in tuples (`[T] extends [U]`) whenever distributive behavior is not desired.

---

## 9. Compiler Internals
- `ConditionalTypeNode` has `checkType`, `extendsType`, `trueType`, and `falseType`.
- Evaluated in `src/compiler/checker.ts` by `getTypeFromConditionalType`.
- If `checkType` is an uninstantiated type parameter, the checker returns a deferred `ConditionalType`.

---

## 10. Best Practices & Design Guidelines
- Use conditional types to extract, filter, or transform types at API boundaries.
- For public generic functions returning conditional types, pair them with function overloads for cleaner implementation bodies.
- Guard against `never` and `any` in complex utility libraries.

---

## 11. Real-World Case Study
### Type-Safe Event Dispatch Engine with Polymorphic Payloads
```typescript
interface SystemEventMap {
  "auth:login": { userId: string; timestamp: number };
  "auth:logout": { userId: string };
  "ping": void;
}

type EventPayload<E extends keyof SystemEventMap> =
  SystemEventMap[E] extends void ? [] : [payload: SystemEventMap[E]];

class EventDispatcher {
  dispatch<E extends keyof SystemEventMap>(event: E, ...args: EventPayload<E>): void {
    console.log(`Dispatching ${event}`, args);
  }
}

const dispatcher = new EventDispatcher();
dispatcher.dispatch("auth:login", { userId: "u1", timestamp: Date.now() });
dispatcher.dispatch("ping"); // Second argument is forbidden!
```

---

## 12. Interview Questions

<details>
<summary>1. What is a conditional type in TypeScript?</summary>

A conditional type takes the form `T extends U ? X : Y`. It tests whether `T` is assignable to `U`, resolving to `X` if true or `Y` if false.
</details>

<details>
<summary>2. Why does <code>any extends string ? 1 : 2</code> evaluate to <code>1 | 2</code>?</summary>

In TypeScript, `any` represents both the top type and bottom type. When evaluating a conditional type against `any`, the compiler evaluates both the true and false branches and returns their union: `1 | 2`.
</details>

<details>
<summary>3. Why can you not return a value matching a deferred conditional return type without casting?</summary>

Because TypeScript's Control Flow Analysis (CFA) does not narrow generic type parameters. When a function has return type `T extends string ? string : number`, narrowing `val` to `string` does not narrow the uninstantiated type parameter `T`, resulting in compile error `TS2322`.
</details>

<details>
<summary>4. How does <code>boolean extends true ? 1 : 2</code> evaluate?</summary>

In TypeScript, `boolean` is syntactic sugar for union `true | false`. Because bare conditional types distribute over unions, it distributes as `(true extends true ? 1 : 2) | (false extends true ? 1 : 2)`, yielding `1 | 2`.
</details>

<details>
<summary>5. How do you prevent distribution in a conditional type?</summary>

Wrap both sides in square brackets: `[T] extends [U] ? X : Y`.
</details>

<details>
<summary>6. What is a deferred conditional type?</summary>

A conditional type that depends on an uninstantiated generic type parameter. The compiler defers its evaluation until concrete type arguments are provided at the call site.
</details>

<details>
<summary>7. How do you check if a type is exactly <code>never</code> using conditional types?</summary>

```typescript
type IsNever<T> = [T] extends [never] ? true : false;
```
Without square brackets, `never` distributes as an empty union and evaluates to `never`.
</details>

<details>
<summary>8. How do conditional types differ from function overloads?</summary>

Conditional types compute return types programmatically and dynamically based on relationships, whereas function overloads declare static, separate call signatures.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: IsArray Type Checker (Easy)
Write a conditional type `IsArray<T>` returning `true` if `T` is an array, else `false`.
<details>
<summary>Solution</summary>

```typescript
type IsArray<T> = T extends readonly unknown[] ? true : false;

type T1 = IsArray<number[]>; // true
type T2 = IsArray<string>;   // false
```
</details>

### Exercise 2: Flatten Single Array Level (Easy)
Write a utility `Flatten<T>` that extracts element type if `T` is an array, else returns `T`.
<details>
<summary>Solution</summary>

```typescript
type Flatten<T> = T extends Array<infer U> ? U : T;

type R1 = Flatten<string[]>; // string
type R2 = Flatten<number>;   // number
```
</details>

### Exercise 3: Filter Null and Undefined (Medium)
Implement `MyNonNullable<T>` using a conditional type.
<details>
<summary>Solution</summary>

```typescript
type MyNonNullable<T> = T extends null | undefined ? never : T;

type Res = MyNonNullable<string | null | undefined>; // string
```
</details>

### Exercise 4: Conditional Argument Function (Medium)
Create a function `logMessage` that accepts an options object if `debug` is `true`, or no options if `false`.
<details>
<summary>Solution</summary>

```typescript
function logMessage<TDebug extends boolean>(
  msg: string,
  debug: TDebug,
  ...extra: TDebug extends true ? [options: { trace: boolean }] : []
): void {
  console.log(msg, debug, extra);
}

logMessage("Starting", true, { trace: true }); // OK
logMessage("Running", false);                  // OK
```
</details>

### Exercise 5: Strictly Typed JSON Serializer Type (Hard)
Create a conditional type `Jsonifiable<T>` that strips functions, symbols, and `undefined` values, mirroring `JSON.stringify`.
<details>
<summary>Solution</summary>

```typescript
type JsonPrimitive = string | number | boolean | null;

type Jsonifiable<T> = T extends JsonPrimitive
  ? T
  : T extends Function | symbol | undefined
  ? never
  : T extends Array<infer U>
  ? Array<Jsonifiable<U>>
  : T extends object
  ? { [K in keyof T as T[K] extends Function | symbol | undefined ? never : K]: Jsonifiable<T[K]> }
  : never;

type Res = Jsonifiable<{ name: string; run(): void; age: number }>;
// { name: string; age: number }
```
</details>

### Exercise 6: Exact Type Identity Verifier (Hard)
Implement `IsExactly<A, B>` that accurately distinguishes between `any`, `never`, `unknown`, and standard primitives.
<details>
<summary>Solution</summary>

```typescript
type IsExactly<A, B> =
  (<T>() => T extends A ? 1 : 2) extends
  (<T>() => T extends B ? 1 : 2) ? true : false;

type Check1 = IsExactly<any, string>; // false
type Check2 = IsExactly<any, any>;    // true
type Check3 = IsExactly<never, never>; // true
```
</details>
