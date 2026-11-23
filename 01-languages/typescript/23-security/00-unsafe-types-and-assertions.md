# Unsafe Types and Assertions in TypeScript: any, unknown, and Type Casts

> Comprehensive security analysis of unsafe type constructs in TypeScript: the dangers of `any`, double assertions (`as unknown as T`), non-null assertions (`!`), and safe alternatives using `unknown` and runtime narrowing.

| Metadata | Details |
| :--- | :--- |
| **Category** | Security |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [01-fundamentals/01-primitive-types.md](../01-fundamentals/01-primitive-types.md), [01-fundamentals/06-type-widening-narrowing.md](../01-fundamentals/06-type-widening-narrowing.md) |
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

TypeScript is not a memory-safe or type-sound language in the academic sense. By design, TypeScript prioritizes pragmatic developer experience and JavaScript interop over absolute soundness. Consequently, TypeScript includes escape hatches that can completely disable the compiler's safety guarantees:
- **`any`**: An infectious type escape hatch that disables all type checking on a value and all values derived from it.
- **Type Assertions (`x as T`)**: Force the compiler to treat a value as type `T` without performing runtime validation.
- **Double Assertions (`x as unknown as T`)**: Forcefully bypasses the compiler's assignability safety check, asserting completely unrelated types.
- **Non-Null Assertion (`x!`)**: Silences `null` and `undefined` checks without runtime validation.

When used carelessly, these escape hatches create a **false sense of security**, causing production outages and severe security vulnerabilities (e.g., prototype pollution, unauthorized data access, and unhandled `TypeError: Cannot read properties of undefined`).

---

## 2. Core Concepts

### `any` vs. `unknown`
- **`any`**: Disables the type checker. You can access arbitrary properties, call it as a function, or assign it to any other type. It is bidirectional: every type is assignable to `any`, and `any` is assignable to every type.
- **`unknown`**: The type-safe counterpart. Every type is assignable to `unknown`, but `unknown` is assignable to *nothing else* except `any` and `unknown` without explicit narrowing or runtime guards.

```
+-------------------------------------------------------------------------+
|                  Type Safety Hierarchy: any vs unknown                  |
+-------------------------------------------------------------------------+
|                                                                         |
|  [ Type: any ]                                                          |
|  x.foo.bar()  ---> Allowed by compiler (No check)                       |
|  const n: number = x; ---> Allowed by compiler (Silent infection!)      |
|  Runtime: Crashes with TypeError if x is undefined!                     |
|                                                                         |
|  [ Type: unknown ]                                                      |
|  x.foo.bar()  ---> COMPILE ERROR (Object is of type 'unknown')          |
|  Must narrow:                                                           |
|  if (typeof x === "object" && x !== null && "foo" in x) { ... }         |
|  Runtime: 100% Guaranteed Safe!                                         |
|                                                                         |
+-------------------------------------------------------------------------+
```

### The Double Assertion Trap (`as unknown as T`)
TypeScript permits `A as B` only if `A` is assignable to `B`, or `B` is assignable to `A`. Casting between incompatible types (`string as number`) is blocked with TS2352. Double assertions circumvent this protection by routing through the top type `unknown`.

---

## 3. How It Works Internally

1. **Type Assignability Bypass**: When the TypeChecker encounters `node as T`, it evaluates `isTypeAssignableTo(sourceType, targetType)`. If both tests fail, it emits TS2352. When written as `node as unknown as T`, both steps pass because every type is assignable to `unknown`, and `unknown` can be asserted to `T`.
2. **Type Erasure**: Type assertions produce zero runtime JavaScript bytecode. `x as User` compiles to `x`. If `x` does not match the `User` schema at runtime, the code will fail silently.

---

## 4. Syntax & API Reference

### Safe Narrowing with Custom Type Guards
```typescript
export interface UserSession {
  userId: string;
  role: "admin" | "member";
}

// User-defined type guard
export function isUserSession(data: unknown): data is UserSession {
  if (typeof data !== "object" || data === null) return false;
  const obj = data as Record<string, unknown>;
  return (
    typeof obj.userId === "string" &&
    (obj.role === "admin" || obj.role === "member")
  );
}
```

### `satisfies` Operator vs Type Assertion
```typescript
interface Theme {
  colors: Record<string, string>;
}

// BAD: Type assertion loses exact keys and bypasses validation
const badTheme = { colors: { primary: "#00f" } } as Theme;

// GOOD: satisfies validates shape without widening exact property keys!
const goodTheme = {
  colors: {
    primary: "#00f",
    secondary: "#f00",
  },
} satisfies Theme;

// goodTheme.colors.primary is inferred as string (not widened or erased)
```

---

## 5. Practical Examples

### Example 1: Eliminating `any` in JSON Parsing
```typescript
// UNSAFE: JSON.parse returns `any` by default
function unsafeParse(json: string) {
  const data = JSON.parse(json); // type is any
  return data.user.profile.name; // Can crash with TypeError
}

// SAFE: Force return to unknown and validate
function safeParse<T>(json: string, validator: (val: unknown) => val is T): T {
  const parsed: unknown = JSON.parse(json);
  if (!validator(parsed)) {
    throw new Error("Invalid payload structure");
  }
  return parsed;
}
```

### Example 2: Dangers of the Non-Null Assertion Operator (`!`)
```typescript
interface AccountSettings {
  notifications?: {
    emailAlerts?: boolean;
  };
}

function sendNotification(settings: AccountSettings) {
  // UNSAFE: If settings.notifications is undefined, throws at runtime!
  // const enabled = settings.notifications!.emailAlerts!;

  // SAFE: Optional chaining with nullish coalescing
  const enabled = settings.notifications?.emailAlerts ?? false;
  return enabled;
}
```

### Example 3: Edge Case - Double Assertion Bypassing Security Gates
```typescript
interface AdminContext {
  isAdmin: true;
  adminKey: string;
}

interface GuestContext {
  isAdmin: false;
}

const guest: GuestContext = { isAdmin: false };

// Catastrophic Bypass:
const fakeAdmin = guest as unknown as AdminContext;
// fakeAdmin.adminKey is undefined at runtime despite TS claiming it is a string!
```

### Example 4: Compiler Error & Incompatible Assertion Block
```typescript
const count = 42;

// Direct assertion between disjoint types is blocked:
// @ts-expect-error - TS2352: Conversion of type 'number' to type 'string' may be a mistake because neither type sufficiently overlaps with the other.
const text = count as string;
```

### Example 5: Defending Against Prototype Pollution with Safe Dictionaries
```typescript
// Object literals inherit Object.prototype by default, vulnerable to __proto__ injection
export function createSafeDictionary<T>(): Record<string, T> {
  // Object with null prototype has no __proto__ or toString properties
  return Object.create(null) as Record<string, T>;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`catch (err)` Typing**:
   In TypeScript versions prior to 4.0, caught errors were typed as `any`. Modern TypeScript allows `"useUnknownInCatchVariables": true` in `tsconfig.json`, typing caught errors as `unknown` and requiring `instanceof Error` checks.
2. **Third-Party Untyped Libraries**:
   Importing modules with missing declarations defaults their exports to `any` unless `"noImplicitAny": true` is enforced.
3. **The Non-Null Assertion Loop Hole**:
   Using `!` on DOM queries (`document.getElementById("btn")!`) will crash in tests or SSR environments where the DOM is unmounted.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Using `any` for Unset Types** | `function f(x: any)` | `function f(x: unknown)` | Spreads untyped corruption |
| **Double Assertion Bypass** | `val as unknown as Target` | Use runtime schema / type guard | Bypasses all compiler type checks |
| **Indiscriminate `!` Usage** | `obj.user!.id` | `obj.user?.id` | `Cannot read properties of undefined` |
| **Unchecked `JSON.parse`** | `const u: User = JSON.parse(str)` | Validate with Zod / TypeBox | Runtime crash on malformed JSON |
| **Casting Caught Errors** | `catch (err) { (err as Error).msg }` | `if (err instanceof Error)` | Non-Error throws (strings, null) crash |
| **Empty Object Assertion** | `const a = {} as Config` | Initialize required fields explicitly | Incomplete object state |

---

## 8. Performance & Type-Checker Complexity

- `any` causes the type checker to immediately abort assignability evaluation ($O(1)$), but creates extreme debugging latency for engineering teams.
- Runtime type guards incur minor CPU overhead for `typeof` and `in` checks ($O(1)$) while ensuring complete system reliability.

---

## 9. Compiler Internals

When the TypeChecker evaluates `any`:
1. `type.flags & ts.TypeFlags.Any` returns true.
2. The compiler short-circuits `checkPropertyAccessExpression` and marks the expression type as `any`.
3. With `unknown` (`ts.TypeFlags.Unknown`), the compiler enforces that no operations (member access, invocation, arithmetic) are permitted until narrowed via control flow analysis.

---

## 10. Best Practices & Design Guidelines

1. **Enable `"noImplicitAny": true` and `"useUnknownInCatchVariables": true`**.
2. **Forbid Double Assertions**: Enforce `@typescript-eslint/consistent-type-assertions`.
3. **Treat All Network & Disk Inputs as `unknown`**.
4. **Use `satisfies` Over `as`**: Validates against a contract without stripping specific literal types.

---

## 11. Real-World Case Study

A healthcare billing microservice experienced unauthorized data access during an API upgrade.

**Vulnerability Trace**:
```typescript
// Legacy handler:
app.post("/update-record", (req, res) => {
  const payload = req.body as PatientRecord; // Forced assertion!
  database.save(payload.id, payload.data);
});
```
An attacker supplied `{ id: "target-id", data: { balance: 0 } }` without authentication headers. Because `as PatientRecord` performed no validation, the malicious payload was written directly to the database.

**Remediation**:
Replaced type assertion with strict Zod schema validation. Requests with missing authentication context were rejected with `400 Bad Request`.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between `any` and `unknown`?</summary>

`any` completely disables type checking for that value and all derived expressions. `unknown` is type-safe: you cannot access properties, call it, or assign it to another typed variable without first proving its shape via runtime type narrowing or guards.
</details>

<details>
<summary>2. Why is `as unknown as T` considered an anti-pattern?</summary>

TypeScript intentionally rejects type assertions between disjoint types (e.g. `number as string`). Casting to `unknown` first bypasses this check, forcing the compiler to accept a fabricated type that does not exist at runtime.
</details>

<details>
<summary>3. What does the non-null assertion operator (`!`) do at runtime?</summary>

Nothing. It is completely erased during compilation. It merely instructs the TypeScript compiler to assume the operand is neither `null` nor `undefined`. If the value is nullish at runtime, a `TypeError` will occur.
</details>

<details>
<summary>4. How does the `satisfies` operator differ from a type assertion (`as`)?</summary>

A type assertion (`x as T`) tells the compiler to treat `x` as `T`, widening literals and hiding missing properties. `x satisfies T` verifies that `x` conforms to contract `T` while preserving the exact literal types and specific properties of `x`.
</details>

<details>
<summary>5. What does the tsconfig option `useUnknownInCatchVariables` do?</summary>

It changes the default type of error variables in `catch (err)` blocks from `any` to `unknown`, preventing unverified property access and encouraging `err instanceof Error` checks.
</details>

<details>
<summary>6. How can an attacker exploit prototype pollution in TypeScript?</summary>

TypeScript interfaces do not prevent prototype pollution at runtime. If an untyped JSON payload contains keys like `__proto__` or `constructor`, merging it into an object can overwrite global `Object.prototype` methods, leading to remote code execution or privilege escalation.
</details>

<details>
<summary>7. What is a user-defined type guard?</summary>

A function whose return type is a type predicate of the form `paramName is Type`. If the function returns `true`, TypeScript narrows the parameter to that type in downstream control-flow blocks.
</details>

<details>
<summary>8. Why is `{}` not equivalent to "any object"?</summary>

In TypeScript, `{}` represents any non-nullish value, meaning primitives like numbers, strings, and booleans are assignable to `{}`. To represent a real object dictionary, use `Record<string, unknown>` or `object`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Refactor any to unknown with type guard</summary>

**Task**: Refactor `function printId(id: any)` to take `unknown` and safely handle strings and numbers.

**Solution**:
```typescript
export function printId(id: unknown): string {
  if (typeof id === "string") return id.toUpperCase();
  if (typeof id === "number") return id.toFixed(2);
  throw new Error("Invalid id type");
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Replace non-null assertion with safe fallback</summary>

**Task**: Fix `const port: number = config.server!.port!;` without using `!`.

**Solution**:
```typescript
interface AppConfig {
  server?: { port?: number };
}

function getPort(config: AppConfig): number {
  return config.server?.port ?? 8080;
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Safe unknown error handler</summary>

**Task**: Write an error extraction utility that accepts `err: unknown` and always returns a string error message.

**Solution**:
```typescript
export function getErrorMessage(err: unknown): string {
  if (err instanceof Error) {
    return err.message;
  }
  if (typeof err === "string") {
    return err;
  }
  return "Unknown exception occurred";
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Custom Type Guard for Arrays</summary>

**Task**: Implement a generic type guard `isArrayOfStrings(val: unknown): val is string[]`.

**Solution**:
```typescript
export function isArrayOfStrings(val: unknown): val is string[] {
  return (
    Array.isArray(val) &&
    val.every((item) => typeof item === "string")
  );
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Deep Prototype Freeze Utility</summary>

**Task**: Write a TypeScript utility that recursively freezes an object and neutralizes its `__proto__` reference to prevent tampering.

**Solution**:
```typescript
export function deepHardenedFreeze<T extends object>(obj: T): Readonly<T> {
  Object.freeze(obj);
  for (const key of Object.getOwnPropertyNames(obj)) {
    const val = (obj as Record<string, unknown>)[key];
    if (val && typeof val === "object" && !Object.isFrozen(val)) {
      deepHardenedFreeze(val);
    }
  }
  return obj;
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Type-Safe Schema Validator from Scratch</summary>

**Task**: Implement a minimal type-safe schema validator that parses unknown objects into typed records without third-party libraries.

**Solution**:
```typescript
export type Validator<T> = (val: unknown) => val is T;

export function createObjectValidator<T extends Record<string, unknown>>(
  schema: { [K in keyof T]: Validator<T[K]> }
): Validator<T> {
  return (val: unknown): val is T => {
    if (typeof val !== "object" || val === null) return false;
    const record = val as Record<string, unknown>;
    for (const key of Object.keys(schema)) {
      const validator = schema[key];
      if (!validator || !validator(record[key])) return false;
    }
    return true;
  };
}
```
</details>
