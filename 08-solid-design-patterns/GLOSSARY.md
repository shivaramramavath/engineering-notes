# SOLID, Design Principles & Design Patterns Glossary (GLOSSARY.md)

Authoritative vocabulary and formal definitions for software design principles, GoF design patterns, architectural paradigms, and refactoring concepts.

---

## A
- **Abstract Factory Pattern**: A creational design pattern that provides an interface for creating families of related or dependent objects without specifying their concrete classes (GoF 1994).
- **Active Record**: An enterprise architectural pattern where an object wraps a row in a database table or view, encapsulates the database access, and adds domain logic on that data (PoEAA 2002).
- **Adapter Pattern**: A structural design pattern that converts the interface of a class into another interface clients expect, allowing classes to work together that couldn't otherwise because of incompatible interfaces (GoF 1994).
- **Anti-Pattern**: A common software development response to a recurring problem that is superficially attractive but systematically counterproductive and results in technical debt.

## B
- **Bridge Pattern**: A structural design pattern that decouples an abstraction from its implementation so that the two can vary independently (GoF 1994).
- **Builder Pattern**: A creational design pattern that separates the construction of a complex object from its representation, allowing the same construction process to create different representations (GoF 1994).
- **Bulkhead Pattern**: A cloud resilience pattern that isolates resources (such as thread pools or memory) into non-interfering pools so that failure of one component does not crash the entire application.

## C
- **Chain of Responsibility Pattern**: A behavioral design pattern that gives more than one object an opportunity to handle a request by chaining the receiving objects together and passing the request along the chain until an object handles it (GoF 1994).
- **Circuit Breaker Pattern**: A distributed resilience pattern that prevents an application from repeatedly trying to execute an operation that is likely to fail, returning an immediate error or fallback when failure thresholds are breached (Michael Nygard, 2007).
- **Clean Architecture**: An architectural paradigm formulated by Robert C. Martin where domain entities reside at the core and all dependencies point inward across strict interface boundaries.
- **Cohesion**: The degree to which the elements inside a single module or class belong together and serve a single well-defined purpose.
- **Command Pattern**: A behavioral design pattern that encapsulates a request as an object, thereby letting you parameterize clients with different requests, queue or log requests, and support undoable operations (GoF 1994).
- **Composite Pattern**: A structural design pattern that composes objects into tree structures to represent part-whole hierarchies, letting clients treat individual objects and compositions uniformly (GoF 1994).
- **Coupling**: The degree of direct interdependence between software modules; low coupling is a core design goal.

## D
- **Data Transfer Object (DTO)**: An enterprise pattern representing an object that carries data between processes in order to reduce the number of method calls over network boundaries (PoEAA 2002).
- **Decorator Pattern**: A structural design pattern that attaches additional responsibilities to an object dynamically, providing a flexible alternative to subclassing for extending functionality (GoF 1994).
- **Dependency Inversion Principle (DIP)**: The principle that high-level modules should not depend on low-level modules; both should depend on abstractions. Abstractions should not depend on details; details should depend on abstractions (Robert C. Martin 1996).
- **Don't Repeat Yourself (DRY)**: A software development principle stating that every piece of knowledge must have a single, unambiguous, authoritative representation within a system (Hunt & Thomas 1999).

## F
- **Facade Pattern**: A structural design pattern that provides a unified, higher-level interface to a set of interfaces in a subsystem, making the subsystem easier to use (GoF 1994).
- **Factory Method Pattern**: A creational design pattern that defines an interface for creating an object, but lets subclasses decide which class to instantiate (GoF 1994).
- **Flyweight Pattern**: A structural design pattern that uses sharing to support large numbers of fine-grained objects efficiently by separating intrinsic state from extrinsic state (GoF 1994).

## I
- **Interface Segregation Principle (ISP)**: The principle that clients should not be forced to depend upon interfaces that they do not use (Robert C. Martin 1996).
- **Iterator Pattern**: A behavioral design pattern that provides a way to access the elements of an aggregate object sequentially without exposing its underlying representation (GoF 1994).

## L
- **Law of Demeter (LoD)**: A design guideline ("Principle of Least Knowledge") stating that a given object should only communicate with its immediate friends (itself, parameters passed to it, objects it instantiates, and direct components) and never chain method calls through foreign objects.
- **Liskov Substitution Principle (LSP)**: The principle that functions that use pointers or references to base classes must be able to use objects of derived classes without knowing it, preserving behavioral subtyping invariants (Barbara Liskov & Jeannette Wing 1994).

## M
- **Mediator Pattern**: A behavioral design pattern that defines an object that encapsulates how a set of objects interact, preventing tight coupling by keeping objects from referring to each other explicitly (GoF 1994).
- **Memento Pattern**: A behavioral design pattern that captures and externalizes an object's internal state without violating encapsulation, allowing the object to be restored to this state later (GoF 1994).

## O
- **Observer Pattern**: A behavioral design pattern that defines a one-to-many dependency between objects so that when one object changes state, all its dependents are notified and updated automatically (GoF 1994).
- **Open/Closed Principle (OCP)**: The principle that software entities (classes, modules, functions) should be open for extension, but closed for modification (Bertrand Meyer 1988).

## P
- **Proxy Pattern**: A structural design pattern that provides a surrogate or placeholder for another object to control access to it (virtual, remote, protection, or caching proxy) (GoF 1994).

## R
- **Repository Pattern**: An enterprise application pattern that mediates between the domain and data mapping layers using a collection-like interface for accessing domain objects (PoEAA 2002).

## S
- **Saga Pattern**: A distributed transaction pattern that manages long-running business transactions across microservices as a sequence of local transactions coordinated through events or orchestrators, with compensating transactions for rollbacks.
- **Single Responsibility Principle (SRP)**: The principle that a module or class should have one, and only one, reason to change (meaning one actor or stakeholder) (Robert C. Martin).
- **State Pattern**: A behavioral design pattern that allows an object to alter its behavior when its internal state changes, appearing as if the object changed its class (GoF 1994).
- **Strategy Pattern**: A behavioral design pattern that defines a family of algorithms, encapsulates each one, and makes them interchangeable, letting the algorithm vary independently from clients that use it (GoF 1994).

## T
- **Template Method Pattern**: A behavioral design pattern that defines the skeleton of an algorithm in an operation, deferring some steps to subclasses without changing the algorithm's structure (GoF 1994).
- **Transactional Outbox**: A reliable distributed messaging pattern where domain events are committed to a local database outbox table in the same local transaction as business entities before being dispatched asynchronously to an event broker.

## U
- **Unit of Work**: An enterprise pattern that maintains a list of objects affected by a business transaction and coordinates the writing out of changes and the resolution of concurrency problems (PoEAA 2002).

## V
- **Visitor Pattern**: A behavioral design pattern that represents an operation to be performed on the elements of an object structure, letting you define a new operation without changing the classes of the elements on which it operates (GoF 1994).
