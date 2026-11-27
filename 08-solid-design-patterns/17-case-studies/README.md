# Module 17: Real-World Case Studies (Low-Level Design)

Real systems never use one pattern in isolation—they **combine** 3–6 patterns cleanly. Each case study below walks through requirements, the exact pattern combination chosen, a class/sequence diagram, and runnable multi-language skeleton code.

---

## Case Studies in This Module

| # | Case Study | Key Patterns Combined |
| :--- | :--- | :--- |
| 00 | [Module Overview](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/00-overview.md) | How to approach any LLD case study |
| 01 | [Text Editor (Undo/Redo & Formatting)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/01-text-editor.md) | Command + Memento + Composite + Flyweight |
| 02 | [Multi-Channel Notification System](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/02-notification-system.md) | Strategy + Factory + Decorator (Retry) + Observer + Outbox |
| 03 | [Payment Gateway Integration](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/03-payment-gateway-integration.md) | Adapter/Gateway + Circuit Breaker + Idempotency Key + Result |
| 04 | [Parking Lot System](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/04-parking-lot.md) | Strategy (Pricing/Spot) + Factory + State + Observer |
| 05 | [Elevator Controller](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/05-elevator-controller.md) | State + Strategy (Scheduling) + Command + Observer |
| 06 | [Logging Framework](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/06-logging-framework.md) | Chain of Responsibility + Strategy (Formatter) + Singleton/DI + Async Queue |
| 07 | [Plugin-Based Application](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/07-plugin-based-application.md) | Microkernel + Registry + Facade + Event Bus |
| 08 | [E-Commerce Checkout](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/08-e-commerce-checkout.md) | Aggregate + Repository + Unit of Work + Saga + Domain Events |
| 09 | [Game Engine Entities](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/09-game-engine-entities.md) | Composition / ECS + Flyweight + Object Pool + State |
| 10 | [Rate Limiter Library](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/10-rate-limiter-library.md) | Strategy (Token Bucket / Sliding Window) + Decorator + Thread-Safe Store |
| 11 | [Workflow Engine](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/11-workflow-engine.md) | State Machine + Command + Saga + Template Method |
| 12 | [HTTP Middleware Pipeline](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/12-http-middleware-pipeline.md) | Chain of Responsibility / Onion Middleware + Decorator + Context |

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Module 16: Production](file:///E:/Learning/engineering-notes/08-solid-design-patterns/16-production/README.md) | [Track Index](file:///E:/Learning/engineering-notes/08-solid-design-patterns/INDEX.md) | [Module 18: Pattern Selection](file:///E:/Learning/engineering-notes/08-solid-design-patterns/18-pattern-selection/README.md) |
