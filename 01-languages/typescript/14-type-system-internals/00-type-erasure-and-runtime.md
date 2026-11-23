# Type Erasure and Runtime Representation in TypeScript

> Deep dive into TypeScript's type erasure phase: compile-time artifacts vs runtime JavaScript, syntax downleveling, reflection limitations, and the TC39 Type Annotations (Erasable Syntax) proposal.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type System Internals |
| **Difficulty** | Advanced |
| **Prerequisites** | [01-fundamentals/00-types-and-inference.md](../01-fundamentals/00-types-and-inference.md), [13-compiler-and-tsconfig/01-compiler-architecture.md](../13-compiler-and-tsconfig/01-compiler-architecture.md) |
| **TypeScript Baseline** | TS 1.0+, TS 5.0+ (`verbatimModuleSyntax`), TC39 Stage 1 (Type Annotations) |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "verbatimModuleSyntax": true` |

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
TypeScript is a compile-time static type system that operates via **Type Erasure**. During compilation, the TypeScript emitter strips away all type annotations, interfaces, type aliases, and type assertions, leaving behind clean, standard JavaScript. At runtime, JavaScript engines (V8, JavaScriptCore) have zero knowledge of TypeScript types.

Understanding type erasure is essential to avoid common architectural fallacies: attempting to inspect interface types at runtime, expecting generic types to survive JSON serialization, or assuming `instanceof` can evaluate TypeScript interfaces. Furthermore, modern runtimes (like Node.js 22 with `--experimental-strip-types`) and the TC39 "Type Annotations" proposal depend strictly on TypeScript's syntax remaining purely erasable.

---

## 2. Core Concepts
- **Purely Erasable Constructs**: Features that emit zero JavaScript code when stripped:
  - `interface`, `type` aliases.
  - Type annotations on variables, parameters, and return types (`: string`).
  - Type assertions (`as User`, `<string>x`).
  - Generic type arguments (`<T>`).
  - `declare` statements and ambient declarations.
- **Non-Erasable Constructs (Historical TypeScript Syntax)**: Features that generate runtime JavaScript code:
  - `enum` and `const enum` (generates lookup objects or inlined constants).
  - `namespace` (generates IIFE closures).
  - Constructor parameter properties (`constructor(public name: string) {}`).
  - Experimental legacy decorators (generates metadata emission via `reflect-metadata`).
- **No Runtime Type Introspection**: Unlike languages like C# or Java with bytecode reflection, TypeScript types cannot be queried via `typeof x === "User"` or `Reflect.getType()`.
- **TC39 Type Annotations Proposal**: Standardizing syntax in JavaScript engines to treat TypeScript-like type annotations as whitespace/comments, enabling direct execution without compilation.

---

## 3. How It Works Internally

```
      TypeScript Source Code
      function greet(user: User): string {
        return `Hello, ${user.name}`;
      }
                |
                v
      +-------------------+
      | TypeScript Parser | Creates AST with TypeAnnotation nodes
      +---------+---------+
                |
                v
      +-------------------+
      | Type Checker      | Validates types statically (compile-time only)
      +---------+---------+
                |
                v
      +-------------------+
      | TypeScript Emitter| Strips SyntaxKind.TypeAnnotation nodes
      +---------+---------+
                |
                v
      Emitted JavaScript
      function greet(user) {
        return `Hello, ${user.name}`;
      }
```

The emitter walks the AST and simply skips writing any token or sub-tree that represents a type. Because the AST positions of runtime expressions remain intact, source mapping is direct and whitespace preservation is trivial.

---

## 4. Syntax & API Reference

```typescript
// 1. Purely Erasable Interface vs Class
interface UserInterface {
  id: string; // Stripped completely
}

class UserClass {
  constructor(public id: string) {} // Emits JavaScript constructor and property assignment!
}

// 2. Type-Only Imports (Guaranteed Erasure)
import type { DatabaseRecord } from './db.js';

// 3. Ambient Type Declaration
declare const __BUILD_VERSION__: string; // Emits zero code
```

---

## 5. Practical Examples

### Example 1: Demonstrating Type Erasure Output
```typescript
// Source TypeScript:
interface Point {
  x: number;
  y: number;
}

function calculateDistance(p1: Point, p2: Point): number {
  const dx = p1.x - p2.x;
  const dy = p1.y - p2.y;
  return Math.sqrt(dx * dx + dy * dy);
}

// Emitted JavaScript (Target ES2022):
// function calculateDistance(p1, p2) {
//   const dx = p1.x - p2.x;
//   const dy = p1.y - p2.y;
//   return Math.sqrt(dx * dx + dy * dy);
// }
```

### Example 2: Erasable vs Non-Erasable Enums
```typescript
// Non-Erasable (Emits IIFE object):
enum Direction {
  Up = 'UP',
  Down = 'DOWN',
}

// Purely Erasable Alternative (TC39 Compatible):
const DirectionMap = {
  Up: 'UP',
  Down: 'DOWN',
} as const;
type DirectionType = (typeof DirectionMap)[keyof typeof DirectionMap];
```

### Example 3: Edge Case - Constructor Parameter Properties
```typescript
// Non-erasable syntax:
class Product {
  constructor(public readonly id: string, public price: number) {}
}

// Transpiles to runtime JavaScript:
// class Product {
//   id; price;
//   constructor(id, price) {
//     this.id = id;
//     this.price = price;
//   }
// }

// Purely erasable alternative (works with Node 22 type-stripping):
class PureProduct {
  id: string;
  price: number;
  constructor(id: string, price: number) {
    this.id = id;
    this.price = price;
  }
}
```

### Example 4: Compiler Failure Case - Treating Interface as Runtime Value
```typescript
interface AdminUser {
  role: 'ADMIN';
}

function verifyUser(obj: unknown) {
  // @ts-expect-error TS2693: 'AdminUser' only refers to a type, but is being used as a value here.
  if (obj instanceof AdminUser) {
    console.log('Is admin');
  }
}
```

### Example 5: Idiomatic Runtime Validation with Type Guards
```typescript
// Bridging compile-time types with runtime validation
interface Config {
  port: number;
  host: string;
}

export function isConfig(val: unknown): val is Config {
  return (
    typeof val === 'object' &&
    val !== null &&
    'port' in val &&
    typeof (val as Record<string, unknown>).port === 'number' &&
    'host' in val &&
    typeof (val as Record<string, unknown>).host === 'string'
  );
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`instanceof` on Interfaces**: In JavaScript, `instanceof` checks prototype inheritance chains. Because interfaces do not exist at runtime, `x instanceof SomeInterface` throws compiler error `TS2693`.
2. **Node.js 22 Type Stripping Restrictions**: When running `node --experimental-strip-types`, Node.js strips types using a fast syntax scanner that does *not* support non-erasable TypeScript features (`enums`, `namespaces`, constructor parameter properties). Using them throws a runtime `SyntaxError`.
3. **Circular Value vs Type Imports**: If Module A imports a type from Module B using `import { TypeA } from './b.js'` without `type`, older bundlers may retain the import at runtime, triggering circular runtime dependency errors. Always use `import type`.
4. **Metadata Reflection Limitations**: Without `emitDecoratorMetadata: true` and the `reflect-metadata` polyfill, generic types cannot be inspected at runtime. Even with metadata enabled, generics are erased to `Object`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `if (user instanceof UserInterface)` | **TS2693**: Interface used as a value | Use custom type predicate or class |
| Relying on `enums` with Node type-stripping | Runtime `SyntaxError: Unexpected token 'enum'` | Use `as const` object dictionaries |
| Expecting generics to validate JSON | Unvalidated data enters application | Use runtime validator (Zod, Valibot) |
| Parameter properties in erasable projects | Breaks single-file transpilers (swc, node strip) | Explicitly declare properties and assign in constructor |
| Importing types without `type` modifier | Bundler fails to tree-shake empty files | Use `import type` or `verbatimModuleSyntax` |

---

## 8. Performance & Type-Checker Complexity
- Type erasure ensures that TypeScript applications run at the exact same native speed as hand-written JavaScript. There is **zero runtime overhead** added by TypeScript type annotations.
- Erasing types in AST transforms is an $\mathcal{O}(N)$ streaming operation over the AST nodes, taking only a few milliseconds even for massive files.

---

## 9. Compiler Internals
In TypeScript's `src/compiler/emitter.ts`:
- The emitter checks the `SyntaxKind` of AST nodes.
- If `isTypeNode(node)` or `node.flags & NodeFlags.TypeExclusively` is true, the emitter emits nothing and advances its print writer offset.
- In `verbatimModuleSyntax`, the emitter checks if an import specifier has the `isTypeOnly` flag; if true, the specifier is omitted from the emitted import declaration.

---

## 10. Best Practices & Design Guidelines
- Write **purely erasable TypeScript**: avoid `enum`, `namespace`, and constructor parameter properties to ensure compatibility with Node.js 22 type-stripping and modern fast bundlers.
- Use `import type` for all imports that are solely referenced in type positions.
- Use schema validation libraries (Zod, ArkType, Valibot) when bridging untrusted runtime inputs (network requests, user forms) with static types.
- Never expect generic type parameters to be inspectable at runtime.
- Design domain models using plain interfaces paired with user-defined type guards.

---

## 11. Real-World Case Study
A development team upgraded their backend microservices to Node.js 22 to take advantage of native TypeScript execution (`node --experimental-strip-types`) without a build step. However, the application crashed immediately on startup with `SyntaxError: Unexpected token 'enum'`.

**Audit and Refactoring:**
1. Identified 42 legacy `enum` declarations across domain models.
2. Migrated enums to erasable const objects:
   ```typescript
   export const UserRole = { Admin: 'ADMIN', Member: 'MEMBER' } as const;
   export type UserRole = (typeof UserRole)[keyof typeof UserRole];
   ```
3. Refactored constructor parameter properties to standard property initializers.
The entire backend booted directly in Node.js 22 with zero transpilation build step, cutting developer container startup times by 70%.

---

## 12. Interview Questions

<details>
<summary>1. What is type erasure in TypeScript?</summary>

Type erasure is the process by which the TypeScript compiler strips all type annotations, interfaces, type aliases, and compile-time constructs from the source code, outputting clean standard JavaScript with zero runtime type overhead.
</details>

<details>
<summary>2. Why does `typeof x === "MyInterface"` not work in TypeScript?</summary>

Because `MyInterface` is completely erased during compilation and does not exist in the JavaScript runtime environment. `typeof` is a runtime JavaScript operator that only evaluates to primitives (`"string"`, `"number"`, `"object"`, etc.).
</details>

<details>
<summary>3. Which TypeScript syntax features are NOT purely erasable?</summary>

- `enum` and `const enum`
- `namespace`
- Constructor parameter properties (`constructor(public x: number)`)
- Legacy experimental decorators with type metadata
</details>

<details>
<summary>4. What is the TC39 "Type Annotations" (Erasable Syntax) proposal?</summary>

A proposal to standardize type annotation syntax in the ECMAScript language specification itself, allowing JavaScript engines to treat TypeScript-like annotations as whitespace/comments and execute TypeScript files directly without any build step.
</details>

<details>
<summary>5. Can you inspect generic type parameters at runtime in TypeScript?</summary>

No. Generic type parameters exist solely in the compiler's type checker and are completely erased during emission. A function `function foo<T>()` has no way of knowing what type `T` was passed at runtime.
</details>

<details>
<summary>6. How does `import type` interact with type erasure?</summary>

`import type` explicitly tells the compiler that the imported symbol is purely a type. The emitter guarantees that this import is 100% eliminated from the emitted JavaScript output, preventing unused runtime module imports.
</details>

<details>
<summary>7. What does Node.js 22's `--experimental-strip-types` feature do?</summary>

It uses an ultra-fast C++ scanner to replace TypeScript type annotations with whitespace in memory before executing the file in V8, allowing native TypeScript execution provided the code uses only purely erasable syntax.
</details>

<details>
<summary>8. How should runtime type validation be implemented in a type-erased language?</summary>

By using runtime validation libraries (such as Zod, Valibot, or TypeBox) or custom user-defined type guards (`x is T`) that perform explicit structural property checks at runtime.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Erasable Refactor**: Convert `enum Priority { Low = 0, High = 1 }` into a purely erasable TypeScript pattern.
   <details>
   <summary>Solution</summary>

   ```typescript
   export const Priority = {
     Low: 0,
     High: 1,
   } as const;
   export type Priority = (typeof Priority)[keyof typeof Priority];
   ```
   </details>

2. **Type Guard Bridge**: Write a type guard `isNumberArray(val: unknown): val is number[]`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function isNumberArray(val: unknown): val is number[] {
     return Array.isArray(val) && val.every(item => typeof item === 'number');
   }
   ```
   </details>

### Medium
3. **Class Parameter Property Refactor**: Refactor `class Customer { constructor(public id: string, public email: string) {} }` into purely erasable syntax for Node.js 22 type stripping.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class Customer {
     id: string;
     email: string;

     constructor(id: string, email: string) {
       this.id = id;
       this.email = email;
     }
   }
   ```
   </details>

4. **Runtime Brand Validator**: Create a branded type `type Email = string & { readonly __brand: unique symbol }` and implement a runtime validation constructor `makeEmail(input: string): Email | null`.
   <details>
   <summary>Solution</summary>

   ```typescript
   declare const EmailBrand: unique symbol;
   export type Email = string & { readonly [EmailBrand]: true };

   export function makeEmail(input: string): Email | null {
     if (/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(input)) {
       return input as Email;
     }
     return null;
   }
   ```
   </details>

### Hard
5. **AST Type Stripper Script**: Using the TypeScript Compiler API, write a Node.js utility `stripTypesFromCode(code: string): string` that parses TypeScript source text and outputs pure JavaScript by visiting and erasing all `TypeNode` spans.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function stripTypesFromCode(code: string): string {
     const result = ts.transpileModule(code, {
       compilerOptions: {
         target: ts.ScriptTarget.ES2022,
         module: ts.ModuleKind.ESNext,
       },
     });
     return result.outputText;
   }
   ```
   </details>

6. **Non-Erasable Syntax Linter**: Implement an AST visitor function using `ts.createSourceFile` that scans a TypeScript file and raises errors if it detects any non-erasable constructs (`EnumDeclaration`, `ModuleDeclaration` with namespace, or parameter properties in `ConstructorDeclaration`).
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function auditErasability(sourceCode: string): string[] {
     const sourceFile = ts.createSourceFile('test.ts', sourceCode, ts.ScriptTarget.Latest, true);
     const violations: string[] = [];

     function visit(node: ts.Node) {
       if (ts.isEnumDeclaration(node)) {
         violations.push(`Enum declaration '${node.name.text}' is not purely erasable.`);
       }
       if (ts.isModuleDeclaration(node) && !node.modifiers?.some(m => m.kind === ts.SyntaxKind.DeclareKeyword)) {
         violations.push(`Namespace '${node.name.text}' is not purely erasable.`);
       }
       if (ts.isParameterPropertyDeclaration(node, node.parent)) {
         violations.push(`Constructor parameter property is not purely erasable.`);
       }
       ts.forEachChild(node, visit);
     }

     visit(sourceFile);
     return violations;
   }
   ```
   </details>
