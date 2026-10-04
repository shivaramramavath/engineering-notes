# 06 - Generics

How to write code that works with many types while keeping each use fully type-safe. Generics are the foundation of utility types, advanced types, typed APIs, and most library code.

## Reading order

| # | File | What you get |
|---|---|---|
| 00 | [generic-functions.md](00-generic-functions.md) | `<T>`, inference, explicit type arguments |
| 01 | [generic-types.md](01-generic-types.md) | Generic interfaces, type aliases and classes |
| 02 | [generic-constraints-and-defaults.md](02-generic-constraints-and-defaults.md) | `extends` constraints and default type parameters |
| 03 | [keyof-and-typeof.md](03-keyof-and-typeof.md) | Turning keys and values into types |
| 04 | [indexed-access-types.md](04-indexed-access-types.md) | `T[K]`, looking up property and element types |
| 05 | [generic-inference.md](05-generic-inference.md) | How TypeScript infers type arguments, and how to steer it |

## Prerequisites
- [02-functions](../02-functions/README.md)
- [03-unions-and-narrowing](../03-unions-and-narrowing/README.md)
- [04-objects-and-interfaces](../04-objects-and-interfaces/README.md)

## Milestone
You can write a generic helper like `pluck`, `groupBy` or `ApiResponse<T>`, constrain a type parameter, and explain why TypeScript inferred a particular type.

## Next
[07-utility-types](../07-utility-types/README.md)
