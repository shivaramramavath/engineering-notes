# Type Aliases

> Type aliases create a new name for an existing type, facilitating structural modeling, union composition, tuple definition, and recursive data modeling without introducing runtime overhead or prototype mutations.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [03-objects.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/03-objects.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 1.4 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `exactOptionalPropertyTypes: true`, `noUncheckedIndexedAccess: true` |

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
- **Problem Statement:** In complex domain modeling, type signatures quickly grow unmanageable if repeated inline across functions, callbacks, and data contracts. Raw primitives and anonymous object shapes fail to convey domain semantics and make refactoring error-prone.
- **Historical Context:** Introduced in TypeScript 1.4 via the `type` keyword, type aliases initially served as simple aliases for union types and primitive names. Over subsequent releases, TypeScript expanded type aliases to support generics, mapped types, conditional types, and recursive definitions.
- **Real-World Relevance:** Type aliases allow software architects to declare rich domain models (discriminated unions, mapped types, branded primitives, recursive JSON trees) that disappear completely upon compilation, ensuring zero runtime cost and zero JavaScript bundle inflation.

---

## 2. Core Concepts
- **Formal Invariants:** A type alias creates an alternate identifier for an underlying type expression; it does not instantiate a new distinct type or nominal nominal wrapper. If two type aliases resolve to identical structural shapes, they are completely interchangeable.
- **Mental Model:** Think of a type alias as an immutable compile-time macro or variable pointing to a type expression. When the compiler evaluates a variable typed with an alias, it unfolds or resolves the alias into its underlying structural constituent during assignability checks.
- **Soundness vs Ergonomics:** Unlike `interface`, type aliases cannot be reopened via declaration merging. This immutability ensures predictability in application code: what you declare in a type alias cannot be silently mutated or augmented by a third-party module imported elsewhere.

---

## 3. How It Works Internally
- **Type Checker Traversal:** When parsing a `type` declaration, the parser constructs a `TypeAliasDeclaration` AST node. The binder registers a symbol with `SymbolFlags.TypeAlias`. During type checking, the checker invokes `getTypeFromTypeAliasDeclaration`, caching the resolved `Type` object.
- **Flow Control & Narrowing:** Control flow analysis operates on the underlying types of aliases. If an alias represents a discriminated union, narrowing on discriminant properties refines the alias to its constituent member.
- **Architectural Flow Diagram:**

```
+-------------------------------------------------------------------------+
|                  Type Alias Compilation & Resolution                     |
|                                                                         |
|  Source: type UserID = string;                                          |
|     │                                                                   |
|     ▼                                                                   |
|  [ Scanner / Parser ]                                                   |
|     │                                                                   |
|     ▼                                                                   |
|  AST: TypeAliasDeclaration (identifier: UserID, type: StringKeyword)   |
|     │                                                                   |
|     ▼                                                                   |
|  [ Binder ] ─── Registers Symbol(UserID) with SymbolFlags.TypeAlias     |
|     │                                                                   |
|     ▼                                                                   |
|  [ Checker ] ── Evaluates assignability via Structural Subtyping        |
|     │           Aliases expanded lazily on demand during unification   |
|     ▼                                                                   |
|  [ Emitter ] ── Erased completely! Zero runtime JS output produced      |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Formal Grammar:**
  ```typescript
  type Identifier<TypeParameters> = TypeExpression;
  ```
- **Supported RHS Expressions:**
  - Primitives: `type ID = string | number;`
  - Object literals: `type Point = { x: number; y: number };`
  - Tuples: `type Pair<T> = [T, T];`
  - Unions and Intersections: `type Result<T> = { ok: true; value: T } | { ok: false; error: Error };`
  - Conditional and Mapped Types: `type Nullable<T> = { [K in keyof T]: T[K] | null };`
  - Functions and Constructors: `type Predicate<T> = (value: T) => boolean;`
- **tsconfig Dependencies:** Operates under all standard configurations. Under `--isolatedDeclarations` (TS 5.5+), exported type aliases with complex inferred RHS types require explicit type annotations if referenced across project boundaries.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Model primitive aliases, unions, and function contracts.
- **Code:**
```typescript
type UUID = string;
type HttpStatus = 200 | 201 | 400 | 401 | 403 | 404 | 500;
type HttpMethod = 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH';

type RequestHandler = (
  url: string,
  method: HttpMethod,
  headers?: Record<string, string>
) => Promise<{ status: HttpStatus; payload: unknown }>;

const fetchUser: RequestHandler = async (url, method) => {
  return { status: 200, payload: { id: 'usr_123', name: 'Alice' } };
};
```
- **Output / Type Evaluation:**
```typescript
// Evaluates to:
// Promise<{ status: HttpStatus; payload: unknown }>
```
- **Explanation:** Aliases establish clear semantic meaning without runtime overhead.

### Example 2: Real-World Enterprise Pattern (Discriminated Union & Recursive Tree)
- **Goal:** Model an enterprise hierarchical organizational node and immutable AST tree.
- **Code:**
```typescript
type JSONPrimitive = string | number | boolean | null;
type JSONObject = { [key: string]: JSONValue };
type JSONArray = JSONValue[];
type JSONValue = JSONPrimitive | JSONObject | JSONArray;

type OrgNode =
  | { type: 'employee'; id: string; name: string; salary: number }
  | { type: 'team'; id: string; name: string; leadId: string; members: OrgNode[] }
  | { type: 'department'; id: string; name: string; budget: number; divisions: OrgNode[] };

function calculateTotalHeadcount(node: OrgNode): number {
  switch (node.type) {
    case 'employee':
      return 1;
    case 'team':
      return node.members.reduce((sum, member) => sum + calculateTotalHeadcount(member), 1);
    case 'department':
      return node.divisions.reduce((sum, div) => sum + calculateTotalHeadcount(div), 0);
  }
}
```
- **Output / Type Evaluation:**
```typescript
// calculateTotalHeadcount exhaustive check verifies all 3 variants.
```
- **Explanation:** Type aliases seamlessly define recursive data structures (like `JSONValue` and `OrgNode`) where interfaces require extra indirection.

### Example 3: Edge Case / Boundary Condition (Recursive Alias Expansion Limit)
- **Goal:** Demonstrate recursive type alias instantiation limits and lazy evaluation.
- **Code:**
```typescript
type LinkedList<T> = {
  value: T;
  next: LinkedList<T> | null;
};

const list: LinkedList<number> = {
  value: 1,
  next: {
    value: 2,
    next: {
      value: 3,
      next: null
    }
  }
};
```
- **Output / Type Evaluation:**
```typescript
// Fully type checked and verified without recursive instantiation error.
```
- **Explanation:** TypeScript 3.7+ supports recursive type aliases directly by deferring alias evaluation inside object property positions.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate declaration merging rejection and assignability mismatch.
- **Code:**
```typescript
type WindowState = { isOpen: boolean };

// @ts-expect-error TS2300: Duplicate identifier 'WindowState'.
type WindowState = { title: string };

const state: WindowState = { isOpen: true };

// @ts-expect-error TS2322: Type '{ isOpen: boolean; extra: number; }' is not assignable to type 'WindowState'.
const invalidState: WindowState = { isOpen: true, extra: 42 };
```
- **Real Compiler Error:**
```
error TS2300: Duplicate identifier 'WindowState'.
error TS2322: Type '{ isOpen: boolean; extra: number; }' is not assignable to type 'WindowState'.
  Object literal may only specify known properties, and 'extra' does not exist in type 'WindowState'.
```
- **Explanation:** Unlike interfaces, type aliases cannot declare the same identifier twice in the same scope, preventing accidental merging. Excess property checks prevent undeclared fields on fresh object literals.

### Example 5: Idiomatic Modern Usage (Branded Nominal Types via Aliases)
- **Goal:** Create zero-runtime-cost nominal branded types to prevent accidental parameter transposition.
- **Code:**
```typescript
declare const __brand: unique symbol;
type Brand<B> = { readonly [__brand]: B };

export type BrandType<T, B> = T & Brand<B>;

export type AccountId = BrandType<string, 'AccountId'>;
export type OrderId = BrandType<string, 'OrderId'>;

export function makeAccountId(id: string): AccountId {
  return id as AccountId;
}

export function makeOrderId(id: string): OrderId {
  return id as OrderId;
}

export function transferFunds(from: AccountId, to: AccountId, amount: number): void {
  // Transfer implementation
}

const userAccount = makeAccountId('acc_9921');
const currentOrder = makeOrderId('ord_5541');

// Valid:
transferFunds(userAccount, userAccount, 100);

// @ts-expect-error TS2345: Argument of type 'OrderId' is not assignable to parameter of type 'AccountId'.
transferFunds(currentOrder, userAccount, 100);
```
- **Output / Type Evaluation:**
```typescript
// Compile-time error TS2345 prevents transferring funds to/from an OrderId!
```
- **Explanation:** Intersection with a unique symbol brand provides nominal type safety over primitive string values without altering JavaScript runtime behavior.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Lack of Declaration Merging**: If an author of an npm library uses `type` for options objects, downstream consumers cannot augment the options using declaration merging. Use `interface` for public extension points.
2. **Circular References in Direct Positions**: Defining `type Bad = Bad[]` is legal, but `type Circular = Circular` causes `TS2456: Type alias 'Circular' circularly references itself`. Indirection through an object wrapper is mandatory for self-referential types.
3. **Display Names in Diagnostics**: Complex intersection or union type aliases may be expanded in hover tooltips rather than displaying their nominal alias name. Using mapped types or utility types like `Prettify<T> = { [K in keyof T]: T[K] } & {}` can force clean object display.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Re-declaring a type alias across files without modules | `TS2300: Duplicate identifier 'User'` | Use ES modules (`export`/`import`) | Prevents global scope pollution and collisions |
| Using `type` for open plugin APIs | `TS2717: Subsequent property declarations must have the same type` | Use `interface` for public extendable contracts | Enables downstream consumers to augment types cleanly |
| Passing raw unbranded strings to domain APIs | Logical runtime bugs (passing CustomerID where VendorID expected) | Branded type aliases (`T & Brand<'Tag'>`) | Eliminates parameter transposition errors at compile time |
| Recursive alias without object property wrapper | `TS2456: Type alias circularly references itself` | Wrap in an object property or array index | Allows the type checker to defer evaluation |
| Using type alias with type assertions everywhere | Runtime exceptions bypass type checker | Use type narrowing and schema validation | Guarantees runtime safety rather than falsifying types |
| Deeply nested intersection aliases | `TS2590: Expression produces a union type that is too complex` | Simplify with discriminated unions or interfaces | Prevents compiler exponential time complexity |

---

## 8. Performance & Type-Checker Complexity
- **Type Instantiation Depth:** Type aliases are evaluated lazily. When a type alias is referenced, the compiler creates a reference node and resolves it upon demand.
- **Interface vs Type Performance:** In historical versions, interfaces were marginally faster to check because the compiler cached their flat declared shape by name. In TypeScript 5.x+, simple object type aliases achieve parity. However, deeply chained intersections (`A & B & C & D`) require computing flat member sets, whereas `interface Sub extends A, B` caches a unified shape.
- **Isolated Declarations:** Under `--isolatedDeclarations` (TS 5.5+), exported type aliases must explicitly type-annotate their inputs if computing conditional types that cannot be trivially deduced by external build tools.

---

## 9. Compiler Internals
- **AST Node Representation:** Represented in TypeScript AST as `ts.TypeAliasDeclaration` (`SyntaxKind.TypeAliasDeclaration`).
- **Symbol Flags:** The symbol is tagged with `SymbolFlags.TypeAlias`. If it shares a name with a value (like a `const` or `function`), TypeScript permits value-type space overloading.
- **Checker Resolution:** The type checker resolves type aliases via `getTypeFromTypeAliasDeclaration` in `checker.ts`. If the declaration is generic, an anonymous generic type is constructed, and instantiations produce `TypeReference` objects.

---

## 10. Best Practices & Design Guidelines
- **Use Type Aliases for Unions, Primitives, and Tuples:** Whenever modeling unions, tuples, function signatures, or mapped/conditional types, always use `type`.
- **Use Interfaces for Open Object Shapes:** If an object contract is part of a library API or public SDK meant to be extended by third parties, use `interface`.
- **Prefer Discriminated Unions over Complex Inheritance:** Discriminated union type aliases make invalid domain states unrepresentable and work cleanly with pattern matching.

---

## 11. Real-World Case Study
- **Scenario:** An enterprise payment orchestration gateway processes multi-channel transactions (CreditCard, ACH, Crypto, ApplePay). Developers frequently passed invalid field combinations (e.g. `routingNumber` with `ApplePay`), causing production transaction rejection.
- **Implementation:** Replaced a loose monolithic type with a strict discriminated union type alias:
  ```typescript
  type PaymentMethod =
    | { kind: 'card'; cardNumber: string; expMonth: number; expYear: number; cvv: string }
    | { kind: 'ach'; routingNumber: string; accountNumber: string; accountType: 'checking' | 'savings' }
    | { kind: 'crypto'; walletAddress: string; network: 'ethereum' | 'solana' | 'bitcoin' }
    | { kind: 'apple_pay'; paymentToken: string };

  type PaymentTransaction = {
    transactionId: string;
    amountCents: number;
    currency: 'USD' | 'EUR' | 'GBP';
    method: PaymentMethod;
  };
  ```
- **Outcome:** Type checking guarantees that each payment kind only accepts its valid payload parameters. Build-time validation prevented 100% of invalid parameter combination errors before deployment.

---

## 12. Interview Questions

### Q1: What are the fundamental differences between `type` and `interface` in modern TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

1. **Declaration Merging:** Interfaces support declaration merging (multiple interfaces with the same name in the same scope merge their members). Type aliases produce `TS2300: Duplicate identifier`.
2. **Right-Hand Side Expressions:** Type aliases can model primitives, unions, intersections, tuples, mapped types, and conditional types (`type X = A | B`). Interfaces can only describe object shapes, function signatures, and class contracts.
3. **Extends vs Intersects:** Interfaces use `extends` for inheritance, which performs property conflict checking and caching. Type aliases combine shapes via intersection (`&`), which creates overloaded signatures or `never` for conflicting primitive properties.
4. **Implementation:** Both can be implemented by classes, but a class cannot implement a union type alias (`type U = A | B; class C implements U {}` is illegal).
</details>

### Q2: What happens when two incompatible primitive types are intersected in a type alias?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When two incompatible primitive types are intersected (for example, `type Impossible = string & number`), the compiler resolves the resulting type to `never`. Because no runtime JavaScript value can simultaneously be a string and a number, the domain of the type is the empty set.
</details>

### Q3: How does TypeScript 3.7+ handle recursive type aliases compared to earlier versions?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Prior to TypeScript 3.7, recursive type aliases required an interface indirection to avoid circular reference errors. TypeScript 3.7 introduced deferred type alias evaluation. When a type alias references itself inside an array, tuple, or object property, the compiler defers unfolding the type until members are accessed, enabling clean recursive declarations like `type JSONValue = string | number | boolean | null | JSONValue[] | { [k: string]: JSONValue }`.
</details>

### Q4: Can a class implement a type alias? What are the limitations?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A class can implement any type alias that statically resolves to an object shape or an intersection of object shapes. However, a class cannot implement:
1. Union type aliases (`type U = { a: string } | { b: number }`).
2. Type aliases that resolve to primitives (`type ID = string`).
3. Type aliases that contain call or construct signatures combined with object properties unless the class prototype matches.
</details>

### Q5: What is a branded type and how does a type alias implement it?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript uses structural subtyping: two types with the same shape are identical. A branded type (or nominal type) prevents accidental assignability between identical underlying representations (e.g. `UserId` and `OrderId` both being strings). A type alias implements this by intersecting a primitive type with an object containing a unique symbol brand:
```typescript
declare const __brand: unique symbol;
type UserId = string & { readonly [__brand]: 'UserId' };
```
Because the symbol cannot be fabricated by plain string literals, only explicit constructors or validation functions can produce a `UserId`.
</details>

### Q6: How does `Prettify<T>` work and why is it used with type aliases?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When composing multiple type aliases with intersections (`type A = B & C & D`), IDE tooltips often display `B & C & D` rather than the flattened object keys. The `Prettify` utility:
```typescript
type Prettify<T> = { [K in keyof T]: T[K] } & {};
```
forces the compiler to map over all keys, flattening the intersection into a single readable object representation in editor hover cards.
</details>

### Q7: How do type aliases interact with `--isolatedDeclarations` in TypeScript 5.5+?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Under `--isolatedDeclarations`, TypeScript requires that any exported entity must have an explicit type signature that downstream single-file transpilers (like `tsgo`, `esbuild`, `swc`, or `oxc`) can emit without cross-file type resolution. Exported type aliases that rely on complex inferred types from other files will trigger compilation errors unless made explicit or self-contained.
</details>

### Q8: What is the effect of intersection on conflicting property types in type aliases?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

If two types `A = { id: string }` and `B = { id: number }` are intersected (`type C = A & B`), the property `id` becomes `string & number`, which evaluates to `never`. The object type `C` can still be referenced, but no value can ever be assigned to `id`, effectively making the object uninstantiable. If the properties are functions, the intersection produces an overloaded function signature.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Recursive Deep Partial Type Alias (Easy)
- **Problem Statement:** Implement a generic type alias `DeepPartial<T>` that recursively makes all nested object properties optional, while preserving primitives, arrays, and functions.
- **Test Case:**
```typescript
type Config = {
  db: { host: string; port: number; auth: { user: string; pass: string } };
  endpoints: string[];
};

type Result = DeepPartial<Config>;
// db should be optional, auth should be optional, host/port/user/pass should be optional.
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type DeepPartial<T> = T extends Function | boolean | number | string | symbol | null | undefined
  ? T
  : T extends Array<infer U>
  ? _DeepPartialArray<U>
  : T extends object
  ? _DeepPartialObject<T>
  : T;

type _DeepPartialArray<T> = Array<DeepPartial<T>>;
type _DeepPartialObject<T> = { [P in keyof T]?: DeepPartial<T[P]> };
```
</details>

### Exercise 2: Strict Union Exclusivity (Easy)
- **Problem Statement:** Write a type alias `MutuallyExclusive<T, U>` that allows either shape `T` or shape `U`, but disallows an object possessing properties from both simultaneously.
- **Test Case:**
```typescript
type Person = { name: string };
type Company = { registeredNumber: number };

type Entity = MutuallyExclusive<Person, Company>;

const valid1: Entity = { name: 'Alice' }; // OK
const valid2: Entity = { registeredNumber: 12345 }; // OK
// const invalid: Entity = { name: 'Bob', registeredNumber: 67890 }; // Error!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type Without<T, U> = { [P in Exclude<keyof T, keyof U>]?: never };
type MutuallyExclusive<T, U> = (T | U) extends object
  ? (T & Without<U, T>) | (U & Without<T, U>)
  : T | U;
```
</details>

### Exercise 3: Type-Safe Event Map Alias (Medium)
- **Problem Statement:** Construct a generic type alias system for an event emitter where event payload contracts are strictly enforced per event name.
- **Test Case:**
```typescript
type AppEvents = {
  login: { userId: string; timestamp: number };
  logout: { userId: string };
  error: { code: number; message: string };
};

// Typed listener function signature
type EventListener<Events, K extends keyof Events> = (payload: Events[K]) => void;
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type TypedEmitter<Events> = {
  on<K extends keyof Events>(event: K, listener: (payload: Events[K]) => void): void;
  emit<K extends keyof Events>(event: K, payload: Events[K]): void;
};
```
</details>

### Exercise 4: Validated Flatten Keys Alias (Medium)
- **Problem Statement:** Create a type alias `Paths<T>` that computes dot-notated string literal union paths for all nested keys of an object up to depth 3.
- **Test Case:**
```typescript
type Model = {
  user: { profile: { bio: string }; age: number };
  active: boolean;
};

// Paths<Model> must evaluate to:
// "active" | "user" | "user.age" | "user.profile" | "user.profile.bio"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type Paths<T, Depth extends number[] = []> = Depth['length'] extends 3
  ? never
  : T extends object
  ? {
      [K in keyof T & string]: K | `${K}.${Paths<T[K], [...Depth, 1]>}`;
    }[keyof T & string]
  : never;
```
</details>

### Exercise 5: Compile-Time JSON Schema Validator Alias (Hard)
- **Problem Statement:** Write a conditional type alias `MatchesSchema<T, Schema>` that returns `true` if `T` conforms to a declarative type description, or returns a compile-time descriptive string error union.
- **Test Case:**
```typescript
type UserSchema = {
  id: 'number';
  name: 'string';
  tags: 'string[]';
};

type ValidPayload = { id: 1; name: 'Alice'; tags: ['admin'] };
type InvalidPayload = { id: 'bad'; name: 'Bob'; tags: ['user'] };
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type TypeMap = {
  number: number;
  string: string;
  boolean: boolean;
  'string[]': string[];
};

type MatchesSchema<T, S extends Record<string, keyof TypeMap>> = {
  [K in keyof S]: K extends keyof T
    ? T[K] extends TypeMap[S[K]]
      ? true
      : `Error: Property ${K & string} expected ${S[K]}`
    : `Error: Missing property ${K & string}`;
}[keyof S] extends true
  ? true
  : false;
```
</details>

### Exercise 6: Curried Function Signature Generator (Hard)
- **Problem Statement:** Author a type alias `Curried<F>` that transforms any function taking a tuple of parameters into its strictly curried form.
- **Test Case:**
```typescript
type Fn = (a: string, b: number, c: boolean) => Date;
type CurriedFn = Curried<Fn>;
// (a: string) => (b: number) => (c: boolean) => Date
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type Curried<F> = F extends (...args: infer Args) => infer Return
  ? Args extends [infer First, ...infer Rest]
    ? Rest extends []
      ? (arg: First) => Return
      : (arg: First) => Curried<(...args: Rest) => Return>
    : () => Return
  : never;
```
</details>
