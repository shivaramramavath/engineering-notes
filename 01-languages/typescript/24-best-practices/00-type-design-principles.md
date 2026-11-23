# Type Design Principles in TypeScript: Parse Don't Validate and Illegal States

> Foundational architectural principles for designing robust TypeScript type systems: "Parse, Don't Validate", making illegal states unrepresentable, nominal branding, and single source of truth.

| Metadata | Details |
| :--- | :--- |
| **Category** | Best Practices |
| **Difficulty** | Advanced |
| **Prerequisites** | [01-fundamentals/06-type-widening-narrowing.md](../01-fundamentals/06-type-widening-narrowing.md), [10-advanced-types/01-nominal-typing.md](../10-advanced-types/01-nominal-typing.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

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

In poorly typed codebases, developers write defensive code everywhere: checking if values are nullish, parsing strings into integers repeatedly, and checking boolean flags across dozens of functions. This pattern—known as **Shotgun Validation**—scatters validation logic across the codebase, introduces silent bugs, and makes refactoring perilous.

Two foundational software design principles solve this problem in TypeScript:
1. **Parse, Don't Validate**: Instead of validating a piece of data and returning a boolean (`isValid(data): boolean`), parse the raw input into a richer, more specific type (`parse(data): ValidatedData`). Once parsed, downstream functions never need to check or validate that data again.
2. **Make Illegal States Unrepresentable**: Design types such that invalid domain configurations (e.g., an order marked "shipped" with no tracking number, or a user marked "unauthenticated" holding an auth token) are rejected by the TypeScript compiler at build time.

---

## 2. Core Concepts

### Validation vs. Parsing
- **Validation**: Checks input against rules and yields a boolean or list of errors. The type of the value remains unchanged (`string`). Downstream functions cannot tell whether the string was validated or not without re-checking.
- **Parsing**: Checks input against rules and, upon success, yields a new, refined type (`EmailAddress` or `NonEmptyArray<T>`). The type system proves that the validation succeeded.

```
+-------------------------------------------------------------------------+
|                    Validation vs. Parsing Architecture                  |
+-------------------------------------------------------------------------+
|                                                                         |
|  [ Approach 1: Validation ]                                             |
|  Raw Input: "test@domain.com"                                           |
|       |                                                                 |
|       v isValidEmail(str) === true                                      |
|  Output: string (Type system learned NOTHING!)                          |
|  Result: sendEmail(to: string) must re-validate or hope for the best.   |
|                                                                         |
|  [ Approach 2: Parsing (Parse, Don't Validate) ]                        |
|  Raw Input: "test@domain.com" (unknown)                                 |
|       |                                                                 |
|       v parseEmail(raw)                                                 |
|  Output: ValidatedEmail (Branded Type)                                  |
|  Result: sendEmail(to: ValidatedEmail) is 100% compile-time guaranteed! |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Making Illegal States Unrepresentable
Avoid combining independent optional fields (`isLoading: boolean; data?: T; error?: Error;`). Instead, use a single **discriminated union** where state cannot be in conflict.

---

## 3. How It Works Internally

1. **Branded Types (Nominal Simulation)**: Because TypeScript's type system is structural, `type UserId = string` is identical to `string`. By attaching a unique phantom symbol property (`declare const __brand: unique symbol`), the compiler treats `UserId` as structurally distinct from plain strings.
2. **Control Flow Analysis (CFA)**: The compiler tracks assignments and type predicates. When a value is parsed via a type guard or Zod schema, CFA narrows the type in the remainder of the block.

---

## 4. Syntax & API Reference

### Nominal Branding Helper
```typescript
declare const __brand: unique symbol;

export type Brand<T, TBrand extends string> = T & {
  readonly [__brand]: TBrand;
};

export type EmailAddress = Brand<string, "EmailAddress">;
export type UserId = Brand<string, "UserId">;
```

### Non-Empty Array Pattern
```typescript
export type NonEmptyArray<T> = [T, ...T[]];

export function head<T>(arr: NonEmptyArray<T>): T {
  return arr[0]; // Guaranteed never undefined!
}
```

---

## 5. Practical Examples

### Example 1: Eliminating Illegal States in Remote Async Data
```typescript
// BAD: 8 possible states, 4 of which are illegal (e.g. loading=true AND error=Error)
interface BadAsyncState<T> {
  isLoading: boolean;
  data?: T;
  error?: Error;
}

// GOOD: Discriminated union enforces exactly one valid state at a time
export type AsyncState<T> =
  | { readonly status: "IDLE" }
  | { readonly status: "LOADING" }
  | { readonly status: "SUCCESS"; readonly data: T }
  | { readonly status: "ERROR"; readonly error: Error };

export function renderUI<T>(state: AsyncState<T>): string {
  switch (state.status) {
    case "IDLE": return "Press start";
    case "LOADING": return "Loading...";
    case "SUCCESS": return `Loaded data: ${JSON.stringify(state.data)}`;
    case "ERROR": return `Failed: ${state.error.message}`;
  }
}
```

### Example 2: Parsing Branded Email Addresses
```typescript
import { Brand } from "./branding.js";

export type EmailAddress = Brand<string, "EmailAddress">;

export function parseEmail(raw: string): EmailAddress {
  const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
  if (!emailRegex.test(raw)) {
    throw new Error(`Invalid email address format: ${raw}`);
  }
  return raw as EmailAddress;
}

export function sendWelcomeEmail(to: EmailAddress): void {
  console.log(`Sending email to verified recipient: ${to}`);
}
```

### Example 3: Edge Case - Exhaustive State Transition Guards
```typescript
type OrderState =
  | { status: "DRAFT" }
  | { status: "PAID"; transactionId: string }
  | { status: "SHIPPED"; transactionId: string; trackingNumber: string };

// Illegal state prevented: An order cannot be "SHIPPED" without a trackingNumber!
export function shipOrder(
  order: Extract<OrderState, { status: "PAID" }>,
  trackingNumber: string
): Extract<OrderState, { status: "SHIPPED" }> {
  return {
    status: "SHIPPED",
    transactionId: order.transactionId,
    trackingNumber,
  };
}
```

### Example 4: Compiler Error & Raw String Rejection
```typescript
const rawUserInput = "alice@example.com";

// @ts-expect-error - TS2345: Argument of type 'string' is not assignable to parameter of type 'EmailAddress'.
sendWelcomeEmail(rawUserInput);

// Correct approach: Parse first!
const verified = parseEmail(rawUserInput);
sendWelcomeEmail(verified); // Compiles cleanly
```

### Example 5: High-Performance Invariant-Enforcing Result Type
```typescript
export type Result<T, E = Error> =
  | { readonly ok: true; readonly value: T }
  | { readonly ok: false; readonly error: E };

export function Ok<T>(value: T): Result<T, never> {
  return { ok: true, value };
}

export function Err<E>(error: E): Result<never, E> {
  return { ok: false, error };
}

export function divide(a: number, b: number): Result<number, string> {
  if (b === 0) return Err("Division by zero");
  return Ok(a / b);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Branding Primitives vs. Serialization**:
   Branded types exist purely in the compiler. Calling `JSON.stringify(email)` serializes the raw underlying string, and `JSON.parse` restores it as a plain string, stripping the brand. Parse data immediately after deserialization.
2. **Boolean Trap in Domain Models**:
   Adding boolean flags (`isPaid`, `isDelivered`, `isRefunded`) creates $2^N$ combinatorial states. As soon as you have more than 1 boolean representing status, refactor to an enum or union.
3. **Over-Branding**:
   Do not brand every single primitive in your codebase. Brand domain identifiers (`UserId`, `AccountId`) and validated boundary values (`PostalCode`, `EmailAddress`), not transient local variables.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Shotgun Validation** | Checking `if (!email)` in 10 functions | Parse into `EmailAddress` at ingress | Fragile, duplicated checks |
| **Boolean Flag Explosion** | `{ isDraft, isPaid, isShipped }` | Discriminated union of statuses | $2^N$ conflicting states possible |
| **Loose String IDs** | `function transfer(from: string, to: string)` | `from: AccountId, to: AccountId` | Swapping argument order undetected |
| **Undefined in Array Heads** | `const x = arr[0];` (where arr is empty) | Use `NonEmptyArray<T>` | Runtime undefined crashes |
| **Throwing Unchecked Strings** | `throw "Network error"` | Return `Result<T, AppError>` | Uncaught runtime exceptions |
| **Structural Drift in Identifiers** | Treating `UserId` and `OrderId` as strings | Use branded types | Passing OrderId as UserId |

---

## 8. Performance & Type-Checker Complexity

- Branded types add negligible compiler overhead ($O(1)$ property access check).
- Discriminated unions prune branches instantly via hash lookups, reducing type checking duration compared to loose optional fields.

---

## 9. Compiler Internals

When evaluating branded types:
1. TypeScript checks property compatibility. Because the branded type contains a unique symbol property (`[__brand]`), plain `string` is not assignable to `Brand<string, "Tag">`.
2. The compiler retains structural compatibility in runtime output: the emitted JavaScript contains no symbol attachments or object wraps.

---

## 10. Best Practices & Design Guidelines

1. **Parse at Boundaries, Trust Within the Core**: Validate once upon ingress.
2. **Make Illegal States Unrepresentable**: Use discriminated unions for multi-stage workflows.
3. **Brand Critical Domain Identifiers**: Prevent swapping parameters like `customerId` and `vendorId`.
4. **Use Result Types for Expected Domain Errors**: Reserve `throw` for catastrophic, unexpected process failures.

---

## 11. Real-World Case Study

A delivery logistics startup suffered from erroneous deliveries: drivers were dispatched to orders that had been created but not yet paid for, because the state was modeled with loose booleans:
```typescript
interface DeliveryOrder {
  orderId: string;
  isPaid?: boolean;
  driverDispatched?: boolean;
  destinationAddress?: string;
}
```
A bug in an API endpoint set `driverDispatched = true` without checking `isPaid`.

**Refactor**:
```typescript
type DispatchableOrder = {
  status: "PAID";
  paymentProof: string;
  destination: string;
};
```
The dispatch function was changed to accept only `DispatchableOrder`. The bug became **impossible to compile**, saving thousands of dollars in wasted fleet logistics.

---

## 12. Interview Questions

<details>
<summary>1. What does "Parse, Don't Validate" mean?</summary>

It means transforming unverified raw input into a refined, type-safe representation upon ingestion. Instead of checking data with a boolean predicate and retaining the loose type, you parse it into a specific type that proves validation succeeded, freeing downstream code from defensive checks.
</details>

<details>
<summary>2. How do you make illegal states unrepresentable in TypeScript?</summary>

By modeling domain entities using discriminated unions instead of multiple optional properties. Each state variant includes only the properties valid for that specific phase of the lifecycle, preventing impossible configurations (like a shipped order without a tracking number).
</details>

<details>
<summary>3. What is a branded type in TypeScript?</summary>

A branded (or nominal) type is a structural type intersected with a unique phantom property (e.g. `type UserId = string & { readonly __brand: unique symbol }`). It allows TypeScript's structural type system to treat otherwise identical primitives as distinct types.
</details>

<details>
<summary>4. Why is `type NonEmptyArray<T> = [T, ...T[]]` useful?</summary>

It guarantees at compile time that the array contains at least one element. Functions accessing `arr[0]` can safely rely on the element being present without checking for `undefined`.
</details>

<details>
<summary>5. What is the difference between throwing an error and returning a `Result<T, E>`?</summary>

Throwing an error bypasses the TypeScript type checker; callers cannot see what errors a function might throw from its type signature. Returning a `Result<T, E>` forces callers to explicitly handle both success and error cases via compiler-enforced pattern matching.
</details>

<details>
<summary>6. Why should you avoid multiple boolean flags in domain models?</summary>

Each boolean flag doubles the number of possible states ($2^N$). With 3 booleans, you have 8 states, many of which may be contradictory or invalid. A discriminated union restricts the entity strictly to the valid states.
</details>

<details>
<summary>7. How do branded types impact runtime performance?</summary>

Zero runtime overhead. The branding symbol is purely a compile-time artifact and is erased during emission. The emitted JavaScript uses plain strings or numbers.
</details>

<details>
<summary>8. How do you ensure exhaustive switch statement checking over a union?</summary>

By asserting the default branch to the `never` type (`const _unreachable: never = state; throw new Error(_unreachable)`). If a new state variant is added to the union, TypeScript throws a compile-time error until a corresponding case is implemented.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Create a branded UserId type</summary>

**Task**: Define a branded type for `UserId` and a parser function that validates non-empty strings.

**Solution**:
```typescript
declare const __brand: unique symbol;
export type UserId = string & { readonly [__brand]: "UserId" };

export function parseUserId(raw: string): UserId {
  if (!raw || raw.trim().length === 0) {
    throw new Error("Invalid UserId: cannot be empty");
  }
  return raw as UserId;
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Safe NonEmptyArray constructor</summary>

**Task**: Implement a function that converts a standard array into a `NonEmptyArray<T>` or throws an error.

**Solution**:
```typescript
export type NonEmptyArray<T> = [T, ...T[]];

export function toNonEmptyArray<T>(arr: T[]): NonEmptyArray<T> {
  if (arr.length === 0) {
    throw new Error("Array must contain at least one element");
  }
  return arr as NonEmptyArray<T>;
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Model a Payment Lifecycle with Discriminated Unions</summary>

**Task**: Design an `OrderPayment` type supporting `Pending`, `Authorized` (with token), `Captured` (with transactionId), and `Failed` (with errorCode).

**Solution**:
```typescript
export type OrderPayment =
  | { readonly status: "PENDING" }
  | { readonly status: "AUTHORIZED"; readonly authCode: string }
  | { readonly status: "CAPTURED"; readonly transactionId: string }
  | { readonly status: "FAILED"; readonly errorCode: string };
```
</details>

<details>
<summary>Challenge 4 (Medium): Implement an exhaustive state machine matcher</summary>

**Task**: Write a function taking `OrderPayment` that returns a human-readable summary, using the `never` type to guarantee exhaustiveness.

**Solution**:
```typescript
import { OrderPayment } from "./challenge3.js";

export function describePayment(p: OrderPayment): string {
  switch (p.status) {
    case "PENDING": return "Awaiting customer authorization";
    case "AUTHORIZED": return `Authorized with code: ${p.authCode}`;
    case "CAPTURED": return `Settled txn: ${p.transactionId}`;
    case "FAILED": return `Declined error: ${p.errorCode}`;
    default: {
      const _unreachable: never = p;
      throw new Error(`Unhandled payment state: ${_unreachable}`);
    }
  }
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Type-Safe Monadic Result Pipeline</summary>

**Task**: Implement a `Result<T, E>` monad with `.map()` and `.flatMap()` methods that propagate errors automatically.

**Solution**:
```typescript
export class ResultAsync<T, E> {
  private constructor(
    private readonly ok: boolean,
    private readonly value?: T,
    private readonly error?: E
  ) {}

  public static ok<T, E = never>(value: T): ResultAsync<T, E> {
    return new ResultAsync<T, E>(true, value, undefined);
  }

  public static err<T = never, E = Error>(error: E): ResultAsync<T, E> {
    return new ResultAsync<T, E>(false, undefined, error);
  }

  public map<U>(fn: (val: T) => U): ResultAsync<U, E> {
    if (!this.ok) return ResultAsync.err<U, E>(this.error!);
    return ResultAsync.ok<U, E>(fn(this.value!));
  }

  public flatMap<U>(fn: (val: T) => ResultAsync<U, E>): ResultAsync<U, E> {
    if (!this.ok) return ResultAsync.err<U, E>(this.error!);
    return fn(this.value!);
  }
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Compile-Time State Transition Matrix</summary>

**Task**: Create a type-safe state machine transition function where valid transitions are governed by a compile-time transition matrix type.

**Solution**:
```typescript
type State = "LOCKED" | "UNLOCKED" | "ALARM";
type Event = "COIN" | "PUSH" | "PASS";

type ValidTransitions = {
  LOCKED: { COIN: "UNLOCKED" };
  UNLOCKED: { PUSH: "LOCKED"; PASS: "LOCKED" };
  ALARM: { COIN: "LOCKED" };
};

export function transition<
  S extends State,
  E extends keyof ValidTransitions[S]
>(currentState: S, event: E): ValidTransitions[S][E] {
  // Runtime lookup matching compile-time matrix
  const matrix: any = {
    LOCKED: { COIN: "UNLOCKED" },
    UNLOCKED: { PUSH: "LOCKED", PASS: "LOCKED" },
    ALARM: { COIN: "LOCKED" },
  };
  return matrix[currentState][event];
}
```
</details>
