# Assignability, Subtyping, and Freshness in TypeScript

> Deep exploration of TypeScript's type relationships: structural subtyping, the assignability relation, identity, excess property checks (freshness), and union assignability.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type System Internals |
| **Difficulty** | Advanced |
| **Prerequisites** | [01-fundamentals/03-objects.md](../01-fundamentals/03-objects.md), [14-type-system-internals/00-type-erasure-and-runtime.md](./00-type-erasure-and-runtime.md) |
| **TypeScript Baseline** | TS 2.0+, TS 5.0+ |
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
Unlike nominal type systems (Java, C++, Rust) where a type's compatibility is determined strictly by explicit inheritance or interface implementation declarations (`class Dog extends Animal`), TypeScript is based on **Structural Subtyping** (duck typing: "if it walks like a duck and quacks like a duck, it is a duck").

However, TypeScript's assignability rules extend beyond simple structural subtyping. The type checker incorporates **Type Freshness** (excess property checks on object literals), union distribution, and pragmatic compatibility rules. Understanding assignability and freshness explains why passing an inline object literal behaves differently than passing a pre-assigned variable.

---

## 2. Core Concepts
- **Structural Subtyping**: Type $S$ is a subtype of type $T$ ($S <: T$) if $S$ includes at least all the members of $T$ with compatible types.
- **Assignability Relation ($S$ is assignable to $T$)**: A superset of subtyping that incorporates pragmatic rules (e.g. `any` is assignable to everything, `unknown` accepts everything, numerical enums are assignable to numbers).
- **Type Freshness and Excess Property Checks**:
  - When an object literal is created *inline* (e.g. `fn({ x: 1, extra: 2 })`), TypeScript flags properties not declared in the target type as errors.
  - When an object literal is assigned to an intermediate variable first (`const obj = { x: 1, extra: 2 }; fn(obj);`), freshness is lost, and structural subtyping permits the extra property!
- **Identity Relation**: Two types are identical if they are mutually assignable without conversions.
- **Union Assignability**:
  - $A$ is assignable to $B \cup C$ if $A$ is assignable to $B$ OR $A$ is assignable to $C$.
  - $A \cup B$ is assignable to $T$ only if BOTH $A$ is assignable to $T$ AND $B$ is assignable to $T$.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                Assignability Check: Source -> Target        |
+------------------------------+------------------------------+
                               |
            Is Source identical to Target? ---> (Yes: OK)
                               |
                               v (No)
            Is Source 'any' or Target 'any'? -> (Yes: OK)
                               |
                               v (No)
            Is Source an Inline Object Literal (Fresh)?
                               |
            +------------------+------------------+
            | (Yes)                               | (No)
            v                                     v
   Excess Property Check:                Structural Check:
   Does Source have keys                 Does Source contain ALL
   NOT in Target?                        required keys of Target?
            |                                     |
    (Yes) -> TS2353 Error                 (Yes) -> Assignable OK
```

In `checker.ts`, every newly created object literal receives a synthetic `TypeFlags.FreshLiteral` flag. When the compiler evaluates assignability against a target interface, if the fresh flag is active, it calls `hasExcessProperties()`. Once the literal is bound to a variable or widened, the fresh flag is stripped.

---

## 4. Syntax & API Reference

```typescript
interface Point2D {
  x: number;
  y: number;
}

// 1. Structural Subtyping in action
const point3D = { x: 10, y: 20, z: 30 };
const p2: Point2D = point3D; // OK: point3D has x and y

// 2. Excess Property Check on Fresh Literal
// @ts-expect-error TS2353: Object literal may only specify known properties, and 'z' does not exist in type 'Point2D'.
const direct: Point2D = { x: 10, y: 20, z: 30 };

// 3. Suppressing excess property checks with index signatures
interface FlexiblePoint {
  x: number;
  y: number;
  [extra: string]: unknown;
}
const flex: FlexiblePoint = { x: 1, y: 2, label: 'Origin' }; // OK
```

---

## 5. Practical Examples

### Example 1: Function Argument Freshness Checks
```typescript
interface RequestOptions {
  timeoutMs?: number;
  retries?: number;
}

function sendRequest(url: string, options: RequestOptions) {
  console.log(`Sending to ${url}`);
}

// Typo in property name:
// @ts-expect-error TS2353: Object literal may only specify known properties, and 'retry' does not exist in type 'RequestOptions'. Did you mean 'retries'?
sendRequest('https://api.com', { retry: 3 });

// Bypassing freshness (Anti-pattern):
const typoOptions = { retry: 3 };
sendRequest('https://api.com', typoOptions); // Compiles! But 'retry' is ignored at runtime.
```

### Example 2: Structural Subtyping between Classes
```typescript
class Vector2 {
  constructor(public x: number, public y: number) {}
}

class Coordinates {
  constructor(public x: number, public y: number) {}
}

// Despite being different classes, they are mutually assignable:
const v: Vector2 = new Coordinates(5, 10); // Completely valid!
```

### Example 3: Edge Case - Empty Object Type `{}` Assignability
```typescript
// In TypeScript, {} matches ANY non-nullish value (including primitives!)
const a: {} = 'text';      // OK!
const b: {} = 42;          // OK!
const c: {} = { foo: 1 };  // OK!

// Only null and undefined fail:
// @ts-expect-error TS2322: Type 'null' is not assignable to type '{}'.
const d: {} = null;
```

### Example 4: Compiler Failure Case - Incompatible Property Types
```typescript
interface Cat {
  name: string;
  lives: number;
}

interface Dog {
  name: string;
  lives: string; // Type mismatch
}

const myDog: Dog = { name: 'Rex', lives: 'many' };
// @ts-expect-error TS2322: Type 'Dog' is not assignable to type 'Cat'. Types of property 'lives' are incompatible: Type 'string' is not assignable to type 'number'.
const myCat: Cat = myDog;
```

### Example 5: Idiomatic Exact Type Utility
```typescript
// Forcing strict exact types without excess properties
type Exactly<T, Shape> = T & Record<Exclude<keyof T, keyof Shape>, never>;

function createPoint<T extends Point2D>(point: Exactly<T, Point2D>): Point2D {
  return point;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The Freshness Bypass Bug**: Assigning an object literal to a temporary `const temp = { ... }` variable disables excess property checks, allowing accidental typos in option objects to compile silently without warnings.
2. **Weak Type Detection**: If an interface contains *only* optional properties (`{ a?: string; b?: number }`), passing an object with zero common properties (`{ x: 1 }`) fails with `TS2559: Type has no properties in common with type...` even though structurally it has no conflicts.
3. **Empty Interface `{}` vs `object`**: `object` represents any non-primitive type. `{}` represents any non-nullish type (including strings and numbers). `Record<string, never>` represents an actual empty object.
4. **Private/Protected Members Break Structural Typing**: If a class contains a `private` or `protected` member, TypeScript checks nominal identity for that member. Two classes with identical private fields are *not* assignable!

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Storing option objects in intermediate variables | Disables excess property checks; typos compile | Pass options directly as inline literals |
| Using `{}` to represent an arbitrary object | Accidental assignment of strings, numbers | Use `object` or `Record<string, unknown>` |
| Assuming classes are nominally typed | Unintended cross-assignment between classes | Add a private unique brand to classes |
| Weak types without common properties | **TS2559**: No properties in common error | Ensure at least one property matches |
| Mutating shared structurally assigned objects | Silent mutation of extra properties | Treat input objects as immutable or clone |

---

## 8. Performance & Type-Checker Complexity
- Structural subtype checking checks all target properties against the source type. In the worst case, checking complex nested types is $\mathcal{O}(P_{target} \times P_{source})$.
- The TypeScript compiler heavily mitigates this with the **Subtype Cache** and **Assignability Cache** (visible in `--extendedDiagnostics`), which store previous assignability evaluations by internal type IDs.

---

## 9. Compiler Internals
In TypeScript's `checker.ts`:
- `checkTypeRelatedTo(source, target, relation)` is the core engine for assignability.
- `hasExcessProperties(source, target)` checks if `source.flags & TypeFlags.FreshLiteral` is true. If true, it compares `getPropertyNames(source)` against `getPropertyNames(target)`.
- If an unknown property exists, it emits diagnostic `TS2353`.

---

## 10. Best Practices & Design Guidelines
- Always pass configuration and options objects as inline literals to benefit from excess property checks.
- If you need nominal typing on classes or objects, use branded types (`readonly _brand: unique symbol`).
- Use `Record<string, unknown>` instead of `{}` when expecting a dictionary object.
- Never rely on class constructor names for runtime verification; use explicit discriminant tags.
- When defining API input contracts, avoid weak types (interfaces where every property is optional) without a discriminant.

---

## 11. Real-World Case Study
A payment processor wrote an API client with an optional configuration:
```typescript
interface PaymentRequest {
  amountInCents: number;
  currency: 'USD' | 'EUR';
  idempotencyKey?: string;
}
```
A developer refactored code to build the request payload in a helper function:
```typescript
const payload = {
  amountInCents: 5000,
  currency: 'USD',
  idempotentKey: 'tx_12345', // TYPO: idempotentKey instead of idempotencyKey!
};
client.charge(payload);
```
Because `payload` was stored in a variable, freshness was lost. The compiler allowed the assignment without errors. In production, requests were executed without idempotency keys, resulting in $120,000 in duplicate customer charges during network retry storms.

**Prevention:**
The team enabled strict lint rules forbidding un-annotated variable assignments to API boundaries, ensuring all payloads are checked as fresh literals or explicitly annotated: `const payload: PaymentRequest = { ... }`.

---

## 12. Interview Questions

<details>
<summary>1. What is structural subtyping and how does it differ from nominal subtyping?</summary>

In structural subtyping (TypeScript), compatibility is based solely on the shape and structure of the types (properties and their types), regardless of their declaration names. In nominal subtyping (Java, C#), compatibility requires an explicit declaration of inheritance or implementation (`class A extends B`).
</details>

<details>
<summary>2. What is "Type Freshness" and when does it apply?</summary>

Type Freshness is TypeScript's mechanism for performing excess property checks on freshly created object literals. When an object literal is passed directly to an expression expecting a specific type, TypeScript raises an error if it contains properties not declared in the target type.
</details>

<details>
<summary>3. Why does `const x = { a: 1, b: 2 }; const y: { a: number } = x;` compile, but `const y: { a: number } = { a: 1, b: 2 };` fails?</summary>

The inline literal `{ a: 1, b: 2 }` is a "fresh" object literal subject to excess property checks. In the first case, assigning to `x` widens the type and strips freshness; subsequent assignment to `y` relies purely on structural subtyping, which permits extra properties.
</details>

<details>
<summary>4. How do private or protected members affect class assignability?</summary>

If a class has private or protected members, structural typing is suspended for those members. They must originate from the exact same class declaration; two separate classes with identical private fields are not assignable to each other.
</details>

<details>
<summary>5. What is a "Weak Type" in TypeScript?</summary>

An object type where all declared properties are optional (e.g. `{ a?: string; b?: number }`). To prevent accidental misassignments, TypeScript forbids assigning any type that has zero properties in common with the weak type.
</details>

<details>
<summary>6. What is the difference between `{}` and `object`?</summary>

`{}` represents any value that is not `null` or `undefined` (including primitives like strings and numbers). `object` represents any non-primitive reference type (functions, arrays, objects).
</details>

<details>
<summary>7. How can you enforce nominal typing in TypeScript?</summary>

By adding a unique symbol brand or private phantom property to the type:
```typescript
type UserId = string & { readonly __brand: unique symbol };
```
</details>

<details>
<summary>8. How does union assignability work?</summary>

A type $T$ is assignable to a union $A \cup B$ if $T$ is assignable to at least one member ($A$ or $B$). A union $A \cup B$ is assignable to $T$ only if both $A$ and $B$ are individually assignable to $T$.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Structural Equivalence**: Create two distinct classes `Meter` and `Yard` with property `value: number` and demonstrate that they are assignable to each other.
   <details>
   <summary>Solution</summary>

   ```typescript
   class Meter { constructor(public value: number) {} }
   class Yard { constructor(public value: number) {} }

   const m: Meter = new Yard(10); // Structurally valid
   ```
   </details>

2. **Freshness Fix**: Fix the compile error in `const opts: { timeout: number } = { timeout: 1000, debug: true };` without deleting `debug`.
   <details>
   <summary>Solution</summary>

   ```typescript
   const raw = { timeout: 1000, debug: true };
   const opts: { timeout: number } = raw; // Freshness stripped
   ```
   </details>

### Medium
3. **Exact Property Rejector**: Write a generic type `DisallowExcess<T, Expected>` that forces compile errors if `T` contains properties not present in `Expected`.
   <details>
   <summary>Solution</summary>

   ```typescript
   type DisallowExcess<T, Expected> = T & {
     [K in Exclude<keyof T, keyof Expected>]: never;
   };

   function validate<T extends { id: string }>(obj: DisallowExcess<T, { id: string }>) {
     return obj.id;
   }
   ```
   </details>

4. **Weak Type Guard**: Write a function that accepts an options object of weak type `interface WeakOpts { a?: number; b?: string }` and returns true only if at least one option is present.
   <details>
   <summary>Solution</summary>

   ```typescript
   interface WeakOpts {
     a?: number;
     b?: string;
   }

   function hasAnyOption(opts: WeakOpts): boolean {
     return opts.a !== undefined || opts.b !== undefined;
   }
   ```
   </details>

### Hard
5. **Nominal Brand Class Transformer**: Author a TypeScript decorator or base class `Nominal<Brand>` that prevents structural cross-assignment between any two inheriting classes.
   <details>
   <summary>Solution</summary>

   ```typescript
   export abstract class Nominal<Tag extends string> {
     // Protected property ensures only subclasses can access,
     // and unique tag breaks structural compatibility between distinct tags
     protected abstract readonly _brand: Tag;
   }

   class UsdCurrency extends Nominal<'USD'> {
     protected readonly _brand = 'USD';
     constructor(public cents: number) { super(); }
   }

   class EurCurrency extends Nominal<'EUR'> {
     protected readonly _brand = 'EUR';
     constructor(public cents: number) { super(); }
   }

   const usd = new UsdCurrency(100);
   // @ts-expect-error TS2322: Brands do not match
   const eur: EurCurrency = usd;
   ```
   </details>

6. **Deep Assignability Verifier Script**: Write a utility using the TypeScript Compiler API that evaluates whether Type A is assignable to Type B programmatically using `checker.isTypeAssignableTo`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function testAssignability(
     typeASource: string,
     typeBSource: string
   ): boolean {
     const code = `
       type A = ${typeASource};
       type B = ${typeBSource};
       declare const a: A;
       const b: B = a;
     `;

     const sourceFile = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);
     const defaultHost = ts.createCompilerHost({});
     const program = ts.createProgram(['test.ts'], { strict: true, noEmit: true }, {
       ...defaultHost,
       getSourceFile: (name) => name === 'test.ts' ? sourceFile : defaultHost.getSourceFile(name, ts.ScriptTarget.Latest),
     });

     const diagnostics = ts.getPreEmitDiagnostics(program, sourceFile);
     return diagnostics.length === 0;
   }
   ```
   </details>
