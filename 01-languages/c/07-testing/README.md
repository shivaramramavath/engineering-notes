# Module 07: Testing in C

> Master enterprise-grade testing methodologies for systems-level C: unit testing architectures with Unity and CMocka, test doubles and mocking patterns (linker wrapping `-Wl,--wrap` and vtable injection), coverage-guided fuzz testing with LibFuzzer and AFL++, Test-Driven Development (TDD) workflows in C, property-based testing and invariant verification, integrating sanitizers and Valgrind into automated test pipelines, and enforcing code coverage metrics (line, branch, and MC/DC).

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `07-testing` |
| **Prerequisites** | [00-setup/07-makefile-basics.md](../00-setup/07-makefile-basics.md), [01-fundamentals/08-functions.md](../01-fundamentals/08-functions.md), [03-intermediate/01-function-pointers.md](../03-intermediate/01-function-pointers.md) |
| **Duration** | ~20 hours |
| **Status** | Complete (Batch 20 Part 3) |
| **Standard Baseline**| C99 / C11 / C17 / POSIX.1-2008 / LLVM 12+ / GCC 9+ |

---

## Overview & Learning Outcomes

Writing reliable, production-grade C code requires rigorous verification methodologies. Because C offers direct manual memory access and lacks runtime exceptions or type-safe reflection, unverified logic easily turns into security vulnerabilities, memory leaks, and undefined behavior crashes.

In this module, you will master:
1. **Unit Testing Architectures**: Building lightweight, portable test suites with Unity (embedded friendly) and CMocka (enterprise POSIX), using structured test runners and assertion families.
2. **Test Doubles & Mocking**: Isolating hardware registers, sockets, and disk I/O using function pointer tables (vtable injection) and GNU linker symbol wrapping (`-Wl,--wrap=<symbol>`).
3. **Coverage-Guided Fuzzing**: Employing LLVM `libFuzzer` and `AFL++` to discover zero-day memory corruptions, buffer overflows, and parser crashes via automated corpus generation.
4. **Test-Driven Development (TDD)**: Applying the Red-Green-Refactor cycle in pure C, designing modular interfaces before implementation, and isolating core algorithms from platform dependencies.
5. **Property-Based Testing**: Validating algorithmic invariants, algebraic laws (associativity, symmetry, idempotency), and roundtrip encode/decode safety using randomized property runners.
6. **Sanitizer & Valgrind Pipelines**: Running test suites under AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), and Valgrind Memcheck to ensure 0 memory leaks and 0 undefined behaviors.
7. **Code Coverage Enforcement**: Measuring statement, branch, and Modified Condition / Decision Coverage (MC/DC), configuring coverage thresholds in CI, and identifying dead safety code.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [Unit Testing with Unity and CMocka](./01-unit-testing-with-unity-or-cmocka.md) | Intermediate | Complete | Test fixtures, setUp/tearDown, assertion families, test runner orchestration |
| **02** | [Test Doubles & Mocking in C](./02-test-doubles-and-mocking-in-c.md) | Intermediate | Complete | Stubs, fakes, spies, mocks, linker wrapping (`-Wl,--wrap`), vtable injection |
| **03** | [Fuzzing with LibFuzzer & AFL++](./03-fuzzing-with-libfuzzer-afl.md) | Advanced | Complete | LLVMFuzzerTestOneInput, coverage-guided mutation, dictionaries, corpus triage |
| **04** | [Test-Driven Development (TDD) in C](./04-test-driven-c.md) | Intermediate | Complete | Red-Green-Refactor cycle, interface-first design, isolating pure logic from hardware |
| **05** | [Property-Based Testing Ideas in C](./05-property-testing-ideas-in-c.md) | Advanced | Complete | Invariant verification, QuickCheck concepts, pseudo-random generators, shrinking |
| **06** | [Testing with Sanitizers & Valgrind](./06-testing-with-sanitizers-and-valgrind.md) | Intermediate | Complete | ASan/UBSan test runners, Valgrind suppressions, memory leak detection gates |
| **07** | [Code Coverage Targets & Metrics](./07-code-coverage-targets.md) | Advanced | Complete | Line vs branch vs MC/DC coverage, CI threshold gates, eliminating unreachable code |

---

## Suggested Study Order
1. **Foundations of Verification**: Start with [Unit Testing with Unity and CMocka](./01-unit-testing-with-unity-or-cmocka.md) and practice the [Test-Driven Development](./04-test-driven-c.md) workflow.
2. **System Isolation**: Master [Test Doubles & Mocking](./02-test-doubles-and-mocking-in-c.md) to decouple business logic from hardware and external APIs.
3. **Automated Defect Discovery**: Deploy [Coverage-Guided Fuzzing](./03-fuzzing-with-libfuzzer-afl.md) and [Property-Based Testing](./05-property-testing-ideas-in-c.md) to discover edge-case panics.
4. **Production Quality Gates**: Enforce memory cleanliness with [Sanitizers & Valgrind](./06-testing-with-sanitizers-and-valgrind.md) and set release benchmarks with [Code Coverage Targets](./07-code-coverage-targets.md).

---

[← Previous Module: Build & Tooling](../06-build-and-tooling/README.md) | [C Language Track Index](../README.md) | [Next Module: Security & Safety](../08-security-and-safety/README.md)
