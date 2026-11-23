# Catching and Narrowing Errors in TypeScript

> Comprehensive guide to type-safe exception handling, `useUnknownInCatchVariables`, runtime error narrowing, structural inspection, and modern `Error.cause` chaining in TypeScript.

| Metadata | Details |
| :--- | :--- |
| **Category** | Error Handling |
| **Difficulty** | Intermediate |
| **Prerequisites** | [01-fundamentals/06-any-and-unknown.md](../01-fundamentals/06-any-and-unknown.md), [01-fundamentals/13-typeof-instanceof-in-quick-reference.md](../01-fundamentals/13-typeof-instanceof-in-quick-reference.md) |
| **TypeScript Baseline** | TS 4.0+ (`useUnknownInCatchVariables`), TS 4.6+ (`Error.cause`) |
| **Tested tsconfig** | `"strict": true, "useUnknownInCatchVariables": true, "target": "ES2022"` |

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
In JavaScript, any expression can be thrown: an instance of `Error`, a plain object, a string, a number, or even `undefined`. Historically, TypeScript typed the `catch (e)` binding as `any`. This led developers into a false sense of security, calling `e.message` or `e.stack` directly, which threw secondary unhandled `TypeError` exceptions at runtime whenever an unexpected primitive or custom rejection was thrown.

TypeScript 4.0 introduced the ability to explicitly annotate catch variables as `unknown` (`catch (e: unknown)`), and TypeScript 4.4 introduced the compiler flag `useUnknownInCatchVariables` (now included under `strict: true`). Under this flag, every caught error is strictly typed as `unknown`. This guarantees type safety by forcing engineers to perform defensive runtime type narrowing before reading properties or invoking methods on the error object.

---

## 2. Core Concepts
- **Catch Variable Typing**: Catch clauses only permit `any` or `unknown`. Type annotations like `catch (err: Error)` are illegal in TypeScript because the compiler cannot guarantee what will be thrown across runtime call stacks or asynchronous boundaries.
- **Narrowing with `instanceof`**: Verifies prototype chain linkage against class constructors such as `Error`, `TypeError`, or custom application error classes.
- **Narrowing with `in` and Property Checks**: Handles cross-realm errors (e.g. `iframe` or Node.js `vm` contexts where `instanceof Error` fails) by validating `typeof err === 'object' && err !== null && 'message' in err`.
- **Type Predicates**: User-defined type guards (e.g., `isError(x: unknown): x is Error`) that cleanly encapsulate structural and nominal validation logic.
- **Error Chaining with `cause`**: ES2022 and TypeScript 4.6+ support `new Error(msg, { cause: err })`, allowing high-level domain errors to wrap low-level system faults without losing stack traces or contextual diagnostic data.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------+
|                      throw expr;                          |
|  (Can be Error instance, string, number, null, or object) |
+-----------------------------+-----------------------------+
                              |
                              v
+-----------------------------------------------------------+
|                     catch (err: unknown)                  |
|          Type Checker prevents unsafe property access     |
+-----------------------------+-----------------------------+
                              |
              +---------------+---------------+
              |                               |
       instanceof Error?            typeof === 'string'?
              |                               |
              v (Yes)                         v (Yes)
   Narrowed to Error               Narrowed to string
   Safe: err.message               Safe: err.toUpperCase()
              |                               |
              +---------------+---------------+
                              |
                       No match: fallback
                     String(err) or unknown
```

When `useUnknownInCatchVariables` is enabled, the TypeScript Binder creates a local symbol for the catch identifier with the `TypeFlags.Unknown` type instead of `TypeFlags.Any`. The Type Checker enforces that no property access, method invocation, or arithmetic operation can occur on a type with `Unknown` flags unless narrowed by control-flow analysis (type guards, equality checks, or assertion functions).

---

## 4. Syntax & API Reference

```typescript
// 1. Catching with unknown
try {
  executeOperation();
} catch (error: unknown) {
  // error is unknown; direct access error.message fails compilation
}

// 2. Type guard definition
function isError(err: unknown): err is Error {
  return err instanceof Error;
}

// 3. Structural error guard (cross-realm safe)
interface AppErrorLike {
  message: string;
  name?: string;
  code?: string | number;
}

function isAppErrorLike(err: unknown): err is AppErrorLike {
  return (
    typeof err === 'object' &&
    err !== null &&
    'message' in err &&
    typeof (err as Record<string, unknown>).message === 'string'
  );
}

// 4. Modern Error constructor with cause (ES2022 / TS 4.6+)
const wrapped = new Error('Database transaction failed', { cause: originalError });
```

---

## 5. Practical Examples

### Example 1: Basic Catch and Narrowing
```typescript
function parseConfig(rawJson: string): Record<string, unknown> {
  try {
    return JSON.parse(rawJson) as Record<string, unknown>;
  } catch (error: unknown) {
    if (error instanceof SyntaxError) {
      console.error(`JSON syntax error: ${error.message}`);
    } else if (error instanceof Error) {
      console.error(`Generic error during parsing: ${error.message}`);
    } else {
      console.error(`Non-error thrown: ${String(error)}`);
    }
    return {};
  }
}
```

### Example 2: Real-World Network & Timeout Guard
```typescript
interface HttpErrorDetails {
  status: number;
  statusText: string;
  url: string;
}

class NetworkFetchError extends Error {
  public readonly details: HttpErrorDetails;
  constructor(message: string, details: HttpErrorDetails, options?: ErrorOptions) {
    super(message, options);
    this.name = 'NetworkFetchError';
    this.details = details;
  }
}

async function fetchUserData(userId: string): Promise<string> {
  try {
    const res = await fetch(`https://api.example.com/users/${userId}`);
    if (!res.ok) {
      throw new NetworkFetchError('Failed to fetch user', {
        status: res.status,
        statusText: res.statusText,
        url: res.url,
      });
    }
    return await res.text();
  } catch (err: unknown) {
    if (err instanceof NetworkFetchError) {
      return `Fallback user (HTTP ${err.details.status})`;
    }
    if (err instanceof Error && err.name === 'AbortError') {
      return 'Request timed out';
    }
    throw new Error('Unexpected user retrieval failure', { cause: err });
  }
}
```

### Example 3: Edge Case - Cross-Realm & Window/Worker Errors
```typescript
// When running in micro-frontends or iframes, instanceof Error fails
// across execution realms because the Error constructor prototype differs.
function getErrorMessage(err: unknown): string {
  if (err instanceof Error) {
    return err.message;
  }
  if (
    typeof err === 'object' &&
    err !== null &&
    'message' in err &&
    typeof (err as { message: unknown }).message === 'string'
  ) {
    return (err as { message: string }).message;
  }
  if (typeof err === 'string') {
    return err;
  }
  try {
    return JSON.stringify(err);
  } catch {
    return String(err);
  }
}
```

### Example 4: Compiler Failure Case with Illegal Catch Type
```typescript
// @ts-expect-error TS1196: Catch clause variable cannot have a type annotation other than 'any' or 'unknown'.
try {
  JSON.parse('{ invalid }');
} catch (e: Error) {
  console.log(e.message);
}
```

### Example 5: Idiomatic Error Normalizer Utility
```typescript
export function toError(maybeError: unknown): Error {
  if (maybeError instanceof Error) {
    return maybeError;
  }
  if (
    typeof maybeError === 'object' &&
    maybeError !== null &&
    'message' in maybeError &&
    typeof (maybeError as { message: unknown }).message === 'string'
  ) {
    const err = new Error((maybeError as { message: string }).message);
    if ('stack' in maybeError && typeof (maybeError as { stack: unknown }).stack === 'string') {
      err.stack = (maybeError as { stack: string }).stack;
    }
    return err;
  }
  return new Error(typeof maybeError === 'string' ? maybeError : JSON.stringify(maybeError));
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Throwing Non-Errors**: Third-party libraries or legacy code might throw strings, numbers, or promises. Code that assumes `error instanceof Error` will miss these completely if there is no fallback handler.
2. **Cross-Realm Prototype Poisoning**: If code evaluates inside a Node `vm` sandbox, worker thread, or browser `iframe`, `error instanceof Error` returns `false` because `window.Error !== iframe.contentWindow.Error`. Always use structural duck-typing for cross-realm resilience.
3. **Promise Rejection with Undefined**: `Promise.reject()` with no arguments passes `undefined` to the catch handler. Unconditional property access like `err.message` throws a `TypeError: Cannot read properties of undefined`.
4. **Error.cause Downleveling**: If compiling to `target: ES2021` or lower without polyfills, passing `{ cause: err }` to `new Error()` silently drops the cause property in older V8/JavaScript runtimes.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Compiler / Runtime Behavior | Correction |
| :--- | :--- | :--- |
| `catch (err: Error)` | **TS1196**: Catch clause variable cannot have type annotation other than 'any' or 'unknown' | Use `catch (err: unknown)` and narrow with `if (err instanceof Error)` |
| `catch (err: any) { err.message }` | **Unsafe**: Bypasses compiler safety; throws if `err` is undefined or string | Type as `unknown` and guard with a helper or `instanceof` |
| `catch (err) { err.code === 404 }` | **TS18046**: 'err' is of type 'unknown' | Check `typeof err === 'object' && err !== null && 'code' in err` |
| `throw "User not found"` | **Bad Practice**: Missing call stack, line numbers, and causes | Always throw `new Error('User not found')` or a custom error subclass |
| `catch (err) { throw err; }` without logging | Loses point of origin context | Wrap using `new Error('Operation failed', { cause: err })` |
| Assuming `instanceof Error` in iframes | Silently fails branch in cross-window execution | Implement structural checking for `.message` and `.name` |

---

## 8. Performance & Type-Checker Complexity
Narrowing with `instanceof` translates directly to the V8 `INSTANCEOF` bytecode instruction. It performs prototype chain walking, which is highly optimized by inline caches (monomorphic call sites take <1ns). Structural checks using `typeof` and `in` require simple dictionary checks.

From a TypeScript type-checker perspective:
- Narrowing `unknown` via control-flow analysis maintains a lightweight branching node in the FlowGraph.
- Complexity is $\mathcal{O}(1)$ for single conditions and $\mathcal{O}(N)$ for chained union checks.
- Avoid deeply nested property guards (e.g. checking 8 levels deep with `in`) as they cause excessive synthetic anonymous types during flow analysis.

---

## 9. Compiler Internals
Inside TypeScript's binder (`src/compiler/binder.ts`):
1. **Declaration Parsing**: `CatchClause` nodes parse a variable declaration with an optional type annotation.
2. **Type Enforcement**: `checkCatchClause` in `checker.ts` checks if the type syntax is `any` or `unknown`. Any other identifier triggers diagnostic `TS1196`.
3. **Implicit Unknown**: When `useUnknownInCatchVariables` is true, unannotated catch clauses receive `unknownType` rather than `anyType`.
4. **Control Flow Analysis**: The compiler creates a flow condition at the branch point. In `if (e instanceof Error)`, `createFlowCondition` associates the true branch with the intersection `unknown & Error` which immediately simplifies to `Error`.

---

## 10. Best Practices & Design Guidelines
- Enable `"useUnknownInCatchVariables": true` (or `"strict": true`) in all production projects.
- Never write `catch (e: any)` to silence the type checker.
- Create a centralized `getErrorMessage(error: unknown): string` utility in your shared toolkit.
- Standardize on `Error.cause` for wrapping errors to preserve the complete causal debugging chain.
- If handling third-party RPC or HTTP rejections, validate using Zod or custom type guards rather than type assertions (`as AxiosError`).

---

## 11. Real-World Case Study
An enterprise observability gateway ingested errors from external micro-frontends and Node.js plugins. Uncaught strings (`throw "db disconnected"`) and third-party SDK errors without prototypes crashed the crash-reporter itself due to naive `report(error.message)` calls.

**Refactored Production Error Ingestion:**
```typescript
export interface SanitizedErrorInfo {
  readonly message: string;
  readonly name: string;
  readonly stack?: string;
  readonly cause?: unknown;
}

export function sanitizeError(caught: unknown): SanitizedErrorInfo {
  if (caught instanceof Error) {
    return {
      message: caught.message,
      name: caught.name,
      stack: caught.stack,
      cause: 'cause' in caught ? caught.cause : undefined,
    };
  }

  if (typeof caught === 'string') {
    return {
      message: caught,
      name: 'ThrownStringError',
    };
  }

  if (typeof caught === 'object' && caught !== null) {
    const record = caught as Record<string, unknown>;
    const message = typeof record.message === 'string' ? record.message : 'Unknown object error';
    const name = typeof record.name === 'string' ? record.name : 'ObjectError';
    return {
      message,
      name,
      stack: typeof record.stack === 'string' ? record.stack : undefined,
    };
  }

  return {
    message: String(caught),
    name: 'PrimitiveError',
  };
}
```

---

## 12. Interview Questions

<details>
<summary>1. Why does TypeScript disallow <code>catch (e: MyCustomError)</code>?</summary>

JavaScript's runtime semantics allow any function in the call stack to throw any arbitrary value (strings, numbers, objects, or errors from external libraries). The TypeScript compiler cannot statically verify every possible exception that could propagate through synchronous or asynchronous call boundaries. Allowing `catch (e: MyCustomError)` would provide false compile-time safety when an unrelated exception (like an `OutOfMemoryError` or `TypeError`) is actually thrown.
</details>

<details>
<summary>2. What is the difference between <code>catch (e: any)</code> and <code>catch (e: unknown)</code>?</summary>

With `any`, the compiler disables all type checking on `e`. You can access arbitrary properties (`e.nonExistentProp.doSomething()`) without compiler warnings, which can lead to runtime crashes. With `unknown`, the compiler forces you to check and narrow the type before reading properties, guaranteeing runtime safety.
</details>

<details>
<summary>3. Why might <code>err instanceof Error</code> evaluate to <code>false</code> even for a legitimate Error object?</summary>

In JavaScript, `instanceof` checks prototype identity against the current execution context's constructor. In multi-realm environments (such as browser iframes, Node.js `vm` sandboxes, or worker threads), the `Error` constructor of the foreign realm is a distinct object in memory. An error created in an iframe will fail `instanceof window.Error`.
</details>

<details>
<summary>4. What flag enables <code>unknown</code> for catch variables automatically?</summary>

`useUnknownInCatchVariables` (introduced in TS 4.4), which is automatically turned on when `"strict": true` is configured in `tsconfig.json`.
</details>

<details>
<summary>5. How does ES2022 <code>Error.cause</code> improve error diagnostics?</summary>

It provides a native, standardized property on the `Error` instance to hold the original low-level exception that triggered the high-level error. This preserves the original stack trace and error properties while allowing applications to report clean, domain-specific error messages.
</details>

<details>
<summary>6. How do you implement a type guard to verify if an unknown error has a specific status code?</summary>

```typescript
interface HttpError {
  status: number;
}
function isHttpError(err: unknown): err is HttpError {
  return (
    typeof err === 'object' &&
    err !== null &&
    'status' in err &&
    typeof (err as Record<string, unknown>).status === 'number'
  );
}
```
</details>

<details>
<summary>7. What happens if an unhandled promise rejects with <code>undefined</code>?</summary>

In Node.js or browser devtools, an unhandled rejection event is emitted with reason `undefined`. If a handler attempts `reason.message`, a runtime `TypeError` occurs. Narrowing with `typeof reason === 'object' && reason !== null` safely avoids this crash.
</details>

<details>
<summary>8. Can assertion functions (<code>asserts condition</code>) be used in catch blocks?</summary>

Yes. You can write `function assertIsError(err: unknown): asserts err is Error` and call it at the top of your catch block. If the check passes, the remainder of the catch block treats `err` as `Error`. If it fails, the assertion function throws an error.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Safe Error Message Extractor**: Write a function `safeErrorMessage(err: unknown): string` that returns `err.message` if `err` is an `Error`, the string itself if `err` is a string, and `"Unknown error"` otherwise.
   <details>
   <summary>Solution</summary>

   ```typescript
   function safeErrorMessage(err: unknown): string {
     if (err instanceof Error) return err.message;
     if (typeof err === 'string') return err;
     return 'Unknown error';
   }
   ```
   </details>

2. **Primitive Throw Detector**: Write a type guard `isPrimitiveThrow(x: unknown): x is string | number | boolean` that returns true if someone threw a primitive value.
   <details>
   <summary>Solution</summary>

   ```typescript
   function isPrimitiveThrow(x: unknown): x is string | number | boolean {
     const t = typeof x;
     return t === 'string' || t === 'number' || t === 'boolean';
   }
   ```
   </details>

### Medium
3. **Cross-Realm Error Guard**: Write a comprehensive type predicate `isAnyError(err: unknown): err is { name: string; message: string; stack?: string }` that correctly validates errors created in foreign iframes without relying on `instanceof`.
   <details>
   <summary>Solution</summary>

   ```typescript
   interface AnyError {
     name: string;
     message: string;
     stack?: string;
   }

   function isAnyError(err: unknown): err is AnyError {
     if (typeof err !== 'object' || err === null) return false;
     const rec = err as Record<string, unknown>;
     return (
       typeof rec.name === 'string' &&
       typeof rec.message === 'string' &&
       (rec.stack === undefined || typeof rec.stack === 'string')
     );
   }
   ```
   </details>

4. **Nested Cause Explorer**: Write a function `getCompleteErrorChain(err: unknown): string[]` that traverses the `cause` chain of an error object and returns an array of all error messages from top-level to root cause.
   <details>
   <summary>Solution</summary>

   ```typescript
   function getCompleteErrorChain(err: unknown): string[] {
     const messages: string[] = [];
     let current: unknown = err;

     while (current) {
       if (current instanceof Error) {
         messages.push(current.message);
         current = current.cause;
       } else if (typeof current === 'object' && current !== null && 'message' in current) {
         messages.push(String((current as { message: unknown }).message));
         current = 'cause' in current ? (current as { cause: unknown }).cause : undefined;
       } else {
         messages.push(String(current));
         break;
       }
     }
     return messages;
   }
   ```
   </details>

### Hard
5. **Type-Safe Try-Catch Wrapper**: Write a higher-order wrapper `tryCatch<T, E = Error>(fn: () => T, mapError: (err: unknown) => E): { ok: true; data: T } | { ok: false; error: E }` that never throws and always returns a strongly-typed discriminated result.
   <details>
   <summary>Solution</summary>

   ```typescript
   type TryResult<T, E> =
     | { readonly ok: true; readonly data: T }
     | { readonly ok: false; readonly error: E };

   function tryCatch<T, E = Error>(
     fn: () => T,
     mapError: (err: unknown) => E
   ): TryResult<T, E> {
     try {
       return { ok: true, data: fn() };
     } catch (err: unknown) {
       return { ok: false, error: mapError(err) };
     }
   }
   ```
   </details>

6. **Async Error Guard Pipeline with Custom Assertion**: Build an asynchronous runner `safeAsync<T>(promise: Promise<T>): Promise<[Error, null] | [null, T]>` that unpacks promises into Go-style error tuples while ensuring unhandled exceptions in the promise reject handler are converted into standardized `Error` instances.
   <details>
   <summary>Solution</summary>

   ```typescript
   async function safeAsync<T>(
     promise: Promise<T>
   ): Promise<[Error, null] | [null, T]> {
     try {
       const data = await promise;
       return [null, data];
     } catch (err: unknown) {
       const normalized = err instanceof Error ? err : new Error(String(err));
       return [normalized, null];
     }
   }
   ```
   </details>
