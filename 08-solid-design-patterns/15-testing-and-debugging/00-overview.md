# Module 15 Overview: Testing & Debugging Design

> *"If a unit test requires 40 lines of mock setup to test a 5-line function, the test is telling you your design is tightly coupled."*

1. **Make Code Testable by Construction** (`01`, `03`): Pure domain core + injected ports + deterministic clock/UUIDs.
2. **Choose the Right Test Double** (`02`, `05`): Prefer **Fakes** and **Stubs** over brittle interaction-heavy **Mocks**.
3. **Protect Architecture Continuously** (`04`, `06`, `07`): TDD feedback loops + Architecture tests.
