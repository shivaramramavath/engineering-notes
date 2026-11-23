# Module 28: TypeScript Design Patterns Cheatsheet

> **Quick Reference**: Idiomatic implementations of Factory, Builder, Strategy, and Observer patterns in TypeScript.

---

## 1. Type-Safe Builder Pattern

```typescript
class RequestBuilder<TBody = never> {
  private url = "";
  private method: "GET" | "POST" = "GET";
  private body?: TBody;

  setUrl(url: string): this {
    this.url = url;
    return this;
  }

  setMethod(method: "GET" | "POST"): this {
    this.method = method;
    return this;
  }

  setBody<B>(body: B): RequestBuilder<B> {
    const builder = this as unknown as RequestBuilder<B>;
    builder.body = body;
    return builder;
  }

  build(): { url: string; method: string; body?: TBody } {
    return { url: this.url, method: this.method, body: this.body };
  }
}
```

---

## 2. Discriminated Union Strategy Pattern

```typescript
type PaymentStrategy =
  | { type: "credit_card"; cardNumber: string; cvv: string }
  | { type: "paypal"; email: string }
  | { type: "crypto"; walletAddress: string };

function processPayment(strategy: PaymentStrategy, amount: number): void {
  switch (strategy.type) {
    case "credit_card":
      console.log(`Processing card ${strategy.cardNumber} for $${amount}`);
      break;
    case "paypal":
      console.log(`Processing PayPal ${strategy.email} for $${amount}`);
      break;
    case "crypto":
      console.log(`Sending $${amount} to wallet ${strategy.walletAddress}`);
      break;
  }
}
```\n