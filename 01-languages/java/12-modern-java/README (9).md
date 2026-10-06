# 12 · Modern Java

Since Java 9 the language has changed more than in the previous fifteen years: shorter data classes, closed type hierarchies, expression-oriented `switch`, and pattern matching. Together these let you model data as plain **types** and write logic over them with the compiler checking you didn't forget a case.

This module covers the language features you will see in current codebases (Java 17/21/25) and how they fit together.

## Contents

| # | Note | What you get |
|---|------|--------------|
| 00 | [Java Release Timeline](00_java-release-timeline.md) | Release cadence, LTS versions, preview vs final, what arrived when, which version to target |
| 01 | [Records](01_records.md) | Immutable data carriers, compact constructors, what records can and can't do |
| 02 | [Sealed Classes](02_sealed-classes.md) | Closed hierarchies with `sealed` / `permits` / `non-sealed` |
| 03 | [Switch Expressions](03_switch-expressions.md) | `->` arms, `yield`, exhaustiveness |
| 04 | [Pattern Matching](04_pattern-matching.md) | `instanceof` patterns, `switch` patterns, record patterns, `_` |
| 05 | [`var` and Type Inference](05_var-and-type-inference.md) | Local variable inference, rules, style guidance |

## How the pieces fit

```text
   record Circle(double r)          ← 01  data as a type
   sealed interface Shape           ← 02  a closed set of types
   switch (shape) { ... }           ← 03  expression form: yields a value, must be exhaustive
   case Circle(double r) -> ...     ← 04  patterns: test the type AND extract the data
   var area = area(shape);          ← 05  less ceremony around locals
```

Read **00 first** (it tells you which version each feature needs), then **01 → 02 → 03 → 04**. Note 05 is independent.

## Prerequisites

[Classes and Objects](../04-oop/00_classes-and-objects.md), [Interfaces](../04-oop/09_interfaces.md), [Enums](../04-oop/11_enums.md), [Polymorphism](../04-oop/07_polymorphism.md), and the [switch basics](../01-fundamentals/05_conditionals.md).

**Next module:** [Advanced Language Features](../13-advanced-language-features/README.md)
