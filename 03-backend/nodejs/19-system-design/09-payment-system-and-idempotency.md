# Payment Gateway Integration & Idempotency Architecture in Node.js

> Engineering mission-critical payment processing: Two-phase commit mitigation, idempotency keys, state machines, and webhook reconciliation.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Payment Provider** | Stripe / Adyen / Braintree |
| **Pattern** | Idempotent State Machine |

---

## 1. The Core Invariant: Zero Double-Charging

In payment processing, network timeouts can occur while waiting for a credit card processor's HTTP response.
If the client retries the request without an idempotency key, the customer could be charged twice.

---

## 2. Idempotent Payment Flow

```
Client ──(POST /api/pay + Idempotency-Key)──> Node.js API
                                                    │
                                                    ▼
                                          [ Check Redis Lock ]
                                                    │
                                     ┌──────────────┴──────────────┐
                                     ▼                             ▼
                              [ Key Exists ]               [ Key Not Exists ]
                                     │                             │
                                     ▼                             ▼
                        Return Cached Response / In-Flight  [ Acquire Lock ]
                                                                   │
                                                                   ▼
                                                       [ Call Stripe API with Key ]
                                                                   │
                                                                   ▼
                                                       [ Persist to Postgres ]
                                                                   │
                                                                   ▼
                                                       [ Cache Success in Redis ]
```

---

## 3. Webhook Reconciliation & Ledger State Machine

Never assume an immediate HTTP response is the authoritative payment outcome.
Payment state transitions (`Pending` $\rightarrow$ `Authorized` $\rightarrow$ `Captured` $\rightarrow$ `Settled` or `Failed`) must be confirmed via cryptographically verified asynchronous Webhooks:

```javascript
export async function handleStripeWebhook(event) {
  switch (event.type) {
    case 'payment_intent.succeeded': {
      const paymentIntent = event.data.object;
      await withTransaction(async (client) => {
        // Enforce state machine transition
        const res = await client.query(
          `UPDATE payments 
           SET status = 'succeeded', provider_tx_id = $1, updated_at = NOW()
           WHERE idempotency_key = $2 AND status = 'pending'`,
          [paymentIntent.id, paymentIntent.metadata.idempotencyKey]
        );
        if (res.rowCount === 0) {
          console.warn('Payment already settled or invalid status transition');
        }
      });
      break;
    }
  }
}
```
