# Module 07: Behavioral Design Patterns

Welcome to **Module 07: Behavioral Design Patterns**. Behavioral patterns govern algorithms, state transitions, undo/redo histories, event propagation, and communication protocols between objects—eliminating brittle `switch` statements, cyclic peer coupling, and rigid control flows.

---

## Module Roadmap & Contents

| # | Note | Core Focus & Purpose | Status | Est. Time |
|---|---|---|:---:|:---:|
| 00 | [Behavioral Patterns Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/00-overview.md) | Communication topology, Strategy vs State vs Command disambiguation | Complete | 15 min |
| 01 | [Strategy Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/01-strategy.md) | Interchangeable algorithms at runtime, functional lambdas (OCP) | Complete | 30 min |
| 02 | [Observer Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/02-observer.md) | Reactive one-to-many event notifications, avoiding memory/listener leaks | Complete | 30 min |
| 03 | [Command Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/03-command.md) | Encapsulating requests as objects for undo/redo, queuing, and audit logs | Complete | 30 min |
| 04 | [State Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/04-state.md) | Replacing fragile `if (status == ...)` machines with polymorphic state objects | Complete | 30 min |
| 05 | [Template Method Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/05-template-method.md) | Fixed algorithm skeleton with overridable step hooks (Hollywood Principle) | Complete | 25 min |
| 06 | [Chain of Responsibility Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/06-chain-of-responsibility.md) | Middleware pipelines (Auth $\to$ RateLimit $\to$ Handler) with short-circuiting | Complete | 30 min |
| 07 | [Iterator Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/07-iterator.md) | Uniform lazy traversal (`Iterable` / `yield`) hiding collection internals | Complete | 25 min |
| 08 | [Mediator Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/08-mediator.md) | Eliminating $N^2$ many-to-many peer coupling via a central coordinator | Complete | 25 min |
| 09 | [Memento Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/09-memento.md) | Capturing opaque state snapshots for rollback/undo without breaking encapsulation | Complete | 25 min |
| 10 | [Visitor Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/10-visitor.md) | Adding external operations over stable ASTs via Double Dispatch vs `sealed` `switch` | Complete | 30 min |
| 11 | [Interpreter Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/11-interpreter.md) | Evaluating mini-DSLs and expression trees (and when to use a real parser instead) | Complete | 25 min |
| 12 | [Specification Pattern (Behavioral)](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/12-specification-as-behavioural.md) | Composable business rules (`and`, `or`, `not`) for validation & eligibility | Complete | 25 min |
| 13 | [Event Bus Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/13-event-bus.md) | In-process decoupled typed event routing with error isolation | Complete | 25 min |
| 14 | [Blackboard & Pub-Sub Variants](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/14-blackboard-and-publish-subscribe-variants.md) | Shared workspace collaboration & Observer vs Pub-Sub broker decoupling | Complete | 25 min |
| 15 | [Behavioral Patterns Comparison](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/15-behavioural-patterns-comparison.md) | Decision matrix, look-alike disambiguation, and modern idioms | Complete | 15 min |

---

[← Back to Module 06: Structural Patterns](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/README.md) | [Next: 00 - Behavioral Patterns Overview →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/00-overview.md)
