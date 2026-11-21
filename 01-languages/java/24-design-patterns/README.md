# 24 · Design Patterns

Software design patterns are reusable, battle-tested solutions to recurring software engineering problems. This module covers the classic Gang of Four (GoF) design patterns modernized with modern Java 17/21/25 features (Records, Sealed Interfaces, Pattern Matching, Lambdas), along with architectural design patterns, a comprehensive pattern selection guide, and enterprise anti-patterns.

## Contents

### Overview & Architecture
- [Pattern Selection Guide](00_pattern-selection-guide.md)
- [Enterprise Anti-Patterns](05_anti-patterns.md)

### 01 · Creational Patterns
- [Singleton](01-creational/00_singleton.md) · Double-Checked Locking, Enum, Reflection/Serialization Defense
- [Factory Method](01-creational/01_factory-method.md) · Static Factories, Supplier Registries, Decoupling
- [Abstract Factory](01-creational/02_abstract-factory.md) · Product Families, Cloud Providers, UI Toolkits
- [Builder](01-creational/03_builder.md) · Telescoping Constructor Defense, Fluent Builders, Step Builders
- [Prototype](01-creational/04_prototype.md) · Copy Constructors, Serialization Cloning, Object Registries

### 02 · Structural Patterns
- [Adapter](02-structural/00_adapter.md) · Object vs Class Adapters, Anti-Corruption Layers, Functional Adapters
- [Decorator](02-structural/01_decorator.md) · Dynamic Responsibilities, Java I/O Streams, Functional Chaining
- [Facade](02-structural/02_facade.md) · Subsystem Simplification, Domain Facades, API Gateways
- [Proxy](02-structural/03_proxy.md) · Dynamic Proxies, Virtual Lazy Loading, Security/RBAC, AOP
- [Composite](02-structural/04_composite.md) · Tree Hierarchies, Uniform Leaf/Composite Treatment, File Systems

### 03 · Behavioral Patterns
- [Strategy](03-behavioral/00_strategy.md) · Interchangeable Algorithms, Lambdas, Comparators
- [Observer](03-behavioral/01_observer.md) · Event Systems, Java 9 Flow API, Thread-Safe Publishers
- [Command](03-behavioral/02_command.md) · Reversible Undo/Redo Stacks, Task Queuing, CQRS
- [Template Method](03-behavioral/03_template-method.md) · Algorithm Skeletons, Hook Methods, Loan Pattern
- [Iterator](03-behavioral/04_iterator.md) · Sequential Traversal, Fail-Fast vs Fail-Safe, Spliterators
- [State](03-behavioral/05_state.md) · State Machines, Order Workflows, Sealed Pattern Matching
- [Chain of Responsibility](03-behavioral/06_chain-of-responsibility.md) · Sequential Pipelines, Servlet Filters, Middleware

### 04 · Architectural Patterns
- [Dependency Injection](04-architecture/00_dependency-injection.md) · Constructor Injection, Pure Java DI, Inversion of Control
- [Layered Architecture](04-architecture/01_layered-architecture.md) · 4-Tier Model, Boundary Enforcement, DTO Records
- [Repository and DAO](04-architecture/02_repository-and-dao.md) · Table-Centric vs Aggregate-Centric, In-Memory Testing

---
**Next module:** [Real-World Patterns](../25-real-world-patterns/README.md)
