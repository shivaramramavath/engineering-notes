# Module 03: General Design Principles

Welcome to **Module 03: General Design Principles** of the Software Design & Patterns Curriculum. While SOLID governs class and interface boundaries in object-oriented architecture, everyday engineering depends on a broader set of foundational principles—governing simplicity, duplication, coupling, failure boundaries, package structure, distributed reliability, and socio-technical alignment.

---

## Module Roadmap & Contents

| # | Note | Core Focus & Purpose | Status | Est. Time |
|---|---|---|:---:|:---:|
| 00 | [General Design Principles Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/00-overview.md) | Taxonomy of design principles, mental models, and how principles balance opposing forces | Complete | 15 min |
| 01 | [DRY & The Rule of Three](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/01-dry-and-rule-of-three.md) | Knowledge duplication vs incidental code similarity, and avoiding the wrong abstraction | Complete | 30 min |
| 02 | [KISS Principle](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/02-kiss-principle.md) | Essential vs accidental complexity, minimizing cognitive load and over-engineering | Complete | 25 min |
| 03 | [YAGNI](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/03-yagni.md) | Cost of speculative features, deferring irreversible decisions, and lean delivery | Complete | 25 min |
| 04 | [Law of Demeter](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/04-law-of-demeter.md) | Principle of Least Knowledge, eliminating train-wreck chains (`a.b().c().d()`) | Complete | 30 min |
| 05 | [Separation of Concerns (SoC)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/05-separation-of-concerns.md) | Horizontal vs vertical slicing, isolating orthogonal responsibilities across layers | Complete | 30 min |
| 06 | [Tell, Don't Ask](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/06-tell-dont-ask.md) | Co-locating data and behavior, eliminating anemic domain models and external state querying | Complete | 30 min |
| 07 | [Principle of Least Astonishment](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/07-principle-of-least-astonishment.md) | Designing predictable APIs, eliminating hidden side effects and surprising defaults | Complete | 25 min |
| 08 | [Fail-Fast Principle](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/08-fail-fast-principle.md) | Surfacing invalid states at system boundaries before corruption spreads downstream | Complete | 30 min |
| 09 | [Boy Scout Rule & Continuous Refactoring](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/09-boy-scout-rule-and-continuous-refactoring.md) | Incremental code stewardship, preventing architectural rot without big-bang rewrites | Complete | 25 min |
| 10 | [High Cohesion & Low Coupling in Practice](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/10-high-cohesion-low-coupling-in-practice.md) | Practical refactoring of tangled dependencies into cohesive, loosely coupled modules | Complete | 30 min |
| 11 | [Orthogonality](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/11-orthogonality.md) | Eliminating cross-cutting ripple effects so independent axes evolve independently | Complete | 30 min |
| 12 | [Postel's Law (Robustness Principle)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/12-postels-law-robustness-principle.md) | Tolerant readers, strict writers, schema evolution, and where strictness beats tolerance | Complete | 30 min |
| 13 | [Hollywood Principle & Inversion of Control](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/13-hollywood-principle-and-inversion-of-control.md) | "Don't call us, we'll call you", framework callbacks, hooks, and event loops | Complete | 30 min |
| 14 | [GRASP Principles](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/14-grasp-principles.md) | Craig Larman's 9 responsibility-assignment patterns (Information Expert, Creator, Controller, etc.) | Complete | 40 min |
| 15 | [Package & Component Principles](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/15-package-and-component-principles.md) | Cohesion (REP, CCP, CRP) and Coupling (ADP, SDP, SAP) for module/package boundaries | Complete | 35 min |
| 16 | [Premature Optimisation & Measuring](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/16-premature-optimisation-and-measuring.md) | Knuth's law, mechanical sympathy, profiling-driven optimization vs micro-obfuscation | Complete | 30 min |
| 17 | [Idempotency Principle](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/17-idempotency-principle.md) | Safe retries, idempotency keys, outbox deduplication, and deterministic state transitions | Complete | 35 min |
| 18 | [Conway's Law & Socio-Technical Design](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/18-conways-law-and-socio-technical-design.md) | Team topologies, Reverse Conway Maneuver, and aligning org structure with architecture | Complete | 30 min |
| 19 | [Principles Conflicts & Trade-offs](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/19-principles-conflicts-and-trade-offs.md) | Resolving DRY vs Coupling, Fail-Fast vs Postel's Law, KISS vs Extensibility | Complete | 30 min |
| 20 | [General Design Principles Cheat Sheet](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/20-principles-cheat-sheet.md) | Rapid operational lookup matrix for all 19 principles and decision rules | Complete | 15 min |

---

## Core Learning Outcomes

By completing this module, you will:
1. Distinguish **knowledge duplication** (true DRY violation) from **incidental syntactic similarity** (where DRY creates toxic coupling).
2. Eliminate train-wreck navigation chains (`order.getCustomer().getAddress().getZipCode()`) using **Law of Demeter** and **Tell, Don't Ask**.
3. Apply **Fail-Fast** validation at trust boundaries and understand when **Postel's Law** helps vs harms API security and schema integrity.
4. Assign class and package responsibilities systematically using **GRASP** and **Package Cohesion/Coupling Principles** (REP, CCP, CRP, ADP, SDP, SAP).
5. Design retry-safe distributed operations with **Idempotency** and structure teams and modules around **Conway's Law**.
6. Navigate trade-offs cleanly when two valid design principles pull in opposite directions.

---

[← Back to Module 02: SOLID Principles](file:///e:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/README.md) | [Next: 00 - General Principles Overview →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/00-overview.md)
