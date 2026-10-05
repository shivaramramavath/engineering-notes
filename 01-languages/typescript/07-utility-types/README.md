# 07 - Utility Types

Utility types are the generic types that ship with TypeScript (`lib.es5.d.ts`) for transforming other types. Almost all of them are one or two lines of mapped or conditional types. Learn what each one does, then learn how it is built, and you can write your own when the built-ins run out.

## Prerequisites

- [06 Generics](../06-generics/README.md): especially [generic types](../06-generics/01-generic-types.md) and [keyof and typeof](../06-generics/03-keyof-and-typeof.md)
- [03 Unions and Narrowing](../03-unions-and-narrowing/README.md): `Exclude` and `Extract` operate on unions
- [04 Objects and Interfaces](../04-objects-and-interfaces/README.md): optional and readonly properties

You do not need [10 Advanced Types](../10-advanced-types/README.md) first, but the last note here leans on mapped and conditional types, so read those next.

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Partial, Required, Readonly](./00-partial-required-readonly.md) | Changing `?` and `readonly` modifiers across a whole type |
| 01 | [Pick, Omit, Record](./01-pick-omit-record.md) | Selecting or dropping keys, and building key-to-value maps |
| 02 | [Exclude, Extract, NonNullable](./02-exclude-extract-nonnullable.md) | Filtering unions and stripping `null` / `undefined` |
| 03 | [Function and class utilities](./03-function-and-class-utilities.md) | `Parameters`, `ReturnType`, `Awaited`, `InstanceType`, `NoInfer`, and friends |
| 04 | [Building custom utility types](./04-building-custom-utility-types.md) | `Mutable`, `PartialBy`, `PickByType`, deep variants, and how to test them |

Read them in order. Each note builds on the previous one.

## Which utility do I need?

| I want to... | Use |
|---|---|
| make all properties optional (patches, options) | `Partial<T>` |
| make all properties required | `Required<T>` |
| make all properties read-only | `Readonly<T>` |
| keep a few properties | `Pick<T, K>` |
| drop a few properties | `Omit<T, K>` |
| build a typed lookup table from a key union | `Record<K, V>` |
| remove members from a union | `Exclude<U, X>` |
| keep only some members of a union | `Extract<U, X>` |
| remove `null` and `undefined` | `NonNullable<T>` |
| get a function's parameters or result | `Parameters<typeof f>`, `ReturnType<typeof f>` |
| get the resolved value of a promise | `Awaited<T>` |
| get a class's constructor args or instance type | `ConstructorParameters<typeof C>`, `InstanceType<typeof C>` |
| make only some keys optional | custom `PartialBy` ([04](./04-building-custom-utility-types.md)) |
| apply a modifier recursively | custom `DeepPartial` / `DeepReadonly` ([04](./04-building-custom-utility-types.md)) |

## Ideas that recur across the section

- **They are not magic.** `Partial`, `Required`, `Readonly`, `Pick`, and `Record` are mapped types. `Exclude`, `Extract`, `Parameters`, and `ReturnType` are conditional types. Reading the definition explains the edge cases.
- **Modifier and key utilities are shallow.** They act on the top level of an object type only.
- **Union behavior differs.** `Exclude`/`Extract` distribute over unions. `Omit` and `Pick` do not, which is why `Omit` can flatten a discriminated union.
- **`Omit` does not validate its keys.** `Pick` does. Prefer `Pick` for allow-lists.
- **All of this is compile-time only.** Nothing here validates data at runtime. See [15 Runtime Validation](../15-runtime-validation/README.md).

## Related sections

- [10 Advanced Types](../10-advanced-types/README.md): the mechanisms behind every utility
- [16 Type-Safe APIs](../16-type-safe-apis/README.md): DTOs built with `Pick`, `Omit`, and `Partial`
- [27 Interview: utility types](../27-interview/03-utility-types.md)
- [28 Cheatsheet: utility types](../28-cheatsheets/01-utility-types.md)
- [26 Projects: type challenges](../26-projects/exercises/00-type-challenges.md)

## Next

[08 Modules](../08-modules/README.md)
