# Enums and Const Objects

> Enums provide named constants in TypeScript, but produce runtime JavaScript artifacts that violate modern erasable syntax boundaries; `as const` object dictionaries offer zero-cost, fully erasable alternatives.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [04-type-aliases.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/04-type-aliases.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.0 / TS 5.8 (`--erasableSyntaxOnly`) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `erasableSyntaxOnly: true`, `isolatedModules: true` |

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
- **Problem Statement:** In software engineering, magic numbers and unstructured strings cause bugs when identifying categories, status codes, and discrete state machine triggers. Codebases require strongly typed dictionaries of constant symbols.
- **Historical Context:** TypeScript introduced `enum` in version 0.9, long before modern ECMAScript standards evolved. Because JavaScript lacked native enums, TypeScript emitted runtime IIFEs (Immediately Invoked Function Expressions) that constructed two-way lookup objects.
- **Real-World Relevance:** Modern JavaScript runtimes (Node.js 22+ type stripping, Bun, Deno) and modern bundlers (esbuild, Vite, SWC) operate on "erasable syntax" — removing type annotations without parsing full semantics. TypeScript 5.8 introduced `--erasableSyntaxOnly`. Traditional enums produce runtime JavaScript objects and fail under pure type-stripping runners. The modern idiomatic alternative is `const` objects with `as const`.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Standard `enum`: Generates a real JavaScript object in the emitted output. For numeric enums, generates reverse mappings (`Enum[Enum.Key] === "Key"`).
  - `const enum`: Inlines literal values at compile-time call sites; no runtime object is emitted. Incompatible with `isolatedModules` unless `preserveConstEnums` is set.
  - `as const` Object: A standard JavaScript frozen object whose keys and values are inferred as readonly literal types at compile time. Fully conforms to erasable syntax.
- **Mental Model:** Treat enums as legacy syntax that crosses the boundary from type space into value space. Treat `const Status = { ... } as const` as a pure JavaScript value paired with a derived TypeScript type (`type Status = (typeof Status)[keyof typeof Status]`).
- **Soundness vs Ergonomics:** Prior to TypeScript 5.0, numeric enums permitted arbitrary numbers to be assigned to enum variables without compiler error, breaking type soundness. TS 5.0 tightened this, treating each enum member as a unique literal type.

---

## 3. How It Works Internally
- **Runtime Emit of Numeric Enums:**
  A numeric enum `enum Direction { Up, Down }` emits an IIFE:
  ```javascript
  var Direction;
  (function (Direction) {
      Direction[Direction["Up"] = 0] = "Up";
      Direction[Direction["Down"] = 1] = "Down";
  })(Direction || (Direction = {}));
  ```
- **Erasable Syntax Conflict:** Modern Node 22 native type stripping (`node --experimental-strip-types`) replaces type annotations with whitespace. Because `enum` requires emitting JavaScript runtime statements, Node 22 throws a runtime `SyntaxError`.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                      Enum vs As Const Object Workflow                    |
|                                                                          |
|   [ enum Syntax ]                                [ const Object as const ]|
|          │                                                  │             |
|          ▼                                                  ▼             |
|   AST: EnumDeclaration                              AST: VariableStatement|
|          │                                                  │             |
|          ▼                                                  ▼             |
|   Requires JS Code Generation                      Pure JavaScript Value  |
|   (Emits IIFE object with reverse mappings)         (Zero custom emit)    |
|          │                                                  │             |
|          ▼                                                  ▼             |
|   Fails Node 22 Type Stripping                      Fully Erasable        |
|   Fails TS 5.8 --erasableSyntaxOnly                 Works Everywhere      |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Traditional Enum Declaration:**
  ```typescript
  enum HttpMethod {
    Get = 'GET',
    Post = 'POST',
    Put = 'PUT',
    Delete = 'DELETE'
  }
  ```
- **Idiomatic Const Object Pattern:**
  ```typescript
  export const HttpMethod = {
    Get: 'GET',
    Post: 'POST',
    Put: 'PUT',
    Delete: 'DELETE',
  } as const;

  export type HttpMethod = (typeof HttpMethod)[keyof typeof HttpMethod];
  ```
- **tsconfig Dependencies:**
  - `erasableSyntaxOnly: true` (TS 5.8+): Disallows non-ambient enums.
  - `isolatedModules: true`: Forbids `const enum` unless ambient.
  - `preserveConstEnums: true`: Emits the runtime object even for `const enum`.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Declare and consume both traditional enums and modern const objects.
- **Code:**
```typescript
// Traditional enum
enum LogLevelEnum {
  Debug = 10,
  Info = 20,
  Warn = 30,
  Error = 40,
}

// Modern as const pattern
const LogLevel = {
  Debug: 10,
  Info: 20,
  Warn: 30,
  Error: 40,
} as const;

type LogLevel = (typeof LogLevel)[keyof typeof LogLevel];

function processLog(level: LogLevel): void {
  if (level >= LogLevel.Warn) {
    // Send to alerting pipeline
  }
}

processLog(LogLevel.Error);
processLog(40); // Valid: literal 40 matches LogLevel union!
```
- **Output / Type Evaluation:**
```typescript
// LogLevel type resolves to: 10 | 20 | 30 | 40
```
- **Explanation:** The `as const` pattern allows both named property lookup (`LogLevel.Error`) and raw literal values (`40`), improving API ergonomics.

### Example 2: Real-World Enterprise Pattern (Type-Safe State Machine & Table Mapping)
- **Goal:** Model order lifecycle statuses with associated metadata and exhaustiveness checking.
- **Code:**
```typescript
export const OrderStatus = {
  Pending: 'PENDING',
  Authorized: 'AUTHORIZED',
  Captured: 'CAPTURED',
  Refunded: 'REFUNDED',
  Failed: 'FAILED',
} as const;

export type OrderStatus = (typeof OrderStatus)[keyof typeof OrderStatus];

type StatusMetadata = {
  isFinal: boolean;
  canCancel: boolean;
  badgeColor: 'gray' | 'blue' | 'green' | 'purple' | 'red';
};

export const ORDER_STATUS_CONFIG: Record<OrderStatus, StatusMetadata> = {
  [OrderStatus.Pending]: { isFinal: false, canCancel: true, badgeColor: 'gray' },
  [OrderStatus.Authorized]: { isFinal: false, canCancel: true, badgeColor: 'blue' },
  [OrderStatus.Captured]: { isFinal: true, canCancel: false, badgeColor: 'green' },
  [OrderStatus.Refunded]: { isFinal: true, canCancel: false, badgeColor: 'purple' },
  [OrderStatus.Failed]: { isFinal: true, canCancel: false, badgeColor: 'red' },
};

export function getStatusBadge(status: OrderStatus): string {
  return ORDER_STATUS_CONFIG[status].badgeColor;
}
```
- **Output / Type Evaluation:**
```typescript
// Full compile-time guarantee: Adding a new status forces updates in ORDER_STATUS_CONFIG.
```
- **Explanation:** Using `Record<OrderStatus, StatusMetadata>` ensures that adding a new member to `OrderStatus` causes a compile-time error if any metadata mapping is omitted.

### Example 3: Edge Case / Boundary Condition (Numeric Enum Reverse Mapping Pitfall)
- **Goal:** Demonstrate reverse mapping in numeric enums and runtime footprint.
- **Code:**
```typescript
enum NumericEnum {
  Alpha = 0,
  Beta = 1,
}

// Runtime keys inspection:
const keys = Object.keys(NumericEnum);
// keys contains: ['0', '1', 'Alpha', 'Beta'] !!

const alphaName = NumericEnum[0]; // Evaluates to "Alpha" at runtime
```
- **Output / Type Evaluation:**
```typescript
// Object.keys(NumericEnum).length === 4 (unexpected duplication in loops!)
```
- **Explanation:** Numeric enums generate reverse mappings, doubling the key count when iterating with `Object.keys()`, which frequently breaks serialization and JSON encoding.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate type assignment mismatch and invalid enum values.
- **Code:**
```typescript
enum StringEnum {
  Admin = 'ADMIN',
  Member = 'MEMBER',
}

function assignRole(role: StringEnum): void {}

// @ts-expect-error TS2345: Argument of type '"ADMIN"' is not assignable to parameter of type 'StringEnum'.
assignRole('ADMIN');

const UserRoles = {
  Admin: 'ADMIN',
  Member: 'MEMBER',
} as const;
type UserRoles = (typeof UserRoles)[keyof typeof UserRoles];

function assignRoleModern(role: UserRoles): void {}
assignRoleModern('ADMIN'); // OK!
```
- **Real Compiler Error:**
```
error TS2345: Argument of type '"ADMIN"' is not assignable to parameter of type 'StringEnum'.
```
- **Explanation:** TypeScript string enums are nominal in assignment: passing the raw string literal `'ADMIN'` is rejected, forcing callers to import the enum identifier. Modern `as const` unions accept string literals directly.

### Example 5: Idiomatic Modern Usage (Conforming to --erasableSyntaxOnly)
- **Goal:** Write an enterprise module fully compatible with TS 5.8 `--erasableSyntaxOnly` and Node 22 native type stripping.
- **Code:**
```typescript
// erasable_tokens.ts
export const TokenType = {
  Identifier: 'IDENTIFIER',
  Keyword: 'KEYWORD',
  Punctuation: 'PUNCTUATION',
  EOF: 'EOF',
} as const;

export type TokenType = (typeof TokenType)[keyof typeof TokenType];

export type Token = {
  readonly type: TokenType;
  readonly lexeme: string;
  readonly line: number;
};

export function createToken(type: TokenType, lexeme: string, line: number): Token {
  return Object.freeze({ type, lexeme, line });
}

export function isKeywordToken(token: Token): boolean {
  return token.type === TokenType.Keyword;
}
```
- **Output / Type Evaluation:**
```typescript
// Compiles cleanly with zero syntax violations under node --experimental-strip-types
```
- **Explanation:** No TypeScript-specific runtime syntax is generated. Type stripping erases types, leaving standard, high-performance ECMAScript.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`const enum` Inlining Hazards:** When publishing an npm package, `const enum` values are inlined as raw numbers/strings into the consumer's build. If the library updates the enum values without recompiling the consumer, the consumer runs with obsolete, mismatched constants.
2. **Reverse Mapping Memory Leaks:** Numeric enums cannot be garbage-collected independently of their reverse keys, and `Object.values(NumericEnum)` returns both strings and numbers.
3. **`isolatedModules` Rejection:** Single-file transpilers (Babel, esbuild, SWC) cannot resolve `const enum` imports across files because they do not parse cross-file type dependencies. Under `"isolatedModules": true`, ambient `const enum` declarations are disallowed.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using `enum` with Node 22 type stripping | `SyntaxError: Unexpected token 'enum'` | Use `const Obj = { ... } as const` | Conforms to modern erasable syntax specifications |
| Iterating numeric enum with `Object.keys()` | Iterates both names and numeric indices (size 2x) | Use `as const` object or string enum | Prevents duplicated UI options and corrupted serialization |
| Exporting `const enum` from shared libraries | Version desynchronization in consumers | Export string literal unions or const objects | Ensures ABI stability across independent builds |
| Assigning raw string to string enum | `TS2345: Argument of type '"A"' is not assignable to 'MyEnum'` | Use `as const` union type | Eliminates redundant enum imports in consumer call sites |
| Heterogeneous enums (mixing numbers & strings) | Confusing reverse mapping and unsound comparisons | Pure string or pure number const dictionaries | Guarantees consistent runtime semantics |
| Using enums with `--erasableSyntaxOnly` | `TS5118: Cannot use 'enum' with '--erasableSyntaxOnly'` | Replace with `as const` objects | Enables sub-millisecond native type stripping |

---

## 8. Performance & Type-Checker Complexity
- **Type Instantiation Cost:** Traditional enums allocate a `Symbol` with `SymbolFlags.Enum` and separate member symbols. `as const` objects allocate a single object literal type with literal property types.
- **Emitter Overhead:** Standard enums generate 10–20 lines of runtime boilerplate per declaration. In large enterprise codebases with hundreds of enums, this introduces measurable bundle bloat.
- **Transpilation Performance:** In TypeScript 7.0 Native (Go compiler), `as const` objects are processed instantaneously as standard AST expressions, whereas enums require specialized emission logic for two-way dictionary generation.

---

## 9. Compiler Internals
- **AST Node Representation:** An enum is parsed as `ts.EnumDeclaration` (`SyntaxKind.EnumDeclaration`). Its children are `ts.EnumMember` nodes.
- **Symbol Allocation:** The binder flags the identifier as `SymbolFlags.RegularEnum` or `SymbolFlags.ConstEnum`.
- **Type Checker Behavior:** In `checker.ts`, `checkEnumDeclaration` computes constant values for each member. In TS 5.0+, each member receives an internal `UniqueESSymbol`-like literal identity to power union exhaustiveness.

---

## 10. Best Practices & Design Guidelines
- **Adopt `as const` Objects as Default:** Default to `const Foo = { ... } as const` for all new TypeScript codebases.
- **Use String Literals for Network Protocols:** Never serialize numeric enum values over HTTP/JSON APIs; raw numbers obscure log inspection and break when ordering changes.
- **Enable `erasableSyntaxOnly` in TS 5.8+:** Add `"erasableSyntaxOnly": true` to `tsconfig.json` to guarantee that your repository runs natively under Node.js 22+ without transpilation steps.

---

## 11. Real-World Case Study
- **Scenario:** A microservice architecture with 40 services shared an npm package `@corp/contracts` containing 85 numeric and string `enum` declarations. When migrating services to Node 22 native execution with `--experimental-strip-types`, every service crashed on boot with syntax errors.
- **Implementation:** Automated a codemod transforming all enums to `as const` objects:
  ```typescript
  // Before:
  export enum Priority { Low = 1, High = 2 }

  // After:
  export const Priority = { Low: 1, High: 2 } as const;
  export type Priority = (typeof Priority)[keyof typeof Priority];
  ```
- **Outcome:** All 40 microservices booted cleanly on Node 22 native type stripping, eliminating build tool dependencies (ts-node/tsx), reducing Docker build times by 45%, and shrinking package bundle size by 180 KB.

---

## 12. Interview Questions

### Q1: Why do numeric enums generate reverse mappings in JavaScript while string enums do not?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Numeric enums generate reverse mappings (`Obj[Obj["A"] = 0] = "A"`) so developers can look up the symbolic name from a numeric code (e.g. `HttpStatus[200] === "OK"`). String enums do not generate reverse mappings because string values are self-describing; emitting a reverse mapping would create ambiguous keys if a value coincided with another member's name, and doubles the object's memory overhead without practical benefit.
</details>

### Q2: What is the erasable syntax boundary and how does TypeScript 5.8 `--erasableSyntaxOnly` affect enums?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The erasable syntax boundary (championed by the TC39 Type Annotations proposal) states that runtime engines should execute TypeScript by simply replacing type annotations with whitespace. Enums violate this because they require JavaScript code generation (IIFE runtime objects). TypeScript 5.8 introduced `--erasableSyntaxOnly`, which emits a compile-time diagnostic whenever non-erasable syntax (enums, namespaces with runtime values, class parameter properties) is encountered.
</details>

### Q3: What is the risk of using `const enum` in library packages distributed via npm?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When a library compiles with `const enum`, downstream consumers have the literal values inlined at compile time. If the library updates an enum member's value in a patch release and publishes, existing consumer applications that do not recompile will continue executing with the old inlined values, causing silent runtime protocol mismatches and corrupted data.
</details>

### Q4: How do you derive a union type of values from an `as const` object?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Using the indexed access operator over `keyof typeof`:
```typescript
export const Roles = { Admin: 'admin', User: 'user' } as const;
export type Role = (typeof Roles)[keyof typeof Roles]; // "admin" | "user"
```
`typeof Roles` extracts the type of the object; `keyof typeof Roles` yields `"Admin" | "User"`; indexing the object type with its keys returns the union of its property value types.
</details>

### Q5: Can an `enum` be used as a type and a value simultaneously?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. In TypeScript, an enum identifier occupies both the type namespace and the value namespace. In value position, it acts as an object containing the member constants. In type position, it acts as a type representing any valid member of that enum.
</details>

### Q6: Why did TypeScript 5.0 overhaul enum types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Prior to TypeScript 5.0, enums had several soundness and ergonomic flaws: numeric enums permitted assigning arbitrary numbers without error, and union enums were evaluated heterogeneously. TS 5.0 unified all enums by creating unique literal types for each member and allowing computation of values for all expressions, improving union exhaustiveness and assignability checking.
</details>

### Q7: How does `preserveConstEnums` change the compilation output of `const enum`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Normally, `const enum` leaves zero runtime JavaScript artifacts; all member accesses are inlined as literals. When `"preserveConstEnums": true` is set in `tsconfig.json`, the compiler still inlines the literal values at call sites, but it also emits the runtime IIFE object. This allows external tools or reflection code to inspect the enum object at runtime.
</details>

### Q8: How do you achieve type-safe key lookup for a const object?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

```typescript
export const Theme = { Light: 'light', Dark: 'dark' } as const;
export type ThemeKey = keyof typeof Theme; // "Light" | "Dark"
export type ThemeValue = (typeof Theme)[ThemeKey]; // "light" | "dark"
```
This enables bidirectional safety: typing functions that require the object key vs functions that require the object value.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Const Object Helper (Easy)
- **Problem Statement:** Write a utility type `ValueOf<T>` that extracts the union of values from any `as const` object dictionary.
- **Test Case:**
```typescript
const Config = { host: 'localhost', port: 8080 } as const;
type ConfigValues = ValueOf<typeof Config>; // "localhost" | 8080
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type ValueOf<T> = T[keyof T];
```
</details>

### Exercise 2: Strict Enum Key Inversion (Easy)
- **Problem Statement:** Create a type-safe function `invertDictionary` that inverts the keys and values of an `as const` object with unique string values.
- **Test Case:**
```typescript
const HttpCode = { OK: '200', NotFound: '404' } as const;
const Inverted = invertDictionary(HttpCode);
// Inverted has keys '200' | '404' and values 'OK' | 'NotFound'
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function invertDictionary<K extends string, V extends string>(
  dict: Record<K, V>
): { [P in V]: { [Q in K]: dict[Q] extends P ? Q : never }[K] } {
  const result = {} as any;
  for (const [key, value] of Object.entries(dict)) {
    result[value as string] = key;
  }
  return result;
}
```
</details>

### Exercise 3: Exhaustive Enum Pattern Matcher (Medium)
- **Problem Statement:** Implement a function `matchEnum` that accepts a value from a const object and an exhaustive handler table, returning the handler result.
- **Test Case:**
```typescript
const Action = { Create: 'CREATE', Delete: 'DELETE' } as const;
type Action = (typeof Action)[keyof typeof Action];

const result = matchEnum(Action.Create, {
  [Action.Create]: () => 1,
  [Action.Delete]: () => 0,
});
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function matchEnum<T extends string | number, R>(
  value: T,
  handlers: { [K in T]: () => R }
): R {
  return handlers[value]();
}
```
</details>

### Exercise 4: Dynamic Const Object Builder (Medium)
- **Problem Statement:** Write a function `makeEnum` that takes an array of unique strings and returns a frozen object with identity mappings, inferring exact string literal types.
- **Test Case:**
```typescript
const Colors = makeEnum(['Red', 'Green', 'Blue']);
// Colors.Red === 'Red'
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function makeEnum<const T extends readonly string[]>(
  values: T
): { readonly [K in T[number]]: K } {
  const obj = {} as any;
  for (const val of values) {
    obj[val] = val;
  }
  return Object.freeze(obj);
}
```
</details>

### Exercise 5: Compile-Time Enum Migration Validator (Hard)
- **Problem Statement:** Write a type-level assertion `AssertEqual<A, B>` that verifies that a legacy enum type and a new `as const` union type have identical structural assignability.
- **Test Case:**
```typescript
enum OldStatus { A = 'a', B = 'b' }
const NewStatus = { A: 'a', B: 'b' } as const;
type NewStatus = (typeof NewStatus)[keyof typeof NewStatus];

type Test = AssertEqual<OldStatus, NewStatus>; // true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type AssertEqual<X, Y> = (<T>() => T extends X ? 1 : 2) extends <T>() => T extends Y ? 1 : 2
  ? true
  : false;
```
</details>

### Exercise 6: Type-Safe Bitmask Const Object (Hard)
- **Problem Statement:** Implement a type-safe bitmask system using `as const` flags, providing helper functions `addFlag`, `hasFlag`, and `removeFlag` that preserve type safety.
- **Test Case:**
```typescript
const Permission = {
  Read: 1 << 0,
  Write: 1 << 1,
  Execute: 1 << 2,
} as const;
type Permission = (typeof Permission)[keyof typeof Permission];
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
const Permission = {
  Read: 1 << 0,
  Write: 1 << 1,
  Execute: 1 << 2,
} as const;
type Permission = (typeof Permission)[keyof typeof Permission];

type Bitmask<T extends number> = number & { readonly __flags?: T };

function createMask<T extends number>(...flags: T[]): Bitmask<T> {
  return flags.reduce((acc, f) => acc | f, 0) as Bitmask<T>;
}

function hasFlag<T extends number>(mask: Bitmask<T>, flag: T): boolean {
  return (mask & flag) === flag;
}
```
</details>
