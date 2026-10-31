# Architecture: Overview

Design patterns shape how a few classes collaborate. **Architecture** shapes how a whole application is divided into parts, which parts may depend on which, and how those parts communicate. The same principles apply ([coupling and cohesion](../01-foundations/03-coupling-and-cohesion.md), [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md)), just at a larger scale.

## What this folder covers

| File | Style | Core question |
|---|---|---|
| [01-layered.md](./01-layered.md) | Layered | How do I separate presentation, business logic and data access? |
| [02-clean-hexagonal-onion.md](./02-clean-hexagonal-onion.md) | Clean / Hexagonal / Onion | How do I keep business logic independent of frameworks and databases? |
| [03-modular-monolith.md](./03-modular-monolith.md) | Modular monolith | How do I get clear boundaries without distributed-system complexity? |
| [04-domain-driven-design.md](./04-domain-driven-design.md) | Domain-Driven Design | How do I model a complex business domain well? |
| [05-event-driven.md](./05-event-driven.md) | Event-driven | How do parts react to each other without direct calls? |

These are not mutually exclusive choices. They operate on different axes and are commonly combined.

```text
Dependency direction (how code is organized):   Layered → Clean / Hexagonal / Onion
Boundaries (how the system is divided):          Modular monolith → (microservices)
Modeling (how the domain is expressed):          Domain-Driven Design
Communication (how parts interact):              Direct calls ↔ Event-driven
```

A typical well-structured system: **a modular monolith**, whose modules use a **hexagonal structure** internally, are modeled with **DDD** concepts, and talk to each other through **events** where decoupling matters.

## Prerequisites

- [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md)
- [Application patterns](../07-application-patterns/00-overview.md): repository, service layer, DTO
- [Observer](../06-behavioral-patterns/02-observer.md) (for event-driven)

## Architecture drivers

Choose by the qualities that matter, not by fashion.

| Quality | Question |
|---|---|
| Changeability | How often do requirements change, and how localized are changes? |
| Testability | Can business rules be tested without a database or web server? |
| Team size and ownership | Can several teams work independently? |
| Deployability | Must parts be released separately? |
| Scalability | Do parts have very different load profiles? |
| Complexity of the domain | Are the rules rich, or is it mostly CRUD? |
| Operational capacity | Can the team run many services, brokers, tracing? |

## Choosing a starting point

```text
Small team, simple domain, CRUD-heavy
   └─ layered, kept simple

Business rules are complex or long-lived
   └─ hexagonal/clean structure + DDD tactical patterns

Growing codebase, several teams, one deployable is still fine
   └─ modular monolith

Parts need independent scaling or release by separate teams
   └─ extract modules into services (and accept distributed-system costs)

Workflows spanning parts, or integration with many consumers
   └─ add events where they pay for themselves
```

The pragmatic default for most products: **start as a well-structured modular monolith** and extract services only for a clear reason.

## Common misconceptions

- **"Microservices are the modern default."** They solve organizational and scaling problems and add network failure, data consistency and operational costs. Many systems are better as a modular monolith.
- **"Clean architecture means more folders."** It's about the *direction of dependencies*, not folder count.
- **"DDD means using its vocabulary everywhere."** Its value is modeling and shared language, and it pays off only in complex domains.
- **"Events make everything decoupled."** They decouple call-time dependency but introduce hidden flow, ordering and consistency issues.
- **"Architecture is decided once."** It evolves; design so that boundaries can be moved.

## Quick Summary

- Architecture applies the same principles as class design, at the scale of modules and services.
- The five styles here address different concerns and are often combined.
- Choose from real drivers: complexity, team structure, deployment and operations.
- A modular monolith with clear boundaries is a strong default.

## Next

[01-layered.md](./01-layered.md)
