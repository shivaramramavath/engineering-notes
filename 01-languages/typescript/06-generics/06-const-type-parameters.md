# Const Type Parameters

> Introduced in TypeScript 5.0, `const` type parameters enable generic functions to infer the most specific, deeply immutable literal types at call sites without requiring callers to write `as const`.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md), [02-arrays-and-tuples.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/02-arrays-and-tuples.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `target: ES2022` |

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
Before TypeScript 5.0, when an object or array literal was passed into a generic function, TypeScript's default inference algorithm widened literal types to their general base types:
- `"admin"` widened to `string`
- `[1, 2, 3]` widened to `number[]`

To preserve exact string literals or tuple shapes, callers were forced to write `as const` at every call site:
```typescript
// Pre-TS 5.0: Callers had to remember 'as const'
setRoutes(["/home", "/dashboard", "/settings"] as const);
```
If a caller omitted `as const`, type safety degraded.

TypeScript 5.0 introduced **Const Type Parameters** (`<const T>`). This shifts the responsibility from the *caller* to the *library author*: by prefixing a type parameter with `const`, the compiler automatically applies `as const`-like inference to argument expressions at the call site.

---

## 2. Core Concepts

### Syntax and Inference Shift
Prefixing a type parameter with the `const` modifier instructs the type checker to prefer literal and `readonly` tuple/object types:

```typescript
// Standard Generic:
function defineRoutesStandard<T extends readonly string[]>(routes: T): T {
  return routes;
}
const r1 = defineRoutesStandard(["/a", "/b"]); 
// Inferred type of r1: string[]

// Const Type Parameter (TS 5.0+):
function defineRoutesConst<const T extends readonly string[]>(routes: T): T {
  return routes;
}
const r2 = defineRoutesConst(["/a", "/b"]); 
// Inferred type of r2: readonly ["/a", "/b"]
```

### Object Inferences with `const`
`const` type parameters deeply retain literal object structures:
```typescript
function createConfig<const T extends Record<string, unknown>>(config: T): T {
  return config;
}

const cfg = createConfig({
  endpoint: "https://api.domain.internal",
  retries: 3,
  headers: { "X-App-Id": "client-v1" }
});
// cfg.endpoint has type "https://api.domain.internal" (not string)
// cfg.retries has type 3 (not number)
// cfg.headers has type { readonly "X-App-Id": "client-v1" }
```

---

## 3. How It Works Internally

```
Call Site: fn(["foo", "bar"])
                 |
Standard Generic: | Widens elements to primitive
                 +---> Inferred T: string[]
                 |
<const T>:       | Infers readonly tuple with literal types
                 +---> Inferred T: readonly ["foo", "bar"]
```

When the type checker inspects an argument passed to a parameter typed as a `const` type variable:
1. It alters the expression's contextual type requirement to ask for literal types.
2. For array literals, it creates a `TupleType` rather than an `ArrayType`.
3. It marks properties as `readonly` recursively.
4. If a mutable constraint is specified (e.g., `<const T extends string[]>`), the compiler cannot satisfy the `readonly` inference and falls back to a mutable array! Always constrain with `readonly`.

---

## 4. Syntax & API Reference

### Combining with Constraints
```typescript
// Constrained to readonly string array
function registerCommands<const T extends readonly string[]>(commands: T): T {
  return commands;
}

// Constrained to object record
function makeRegistry<const T extends Record<string, any>>(registry: T): T {
  return registry;
}

// Classes and Methods
class Router {
  register<const TRoute extends { path: string; method: "GET" | "POST" }>(route: TRoute): TRoute {
    return route;
  }
}
```

---

## 5. Practical Examples

### 5.1 Basic Route Registry
```typescript
function createRouter<const TRoutes extends readonly string[]>(routes: TRoutes) {
  return {
    routes,
    hasRoute(route: TRoutes[number]): boolean {
      return routes.includes(route as any);
    }
  };
}

const router = createRouter(["/users", "/posts", "/settings"]);
// router.hasRoute argument is strictly: "/users" | "/posts" | "/settings"
router.hasRoute("/users");
```

### 5.2 Real-World Application: Type-Safe Schema Form Builder
```typescript
interface FieldConfig<TName extends string, TType extends "string" | "number" | "boolean"> {
  name: TName;
  type: TType;
  required?: boolean;
}

function buildFormSchema<const TFields extends readonly FieldConfig<string, any>[]>(fields: TFields) {
  return {
    fields,
    validate(data: Record<string, unknown>): boolean {
      for (const field of fields) {
        if (field.required && data[field.name] === undefined) return false;
      }
      return true;
    }
  };
}

const form = buildFormSchema([
  { name: "email", type: "string", required: true },
  { name: "age", type: "number", required: false },
]);

// Inferred field names are exact string literals: "email" | "age"
type FormFieldNames = typeof form.fields[number]["name"]; // "email" | "age"
```

### 5.3 Edge Case: Mutable Constraints Defeating `const`
```typescript
// Mutable constraint:
function invalidConst<const T extends string[]>(items: T): T {
  return items;
}

// Inferred as string[] because 'string[]' constraint rejects 'readonly ["a", "b"]'
const res = invalidConst(["a", "b"]); // type: string[]
```

### 5.4 Failure Case: Passing Readonly to Mutable Consumer
```typescript
function mutateList(list: string[]): void {
  list.push("extra");
}

function processItems<const T extends readonly string[]>(items: T): void {
  // @ts-expect-error TS2345: Argument of type 'T' is not assignable to parameter of type 'string[]'. The type 'T' is 'readonly' and cannot be assigned to the mutable type 'string[]'.
  mutateList(items);
}
```

### 5.5 Idiomatic Pattern: Strongly-Typed State Machine
```typescript
interface StateMachineConfig<TState extends string, TEvent extends string> {
  initial: TState;
  transitions: Record<TState, Record<TEvent, TState>>;
}

function defineStateMachine<const TConfig extends StateMachineConfig<string, string>>(config: TConfig): TConfig {
  return config;
}

const machine = defineStateMachine({
  initial: "IDLE",
  transitions: {
    IDLE: { START: "RUNNING" },
    RUNNING: { STOP: "IDLE" },
  },
});

type States = typeof machine.initial; // "IDLE"
type Transitions = typeof machine.transitions;
// Exact literal transition map preserved!
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Mutable Constraints Reject Const Inference**: If the constraint is mutable (`T extends any[]` or `T extends object`), TypeScript cannot produce a `readonly` type and widens to mutable types. Always write `T extends readonly unknown[]`.
2. **Pre-allocated Variables**: `const` type parameters only infer literals from *inline expressions* passed directly into the function call. Passing a pre-existing mutable variable does not retroactively make it literal:
   ```typescript
   const arr = ["a", "b"]; // inferred as string[]
   defineRoutesConst(arr); // T is string[], not ["a", "b"]
   ```
3. **Only Modifies Type Inference**: The `const` modifier on type parameters does not enforce runtime immutability. The runtime output JavaScript is unmodified.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Omitting `readonly` in array constraints | Defeats `const` inference; falls back to mutable widening | Use `<const T extends readonly unknown[]>` | Unexpected widened `string[]` type |
| Passing pre-declared `let` / `var` variables | Value is already widened before function call | Pass literal inline or declare variable with `as const` | Premature widening |
| Expecting mutable returned array | `const` type parameter returns `readonly` tuple | Do not attempt mutation; use spread to clone if needed | `TS2339: Property 'push' does not exist on 'readonly ...'` |
| Using `const` on non-generic parameters | `const` modifier is only valid on generic type declarations | Apply `const` to `<const T>`, not `(const x: T)` | `TS1005: ',' expected.` |
| Over-using on simple primitive arguments | Redundant overhead when literal widening is not an issue | Use standard `T extends string` | Cluttered signatures |
| Attempting to assign `readonly` result to mutable sink | Readonly arrays are invariant/incompatible with mutable arrays | Cast or clone explicitly before passing to legacy APIs | `TS2345: The type is 'readonly' and cannot be assigned...` |

---

## 8. Performance & Type-Checker Complexity
- Const type parameters create deeply nested `readonly` tuple and literal object types in the compiler cache.
- For extremely large nested object structures (e.g., thousands of properties), generating full literal types increases memory consumption compared to widened primitive types. Scope literal inference to configuration boundaries.

---

## 9. Compiler Internals
- Parsed as a `TypeParameterDeclaration` with modifier `SyntaxKind.ConstKeyword`.
- In `src/compiler/checker.ts`, `hasSyntacticModifier(param, ModifierFlags.Const)` is checked during `inferFromLiteralType`.
- When set, the inference engine treats argument literals as though they have contextual `as const` assertions, triggering tuple creation and marking property signatures with `Readonly`.

---

## 10. Best Practices & Design Guidelines
- Always pair `<const T>` with `readonly` constraints: `<const T extends readonly string[]>`.
- Use const type parameters when designing APIs for configuration builders, schema validators, state machines, and routing tables.
- Return the exact literal type `T` to allow callers to derive exhaustive unions from the function output.

---

## 11. Real-World Case Study
### Type-Safe SQL Query Builder Columns
```typescript
class SqlQueryBuilder<TTable extends string, TColumns extends readonly string[]> {
  constructor(
    private table: TTable,
    private columns: TColumns
  ) {}

  static selectFrom<TTable extends string, const TCols extends readonly string[]>(
    table: TTable,
    columns: TCols
  ): SqlQueryBuilder<TTable, TCols> {
    return new SqlQueryBuilder(table, columns);
  }

  where(column: TColumns[number], value: unknown): string {
    return `SELECT ${this.columns.join(", ")} FROM ${this.table} WHERE ${String(column)} = '${value}'`;
  }
}

// No 'as const' required at call site:
const query = SqlQueryBuilder.selectFrom("users", ["id", "email", "created_at"]);

// Type-checked column name:
query.where("email", "ada@example.com"); // OK
// query.where("password", "xxx"); // Compile error: TS2345
```

---

## 12. Interview Questions

<details>
<summary>1. What problem did const type parameters solve in TypeScript 5.0?</summary>

Before TypeScript 5.0, generic functions widened literal arguments (e.g. array literals became `string[]` instead of tuples). Callers had to remember to attach `as const` at every call site. Const type parameters (`<const T>`) allow library authors to enforce literal, readonly inference automatically without requiring callers to write `as const`.
</details>

<details>
<summary>2. Why does <code>function fn&lt;const T extends string[]&gt;(x: T)</code> fail to infer literal tuples?</summary>

The constraint `string[]` is mutable. Because literal tuple inference creates `readonly` tuples, and a `readonly` tuple cannot be assigned to a mutable `string[]` constraint, TypeScript abandons the literal inference and falls back to `string[]`. The constraint must be `readonly string[]`.
</details>

<details>
<summary>3. Does <code>&lt;const T&gt;</code> freeze or alter the runtime JavaScript object?</summary>

No. `const` on a type parameter is purely a compile-time type-level modifier. It produces zero runtime JavaScript and does not call `Object.freeze`.
</details>

<details>
<summary>4. How does <code>&lt;const T&gt;</code> behave when passed a pre-declared variable?</summary>

It infers the variable's existing type. If the variable was declared as `const x = ["a", "b"]` (inferred as `string[]`), `<const T>` receives `string[]`. It only generates literal tuples for inline literal expressions.
</details>

<details>
<summary>5. Can you use <code>const</code> on class type parameters?</summary>

Yes, starting in TypeScript 5.0, `class Container<const T> { ... }` is valid syntax, ensuring instance creation infers literal types from constructor arguments.
</details>

<details>
<summary>6. How do const type parameters interact with objects?</summary>

Object literals passed to `<const T>` are inferred with literal property types and `readonly` property modifiers recursively throughout the structure.
</details>

<details>
<summary>7. What is the difference between <code>as const</code> and <code>&lt;const T&gt;</code>?</summary>

`as const` is a value-space type assertion written by the consumer at the site of literal creation. `<const T>` is an API declaration written by the function author that applies the same inference rules automatically to arguments.
</details>

<details>
<summary>8. Can you combine <code>const</code> with default type arguments?</summary>

Yes: `<const T extends readonly string[] = readonly ["default"]>` is completely valid.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Literal Tuple Collector (Easy)
Implement a generic function `tuple<const T extends readonly unknown[]>(...args: T): T`.
<details>
<summary>Solution</summary>

```typescript
function tuple<const T extends readonly unknown[]>(...args: T): T {
  return args;
}

const t = tuple(1, "hello", true);
// type: readonly [1, "hello", true]
```
</details>

### Exercise 2: Strict Dictionary Builder (Easy)
Write `createDictionary<const T extends Record<string, string>>(entries: T): T`.
<details>
<summary>Solution</summary>

```typescript
function createDictionary<const T extends Record<string, string>>(entries: T): T {
  return entries;
}

const dict = createDictionary({ host: "localhost", env: "production" });
// type: { readonly host: "localhost"; readonly env: "production" }
```
</details>

### Exercise 3: Validated Config Option Selector (Medium)
Create a function `makeSelector` that accepts an array of options and returns a function that only accepts valid option members.
<details>
<summary>Solution</summary>

```typescript
function makeSelector<const TOptions extends readonly string[]>(options: TOptions) {
  return function select(choice: TOptions[number]): TOptions[number] {
    return choice;
  };
}

const selectTheme = makeSelector(["light", "dark", "system"]);
selectTheme("dark"); // OK
// selectTheme("blue"); // Type error
```
</details>

### Exercise 4: Discriminated Action Factory (Medium)
Implement an action creator that accepts an array of action definitions and preserves their exact action `type` literals.
<details>
<summary>Solution</summary>

```typescript
interface ActionDef<TName extends string> {
  name: TName;
  timestamp: number;
}

function createActionRegistry<const TActions extends readonly ActionDef<string>[]>(actions: TActions) {
  return actions;
}

const reg = createActionRegistry([
  { name: "INIT", timestamp: Date.now() },
  { name: "DESTROY", timestamp: Date.now() },
]);
type ActionNames = typeof reg[number]["name"]; // "INIT" | "DESTROY"
```
</details>

### Exercise 5: Generic Deep Event Router (Hard)
Build an event handler registration system where event listeners must match the exact string event names declared in a route table.
<details>
<summary>Solution</summary>

```typescript
interface RouteEntry {
  path: string;
  method: "GET" | "POST" | "PUT" | "DELETE";
}

class EventRouter<const TRoutes extends readonly RouteEntry[]> {
  constructor(public routes: TRoutes) {}

  dispatch(path: TRoutes[number]["path"], method: TRoutes[number]["method"]): void {
    console.log(`Dispatch ${method} ${path}`);
  }
}

const router = new EventRouter([
  { path: "/api/users", method: "GET" },
  { path: "/api/orders", method: "POST" },
]);

router.dispatch("/api/users", "GET"); // Valid
```
</details>

### Exercise 6: Strongly Typed Plugin Pipeline with Const Signatures (Hard)
Implement a pipeline registration function that chains middleware and captures every plugin's unique name as a literal union.
<details>
<summary>Solution</summary>

```typescript
interface Plugin<TName extends string> {
  name: TName;
  execute: (ctx: Record<string, unknown>) => void;
}

function createPipeline<const TPlugins extends readonly Plugin<string>[]>(plugins: TPlugins) {
  return {
    getPluginNames(): Array<TPlugins[number]["name"]> {
      return plugins.map((p) => p.name);
    },
    run(ctx: Record<string, unknown>): void {
      for (const p of plugins) {
        p.execute(ctx);
      }
    }
  };
}

const pipeline = createPipeline([
  { name: "logger", execute: (c) => console.log(c) },
  { name: "auth", execute: (c) => console.log("auth check") },
]);

type PNames = ReturnType<typeof pipeline.getPluginNames>[number]; // "logger" | "auth"
```
</details>
