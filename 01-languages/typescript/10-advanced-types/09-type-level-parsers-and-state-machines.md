# Type-Level Parsers and State Machines

> Compiling string domain-specific languages (DSLs) at type-checking time: state machines, tokenizers, recursive-descent parsers, and AST synthesis entirely in TypeScript types.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [08-type-level-programming.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/08-type-level-programming.md), [04-template-literal-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/04-template-literal-types.md) |
| **Time to Read** | 26 minutes |
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
7. [Common Mistakes & Anti-Patterns](#7-Common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
Modern enterprise web development relies heavily on string-based DSLs:
- SQL queries (`SELECT id, email FROM users WHERE active = true`)
- GraphQL documents (`query { user { id, profile { avatar } } }`)
- URL route patterns (`/org/:orgId/projects/:projectId`)
- Regular expressions and formatting strings

Historically, these strings were opaque to TypeScript. If a developer wrote a typo in a SQL column name, TypeScript had no way of knowing until runtime.

By combining template literal pattern matching, recursive conditional types, and finite state machines, developers can author **Type-Level Parsers** that parse string DSLs into structured AST objects at compile time.

---

## 2. Core Concepts

### The Parsing Pipeline in Types
A type-level parser follows the same classical phases as a compiler:
1. **Lexing / Tokenization**: Consumes raw string characters, splitting them into an array of typed tokens.
2. **State Transition**: A Deterministic Finite Automaton (DFA) transitions between parser states.
3. **AST Generation**: Recursively folds tokens into typed object trees.

```
Raw String: "name=ada&age=36"
               |
        Tokenize / Split
               v
Tokens: ["name=ada", "age=36"]
               |
         Parse Key-Value
               v
Parsed AST: { name: "ada"; age: "36" }
```

---

## 3. How It Works Internally

```
Type State Machine:
Current State: "IDLE"
Input String:  "START -> RUNNING"
                     |
                     v
  Match "START": Transition state to "RUNNING"
  Emit state change event in type signature
```

Mechanics:
- States are represented as string literal union types (`"IDLE" | "PROCESSING" | "COMPLETED"`).
- Transitions are modeled as mapped lookup tables: `Record<State, Record<Event, State>>`.
- If an invalid event is applied to a state, the transition lookup resolves to `never`, halting compilation with a type error.

---

## 4. Syntax & API Reference

### Classical State Transition Matrix
```typescript
type Transitions = {
  IDLE: { START: "RUNNING" };
  RUNNING: { PAUSE: "PAUSED"; STOP: "IDLE" };
  PAUSED: { RESUME: "RUNNING"; STOP: "IDLE" };
};

type NextState<
  Current extends keyof Transitions,
  Event extends keyof Transitions[Current]
> = Transitions[Current][Event];

type S1 = NextState<"IDLE", "START">; // "RUNNING"
type S2 = NextState<"RUNNING", "PAUSE">; // "PAUSED"
```

---

## 5. Practical Examples

### 5.1 URL Query String Parser
```typescript
type ParseParam<T extends string> = T extends `${infer Key}=${infer Val}`
  ? { [K in Key]: Val }
  : never;

type MergeObjects<A, B> = {
  [K in keyof A | keyof B]: K extends keyof A ? A[K] : K extends keyof B ? B[K] : never;
};

type ParseQuery<Q extends string> =
  Q extends `${infer Param}&${infer Rest}`
    ? MergeObjects<ParseParam<Param>, ParseQuery<Rest>>
    : ParseParam<Q>;

type Parsed = ParseQuery<"mode=dark&filter=active&limit=10">;
// { mode: "dark"; filter: "active"; limit: "10" }
```

### 5.2 Real-World Application: Type-Level SQL SELECT Column Extractor
```typescript
type Trim<S extends string> =
  S extends ` ${infer R}` ? Trim<R> :
  S extends `${infer R} ` ? Trim<R> :
  S;

type SplitColumns<S extends string> =
  S extends `${infer Col},${infer Rest}`
    ? [Trim<Col>, ...SplitColumns<Rest>]
    : [Trim<S>];

type ParseSelect<Query extends string> =
  Query extends `SELECT ${infer Cols} FROM ${string}`
    ? SplitColumns<Cols>[number]
    : never;

type Columns = ParseSelect<"SELECT id, username, email FROM users">;
// "id" | "username" | "email"
```

### 5.3 Edge Case: Parsing Quoted Strings with Escapes
```typescript
type Unquote<S extends string> =
  S extends `"${infer Content}"` ? Content :
  S extends `'${infer Content}'` ? Content :
  S;

type Clean = Unquote<'"my-token"'>; // "my-token"
```

### 5.4 Failure Case: Illegal Transition in State Machine
```typescript
// @ts-expect-error TS2345: Argument of type '"RESUME"' is not assignable to parameter of type '"PAUSE" | "STOP"'.
type Invalid = NextState<"RUNNING", "RESUME">;
```

### 5.5 Idiomatic Pattern: Strongly Typed Route Matcher with Typed Return
```typescript
type RouteParams<Route extends string> =
  Route extends `${string}:${infer Param}/${infer Rest}`
    ? { [K in Param]: string } & RouteParams<`/${Rest}`>
    : Route extends `${string}:${infer Param}`
    ? { [K in Param]: string }
    : {};

function defineRoute<R extends string>(
  route: R,
  handler: (params: RouteParams<R>) => void
): void {
  console.log("Registered route", route);
}

defineRoute("/org/:orgId/users/:userId", (params) => {
  console.log(params.orgId, params.userId);
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Recursion Depth on Long Strings**: Complex parsers consume multiple stack frames per character. Strings longer than ~100 characters can trigger `TS2589`.
2. **Non-Greedy Delimiter Traps**: In template literals, the first `infer` captures the minimal substring. Structure delimiters carefully.
3. **Union Merging Overhead**: Merging hundreds of parsed object fragments can trigger compiler union complexity warnings.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Parsing entire SQL queries at compile time | Overwhelms compiler with massive grammars | Parse only critical clauses (SELECT columns) | Slow IDE autocompletion |
| Forgetting whitespace trimming | Leaves spaces in extracted keys (e.g. `" id"`) | Add recursive `Trim<S>` helper | Unmatched property keys |
| Non-tail recursive tokenizers | Hits instantiation limit quickly | Use accumulator tuples in tail position | `TS2589: Type instantiation is excessively deep` |
| Creating circular state transitions | Can cause infinite loops if unrolled naively | Use explicit transition matrix | Compiler stack overflow |
| Building full ASTs when simple union suffices | Unnecessary compiler overhead | Extract only the needed union of identifiers | Slower build times |
| Parsing untyped dynamic runtime variables | Template types only work on compile-time literals | Enforce literal inputs or use `as const` | Widen to `string` / no parsing |

---

## 8. Performance & Type-Checker Complexity
- Measure type parsing latency with `tsc --extendedDiagnostics`.
- Keep parsed DSL strings concise and scope compile-time parsing to API and route definition files.

---

## 9. Compiler Internals
- Template literal matching compiles into regex-like sub-string slices in `checker.ts`.
- If an expression contains multiple `infer` holes, the checker executes backtracking until a valid assignment is found.

---

## 10. Best Practices & Design Guidelines
- Use type-level parsers for routing tables, SQL query builders, and environment format validators.
- Always implement a `Trim<S>` utility to sanitize parsed identifier tokens.
- Return flat literal unions whenever possible rather than complex deeply nested AST trees.

---

## 11. Real-World Case Study
### Type-Safe GraphQL Document Operation Extractor
```typescript
type ExtractOperation<Gql extends string> =
  Gql extends `${string}query ${infer OpName} {${string}`
    ? { operationType: "query"; name: OpName }
    : Gql extends `${string}mutation ${infer OpName} {${string}`
    ? { operationType: "mutation"; name: OpName }
    : never;

type QueryDoc = "query GetUserList { users { id } }";
type Op = ExtractOperation<QueryDoc>;
// { operationType: "query"; name: "GetUserList" }
```

---

## 12. Interview Questions

<details>
<summary>1. How do you implement a state machine in TypeScript types?</summary>

By modeling states as string literal types and transitions as an object matrix (`type Transitions = { STATE: { EVENT: NEXT_STATE } }`), then querying next states via indexed access (`Transitions[CurrentState][Event]`).
</details>

<details>
<summary>2. How can template literal types be used as a tokenizer?</summary>

By using recursive pattern matching with delimiters:
```typescript
type Tokenize<S extends string, Delim extends string> =
  S extends `${infer Head}${Delim}${infer Tail}`
    ? [Head, ...Tokenize<Tail, Delim>]
    : [S];
```
</details>

<details>
<summary>3. Why is whitespace trimming essential in type-level string parsers?</summary>

Because template literal pattern matching preserves literal whitespace. If a user writes `"a, b"`, splitting by `","` leaves a leading space on `" b"`, causing key mismatches unless trimmed.
</details>

<details>
<summary>4. What prevents type-level parsers from parsing arbitrary-length programs?</summary>

The TypeScript compiler's internal recursion depth limit (~1,000 steps for tail recursion) and memory footprint limits.
</details>

<details>
<summary>5. How can you parse a route parameter like <code>/users/:id</code> into a strongly typed object?</summary>

By matching the `:` prefix:
```typescript
type RouteParam<T> = T extends `${string}:${infer P}` ? { [K in P]: string } : {};
```
</details>

<details>
<summary>6. What happens if a type-level state machine receives an invalid event?</summary>

The indexed access `Transitions[State][InvalidEvent]` evaluates to `never` or triggers a compile-time indexing error, preventing invalid state transitions.
</details>

<details>
<summary>7. How can you merge an array of parsed key-value pairs into a single object?</summary>

Using distributive mapped types or recursive intersection reduction:
```typescript
type Merge<T> = { [K in keyof T]: T[K] } & {};
```
</details>

<details>
<summary>8. How do libraries like Kysely or tRPC benefit from type-level parsers?</summary>

They extract route parameters, query identifiers, and database column names from string literals at compile time, eliminating the possibility of runtime typos.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: String Trimmer (Easy)
Write a utility `Trim<S>` that strips leading and trailing spaces from a string literal type.
<details>
<summary>Solution</summary>

```typescript
type Trim<S extends string> =
  S extends ` ${infer R}` ? Trim<R> :
  S extends `${infer R} ` ? Trim<R> :
  S;

type Res = Trim<"   hello world   ">; // "hello world"
```
</details>

### Exercise 2: Simple Key-Value Parser (Easy)
Parse a string of format `"key=value"` into `{ key: string; value: string }`.
<details>
<summary>Solution</summary>

```typescript
type ParseKV<S extends string> =
  S extends `${infer K}=${infer V}` ? { key: K; value: V } : never;

type Res = ParseKV<"theme=dark">; // { key: "theme"; value: "dark" }
```
</details>

### Exercise 3: Type-Safe State Machine Validator (Medium)
Create a function that only permits valid state transitions based on an internal transition matrix.
<details>
<summary>Solution</summary>

```typescript
type StateMap = {
  LOCKED: { UNLOCK: "UNLOCKED" };
  UNLOCKED: { LOCK: "LOCKED"; OPEN: "OPEN" };
  OPEN: { CLOSE: "UNLOCKED" };
};

class Door<Current extends keyof StateMap> {
  transition<E extends keyof StateMap[Current]>(
    event: E
  ): Door<StateMap[Current][E] & keyof StateMap> {
    return new Door() as any;
  }
}

const door = new Door<"LOCKED">();
const unlocked = door.transition("UNLOCK"); // Door<"UNLOCKED">
const open = unlocked.transition("OPEN");   // Door<"OPEN">
```
</details>

### Exercise 4: Parse Dotted Field Path to Tuple (Medium)
Convert `"user.profile.avatar"` into `["user", "profile", "avatar"]`.
<details>
<summary>Solution</summary>

```typescript
type PathToTuple<P extends string> =
  P extends `${infer Head}.${infer Tail}`
    ? [Head, ...PathToTuple<Tail>]
    : [P];

type T = PathToTuple<"user.profile.avatar">; // ["user", "profile", "avatar"]
```
</details>

### Exercise 5: Compile-Time Calculator Expression Evaluator (Hard)
Parse and evaluate simple addition strings of two single digits like `"2+3"`.
<details>
<summary>Solution</summary>

```typescript
type TupleOf<L extends number, T extends any[] = []> =
  T["length"] extends L ? T : TupleOf<L, [...T, any]>;

type ParseInt<S extends string> =
  S extends "0" ? 0 : S extends "1" ? 1 : S extends "2" ? 2 : S extends "3" ? 3 :
  S extends "4" ? 4 : S extends "5" ? 5 : S extends "6" ? 6 : S extends "7" ? 7 :
  S extends "8" ? 8 : S extends "9" ? 9 : 0;

type AddStrings<S extends string> =
  S extends `${infer A}+${infer B}`
    ? [...TupleOf<ParseInt<A>>, ...TupleOf<ParseInt<B>>]["length"]
    : never;

type Sum = AddStrings<"4+5">; // 9
```
</details>

### Exercise 6: Complete JSON Lexer & Parser in Types (Hard)
Parse a flat JSON string with string and boolean fields into an object type.
<details>
<summary>Solution</summary>

```typescript
type ParseJsonValue<V extends string> =
  V extends "true" ? true :
  V extends "false" ? false :
  V extends `"${infer Str}"` ? Str :
  V;

type ParseJsonPair<P extends string> =
  P extends `"${infer Key}": ${infer Val}`
    ? { [K in Key]: ParseJsonValue<Val> }
    : {};

type ParseJsonRecord<S extends string> =
  S extends `{${infer Content}}`
    ? Content extends `${infer Pair}, ${infer Rest}`
      ? ParseJsonPair<Pair> & ParseJsonRecord<`{${Rest}}`>
      : ParseJsonPair<Content>
    : {};

type Result = ParseJsonRecord<'{"name": "Alice", "active": true}'>;
// { name: "Alice" } & { active: true }
```
</details>
