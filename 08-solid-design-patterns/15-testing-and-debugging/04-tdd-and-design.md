# TDD & Emergent Design

| Field | Value |
| :--- | :--- |
| **Category** | Testing & Design |
| **Origin** | Kent Beck (*Test-Driven Development by Example*, 2002) |

> **TL;DR**: **Red → Green → Refactor** uses tests as an immediate design feedback tool: if writing the first test feels awkward, the API you are designing is awkward too.

## Classicist (Detroit) vs Mockist (London) TDD
- **Classicist**: Tests real collaborating domain objects together; only uses Fakes/Stubs at I/O boundaries. Produces resilient tests that survive refactoring.
- **Mockist**: Mocks every collaborator of the class under test, driving outside-in interface discovery—but risks brittle tests coupled to internal call sequences.
