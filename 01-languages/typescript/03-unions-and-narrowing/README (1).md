# 03 - Unions and Narrowing

How to model "one of several things", combine types, and let the compiler work out which one you have at each point in the code. This is the most important chapter for writing idiomatic TypeScript.

## Reading order

| # | File | What you get |
|---|---|---|
| 00 | [union-types.md](00-union-types.md) | `A \| B`, what you can do with a union value |
| 01 | [intersection-types.md](01-intersection-types.md) | `A & B`, merging shapes, conflicts |
| 02 | [literal-types-and-const-assertions.md](02-literal-types-and-const-assertions.md) | Exact values as types, `as const` |
| 03 | [type-narrowing.md](03-type-narrowing.md) | `typeof`, `instanceof`, `in`, truthiness, equality, control flow |
| 04 | [discriminated-unions.md](04-discriminated-unions.md) | Tagged unions: the main modelling tool |
| 05 | [type-guards-and-assertion-functions.md](05-type-guards-and-assertion-functions.md) | Custom narrowing with `x is T` and `asserts` |
| 06 | [exhaustiveness-checking.md](06-exhaustiveness-checking.md) | Making the compiler flag unhandled cases |
| 07 | [type-assertions-and-satisfies.md](07-type-assertions-and-satisfies.md) | `as`, `!`, `satisfies`, and when each is safe |

## Prerequisites
- [01-fundamentals](../01-fundamentals/README.md)
- [02-functions](../02-functions/README.md) (for type guard signatures)

## Milestone
You can model a state with a discriminated union, narrow it with a `switch`, get a compile error when a new variant is added, and explain when an assertion is acceptable.

## Next
[04-objects-and-interfaces](../04-objects-and-interfaces/README.md)
