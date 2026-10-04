# 01 - Fundamentals

The building blocks of the type system: how types are assigned, the basic types, and the special types you must understand before anything else.

## Reading order

| # | File | What you get |
|---|---|---|
| 00 | [types-and-inference.md](00-types-and-inference.md) | Annotations vs. inference; when to write types yourself |
| 01 | [primitive-types.md](01-primitive-types.md) | `string`, `number`, `boolean`, `bigint`, `symbol` |
| 02 | [arrays-and-tuples.md](02-arrays-and-tuples.md) | Typing lists and fixed-length sequences |
| 03 | [objects.md](03-objects.md) | Object type literals, nested and optional shapes |
| 04 | [type-aliases.md](04-type-aliases.md) | Naming and reusing types with `type` |
| 05 | [enums-and-const-objects.md](05-enums-and-const-objects.md) | Enums and the `as const` alternative |
| 06 | [any-and-unknown.md](06-any-and-unknown.md) | The escape hatch vs. the safe top type |
| 07 | [never-and-void.md](07-never-and-void.md) | Functions that return nothing or never return |
| 08 | [null-and-undefined.md](08-null-and-undefined.md) | `strictNullChecks`, optional chaining, `??` |

## Prerequisites
- [00-setup](../00-setup/README.md): a project where `tsc --noEmit` works with `strict: true`
- Basic JavaScript

## Milestone
You can read and write type annotations for variables and plain data, explain why `unknown` is safer than `any`, and handle `null`/`undefined` without assertions.

## Next
[02-functions](../02-functions/README.md)
