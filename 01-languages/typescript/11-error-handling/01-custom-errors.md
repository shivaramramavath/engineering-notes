# Custom Error Classes in TypeScript

> Comprehensive guide to authoring type-safe, extensible custom Error classes in modern TypeScript, handling prototype downleveling, nominal branding, error codes, and serialization.

| Metadata | Details |
| :--- | :--- |
| **Category** | Error Handling |
| **Difficulty** | Intermediate |
| **Prerequisites** | [05-classes/00-classes.md](../05-classes/00-classes.md), [05-classes/02-inheritance-and-abstract-classes.md](../05-classes/02-inheritance-and-abstract-classes.md), [11-error-handling/00-catching-and-narrowing-errors.md](./00-catching-and-narrowing-errors.md) |
| **TypeScript Baseline** | TS 2.2+ (`new.target`), TS 4.6+ (`ErrorOptions.cause`), TS 5.0+ |
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
Standard JavaScript `Error` objects only carry a `message`, `name`, and engine-generated `stack`. In complex applications, domain failures require structured metadata: HTTP status codes, validation constraint violations, retry counts, machine-readable error codes (`AUTH_TOKEN_EXPIRED`), and nominal classification.

Writing custom error classes in TypeScript bridges runtime diagnostics with compile-time type safety. However, subclassing built-in classes like `Error` in JavaScript historically suffered from prototype chain breakage during downlevel compilation (e.g. compiling ES6 classes down to ES5 functions). Modern TypeScript solves these issues while enabling nominal discrimination through tags and brands.

---

## 2. Core Concepts
- **Subclassing Built-in `Error`**: Inheriting from `Error` gives access to native V8 call stack capture and stack trace formatting.
- **The Prototype Restoration Problem (`Object.setPrototypeOf`)**: When downleveling to ES5/ES3, the JavaScript constructor invocation fails to link the prototype chain correctly unless `Object.setPrototypeOf(this, new.target.prototype)` or `this.constructor.prototype` is explicitly set. In ES6+ targets, native `super()` handles prototype wiring automatically.
- **Nominal Branding / Tagging**: TypeScript uses structural typing. Two distinct error classes with identical fields (e.g. `NotFoundUserError` vs `NotFoundPostError`) are considered assignable to each other unless explicitly differentiated using a discriminant like `readonly _tag: 'NotFoundUserError'` or unique symbol brands.
- **Error Serialization**: Native `Error` objects cannot be cleanly serialized to JSON via `JSON.stringify(err)` because `message` and `stack` are non-enumerable properties on `Error.prototype`. Custom classes must provide a dedicated `.toJSON()` method.
- **Stack Trace Customization (`Error.captureStackTrace`)**: In V8 (Node.js, Chrome), `Error.captureStackTrace(this, CustomError)` prunes the constructor invocation itself from the generated stack string.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------+
|               class DomainError extends Error             |
+-----------------------------+-----------------------------+
                              |
                     constructor(message)
                              |
                              v
                  super(message, options)
                              |
    +-------------------------+-------------------------+
    | ES5 Target Downleveling | ES6 / ES2022+ Target    |
    | Object.setPrototypeOf(  | Native class heritage;  |
    |   this,                 | [[Prototype]] linked    |
    |   new.target.prototype  | automatically by VM     |
    | );                      |                         |
    +-------------------------+-------------------------+
                              |
                              v
                  Error.captureStackTrace?(this, this.constructor)
                              |
                              v
            instanceof DomainError === true (Guaranteed)
```

In ES5 compilation targets, constructors are emitted as standard functions called with `this` created by the `new` operator. But `Error.call(this, message)` in ES5 ignores the passed `this` and returns a newly allocated `Error` instance from the native C++ engine, severing prototype linkage. Calling `Object.setPrototypeOf(this, CustomError.prototype)` re-attaches the created instance to the custom class prototype.

---

## 4. Syntax & API Reference

```typescript
// 1. Base Domain Error with Discriminant Tag and Cause
abstract class AppBaseError extends Error {
  abstract readonly _tag: string;
  readonly timestamp: number;

  constructor(message: string, options?: ErrorOptions) {
    super(message, options);
    this.name = this.constructor.name;
    this.timestamp = Date.now();

    // ES5 prototype fix (needed if downleveling below ES2015)
    Object.setPrototypeOf(this, new.target.prototype);

    // V8 stack frame pruning
    if (Error.captureStackTrace) {
      Error.captureStackTrace(this, this.constructor);
    }
  }

  abstract toJSON(): Record<string, unknown>;
}

// 2. Concrete Subclass
class EntityNotFoundError extends AppBaseError {
  readonly _tag = 'EntityNotFoundError' as const;

  constructor(
    public readonly entityName: string,
    public readonly entityId: string | number,
    options?: ErrorOptions
  ) {
    super(`${entityName} with ID ${entityId} was not found`, options);
  }

  toJSON() {
    return {
      tag: this._tag,
      message: this.message,
      entityName: this.entityName,
      entityId: this.entityId,
      timestamp: this.timestamp,
    };
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic HTTP Error Hierarchy
```typescript
class HttpError extends Error {
  constructor(
    public readonly statusCode: number,
    message: string,
    options?: ErrorOptions
  ) {
    super(message, options);
    this.name = 'HttpError';
    Object.setPrototypeOf(this, new.target.prototype);
  }
}

class UnauthorizedError extends HttpError {
  constructor(message = 'Authentication required') {
    super(401, message);
    this.name = 'UnauthorizedError';
  }
}

class ForbiddenError extends HttpError {
  constructor(public readonly requiredRole: string) {
    super(403, `Access forbidden. Requires role: ${requiredRole}`);
    this.name = 'ForbiddenError';
  }
}
```

### Example 2: Real-World Validation Failure with Field Error Map
```typescript
interface ValidationErrorField {
  readonly field: string;
  readonly constraint: string;
  readonly received: unknown;
}

class SchemaValidationError extends Error {
  public readonly _tag = 'SchemaValidationError' as const;
  public readonly errors: ReadonlyArray<ValidationErrorField>;

  constructor(errors: ReadonlyArray<ValidationErrorField>, options?: ErrorOptions) {
    const summary = errors.map(e => `${e.field}: ${e.constraint}`).join('; ');
    super(`Validation failed (${errors.length} errors): ${summary}`, options);
    this.name = 'SchemaValidationError';
    this.errors = errors;
    Object.setPrototypeOf(this, new.target.prototype);
  }

  hasField(fieldName: string): boolean {
    return this.errors.some(e => e.field === fieldName);
  }
}
```

### Example 3: Edge Case - Custom JSON Serialization
```typescript
// JSON.stringify(new Error("foo")) returns "{}" by default!
class SerializableError extends Error {
  constructor(
    message: string,
    public readonly code: string,
    public readonly context: Record<string, unknown> = {}
  ) {
    super(message);
    this.name = 'SerializableError';
    Object.setPrototypeOf(this, new.target.prototype);
  }

  toJSON() {
    return {
      name: this.name,
      code: this.code,
      message: this.message,
      context: this.context,
      stack: this.stack,
    };
  }
}

const err = new SerializableError('Account locked', 'AUTH_007', { attempts: 5 });
const serialized = JSON.stringify(err);
// Result: '{"name":"SerializableError","code":"AUTH_007","message":"Account locked","context":{"attempts":5},...}'
```

### Example 4: Compiler Failure Case - Structural Equivalence
```typescript
class DatabaseTimeoutError extends Error {
  readonly timeoutMs = 5000;
}

class ApiTimeoutError extends Error {
  readonly timeoutMs = 5000;
}

const dbErr: DatabaseTimeoutError = new DatabaseTimeoutError('Query failed');
// TypeScript structurally permits this because both classes have identical shapes!
const apiErr: ApiTimeoutError = dbErr;

// Fix: Add nominal brand or distinct discriminant tag
class BrandedTimeoutError extends Error {
  declare private readonly __brand: unique symbol;
  readonly timeoutMs = 5000;
}
class OtherTimeoutError extends Error {
  declare private readonly __otherBrand: unique symbol;
  readonly timeoutMs = 5000;
}

const b: BrandedTimeoutError = new BrandedTimeoutError('failed');
// @ts-expect-error TS2322: Type 'BrandedTimeoutError' is not assignable to type 'OtherTimeoutError'.
const o: OtherTimeoutError = b;
```

### Example 5: Idiomatic Error Factory with Error.cause Preservation
```typescript
type ErrorContext = Record<string, unknown>;

class ApplicationError extends Error {
  constructor(
    message: string,
    public readonly code: string,
    public readonly context: ErrorContext = {},
    options?: ErrorOptions
  ) {
    super(message, options);
    this.name = this.constructor.name;
    Object.setPrototypeOf(this, new.target.prototype);
  }

  static wrap(caught: unknown, code: string, message: string, context?: ErrorContext): ApplicationError {
    const cause = caught instanceof Error ? caught : new Error(String(caught));
    return new ApplicationError(message, code, context, { cause });
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **ES5 Prototype Breakage**: If `target: "ES5"` is used and you omit `Object.setPrototypeOf(this, new.target.prototype)`, `new CustomError() instanceof CustomError` returns `false` at runtime!
2. **`JSON.stringify` Omits Properties**: Standard `Error` properties (`message`, `stack`, `name`) are defined on the prototype with `{ enumerable: false }`. Serializing custom errors into HTTP JSON responses requires explicit getters or `.toJSON()` implementations.
3. **Structural Assignment Ambiguity**: Because TypeScript is structural, two classes with identical property sets are mutually assignable. In union types (`type AppErrors = UserNotFoundError | PostNotFoundError`), type narrowing with `in` will fail unless unique discriminator keys (`_tag`, `code`) are present.
4. **`Error.captureStackTrace` is V8 Only**: `Error.captureStackTrace` is an engine extension in V8 (Node.js, Chrome). It is `undefined` in Safari (JavaScriptCore) and Firefox (SpiderMonkey). Always check `if (Error.captureStackTrace)` before calling.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Compiler / Runtime Consequence | Correction |
| :--- | :--- | :--- |
| Omitting `super(message)` | **TS17009**: 'super' must be called before accessing 'this' | Always call `super(message, options)` on the first line |
| Forgetting `Object.setPrototypeOf` in ES5 targets | `instanceof CustomError` returns `false` at runtime | Call `Object.setPrototypeOf(this, new.target.prototype)` |
| Overwriting `this.name` with hardcoded string | Renaming the class fails to update the runtime name | Use `this.name = this.constructor.name` or `readonly _tag` |
| Calling `JSON.stringify(customErr)` directly | Produces `{}` because `Error` properties are non-enumerable | Implement `toJSON()` method returning a plain object |
| Not adding discriminant tags | Different custom error classes collapse structurally in unions | Add `readonly _tag = 'MyError' as const` |
| Swallowing cause | Debugging root causes becomes impossible in production | Pass `{ cause }` to `super(msg, { cause })` |

---

## 8. Performance & Type-Checker Complexity
Instantiating custom errors involves native V8 heap allocation and stack-frame string generation. Creating an `Error` instance takes ~2–10 microseconds in V8 primarily due to reading the execution call stack.

In the TypeScript compiler:
- Class declarations add symbols to the Symbol Table with both type and value meanings.
- Structural checking for errors requires verifying inheritance against the built-in `Error` type interface.
- Keep inheritance trees shallow ($\le 3$ levels) to avoid excessive property traversal in union discriminants.

---

## 9. Compiler Internals
When the TypeScript emitter downlevels a class extending `Error` to ES5:
1. It generates a constructor function: `function CustomError() { var _this = _super.call(this) || this; }`.
2. Because `Error.call(this)` returns a new instance whose prototype is `Error.prototype`, `_this` is detached from `CustomError.prototype`.
3. The TypeScript compiler documentation specifically advises adding `Object.setPrototypeOf(this, CustomError.prototype)` or using `new.target.prototype` to reconnect the chain.

---

## 10. Best Practices & Design Guidelines
- Extend a common abstract `BaseApplicationError` across your system.
- Always include a literal `readonly _tag` or `readonly code` discriminant for discriminated unions.
- Leverage `ErrorOptions.cause` to chain underlying transport or database failures.
- Provide a `.toJSON()` method on every error class that will be sent over an API response or written to structured JSON logs.
- Guard against non-V8 runtimes when calling `Error.captureStackTrace`.

---

## 11. Real-World Case Study
A financial services backend needed to distinguish between transient payment gateway timeouts (retryable) and fatal merchant account bans (non-retryable). Naive `catch (err)` handling was retrying non-retryable errors, triggering regulatory penalty flags.

**Refactored Domain Error Architecture:**
```typescript
export abstract class PaymentGatewayError extends Error {
  abstract readonly _tag: string;
  abstract readonly retryable: boolean;
  public readonly transactionId: string;

  constructor(message: string, transactionId: string, options?: ErrorOptions) {
    super(message, options);
    this.name = this.constructor.name;
    this.transactionId = transactionId;
    Object.setPrototypeOf(this, new.target.prototype);
  }
}

export class PaymentTimeoutError extends PaymentGatewayError {
  readonly _tag = 'PaymentTimeoutError' as const;
  readonly retryable = true;
  constructor(txId: string, public readonly timeoutMs: number, options?: ErrorOptions) {
    super(`Payment transaction ${txId} timed out after ${timeoutMs}ms`, txId, options);
  }
}

export class MerchantSuspendedError extends PaymentGatewayError {
  readonly _tag = 'MerchantSuspendedError' as const;
  readonly retryable = false;
  constructor(txId: string, public readonly merchantId: string, options?: ErrorOptions) {
    super(`Merchant ${merchantId} is suspended; transaction ${txId} aborted`, txId, options);
  }
}

// Exhaustive retry handler
export function handlePaymentFailure(error: PaymentGatewayError): 'RETRY' | 'ABORT' {
  if (error.retryable) {
    console.warn(`[RETRY] Transaction ${error.transactionId}: ${error.message}`);
    return 'RETRY';
  }
  console.error(`[FATAL] Transaction ${error.transactionId}: ${error.message}`);
  return 'ABORT';
}
```

---

## 12. Interview Questions

<details>
<summary>1. Why was <code>Object.setPrototypeOf(this, new.target.prototype)</code> historically required when subclassing <code>Error</code> in TypeScript?</summary>

When compiling to ES5, JavaScript constructors are regular functions. Invoking `Error.call(this)` within the constructor ignores `this` and returns a fresh native `Error` object linked to `Error.prototype` instead of the subclass prototype. Calling `Object.setPrototypeOf` manually repoints the prototype of `this` back to the subclass, restoring `instanceof` checks.
</details>

<details>
<summary>2. Why does <code>JSON.stringify(new Error("msg"))</code> output <code>{}</code>?</summary>

The JavaScript `Error` specification defines properties like `message`, `name`, and `stack` as non-enumerable (`enumerable: false`). `JSON.stringify` only serializes an object's own enumerable properties, thus omitting `message` and `stack` entirely unless an explicit `.toJSON()` method is provided.
</details>

<details>
<summary>3. How does TypeScript's structural typing affect custom error classes, and how do you resolve it?</summary>

TypeScript considers two classes with identical properties structurally equivalent. If `UserError` and `OrderError` both only possess a `code: number` property, a variable of type `UserError` accepts an instance of `OrderError`. To fix this, use a literal discriminant (`readonly _tag = 'UserError' as const`) or unique symbol brands (`declare readonly __brand: unique symbol`).
</details>

<details>
<summary>4. What does <code>Error.captureStackTrace</code> do, and what are its parameters?</summary>

It is a V8-specific method that creates a `.stack` property on the target object. Its second argument is an optional function (typically the constructor); when passed, all stack frames above and including that function are omitted from the generated stack trace, keeping traces clean.
</details>

<details>
<summary>5. How does <code>new.target</code> work inside a custom error constructor?</summary>

`new.target` evaluates to the constructor function that was directly invoked with `new`. If `SpecialError` extends `BaseError`, inside `BaseError`'s constructor `new.target` refers to `SpecialError`, allowing base classes to dynamically link prototypes correctly.
</details>

<details>
<summary>6. Should you subclass <code>Error</code> or return a plain object in modern TypeScript?</summary>

Subclass `Error` when throwing exceptions that propagate up the stack and need stack traces for debugging or logging unhandled conditions. Use plain objects (discriminated unions/Result types) for expected domain failure cases where control flow should be explicit and checked at compile time.
</details>

<details>
<summary>7. How do you pass low-level root causes to custom errors in TypeScript 4.6+?</summary>

By accepting `options?: ErrorOptions` in the constructor and passing it to `super(message, options)`. The original error is accessible via `error.cause`.
</details>

<details>
<summary>8. What compiler target eliminates the need for <code>Object.setPrototypeOf</code> in custom errors?</summary>

`target: "ES2015"` (ES6) or higher. In native ES6 class runtimes, `super()` correctly constructs the instance with `[[Prototype]]` wired to `new.target.prototype`.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Basic Subclass**: Create an `InvalidInputError` class that extends `Error`, defaults its message to `"Invalid input provided"`, and sets `this.name = 'InvalidInputError'`.
   <details>
   <summary>Solution</summary>

   ```typescript
   class InvalidInputError extends Error {
     constructor(message = 'Invalid input provided') {
       super(message);
       this.name = 'InvalidInputError';
       Object.setPrototypeOf(this, new.target.prototype);
     }
   }
   ```
   </details>

2. **Error with HTTP Code**: Create an `HttpNotFoundError` that accepts an optional resource string and sets status code to 404.
   <details>
   <summary>Solution</summary>

   ```typescript
   class HttpNotFoundError extends Error {
     public readonly statusCode = 404;
     constructor(resource = 'Resource') {
       super(`${resource} not found`);
       this.name = 'HttpNotFoundError';
       Object.setPrototypeOf(this, new.target.prototype);
     }
   }
   ```
   </details>

### Medium
3. **Tagged Union Errors**: Create three custom error classes (`NotFoundErr`, `UnauthorizedErr`, `RateLimitErr`) sharing a discriminated `_tag` union, and write an exhaustive handler using `switch`.
   <details>
   <summary>Solution</summary>

   ```typescript
   class NotFoundErr extends Error {
     readonly _tag = 'NotFound' as const;
   }
   class UnauthorizedErr extends Error {
     readonly _tag = 'Unauthorized' as const;
   }
   class RateLimitErr extends Error {
     readonly _tag = 'RateLimit' as const;
     constructor(public readonly retryAfterSeconds: number) {
       super();
     }
   }

   type AppErr = NotFoundErr | UnauthorizedErr | RateLimitErr;

   function handleErr(err: AppErr): string {
     switch (err._tag) {
       case 'NotFound': return '404 - Not Found';
       case 'Unauthorized': return '401 - Unauthorized';
       case 'RateLimit': return `429 - Retry after ${err.retryAfterSeconds}s`;
     }
   }
   ```
   </details>

4. **JSON Serializable Base Error**: Implement an abstract class `BaseSerializableError` that automatically exposes `name`, `message`, `code`, and custom fields in its `.toJSON()` output.
   <details>
   <summary>Solution</summary>

   ```typescript
   abstract class BaseSerializableError extends Error {
     abstract readonly code: string;
     constructor(message: string, options?: ErrorOptions) {
       super(message, options);
       this.name = this.constructor.name;
       Object.setPrototypeOf(this, new.target.prototype);
     }

     toJSON(): Record<string, unknown> {
       return {
         name: this.name,
         message: this.message,
         code: this.code,
       };
     }
   }
   ```
   </details>

### Hard
5. **Nominally Branded Error Hierarchy**: Create two error classes with identical fields that cannot be assigned to each other by utilizing TypeScript unique symbols for nominal branding.
   <details>
   <summary>Solution</summary>

   ```typescript
   declare const CustomerErrorBrand: unique symbol;
   declare const VendorErrorBrand: unique symbol;

   class CustomerAccountError extends Error {
     declare readonly [CustomerErrorBrand]: true;
     constructor(public readonly accountId: string) {
       super(`Customer error: ${accountId}`);
       Object.setPrototypeOf(this, new.target.prototype);
     }
   }

   class VendorAccountError extends Error {
     declare readonly [VendorErrorBrand]: true;
     constructor(public readonly accountId: string) {
       super(`Vendor error: ${accountId}`);
       Object.setPrototypeOf(this, new.target.prototype);
     }
   }

   const c = new CustomerAccountError('123');
   // @ts-expect-error TS2322: Brands do not match
   const v: VendorAccountError = c;
   ```
   </details>

6. **Error Aggregator with Composite Pattern**: Implement an `AggregateErrorPolyfill` class that holds an array of child `Error` instances, flattens recursive sub-aggregates, formats a tree of error messages, and prunes V8 stack traces cleanly.
   <details>
   <summary>Solution</summary>

   ```typescript
   class CompositeError extends Error {
     public readonly errors: ReadonlyArray<Error>;

     constructor(errors: Iterable<Error>, message = 'Multiple errors occurred') {
       const flattened: Error[] = [];
       for (const err of errors) {
         if (err instanceof CompositeError) {
           flattened.push(...err.errors);
         } else {
           flattened.push(err);
         }
       }
       const combinedMsg = `${message}:\n` + flattened.map((e, i) => `  [${i + 1}] ${e.message}`).join('\n');
       super(combinedMsg);
       this.name = 'CompositeError';
       this.errors = flattened;
       Object.setPrototypeOf(this, new.target.prototype);
       if (Error.captureStackTrace) {
         Error.captureStackTrace(this, CompositeError);
       }
     }
   }
   ```
   </details>
