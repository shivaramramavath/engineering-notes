# 02 - Functions

How to describe what functions accept and return, how callbacks and overloads are typed, and how `this` is handled.

## Reading order

| # | File | What you get |
|---|---|---|
| 00 | [function-types.md](00-function-types.md) | Function type expressions, call signatures, assignability |
| 01 | [parameters-and-return-types.md](01-parameters-and-return-types.md) | Required, optional, default, rest and destructured parameters; return inference |
| 02 | [callbacks.md](02-callbacks.md) | Typing callbacks, contextual typing, `void` returns |
| 03 | [function-overloads.md](03-function-overloads.md) | Overload signatures and when to avoid them |
| 04 | [this-parameters.md](04-this-parameters.md) | Typing `this`, arrow vs. method behaviour |

## Prerequisites
- [01-fundamentals](../01-fundamentals/README.md), especially [types-and-inference](../01-fundamentals/00-types-and-inference.md), [objects](../01-fundamentals/03-objects.md) and [never-and-void](../01-fundamentals/07-never-and-void.md)

## Milestone
You can type any function signature, pass typed callbacks, explain why overloads are sometimes the wrong tool, and fix "implicit `this`" errors.

## Next
[03-unions-and-narrowing](../03-unions-and-narrowing/README.md)
