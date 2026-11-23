# Module 14: Type System Internals and Compiler Extensibility in TypeScript

Welcome to **Module 14: Type System Internals and Compiler Extensibility in TypeScript**. This module provides a complete, deep-level exploration of TypeScript's internal type-checking engine, subtyping rules, variance mechanics, compiler pipeline phases, and programmatic APIs.

---

## Module Overview

| Note | Title | Level | Core Topics |
| :--- | :--- | :--- | :--- |
| **00** | [Type Erasure and Runtime Representation](./00-type-erasure-and-runtime.md) | Advanced | Compile-time vs runtime, syntax downleveling, reflection limits, TC39 erasable syntax |
| **01** | [Assignability, Subtyping, and Freshness](./01-assignability-and-subtyping.md) | Advanced | Structural subtyping, assignability relation, identity, excess property checks, weak types |
| **02** | [Variance Mechanics](./02-variance.md) | Advanced | Covariance, contravariance, invariance, method bivariance, TS 4.7 `in`/`out` annotations |
| **03** | [Type Widening and Contextual Typing](./03-type-widening-and-inference.md) | Advanced | Literal widening, `let` vs `const`, property widening, `as const`, contextual typing |
| **04** | [Soundness and Escape Hatches](./04-soundness-and-escape-hatches.md) | Advanced | Intentional unsoundness (Non-Goal 3), array covariance, `any`, `unknown`, `as`, `!`, `@ts-expect-error` |
| **05** | [Compiler Architecture: The 5 Phases](./05-compiler-architecture.md) | Advanced | Scanner -> Parser -> Binder -> Type Checker -> Emitter, AST, Symbols, FlowGraphs |
| **06** | [TypeScript Compiler API and AST](./06-ast-and-compiler-api.md) | Advanced | `ts.createProgram`, `ts.SourceFile`, visitors, `ts.factory` AST nodes, custom transformers |
| **07** | [Programmatic AST with ts-morph](./07-ts-morph-and-programmatic-ast-manipulation.md) | Advanced | `ts-morph` Project, wrapped nodes, symbol navigation, automated codemods, code generation |
| **08** | [Custom Language Service Plugins](./08-writing-a-custom-language-service-plugin.md) | Advanced | TSServer plugin architecture, intercepting diagnostics, autocompletions, hover quick info, quick fixes |

---

## Learning Path

```
                    +------------------------------------+
                    | 00-type-erasure-and-runtime        |
                    | (Type erasure, TC39 erasable)      |
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 01-assignability-and-subtyping     |
                    | (Structural subtyping, freshness)  |
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 02-variance                        |
                    | (Covariance, contravariance, in/out|
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 03-type-widening-and-inference     |
                    | (Literal widening, contextual type)|
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 04-soundness-and-escape-hatches    |
                    | (Unsound trade-offs, any, unknown) |
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 05-compiler-architecture           |
                    | (Scanner, Parser, Binder, Checker) |
                    +-----------------+------------------+
                                      |
                 +--------------------+--------------------+
                 |                                         |
                 v                                         v
+------------------------------------+   +------------------------------------+
| 06-ast-and-compiler-api            |   | 07-ts-morph-and-programmatic-ast   |
| (Compiler API, ts.factory, visitors|   | (ts-morph Project, codemods)       |
+----------------+-------------------+   +-----------------+------------------+
                 |                                         |
                 +-------------------+---------------------+
                                     |
                                     v
                   +----------------------------------+
                   | 08-writing-a-custom-plugin       |
                   | (TSServer language service, IDE) |
                   +----------------------------------+
```

---

## Key Best Practices
1. **Embrace Purely Erasable Syntax**: Avoid `enum`, `namespace`, and constructor parameter properties to ensure compatibility with modern fast runtimes (Node 22, Bun, SWC).
2. **Beware the Freshness Bypass**: Pass object literals inline to API calls to ensure excess property checks catch typos.
3. **Use Property Syntax for Callbacks**: Declare callback signatures as properties (`cb: (x: T) => void`) rather than methods to enforce strict contravariant parameter checking.
4. **Prefer `@ts-expect-error` Over `@ts-ignore`**: Ensure every suppression is justified and fails when the underlying issue is resolved.
5. **Decouple AST Traversal from Type Checking**: In custom tools, avoid instantiating the heavy `TypeChecker` if pure syntactic analysis is sufficient.
