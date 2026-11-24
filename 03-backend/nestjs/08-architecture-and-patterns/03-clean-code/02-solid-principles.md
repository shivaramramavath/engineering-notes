---
title: "SOLID Principles in NestJS"
description: "Applying Single Responsibility, Open/Closed, Liskov Substitution, Interface Segregation, and Dependency Inversion in NestJS"
tags: [nestjs, clean-code, solid, oop, design-principles]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# SOLID Principles in NestJS

## Conceptual Overview
The SOLID principles form the foundation of object-oriented architecture:
- **S - Single Responsibility Principle (SRP)**: A class should have one, and only one, reason to change.
- **O - Open/Closed Principle (OCP)**: Software entities should be open for extension, but closed for modification.
- **L - Liskov Substitution Principle (LSP)**: Subtypes must be substitutable for their base types without altering correctness.
- **I - Interface Segregation Principle (ISP)**: Clients should not be forced to depend on interfaces they do not use.
- **D - Dependency Inversion Principle (DIP)**: Depend upon abstractions, not concretions.

---

## SOLID in NestJS

- **SRP**: Separate HTTP controller parsing from business calculations (Service) and database queries (Repository).
- **OCP**: Use Strategy or Adapter patterns to support new payment processors without editing existing controller code.
- **DIP**: High-level Application Use Cases depend on `IOrderRepository` interfaces, while low-level TypeORM repositories implement them via `@Inject('ORDER_REPOSITORY')`.

---

## Key Takeaways & Cheat Sheet

- High-level business logic must never depend directly on database libraries or low-level HTTP drivers.
- Keep interfaces small and purpose-specific (ISP).
- Break large multipurpose classes into focused single-responsibility services.

---

## Cross-References
- [01-clean-code-and-code-organization.md](./01-clean-code-and-code-organization.md) - Clean code
- [03-cohesion-coupling-and-separation-of-concerns.md](./03-cohesion-coupling-and-separation-of-concerns.md) - Cohesion and coupling
