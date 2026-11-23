# Function and Class Utilities

> Function and class type utilities extract signatures, parameter tuples, return types, and instance shapes directly from executable constructors and functions using type-level inference.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-function-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/00-function-types.md), [05-generic-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/05-generic-inference.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.8 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitThis: true` |

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
JavaScript architectures often rely on higher-order patterns: wrapping functions with decorators, creating factory functions, writing dependency injection containers, and creating generic middleware.

Manually writing types for wrapped functions or class factories creates brittle boilerplate. If a service constructor changes its arguments, all factory signatures break unless they are dynamically linked.

TypeScript provides standard utilities to inspect and destructure callable signatures:
- **`Parameters<T>`**: Extracts parameter types as a tuple.
- **`ReturnType<T>`**: Extracts the return type.
- **`ConstructorParameters<T>`**: Extracts constructor parameters as a tuple.
- **`InstanceType<T>`**: Extracts the instance type produced by a constructor.
- **`ThisParameterType<T>`** & **`OmitThisParameter<T>`**: Inspects and strips explicit `this` contexts.

---

## 2. Core Concepts

### `Parameters<T>` and `ReturnType<T>`
These inspect call signatures:
```typescript
function registerUser(name: string, age: number, isAdmin = false): { id: string; success: boolean } {
  return { id: "u_1", success: true };
}

type UserParams = Parameters<typeof registerUser>; // [name: string, age: number, isAdmin?: boolean]
type UserReturn = ReturnType<typeof registerUser>; // { id: string; success: boolean }
```

### `ConstructorParameters<T>` and `InstanceType<T>`
These inspect construct signatures:
```typescript
class DatabaseConnection {
  constructor(public url: string, public timeoutMs: number) {}
}

type DbArgs = ConstructorParameters<typeof DatabaseConnection>; // [url: string, timeoutMs: number]
type DbInstance = InstanceType<typeof DatabaseConnection>;       // DatabaseConnection
```

### `ThisParameterType<T>` and `OmitThisParameter<T>`
Manage explicit `this` bindings:
```typescript
function toHex(this: Number): string {
  return this.toString(16);
}

type ThisCtx = ThisParameterType<typeof toHex>; // Number
type CleanFn = OmitThisParameter<typeof toHex>;  // () => string
```

---

## 3. How It Works Internally

```
Type Definitions in lib.es5.d.ts:

type Parameters<T extends (...args: any) => any> =
  T extends (...args: infer P) => any ? P : never;

type ReturnType<T extends (...args: any) => any> =
  T extends (...args: any) => infer R ? R : any;

type ConstructorParameters<T extends abstract new (...args: any) => any> =
  T extends abstract new (...args: infer P) => any ? P : never;

type InstanceType<T extends abstract new (...args: any) => any> =
  T extends abstract new (...args: any) => infer R ? any : never;
```

Mechanics:
1. Each utility constrains `T` to a generic callable or constructable type.
2. In the `extends` condition, `infer` captures the target component (parameters tuple `infer P` or return/instance `infer R`).
3. Since parameters appear in contravariant position, `infer P` preserves labeled tuple elements.

---

## 4. Syntax & API Reference

### Function Signatures vs. Value Identifiers
Remember that `Parameters` and `ReturnType` require a **type**. You must pass `typeof fn`, not `fn`:
```typescript
function calculate(x: number): number { return x * 2; }

// Correct:
type CalcParams = Parameters<typeof calculate>;

// @ts-expect-error TS2749: 'calculate' refers to a value, but is being used as a type here.
type BadParams = Parameters<calculate>;
```

### Handling Abstract Constructors
Starting in TypeScript 4.2, `ConstructorParameters` and `InstanceType` accept `abstract new (...args: any) => any`, allowing them to inspect abstract classes without errors.

---

## 5. Practical Examples

### 5.1 Basic Function Wrapping with Forwarded Parameters
```typescript
function logExecution<T extends (...args: any[]) => any>(fn: T) {
  return (...args: Parameters<T>): ReturnType<T> => {
    console.log("Calling with args:", args);
    const result = fn(...args);
    console.log("Returned:", result);
    return result;
  };
}

const add = (a: number, b: number) => a + b;
const loggedAdd = logExecution(add);
loggedAdd(5, 10);
```

### 5.2 Real-World Application: Generic Class Factory & DI Container
```typescript
type Constructor<T = any> = abstract new (...args: any[]) => T;

class DependencyContainer {
  private services = new Map<Constructor, any>();

  register<T extends Constructor>(
    token: T,
    factory: (...args: ConstructorParameters<T>) => InstanceType<T>
  ): void {
    this.services.set(token, factory);
  }

  resolve<T extends Constructor>(token: T, ...args: ConstructorParameters<T>): InstanceType<T> {
    const factory = this.services.get(token);
    if (!factory) throw new Error("Service not registered");
    return factory(...args);
  }
}

class AuthService {
  constructor(public apiUrl: string) {}
}

const container = new DependencyContainer();
container.register(AuthService, (url) => new AuthService(url));
const auth = container.resolve(AuthService, "https://auth.internal");
```

### 5.3 Edge Case: Overloaded Functions
```typescript
function format(val: string): string;
function format(val: number): number;
function format(val: string | number): string | number {
  return val;
}

// In TypeScript, ReturnType on overloaded functions matches the LAST overload signature!
type FormatReturn = ReturnType<typeof format>; // number
type FormatParams = Parameters<typeof format>; // [val: number]
```

### 5.4 Failure Case: Passing Non-Function to `Parameters`
```typescript
const nonFunction = { a: 1 };

// @ts-expect-error TS2344: Type '{ a: number; }' does not satisfy the constraint '(...args: any) => any'.
type Invalid = Parameters<typeof nonFunction>;
```

### 5.5 Idiomatic Pattern: Method Decorator Wrapper
```typescript
function MeasurePerf<T extends (...args: any[]) => Promise<any>>(
  target: any,
  propertyKey: string,
  descriptor: TypedPropertyDescriptor<T>
): TypedPropertyDescriptor<T> {
  const original = descriptor.value!;
  descriptor.value = (async function (this: any, ...args: Parameters<T>): Promise<Awaited<ReturnType<T>>> {
    const start = performance.now();
    const result = await original.apply(this, args);
    console.log(`Execution of ${propertyKey}: ${performance.now() - start}ms`);
    return result;
  }) as T;
  return descriptor;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Overloads**: `ReturnType` and `Parameters` only inspect the **last** overload declared in source order. They do not represent a union of all overloads.
2. **`any` Argument**: Passing `Parameters<any>` evaluates to `unknown[]`. Passing `ReturnType<any>` evaluates to `any`.
3. **`never` Argument**: Evaluates to `never` due to conditional type distribution.
4. **Tuple Labels**: Parameter names in `Parameters<T>` are preserved as labeled tuple elements (e.g. `[id: string, name: string]`), aiding IDE autocompletion.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Forgetting `typeof` on runtime functions | Passes runtime identifier instead of type | Write `ReturnType<typeof myFn>` | `TS2749: 'myFn' refers to a value...` |
| Relying on `ReturnType` for overloaded functions | Only returns type of the last declared overload | Use individual overloads or explicit generic wrappers | Incomplete return type |
| Using `InstanceType` on instance instead of constructor | Instance is already instantiated | Pass `typeof MyClass` to `InstanceType` | `TS2344: Type does not satisfy constraint` |
| Dropping `this` parameter context | Binding `this` implicitly causes runtime bugs | Use `ThisParameterType` and `bind` | `TS2684: The 'this' context of type...` |
| Typing args as `any[]` in wrappers | Disables parameter checking | Forward `Parameters<T>` | Loss of argument type safety |
| Using `Function` as generic constraint | Too loose; permits non-callables in older tsconfigs | Constrain to `(...args: any[]) => any` | Unsheltered invocation errors |

---

## 8. Performance & Type-Checker Complexity
- Parameter and return type extractions execute in constant time $O(1)$ via direct symbol table inspection of call signatures.
- Wrapping many nested higher-order functions increases depth of AST expressions; keep wrapper chains shallow.

---

## 9. Compiler Internals
- `Parameters` and `ReturnType` are defined in `lib.es5.d.ts`.
- Evaluated in `src/compiler/checker.ts` through `getTypeFromConditionalType`.
- `getSignaturesOfType(type, SignatureKind.Call)` yields the list of call signatures. The compiler extracts parameters and return types from the last signature in that list.

---

## 10. Best Practices & Design Guidelines
- Always use `Parameters<T>` and `ReturnType<T>` when authoring function proxies, throttles, debouncers, or memoizers.
- Use `InstanceType<typeof MyClass>` in factory methods to dynamically track subclasses.
- If a function has multiple overloads, prefer explicitly specifying generic types rather than relying on `ReturnType`.

---

## 11. Real-World Case Study
### Generic API Proxy Client with Typed Forwarding
```typescript
const backendApi = {
  getUser: (id: string) => Promise.resolve({ id, name: "Alice" }),
  deleteUser: (id: string, force: boolean) => Promise.resolve({ ok: true }),
  updateRole: (id: string, role: "admin" | "user") => Promise.resolve({ success: true }),
};

type ApiContract = typeof backendApi;

function createApiProxy<T extends Record<string, (...args: any[]) => Promise<any>>>(api: T) {
  return {
    async call<K extends keyof T>(
      method: K,
      ...args: Parameters<T[K]>
    ): Promise<ReturnType<T[K]>> {
      console.log(`Dispatching API call: ${String(method)}`, args);
      return api[method](...args);
    }
  };
}

const proxy = createApiProxy(backendApi);
const res = await proxy.call("deleteUser", "u_10", true);
```

---

## 12. Interview Questions

<details>
<summary>1. How does <code>ReturnType&lt;T&gt;</code> extract the return type of a function?</summary>

It uses a conditional type with the `infer` keyword:
```typescript
type ReturnType<T extends (...args: any) => any> = T extends (...args: any) => infer R ? R : any;
```
The compiler matches the function signature against `T` and infers `R` as the return type.
</details>

<details>
<summary>2. Why does <code>ReturnType</code> on an overloaded function only return the last overload?</summary>

TypeScript models overloaded functions as a list of call signatures. When `infer` inspects a function type with multiple signatures, it matches against the last overload signature in the list, reflecting how TypeScript resolves final implementations.
</details>

<details>
<summary>3. What is the difference between <code>MyClass</code> and <code>typeof MyClass</code> in TypeScript?</summary>

`MyClass` in type space refers to the **instance type** (the shape of objects created with `new MyClass()`). `typeof MyClass` refers to the **constructor function / static side** (the class itself, including static methods and constructor signatures).
</details>

<details>
<summary>4. How does <code>InstanceType&lt;T&gt;</code> work?</summary>

It pattern-matches against a construct signature:
```typescript
type InstanceType<T extends abstract new (...args: any) => any> =
  T extends abstract new (...args: any) => infer R ? R : any;
```
It extracts `R`, the instance type produced when the constructor is invoked with `new`.
</details>

<details>
<summary>5. How can you remove the <code>this</code> parameter from a function type?</summary>

Using the standard utility `OmitThisParameter<T>`:
```typescript
function greet(this: { name: string }, greeting: string) {
  return `${greeting}, ${this.name}`;
}
type CleanGreet = OmitThisParameter<typeof greet>; // (greeting: string) => string
```
</details>

<details>
<summary>6. What does <code>ConstructorParameters&lt;T&gt;</code> return when given an abstract class?</summary>

Since TypeScript 4.2, `ConstructorParameters` accepts `abstract new (...args: any) => any`, so it successfully returns the constructor argument tuple of abstract classes.
</details>

<details>
<summary>7. What is returned if you pass <code>any</code> or <code>never</code> to <code>Parameters&lt;T&gt;</code>?</summary>

Passing `any` returns `unknown[]`. Passing `never` returns `never` due to distributive conditional type behavior.
</details>

<details>
<summary>8. How do labeled tuple elements behave in <code>Parameters&lt;T&gt;</code>?</summary>

TypeScript preserves parameter names as tuple element labels (e.g. `[id: string, count: number]`). When used in function declarations, IDEs display these original parameter names in IntelliSense tooltips.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Extract First Parameter (Easy)
Write a utility type `FirstParameter<T>` that extracts the type of the first argument of a function.
<details>
<summary>Solution</summary>

```typescript
type FirstParameter<T extends (...args: any[]) => any> =
  Parameters<T> extends [infer First, ...any[]] ? First : never;

function log(message: string, code: number) {}
type MsgType = FirstParameter<typeof log>; // string
```
</details>

### Exercise 2: Async Function Result Unwrapper (Easy)
Extract the inner resolved type of an async function's return promise.
<details>
<summary>Solution</summary>

```typescript
type AsyncFunctionReturn<T extends (...args: any[]) => Promise<any>> =
  Awaited<ReturnType<T>>;

async function fetchStats() { return { uptime: 1200 }; }
type Stats = AsyncFunctionReturn<typeof fetchStats>; // { uptime: number }
```
</details>

### Exercise 3: Generic Debounce Typing (Medium)
Type a debounce wrapper function that preserves arguments and returns void.
<details>
<summary>Solution</summary>

```typescript
function debounce<T extends (...args: any[]) => any>(
  fn: T,
  delayMs: number
): (...args: Parameters<T>) => void {
  let timer: any;
  return (...args: Parameters<T>) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), delayMs);
  };
}

const send = (msg: string, count: number) => console.log(msg, count);
const debouncedSend = debounce(send, 200);
debouncedSend("hello", 1);
```
</details>

### Exercise 4: Factory Function Return Type Extractor (Medium)
Given a dictionary of factory functions, construct a type mapping factory keys to their return instances.
<details>
<summary>Solution</summary>

```typescript
type FactoryMap = {
  user: () => { id: string };
  order: () => { orderId: number };
};

type ProducedTypes<T extends Record<string, () => any>> = {
  [K in keyof T]: ReturnType<T[K]>;
};

type Result = ProducedTypes<FactoryMap>;
// { user: { id: string }; order: { orderId: number } }
```
</details>

### Exercise 5: Constructor Dependency Injector (Hard)
Implement a dynamic instantiator function that takes a class constructor and an argument provider, returning an instance.
<details>
<summary>Solution</summary>

```typescript
type Constructable<T> = new (...args: any[]) => T;

function createInstance<T extends Constructable<any>>(
  ctor: T,
  argProvider: () => ConstructorParameters<T>
): InstanceType<T> {
  const args = argProvider();
  return new ctor(...args);
}

class Client {
  constructor(public token: string, public retries: number) {}
}

const instance = createInstance(Client, () => ["secret_token", 3]);
```
</details>

### Exercise 6: Curried Function Parameter Type (Hard)
Write a utility type `Curried<T>` that converts a function `(a: A, b: B) => R` into `(a: A) => (b: B) => R`.
<details>
<summary>Solution</summary>

```typescript
type Curried<P extends any[], R> =
  P extends [infer First, ...infer Rest]
    ? (arg: First) => Curried<Rest, R>
    : R;

type CurryFn<T extends (...args: any[]) => any> =
  Curried<Parameters<T>, ReturnType<T>>;

type Simple = (a: string, b: number, c: boolean) => string[];
type C = CurryFn<Simple>;
// (arg: string) => (arg: number) => (arg: boolean) => string[]
```
</details>
