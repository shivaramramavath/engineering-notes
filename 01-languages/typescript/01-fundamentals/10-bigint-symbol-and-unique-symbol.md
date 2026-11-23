# BigInt, Symbol, and Unique Symbol

> `bigint` enables arbitrary-precision integer arithmetic, while `symbol` and `unique symbol` provide globally unique, unforgeable property keys and nominal type branding mechanisms.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-types-and-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/00-types-and-inference.md), [01-primitive-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/01-primitive-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 2.7 (`unique symbol`) / TS 3.2 (`bigint`) / TS 5.2 (`Symbol.dispose`) / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `target: "ES2022"`, `lib: ["ES2022", "DOM"]` |

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
- **Problem Statement:** Standard JavaScript numbers are double-precision 64-bit floats (IEEE 754), losing integer precision above $2^{53} - 1$ (`Number.MAX_SAFE_INTEGER`). Additionally, string object keys are prone to collisions, accidental overrides, and leakage during object enumeration.
- **Historical Context:** ECMAScript 2015 introduced `Symbol` for unique object keys, and ECMAScript 2020 introduced `BigInt`. TypeScript 2.7 added `unique symbol` to allow individual symbols to have distinct, nominal singleton types. TypeScript 3.2 integrated `bigint`. TypeScript 5.2 introduced support for explicit resource management via `Symbol.dispose` and `Symbol.asyncDispose`.
- **Real-World Relevance:** Financial transactions, microsecond timestamps, database primary keys (64-bit integers), and cryptography depend on `bigint`. `unique symbol` is the foundational primitive for nominal branding, private class metadata, and well-known protocols (`[Symbol.iterator]`, `[Symbol.dispose]`).

---

## 2. Core Concepts
- **Formal Invariants:**
  - `bigint`: Represents integers of arbitrary size. Cannot be mixed with standard `number` in mathematical operations without explicit conversion. Cannot be serialized with default `JSON.stringify()`.
  - `symbol`: Represents any dynamically generated symbol instance.
  - `unique symbol`: A subtype of `symbol` representing one exact, immutable symbol identity. Can only be declared using `const` or `readonly static` class properties. Two `unique symbol` types are never assignable to each other.
- **Mental Model:**
  - `bigint`: Exact integer math with zero floating-point rounding errors.
  - `symbol`: An unguessable, non-colliding key.
  - `unique symbol`: A nominal type-level fingerprint that exists at compile-time and value-level.
- **Soundness vs Ergonomics:** TypeScript prevents `1n + 1` to prevent accidental precision loss when coercing between 64-bit floats and arbitrary integers.

---

## 3. How It Works Internally
- **Symbol Type Identity:** The type checker tracks `unique symbol` via a dedicated `UniqueESSymbolType` referencing the symbol's declaration node. Each `const x = Symbol()` gets a distinct type ID.
- **BigInt Operators:** The type checker permits `+`, `-`, `*`, `/`, `%`, `**`, and bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`) between two `bigint` operands, but rejects mixed operations with `number`.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                        Symbol vs Unique Symbol                           |
|                                                                          |
|   let s = Symbol("key")                const us = Symbol("brand")        |
|            │                                      │                      |
|            ▼                                      ▼                      |
|      Type: symbol                           Type: typeof us              |
|   (General symbol domain)             (UniqueESSymbol singleton type)    |
|            │                                      │                      |
|            ▼                                      ▼                      |
|   Cannot be used for                   Permitted as computed property    |
|   computed interface keys               names in types & nominal brands   |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **BigInt Syntax:**
  ```typescript
  const largeInt: bigint = 9007199254740993n;
  const fromNum: bigint = BigInt(123);
  ```
- **Symbol and Unique Symbol Declarations:**
  ```typescript
  let generalSym: symbol = Symbol('tag');
  const uniqueSym: unique symbol = Symbol('brand');

  interface HasCustomKey {
    [uniqueSym]: string;
  }
  ```
- **Well-Known Symbols (TypeScript 5.2+):**
  - `Symbol.dispose`: Synchronous disposable protocol (`using`).
  - `Symbol.asyncDispose`: Asynchronous disposable protocol (`await using`).
  - `Symbol.iterator`: Iteration protocol.
- **tsconfig Dependencies:**
  - Requires `target: "ES2020"` or higher for `bigint`.
  - Requires `lib: ["ESNext.Disposable"]` or `target: "ES2022"`+ for `Symbol.dispose`.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Compare `bigint` arithmetic and `symbol` uniqueness.
- **Code:**
```typescript
// BigInt precision beyond MAX_SAFE_INTEGER
const safeMax = BigInt(Number.MAX_SAFE_INTEGER);
const beyond1 = safeMax + 1n;
const beyond2 = safeMax + 2n;

console.log(beyond1 !== beyond2); // true (precise!)

// Symbol uniqueness
const k1 = Symbol('key');
const k2 = Symbol('key');
console.log(k1 === k2); // false (each symbol is unique)
```
- **Output / Type Evaluation:**
```typescript
// true, false
```
- **Explanation:** Unlike numbers, bigints maintain single-unit precision at arbitrary magnitudes.

### Example 2: Real-World Enterprise Pattern (Nominal Type Branding via Unique Symbol)
- **Goal:** Construct unforgeable compile-time domain types using `unique symbol`.
- **Code:**
```typescript
declare const UsdBrand: unique symbol;
declare const EurBrand: unique symbol;

export type USD = number & { readonly [UsdBrand]: typeof UsdBrand };
export type EUR = number & { readonly [EurBrand]: typeof EurBrand };

export function makeUSD(amount: number): USD {
  return amount as USD;
}

export function makeEUR(amount: number): EUR {
  return amount as EUR;
}

function transferDollars(amount: USD): void {
  console.log(`Transferred $${amount} USD`);
}

const dollars = makeUSD(100);
const euros = makeEUR(100);

transferDollars(dollars); // OK

// @ts-expect-error TS2345: Argument of type 'EUR' is not assignable to parameter of type 'USD'.
transferDollars(euros);
```
- **Output / Type Evaluation:**
```typescript
// Compile-time error TS2345 prevents accidental currency mixing!
```
- **Explanation:** By using `unique symbol` brands, different currencies cannot be assigned to each other despite sharing the underlying `number` runtime representation.

### Example 3: Edge Case / Boundary Condition (TypeScript 5.2 Resource Management via Symbol.dispose)
- **Goal:** Implement an explicit resource disposable using `Symbol.dispose`.
- **Code:**
```typescript
// Ensure polyfill or modern runtime supports Symbol.dispose
(Symbol as any).dispose ??= Symbol('dispose');

class DatabaseConnection implements Disposable {
  private active = true;

  query(sql: string): string {
    if (!this.active) throw new Error('Connection closed');
    return `Result of ${sql}`;
  }

  [Symbol.dispose](): void {
    this.active = false;
    console.log('Database connection cleanly released.');
  }
}

function executeTransaction(): void {
  using conn = new DatabaseConnection();
  console.log(conn.query('SELECT * FROM users'));
  // conn[Symbol.dispose]() is invoked automatically when exiting scope!
}
```
- **Output / Type Evaluation:**
```typescript
// Result of SELECT * FROM users
// Database connection cleanly released.
```
- **Explanation:** The TypeScript 5.2 `using` keyword leverages `Symbol.dispose` to guarantee resource cleanup even if exceptions occur.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate rejection of mixed number/bigint arithmetic and invalid unique symbol declarations.
- **Code:**
```typescript
const count: number = 10;
const weight: bigint = 50n;

// @ts-expect-error TS2365: Operator '+' cannot be applied to types 'bigint' and 'number'.
const total = weight + count;

// @ts-expect-error TS1332: A variable whose type is a 'unique symbol' type must be 'const'.
let invalidSymbol: unique symbol = Symbol('test');
```
- **Real Compiler Error:**
```
error TS2365: Operator '+' cannot be applied to types 'bigint' and 'number'.
error TS1332: A variable whose type is a 'unique symbol' type must be 'const'.
```
- **Explanation:** The compiler disallows implicit coercion between `number` and `bigint`, and requires `unique symbol` to be declared as an immutable `const`.

### Example 5: Idiomatic Modern Usage (Safe BigInt JSON Serialization)
- **Goal:** Implement custom serialization for BigInt data in distributed APIs.
- **Code:**
```typescript
export type FinancialRecord = {
  transactionId: bigint;
  amountSatoshis: bigint;
  timestamp: number;
};

export function serializeFinancialRecord(record: FinancialRecord): string {
  return JSON.stringify(record, (_key, value) =>
    typeof value === 'bigint' ? `${value.toString()}n` : value
  );
}

export function deserializeFinancialRecord(json: string): FinancialRecord {
  return JSON.parse(json, (_key, value) => {
    if (typeof value === 'string' && /^-?\d+n$/.test(value)) {
      return BigInt(value.slice(0, -1));
    }
    return value;
  });
}
```
- **Output / Type Evaluation:**
```typescript
// Clean round-trip serialization without runtime TypeError: Do not know how to serialize a BigInt.
```
- **Explanation:** By default, `JSON.stringify` throws on BigInt values. Custom revivers and replacers ensure lossless serialization.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **JSON.stringify Throws on BigInt:** Calling `JSON.stringify({ val: 10n })` throws `TypeError: Do not know how to serialize a BigInt` at runtime.
2. **`unique symbol` Narrowing across Modules:** If an exported `unique symbol` is imported, consumers preserve its exact unique identity only if imported as a value.
3. **BigInt Math Performance:** BigInt operations are executed in software on arbitrary-precision byte buffers, making them significantly slower than hardware-accelerated 64-bit float math. Avoid `bigint` in hot graphics/animation loops.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Serializing `bigint` via `JSON.stringify()` | `TypeError: Do not know how to serialize a BigInt` | Custom JSON replacer or string encoding | Prevents unhandled production runtime exceptions |
| Declaring `let x: unique symbol` | `TS1332: A variable whose type is 'unique symbol' must be 'const'` | Use `const x: unique symbol = Symbol()` | Enforces immutable singleton identity |
| Implicitly mixing `number` and `bigint` | `TS2365: Operator '+' cannot be applied to 'bigint' and 'number'` | Explicit conversion (`BigInt(num)` or `Number(bi)`) | Forces conscious decision regarding precision loss |
| Storing 64-bit DB IDs as JavaScript `number` | Data corruption beyond $2^{53} - 1$ | Store as `bigint` or `string` | Prevents corrupted primary keys and foreign key failures |
| Using `Symbol.for()` for unique branding | Re-uses global symbols across different contexts | Use `Symbol()` or `declare const brand: unique symbol` | Guarantees true nominal uniqueness |
| Attempting `Math.max(...bigintArr)` | `TS2345: Argument of type 'bigint' not assignable to 'number'` | Custom loop or BigInt comparator | Standard `Math` functions only operate on `number` |

---

## 8. Performance & Type-Checker Complexity
- **Type Checker Allocation:** Each `unique symbol` generates a fresh internal symbol ID in `checker.ts`. The comparison `typeA === typeB` is an $O(1)$ reference check.
- **BigInt Memory Footprint:** Small integers fit in tagged pointer representations in V8; large bigints allocate heap memory dynamically.
- **Isolated Declarations:** Under `--isolatedDeclarations`, exported `const x = Symbol()` must have an explicit type annotation `const x: unique symbol = Symbol()`.

---

## 9. Compiler Internals
- **AST Node Representation:**
  - `bigint` keyword is `SyntaxKind.BigIntKeyword`.
  - BigInt literals (e.g. `100n`) are `SyntaxKind.BigIntLiteral`.
  - `unique symbol` is `SyntaxKind.UniqueSymbolKeyword`.
- **Type Flags:**
  - `TypeFlags.BigInt`
  - `TypeFlags.BigIntLiteral`
  - `TypeFlags.ESSymbol`
  - `TypeFlags.UniqueESSymbol`
- **Checker Behavior:** `checkBinaryExpression` enforces that both operands of arithmetic operators belong exclusively to the `BigInt` family or exclusively to the `Number` family.

---

## 10. Best Practices & Design Guidelines
- **Use `bigint` for Financial & High-Precision Units:** Never represent satoshis, wei, or high-precision inventory counts as standard numbers.
- **Use `declare const brand: unique symbol` for Nominal Types:** This provides compile-time branding with zero JavaScript emission.
- **Leverage `Symbol.dispose` for Managed Resources:** Use TypeScript 5.2 `using` declarations to manage file handles, locks, and network sockets safely.

---

## 11. Real-World Case Study
- **Scenario:** A crypto custodial wallet recorded Ethereum transaction gas fees in Wei ($10^{18}$ Wei = 1 Ether). The backend stored fees as JavaScript numbers. High-volume transfers began dropping low-order Wei precision due to floating-point truncation, causing gas price estimation mismatches that resulted in $230,000 in stuck, underpriced mempool transactions.
- **Implementation:** Refactored the core ledger to use `bigint` with nominal branding:
  ```typescript
  declare const WeiBrand: unique symbol;
  export type Wei = bigint & { readonly [WeiBrand]: typeof WeiBrand };

  export function parseGweiToWei(gwei: bigint): Wei {
    return (gwei * 1_000_000_000n) as Wei;
  }
  ```
- **Outcome:** Eliminated 100% of precision loss; all transactions were submitted with exact integer Wei values, completely fixing mempool rejection issues.

---

## 12. Interview Questions

### Q1: Why does JavaScript and TypeScript forbid mixing `number` and `bigint` in arithmetic operations?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

JavaScript forbids implicit coercion between `number` and `bigint` because neither type is a strict superset of the other. Converting `bigint` to `number` can cause silent loss of precision (numbers cannot represent integers above $2^{53} - 1$ accurately). Converting `number` to `bigint` truncates fractional components. To avoid silent bugs, the language mandates explicit conversion: `bi + BigInt(num)` or `Number(bi) + num`.
</details>

### Q2: What is the difference between `symbol` and `unique symbol`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`symbol` is the general primitive type representing any symbol value created via `Symbol()`. `unique symbol` is a nominal subtype that represents one specific, immutable symbol instance. Because `unique symbol` has a known identity at compile time, it can be used as a computed property name in types and interfaces (`[myUniqueSymbol]: string`), whereas general `symbol` cannot.
</details>

### Q3: What are the syntactical requirements for declaring a `unique symbol`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A `unique symbol` can only be declared:
1. As a top-level `const` variable: `const mySymbol: unique symbol = Symbol();`
2. As a `readonly static` member of a class: `static readonly key: unique symbol = Symbol();`
3. Using ambient declarations: `declare const brand: unique symbol;`
It cannot be declared with `let`, `var`, or as an instance property of a class.
</details>

### Q4: How does `Symbol.dispose` work with the TypeScript 5.2 `using` keyword?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript 5.2 implements the TC39 Explicit Resource Management proposal. When an object implements the `Disposable` interface (possessing a `[Symbol.dispose](): void` method), declaring it with `using res = new Resource()` registers it for automatic cleanup. When the enclosing block scope is exited (via normal return, break, or thrown exception), the compiler guarantees that `res[Symbol.dispose]()` is executed.
</details>

### Q5: Can `BigInt` represent fractional numbers?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

No. `BigInt` only represents integers. Performing division on BigInt values (e.g. `5n / 2n`) rounds toward zero, producing `2n`. For fractional numbers requiring arbitrary precision, libraries like `decimal.js` or `bignumber.js` must be used.
</details>

### Q6: What is the difference between `Symbol()` and `Symbol.for()`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`Symbol('key')` always creates a brand new, unique symbol instance in memory; two calls with the same string argument produce different symbols. `Symbol.for('key')` accesses a runtime-wide global symbol registry: if a symbol with that key already exists, it is returned; otherwise, a new symbol is registered and returned. Symbols created via `Symbol.for()` are shared across iframes and service workers.
</details>

### Q7: Why does `typeof 10n` return `"bigint"`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In ECMAScript 2020, `bigint` was added as a distinct primitive data type alongside `number`, `string`, `boolean`, `symbol`, `undefined`, and `object`. Therefore, `typeof` returns the string `"bigint"`, allowing control flow analysis in TypeScript to narrow unions between numbers and bigints via `typeof x === "bigint"`.
</details>

### Q8: How can you safely serialize objects containing BigInt properties to JSON?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

You can pass a custom replacer function to `JSON.stringify(data, (key, value) => typeof value === 'bigint' ? value.toString() : value)`, or augment `BigInt.prototype.toJSON = function() { return this.toString(); }` (though modifying prototypes is discouraged in libraries).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Safe BigInt Sum (Easy)
- **Problem Statement:** Write a function `sumBigInts(arr: bigint[]): bigint` that computes the sum of an array of bigints without converting to number.
- **Test Case:**
```typescript
sumBigInts([10n, 20n, 30n]); // 60n
sumBigInts([]); // 0n
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function sumBigInts(arr: bigint[]): bigint {
  return arr.reduce((acc, curr) => acc + curr, 0n);
}
```
</details>

### Exercise 2: Unique Symbol Brand Creator (Easy)
- **Problem Statement:** Create a generic branded type helper using a unique symbol and construct a branded `Timestamp` type.
- **Test Case:**
```typescript
declare const TimestampBrand: unique symbol;
type Timestamp = number & { readonly [TimestampBrand]: typeof TimestampBrand };
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
declare const TimestampBrand: unique symbol;
export type Timestamp = number & { readonly [TimestampBrand]: typeof TimestampBrand };

export function asTimestamp(val: number): Timestamp {
  return val as Timestamp;
}
```
</details>

### Exercise 3: Generic Disposable Wrapper (Medium)
- **Problem Statement:** Write a function `makeDisposable<T>(resource: T, cleanup: (res: T) => void): T & Disposable` that wraps any object with a `Symbol.dispose` implementation.
- **Test Case:**
```typescript
let closed = false;
{
  using res = makeDisposable({ handle: 1 }, (r) => { closed = true; });
}
// closed is true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function makeDisposable<T extends object>(resource: T, cleanup: (res: T) => void): T & Disposable {
  return Object.assign(resource, {
    [Symbol.dispose]() {
      cleanup(resource);
    }
  });
}
```
</details>

### Exercise 4: BigInt Min-Max Utility (Medium)
- **Problem Statement:** Write `bigIntMax(...values: bigint[]): bigint` that returns the maximum value among given bigints, throwing on empty input.
- **Test Case:**
```typescript
bigIntMax(10n, 50n, 30n); // 50n
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function bigIntMax(...values: bigint[]): bigint {
  if (values.length === 0) throw new TypeError('Expected at least one bigint');
  let max = values[0]!;
  for (let i = 1; i < values.length; i++) {
    if (values[i]! > max) {
      max = values[i]!;
    }
  }
  return max;
}
```
</details>

### Exercise 5: Compile-Time Nominal Uniqueness Checker (Hard)
- **Problem Statement:** Write a utility type `AreDistinctSymbols<A, B>` that evaluates to `true` if `A` and `B` are two non-assignable `unique symbol` types, and `false` if they are identical.
- **Test Case:**
```typescript
declare const S1: unique symbol;
declare const S2: unique symbol;

type T1 = AreDistinctSymbols<typeof S1, typeof S2>; // true
type T2 = AreDistinctSymbols<typeof S1, typeof S1>; // false
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type AreDistinctSymbols<A, B> = [A] extends [B] ? false : true;
```
</details>

### Exercise 6: Lossless BigInt Math Pipeline (Hard)
- **Problem Statement:** Implement a type-safe fixed-point decimal class `FixedDecimal<Decimals extends number>` backed entirely by `bigint`, supporting exact addition, subtraction, and multiplication without floating point errors.
- **Test Case:**
```typescript
const d1 = FixedDecimal.from('1.50', 2); // 150n with scale 2
const d2 = FixedDecimal.from('2.25', 2); // 225n with scale 2
const sum = d1.add(d2); // 3.75
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class FixedDecimal<D extends number> {
  private constructor(public readonly value: bigint, public readonly decimals: D) {}

  static from<D extends number>(str: string, decimals: D): FixedDecimal<D> {
    const [intPart, fracPart = ''] = str.split('.');
    const paddedFrac = fracPart.padEnd(decimals, '0').slice(0, decimals);
    const bigintStr = `${intPart}${paddedFrac}`;
    return new FixedDecimal(BigInt(bigintStr), decimals);
  }

  add(other: FixedDecimal<D>): FixedDecimal<D> {
    return new FixedDecimal(this.value + other.value, this.decimals);
  }

  toString(): string {
    const str = this.value.toString().padStart(this.decimals + 1, '0');
    const cut = str.length - this.decimals;
    return `${str.slice(0, cut)}.${str.slice(cut)}`;
  }
}
```
</details>
