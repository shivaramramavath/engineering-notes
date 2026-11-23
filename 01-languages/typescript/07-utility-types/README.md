# 07 Utility Types

> Complete architectural reference and practical guide for TypeScript's built-in and advanced utility types: property modifiers, projections, union filters, call/construct signature extractors, string intrinsics, and recursive deep transformations.

## Module Overview

TypeScript utility types facilitate compile-time data transformations, allowing developers to derive specialized models, DTOs, and event contracts from base domain types without duplication or drift.

---

## Documents in this Module

| File | Level | Focus | Key Topics |
| :--- | :--- | :--- | :--- |
| [00-partial-required-readonly.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/00-partial-required-readonly.md) | Intermediate | Property Modifiers | `Partial`, `Required`, `Readonly`, `Record`, `exactOptionalPropertyTypes` |
| [01-pick-omit-record.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/01-pick-omit-record.md) | Intermediate | Projections | `Pick`, `Omit`, DTO design, strict omit, discriminated union projection |
| [02-exclude-extract-nonnullable.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/02-exclude-extract-nonnullable.md) | Intermediate | Union Filtering | `Exclude`, `Extract`, `NonNullable`, distributive conditional types |
| [03-function-and-class-utilities.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/03-function-and-class-utilities.md) | Advanced | Signature Introspection | `Parameters`, `ReturnType`, `ConstructorParameters`, `InstanceType`, `ThisParameterType` |
| [04-building-custom-utility-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/04-building-custom-utility-types.md) | Advanced | Custom Type Engineering | Key remapping via `as`, filtering keys by value type, homomorphic mapping, `Prettify` |
| [05-awaited-and-promise-unwrapping.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/05-awaited-and-promise-unwrapping.md) | Advanced | Async Unwrapping | `Awaited<T>`, recursive thenable unpacking, Promises/A+, `Promise.all` typing |
| [06-string-manipulation-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/06-string-manipulation-types.md) | Advanced | Intrinsic String Utilities | `Uppercase`, `Lowercase`, `Capitalize`, `Uncapitalize`, template literal pattern matching |
| [07-deep-utility-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/07-utility-types/07-deep-utility-types.md) | Advanced | Recursive Data Transformations | `DeepPartial`, `DeepReadonly`, `DeepRequired`, `DeepMutable`, function and Date guards |

---

## Prerequisites
Before tackling this module, ensure you are familiar with:
- [01-fundamentals/](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/) (primitive types, objects, arrays, unions)
- [02-functions/](file:///E:/Learning/engineering-notes/01-languages/typescript/02-functions/) (call signatures, return types, this parameters)
- [06-generics/](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/) (`keyof`, indexed access, conditional types, `infer`)

---

## Suggested Study Path
1. Master core modifiers and projections in **00-partial-required-readonly** and **01-pick-omit-record**.
2. Learn set operations on unions in **02-exclude-extract-nonnullable**.
3. Inspect callables and constructors in **03-function-and-class-utilities**.
4. Learn how to engineer your own utilities in **04-building-custom-utility-types**.
5. Handle asynchronous types with **05-awaited-and-promise-unwrapping**.
6. Transform string keys and parse routes with **06-string-manipulation-types**.
7. Enforce recursive contracts across entire object graphs with **07-deep-utility-types**.
