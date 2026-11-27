# Clean, Hexagonal (Ports & Adapters) & Onion Architecture

| Field | Value |
| :--- | :--- |
| **Category** | Software Architecture (Dependency Inversion) |
| **Origin** | Alistair Cockburn (Hexagonal, 2005), Jeffrey Palermo (Onion, 2008), Robert C. Martin (Clean, 2012) |
| **Difficulty** | Intermediate → Advanced |
| **Prerequisite(s)** | [Dependency Inversion Principle](file:///E:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/05-dependency-inversion.md), [Layered Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/01-layered.md) |
| **Status** | Complete |

> **TL;DR**: Hexagonal, Onion, and Clean Architecture all enforce one golden rule—**The Dependency Rule**: source-code dependencies must point **inward** toward the pure Domain Core. Databases, HTTP frameworks, and external APIs are **plugins (Adapters)** implementing **Ports (Interfaces)** owned by the core.

---

## 1. The Problem: Why Do Clean & Hexagonal Exist?

In classic Layered Architecture, your business logic imports Hibernate `@Entity`, Express `Request`, or Django `models.Model`. When you try to:
1. Run fast unit tests → you have to boot PostgreSQL or an HTTP test server.
2. Upgrade a major framework or swap MongoDB for PostgreSQL → you rewrite business logic.
3. Expose the same domain use case via both REST and Kafka → HTTP assumptions are baked into the core.

---

## 2. How Ports & Adapters (Hexagonal) Works

- **Inside the Hexagon (Core)**: Domain Entities, Value Objects, Use Cases / Application Services, and **Ports** (interfaces). Zero framework/SQL imports!
- **Inbound (Driving) Adapters**: REST Controllers, GraphQL resolvers, CLI commands, Kafka consumers—they call **Inbound Ports** (`PlaceOrderUseCase`).
- **Outbound (Driven) Adapters**: Postgres Repositories, Stripe Gateways, Redis Caches—they *implement* **Outbound Ports** (`OrderRepository`, `PaymentGateway`) defined by the core.

```mermaid
flowchart LR
    REST["REST Controller<br/>(Driving Adapter)"] --> IP["Inbound Port<br/>(UseCase Interface)"]
    IP --> CORE["Domain Core + Use Cases<br/>(Zero Framework Imports)"]
    CORE --> OP["Outbound Port<br/>(Repo / Gateway Interface)"]
    OP <|.. PG["Postgres Adapter<br/>(Driven Adapter)"]
    OP <|.. ST["Stripe Adapter<br/>(Driven Adapter)"]
```

---

## 3. Clean vs Hexagonal vs Onion

| Name | Emphasis |
| :--- | :--- |
| **Hexagonal (Ports & Adapters)** | Symmetry of Inbound/Outbound Ports and swappable Adapters |
| **Onion Architecture** | Concentric rings around Domain Model → Domain Services → Application Services → Infra |
| **Clean Architecture** | Entities → Use Cases → Interface Adapters → Frameworks & Drivers (**Dependency Rule**) |

All three share the **exact same core idea**: isolate business rules from I/O and frameworks via Dependency Inversion.

---

## 4. Multi-Language Minimal Port & Adapter Example

### Java 25 LTS
```java
public class HexagonalDemo {
    // Outbound Port (owned by Core)
    interface OrderRepoPort { void save(String id, long cents); }
    // Inbound Port / Use Case
    static class PlaceOrderUseCase {
        private final OrderRepoPort repo;
        PlaceOrderUseCase(OrderRepoPort repo) { this.repo = repo; }
        String execute(long cents) {
            if (cents <= 0) throw new IllegalArgumentException("Bad cents");
            repo.save("ord-1", cents);
            return "ord-1";
        }
    }
    public static void main(String[] args) {
        var useCase = new PlaceOrderUseCase((id, c) -> System.out.println("Adapter saved " + id));
        System.out.println("Result: " + useCase.execute(1000));
    }
}
```

### TypeScript 5.8
```ts
// Port
interface OrderRepoPort { save(id: string, cents: number): Promise<void>; }
// Core Use Case
const makePlaceOrder = (repo: OrderRepoPort) => async (cents: number) => {
  if (cents <= 0) throw new Error('Bad cents');
  await repo.save('ord-1', cents);
  return 'ord-1';
};
// Adapter
makePlaceOrder({ save: async (id, c) => console.log('Adapter saved', id, c) })(1000);
```

### Python 3.14
```python
from typing import Protocol

class OrderRepoPort(Protocol):
    def save(self, order_id: str, cents: int) -> None: ...

class PlaceOrderUseCase:
    def __init__(self, repo: OrderRepoPort) -> None:
        self._repo = repo
    def execute(self, cents: int) -> str:
        if cents <= 0: raise ValueError("Bad cents")
        self._repo.save("ord-1", cents)
        return "ord-1"

class MemoryAdapter:
    def save(self, order_id: str, cents: int) -> None:
        print("Adapter saved", order_id, cents)

print(PlaceOrderUseCase(MemoryAdapter()).execute(1000))
```

---

## 5. Real-World Examples

1. **Testability**: Testing 200 domain use cases in < 100 ms using in-memory Port fakes instead of Dockerized Postgres.
2. **Multi-Channel Apps**: Driving the exact same Core from REST, gRPC, and CLI adapters.

---

## 6. Advantages & Limitations

- **Advantages**: Millisecond unit tests, framework independence, crystal-clear separation of business rules vs I/O.
- **Limitations**: Extra mapping/interface boilerplate (`DomainOrder` ↔ `JpaOrderEntity` ↔ `OrderResponseDto`)—overkill for simple CRUD microservices.

---

## 7. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Importing Spring/NestJS/SQLAlchemy inside the Core | Violates the Dependency Rule | Keep Core 100% pure language types |
| Using Clean Architecture for a 3-endpoint CRUD proxy | Massive mapping overhead for zero domain logic | Use Layered/Transaction Script for simple CRUD |
| Creating 1-to-1 `Port` interfaces for things that will never be swapped (e.g., `StringUtilsPort`) | Boilerplate bloat | Only define Ports at **I/O boundaries** (DB, HTTP, Queue, Clock) |
| Letting ORM entities act as Domain Entities | Couples Core to DB annotations & lazy proxies | Map at the Persistence Adapter boundary |

---

## 8. Interview Questions

<details>
<summary>1. What is the Dependency Rule in Clean Architecture?</summary>

Source-code dependencies may only point **inward**: outer layers (Web, DB, Frameworks) can depend on inner layers (Use Cases, Entities), never the reverse.

</details>

<details>
<summary>2. What is the difference between a Driving (Inbound) Adapter and a Driven (Outbound) Adapter?</summary>

A Driving Adapter initiates calls *into* the application (REST Controller, CLI, Queue Consumer). A Driven Adapter is called *by* the application to talk to external systems (Database Repository, Email Gateway).

</details>

<details>
<summary>3. When is Clean/Hexagonal Architecture overkill?</summary>

For simple CRUD services, admin dashboards, or short-lived prototypes with minimal business rules.

</details>

<details>
<summary>4. How do you enforce the Dependency Rule automatically?</summary>

Using Architecture Fitness Functions (`ArchUnit` in Java, `dependency-cruiser` in TS, `import-linter` in Python) in CI.

</details>

---

## 9. Cheat Sheet

- **Rule**: Dependencies point **inward**; Core has zero framework/DB imports.
- **Ports**: Interfaces at I/O boundaries owned by Core.
- **Adapters**: Swappable REST, SQL, HTTP, and Test implementations.

---

## 10. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Layered Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/01-layered.md) | [Module 10](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/README.md) | [Modular Monolith](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/03-modular-monolith.md) |
