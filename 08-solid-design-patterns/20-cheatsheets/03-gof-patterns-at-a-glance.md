# GoF 23 Design Patterns at a Glance

## Creational (5)
| Pattern | Intent |
| :--- | :--- |
| **Singleton** | Ensure 1 instance (prefer DI singleton scope over static `getInstance()`) |
| **Factory Method** | Subclass/function decides which concrete product to instantiate |
| **Abstract Factory** | Create families of related/matching products |
| **Builder** | Step-by-step construction of complex/immutable objects |
| **Prototype** | Clone existing objects instead of constructing from scratch |

## Structural (7)
| Pattern | Intent |
| :--- | :--- |
| **Adapter** | Convert an incompatible interface into the one callers expect |
| **Bridge** | Decouple 2 independent hierarchies (Abstraction × Implementation) |
| **Composite** | Treat individual leaves and nested trees uniformly |
| **Decorator** | Dynamically stack extra responsibilities onto an interface |
| **Facade** | Provide a simple unified API over a complex subsystem |
| **Flyweight** | Share immutable intrinsic state across thousands of instances |
| **Proxy** | Surrogate controlling access (lazy init, auth, caching, remote) |

## Behavioural (11)
| Pattern | Intent |
| :--- | :--- |
| **Strategy** | Swap interchangeable algorithms at runtime |
| **Observer** | 1-to-many event notification to subscribers |
| **Command** | Encapsulate a request as an object (undo, queue, log) |
| **Chain of Responsibility** | Pass request along a chain of handlers |
| **Template Method** | Skeleton algorithm in base with overridable steps |
| **Iterator** | Traverse a collection sequentially without exposing internals |
| **Memento** | Capture & restore object state snapshot without breaking encapsulation |
| **Mediator** | Centralize many-to-many object communication |
| **State** | Alter behavior cleanly as internal lifecycle state changes |
| **Visitor** | Add operations to a closed hierarchy (replaced by `sealed` ADTs + `switch`) |
| **Interpreter** | Evaluate a small domain-specific grammar AST |
