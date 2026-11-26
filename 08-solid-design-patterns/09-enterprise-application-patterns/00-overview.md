# Application Patterns: Overview

The GoF patterns in the previous folders describe how classes and objects collaborate. This folder covers patterns for the **structure of a typical business application**: how to organize use cases, access data, group changes into transactions and move data across boundaries.

They were popularized mainly by Martin Fowler's *Patterns of Enterprise Application Architecture* and Eric Evans' *Domain-Driven Design*. They are less about clever object structures and more about keeping layers clean.

## The five topics

| Pattern | Problem it solves | One-line idea |
|---|---|---|
| [Repository](./01-repository.md) | Business code is tangled with data-access code | A collection-like interface for loading and saving domain objects |
| [Unit of Work](./02-unit-of-work.md) | Several changes must be saved together, consistently | Track changes, then commit them as one |
| [Specification](./03-specification.md) | Business rules and query criteria get scattered and duplicated | Encapsulate a rule as a reusable, combinable object |
| [Service Layer](./04-service-layer.md) | Use-case logic leaks into controllers or the UI | A layer that defines the application's use cases and transaction boundaries |
| [Data Transfer Object](./05-data-transfer-object.md) | Domain objects shouldn't cross every boundary | Simple objects shaped for transport |

The old standalone "transaction script" note and the "DI container" note are folded in: transaction script is compared inside [Service Layer](./04-service-layer.md), and containers are covered in [dependency injection](../03-patterns-intro/02-dependency-injection.md).

## Prerequisites

- [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md): repositories are the textbook example.
- [Dependency injection](../03-patterns-intro/02-dependency-injection.md)
- [Single Responsibility](../02-solid-principles/01-single-responsibility.md)

## How they fit together

```text
Client / HTTP / UI
      │  DTOs in/out
      ▼
Controller (translates HTTP ↔ DTO)
      │
      ▼
Service Layer           ← use case, transaction boundary
      │   uses
      ├──► Domain objects (entities, value objects, Specifications)
      └──► Repositories (interfaces owned by the domain)
                 │
                 ▼  implemented with Unit of Work (JPA session, JDBC transaction)
              Database
```

A request typically flows: **controller → DTO → service → repositories + domain objects → unit of work commits → DTO back**.

## Where each one lives

| Pattern | Layer | Depends on |
|---|---|---|
| Controller | Presentation | Service layer, DTOs |
| Service layer | Application | Domain, repository interfaces |
| Domain, Specification | Domain | Nothing external |
| Repository interface | Domain | Domain objects |
| Repository implementation, Unit of Work | Infrastructure | Database/ORM |
| DTO | Boundary | Plain data |

This arrangement is the common thread of [layered](../08-architecture/01-layered.md) and [clean/hexagonal](../08-architecture/02-clean-hexagonal-onion.md) architecture.

## Common misconceptions

- **"These patterns need a framework."** Frameworks implement them, but the ideas are plain.
- **"Always use all of them."** Small CRUD apps often need only a thin service and a repository. See [avoiding overengineering](../10-production/05-avoiding-overengineering.md).
- **"Repository equals DAO."** They overlap but differ in level of abstraction (see the repository note).
- **"Service layer is where all logic goes."** That path leads to an anemic domain model.

## Quick Summary

- These patterns organize a business application into clear layers.
- Repository abstracts persistence, Unit of Work groups changes, Specification captures rules, Service Layer defines use cases, DTOs carry data across boundaries.
- Apply as much of the structure as the application's complexity justifies.

## Next

[01-repository.md](./01-repository.md)
