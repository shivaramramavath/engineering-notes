# Case Study 08: E-Commerce Checkout

**Patterns Used**: **Aggregate** (`Order` + `OrderLine` + `Money` Value Object), **Repository + Unit of Work**, **Specification** (Coupon eligibility), **Saga + Transactional Outbox** (Reserve Inventory → Charge Payment → Ship), **Idempotency Key**.

```mermaid
flowchart LR
    API["POST /checkout (Idempotency-Key)"] --> Agg["Order Aggregate"]
    Agg --> UoW["UnitOfWork (Order + Outbox)"]
    UoW --> Saga["Checkout Saga"]
    Saga --> Inv["Inventory"]
    Saga --> Pay["PaymentGateway (ACL)"]
```

## Key Design Insight
Protect `Order` invariants inside the Aggregate, persist `Order` + `Outbox` atomically, and coordinate Inventory + Payment + Shipping via an idempotent Saga with compensating releases/refunds.
