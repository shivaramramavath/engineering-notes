# 10 - Advanced Types

The features that let types **compute**: conditionals, pattern matching with `infer`, mapped types, string manipulation, recursion, and tuple spreads. Most built-in utility types and most library-grade type definitions are built from these pieces. The final note puts them together and discusses when this power is worth its cost.

## Prerequisites

- [03 Unions and Narrowing](../03-unions-and-narrowing/README.md): unions and intersections are the data these features operate on
- [06 Generics](../06-generics/README.md): every technique here is a generic type alias
- [07 Utility Types](../07-utility-types/README.md): the built-ins you are about to learn to build yourself

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Conditional types](./00-conditional-types.md) | `T extends U ? X : Y`, deferral, narrowing, why conditional return types need care |
| 01 | [Distributive conditional types](./01-distributive-conditional-types.md) | How conditionals map over unions, turning it off, `never`, `IsUnion` |
| 02 | [infer](./02-infer.md) | Capturing parts of types, tuple head/tail, string parsing, variance of multiple `infer`s |
| 03 | [Mapped types](./03-mapped-types.md) | `[K in keyof T]`, modifiers, `as` key remapping, arrays and unions |
| 04 | [Template literal types](./04-template-literal-types.md) | Building and parsing string types, route params, casing |
| 05 | [Recursive types](./05-recursive-types.md) | JSON and trees, recursive conditionals, depth limits, tail recursion |
| 06 | [Variadic tuple types](./06-variadic-tuple-types.md) | `[...T]`, concat, head/tail, typed argument forwarding |
| 07 | [Branded types](./07-branded-types.md) | Nominal-style IDs and validated values, smart constructors, flavoring |
| 08 | [Type-level programming](./08-type-level-programming.md) | Putting it together: idioms, testing, costs, when to stop |

Read 00 to 03 in order. 04 to 06 build on them. 07 stands mostly alone and is the most immediately practical. 08 is a synthesis, so read it last.

## Which feature do I need?

| I want to... | Use |
|---|---|
| choose a type based on another type | [conditional type](./00-conditional-types.md) |
| apply a transform to each member of a union | a distributive conditional ([01](./01-distributive-conditional-types.md)) |
| treat a union as one unit in a conditional | `[T] extends [U]` ([01](./01-distributive-conditional-types.md)) |
| extract a return type, element type, or tuple member | [`infer`](./02-infer.md) |
| transform every property of an object type | [mapped type](./03-mapped-types.md) |
| rename or filter object keys | mapped type with `as` ([03](./03-mapped-types.md)) |
| build or parse string literal types | [template literal types](./04-template-literal-types.md) |
| describe nested data of any depth | [recursive type](./05-recursive-types.md) |
| forward or reshape a function's parameter list | [variadic tuples](./06-variadic-tuple-types.md) |
| stop two same-shaped values from being mixed up | [branded type](./07-branded-types.md) |

## Ideas that recur across the section

- **`extends` means assignable, not equal.** Exact equality needs a dedicated `Equal` helper.
- **Distribution is on by default** for a bare type parameter in a conditional, and it is behind most "why is the result a union?" questions.
- **`infer` is pattern matching.** Its result depends on position: unions in covariant positions, intersections in contravariant ones.
- **Mapped types preserve structure** when written as `[K in keyof T]`: modifiers, arrays, tuples, and unions.
- **Recursion needs a base case and a depth budget.** Accumulators let the compiler optimize tail recursion.
- **It is all compile-time.** None of these types validate data at runtime, and heavy ones cost compile time.
- **Simple beats clever.** Reach for these when many callers benefit, and keep the public type small.

## Related sections

- [07 Utility Types: building custom utility types](../07-utility-types/04-building-custom-utility-types.md): applied examples of these features
- [14 Type System Internals: variance](../14-type-system-internals/02-variance.md) and [assignability](../14-type-system-internals/01-assignability-and-subtyping.md)
- [15 Runtime Validation](../15-runtime-validation/README.md): checking the data that types only describe
- [18 Testing and Debugging: type testing](../18-testing-and-debugging/04-type-testing.md)
- [22 Performance: type-checking performance](../22-performance/00-type-checking-performance.md)
- [26 Projects: type challenges](../26-projects/exercises/00-type-challenges.md)
- [27 Interview: advanced types](../27-interview/04-advanced-types.md)

## Next

[11 Error Handling](../11-error-handling/README.md)