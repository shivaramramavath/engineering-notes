# Module 28: Syntax & Basic Types Cheatsheet

> **Quick Reference**: Comprehensive syntax cheat sheet for TypeScript primitive types, literals, unions, intersections, arrays, tuples, enums, and type assertions.

---

## 1. Primitive & Literal Types

| Type | Syntax Example | Notes |
|---|---|---|
| `string` | `const name: string = "Ada";` | UTF-16 character sequences |
| `number` | `const count: number = 42;` | 64-bit IEEE 754 floats |
| `boolean` | `const isValid: boolean = true;` | `true` or `false` |
| `bigint` | `const big: bigint = 9007199254740991n;` | Target >= ES2020 |
| `symbol` | `const sym: symbol = Symbol("key");` | Unique primitive keys |
| `null` | `const empty: null = null;` | Absorbed unless strictNullChecks |
| `undefined`| `const uninit: undefined = undefined;` | Unassigned primitive |
| `literal` | `const status: "open" | "closed" = "open";` | Exact primitive value restriction |

---

## 2. Top & Bottom Types

| Type | Assignable From | Assignable To | Key Rule |
|---|---|---|---|
| `any` | Everything | Everything (except `never`) | Disables type checker; avoid in production! |
| `unknown` | Everything | Only `unknown` and `any` | Type-safe top type; requires narrowing |
| `never` | Nothing (empty set) | Everything | Bottom type; unachievable execution branch |
| `void` | `undefined` (or nothing) | Only `any` / `unknown` | Function returns with no meaningful value |

---

## 3. Arrays, Tuples & Readonly

```typescript
// Arrays
const numbers: number[] = [1, 2, 3];
const strings: Array<string> = ["a", "b"];
const readonlyArr: readonly number[] = [1, 2, 3];

// Tuples
type Point2D = [x: number, y: number];
const pt: Point2D = [10, 20];

// Named optional & rest elements in tuples:
type ResponseState = [code: number, message?: string, ...details: string[]];
const res1: ResponseState = [200];
const res2: ResponseState = [404, "Not Found", "log1", "log2"];

// Immutable tuple ('as const')
const config = ["localhost", 8080] as const; // readonly ["localhost", 8080]
```

---

## 4. Object Types, Interfaces & Type Aliases

```typescript
// Interface: Open for declaration merging, object-oriented
interface User {
  readonly id: string;
  name: string;
  age?: number;
}

// Type Alias: Supports unions, primitives, tuples, and intersections
type Status = "pending" | "resolved" | "rejected";
type UserWithRole = User & { role: "admin" | "member" };

// Index Signatures & Record
type Dictionary = Record<string, unknown>;
interface ConfigMap {
  [key: `app_${string}`]: string | number; // Template literal index signature
}
```

---

## 5. Type Assertions & Non-Null Operator

```typescript
// Type Assertion (compile-time only, NO runtime cast)
const rawInput: unknown = "hello world";
const strLength = (rawInput as string).length;

// Non-null assertion operator (!)
function process(el: HTMLElement | null) {
  el!.focus(); // Asserts el is not null or undefined
}

// Const Assertion
const routes = {
  home: "/",
  about: "/about",
} as const; // Freezes object values and properties as literals
```\n