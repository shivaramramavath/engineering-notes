---
title: "The Strategy Pattern in NestJS"
description: "Implementing interchangeable algorithms, Passport authentication strategies, and multi-gateway payment processing in NestJS"
tags: [nestjs, design-patterns, strategy, passport, payment-gateways]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Strategy Pattern in NestJS

## Conceptual Overview
The Strategy pattern defines a family of algorithms, encapsulates each one, and makes them interchangeable at runtime.

In NestJS:
- **`@nestjs/passport`**: Extensively uses strategies (`JwtStrategy`, `LocalStrategy`, `GoogleStrategy`).
- **Payment Processing**: Selecting Stripe, PayPal, or Adyen based on tenant or currency.

---

## Implementation Details: Multi-Payment Gateway Strategy

```typescript
// src/payments/payment-strategy.interface.ts
export interface PaymentStrategy {
  readonly gatewayName: string;
  charge(amount: number, currency: string, token: string): Promise<PaymentResult>;
}

@Injectable()
export class StripeStrategy implements PaymentStrategy {
  readonly gatewayName = 'STRIPE';
  async charge(amount: number, currency: string, token: string) {
    // Stripe SDK logic...
    return { status: 'SUCCESS', transactionId: 'ch_123' };
  }
}

@Injectable()
export class PayPalStrategy implements PaymentStrategy {
  readonly gatewayName = 'PAYPAL';
  async charge(amount: number, currency: string, token: string) {
    // PayPal SDK logic...
    return { status: 'SUCCESS', transactionId: 'pp_456' };
  }
}

// Strategy Context Resolver
@Injectable()
export class PaymentContextService {
  private readonly strategies = new Map<string, PaymentStrategy>();

  constructor(stripe: StripeStrategy, paypal: PayPalStrategy) {
    this.strategies.set(stripe.gatewayName, stripe);
    this.strategies.set(paypal.gatewayName, paypal);
  }

  async executePayment(gateway: string, amount: number, currency: string, token: string) {
    const strategy = this.strategies.get(gateway.toUpperCase());
    if (!strategy) throw new BadRequestException(`Unsupported gateway: ${gateway}`);
    return strategy.charge(amount, currency, token);
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Strategy decouples the caller from concrete algorithmic implementations.
- Passport strategies implement the strategy pattern natively.
- Use a Context service or map to resolve strategies dynamically at runtime.

---

## Cross-References
- [06-authentication](../../04-intermediate/06-authentication/README.md) - Passport JWT strategies
- [03-adapter.md](./03-adapter.md) - Adapter pattern
