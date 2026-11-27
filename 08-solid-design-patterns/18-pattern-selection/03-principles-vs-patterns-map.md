# Principles vs Patterns Map

| Principle | Primary Patterns That Implement It |
| :--- | :--- |
| **SRP (Single Responsibility)** | Facade, Repository, Service Layer, Strategy, Extract Class |
| **OCP (Open/Closed)** | Strategy, Decorator, Plugin & Registry, Middleware, Chain of Responsibility |
| **LSP (Liskov Substitution)** | Proper Polymorphism, Null Object, Contract-safe Subtypes |
| **ISP (Interface Segregation)** | Role Interfaces (Ports), Narrow `ReadRepo` vs `WriteRepo` (CQRS) |
| **DIP (Dependency Inversion)** | Dependency Injection, Repository, Gateway / Ports & Adapters |
| **Composition over Inheritance** | Strategy, Decorator, Bridge, ECS |
| **Tell, Don't Ask** | Rich Domain Model, State, Command |
| **Fail Fast / Resilience** | Circuit Breaker, Timeout, Bulkhead, Startup Config Validation |
