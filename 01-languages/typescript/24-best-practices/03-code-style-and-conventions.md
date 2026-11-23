# TypeScript Code Style, Conventions, and Consistency

> Comprehensive guide to enterprise TypeScript conventions: naming standards, interfaces vs type aliases, explicit return types on boundaries, and formatting rules.

| Metadata | Details |
| :--- | :--- |
| **Category** | Best Practices |
| **Difficulty** | Intermediate |
| **Prerequisites** | [02-interfaces-and-types/00-interfaces.md](../02-interfaces-and-types/00-interfaces.md), [21-production-tooling/00-linting-and-formatting.md](../21-production-tooling/00-linting-and-formatting.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
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

In multi-developer engineering organizations, inconsistent coding styles create code review friction, confuse new hires, and mask genuine logic errors. Common debates include:
- When to use `interface` vs `type`?
- Should interfaces be prefixed with `I` (`IUser`)?
- Should function return types always be explicitly annotated?
- How should enums, types, and constants be cased?

Establishing unambiguous, automated style guidelines unifies the codebase, maximizes compiler performance, and maintains pristine codebase health.

---

## 2. Core Concepts

### The Authoritative Naming Standards
- **Types & Interfaces**: PascalCase (`UserSession`, `PaymentRequest`). Never prefix with `I` (anti-pattern from C#).
- **Generics**: Single uppercase letters for simple types (`T`, `U`), or descriptive PascalCase prefixed with `T` for complex types (`TEntity`, `TPayload`).
- **Variables & Functions**: camelCase (`fetchOrders`, `userCount`).
- **Constants**: UPPER_SNAKE_CASE for global static literals (`MAX_RETRIES`), camelCase for module objects.
- **Enums & Const Maps**: PascalCase object, UPPER_SNAKE_CASE keys.

```
+-------------------------------------------------------------------------+
|                  TypeScript Naming Conventions Matrix                   |
+-------------------------------------------------------------------------+
|                                                                         |
|  Identifier Kind         Convention             Example                 |
|  ---------------------------------------------------------------------  |
|  Interface               PascalCase (No I)      UserProfile             |
|  Type Alias              PascalCase             PaymentResult           |
|  Generic Type Param      T or TPrefix           T, TRecord              |
|  Function / Method       camelCase              calculateTax()          |
|  Constant Literal        UPPER_SNAKE_CASE       DEFAULT_TIMEOUT_MS      |
|  Enum / Const Map        PascalCase             HttpStatus.OK           |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Interface vs. Type Alias Decision Rule
- **Use `interface`**: For public API models, service contracts, object hierarchies, and extendable architectural structures (interfaces benefit from declaration merging and compiler symbol caching).
- **Use `type`**: For unions, primitives, tuples, mapped types, and complex conditional type algebra.

---

## 3. How It Works Internally

- Naming conventions are enforced via AST linting rules (`@typescript-eslint/naming-convention`), querying node syntax kinds (`Identifier`, `TypeParameter`, `InterfaceDeclaration`).
- Explicit boundary return types prevent accidental type leaks where internal implementation types unintentionally become part of the public API signature.

---

## 4. Syntax & API Reference

### ESLint Naming Convention Configuration
```javascript
export default [
  {
    rules: {
      '@typescript-eslint/naming-convention': [
        'error',
        { selector: 'interface', format: ['PascalCase'], custom: { regex: '^I[A-Z]', match: false } },
        { selector: 'typeAlias', format: ['PascalCase'] },
        { selector: 'variable', format: ['camelCase', 'UPPER_CASE'] },
        { selector: 'typeParameter', format: ['PascalCase'] },
      ],
      '@typescript-eslint/explicit-module-boundary-types': 'error',
    },
  },
];
```

---

## 5. Practical Examples

### Example 1: Explicit Boundary Return Types
```typescript
// BAD: Inferred return type leaks internal database entity to public caller
export function getCustomer(id: string) {
  return { id, email: "user@test.com", passwordHash: "secret_123" };
}

// GOOD: Explicit return type restricts exposed shape
export interface CustomerDTO {
  id: string;
  email: string;
}

export function getSafeCustomer(id: string): CustomerDTO {
  return { id, email: "user@test.com" };
}
```

### Example 2: Descriptive Generics
```typescript
// POOR: Ambiguous single letters for complex signatures
// function transform<T, U, V>(a: T, b: U): V

// IDIOMATIC: Descriptive T-prefixed generics
export function mapPayload<TSource, TTarget>(
  source: TSource,
  transformer: (item: TSource) => TTarget
): TTarget {
  return transformer(source);
}
```

### Example 3: Edge Case - Declaration Merging with Interfaces
```typescript
// Interfaces allow declaration merging (useful for plugin systems)
export interface AppPluginRegistry {
  auth: { version: string };
}

// Another module can augment the registry cleanly:
export interface AppPluginRegistry {
  analytics: { trackerId: string };
}
// Type aliases CANNOT be merged (throws duplicate identifier error)
```

### Example 4: Compiler Error & Interface Prefix Rejection
```typescript
// Under @typescript-eslint/naming-convention rule (no 'I' prefix):
// Lint Error: Interface name must not match /^I[A-Z]/
interface IUser {
  name: string;
}

// Idiomatic:
interface User {
  name: string;
}
```

### Example 5: Consistent Const Object Lookup
```typescript
export const ORDER_STATUS = {
  Pending: "PENDING",
  Processing: "PROCESSING",
  Completed: "COMPLETED",
} as const;

export type OrderStatus = (typeof ORDER_STATUS)[keyof typeof ORDER_STATUS];
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **The `I` Prefix Anti-Pattern**:
   Developers coming from C# often write `IUserService`. In TypeScript, interfaces and classes inhabit the same structural design space. Prefixing interfaces with `I` leaks implementation details and is rejected by the official TypeScript team.
2. **Implicit Any in Function Boundaries**:
   Omitting return types on exported functions slows down compiler type resolution across module boundaries.
3. **Overusing JSDoc Comments**:
   Avoid writing obvious comments (`/** The name */ name: string`). Write comments only to explain *why* non-intuitive logic exists.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **`I` Interface Prefix** | `interface IUser` | `interface User` | Non-idiomatic in TypeScript |
| **Missing Boundary Returns** | Exporting unannotated functions | Annotate all exported function returns | Leaks internal types, slows `tsc` |
| **Obscure Single-Letter Generics** | `<T, K, V, X, Y>` in business code | `<TEntity, TResponse>` | Unreadable generic signatures |
| **Inconsistent Case in Types** | `type user_data = ...` | `type UserData = ...` | Violates ecosystem conventions |
| **Using `any` for Missing Types** | `const data: any` | `const data: unknown` | Destroys type safety |
| **Comments Duplicating Types** | `// string id\nid: string` | Self-documenting code | Pointless trivia noise |

---

## 8. Performance & Type-Checker Complexity

- Explicit return types on exported module boundaries prevent cascading type inference, improving incremental compiler build times by **20%–35%**.

---

## 9. Compiler Internals

When a function return type is omitted:
1. `ts.TypeChecker` invokes `checkReturnStatement` on every return expression in the AST.
2. It synthesizes a union type of all possible return values.
3. With an explicit annotation, the compiler simply performs an assignability check (`isTypeAssignableTo`), terminating earlier.

---

## 10. Best Practices & Design Guidelines

1. **Automate Conventions via Linter**: Enforce naming and formatting in CI.
2. **Never Prefix Interfaces with `I`**: Use clean PascalCase.
3. **Always Annotate Exported Function Returns**: Safeguard public API contracts.
4. **Prefer Descriptive Generic Names**: Use `TRequest`, `TResponse`.

---

## 11. Real-World Case Study

An enterprise banking API experienced broken client integration contracts when an engineer refactored an internal helper function. Because the route handler omitted an explicit return type, the compiler silently inferred a widened internal schema, breaking mobile client JSON parsing.

**Fix**:
Enabled `@typescript-eslint/explicit-module-boundary-types` in CI. The rule blocked the PR, forcing the developer to supply explicit DTO return annotations.

---

## 12. Interview Questions

<details>
<summary>1. When should you choose `interface` over `type`?</summary>

Use `interface` for object models, public library contracts, and OOP architectures because interfaces support declaration merging and compile-time symbol caching. Use `type` for unions, intersections, primitives, tuples, and mapped/conditional type algebra.
</details>

<details>
<summary>2. Why does the TypeScript community discourage the `I` prefix (e.g. `IUser`) for interfaces?</summary>

TypeScript is structurally typed, not nominally typed like C# or Java. An interface describes a shape, and consumers should not care whether an object is an interface, a class instance, or a type alias. The `I` prefix leaks implementation details.
</details>

<details>
<summary>3. Why are explicit return types recommended on exported functions?</summary>

They prevent accidental changes to public API contracts, speed up compiler build times by eliminating recursive return type inference, and provide clear documentation for consumers.
</details>

<details>
<summary>4. What is declaration merging in TypeScript interfaces?</summary>

When multiple interface declarations share the same name in the same scope, the compiler merges their property definitions into a single unified interface.
</details>

<details>
<summary>5. How should generic type parameters be named?</summary>

For simple utility functions, standard single letters (`T`, `U`) are acceptable. For domain logic and complex APIs, use descriptive PascalCase prefixed with `T` (e.g. `TEntity`, `TFilter`, `TContext`).
</details>

<details>
<summary>6. How can code style conventions be enforced automatically?</summary>

Using `@typescript-eslint/naming-convention` in ESLint combined with formatting tools like Biome or Prettier configured in pre-commit hooks.
</details>

<details>
<summary>7. What does the `@typescript-eslint/explicit-module-boundary-types` rule do?</summary>

It requires explicit type annotations on the inputs and outputs of functions that are exported from a module, ensuring public API contracts are always documented in code.
</details>

<details>
<summary>8. Why is self-documenting code preferred over JSDoc for simple TypeScript types?</summary>

TypeScript type annotations already describe the shape, nullability, and structure of data. Comments that merely restate type definitions quickly fall out of date and add visual clutter.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Add explicit return type annotation</summary>

**Task**: Add explicit return type to this exported helper function.
```typescript
export function createUser(id: string, name: string) {
  return { id, name, createdAt: new Date() };
}
```

**Solution**:
```typescript
export interface UserRecord {
  id: string;
  name: string;
  createdAt: Date;
}

export function createUser(id: string, name: string): UserRecord {
  return { id, name, createdAt: new Date() };
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Fix interface naming violation</summary>

**Task**: Refactor `interface IAuthToken` and its usage to conform to TypeScript standards.

**Solution**:
```typescript
export interface AuthToken {
  token: string;
  expiresIn: number;
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Descriptive Generic Naming</summary>

**Task**: Refactor `function pipe<A, B, C>(a: A, f1: (x: A) => B, f2: (y: B) => C): C` to use descriptive generics.

**Solution**:
```typescript
export function pipe<TInput, TIntermediate, TOutput>(
  initial: TInput,
  step1: (input: TInput) => TIntermediate,
  step2: (intermediate: TIntermediate) => TOutput
): TOutput {
  return step2(step1(initial));
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Consistent Enum Replacement Pattern</summary>

**Task**: Write an idiomatic `as const` object map and derived union type for `LogLevel`.

**Solution**:
```typescript
export const LOG_LEVEL = {
  Debug: "DEBUG",
  Info: "INFO",
  Warn: "WARN",
  Error: "ERROR",
} as const;

export type LogLevel = (typeof LOG_LEVEL)[keyof typeof LOG_LEVEL];
```
</details>

<details>
<summary>Challenge 5 (Hard): AST Linter for Explicit Boundary Returns</summary>

**Task**: Sketch an ESLint rule selector that reports any exported function without a return type annotation.

**Solution**:
```javascript
export const rule = {
  create(context) {
    return {
      'ExportNamedDeclaration > FunctionDeclaration'(node) {
        if (!node.returnType) {
          context.report({
            node,
            message: 'Exported functions must have an explicit return type annotation.',
          });
        }
      },
    };
  },
};
```
</details>

<details>
<summary>Challenge 6 (Hard): Interface Declaration Merging Plugin Pattern</summary>

**Task**: Implement an extensible CLI command registry using interface declaration merging.

**Solution**:
```typescript
// core-registry.ts
export interface CommandRegistry {
  help: () => void;
  version: () => void;
}

export class CLI {
  private commands: Partial<CommandRegistry> = {};

  public register<K extends keyof CommandRegistry>(cmd: K, fn: CommandRegistry[K]): void {
    this.commands[cmd] = fn;
  }
}

// plugin-deploy.ts (Augments registry in another file)
declare module "./core-registry.js" {
  interface CommandRegistry {
    deploy: (target: string) => Promise<void>;
  }
}
```
</details>
