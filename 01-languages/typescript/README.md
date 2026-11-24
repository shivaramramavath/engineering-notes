# TypeScript Engineering Curriculum & Technical Reference

> A production-grade, exhaustive curriculum and technical reference manual for modern TypeScript engineering (TypeScript 5.0 to 5.8+ and TypeScript 7.0 Native Go Port `tsgo`). Covers structural type theory, generic metaprogramming, compiler internals, full-stack typing, enterprise design patterns, and high-performance algorithms.

---

## 🏛️ Curriculum Architecture & Module Overview

The TypeScript curriculum is partitioned into **30 specialized modular tracks** spanning fundamentals, type systems, compiler mechanics, runtime integration, enterprise architecture, and technical interview mastery.

```text
+----------------------------------------------------------------------------------------------------+
|                               TYPESCRIPT ENGINEERING TRACKS (00 - 29)                              |
+------------------------------------+-----------------------------------+---------------------------+
| FOUNDATIONS & CORE TYPINGS         | TYPE SYSTEM & RUNTIME SYSTEMS     | ENTERPRISE & ARCHITECTURE |
|  00-setup                          |  10-advanced-types (Conditional)  |  19-react-and-frontend    |
|  01-basics-and-primitive-types     |  11-error-handling                |  20-nodejs-backend        |
|  02-functions                      |  12-async-and-promises            |  21-production-tooling    |
|  03-objects-and-interfaces         |  13-compiler-and-configuration    |  22-performance           |
|  04-union-and-intersection-types   |  14-compiler-internals            |  23-security              |
|  05-classes-and-oop                |  15-runtime-type-checking (Zod)   |  24-best-practices        |
|  06-generics                       |  16-type-safe-apis (tRPC, RPC)    |  25-real-world-patterns   |
|  07-utility-types                  |  17-design-patterns (GoF & Typed) |  26-projects (14 Apps)    |
|  08-modules-and-namespaces         |  18-testing (Vitest & Jest)       |  27-interview (Mastery)   |
|  09-declaration-files                                                  |  28-cheatsheets (QuickRef)|
|                                                                        |  29-typed-dsa (Algorithms)|
+----------------------------------------------------------------------------------------------------+
```

---

## ⚙️ Verified Toolchain & Platform Baseline

Every code example, benchmark, test case, and architectural demonstration throughout this curriculum has been tested and verified against:
- **Baseline TypeScript:** **TypeScript 5.0 - 5.8+** (with full forward compatibility for **TypeScript 7.0 Native Go Port `tsgo`**)
- **Runtime Baseline:** **Node.js 22 LTS** (incorporating native type-stripping via `node --experimental-strip-types`) and evergreen browsers
- **Compilation Parameters:** Strict type checking suite:
  ```json
  {
    "strict": true,
    "noImplicitAny": true,
    "strictNullChecks": true,
    "strictFunctionTypes": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "verbatimModuleSyntax": true,
    "isolatedModules": true
  }
  ```
- **Validation & Tooling:** Zod 3.23+, Vitest 2.0+, typescript-eslint v8 (Flat Config), Biome 1.8+

---

## 📐 Unified Note Architecture

Every concept note strictly follows the unified 13-section architectural standard defined in `_templates/concept-template.md`:
1. **What & Why**: Industry motivation, problems solved, and mental model.
2. **Core Concepts**: Theoretical axioms, structural subtyping rules, and formal type models.
3. **How It Works Internally**: AST nodes, Binder symbols, Checker type representations, and ASCII diagrams.
4. **Syntax & API Reference**: Exhaustive declaration options, keywords, and compiler flags.
5. **Code Examples**: 5 complete verified implementations (Basic, Real-World, Edge Case, Failure Case with `@ts-expect-error`, Idiomatic Pattern).
6. **Edge Cases & Pitfalls**: Subtle bugs, type widening traps, EPC quirks, and runtime boundaries.
7. **Common Mistakes & Anti-Patterns**: Structured tables detailing symptom, cause, compiler error code, and correct fix.
8. **Performance & Complexity**: Compiler type-checking latency, AST node volume, and runtime V8 engine impacts.
9. **Compiler Internals**: Interaction with Scanner, Parser, Binder, Checker, and Emitter.
10. **Best Practices**: Enterprise production guidelines, naming rules, and ESLint configs.
11. **Real-World Case Study**: Production architecture from high-scale real-world codebases.
12. **Interview Questions**: 8 progressive interview questions with collapsible `<details>` answers.
13. **Practical Exercises & Challenges**: 6 self-contained coding challenges with `<details>` solutions.

---

## 🗂️ Module Directory

| Module | Name | Key Topics Covered |
|---|---|---|
| [00-setup](./00-setup/README.md) | Setup & Toolchain | tsc, tsconfig.json, tsx, Node 22 type stripping, TS 7 native go compiler |
| [01-basics-and-primitive-types](./01-basics-and-primitive-types/README.md) | Primitives & Types | Primitives, type inference, any vs unknown, never, void, arrays, tuples |
| [02-functions](./02-functions/README.md) | Functions & Callables | Function types, parameter flags, overloading, this typing, assertion functions |
| [03-objects-and-interfaces](./03-objects-and-interfaces/README.md) | Objects & Interfaces | Structural typing, EPC, declaration merging, index signatures, readonly |
| [04-union-and-intersection-types](./04-union-and-intersection-types/README.md) | Unions & Intersections | Discriminated unions, exhaustive switch, CFA, intersections, type guards |
| [05-classes-and-oop](./05-classes-and-oop/README.md) | Classes & OOP | Access modifiers, ECMAScript private `#`, abstract classes, parameter properties |
| [06-generics](./06-generics/README.md) | Generics & Constraints | Type parameters, `extends` constraints, default types, variance, `infer` |
| [07-utility-types](./07-utility-types/README.md) | Utility Types | Partial, Required, Readonly, Record, Pick, Omit, Exclude, Extract, Awaited |
| [08-modules-and-namespaces](./08-modules-and-namespaces/README.md) | Modules & Namespaces | ESM vs CJS, NodeNext vs Bundler resolution, `import type`, isolatedModules |
| [09-declaration-files](./09-declaration-files/README.md) | Declaration Files (`.d.ts`) | Ambient types, DefinitelyTyped, global augmentation, library typing |
| [10-advanced-types](./10-advanced-types/README.md) | Advanced Types | Conditional types, distributive unions, template literals, recursive types |
| [11-error-handling](./11-error-handling/README.md) | Error Handling | `useUnknownInCatchVariables`, Result type pattern, custom typed Error hierarchies |
| [12-async-and-promises](./12-async-and-promises/README.md) | Async & Promises | Typed Promises, async/await, unhandled rejections, AsyncGenerators |
| [13-compiler-and-configuration](./13-compiler-and-configuration/README.md) | Compiler & tsconfig | Strict flags, project references (`composite`), incremental builds |
| [14-compiler-internals](./14-compiler-internals/README.md) | Compiler Internals | Scanner, Parser (AST), Binder (Symbols), Checker (Types), Emitter |
| [15-runtime-type-checking](./15-runtime-type-checking/README.md) | Runtime Validation | Zod, Valibot, ArkType, Typebox, parse vs safeParse, brand types |
| [16-type-safe-apis](./16-type-safe-apis/README.md) | Type-Safe APIs | tRPC, OpenAPI contract sharing, Fetch client typing, GraphQL Codegen |
| [17-design-patterns](./17-design-patterns/README.md) | Design Patterns | Factory, Builder, Observer, Strategy, Decorators (TC39 Stage 3) |
| [18-testing](./18-testing/README.md) | Testing & QA | Vitest type assertions (`expectTypeOf`), Jest, negative tests (`@ts-expect-error`) |
| [19-react-and-frontend](./19-react-and-frontend/README.md) | React & Frontend | React 18/19 typings, polymorphic components, custom hooks, context API |
| [20-nodejs-backend](./20-nodejs-backend/README.md) | Node.js Backend | Express/Fastify augmentation, Drizzle/Prisma ORM, Zod env boot schemas |
| [21-production-tooling](./21-production-tooling/README.md) | Production Tooling | typescript-eslint v8 Flat Config, Biome, monorepo type strategies, Turborepo |
| [22-performance](./22-performance/README.md) | Performance | `--extendedDiagnostics`, `--generateTrace`, union explosion, compiler latency |
| [23-security](./23-security/README.md) | Security | Prototype pollution prevention, injection defenses, secret branding |
| [24-best-practices](./24-best-practices/README.md) | Best Practices | Clean code rules, naming conventions, codebase anti-patterns, code reviews |
| [25-real-world-patterns](./25-real-world-patterns/README.md) | Real-World Patterns | Hexagonal architecture, event-driven types, CQRS, domain modeling |
| [26-projects](./26-projects/README.md) | End-to-End Projects | 14 production reference projects + 5-tier type challenges |
| [27-interview](./27-interview/README.md) | Interview Mastery | 12 technical interview modules, live coding challenges, system design |
| [28-cheatsheets](./28-cheatsheets/README.md) | Reference Cheatsheets | 13 dense reference cards covering all syntax, flags, and APIs |
| [29-typed-data-structures-and-algorithms](./29-typed-data-structures-and-algorithms/README.md) | Typed DSA | Generic BSTs, Priority Queues, Type-Safe Graphs, LRU Cache, Type-Space Complexity |
