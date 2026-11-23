# Declaration Merging

> Declaration merging combines multiple separate declarations sharing the same identifier into a single unified symbol definition across interfaces, namespaces, functions, and enums.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [03-namespaces.md](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/03-namespaces.md), [00-declaration-files.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/00-declaration-files.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true` |

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
In JavaScript, an identifier can simultaneously represent multiple concepts:
- A function can have properties attached to it (`jQuery.ajax()`).
- An object can act as both a constructor and a namespace.
- Multiple scripts in a browser environment can add properties to the global `window` object.

In strict languages, declaring two entities with the exact same name causes a compile error. TypeScript resolves this with **Declaration Merging**: when two or more declarations share the same name in the same scope, the compiler merges their distinct type and value spaces into a single composite entity.

---

## 2. Core Concepts

### What Can Merge
TypeScript entities exist across three spaces: **Namespaces**, **Types**, and **Values**:
- **Interface + Interface**: Merges property members and overloads.
- **Function + Namespace**: Attaches static properties or sub-types to a function.
- **Class + Namespace**: Attaches static helper classes or utilities to a class.
- **Enum + Namespace**: Attaches helper methods to an enum.

### What CANNOT Merge
- **Type Alias + Type Alias**: Merging two `type Foo = ...` declarations is forbidden (`TS2300: Duplicate identifier`).
- **Class + Class**: Classes cannot merge with classes.
- **Variable + Variable**: Cannot merge value declarations.

---

## 3. How It Works Internally

```
First Declaration:
  interface Box { height: number; }

Second Declaration:
  interface Box { width: number; }

Compiler Symbol Table:
+------------------------------------+
| Symbol: Box                        |
| Flags: SymbolFlags.Interface       |
| Declarations: [Decl1, Decl2]       |
+------------------------------------+
                 |
                 v
Resulting Type:
  { height: number; width: number; }
```

Mechanics:
1. When the compiler registers an interface, it checks if a symbol already exists with `SymbolFlags.Interface`.
2. If it exists, the new declaration is appended to the symbol's `declarations` array.
3. Method overloads are merged in declaration order, with later interface declarations having higher priority (except for string literal parameter overloads, which bubble to the top).

---

## 4. Syntax & API Reference

### Function + Namespace Merging
```typescript
function buildWidget(label: string): { label: string } {
  return { label };
}

namespace buildWidget {
  export const defaultLabel = "Widget-01";
}

const w = buildWidget(buildWidget.defaultLabel);
```

### Enum + Namespace Merging
```typescript
enum Direction {
  NORTH = "N",
  SOUTH = "S",
}

namespace Direction {
  export function isNorth(dir: Direction): boolean {
    return dir === Direction.NORTH;
  }
}
```

---

## 5. Practical Examples

### 5.1 Basic Interface Merging
```typescript
interface UserProfile {
  name: string;
}

interface UserProfile {
  email: string;
}

const user: UserProfile = {
  name: "Ada Lovelace",
  email: "ada@computing.org",
};
```

### 5.2 Real-World Application: Static Factory Class Merging
```typescript
class Order {
  constructor(public id: string, public total: number) {}
}

namespace Order {
  export function createGuestOrder(total: number): Order {
    return new Order("guest_" + Date.now(), total);
  }
}

const order = Order.createGuestOrder(150);
```

### 5.3 Edge Case: Method Overload Ordering in Merged Interfaces
```typescript
interface DocumentViewer {
  render(element: HTMLElement): void; // Position 2
}

interface DocumentViewer {
  render(element: HTMLCanvasElement): void; // Position 1 (Later interface takes priority!)
  render(element: "special"): void;          // Position 0 (Literal parameters bubble to highest priority!)
}
```

### 5.4 Failure Case: Duplicate Incompatible Non-Method Properties
```typescript
interface Account {
  id: string;
}

// @ts-expect-error TS2717: Subsequent property declaration must have the same type. Property 'id' must be of type 'string', but here has type 'number'.
interface Account {
  id: number;
}
```

### 5.5 Idiomatic Pattern: Attaching Static Metadata to Enums
```typescript
enum ErrorCode {
  NOT_FOUND = "ERR_404",
  FORBIDDEN = "ERR_403",
}

namespace ErrorCode {
  export function getHttpStatus(code: ErrorCode): number {
    switch (code) {
      case ErrorCode.NOT_FOUND: return 404;
      case ErrorCode.FORBIDDEN: return 403;
    }
  }
}

console.log(ErrorCode.getHttpStatus(ErrorCode.NOT_FOUND)); // 404
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Type Aliases Do Not Merge**: `type A = { x: 1 }; type A = { y: 2 };` fails with `TS2300`. Only `interface` supports merging.
2. **Non-Method Properties Must Match Exactly**: If interface 1 defines `age: number`, interface 2 cannot define `age: string` or even `age?: number`.
3. **Literal Parameter Overloads Bubble Up**: In merged method signatures, any overload whose parameter is a string literal type jumps to the top of the overload list.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Problem | Solution | Compiler Error |
| :--- | :--- | :--- | :--- |
| Attempting to merge type aliases | Type aliases are strictly non-merging | Use `interface` instead of `type` | `TS2300: Duplicate identifier 'T'` |
| Changing property type in second interface | Conflicting property definition | Keep property types identical | `TS2717: Subsequent property declaration...` |
| Over-using merging in application code | Hard to track where properties were defined | Prefer explicit interface extension (`extends`) | High cognitive overhead |
| Merging function with class | Functions and classes both inhabit value space | Use namespace to attach statics | `TS2300: Duplicate identifier` |
| Accidentally merging with global library | Unintended property collisions | Use unique names or scoped modules | Unexpected global properties |
| Modifying method overload order unintentionally | Later interface takes higher priority | Order interface declarations intentionally | Overload resolution mismatches |

---

## 8. Performance & Type-Checker Complexity
- Declaration merging is efficient during binding, but wide chains of merged interfaces require scanning multiple AST declarations during member lookups.
- Prefer explicit inheritance (`interface B extends A`) over repeated declarations of the same interface when authoring domain application code.

---

## 9. Compiler Internals
- `src/compiler/binder.ts` checks `SymbolFlags.Interface` during symbol table insertion.
- The binder combines flags: `symbol.flags |= SymbolFlags.Interface`.
- `mergeSymbol` merges symbol tables and links declaration nodes into a linked list on `symbol.declarations`.

---

## 10. Best Practices & Design Guidelines
- Use declaration merging primarily for:
  - Global scope augmentation (`Window`, `ProcessEnv`).
  - Third-party library plugins (module augmentation).
  - Associating static utilities with functions or enums.
- In application domain logic, prefer explicit inheritance (`interface Admin extends User`) over splitting declarations.

---

## 11. Real-World Case Study
### Extending Standard DOM Interfaces
Web APIs evolve continuously. When a browser introduces a new experimental method on `Document` before TypeScript updates its standard library:
```typescript
interface Document {
  requestStorageAccess(): Promise<void>;
}

// User code can call it immediately with full type safety:
async function enableStorage() {
  if ("requestStorageAccess" in document) {
    await document.requestStorageAccess();
  }
}
```
TypeScript's standard library uses interface merging so developers can seamlessly polyfill and extend native browser contracts.

---

## 12. Interview Questions

<details>
<summary>1. What is Declaration Merging in TypeScript?</summary>

Declaration merging is the compiler's mechanism of combining two or more separate declarations sharing the same identifier in the same scope into a single unified symbol definition across interface, namespace, class, and enum declarations.
</details>

<details>
<summary>2. Why can interfaces merge while type aliases cannot?</summary>

Interfaces are open-ended by design to support gradual typing, standard library evolution, and third-party plugin extension. Type aliases represent closed, exact definitions of a type and cannot be re-declared without error.
</details>

<details>
<summary>3. How does method overload ordering work across merged interfaces?</summary>

Method overloads declared in later interface definitions take precedence (appear earlier in the overload list) over earlier interfaces, with one exception: signatures with string literal parameters always bubble to the top of the merged overload list.
</details>

<details>
<summary>4. What happens when two merged interfaces define the same non-method property with different types?</summary>

The compiler emits error `TS2717: Subsequent property declaration must have the same type`. Non-method properties must be of identical types.
</details>

<details>
<summary>5. How can you merge a namespace with a function?</summary>

By declaring a function and a namespace with the same name in the same scope. The namespace exports become static properties on the function object.
</details>

<details>
<summary>6. Can a class merge with another class?</summary>

No. Two class declarations with the same name will trigger error `TS2300: Duplicate identifier`.
</details>

<details>
<summary>7. How can an enum merge with a namespace?</summary>

Declaring a namespace with the same name as an enum allows attaching static helper methods and metadata functions directly to the enum object.
</details>

<details>
<summary>8. How does TypeScript's DOM standard library utilize declaration merging?</summary>

The TypeScript standard library defines DOM interfaces (like `Window`, `Document`, `HTMLElement`) across multiple internal files. They merge automatically into single complete contracts.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Merge Two Interfaces (Easy)
Merge two declarations of `Car` to include `make: string` and `model: string`.
<details>
<summary>Solution</summary>

```typescript
interface Car { make: string; }
interface Car { model: string; }

const myCar: Car = { make: "Toyota", model: "Corolla" };
```
</details>

### Exercise 2: Function with Static Counter (Easy)
Merge a function `nextId` with a namespace providing an incrementing counter.
<details>
<summary>Solution</summary>

```typescript
function nextId(): number {
  return ++nextId.current;
}

namespace nextId {
  export let current = 0;
}

console.log(nextId()); // 1
console.log(nextId()); // 2
```
</details>

### Exercise 3: Polyfill Native Array Method (Medium)
Declare a merged interface on `Array<T>` adding `groupBy(keySelector: (item: T) => string): Record<string, T[]>`.
<details>
<summary>Solution</summary>

```typescript
interface Array<T> {
  groupBy(keySelector: (item: T) => string): Record<string, T[]>;
}
```
</details>

### Exercise 4: Enum with Formatter Method (Medium)
Merge an enum `HttpStatus` with a namespace providing a `getMessage` helper.
<details>
<summary>Solution</summary>

```typescript
enum HttpStatus {
  OK = 200,
  BAD_REQUEST = 400,
}

namespace HttpStatus {
  export function getMessage(status: HttpStatus): string {
    switch (status) {
      case HttpStatus.OK: return "Success";
      case HttpStatus.BAD_REQUEST: return "Bad Request";
    }
  }
}

console.log(HttpStatus.getMessage(HttpStatus.OK)); // "Success"
```
</details>

### Exercise 5: Class with Companion Namespace Factory (Hard)
Merge a class `ComplexNumber` with a companion namespace providing polar coordinate factories.
<details>
<summary>Solution</summary>

```typescript
class ComplexNumber {
  constructor(public real: number, public imag: number) {}
}

namespace ComplexNumber {
  export function fromPolar(r: number, theta: number): ComplexNumber {
    return new ComplexNumber(r * Math.cos(theta), r * Math.sin(theta));
  }
}

const c = ComplexNumber.fromPolar(1, Math.PI / 2);
```
</details>

### Exercise 6: Multi-Stage Method Overload Merging (Hard)
Demonstrate how method overload precedence behaves when merging three distinct interface declarations.
<details>
<summary>Solution</summary>

```typescript
interface Parser {
  parse(input: any): unknown; // Priority 3
}

interface Parser {
  parse(input: string): object; // Priority 2
}

interface Parser {
  parse(input: "json"): Record<string, any>; // Priority 1 (string literal bubbles to top!)
}

declare const p: Parser;
const r1 = p.parse("json"); // Returns Record<string, any>
const r2 = p.parse("other"); // Returns object
```
</details>
