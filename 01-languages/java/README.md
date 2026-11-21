# Java Engineering Curriculum & Technical Reference

> A production-grade, exhaustive curriculum and technical reference manual for modern Java engineering (Java 8 to 25 LTS). Covers core language mechanics, HotSpot JVM internals, high-concurrency systems, design patterns, production troubleshooting, and distributed backend system design.

---

## Curriculum Architecture & Module Overview

The Java curriculum is partitioned into **30 specialized modular tracks** spanning fundamentals, modern language evolution, JVM memory and runtime diagnostics, enterprise software engineering, and technical interview mastery.

```
+----------------------------------------------------------------------------------------------------+
|                                    JAVA ENGINEERING TRACKS (00 - 29)                               |
+------------------------------------+-----------------------------------+---------------------------+
| FOUNDATIONS & LANGUAGE CORE        | ADVANCED & RUNTIME SYSTEMS        | ENTERPRISE & ARCHITECTURE |
|  00-setup                          |  12-modern-java (8-25 LTS)        |  20-performance           |
|  01-fundamentals                   |  13-advanced-language-features    |  21-security              |
|  02-methods                        |  14-concurrency (JMM & Threads)   |  22-production-engineering|
|  03-strings-and-text               |  15-jvm-internals (HotSpot & GC)  |  23-design-and-clean-code |
|  04-oop                            |  16-jdbc-and-databases            |  24-design-patterns (GoF) |
|  05-packages-and-modules           |  17-json-and-data-formats         |  25-real-world-patterns   |
|  06-exceptions-and-debugging       |  18-build-and-dependencies        |  26-projects (15 apps)    |
|  07-generics                       |  19-testing (JUnit 5 & AssertJ)   |  27-dsa (28 DSA notes)    |
|  08-collections                                                        |  28-interview (Mastery)   |
|  09-functional-java                                                    |  29-cheatsheets (Quick Ref)|
|  10-date-and-time                                                                                  |
|  11-io-and-networking                                                                              |
+----------------------------------------------------------------------------------------------------+
```

---

## Verified Toolchain & Platform Baseline

Every code example, benchmark, test case, and architectural demonstration throughout this curriculum has been compiled and verified against:
- **Baseline LTS:** **Oracle JDK 25 LTS** (`java version "25.0.1" 2025-10-21 LTS`)
- **Compilation Parameters:** `javac --release 25 -Xlint:all -Werror`
- **Testing Engine:** JUnit 5 (5.11.0) / AssertJ (3.26.3)
- **Benchmarking Suite:** Java Microbenchmark Harness (JMH 1.37)
- **Container Target:** Linux x86_64 / cgroups v2 container runtime

---

## Standard Note Architecture

Every concept note strictly follows the unified 13-section architectural standard defined in `_templates/concept-template.md`:
1. **What & Why:** High-level architectural intent and industry motivation.
2. **Core Concepts:** Fundamental axioms, structural components, and mathematical/formal models.
3. **How It Works Internally:** HotSpot JVM memory layouts, assembly bytecode, object headers, and ASCII diagrams.
4. **Syntax & Usage:** Complete language syntax, method signatures, and configuration options.
5. **Practical Examples:** 5+ production-grade, verified code snippets (basic to edge cases).
6. **Best Practices:** Production engineering rules, clean code guidelines, and design idioms.
7. **Common Mistakes & Pitfalls:** Structured table detailing common failures, underlying reasons, and exact fixes.
8. **Performance & Security Notes:** CPU cache alignment, memory allocation overhead, and OWASP security mitigations.
9. **Comparison / When to Use What:** Trade-off comparison matrices against competing approaches.
10. **Interview Questions:** 8+ high-difficulty interview questions with collapsible answers.
11. **Practice Exercises:** 3+ coding problems with verified solutions in collapsible blocks.
12. **Summary / Cheat Sheet:** Rapid-review flash summary card.
13. **Further Reading:** Authoritative links to OpenJDK JEPs, JLS specifications, and academic textbooks.

---

## Recommended Learning Pathways

### Pathway 1: Senior Systems & Backend Engineer
```
00-setup -> 04-oop -> 08-collections -> 09-functional-java -> 11-io-and-networking ->
12-modern-java -> 14-concurrency -> 15-jvm-internals -> 16-jdbc-and-databases ->
20-performance -> 22-production-engineering -> 26-projects
```

### Pathway 2: Staff / Principal Technical Interview Mastery
```
07-generics -> 08-collections -> 14-concurrency -> 15-jvm-internals ->
24-design-patterns -> 27-dsa -> 28-interview -> 29-cheatsheets
```

---

## Repository Standards & Navigation

- [Comprehensive Track Index (INDEX.md)](./INDEX.md) - Full catalog of all 270+ notes and projects.
- [Technical Glossary (GLOSSARY.md)](./GLOSSARY.md) - Comprehensive definitions for 120+ JVM, concurrency, and language terms.
- [Curriculum Roadmap (ROADMAP.md)](./ROADMAP.md) - Progressive mastery roadmap from fundamentals to distributed architecture.
- [Authoring Standards (CONTRIBUTING.md)](./CONTRIBUTING.md) - Verification rules, note formatting guidelines, and templates.
