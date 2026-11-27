# General Design Principles Cheat Sheet

> **One-line summary:** A rapid one-page operational reference summarizing all 19 General Design Principles, their core rules, diagnostic red flags, and key trade-offs.

| Attribute | Specification |
|---|---|
| **Level** | Quick Reference / All Levels |
| **Prerequisites** | [Module 03 Overview](00-overview.md) |
| **Estimated Reading Time** | 15 minutes |
| **Status** | Complete |
| **Category** | General Design Principles |

---

## 1. Master Reference Matrix

| # | Principle | One-Line Rule | Primary Red Flag | Primary Caution |
|---|---|---|---|---|
| 01 | **DRY & Rule of Three** | One authoritative source for each piece of domain knowledge; abstract on 3rd occurrence | Copy-pasted tax/pricing rules drifting out of sync | Premature DRY coupling unrelated actors with boolean flags |
| 02 | **KISS** | Eliminate accidental complexity; prefer clear, direct constructs | Reflection factories for simple static lookups | Confusing "simple" with skipping error handling |
| 03 | **YAGNI** | Build only what is needed today; defer speculative abstractions | Unused plugin hooks, dead config flags | Never skip tests, security, or clean code |
| 04 | **Law of Demeter** | Only talk to immediate friends; never chain into strangers | `a.getB().getC().mutate()` | Does not apply to passive DTOs or Streams/Builders |
| 05 | **Separation of Concerns** | Partition Transport, Domain Policy, Persistence, and Cross-Cutting concerns | Raw SQL inside HTTP route handlers | "Lasagna Architecture" of empty passthrough layers |
| 06 | **Tell, Don't Ask** | Co-locate data and behavior; tell objects what to do | External `if (x.getA() > 0) x.setB(...)` | Read-only display getters on DTOs are fine |
| 07 | **Least Astonishment (POLA)** | Match caller intuition in names, units, mutability, defaults | `get*()` mutating DB; `median()` sorting caller's list | Always default to safe, non-destructive modes |
| 08 | **Fail-Fast** | Detect and reject invalid state immediately at the boundary | Empty `catch`, silent defaults, bad data reaching DB | Use DLQ isolation in batch ETL streams |
| 09 | **Boy Scout Rule** | Leave every touched file slightly cleaner than you found it | Huge unrelated formatting diffs in feature PRs | Skip during urgent 3:00 AM outage hotfixes |
| 10 | **High Cohesion / Low Coupling** | Group things that change together; minimize cross-module links | `CommonUtils` dumping ground; circular dependencies | Avoid splitting one cohesive task into 15 micro-files |
| 11 | **Orthogonality** | Independent axes (Format $\times$ Storage) must vary independently | $M \times N$ subclass explosion (`CsvS3`, `JsonEmail`) | Don't split steps that always change together |
| 12 | **Postel's Law** | Conservative writer; liberal reader on additive extensions | Crashing on extra JSON fields during rolling deploy | Reject malformed syntax/auth strictly (RFC 9413) |
| 13 | **Hollywood Principle / IoC** | Frameworks/runners own lifecycle and invoke domain hooks | Duplicated transaction/lock boilerplate in every job | Keep domain entities free of framework annotations |
| 14 | **GRASP (9 Patterns)** | Systematic responsibility assignment (Expert, Creator, Pure Fabrication...) | Domain entities executing raw SQL instead of Repos | Don't invent Pure Fabrications for simple domain math |
| 15 | **Package Principles** | Cohesion (REP, CCP, CRP) + Coupling (ADP, SDP, SAP) | Cyclic package builds; bloated `common` deps | Don't over-split small single-team apps into 20 packages |
| 16 | **Premature Optimisation** | Measure with a profiler before optimizing the 3% hot path | Unreadable bit-hacks while ignoring N+1 DB queries | Still pick $O(1)$ Maps over $O(N^2)$ loops upfront |
| 17 | **Idempotency** | $f(f(x)) = f(x)$: retries leave identical state | Double charges on mobile network retries | Client must generate Idempotency-Key; use `UNIQUE` DB guard |
| 18 | **Conway's Law** | Architecture mirrors team communication structures | Siloed UI/API/DBA teams producing 3-tier bottlenecks | Align cross-functional Stream Teams to bounded contexts |

---

## 2. Quick Decision Tree for Code Reviews

```text
1. Does it do something NOT requested yet?               --> YAGNI (Delete it)
2. Is it more complex than the problem requires?         --> KISS (Simplify it)
3. Is business knowledge duplicated 3+ times?            --> DRY (Extract SSOT)
4. Is a caller chaining getters (`a.getB().getC()`)?     --> Law of Demeter / Tell, Don't Ask
5. Does a getter/query mutate state or input arguments?  --> POLA / CQS (Fix side effect)
6. Can invalid state enter silently without failing?     --> Fail-Fast (Guard at boundary)
7. Can a network retry double-execute this mutation?     --> Idempotency (Add unique key)
8. Are two unrelated teams blocking each other on 1 pkg? --> Conway's Law / CCP / ADP
```

---

[← Previous: 19 - Principles Conflicts & Trade-offs](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/19-principles-conflicts-and-trade-offs.md) | [Module 03 Index](file:///e:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/README.md) | [Next: Module 04 - Patterns Introduction →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/04-patterns-intro/README.md)
