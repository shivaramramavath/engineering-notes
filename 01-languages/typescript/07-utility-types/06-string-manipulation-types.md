# String Manipulation Types

> Built-in intrinsic string manipulation types (`Uppercase`, `Lowercase`, `Capitalize`, `Uncapitalize`) pair with template literal types to parse, re-case, and validate domain strings entirely at compile time.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [04-building-custom-utility-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/04-building-custom-utility-types.md), [05-generic-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/05-generic-inference.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 4.1 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `target: ES2022` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-Common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
JavaScript codebases frequently deal with string naming conventions:
- Converting `snake_case` database column names to `camelCase` JavaScript properties.
- Building type-safe event listener names (e.g. `on<EventName>Changed`).
- Enforcing URI formats (`https://`), hex colors (`#ffffff`), or semantic version strings.

Before TypeScript 4.1, string types were either generic `string` or static literal unions (`"a" | "b"`). Developers could not transform strings at type-checking time.

TypeScript 4.1 introduced **Template Literal Types** and **Intrinsic String Manipulation Utilities**:
- **`Uppercase<StringType>`**
- **`Lowercase<StringType>`**
- **`Capitalize<StringType>`**
- **`Uncapitalize<StringType>`**

These empower developers to build compile-time string parsers, case converters, and strongly-typed formatting layers.

---

## 2. Core Concepts

### The Four Intrinsic String Utilities
```typescript
type Greeting = "hello world";

type Upper = Uppercase<Greeting>;     // "HELLO WORLD"
type Lower = Lowercase<"SYSTEM_RUN">;  // "system_run"
type Cap   = Capitalize<"user">;       // "User"
type Uncap = Uncapitalize<"Service">;  // "service"
```

### Distribution Over String Unions
When applied to unions of string literals, string manipulation types distribute automatically over each member:
```typescript
type HttpVerb = "get" | "post" | "delete";
type UpperVerb = Uppercase<HttpVerb>;
// "GET" | "POST" | "DELETE"
```

### Template Literal Pattern Matching with `infer`
By combining template literals with conditional `infer`, TypeScript can parse substrings dynamically:
```typescript
type ExtractEventTarget<T extends string> =
  T extends `on${infer Target}Click` ? Target : never;

type Target = ExtractEventTarget<"onButtonClick">; // "Button"
```

---

## 3. How It Works Internally

```
Type Declaration in lib.es5.d.ts:

type Uppercase<S extends string> = intrinsic;
type Lowercase<S extends string> = intrinsic;
type Capitalize<S extends string> = intrinsic;
type Uncapitalize<S extends string> = intrinsic;
```

Unlike standard utility types written in TypeScript syntax, these four utilities are marked **`intrinsic`**.
- The compiler directly intercepts the type application in `src/compiler/checker.ts`.
- It executes native string transformation functions (e.g. `String.prototype.toUpperCase()`) directly inside the compiler process during type checking.
- The resulting transformed string literal type is interned and returned.

---

## 4. Syntax & API Reference

### Combining Template Literals and Case Modifiers
```typescript
type EventType = "click" | "hover" | "focus";
type HandlerName = `on${Capitalize<EventType>}`;
// "onClick" | "onHover" | "onFocus"

type EnvVar = "db_port" | "db_host";
type ConfigKey = Uppercase<`app_${EnvVar}`>;
// "APP_DB_PORT" | "APP_DB_HOST"
```

---

## 5. Practical Examples

### 5.1 Basic Event Handler Generator
```typescript
type DOMEvents = "change" | "submit" | "reset";

type EventHandlerProps = {
  [K in DOMEvents as `on${Capitalize<K>}`]: (event: any) => void;
};

// Resulting interface:
// {
//   onChange: (event: any) => void;
//   onSubmit: (event: any) => void;
//   onReset: (event: any) => void;
// }
```

### 5.2 Real-World Application: CamelCase to SnakeCase Converter
```typescript
type CamelToSnakeCase<S extends string> = S extends `${infer Head}${infer Tail}`
  ? `${Head extends Uppercase<Head> ? `_${Lowercase<Head>}` : Head}${CamelToSnakeCase<Tail>}`
  : S;

type S1 = CamelToSnakeCase<"userName">;       // "user_name"
type S2 = CamelToSnakeCase<"createdDateTime">; // "created_date_time"
```

### 5.3 Edge Case: Parsing Hex Colors
```typescript
type HexDigit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" | "a" | "b" | "c" | "d" | "e" | "f";

type ShortHexColor = `#${HexDigit}${HexDigit}${HexDigit}`;

const validColor: ShortHexColor = "#f0a"; // OK
// @ts-expect-error TS2322: Type '"#xyz"' is not assignable to type '`#${HexDigit}${HexDigit}${HexDigit}`'.
const badColor: ShortHexColor = "#xyz";
```

### 5.4 Failure Case: Passing Non-String to Intrinsic
```typescript
// @ts-expect-error TS2344: Type 'number' does not satisfy the constraint 'string'.
type BadUpper = Uppercase<42>;
```

### 5.5 Idiomatic Pattern: Strongly Typed Route Parameter Extractor
```typescript
type ExtractRouteParams<T extends string> =
  T extends `${infer _}:${infer Param}/${infer Rest}`
    ? Param | ExtractRouteParams<`/${Rest}`>
    : T extends `${infer _}:${infer Param}`
    ? Param
    : never;

type Route = "/users/:userId/posts/:postId";
type Params = ExtractRouteParams<Route>; // "userId" | "postId"
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Recursion Depth on Long Strings**: Parsing a string of more than 50 characters via recursion will exceed TypeScript's recursion limit (`TS2589`).
2. **Special Characters & Unicode**: Case conversion is locale-agnostic and mirrors JavaScript's root locale casing.
3. **Cartesian Product Explosion**: Combining template literals across multiple unions (``${A}_${B}_${C}``) creates $N_A 	imes N_B 	imes N_C$ types. Keep union sizes modest.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Parsing huge strings with recursive template types | Exceeds compiler recursion stack | Keep template type parsing to identifiers/routes | `TS2589: Type instantiation is excessively deep` |
| Multiplying large unions in template literal | Exponential combinatorial explosion ($O(N^K)$) | Separate checks or limit union sizes | Slow compiler / out of memory |
| Passing non-string to `Uppercase` | Intrinsics require string constraint | Constrain generic with `T extends string` | `TS2344: Type does not satisfy constraint 'string'` |
| Attempting regex matching in types | TypeScript does not support arbitrary regex | Use recursive template pattern matching | Syntax errors |
| Forgetting `infer` in template literal patterns | Pattern matching fails to capture substrings | Use `${infer Var}` syntax | Incomplete matching |
| Capitalizing string with leading symbols or digits | Digits and symbols have no uppercase equivalent | Sanitize input or account for non-letter prefixes | Unchanged string |

---

## 8. Performance & Type-Checker Complexity
- Intrinsic utilities (`Uppercase`, etc.) are fast $O(1)$ operations because they execute compiled native C++/V8 string routines.
- Template literal unions, however, compute Cartesian products eagerly. `${"a"|"b"}${"1"|"2"}` generates 4 types instantly. Watch union branching factors.

---

## 9. Compiler Internals
- Intrinsic types are identified by `TypeFlags.Intrinsic`.
- In `src/compiler/checker.ts`, `evaluateIntrinsicType` routes `Uppercase` to `toUpperCase()`, `Lowercase` to `toLowerCase()`, etc.
- When arguments are string literal types, the checker creates a new string literal type with the transformed text and caches it in the compiler intern pool.

---

## 10. Best Practices & Design Guidelines
- Use template literal types for URL route matching, event emitter contracts, and CSS property validators.
- Pair `Capitalize` with key remapping in mapped types to enforce standard getter and listener conventions.
- Provide sensible fallbacks for generic string arguments to avoid accidental Cartesian blowouts.

---

## 11. Real-World Case Study
### Type-Safe REST Route Parameter Extractor
```typescript
type RouteParams<T extends string> = {
  [K in ExtractRouteParams<T>]: string;
};

class ApiRouter {
  get<Path extends string>(
    path: Path,
    handler: (params: RouteParams<Path>) => void
  ): void {
    console.log(`Registered route ${path}`);
  }
}

const router = new ApiRouter();

router.get("/org/:orgId/members/:memberId", (params) => {
  // params is strictly typed as: { orgId: string; memberId: string }
  console.log(params.orgId, params.memberId);
});
```

---

## 12. Interview Questions

<details>
<summary>1. What are the four intrinsic string manipulation utilities in TypeScript?</summary>

`Uppercase<S>`, `Lowercase<S>`, `Capitalize<S>`, and `Uncapitalize<S>`.
</details>

<details>
<summary>2. What makes these string utilities "intrinsic"?</summary>

They are declared with the `intrinsic` keyword in `lib.es5.d.ts` and implemented directly in the compiler's TypeScript type-checker engine, rather than being expressed in TypeScript type syntax.
</details>

<details>
<summary>3. How do template literal types handle union types?</summary>

Template literal types automatically distribute over unions. For example, ``btn-${"primary" | "secondary"}`` expands to `"btn-primary" | "btn-secondary"`.
</details>

<details>
<summary>4. How can you parse the protocol out of a URL string using template literals?</summary>

```typescript
type GetProtocol<T extends string> = T extends `${infer Protocol}://${string}` ? Protocol : never;
type Proto = GetProtocol<"https://github.com">; // "https"
```
</details>

<details>
<summary>5. What happens when you apply <code>Capitalize</code> to an empty string?</summary>

It returns an empty string (`""`) without errors.
</details>

<details>
<summary>6. Can template literal types match numbers and booleans?</summary>

Yes, template literals coerce numbers, booleans, bigints, and null/undefined into their string representations (e.g. ``val: ${number}``).
</details>

<details>
<summary>7. What causes error TS2589 when parsing strings with template literal types?</summary>

Exceeding the compiler's recursion depth limit when parsing strings character-by-character. Deep string recursion should be restricted to shorter tokens.
</details>

<details>
<summary>8. How do you generate dynamic getter method names from an object's keys?</summary>

```typescript
type Getters<T> = {
  [K in keyof T as `get${Capitalize<string & K>}`]: () => T[K];
};
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Uppercase Union (Easy)
Convert a union of environment names into uppercase.
<details>
<summary>Solution</summary>

```typescript
type Env = "dev" | "stage" | "prod";
type UpperEnv = Uppercase<Env>; // "DEV" | "STAGE" | "PROD"
```
</details>

### Exercise 2: Prefix and Capitalize (Easy)
Write a utility `EventName<T extends string>` that produces ``on${Capitalize<T>}``.
<details>
<summary>Solution</summary>

```typescript
type EventName<T extends string> = `on${Capitalize<T>}`;
type ClickEvent = EventName<"click">; // "onClick"
```
</details>

### Exercise 3: SnakeCase to CamelCase Converter (Medium)
Convert snake_case strings to camelCase (e.g. `"foo_bar_baz"` -> `"fooBarBaz"`).
<details>
<summary>Solution</summary>

```typescript
type SnakeToCamel<S extends string> = S extends `${infer Head}_${infer Tail}`
  ? `${Head}${Capitalize<SnakeToCamel<Tail>>}`
  : S;

type Res = SnakeToCamel<"user_first_name">; // "userFirstName"
```
</details>

### Exercise 4: Validated SemVer Parser (Medium)
Create a template literal type matching `Major.Minor.Patch` where each is a number.
<details>
<summary>Solution</summary>

```typescript
type SemVer = `${number}.${number}.${number}`;

const v1: SemVer = "1.2.3"; // OK
// @ts-expect-error TS2322: Type '"1.2"' is not assignable to type '`${number}.${number}.${number}`'.
const v2: SemVer = "1.2";
```
</details>

### Exercise 5: Query String Parser (Hard)
Parse a URL query string `"key1=val1&key2=val2"` into an object type.
<details>
<summary>Solution</summary>

```typescript
type ParseQueryParam<T extends string> = T extends `${infer K}=${infer V}`
  ? { [Key in K]: V }
  : never;

type MergeParams<A, B> = {
  [K in keyof A | keyof B]: K extends keyof A
    ? A[K]
    : K extends keyof B
    ? B[K]
    : never;
};

type ParseQueryString<Q extends string> =
  Q extends `${infer First}&${infer Rest}`
    ? MergeParams<ParseQueryParam<First>, ParseQueryString<Rest>>
    : ParseQueryParam<Q>;

type Parsed = ParseQueryString<"name=ada&lang=ts&mode=fast">;
// { name: "ada"; lang: "ts"; mode: "fast" }
```
</details>

### Exercise 6: String Joiner Utility (Hard)
Implement a utility `Join<T, Delimiter>` that joins a tuple of string literals with a delimiter.
<details>
<summary>Solution</summary>

```typescript
type Join<T extends readonly string[], Delim extends string> =
  T extends []
    ? ""
    : T extends [infer Single extends string]
    ? Single
    : T extends [infer Head extends string, ...infer Rest extends string[]]
    ? `${Head}${Delim}${Join<Rest, Delim>}`
    : string;

type Joined = Join<["a", "b", "c"], "/">; // "a/b/c"
```
</details>
