# System Design Blueprint: Robust Payment Gateway & Financial Ledger Integration

## Metadata

- **Topic**: Financial Payment Orchestration, Idempotency, Double-Entry Ledger & Webhook Reconciliation
- **Scale Requirements**: 5 Million daily transactions, $500M annual transaction volume, strict PCI-DSS scope minimization
- **Target SLA**: Zero double-charges, zero silent payment dropouts, 99.999% ledger consistency, sub-second checkout response
- **Tech Stack**: FastAPI, Stripe / Adyen / Braintree APIs, PostgreSQL (Double-Entry Bookkeeping Ledger), Redis Mutex

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Zero Double-Charging Guarantee**: Strict idempotency on checkout endpoints using client-generated unique `Idempotency-Key` headers.
2. **PCI-DSS Compliance**: No raw credit card numbers or CVVs ever touch our backend servers (Stripe Elements / Tokens only).
3. **Double-Entry Bookkeeping Ledger**: Every monetary movement records balancing credit and debit entries for mathematically guaranteed balance verification.
4. **Resilient Webhook Ingestion**: Asynchronous reconciliation with automatic retry and dead-letter queueing for vendor outages.

### 1.2 Non-Functional Requirements & Security
- **Strict Consistency**: ACID transactions with serializable or repeatable read isolation levels on financial accounts.
- **Auditability**: Immutable ledger rows; no `UPDATE` or `DELETE` statements allowed on financial history. Corrections must be booked as new reversing transactions.

---

## 2. High-Level Architecture Diagram

```
                 +-----------------------------------------------+
                 |        Client Web / Mobile Application        |
                 +-----------------------+-----------------------+
                                         |
                                         | 1. Collect CC via Stripe Elements (Client -> Stripe)
                                         |    Returns payment_method_id
                                         v
                 +-----------------------------------------------+
                 | POST /api/v1/payments/charge                  |
                 | Headers: Idempotency-Key: <UUID>              |
                 +-----------------------+-----------------------+
                                         |
                                         v
                 +-----------------------+-----------------------+
                 | FastAPI Payment Gateway Cluster               |
                 +-----------------------+-----------------------+
                                         |
                                         | 2. Check / Acquire Redis Idempotency Mutex
                                         v
                         +---------------+---------------+
                         | Redis Lock & Cache            |
                         | Key: "idem:pay:{key}"         |
                         +---------------+---------------+
                                         |
                                         | 3. Invoke Vendor API (Stripe PaymentIntent.create)
                                         v
                         +---------------+---------------+
                         | Stripe / Adyen Payment Gateway|
                         +---------------+---------------+
                                         |
                                         | 4. Record Double-Entry Transactions
                                         v
                         +---------------+---------------+
                         | PostgreSQL Financial Ledger   |
                         | |-- accounts                  |
                         | |-- ledger_entries (Immutable)|
                         | `-- idempotency_records       |
                         +---------------+---------------+
```

---

## 3. Double-Entry Bookkeeping Schema

In double-entry bookkeeping, the fundamental accounting equation must hold: $\sum \text{Debits} = \sum \text{Credits}$.

```sql
CREATE TABLE accounts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NULL,
    type VARCHAR(32) NOT NULL, -- ASSET, LIABILITY, REVENUE, EXPENSE
    currency VARCHAR(3) NOT NULL DEFAULT 'USD',
    balance NUMERIC(14, 2) NOT NULL DEFAULT 0.00,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE ledger_transactions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    reference_id VARCHAR(128) UNIQUE NOT NULL, -- e.g. stripe_charge_id
    description TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE ledger_entries (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    transaction_id UUID NOT NULL REFERENCES ledger_transactions(id),
    account_id UUID NOT NULL REFERENCES accounts(id),
    amount NUMERIC(14, 2) NOT NULL, -- Positive for Debit, Negative for Credit
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Constraint: Sum of amounts in any transaction MUST equal 0.00
```

---

## 4. FastAPI Production Implementation Blueprint

```python
from decimal import Decimal
import uuid
from fastapi import APIRouter, Depends, Header, HTTPException, status
from pydantic import BaseModel, Field
import redis.asyncio as aioredis
from sqlalchemy.ext.asyncio import AsyncSession
import stripe

router = APIRouter(prefix="/payments", tags=["Payments"])
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)


class ChargeRequest(BaseModel):
    amount: Decimal = Field(..., gt=0)
    currency: str = "USD"
    payment_method_id: str


@router.post("/charge", status_code=status.HTTP_200_OK)
async def process_payment(
    req: ChargeRequest,
    idempotency_key: str = Header(..., alias="Idempotency-Key"),
):
    lock_key = f"lock:pay:{idempotency_key}"
    # Acquire Redis Mutex (TTL 60s) to block concurrent duplicate requests
    acquired = await redis_client.set(lock_key, "LOCKED", nx=True, ex=60)
    if not acquired:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="A payment with this Idempotency-Key is already in flight.",
        )

    try:
        # Check if already processed
        cached_result = await redis_client.get(f"result:pay:{idempotency_key}")
        if cached_result:
            return {"status": "SUCCESS", "cached": True, "charge_id": cached_result}

        # 1. Dispatch charge to Stripe
        amount_cents = int(req.amount * 100)
        intent = stripe.PaymentIntent.create(
            amount=amount_cents,
            currency=req.currency,
            payment_method=req.payment_method_id,
            confirm=True,
            idempotency_key=idempotency_key,  # Pass through to Stripe's own API
        )

        # 2. Record in Ledger (Atomic DB transaction omitted for brevity)
        # 3. Cache Result (24-hour retention)
        await redis_client.set(f"result:pay:{idempotency_key}", intent.id, ex=86400)
        return {"status": "SUCCESS", "charge_id": intent.id}
    finally:
        await redis_client.delete(lock_key)
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **API Timeout after Stripe Charge Created** | Customer billed, but backend network cut before saving order record. | Stripe Webhook (`payment_intent.succeeded`) triggers reconciliation worker, creating the missing ledger and order entries automatically. |
| **Ledger Imbalance Bug** | Code defect inserts debit without equal credit, corrupting financial reporting. | Database Trigger enforces `CHECK (SUM(amount) = 0)` on transaction commit; nightly automated reconciliation job audits account balances. |
| **Webhook Flooding Attack** | Attacker replays forged Stripe webhook payloads. | Compute constant-time HMAC check against `Stripe-Signature` header; drop events older than 5 minutes. |
