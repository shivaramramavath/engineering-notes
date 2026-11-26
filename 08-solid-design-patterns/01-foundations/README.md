# Module 01: Object-Oriented Foundations

Welcome to **Module 01: Foundations** of the Software Design & Patterns Curriculum. This module establishes the structural bedrock upon which SOLID principles, GoF design patterns, and enterprise architectural paradigms are constructed.

---

## Module Roadmap & Contents

| # | Note | Core Focus & Purpose | Status | Est. Time |
|---|---|---|:---:|:---:|
| 00 | [Foundations Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/00-overview.md) | The cost of change, mental models, and foundational relationships | Complete | 15 min |
| 01 | [OOP Pillars](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/01-oop-pillars.md) | Encapsulation, Abstraction, Inheritance, and Dynamic Polymorphism | Complete | 30 min |
| 02 | [Composition vs Inheritance](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/02-composition-vs-inheritance.md) | Fragile base class problem, explosion of subclasses, composition solutions | Complete | 35 min |
| 03 | [Coupling and Cohesion](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/03-coupling-and-cohesion.md) | Afferent/Efferent coupling, structural metrics, single-focus cohesion | Complete | 30 min |
| 04 | [Programming to Interfaces](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/04-programming-to-interfaces.md) | Decoupling clients from volatile concretions, test seams, replaceability | Complete | 30 min |
| 05 | [Abstraction & Information Hiding](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/05-abstraction-and-information-hiding.md) | David Parnas information hiding, protecting design secrets, leaky abstractions | Complete | 30 min |
| 06 | [Encapsulate What Varies](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/06-encapsulate-what-varies.md) | Identifying volatility vectors, separating policy from mechanism | Complete | 25 min |
| 07 | [Immutability & Value Objects](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/07-immutability-and-value-objects.md) | Eliminating aliasing bugs, thread safety, structural equality | Complete | 30 min |
| 08 | [Command-Query Separation (CQS)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/08-command-query-separation.md) | Bertrand Meyer rule, side-effect free queries, command idempotency | Complete | 25 min |
| 09 | [Design by Contract & Defensive Programming](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/09-design-by-contract-and-defensive-programming.md) | Preconditions, postconditions, class invariants, fail-fast vs defensive | Complete | 30 min |
| 10 | [What is Good Design?](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/10-what-is-good-design.md) | Cost of change curve, technical debt taxonomy, identifying code smells | Complete | 30 min |

---

## Core Learning Outcomes

By completing this module, you will:
1. Diagnose why naive class hierarchies experience the **Fragile Base Class problem** and how composition resolves it.
2. Evaluate designs objectively using formal **Afferent ($C_a$) and Efferent ($C_e$) coupling** metrics and cohesion indices.
3. Replace dangerous inheritance trees with polymorphic interfaces, enabling zero-risk testability with mocks and stubs.
4. Distinguish between data encapsulation (hiding state) and true information hiding (encapsulating volatile business decisions).
5. Prevent concurrent data races and aliasing side-effects using immutable Value Objects.

---

[← Back to Track Root](file:///e:/Learning/engineering-notes/08-solid-design-patterns/README.md) | [Next: 00 - Foundations Overview →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/01-foundations/00-overview.md)
