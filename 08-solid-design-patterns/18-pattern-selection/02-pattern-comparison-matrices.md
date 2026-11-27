# Pattern Comparison Matrices (Look-Alikes)

| Pair / Group | Shared Structure | Key Difference in **Intent** |
| :--- | :--- | :--- |
| **Adapter vs Facade vs Proxy vs Decorator** | All wrap another object | **Adapter**: changes interface. **Facade**: simplifies subsystem. **Proxy**: controls access (same interface). **Decorator**: adds behavior (stackable). |
| **Strategy vs State** | Both delegate to an interchangeable object | **Strategy**: caller picks algorithm; rarely changes mid-op. **State**: object transitions *itself* between lifecycle states. |
| **Factory Method vs Abstract Factory vs Builder** | All encapsulate creation | **Factory Method**: 1 product via subclass/fn. **Abstract Factory**: family of matching products. **Builder**: step-by-step complex object. |
| **Chain of Responsibility vs Decorator** | Both wrap/chain handlers | **CoR**: stops when one handler handles (or passes along). **Decorator**: all layers enhance the same call. |
| **Repository vs DAO vs Gateway** | All abstract I/O | **Repository**: Aggregate collection. **DAO**: Table CRUD. **Gateway**: External API/service. |
| **Observer vs Mediator vs Pub/Sub** | All decouple communication | **Observer**: 1-to-many broadcast. **Mediator**: many-to-many hub. **Pub/Sub**: brokered async topics. |
