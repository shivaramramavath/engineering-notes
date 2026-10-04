# 04 - Objects and Interfaces

How to describe the shape of objects in depth: interfaces, property modifiers, dynamic keys, and the structural rules that decide whether one type fits another.

## Reading order

| # | File | What you get |
|---|---|---|
| 00 | [interfaces.md](00-interfaces.md) | Declaring, extending and using interfaces |
| 01 | [interface-vs-type.md](01-interface-vs-type.md) | The decision guide (single canonical copy) |
| 02 | [readonly-and-optional-properties.md](02-readonly-and-optional-properties.md) | `readonly`, `?`, and their limits |
| 03 | [index-signatures.md](03-index-signatures.md) | Dynamic keys, `Record`, safe lookups |
| 04 | [structural-typing.md](04-structural-typing.md) | Compatibility rules, excess property checks |

## Prerequisites
- [01-fundamentals](../01-fundamentals/README.md), especially [objects](../01-fundamentals/03-objects.md) and [type-aliases](../01-fundamentals/04-type-aliases.md)
- [03-unions-and-narrowing](../03-unions-and-narrowing/README.md)

## Milestone
You can model an API entity with an interface, extend it, choose between `interface` and `type` with a reason, and explain why an object with extra properties is sometimes accepted and sometimes rejected.

## Next
[05-classes](../05-classes/README.md)
