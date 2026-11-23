# Primitive Types & Literal Types in TypeScript

> Comprehensive technical guide to TypeScript's 7 primitive types, literal types, wrapper objects vs primitive types, and template literal type inference.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///e:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0+ / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true`, `strictNullChecks: true` |

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

### Problem Statement
In JavaScript, primitives (`string`, `number`, `boolean`, `bigint`, `symbol`, `null`, `undefined`) represent immutable scalar values. Without static typing, functions accepting a `"status"` often receive unexpected numbers, `null`, or arbitrary objects, leading to runtime exceptions. TypeScript models these primitives precisely and introduces **Literal Types**—allowing types that represent exact individual values (e.g. `'active'` or `200`).

### Historical Context
JavaScript auto-boxes primitives into object wrappers (`new String()`, `new Number()`) when accessing methods like `.toUpperCase()`. Early TypeScript developers frequently confused the wrapper object types (`String`, `Number`, `Boolean`) with lowercase primitive types (`string`, `number`, `boolean`), leading to type safety loopholes.

### Real-World Relevance
Primitives and literal unions form the backbone of domain modeling, type-safe API contracts, database entity definitions, and state machine transitions.

---

## 2. Core Concepts

### The 7 Primitive Types in TypeScript

| Primitive | Runtime `typeof` | TypeScript Type | Example Value |
| :--- | :--- | :--- | :--- |
| **String** | `'string'` | `string` | `"hello"`, `'world'`, \`data\` |
| **Number** | `'number'` | `number` | `42`, `3.14`, `NaN`, `Infinity` |
| **Boolean** | `'boolean'` | `boolean` | `true`, `false` |
| **BigInt** | `'bigint'` | `bigint` | `100n`, `9007199254740992n` |
| **Symbol** | `'symbol'` | `symbol` / `unique symbol` | `Symbol('key')` |
| **Undefined**| `'undefined'`| `undefined` | `undefined` |
| **Null** | `'object'` (legacy)| `null` | `null` |

### Primitives vs Wrapper Objects
Never use uppercase `String`, `Number`, `Boolean`, or `Symbol` as type annotations:
- `string`: Primitive scalar value.
- `String`: Object wrapper instance (`new String()`). Primitives are assignable to `String`, but `String` objects are **not** assignable to primitive `string`!

```typescript
const prim: string = 'hello'; // Correct
const wrap: String = new String('hello'); // Anti-pattern!
```

---

## 3. How It Works Internally

### V8 Smi Representation and Tagged Pointers
In V8 and modern engines, small integers (`-2^30` to `2^30 - 1`) are represented as Small Integers (`Smi`), stored directly in CPU registers without heap allocation. Floating point numbers and large strings are allocated on the V8 managed heap. TypeScript's `number` type encompasses both integer Smis and IEEE 754 64-bit floats.

---

## 4. Syntax & API Reference

### Literal Types & Unions

```typescript
// String Literal Union
type HttpMethod = 'GET' | 'POST' | 'PUT' | 'DELETE';

// Numeric Literal Union
type HttpStatus = 200 | 201 | 400 | 401 | 404 | 500;

// Boolean Literal Type
type TrueOnly = true;

// Template Literal Types (Since TS 4.1)
type EventName = `on${Capitalize<string>}`;
```

---

## 5. Practical Examples

### Example 1: Literal Union for State Machine Status
- **Goal:** Model type-safe payment transaction statuses.
- **Code:**
```typescript
type TransactionStatus = 'pending' | 'settled' | 'failed' | 'refunded';

interface Transaction {
  readonly id: string;
  readonly amountCents: number;
  status: TransactionStatus;
}

function processTransaction(tx: Transaction): string {
  switch (tx.status) {
    case 'pending': return 'Hold placed on funds';
    case 'settled': return 'Funds deposited';
    case 'failed': return 'Transaction declined';
    case 'refunded': return 'Funds returned to cardholder';
  }
}

const tx: Transaction = { id: 'tx_01', amountCents: 5000, status: 'settled' };
console.log(processTransaction(tx));
```
- **Output:**
```
Funds deposited
```

### Example 2: BigInt Precision Tracking
- **Goal:** Use `bigint` for arbitrary precision currency or blockchain calculations.
- **Code:**
```typescript
// BigInt literals require "target": "ES2020" or newer
const satoshis: bigint = 2100000000000000n;
const fee: bigint = 1500n;

const total: bigint = satoshis + fee;
console.log('Total satoshis:', total.toString());
```
- **Output:**
```
Total satoshis: 2100000000001500
```

### Example 3: Template Literal Type Validation
- **Goal:** Validate API endpoints at compile time using template literal strings.
- **Code:**
```typescript
type ApiVersion = 'v1' | 'v2';
type Endpoint = `/${ApiVersion}/${string}`;

function fetchEndpoint(endpoint: Endpoint): void {
  console.log(`Connecting to ${endpoint}`);
}

fetchEndpoint('/v1/users'); // OK
fetchEndpoint('/v2/payments/charge'); // OK
```

### Example 4: Negative Test — Wrapper Object Assignability
- **Goal:** Demonstrate error when passing `String` wrapper object to primitive `string`.
- **Code:**
```typescript
function printName(name: string) {
  console.log(name);
}

const wrapped = new String('Alice');

// @ts-expect-error TS2345: Argument of type 'String' is not assignable to parameter of type 'string'.
printName(wrapped);
```
- **Real Compiler Error:**
```
error TS2345: Argument of type 'String' is not assignable to parameter of type 'string'.
  'string' is a primitive, but 'String' is a wrapper object. Prefer using 'string' when possible.
```

### Example 5: Template Literal String Transformations
- **Goal:** Create typed event listener mappings using intrinsic string manipulation types.
- **Code:**
```typescript
type Event = 'click' | 'hover' | 'focus';
type HandlerName = `on${Capitalize<Event>}`; // "onClick" | "onHover" | "onFocus"

const handlers: Record<HandlerName, () => void> = {
  onClick: () => console.log('clicked'),
  onHover: () => console.log('hovered'),
  onFocus: () => console.log('focused')
};

handlers.onClick();
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`NaN` and `Infinity` are `number`**: In TypeScript and JavaScript, `typeof NaN === 'number'`. A variable typed as `number` can hold `NaN` or `-Infinity`, which breaks mathematical assumptions.
2. **BigInt & Number Mixing**: You cannot perform mathematical operations between `bigint` and `number` without explicit casting (`10n + 5` produces `TypeError`).
3. **Template Literal Explosion**: Generating unions of template literals across multiple large unions (`type Hex = #${Color}${Color}${Color}`) can generate millions of permutations, crashing the TypeScript compiler with out-of-memory errors.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Real Compiler Issue | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using `String`, `Number`, `Boolean` types | TS2345 wrapper object incompatibility | Always use lowercase: `string`, `number`, `boolean` | Avoids confusing wrapper objects with primitives |
| Assuming `number` excludes `NaN` | Runtime mathematical faults | Validate with `Number.isFinite(n)` | `NaN` satisfies type `number` silently |
| Mixing `number` and `bigint` in math | `TS2365: Operator '+' cannot be applied to types 'number' and 'bigint'` | Cast explicitly: `BigInt(n)` or `Number(b)` | JavaScript engine forbids implicit mixing |
| Using plain `string` for enum-like states | Zero autocomplete, allows typos (`"pedning"`) | Use string literal union: `'pending' \| 'active'` | Eliminates invalid states at compile time |

---

## 8. Performance & Type-Checker Complexity

- **Literal Unions vs String**: Checking assignability of a string literal union with $< 50$ members is $O(1)$ constant time via internal hash sets in the TypeScript Checker.
- **Template Literal Unions**: An expression like \`${A}_${B}\` where $A$ has 50 elements and $B$ has 50 elements constructs a Cartesian product of $2,500$ types, increasing `tsc` memory usage.

---

## 9. Compiler Internals

Within `src/compiler/types.ts`:
- Primitives are represented by `TypeFlags`:
  - `TypeFlags.String = 1 << 2`
  - `TypeFlags.Number = 1 << 3`
  - `TypeFlags.Boolean = 1 << 4`
- Literal types hold `TypeFlags.StringLiteral` or `TypeFlags.NumberLiteral` and store the raw value in `(type as LiteralType).value`.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Lowercase Primitive Names**: `string`, `number`, `boolean`, `symbol`, `bigint`.
2. **Constrain Strings to Literal Unions**: Avoid generic `string` for finite sets of options.
3. **Use Branded Primitives for Domain Safety**: Distinguish between `USD` and `EUR` currencies using nominal branding.

---

## 11. Real-World Case Study

A healthcare records application stored temperature values in Celsius. A developer accidentally passed temperatures in Fahrenheit, causing medication calculation errors because both were typed simply as `number`.

### Solution:
Introduced branded primitive numbers:
```typescript
type Celsius = number & { readonly __unit: 'Celsius' };
type Fahrenheit = number & { readonly __unit: 'Fahrenheit' };

function toCelsius(f: Fahrenheit): Celsius {
  return (((f - 32) * 5) / 9) as Celsius;
}
```
**Outcome**: Attempting to pass raw numbers or Fahrenheit to Celsius calculation functions produced compile-time errors.

---

## 12. Interview Questions

### Q1: Why does TypeScript have both `string` and `String`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`string` is the primitive type representing raw string values. `String` is the JavaScript wrapper object interface (`new String('foo')`). While JavaScript automatically wraps primitives to access methods like `.trim()`, the types are distinct. Primitives are assignable to wrapper objects, but wrapper objects cannot be assigned to primitive types. Using uppercase `String` as a type annotation is an anti-pattern.
</details>

### Q2: What is the difference between `null` and `undefined` in TypeScript under `strictNullChecks`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Under `"strictNullChecks": true`, `null` and `undefined` are distinct concrete types that cannot be assigned to each other, nor to any other primitive (like `string` or `number`). `undefined` represents the uninitialized or absent state (e.g. unprovided function arguments or unassigned object properties), while `null` represents the intentional absence of an object value.
</details>

### Q3: How do template literal types work in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Introduced in TypeScript 4.1, template literal types have the same syntax as ECMAScript template literal strings, but are used in type positions: \`prefix_${T}\`. When paired with union types, they expand into Cartesian products of string literals and support string transformation utility types (`Uppercase`, `Lowercase`, `Capitalize`, `Uncapitalize`).
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): String Literal Union
- **Problem Statement**: Define a type `ThemeMode` that only accepts `'light'`, `'dark'`, or `'system'`.
- **Solution**:
```typescript
export type ThemeMode = 'light' | 'dark' | 'system';
```

### Exercise 2 (Medium): Template Literal HTTP Route
- **Problem Statement**: Create a type `Route` that must start with `/api/` followed by any string.
- **Solution**:
```typescript
export type Route = `/api/${string}`;
```

### Exercise 3 (Hard): Branded Currency Primitive
- **Problem Statement**: Implement nominal typing for `Cents` such that integers cannot be passed to a checkout function without going through a constructor validator.
- **Solution**:
```typescript
export type Cents = number & { readonly __brand: unique symbol };

export function parseCents(amount: number): Cents {
  if (!Number.isInteger(amount) || amount < 0) {
    throw new Error('Cents must be a positive integer.');
  }
  return amount as Cents;
}
```
