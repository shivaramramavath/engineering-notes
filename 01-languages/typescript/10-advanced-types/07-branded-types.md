# Branded and Nominal Types

> TypeScript uses structural typing by default. Branded types introduce nominal typing techniques to enforce domain safety between structurally identical primitives like UserID, OrderID, USD, and EUR.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [01-primitive-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/01-primitive-types.md), [04-type-aliases.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/04-type-aliases.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.0 / TS 5.x / TS 7.0 Native |
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
TypeScript's type system is **structural**: if two types have the same shape, they are interchangeable:
```typescript
type UserId = string;
type OrderId = string;

let u: UserId = "user_123";
let o: OrderId = u; // Compiles without error!
```
In critical domain applications, this structural equivalence causes catastrophic bugs:
- Passing a `UserId` into a function expecting an `OrderId`.
- Adding `EUR` currency amounts to `USD` amounts.
- Passing an unvalidated, unsanitized HTML string into an innerHTML sink.

**Branded Types** (also known as Nominal Types or Tagged Types) intersect a base primitive with a unique compile-time phantom marker, forcing the compiler to treat structurally identical primitives as strictly distinct types.

---

## 2. Core Concepts

### The Phantom Brand Pattern
A unique symbol or string tag is intersected with the base primitive:
```typescript
declare const __brand: unique symbol;

export type Brand<T, TBrand extends string> = T & {
  readonly [__brand]: TBrand;
};

export type UserId = Brand<string, "UserId">;
export type OrderId = Brand<string, "OrderId">;

let userId = "u_101" as UserId;
let orderId = "ord_555" as OrderId;

// @ts-expect-error TS2322: Type 'OrderId' is not assignable to type 'UserId'.
userId = orderId;
```

### Zero Runtime Overhead
The brand property `[__brand]` exists **only in type space**. At runtime, a branded `UserId` is just a standard JavaScript string with zero wrapper objects, memory allocations, or prototype overhead!

---

## 3. How It Works Internally

```
Type Structure:
UserId  = string & { readonly [__brand]: "UserId" }
OrderId = string & { readonly [__brand]: "OrderId" }

Subtyping Check: Can OrderId be assigned to UserId?
1. Does string extend string? ===> YES
2. Does { [__brand]: "OrderId" } extend { [__brand]: "UserId" }?
   ===> Property '[__brand]' has type '"OrderId"' which is NOT assignable to '"UserId"'!
3. Assignment REJECTED (TS2322)!
```

During assignability checks:
1. The compiler verifies subtyping across all members of the intersection.
2. The phantom property tag creates an irreconcilable structural mismatch between different brands.
3. Because the tag property is non-existent at runtime, values are created using validation constructor functions with safe type assertions.

---

## 4. Syntax & API Reference

### Smart Constructors and Type Guards
```typescript
export function makeUserId(raw: string): UserId {
  if (!raw.startsWith("usr_")) {
    throw new Error(`Invalid UserId format: ${raw}`);
  }
  return raw as UserId;
}

export function isUserId(raw: string): raw is UserId {
  return raw.startsWith("usr_");
}
```

---

## 5. Practical Examples

### 5.1 Currency Type Safety
```typescript
export type USD = Brand<number, "USD">;
export type EUR = Brand<number, "EUR">;

function addUSD(a: USD, b: USD): USD {
  return ((a as number) + (b as number)) as USD;
}

const priceUSD = 100 as USD;
const feeEUR = 20 as EUR;

// @ts-expect-error TS2345: Argument of type 'EUR' is not assignable to parameter of type 'USD'.
addUSD(priceUSD, feeEUR);
```

### 5.2 Real-World Application: Sanitized HTML vs Raw Input
```typescript
export type SanitizedHtml = Brand<string, "SanitizedHtml">;

function sanitizeInput(dirty: string): SanitizedHtml {
  const clean = dirty.replace(/</g, "&lt;").replace(/>/g, "&gt;");
  return clean as SanitizedHtml;
}

function renderHtml(html: SanitizedHtml): void {
  // Safe from XSS: Only sanitized HTML can be passed here!
  console.log("Rendering HTML safely:", html);
}

const userInput = "<script>alert(1)</script>";
// renderHtml(userInput); // Compile error!
renderHtml(sanitizeInput(userInput)); // OK
```

### 5.3 Edge Case: Branded Numbers and Arithmetic
```typescript
const a = 10 as USD;
const b = a + 5; // Inferred as 'number'! Built-in JS math operations strip branding.
// To retain branding, wrap arithmetic in helper functions.
```

### 5.4 Failure Case: Direct String Assignment to Brand
```typescript
// @ts-expect-error TS2322: Type 'string' is not assignable to type 'UserId'.
const id: UserId = "usr_123"; // Must use smart constructor or assertion!
```

### 5.5 Idiomatic Pattern: Flavored Types (Soft Nominality)
If you want autocompletion to allow raw strings but still highlight the brand in tooltips, make the brand property optional:
```typescript
export type Flavor<T, TFlavor extends string> = T & {
  readonly _flavor?: TFlavor;
};

type SoftUserId = Flavor<string, "UserId">;
const u: SoftUserId = "usr_100"; // Allowed without assertion!
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Arithmetic Stripping**: Arithmetic operators (`+`, `-`, `*`, `/`) return primitive `number`, shedding the brand tag.
2. **JSON Serialization**: `JSON.stringify(brand)` emits the bare primitive. `JSON.parse()` yields an unbranded primitive; always re-validate.
3. **Unique Symbol Scoping**: Use a shared unique symbol (`declare const __brand: unique symbol`) across the project so brands don't conflict across declaration files.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Direct assignment without validation | Defeats the safety of branded types | Use smart constructor or type guard | `TS2322: Type 'string' is not assignable...` |
| Using string brand property name directly | Can clash with real object properties | Use `unique symbol` for brand key | Property name clash |
| Expecting arithmetic to preserve brand | JavaScript arithmetic returns base `number` | Wrap math operations in typed helpers | Brand dropped after math |
| Blindly casting arbitrary strings with `as Brand` | Bypasses runtime format validation | Validate with regex/predicate before cast | Silent runtime format bugs |
| Serializing brand key to database | Brand tag is a compile-time phantom | Write bare primitive to DB | Unnecessary complexity |
| Creating classes for primitives | High memory overhead, GC pressure | Use zero-cost type brands | 10x slower execution |

---

## 8. Performance & Type-Checker Complexity
- Branded types have **zero runtime overhead** (100% erased during emit).
- Checking branded assignability is $O(1)$ in the compiler symbol table.

---

## 9. Compiler Internals
- An intersection type `TypeFlags.Intersection` checks assignability of each constituent member.
- The phantom property symbol is evaluated like any other property on `AnonymousType`. Because the phantom symbol has no implementation, it only participates in static subtyping checks.

---

## 10. Best Practices & Design Guidelines
- Create a shared `Brand<T, Tag>` utility in your core typing library.
- Pair every branded type with a **Smart Constructor** that validates runtime data before returning the branded type.
- Use branded types for database IDs, monetary amounts, validated emails, and sanitized strings.

---

## 11. Real-World Case Study
### Preventing Multi-Million Dollar Currency Discrepancies in Trading Platform
A high-frequency trading platform handled transactions across multiple international currencies (USD, JPY, EUR). A junior developer accidentally executed:
```typescript
const balance = getAccountBalance(acc) + pendingFee; // where balance was JPY and fee was USD!
```
**Solution:**
Branded all currency types: `type JPY = Brand<number, "JPY">`, `type USD = Brand<number, "USD">`.
All currency conversion functions required explicit rate multipliers:
```typescript
function usdToJpy(amount: USD, rate: JpyPerUsd): JPY {
  return ((amount as number) * (rate as number)) as JPY;
}
```
The compiler caught 4 cross-currency assignment bugs across legacy code, completely preventing trading calculation errors.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between structural typing and nominal typing?</summary>

In structural typing (TypeScript's default), type compatibility is determined solely by the shape and members of the types. In nominal typing (Java, C++, Rust), type compatibility is determined by explicit declaration names and identity, even if two types have identical internal structures.
</details>

<details>
<summary>2. How do Branded Types achieve nominal typing in TypeScript?</summary>

By intersecting a primitive type (such as `string` or `number`) with an object containing a phantom property tagged with a unique symbol or literal type (`T & { readonly [__brand]: "Tag" }`). This forces the structural checker to require the unique brand tag during assignability checks.
</details>

<details>
<summary>3. Is there any runtime performance cost to using branded types?</summary>

No. Branded types are purely compile-time constructs. The brand property is never instantiated at runtime. Emitted JavaScript treats branded types as regular primitives.
</details>

<details>
<summary>4. What is a "Smart Constructor" in the context of branded types?</summary>

A smart constructor is a runtime validation function that inspects an unbranded primitive, validates that it satisfies domain business rules (e.g. valid UUID format or positive number), and returns the branded type using a safe type assertion.
</details>

<details>
<summary>5. What happens when you perform arithmetic on a branded number?</summary>

JavaScript arithmetic operators (`+`, `-`, `*`, `/`) evaluate to the primitive `number` type, which automatically strips the brand tag.
</details>

<details>
<summary>6. What is a "Flavored" type in TypeScript?</summary>

A flavored type makes the brand tag property optional (`_flavor?: "Tag"`). This allows raw primitives to be assigned to the flavored type without explicit casting, while still displaying the tag name in IDE tooltips.
</details>

<details>
<summary>7. Why should <code>unique symbol</code> be used for the brand property?</summary>

Using a `unique symbol` guarantees that the brand tag cannot conflict with any real property name on existing objects or other libraries.
</details>

<details>
<summary>8. How do you unbrand a branded type to get the underlying primitive?</summary>

Using a conditional type or simple cast:
```typescript
type Unbrand<T> = T extends Brand<infer U, any> ? U : T;
```
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Create a PositiveNumber Brand (Easy)
Define a branded type `PositiveNumber` and a smart constructor.
<details>
<summary>Solution</summary>

```typescript
declare const brand: unique symbol;
type PositiveNumber = number & { readonly [brand]: "PositiveNumber" };

function makePositive(n: number): PositiveNumber {
  if (n <= 0) throw new Error("Must be positive");
  return n as PositiveNumber;
}
```
</details>

### Exercise 2: Validated Email Brand (Easy)
Create a `ValidEmail` branded string type with a type guard.
<details>
<summary>Solution</summary>

```typescript
declare const emailBrand: unique symbol;
type ValidEmail = string & { readonly [emailBrand]: "ValidEmail" };

function isValidEmail(s: string): s is ValidEmail {
  return /^[^s@]+@[^s@]+.[^s@]+$/.test(s);
}
```
</details>

### Exercise 3: Generic Brand Utility (Medium)
Implement a reusable `Brand<Base, Tag>` utility type and demonstrate it with two distinct ID types.
<details>
<summary>Solution</summary>

```typescript
declare const __brand: unique symbol;
type Brand<T, Tag extends string> = T & { readonly [__brand]: Tag };

type AccountId = Brand<string, "AccountId">;
type PostId = Brand<string, "PostId">;

let a = "acc_1" as AccountId;
let p = "post_1" as PostId;
// @ts-expect-error TS2322
a = p;
```
</details>

### Exercise 4: Type-Safe Currency Addition (Medium)
Implement a strongly typed money calculator that prevents adding different currencies.
<details>
<summary>Solution</summary>

```typescript
type Money<Currency extends string> = Brand<number, Currency>;

function addMoney<C extends string>(a: Money<C>, b: Money<C>): Money<C> {
  return ((a as number) + (b as number)) as Money<C>;
}

const d1 = 10 as Money<"USD">;
const d2 = 20 as Money<"USD">;
const e1 = 15 as Money<"EUR">;

addMoney(d1, d2); // OK: Returns Money<"USD">
// @ts-expect-error TS2345
addMoney(d1, e1); // Error: Currency mismatch!
```
</details>

### Exercise 5: Unbrand Utility Type (Hard)
Write an `Unbrand<T>` utility that extracts the base primitive from any branded type.
<details>
<summary>Solution</summary>

```typescript
type Unbrand<T> = T extends infer U & { readonly [__brand]: any } ? U : T;

type RawNum = Unbrand<Money<"USD">>; // number
```
</details>

### Exercise 6: Multi-Brand Tagging (Hard)
Implement a system where a type can carry multiple brands (e.g. `ValidEmail & VerifiedEmail`).
<details>
<summary>Solution</summary>

```typescript
type AddBrand<T, NewBrand extends string> = T & { readonly [__brand]: { [K in NewBrand]: true } };

type BasicEmail = AddBrand<string, "ValidFormat">;
type FullyVerifiedEmail = AddBrand<BasicEmail, "MxChecked">;

function sendVerification(email: BasicEmail): FullyVerifiedEmail {
  // perform verification
  return email as FullyVerifiedEmail;
}
```
</details>
