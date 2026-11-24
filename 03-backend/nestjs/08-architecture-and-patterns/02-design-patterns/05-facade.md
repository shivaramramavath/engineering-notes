---
title: "The Facade Pattern in NestJS"
description: "Simplifying complex subsystem workflows into unified entrypoints for controllers and external consumers"
tags: [nestjs, design-patterns, facade, subsystem-orchestration, simplification]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Facade Pattern in NestJS

## Conceptual Overview
The Facade pattern provides a simplified, high-level interface to a complex set of classes, library routines, or microservices. In NestJS, module facades encapsulate multi-step domain workflows so controllers remain lean and declarative.

---

## Implementation Details: Order Checkout Facade

```typescript
@Injectable()
export class CheckoutFacade {
  constructor(
    private readonly cartService: CartService,
    private readonly paymentService: PaymentService,
    private readonly inventoryService: InventoryService,
    private readonly notificationService: NotificationService,
  ) {}

  async checkout(userId: string, paymentMethodId: string) {
    const cart = await this.cartService.getCart(userId);
    await this.inventoryService.reserveItems(cart.items);
    const payment = await this.paymentService.charge(userId, cart.total, paymentMethodId);
    await this.cartService.clearCart(userId);
    await this.notificationService.sendReceipt(userId, payment.id);

    return { orderId: payment.id, status: 'COMPLETED' };
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Use Facades to coordinate multiple domain services behind a single clean method call.
- Keeps controllers thin and shields consumers from internal subsystem churn.

---

## Cross-References
- [01-layered-architecture.md](../01-architecture/01-layered-architecture.md) - Layered architecture
- [07-service-layer.md](./07-service-layer.md) - Service layer pattern
