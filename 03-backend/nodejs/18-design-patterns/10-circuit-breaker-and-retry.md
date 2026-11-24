# Circuit Breaker & Exponential Backoff in Node.js

> Building fault-tolerant distributed integrations using the Circuit Breaker pattern (opossum) and jittered exponential retry backoff.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Packages** | `opossum` v8+ |
| **Patterns** | Circuit Breaker (Closed, Open, Half-Open), Exponential Jitter |

---

## 1. What is a Circuit Breaker?

When a downstream third-party service (e.g. Stripe, SMS Gateway) suffers an outage, continuing to hammer it with thousands of requests wastes server sockets, exhausts worker threads, and degrades your entire application.

The **Circuit Breaker** acts as an electrical fuse with 3 states:
- **Closed**: Normal operation. Requests flow to downstream service. Failure counts are tracked.
- **Open**: Failure rate exceeds threshold (e.g. 50% errors). **Calls fail immediately** without hitting the network, protecting the downstream system.
- **Half-Open**: After a reset timeout (e.g. 10s), trial requests test whether the downstream service has recovered.

```
       ┌─────────── Success (Reset) ───────────┐
       ▼                                       │
  [ Closed ] ──(Failures > 50%)──> [ Open ] ──(Timeout)──> [ Half-Open ]
                                       ▲                          │
                                       └──────── Failure ─────────┘
```

---

## 2. Implementation with `opossum`

```javascript
import CircuitBreaker from 'opossum';

async function callThirdPartyPaymentApi(paymentData) {
  const res = await fetch('https://api.payment-provider.com/charge', {
    method: 'POST',
    body: JSON.stringify(paymentData),
    signal: AbortSignal.timeout(3000), // 3s timeout
  });
  if (!res.ok) throw new Error(`HTTP Error: ${res.status}`);
  return res.json();
}

const breaker = new CircuitBreaker(callThirdPartyPaymentApi, {
  timeout: 3000,             // If action takes longer than 3s, trigger failure
  errorThresholdPercentage: 50, // Open circuit if 50% of requests fail
  resetTimeout: 10000,       // Wait 10s before testing recovery in half-open state
});

// Fallback response when circuit is OPEN
breaker.fallback(() => ({
  status: 'queued',
  message: 'Payment service currently unavailable. Queued for background processing.',
}));

export async function processPayment(paymentData) {
  return await breaker.fire(paymentData);
}
```
