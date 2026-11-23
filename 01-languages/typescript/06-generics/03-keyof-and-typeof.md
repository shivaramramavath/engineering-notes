# keyof and typeof Operators

> The `keyof` and `typeof` type operators form the bedrock of type-level reflection in TypeScript, converting runtime object shapes and property keys into compile-time union and record definitions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md), [03-objects.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/03-objects.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.1 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `keyofStringsOnly: false` |

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
In JavaScript, objects are dynamic hash tables whose keys and shapes vary freely. In statically typed systems, interacting dynamically with object properties (e.g. `obj[key]`) without losing type safety traditionally required verbose boilerplate or unsafe casts.

TypeScript introduced two foundational operators to bridge runtime values and compile-time types:
- **`typeof` in type space**: Queries the static TypeScript type of a runtime value, variable, or expression without evaluating it at runtime.
- **`keyof`**: Produces a union type representing all known property keys (strings, numbers, and symbols) of an object type.

Together, `keyof typeof obj` enables "single source of truth" designs where JavaScript configuration objects automatically generate exhaustive, strictly verified TypeScript type unions without duplicating definitions.

---

## 2. Core Concepts

### `typeof` in Value Space vs. Type Space
JavaScript provides a runtime `typeof` operator that returns one of eight string primitives (`"string"`, `"number"`, `"boolean"`, `"undefined"`, `"object"`, `"function"`, `"symbol"`, `"bigint"`).
In TypeScript, when `typeof` appears in a type context (after a colon, in a type alias, or within generic arguments), it invokes the **Type Query Operator**, extracting the structural compile-time type of an identifier:

```typescript
const appConfig = {
  port: 8080,
  host: "0.0.0.0",
  tls: { enabled: true, certPath: "/etc/ssl/cert.pem" }
} as const;

// Value space typeof (JS):
console.log(typeof appConfig); // "object"

// Type space typeof (TS):
type AppConfig = typeof appConfig;
// { readonly port: 8080; readonly host: "0.0.0.0"; readonly tls: { ... } }
```

### The `keyof` Index Type Query
Given a type `T`, `keyof T` yields a union of permitted property key types. For concrete object types without index signatures, `keyof` yields a union of literal types:

```typescript
interface UserProfile {
  id: string;
  username: string;
  age: number;
}

type UserKeys = keyof UserProfile; // "id" | "username" | "age"
```

When index signatures are present:
- `{ [key: string]: unknown }` has `keyof` equal to `string | number` (because JS coerces number indices to strings).
- `{ [key: number]: unknown }` has `keyof` equal to `number`.
- `any` has `keyof` equal to `string | number | symbol`.

---

## 3. How It Works Internally

```
Type Space:
+-------------------+        typeof         +-----------------------+
|  Runtime Value    | ------------------->  |  Inferred Object Type |
|  const DB_CONFIG  |                       |  { host: string, ...} |
+-------------------+                       +-----------------------+
                                                       |
                                                       | keyof
                                                       v
                                            +-----------------------+
                                            | Key Union Type        |
                                            | "host" | "port" | ... |
                                            +-----------------------+
```

When the TypeScript checker evaluates `keyof T`:
1. It resolves `T` to its base declared type.
2. It gathers all visible properties (including public declared members and indexed signatures).
3. If `T` is a union `A | B`, `keyof (A | B)` distributes as `(keyof A) & (keyof B)` (intersection of keys, because only keys present in both variants can be safely accessed without narrowing).
4. If `T` is an intersection `A & B`, `keyof (A & B)` computes as `(keyof A) | (keyof B)`.

---

## 4. Syntax & API Reference

### Combining `keyof` and `typeof`
```typescript
const Endpoints = {
  USERS: "/api/v1/users",
  ORDERS: "/api/v1/orders",
  PAYMENTS: "/api/v1/payments",
} as const;

type EndpointKey = keyof typeof Endpoints; 
// "USERS" | "ORDERS" | "PAYMENTS"

type EndpointPath = (typeof Endpoints)[EndpointKey]; 
// "/api/v1/users" | "/api/v1/orders" | "/api/v1/payments"
```

### Indexed Signatures and Numbers
```typescript
interface NumericDictionary {
  [index: number]: string;
}
type NumKey = keyof NumericDictionary; // number

interface StringDictionary {
  [index: string]: string;
}
type StrKey = keyof StringDictionary; // string | number
```

---

## 5. Practical Examples

### 5.1 Basic Usage
```typescript
function getProperty<T, K extends keyof T>(obj: T, key: K): T[K] {
  return obj[key];
}

const user = { name: "Ada", age: 36, active: true };
const userName = getProperty(user, "name"); // type: string
const userAge = getProperty(user, "age");   // type: number
```

### 5.2 Real-World Application: Type-Safe Event Bus
```typescript
interface SystemEvents {
  "user:login": { userId: string; ip: string };
  "order:created": { orderId: string; totalCents: number };
  "system:shutdown": { reason: string; force: boolean };
}

class TypedEventEmitter {
  private listeners: {
    [K in keyof SystemEvents]?: Array<(payload: SystemEvents[K]) => void>;
  } = {};

  on<K extends keyof SystemEvents>(event: K, handler: (payload: SystemEvents[K]) => void): void {
    if (!this.listeners[event]) {
      this.listeners[event] = [];
    }
    this.listeners[event]!.push(handler);
  }

  emit<K extends keyof SystemEvents>(event: K, payload: SystemEvents[K]): void {
    const handlers = this.listeners[event];
    if (handlers) {
      for (const fn of handlers) {
        fn(payload);
      }
    }
  }
}

const bus = new TypedEventEmitter();
bus.on("user:login", (data) => console.log(data.userId, data.ip));
bus.emit("user:login", { userId: "usr_101", ip: "127.0.0.1" });
```

### 5.3 Edge Case: `keyof` on Union Types
```typescript
type AdminUser = { id: string; role: "admin"; permissions: string[] };
type GuestUser = { id: string; role: "guest"; sessionId: string };

type SharedKeys = keyof (AdminUser | GuestUser); 
// "id" | "role" (intersection of property keys)

type AllKeys = (keyof AdminUser) | (keyof GuestUser); 
// "id" | "role" | "permissions" | "sessionId"
```

### 5.4 Failure Case: Accessing Non-Shared Keys
```typescript
function auditEntity(entity: AdminUser | GuestUser, key: keyof (AdminUser | GuestUser)): void {
  console.log(entity[key]);
}

declare const entity: AdminUser | GuestUser;

// @ts-expect-error TS2345: Argument of type '"permissions"' is not assignable to parameter of type '"id" | "role"'.
auditEntity(entity, "permissions");
```

### 5.5 Idiomatic Pattern: Immutable Enum-Like Dictionary
```typescript
export const HTTP_STATUS = {
  OK: 200,
  CREATED: 201,
  BAD_REQUEST: 400,
  UNAUTHORIZED: 401,
  NOT_FOUND: 404,
  INTERNAL_ERROR: 500,
} as const;

export type HttpStatusKey = keyof typeof HTTP_STATUS;
export type HttpStatusCode = (typeof HTTP_STATUS)[HttpStatusKey];

export function isSuccessStatus(code: HttpStatusCode): boolean {
  return code >= 200 && code < 300;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`keyof any`**: Evaluates to `string | number | symbol`.
2. **String dictionary indexing**: In JavaScript, `obj[1]` is accessed as `obj["1"]`. Hence, for `interface Dict { [k: string]: any }`, `keyof Dict` is `string | number`.
3. **`typeof` on Classes**: `typeof MyClass` refers to the constructor function / static side, whereas `MyClass` refers to the instance type.
4. **Symbol properties**: When `keyof` inspects a type containing unique symbol keys, the symbol literals are preserved in the union.
5. **Private and protected fields**: In class types, `keyof ClassType` only includes **public** members. Private and protected properties are excluded from the `keyof` union.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| `type K = keyof obj` (passing value to `keyof`) | `keyof` expects a type, not a runtime value | Use `keyof typeof obj` | `TS2749: 'obj' refers to a value, but is being used as a type here.` |
| `typeof MyClass` when instance is desired | Targets constructor rather than instance | Use `InstanceType<typeof MyClass>` or `MyClass` directly | `TS2322: Type 'typeof MyClass' is not assignable to type 'MyClass'.` |
| Accessing unshared key in union | Only intersection of keys is safe | Discriminate union before accessing variant keys | `TS2339: Property 'x' does not exist on type 'A | B'.` |
| `keyof` on unbounded generic `T` | `keyof T` is unknown / empty without constraints | Constrain generic: `<T extends object>` | `TS2536: Type 'K' cannot be used to index type 'T'.` |
| Forgetting `as const` on lookup maps | Inferred as `string` instead of string literals | Add `as const` to lock literal keys and values | `TS2345: Argument of type 'string' not assignable to literal.` |
| Over-relying on `string` index signature | Destroys strict key checking for all properties | Use explicit mapped types or `Record<Keys, T>` | `TS7053: Element implicitly has an 'any' type.` |

---

## 8. Performance & Type-Checker Complexity
- Computing `keyof` on large interfaces with hundreds of properties generates wide union types. If crossed with distribution in conditional types (e.g. `K extends keyof T ? ... : ...`), it can cause exponential expansion ($O(N)$ or $O(N^2)$ instantiations).
- Union intersections (`keyof (A | B)`) require checking pairwise subtyping across union members, which scales quadratically with union size. Keep union branches normalized.

---

## 9. Compiler Internals
In `src/compiler/checker.ts`:
- The `keyof` operator maps to `IndexTypeNode` in the AST.
- Handled by `getIndexType(type: Type, flags: IndexFlags)`.
- The compiler iterates over the target type's symbol table (`type.symbol.members`), filtering out members that have `SymbolFlags.Private` or `SymbolFlags.Protected`.
- For intersection and union types, `getIndexType` invokes `getIntersectionType` or `getUnionType` on the constituent index types respectively.

---

## 10. Best Practices & Design Guidelines
- Use `as const` alongside `typeof` to capture exact literal dictionary types.
- Export both the key union (`keyof typeof obj`) and value union (`(typeof obj)[keyof typeof obj]`) for reusable lookup objects.
- Constrain generic property access functions using `<T, K extends keyof T>(obj: T, key: K)`.
- When dealing with class types, be mindful whether you need the static side (`typeof Service`) or the instance side (`Service`).

---

## 11. Real-World Case Study
### Safe Config Dynamic Reader in Microservices
```typescript
const ServiceRegistry = {
  AUTH_SERVICE: { host: "auth.internal", port: 5001, timeoutMs: 2000 },
  BILLING_SERVICE: { host: "billing.internal", port: 5002, timeoutMs: 5000 },
  NOTIF_SERVICE: { host: "notif.internal", port: 5003, timeoutMs: 1500 },
} as const;

type ServiceName = keyof typeof ServiceRegistry;
type ServiceConfig<K extends ServiceName> = (typeof ServiceRegistry)[K];

class ServiceClientFactory {
  static getServiceUrl<K extends ServiceName>(service: K): string {
    const config = ServiceRegistry[service];
    return `http://${config.host}:${config.port}`;
  }
}

// Compile-time guaranteed service name and URL resolution:
const authUrl = ServiceClientFactory.getServiceUrl("AUTH_SERVICE");
console.log(authUrl); // "http://auth.internal:5001"
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between JavaScript's <code>typeof</code> and TypeScript's <code>typeof</code>?</summary>

JavaScript's `typeof` is an expression operator evaluated at runtime that returns a string representing the primitive type of an operand (e.g., `"string"`, `"object"`). TypeScript's `typeof` operates in type space (after `type`, `:`, or in generic type arguments) and extracts the static TypeScript type representation of any identifier at compile time.
</details>

<details>
<summary>2. Why does <code>keyof { [k: string]: boolean }</code> include <code>number</code>?</summary>

In JavaScript, object property access converts numbers to strings (`obj[0]` is equivalent to `obj["0"]`). TypeScript reflects this ECMAScript behavior by defining the key type of a string indexed signature as `string | number`.
</details>

<details>
<summary>3. What does <code>keyof (A | B)</code> produce vs <code>keyof (A & B)</code>?</summary>

`keyof (A | B)` produces `(keyof A) & (keyof B)`—only keys common to both types, ensuring safe access without discrimination. Conversely, `keyof (A & B)` produces `(keyof A) | (keyof B)`—the union of all keys from both types.
</details>

<details>
<summary>4. Why are private class fields not present in <code>keyof MyClass</code>?</summary>

TypeScript's `keyof` operator inspects publicly accessible property keys. Since private (`#field` or `private field`) and protected fields cannot be accessed externally via index notation, they are deliberately excluded from `keyof`.
</details>

<details>
<summary>5. How do you extract the value union from a `const` object?</summary>

By combining indexed access with `keyof typeof`:
```typescript
const Config = { A: 1, B: 2 } as const;
type Values = (typeof Config)[keyof typeof Config]; // 1 | 2
```
</details>

<details>
<summary>6. What is the type of <code>keyof any</code> in TypeScript?</summary>

In TypeScript, `keyof any` is strictly `string | number | symbol`, which represents all possible valid property key types in ECMAScript.
</details>

<details>
<summary>7. Can you use <code>typeof</code> on an expression like <code>typeof (a + b)</code> in type annotations?</summary>

No. TypeScript's type query `typeof` only accepts identifiers or qualified dotted names (e.g. `typeof myVar.prop`). Arbitrary expressions like `typeof (a + b)` or `typeof fn()` are syntactically invalid in type space; use `ReturnType<typeof fn>` instead.
</details>

<details>
<summary>8. What happens to optional properties when applying <code>keyof</code>?</summary>

Optional properties (e.g. `age?: number`) still contribute their key name (e.g. `"age"`) to the `keyof` union. The optional modifier affects the value type (`number | undefined`), not the key type.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Extract Keys by Value Type (Easy)
Write a utility type `KeysOfType<T, V>` that extracts keys of `T` whose values extend `V`.
<details>
<summary>Solution</summary>

```typescript
type KeysOfType<T, V> = {
  [K in keyof T]: T[K] extends V ? K : never;
}[keyof T];

interface Person {
  id: number;
  name: string;
  age: number;
  isActive: boolean;
}

type NumericKeys = KeysOfType<Person, number>; // "id" | "age"
```
</details>

### Exercise 2: Safe Object Property Plucker (Easy)
Implement `pluck<T, K extends keyof T>(items: T[], key: K): T[K][]`.
<details>
<summary>Solution</summary>

```typescript
function pluck<T, K extends keyof T>(items: T[], key: K): T[K][] {
  return items.map((item) => item[key]);
}

const users = [{ id: 1, name: "Alice" }, { id: 2, name: "Bob" }];
const names = pluck(users, "name"); // string[]
```
</details>

### Exercise 3: Dynamic Property Modifier (Medium)
Create a function that takes an object and a key and applies an update function to that specific property.
<details>
<summary>Solution</summary>

```typescript
function updateProperty<T, K extends keyof T>(
  obj: T,
  key: K,
  updater: (prev: T[K]) => T[K]
): T {
  return {
    ...obj,
    [key]: updater(obj[key]),
  };
}

const res = updateProperty({ count: 10, label: "counter" }, "count", (c) => c + 1);
```
</details>

### Exercise 4: Inverting Dictionary Keys and Values (Medium)
Create a type `Invert<T>` that swaps keys and string-literal values of a record.
<details>
<summary>Solution</summary>

```typescript
type Invert<T extends Record<string, string>> = {
  [K in keyof T as T[K]]: K;
};

const Actions = {
  CREATE: "act_create",
  DELETE: "act_delete",
} as const;

type InvertedActions = Invert<typeof Actions>;
// { act_create: "CREATE"; act_delete: "DELETE" }
```
</details>

### Exercise 5: Deep Path Key Extraction (Hard)
Create a utility type `ObjectPaths<T>` that generates a union of dotted string paths for nested objects.
<details>
<summary>Solution</summary>

```typescript
type ObjectPaths<T> = T extends object
  ? {
      [K in keyof T & string]: T[K] extends object
        ? K | `${K}.${ObjectPaths<T[K]>}`
        : K;
    }[keyof T & string]
  : never;

interface Schema {
  db: {
    primary: { host: string; port: number };
    replica: { host: string };
  };
  env: string;
}

type Paths = ObjectPaths<Schema>;
// "env" | "db" | "db.primary" | "db.primary.host" | "db.primary.port" | "db.replica" | "db.replica.host"
```
</details>

### Exercise 6: Strictly Checked Runtime Object Diff Tracker (Hard)
Implement a function `diffKeys<T extends object>(before: T, after: T): (keyof T)[]` returning only keys whose values changed according to `Object.is`.
<details>
<summary>Solution</summary>

```typescript
function diffKeys<T extends object>(before: T, after: T): (keyof T)[] {
  const changed: (keyof T)[] = [];
  const keys = Object.keys(before) as (keyof T)[];
  for (const k of keys) {
    if (!Object.is(before[k], after[k])) {
      changed.push(k);
    }
  }
  return changed;
}

const stateA = { x: 1, y: "hello", z: true };
const stateB = { x: 1, y: "world", z: true };
const diff = diffKeys(stateA, stateB); // ["y"]
```
</details>
