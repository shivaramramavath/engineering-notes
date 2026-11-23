# 10 Advanced Types

> The definitive engineering reference for TypeScript's advanced type-level programming capabilities: conditional types, distributive evaluation, pattern matching with `infer`, mapped transformations, template literal types, recursive structures, variadic tuples, branded nominality, compile-time parsers, deep path navigation, and type arithmetic.

## Module Overview

TypeScript possesses a Turing-complete, purely functional type system. This module explores advanced techniques that allow engineers to implement complex validation rules, domain modeling, and compile-time verification across mission-critical systems.

---

## Documents in this Module

| File | Level | Focus | Key Topics |
| :--- | :--- | :--- | :--- |
| [00-conditional-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/00-conditional-types.md) | Advanced | Ternary Logic | Ternary syntax, deferred conditional types, assignability checks |
| [01-distributive-conditional-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/01-distributive-conditional-types.md) | Advanced | Union Distribution | Bare type parameter rule, union filtering, tuple wrapping, `never` trap |
| [02-infer.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/02-infer.md) | Advanced | Pattern Matching | `infer` keyword, covariance vs contravariance, constrained `infer` |
| [03-mapped-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/03-mapped-types.md) | Advanced | Property Iteration | Homomorphic mapping, `+` and `-` modifiers, key remapping with `as` |
| [04-template-literal-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/04-template-literal-types.md) | Advanced | String Synthesis | String interpolation in types, Cartesian products, string pattern matching |
| [05-recursive-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/05-recursive-types.md) | Advanced | Self-Referential Types | Recursive type aliases, JSON models, tail-call recursion optimization |
| [06-variadic-tuple-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/06-variadic-tuple-types.md) | Advanced | Dynamic Tuples | Generic tuple spread `...T`, prefixing, suffixing, labeled tuples |
| [07-branded-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/07-branded-types.md) | Advanced | Nominal Typing | Phantom tags, unique symbols, smart constructors, type-safe currencies |
| [08-type-level-programming.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/08-type-level-programming.md) | Advanced | Turing Completeness | Type-level boolean logic, accumulator pattern, compiler execution model |
| [09-type-level-parsers-and-state-machines.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/09-type-level-parsers-and-state-machines.md) | Advanced | Compile-Time Parsers | Query string parsers, SQL column extractors, state transition matrices |
| [10-deep-keys-and-path-navigation.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/10-deep-keys-and-path-navigation.md) | Advanced | Nested Path Navigation | Exhaustive dotted paths, `DeepPaths`, `DeepPathValue`, lenses |
| [11-type-arithmetic-and-tuple-manipulation.md](file:///E:/Learning/engineering-notes/01-languages/typescript/10-advanced-types/11-type-arithmetic-and-tuple-manipulation.md) | Advanced | Number Simulation | Peano arithmetic, tuple length math, `Add`, `Subtract`, `GreaterThan` |

---

## Prerequisites
Before tackling this module, ensure you are familiar with:
- [01-fundamentals/](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/) (primitive types, objects, arrays)
- [06-generics/](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/) (type parameters, constraints, keyof)
- [07-utility-types/](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/) (standard library utilities)

---

## Suggested Study Path
1. Master conditional logic in **00-conditional-types** and **01-distributive-conditional-types**.
2. Learn pattern matching with **02-infer**.
3. Master property iteration in **03-mapped-types**.
4. Synthesize string types with **04-template-literal-types**.
5. Build self-referential structures in **05-recursive-types**.
6. Compose dynamic tuples with **06-variadic-tuple-types**.
7. Enforce domain boundary safety with **07-branded-types**.
8. Explore type computation principles in **08-type-level-programming**.
9. Build DSL tokenizers in **09-type-level-parsers-and-state-machines**.
10. Navigate nested object trees with **10-deep-keys-and-path-navigation**.
11. Compute tuple counts with **11-type-arithmetic-and-tuple-manipulation**.
