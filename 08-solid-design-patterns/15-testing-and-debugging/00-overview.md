# Testing and Debugging: Overview

Design and testing are two views of the same thing. Code that follows the principles in this repository (low coupling, high cohesion, dependencies on abstractions) is easy to test. Code that is hard to test almost always has a design problem. This folder uses tests as a **feedback tool for design**, and design knowledge as a **tool for finding and fixing problems**.

## What's in this folder

| File | Question it answers |
|---|---|
| [01-testable-design.md](./01-testable-design.md) | What makes code easy or hard to test, and how do I design for it? |
| [02-test-doubles.md](./02-test-doubles.md) | How do I isolate code from slow, random or external dependencies? |
| [03-refactoring-to-patterns.md](./03-refactoring-to-patterns.md) | How do I safely move smelly code toward a good design? |
| [04-debugging-design-issues.md](./04-debugging-design-issues.md) | How do I diagnose problems that come from structure, not from a single bad line? |

## Prerequisites

- [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md)
- [Dependency injection](../03-patterns-intro/02-dependency-injection.md)
- [Coupling and cohesion](../01-foundations/03-coupling-and-cohesion.md)

## Tests as a design signal

```text
Hard to write a test?
        │
        ▼
   Why is it hard?
   ├─ needs a database / network / clock        → hidden or concrete dependency   (DIP, DI)
   ├─ needs a dozen objects set up              → too much coupling               (coupling)
   ├─ one test touches many behaviors           → too many responsibilities       (SRP)
   ├─ must mock internals to assert anything    → leaky or poorly placed logic    (encapsulation)
   └─ results change from run to run            → hidden state, time, randomness  (global state)
```

Listen to that signal. Fixing the design usually fixes the test, and the reverse (a clever test hack) usually leaves the design problem in place.

## The testing pyramid

```text
         /\
        /UI\          few: slow, brittle, high confidence in the whole
       /----\
      / Integ \       some: real database, real wiring, adapters
     /--------\
    /   Unit    \     many: fast, isolated, test business logic
   /------------\
```

| Level | Tests | Typical tools in Java |
|---|---|---|
| **Unit** | A class or small group, with dependencies replaced by [test doubles](./02-test-doubles.md) | JUnit 5, AssertJ, Mockito |
| **Integration** | Adapters against real infrastructure (database, message broker) | Spring test support, Testcontainers |
| **End-to-end** | A complete flow through the running system | Browser or API test tools |

Architectures like [hexagonal](../08-architecture/02-clean-hexagonal-onion.md) make the pyramid easy: most logic is testable at the unit level, and integration tests only cover adapters.

## What good tests have in common

A common checklist is **FIRST**:

| Letter | Meaning |
|---|---|
| **F**ast | Run in milliseconds, so people run them constantly |
| **I**ndependent | No test depends on another or on execution order |
| **R**epeatable | Same result every time, in any environment |
| **S**elf-validating | Pass or fail without manual inspection |
| **T**imely | Written alongside (or before) the code |

## Common misconceptions

- **"High coverage means good tests."** Coverage shows what code ran, not whether behavior was checked.
- **"Mock everything for isolation."** Over-mocking couples tests to implementation. See [test doubles](./02-test-doubles.md).
- **"Testing is a separate phase."** Tests are a design activity; they guide structure as you write.
- **"You need a framework to be testable."** Plain constructors and interfaces are enough.
- **"Refactoring needs no tests."** Without tests, you're only hoping behavior is unchanged.

## Quick Summary

- Testability and good design are strongly correlated; use difficulty testing as a design alarm.
- Keep most tests small, fast and isolated; use fewer integration and end-to-end tests.
- This folder covers designing for tests, isolating with doubles, refactoring safely, and diagnosing structural problems.

## Next

[01-testable-design.md](./01-testable-design.md)
