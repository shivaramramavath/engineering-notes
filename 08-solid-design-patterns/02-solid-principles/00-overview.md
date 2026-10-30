# SOLID Principles: Overview

SOLID is a set of five design principles for object-oriented code, popularized by Robert C. Martin. They don't describe a framework or a syntax. They describe *what makes code easy to change* and give you vocabulary to explain why a design hurts.

| Letter | Principle | One-line version |
|---|---|---|
| S | [Single Responsibility](./01-single-responsibility.md) | A class should have one reason to change |
| O | [Open/Closed](./02-open-closed.md) | Add behavior by adding code, not by editing working code |
| L | [Liskov Substitution](./03-liskov-substitution.md) | A subtype must be usable wherever its parent is expected |
| I | [Interface Segregation](./04-interface-segregation.md) | Don't force clients to depend on methods they don't use |
| D | [Dependency Inversion](./05-dependency-inversion.md) | Depend on abstractions, not on concrete details |

## Prerequisites

This section builds on [coupling and cohesion](../01-foundations/03-coupling-and-cohesion.md) and [programming to interfaces](../01-foundations/04-programming-to-interfaces.md). If those terms are unfamiliar, read them first.

## How they fit together

The principles overlap and reinforce each other.

```text
SRP ──► keeps classes focused (high cohesion)
OCP ──► lets you extend without editing (via polymorphism)
LSP ──► makes that polymorphism safe to rely on
ISP ──► keeps the contracts small and relevant
DIP ──► points dependencies at those contracts
```

OCP is usually *achieved* through LSP-respecting subtypes, small ISP-style interfaces and DIP-style wiring. They are one design philosophy, not five separate rules.

## Typical symptoms and the principle they point to

| Symptom | Look at |
|---|---|
| Class changes for unrelated reasons | SRP |
| Adding a feature means editing a long `if/else` or `switch` | OCP |
| Subclass throws "not supported" or has empty overrides | LSP |
| Implementations stub out methods they don't need | ISP |
| Can't test a class without a database or network | DIP |

## Common misconceptions

- **"SOLID is a set of laws."** They are guidelines with trade-offs. Applying them mechanically produces over-engineered code. See [SOLID in practice](./06-solid-in-practice.md).
- **"SOLID only applies to classes."** The same ideas apply to functions, modules, packages and services.
- **"More interfaces and smaller classes is always better."** The goal is lower cost of change, not a higher class count.
- **"Follow all five on every class."** Apply them where change is actually happening or likely.

## How to use this folder

- **Learning:** read 01 to 05 in order, then 06 for how to apply them without overdoing it.
- **Reference:** use the symptom table above, or jump straight to a principle.
- **Interview prep:** each file ends with a short summary; questions are in [../12-interview/01-questions.md](../12-interview/01-questions.md).

## Quick Summary

- SOLID is about managing change, not about rules for their own sake.
- The five principles work together; OCP is the goal, the others are the means.
- Use symptoms (rigid code, fragile tests, `switch` growth) to decide which principle to apply.

## Next

[01-single-responsibility.md](./01-single-responsibility.md)
