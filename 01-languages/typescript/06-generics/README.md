# 06 Generics

> Production-grade generic programming in TypeScript: parameterization, constraints, type-level lookup, conditional inference, const parameters, variance, and compiler performance optimization.

## Module Overview

Generics enable flexible, reusable, and type-safe components, algorithms, and data structures. This module covers everything from foundational generic functions and classes to advanced compiler-level optimization techniques.

---

## Documents in this Module

| File | Level | Focus | Key Topics |
| :--- | :--- | :--- | :--- |
| [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md) | Intermediate | Generic Callables | Type parameters, inference, multiple generics, generic constraints |
| [01-generic-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/01-generic-types.md) | Intermediate | Interfaces & Classes | Generic interfaces, generic classes, static constraints, defaults |
| [02-generic-constraints-and-defaults.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/02-generic-constraints-and-defaults.md) | Intermediate | Bounds & Fallbacks | `extends` constraints, default arguments, multiple bounds, conditional defaults |
| [03-keyof-and-typeof.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/03-keyof-and-typeof.md) | Intermediate | Type Operators | `keyof`, type-space `typeof`, dynamic keys, dictionary types |
| [04-indexed-access-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/04-indexed-access-types.md) | Intermediate | Lookup Types | `T[K]`, tuple indices, array `T[number]`, chained lookup |
| [05-generic-inference.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/05-generic-inference.md) | Advanced | Conditional Types | `infer` keyword, pattern matching, unboxing promises, distribution |
| [06-const-type-parameters.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/06-const-type-parameters.md) | Advanced | TS 5.0+ Features | `<const T>`, literal inference, readonly tuples, schema builders |
| [07-type-parameter-variance-annotations.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/07-type-parameter-variance-annotations.md) | Advanced | Variance & Perf | `in`, `out`, covariance, contravariance, invariance, fast subtyping |
| [08-generic-instantiation-limits-and-compiler-perf.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/08-generic-instantiation-limits-and-compiler-perf.md) | Advanced | Performance & Internals | `TS2589`, tail-call recursion, `--extendedDiagnostics`, recursion guards |

---

## Prerequisites
Before tackling this module, ensure you are familiar with:
- [01-fundamentals/](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/) (primitive types, objects, arrays, tuples)
- [02-functions/](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/) (parameters, return types, call signatures)
- [05-classes/](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/) (class mechanics, access modifiers)

---

## Suggested Study Path
1. Master basic parameterization in **00-generic-functions** and **01-generic-types**.
2. Learn boundary controls with **02-generic-constraints-and-defaults**.
3. Harness type queries with **03-keyof-and-typeof** and **04-indexed-access-types**.
4. Level up to advanced type transformation with **05-generic-inference**.
5. Explore modern TypeScript features with **06-const-type-parameters** and **07-type-parameter-variance-annotations**.
6. Optimize production build times with **08-generic-instantiation-limits-and-compiler-perf**.
