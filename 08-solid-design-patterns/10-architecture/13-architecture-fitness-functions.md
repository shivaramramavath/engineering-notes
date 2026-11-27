# Architecture Fitness Functions

| Field | Value |
| :--- | :--- |
| **Category** | Software Architecture (Automated Governance) |
| **Origin** | Ford, Parsons, Kua (*Building Evolutionary Architectures*, 2017) |
| **Difficulty** | Intermediate |
| **Prerequisite(s)** | [Clean & Hexagonal](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/02-clean-hexagonal-onion.md), [Modular Monolith](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/03-modular-monolith.md) |
| **Status** | Complete |

> **TL;DR**: An **Architecture Fitness Function** is an automated test or CI check (like `ArchUnit`, `dependency-cruiser`, `import-linter`, or latency budgets) that continuously verifies your code still obeys its architectural boundaries and non-functional goals.

---

## 1. The Problem: Why Architecture Diagrams Rot

Teams draw a clean Hexagonal diagram on Day 1. Six months later, under deadline pressure, someone imports `JpaRepository` inside `domain/` or creates a cyclic import between `orders/` and `billing/`. Humans miss these in PR reviews; **automated Fitness Functions catch them in CI before merge**.

---

## 2. Types of Fitness Functions

- **Structural / Dependency**: *"Domain package must never import Infrastructure or Spring."*
- **Modularity / Acyclicity**: *"No circular dependencies between top-level modules."*
- **Performance / Resilience**: *"p99 checkout latency < 200 ms; Chaos test survives 1 AZ failure."*
- **Security / Compliance**: *"Every public controller endpoint must have an `@Auth` / guard annotation."*

---

## 3. Concrete Examples Across Languages

### Java (`ArchUnit` Concept)
```java
// Enforce Clean Architecture Dependency Rule in JUnit
noClasses()
    .that().resideInAPackage("..domain..")
    .should().dependOnClassesThat().resideInAnyPackage("..infrastructure..", "org.springframework..");
```

### TypeScript (`dependency-cruiser` / ESLint Boundaries)
```json
{
  "forbidden": [
    { "name": "no-domain-to-infra", "from": { "path": "^src/domain" }, "to": { "path": "^src/infra" } },
    { "name": "no-circular", "from": {}, "to": { "circular": true } }
  ]
}
```

### Python (`import-linter` `.importlinter` Contract)
```ini
[importlinter:contract:clean-layers]
name = Domain must not import Infra
type = forbidden
source_modules = app.domain
forbidden_modules = app.infra, fastapi, sqlalchemy
```

---

## 4. Interview Questions

<details>
<summary>1. What is an Architecture Fitness Function?</summary>

An objective, automated check (unit test, linter, or CI/monitoring gate) that measures how close an architecture remains to its desired structural, performance, or security characteristics.

</details>

<details>
<summary>2. Name one Fitness Function tool for Java, TypeScript, and Python.</summary>

Java: **ArchUnit** (or Spring Modulith). TypeScript: **dependency-cruiser** / ESLint `boundaries`. Python: **import-linter**.

</details>

<details>
<summary>3. Why are Fitness Functions better than wiki architecture guidelines?</summary>

Wiki rules rely on memory and manual review; Fitness Functions run on every commit in CI and fail the build the instant a boundary is violated.

</details>

---

## 5. Cheat Sheet

- **Automate architecture rules** in CI (`ArchUnit`, `dependency-cruiser`, `import-linter`).
- **Protect**: Dependency Rule, acyclicity, module encapsulation, p99 latency budgets.

---

## 6. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Architecture Decision Records (ADRs)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/12-architecture-decision-records.md) | [Module 10](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/README.md) | [Evolutionary Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/14-evolutionary-architecture.md) |
