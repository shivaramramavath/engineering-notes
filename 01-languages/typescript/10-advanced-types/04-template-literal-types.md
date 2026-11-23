# Template Literal Types

> Template literal types bring JavaScript template string interpolation syntax into the type system, enabling string pattern matching, compile-time parsers, and combinatoric union generation.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [06-string-manipulation-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/06-string-manipulation-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 4.1 / TS 5.x / TS 7.0 Native |
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
In JavaScript, developers compose strings dynamically using template literals (``${prefix}_${name}``). Before TypeScript 4.1, the type system could only represent strings as broad `string` primitives or exact static string literals (`"GET"`).

**Template Literal Types** use the exact same backtick syntax in type positions:
```typescript
type EventName = `on${Capitalize<string>}`;
```
They can interpolate unions, numbers, booleans, and other template types, automatically generating Cartesian products or acting as patterns in conditional type matching.

---

## 2. Core Concepts

### Cartesian Product Generation
When unions are interpolated inside a template literal type, TypeScript automatically computes the Cartesian product:
```typescript
type Direction = "top" | "bottom";
type Alignment = "left" | "right";

type Position = `${Direction}-${Alignment}`;
// "top-left" | "top-right" | "bottom-left" | "bottom-right"
```

### Type Matching & Parsing
When used with `infer`, template literals function as compile-time string parsers:
```typescript
type SplitPath<T extends string> =
  T extends `${infer Segment}/${infer Rest}`
    ? [Segment, ...SplitPath<Rest>]
    : [T];

type Segments = SplitPath<"api/v1/users">;
// ["api", "v1", "users"]
```

---

## 3. How It Works Internally

```
Type Expression: `${"a" | "b"}_${1 | 2}`
                     |
                     v
   Compiler computes combinations:
       "a" + "_" + "1" ===> "a_1"
       "a" + "_" + "2" ===> "a_2"
       "b" + "_" + "1" ===> "b_1"
       "b" + "_" + "2" ===> "b_2"
                     |
                     v
Result Union: "a_1" | "a_2" | "b_1" | "b_2"
```

In `src/compiler/checker.ts`:
1. The parser generates a `TemplateLiteralTypeNode` containing a `head` and a list of `templateSpans`.
2. When checking assignability, `checkTypeRelatedTo` runs a regex-like pattern matcher that matches character slices between template holes.
3. If `infer` is inside a span, the matched substring is extracted and bound to that type parameter.

---

## 4. Syntax & API Reference

### Primitives Allowed in Template Types
Template literals coerce specific primitive types:
- `string`, string literals
- `number`, numeric literals
- `boolean`, `true`, `false`
- `bigint`, bigint literals
- `null`, `undefined`
*(Symbols and objects are forbidden).*

---

## 5. Practical Examples

### 5.1 CSS Unit Validator
```typescript
type LengthUnit = "px" | "em" | "rem" | "%" | "vh" | "vw";
type CSSLength = `${number}${LengthUnit}` | "0" | "auto";

const width: CSSLength = "100px";
const height: CSSLength = "50vh";
// @ts-expect-error TS2322: Type '"100"' is not assignable to type 'CSSLength'.
const bad: CSSLength = "100";
```

### 5.2 Real-World Application: Type-Safe Dotted Path Object Accessor
```typescript
type DottedPath<T> = T extends object
  ? {
      [K in keyof T & string]: T[K] extends object
        ? K | `${K}.${DottedPath<T[K]>}`
        : K;
    }[keyof T & string]
  : never;

interface AppConfig {
  db: {
    primary: { host: string; port: number };
    replica: { host: string };
  };
  env: string;
}

type Paths = DottedPath<AppConfig>;
// "env" | "db" | "db.primary" | "db.primary.host" | "db.primary.port" | "db.replica" | "db.replica.host"
```

### 5.3 Edge Case: Greedy vs Non-Greedy String Inference
```typescript
// In template literal pattern matching, inference is NON-GREEDY:
type MatchProtocol<T extends string> = T extends `${infer Proto}://${string}` ? Proto : never;
type P = MatchProtocol<"https://my.domain.com/path">; // "https"
```

### 5.4 Failure Case: Symbol in Template Literal
```typescript
declare const sym: unique symbol;

// @ts-expect-error TS2322: Type 'symbol' is not assignable to type 'string | number | bigint | boolean | null | undefined'.
type BadTemplate = `prefixed_${typeof sym}`;
```

### 5.5 Idiomatic Pattern: Safe SQL Query Builder
```typescript
type SqlVerb = "SELECT" | "INSERT" | "UPDATE" | "DELETE";
type SqlStatement = `${SqlVerb} ${string}`;

function executeSql(query: SqlStatement): void {
  console.log("Executing:", query);
}

executeSql("SELECT * FROM users"); // OK
// @ts-expect-error TS2345: Argument of type '"DROP TABLE users"' is not assignable to parameter of type 'SqlStatement'.
executeSql("DROP TABLE users");
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Exponential Instantiation Limit**: Multiplying multiple unions can quickly produce millions of types. TypeScript caps union size in template literal types to prevent compiler out-of-memory errors.
2. **Numbers in Template Literals**: ``${number}`` matches any string containing valid digits or decimals, but does not parse mathematical boundaries at compile time.
3. **Symbols are Forbidden**: Symbols cannot be coerced to strings in type space.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Multiplying wide unions | Creates Cartesian explosion ($O(N^K)$) | Limit union size in template strings | `TS2590: Expression produces a union type that is too complex...` |
| Using `symbol` in template string | Symbol cannot be converted to string type | Filter keys with `string & K` | `TS2322: Type 'symbol' not assignable to...` |
| Recursive template type on long strings | Exceeds compiler recursion limit (~1,000) | Restrict parsing to identifiers | `TS2589: Type instantiation is excessively deep` |
| Expecting numeric arithmetic in template types | Template types only perform string concatenation | Use tuple length simulation for math | Loss of numeric arithmetic |
| Forgetting non-greedy matching behavior | First `infer` captures minimal matching string | Anchor with delimiters | Unexpected substring match |
| Using template literals for enum replacement | Enums generate runtime values; template types do not | Use `as const` object dictionaries | Missing runtime value |

---

## 8. Performance & Type-Checker Complexity
- Simple template strings resolve in $O(1)$.
- Template types with multiple union holes compute Cartesian products with complexity $O(prod N_i)$. Keep total permutations below 10,000.

---

## 9. Compiler Internals
- `TemplateLiteralTypeNode` contains `head: TemplateHead` and `templateSpans: NodeArray<TemplateSpan>`.
- Evaluated in `src/compiler/checker.ts` by `getTemplateLiteralType`.
- If any interpolated type parameter is generic, the compiler creates an unevaluated `TemplateLiteralType` deferred until instantiation.

---

## 10. Best Practices & Design Guidelines
- Use template literal types for route paths, event names, and CSS unit validation.
- Intersect with `string & K` when mapping object keys into template literals.
- Guard recursive template parsers with base-case termination branches.

---

## 11. Real-World Case Study
### Type-Safe REST API Endpoint Route Definition with Path Parameters
```typescript
type ExtractParams<T extends string> =
  T extends `${infer _}:${infer Param}/${infer Rest}`
    ? Param | ExtractParams<`/${Rest}`>
    : T extends `${infer _}:${infer Param}`
    ? Param
    : never;

type RouteDefinition<T extends string> = {
  path: T;
  handler: (params: Record<ExtractParams<T>, string>) => void;
};

const route: RouteDefinition<"/api/v1/users/:userId/posts/:postId"> = {
  path: "/api/v1/users/:userId/posts/:postId",
  handler: (params) => {
    // params is strictly typed as: { userId: string; postId: string }
    console.log(params.userId, params.postId);
  }
};
```

---

## 12. Interview Questions

<details>
<summary>1. What are Template Literal Types in TypeScript?</summary>

Template literal types allow composing string types using the backtick interpolation syntax ``${A}_${B}`` in type positions. They can interpolate unions to generate Cartesian products or use `infer` to pattern-match and destructure strings.
</details>

<details>
<summary>2. What happens when unions are passed into template literal types?</summary>

TypeScript computes the Cartesian product of all union members. For example, ``${"a"|"b"}_${1|2}`` resolves to `"a_1" | "a_2" | "b_1" | "b_2"`.
</details>

<details>
<summary>3. What error occurs if a template literal type generates too many permutations?</summary>

`TS2590: Expression produces a union type that is too complex to represent.`
</details>

<details>
<summary>4. What types are permitted inside template literal interpolations?</summary>

`string`, `number`, `boolean`, `bigint`, `null`, and `undefined`, along with their literal types. Symbols and objects are not permitted.
</details>

<details>
<summary>5. How does pattern matching with <code>infer</code> in template literals work?</summary>

By specifying ``${infer Head}-${infer Tail}``, TypeScript matches substrings before and after the delimiter `-`, assigning them to `Head` and `Tail` respectively.
</details>

<details>
<summary>6. Is template literal pattern matching greedy or non-greedy?</summary>

The first `infer` before a fixed delimiter is **non-greedy**; it matches the shortest possible substring until the delimiter is encountered.
</details>

<details>
<summary>7. How can you validate that a string is a valid CSS pixel value?</summary>

```typescript
type Pixels = `${number}px`;
```
</details>

<details>
<summary>8. How do template literal types integrate with mapped types?</summary>

Using key remapping with `as`:
```typescript
type Getters<T> = { [K in keyof T as `get${Capitalize<string & K>}`]: () => T[K] };
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: CSS Percentage Validator (Easy)
Write a template literal type matching valid percentage strings (e.g. `"50%"`, `"100%"`).
<details>
<summary>Solution</summary>

```typescript
type CSSPercentage = `${number}%`;
const valid: CSSPercentage = "50%"; // OK
// @ts-expect-error TS2322
const bad: CSSPercentage = "50px";
```
</details>

### Exercise 2: Combine HTTP Methods and Paths (Easy)
Combine HTTP methods `"GET" | "POST"` with paths `"/users" | "/orders"`.
<details>
<summary>Solution</summary>

```typescript
type Method = "GET" | "POST";
type Path = "/users" | "/orders";
type Endpoint = `${Method} ${Path}`;
// "GET /users" | "GET /orders" | "POST /users" | "POST /orders"
```
</details>

### Exercise 3: Extract Protocol from URL (Medium)
Write a utility `ExtractProtocol<T>` that extracts the protocol (e.g. `"https"`) from a URL string.
<details>
<summary>Solution</summary>

```typescript
type ExtractProtocol<T extends string> =
  T extends `${infer Protocol}://${string}` ? Protocol : never;

type P = ExtractProtocol<"https://github.com">; // "https"
```
</details>

### Exercise 4: Replace String Substring (Medium)
Implement `Replace<S, From, To>` using template literal inference.
<details>
<summary>Solution</summary>

```typescript
type Replace<S extends string, From extends string, To extends string> =
  From extends ""
    ? S
    : S extends `${infer Head}${From}${infer Tail}`
    ? `${Head}${To}${Tail}`
    : S;

type Res = Replace<"foo_bar", "_", "-">; // "foo-bar"
```
</details>

### Exercise 5: CamelCase to KebabCase Converter (Hard)
Convert `camelCaseString` to `kebab-case-string` entirely at compile time.
<details>
<summary>Solution</summary>

```typescript
type CamelToKebab<S extends string> = S extends `${infer Head}${infer Tail}`
  ? `${Head extends Lowercase<Head> ? "" : "-"}${Lowercase<Head>}${CamelToKebab<Tail>}`
  : S;

type K = CamelToKebab<"myFavoriteColor">; // "my-favorite-color"
```
</details>

### Exercise 6: Parse Dotted JSON Path Values (Hard)
Given a nested object type and a dotted string path, return the target value type.
<details>
<summary>Solution</summary>

```typescript
type GetPathValue<T, P extends string> =
  P extends `${infer Key}.${infer Rest}`
    ? Key extends keyof T
      ? GetPathValue<T[Key], Rest>
      : never
    : P extends keyof T
    ? T[P]
    : never;

interface Database {
  connections: {
    primary: { port: number };
  };
}

type Port = GetPathValue<Database, "connections.primary.port">; // number
```
</details>
