# SOLID, Design Principles & Design Patterns Decisions (SDP_DECISIONS.md)

| ID | Date | Decision | Context & Rationale | Status |
|---|---|---|---|---|
| **ADR-001** | 2026-11-26 | **Problem-First Pedagogy Structure** | Every principle and pattern must open with a concrete, recognizable software engineering failure/pain scenario before introducing the definition or theory. | Accepted |
| **ADR-002** | 2026-11-26 | **Three-Language Execution Baseline** | All runnable code examples must be provided in Java 25 LTS, TypeScript 5.8 (Node 24 native strip), and Python 3.14. Idiomatic language features (lambdas, structural typing `Protocol`, closures) must be highlighted. | Accepted |
| **ADR-003** | 2026-11-26 | **Scratch Folder Isolation for Test Runner** | The verification runner lives outside the repository in `<appDataDir>/brain/<conversation-id>/scratch/sdp_runner/` and compiles/runs code externally. | Accepted |
| **ADR-004** | 2026-11-26 | **Strict Markdown-Only Invariant** | No raw `.java`, `.ts`, or `.py` files inside `08-solid-design-patterns/`. All code is contained in fenced markdown blocks with intended file paths in comments/headings. | Accepted |
| **ADR-005** | 2026-11-26 | **Full Module Renumbering & Insertions** | To accommodate Module 03 (General Design Principles: DRY, KISS, YAGNI, Law of Demeter, etc.), subsequent modules are renumbered cleanly from 00 to 20. | Accepted |
| **ADR-006** | 2026-11-26 | **Resolution of Stray Parenthesis Files** | All stray `00-overview (N).md` files found in the legacy tree are merged into their respective module `00-overview.md` without data loss, and the duplicate copies deleted and logged. | Accepted |
| **ADR-007** | 2026-11-26 | **Verified Real-World Claims Policy** | Real-world library citations (e.g. JDK `java.io.FilterInputStream`, Python `functools.lru_cache`, `EventTarget`) must be strictly verifiable against official standard library sources. No invented internals. | Accepted |
| **ADR-008** | 2026-11-26 | **Same Scenario Comparative Invariant** | For sections 4 ("The Problem Without It") and 5 ("How It Solves It"), the exact same business domain scenario and exact same Change Request must be evaluated so the comparative blast radius is fair and objective. | Accepted |

---
*Signed off on 2026-11-26.*
