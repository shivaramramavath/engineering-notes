# Parameters and Return Types

> Function parameters and return types govern input validation, optionality, default values, rest tuples, and control-flow return narrowing across synchronous and asynchronous execution boundaries.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-function-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/00-function-types.md), [08-null-and-undefined.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/08-null-and-undefined.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 3.0 (Tuple Rest) / TS 4.4 / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitReturns: true`, `exactOptionalPropertyTypes: true` |

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
- **Problem Statement:** In JavaScript, any function can be called with any number of arguments, regardless of parameter declarations. Missing arguments become `undefined`, and excess arguments are ignored silently. Return statements can be omitted, causing functions to unexpectedly return `undefined`.
- **Historical Context:** TypeScript enforces strict parameter count and type assignability. Over time, TypeScript added optional parameters (`x?:`), default value inference, rest parameters (`...args`), tuple rest parameters (TS 3.0), and return type analysis via `noImplicitReturns`.
- **Real-World Relevance:** Explicit parameter and return contracts prevent runtime `TypeError` bugs, eliminate unexpected `undefined` data leakage, and ensure that refactored functions immediately flag broken call sites across large enterprise codebases.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Required Parameters: Must precede optional parameters in positional parameter lists.
  - Optional Parameters: Marked with `?:`. Their type automatically includes `| undefined`.
  - Default Parameters: Parameters with initializers (`x = 10`). Inferred as non-nullable within the function body, while remaining optional for callers.
  - Rest Parameters: Must be the final parameter. Typed as an array (`...rest: string[]`) or fixed-length/labeled tuple (`...args: [string, number]`).
  - Return Type Inference: If unannotated, TypeScript infers the return type from all `return` expressions in the function body.
- **Mental Model:** Think of the parameter list as a tuple schema being validated upon entry, and the return type as a promise made to the caller.
- **Soundness vs Ergonomics:** With `noImplicitReturns: true`, the compiler guarantees that every code branch returns an explicit value, preventing accidental fallthrough.

---

## 3. How It Works Internally
- **Signature Construction:** The compiler parses parameters into `ts.ParameterDeclaration` nodes. If a parameter has a question mark or initializer, it is flagged with `SymbolFlags.Optional`.
- **Control Flow Analysis on Returns:** CFA builds an execution graph from each entry point to every exit. If an exit path terminates without a `return` statement (and the return type is not `void` or `any`), the compiler raises `TS2366`.
- **Architectural Flow Diagram:**

```
+-------------------------------------------------------------------------+
|                  Parameter & Return Validation Pipeline                 |
|                                                                         |
|   Caller Invocation: fn("hello", 42)                                    |
|          │                                                              |
|          ▼                                                              |
|   [ Type Checker: Parameter Assignability ]                             |
|   ├── Param 0: "hello" matches string                                   |
|   ├── Param 1: 42 matches number                                        |
|   └── Check: Arity <= MaxParams && Arity >= MinRequiredParams           |
|          │                                                              |
|          ▼                                                              |
|   [ Body Execution Analysis (CFA) ]                                     |
|   ├── Branch 1: returns boolean                                         |
|   ├── Branch 2: returns boolean                                         |
|   └── Evaluates: Union of return types === declared ReturnType          |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Syntax Forms:**
  ```typescript
  function execute(
    required: string,
    optional?: number,
    defaulted: boolean = false,
    ...rest: string[]
  ): Promise<string> {
    // ...
  }
  ```
- **Tuple Rest Parameters:**
  ```typescript
  function coordinate(...coords: [x: number, y: number, z?: number]): void {}
  ```
- **Utility Types:**
  - `Parameters<T>`: Extracts parameter types as a tuple.
  - `ReturnType<T>`: Extracts the return type of a function.
- **tsconfig Dependencies:**
  - `noImplicitReturns: true`: Ensures all code branches return a value.
  - `noImplicitAny: true`: Prevents parameters from defaulting to `any`.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compare optional parameters with defaulted parameters.
- **Code:**
```typescript
function greetUser(name: string, title?: string, greeting: string = 'Hello'): string {
  // Inside the function, 'greeting' is guaranteed to be string (not undefined!)
  // 'title' is string | undefined
  const prefix = title ? `${title} ` : '';
  return `${greeting}, ${prefix}${name}!`;
}

console.log(greetUser('Alice'));                       // "Hello, Alice!"
console.log(greetUser('Bob', 'Dr.'));                  // "Hello, Dr. Bob!"
console.log(greetUser('Charlie', undefined, 'Welcome')); // "Welcome, Charlie!"
```
- **Output / Type Evaluation:**
```typescript
// Returns verified string; greeting defaults safely when omitted or undefined.
```
- **Explanation:** Default parameters provide fallback values while maintaining strict types inside the body.

### Example 2: Real-World Enterprise Pattern (Type-Safe Dispatcher with Tuple Rest)
- **Goal:** Model a type-safe RPC dispatcher where the method name selects the exact argument tuple.
- **Code:**
```typescript
type RPCCommands = {
  createUser: [name: string, email: string];
  deleteUser: [userId: string];
  updateRole: [userId: string, role: 'admin' | 'user'];
};

export class RPCClient {
  call<K extends keyof RPCCommands>(
    command: K,
    ...args: RPCCommands[K]
  ): Promise<{ status: 'ok' }> {
    console.log(`Dispatching ${command} with ${args.length} arguments`);
    return Promise.resolve({ status: 'ok' });
  }
}

const client = new RPCClient();
client.call('createUser', 'Alice', 'alice@corp.com'); // OK!
client.call('deleteUser', 'usr_991');                 // OK!
```
- **Output / Type Evaluation:**
```typescript
// Exact parameter count and types enforced per command!
```
- **Explanation:** Tuple rest parameters (`...args: RPCCommands[K]`) dynamically bind argument signatures to the chosen command name.

### Example 3: Edge Case / Boundary Condition (Destructured Parameters with Defaults)
- **Goal:** Type complex destructured options objects with safe defaults.
- **Code:**
```typescript
type RequestConfig = {
  retries?: number;
  timeoutMs?: number;
  verbose?: boolean;
};

function sendRequest(
  url: string,
  { retries = 3, timeoutMs = 5000, verbose = false }: RequestConfig = {}
): void {
  // retries, timeoutMs, verbose are all narrowed to number and boolean!
  console.log(`Fetching ${url} (retries: ${retries}, timeout: ${timeoutMs}ms)`);
}

sendRequest('https://api.example.com'); // Valid: defaults to {}
sendRequest('https://api.example.com', { retries: 5 }); // Valid
```
- **Output / Type Evaluation:**
```typescript
// Fully typed destructured properties with zero runtime undefined hazards.
```
- **Explanation:** Setting `= {}` on the destructured argument allows callers to omit the options object entirely.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler error on missing return branch or argument count mismatch.
- **Code:**
```typescript
// tsconfig: { "noImplicitReturns": true }
function parseStatus(code: number): string {
  if (code === 200) {
    return 'OK';
  } else if (code === 404) {
    return 'Not Found';
  }
  // Missing return statement for other codes!
  // @ts-expect-error TS2366: Function lacks ending return statement and return type does not include 'undefined'.
}

function add(a: number, b: number): number {
  return a + b;
}

// @ts-expect-error TS2554: Expected 2 arguments, but got 1.
add(10);
```
- **Real Compiler Error:**
```
error TS2366: Function lacks ending return statement and return type does not include 'undefined'.
error TS2554: Expected 2 arguments, but got 1.
```
- **Explanation:** TypeScript strictly validates both caller arity and callee exhaustive returns.

### Example 5: Idiomatic Modern Usage (Inferring Types via Parameters and ReturnType)
- **Goal:** Intercept and log third-party functions dynamically using utility types.
- **Code:**
```typescript
function fetchUserData(userId: string, includeHistory: boolean): { id: string; active: boolean } {
  return { id: userId, active: true };
}

type FetchParams = Parameters<typeof fetchUserData>;
// [userId: string, includeHistory: boolean]

type FetchResult = ReturnType<typeof fetchUserData>;
// { id: string; active: boolean }

export function withLogging<Fn extends (...args: any[]) => any>(
  fn: Fn
): (...args: Parameters<Fn>) => ReturnType<Fn> {
  return (...args: Parameters<Fn>): ReturnType<Fn> => {
    console.log('Invocation arguments:', args);
    const result = fn(...args);
    console.log('Invocation result:', result);
    return result;
  };
}

const loggedFetch = withLogging(fetchUserData);
loggedFetch('usr_123', true);
```
- **Output / Type Evaluation:**
```typescript
// Preserves exact parameter types and return type dynamically.
```
- **Explanation:** `Parameters<T>` and `ReturnType<T>` enable high-order decorators that preserve exact function signatures without manual duplication.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Optional Parameters Ordering:** An optional parameter cannot precede a required parameter (`function(a?: string, b: number)` is a compile error `TS1016: A required parameter cannot follow an optional parameter`).
2. **Explicit `undefined` with Default Parameters:** Passing `undefined` explicitly triggers the default initializer: `fn(undefined)` uses the default value. Passing `null` does NOT trigger the default; it passes `null`.
3. **Async Functions Always Return Promises:** An async function annotated with `: string` produces `TS1055: The return type of an async function or method must be the global Promise<T> type`. Always annotate async functions with `Promise<T>`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Omitting return type on complex recursive functions | `TS7022: implicitly has return type 'any'` | Annotate return type explicitly | Prevents recursive inference bailout |
| Annotating async function with non-Promise return | `TS1055: Return type must be Promise<T>` | `: Promise<MyType>` | Aligns with JavaScript runtime Promises |
| Placing optional parameter before required | `TS1016: Required parameter cannot follow optional` | Move optional parameters to the end | Enforces positional argument alignment |
| Assuming `null` triggers default parameter | `null` overrides default; crashes on property read | Pass `undefined` or use nullish check | JavaScript defaults only trigger on `undefined` |
| Missing return on branch under `noImplicitReturns` | `TS2366: Function lacks ending return statement` | Add explicit return or throw | Eliminates silent `undefined` return bugs |
| Typing rest parameter as `any` | Permitted to pass invalid arguments | Type as `...args: KnownType[]` | Enforces rest parameter validation |

---

## 8. Performance & Type-Checker Complexity
- **Return Type Inference Overhead:** For complex conditional expressions, the type checker computes the union of all return paths. Explicit return annotations avoid this calculation and speed up type checking.
- **Tuple Rest Spreading:** Destructuring tuple rest parameters requires tuple length validation in the checker ($O(N)$ operations).
- **Isolated Declarations:** Exported functions MUST have explicit return type annotations under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Representation:**
  - Parameters: `ts.ParameterDeclaration` (`SyntaxKind.Parameter`).
  - Return type: `type` property on `ts.FunctionDeclaration`.
- **Checker Resolution:** In `checker.ts`, `getReturnTypeOfSignature` evaluates the return type. If inferred, it traverses the function body collecting all `ReturnStatement` nodes.
- **Diagnostic Generation:** Flags `Diagnostics.Function_lacks_ending_return_statement` (TS2366) when any control path exits without a value.

---

## 10. Best Practices & Design Guidelines
- **Explicitly Annotate Return Types for Exported Functions:** Speeds up compilation, satisfies `--isolatedDeclarations`, and prevents unintentional return type regressions.
- **Use Default Parameters instead of Optional + Null Checks:** Simplifies function bodies by guaranteeing non-nullish parameter values.
- **Leverage Labeled Tuple Elements for Rest Parameters:** `...coords: [x: number, y: number]` provides clear documentation in IDE tooltips.

---

## 11. Real-World Case Study
- **Scenario:** A financial trading gateway executed buy and sell orders. An order placement function lacked an explicit return type: `async function placeOrder(order: Order) { ... }`. A junior developer refactored a branch to return early with `return;` instead of `return { orderId, status: 'FILLED' }`. The inferred return type silently widened to `Promise<OrderResult | undefined>`. Callers expecting an order object crashed with `Cannot read properties of undefined (reading 'orderId')`, losing $310,000 in unconfirmed trades.
- **Implementation:** Enabled `noImplicitReturns: true` and enforced explicit return annotations across all trading services:
  ```typescript
  // Enforced explicit return signature:
  async function placeOrder(order: Order): Promise<OrderResult> {
    if (!order.isValid) {
      throw new InvalidOrderError('Validation failed');
    }
    return executeOnExchange(order);
  }
  ```
- **Outcome:** The compiler caught the missing return branch immediately, permanently eliminating unhandled `undefined` returns in mission-critical trading flows.

---

## 12. Interview Questions

### Q1: How does JavaScript treat passing `null` vs `undefined` to a function parameter with a default value?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, default parameter values are ONLY triggered when the argument is `undefined` (or omitted). If `null` is passed (`fn(null)`), `null` is treated as an explicit, valid value; the default initializer is NOT evaluated, and the parameter receives `null`. In TypeScript, if the parameter is typed as `x: string = "default"`, passing `null` raises a compile error `TS2345: Argument of type 'null' is not assignable to parameter of type 'string | undefined'`.
</details>

### Q2: Why should exported library functions always have explicit return type annotations?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

1. **Performance:** Without explicit return types, the TypeScript type checker must inspect the entire function body and all `return` statements to synthesize the return type. With explicit types, the compiler verifies the body in isolation.
2. **Contract Preservation:** Inferred return types can accidentally widen or change when internal implementation details change, breaking downstream consumers without compiler warnings in the library itself.
3. **Fast Tooling Compatibility:** Under `--isolatedDeclarations` (TS 5.5+), exported functions must have explicit return types so fast transpilers (like `tsgo`, `esbuild`, or `swc`) can emit `.d.ts` files without full type inference.
</details>

### Q3: What is a tuple rest parameter and how does it improve on standard rest arrays?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A standard rest parameter (`...args: number[]`) accepts an arbitrary number of elements of that type. A tuple rest parameter (`...args: [string, number, boolean?]`) binds the rest operator to a fixed-length, typed tuple schema. This allows enforcing exact positional argument requirements, optional elements, and labeled parameters in IDE autocomplete without declaring rigid positional names.
</details>

### Q4: What does `noImplicitReturns` enforce in `tsconfig.json`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

When `noImplicitReturns: true` is enabled, the compiler checks that every possible execution branch in a function with a declared return type returns an explicit value. If any code path falls off the end of the function body without a `return` statement, the compiler emits `TS2366: Function lacks ending return statement`.
</details>

### Q5: How does `Parameters<T>` extract parameter types using conditional types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`Parameters<T>` is defined in the standard library as:
```typescript
type Parameters<T extends (...args: any) => any> =
  T extends (...args: infer P) => any ? P : never;
```
It uses pattern matching via `infer P` to capture the parameter list as a tuple type.
</details>

### Q6: Can a required parameter follow an optional parameter?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. In positional parameter lists, required parameters cannot follow optional parameters. Writing `function(a?: string, b: number)` produces `TS1016: A required parameter cannot follow an optional parameter`. To make earlier parameters optional while keeping later ones required, wrap the parameters in an options object.
</details>

### Q7: What is the return type of a function that only throws an error?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The return type is `never`. Because the function never returns control to the caller (it terminates abnormally by throwing an exception), its return type represents the empty set ($emptyset$).
</details>

### Q8: How do destructured parameter defaults interact with overall argument optionality?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

If a function defines destructured parameter defaults (`function({ a = 1, b = 2 }: Options)`), callers are still required to pass an argument (`fn({})`). To allow callers to omit the argument entirely (`fn()`), the parameter itself must be given a default value: `function({ a = 1, b = 2 }: Options = {})`.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Default Parameter Multiplier (Easy)
- **Problem Statement:** Write a function `scale(value: number, factor: number = 2): number` that scales a value by a factor, defaulting to 2.
- **Test Case:**
```typescript
scale(5); // 10
scale(5, 3); // 15
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function scale(value: number, factor: number = 2): number {
  return value * factor;
}
```
</details>

### Exercise 2: Tuple Rest Coordinate Formatter (Easy)
- **Problem Statement:** Write a function `formatCoords(...point: [x: number, y: number, z?: number]): string` that formats 2D or 3D coordinates.
- **Test Case:**
```typescript
formatCoords(10, 20); // "(10, 20)"
formatCoords(10, 20, 30); // "(10, 20, 30)"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function formatCoords(...point: [x: number, y: number, z?: number]): string {
  const [x, y, z] = point;
  return z !== undefined ? `(${x}, ${y}, ${z})` : `(${x}, ${y})`;
}
```
</details>

### Exercise 3: Strict Exhaustive Return Branching (Medium)
- **Problem Statement:** Write a function `getHttpCategory(status: number): 'SUCCESS' | 'CLIENT_ERROR' | 'SERVER_ERROR'` that handles 2xx, 4xx, and 5xx, throwing on unknown codes with full exhaustiveness.
- **Test Case:**
```typescript
getHttpCategory(200); // "SUCCESS"
getHttpCategory(404); // "CLIENT_ERROR"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function getHttpCategory(status: number): 'SUCCESS' | 'CLIENT_ERROR' | 'SERVER_ERROR' {
  if (status >= 200 && status < 300) {
    return 'SUCCESS';
  } else if (status >= 400 && status < 500) {
    return 'CLIENT_ERROR';
  } else if (status >= 500 && status < 600) {
    return 'SERVER_ERROR';
  }
  throw new Error(`Unsupported HTTP status: ${status}`);
}
```
</details>

### Exercise 4: Dynamic Curried Invoker (Medium)
- **Problem Statement:** Write a function `curry<A, B, R>(fn: (a: A, b: B) => R): (a: A) => (b: B) => R` preserving strict parameter and return types.
- **Test Case:**
```typescript
const add = (a: number, b: number) => a + b;
const curried = curry(add);
curried(2)(3); // 5
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function curry<A, B, R>(fn: (a: A, b: B) => R): (a: A) => (b: B) => R {
  return (a: A) => (b: B) => fn(a, b);
}
```
</details>

### Exercise 5: Extract First Parameter Type Utility (Hard)
- **Problem Statement:** Author a utility type `FirstParameter<T>` that extracts the type of the first parameter of any function, or `never` if the function accepts zero arguments.
- **Test Case:**
```typescript
type F1 = (name: string, age: number) => void;
type P1 = FirstParameter<F1>; // string
type F0 = () => void;
type P0 = FirstParameter<F0>; // never
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type FirstParameter<T extends (...args: any[]) => any> =
  Parameters<T> extends [infer First, ...any[]] ? First : never;
```
</details>

### Exercise 6: Tail Arguments Extractor Utility (Hard)
- **Problem Statement:** Write a utility type `TailParameters<T>` that returns a tuple of all parameters except the first.
- **Test Case:**
```typescript
type F = (id: string, name: string, active: boolean) => void;
type Tail = TailParameters<F>; // [name: string, active: boolean]
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type TailParameters<T extends (...args: any[]) => any> =
  Parameters<T> extends [any, ...infer Rest] ? Rest : [];
```
</details>
