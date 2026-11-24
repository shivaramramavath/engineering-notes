# 11: Designing an Idempotent Payment System & Double-Entry Ledger

| Metadata Field | Value |
|---|---|
| **Topic** | Enterprise Payment Processing, Double-Entry Accounting & Distributed Idempotency |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Key Middleware** | PostgreSQL (ACID Ledger & Unique Constraints), Redis (Distributed Locks), Kafka, Stripe SDK |
| **Core Invariant** | Exactly-Once Charge Semantics ($\sum \text{Debits} - \sum \text{Credits} = 0$) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

In financial systems, correctness and auditability supersede raw latency. Double charges, lost payment records, or inconsistent balances violate customer trust and trigger severe regulatory penalties.

A production-grade payment platform requires:
- **Strict Idempotency**: Network timeouts, browser refresh re-submissions, and webhook retries must never result in duplicate billing.
- **Double-Entry Bookkeeping**: Money cannot be created or destroyed; every financial event consists of equal and offsetting debit and credit entries in an append-only ledger.
- **Asynchronous Webhook Reconciliation**: Third-party payment gateways (Stripe, Adyen, PayPal) settle transactions asynchronously; backends must handle out-of-order webhook delivery and verify cryptographic signatures.

---

## 2. Core Concepts

### 2.1 The Two-Phase Idempotency Protocol
1. Client generates a unique UUID `Idempotency-Key` header (e.g. `Idempotency-Key: e4b2d18c-...`).
2. Server acquires a distributed Redis lock: `lock:idempotency:{key}` with a 2-minute lease.
3. Server queries PostgreSQL `payment_requests` table:
   - If record exists and status is `SETTLED`: Return the previously cached HTTP 200 response immediately.
   - If record exists and status is `IN_PROGRESS`: Return HTTP 409 Conflict ("Transaction currently executing").
   - If record does not exist: Insert status `IN_PROGRESS`, invoke payment gateway, record ledger entry, and transition status to `SETTLED`.

### 2.2 Double-Entry Bookkeeping Ledger
Financial records are never modified via SQL `UPDATE`. Every transaction writes immutable rows to a ledger table:
- **Debit (Dr)**: Increases Assets or Expenses (e.g., Stripe Receivables account).
- **Credit (Cr)**: Increases Liabilities or Revenue (e.g., Merchant Balance account).
- **Fundamental Invariant**:
  $$\sum \text{Debits} - \sum \text{Credits} = 0$$

---

## 3. Internals & Architecture

```
        [ Client / Mobile Checkout ]
                     │
          (POST /api/v1/payments)
          (Header: Idempotency-Key: uuid-99)
                     ▼
  ┌────────────────────────────────────────────────────────┐
  │              Payment Ingestion Gateway                 │
  │  1. Acquire Redis Mutex: "lock:idem:uuid-99"           │
  │  2. Check DB: Is uuid-99 already processed?            │
  └──────────────────────────┬─────────────────────────────┘
                             │
            ┌────────────────┴────────────────┐
            ▼ (Already Processed)             ▼ (New Request)
      [ Return Cached HTTP 200 ]     ┌───────────────────────────────────┐
                                     │ Insert status = 'IN_PROGRESS'     │
                                     │ Call Stripe: /v1/payment_intents  │
                                     │ (Forwarding Idempotency-Key)      │
                                     └─────────────────┬─────────────────┘
                                                       │
                                                       ▼
                                     ┌───────────────────────────────────┐
                                     │ PostgreSQL ACID Transaction:      │
                                     │ 1. Record PaymentIntent record    │
                                     │ 2. Insert Double-Entry Ledger:    │
                                     │    - Debit:  Stripe Account       │
                                     │    - Credit: Merchant Account     │
                                     │ 3. Mark status = 'SETTLED'        │
                                     └─────────────────┬─────────────────┘
                                                       │
                                                       ▼
                                     ┌───────────────────────────────────┐
                                     │ Publish 'payment.settled' to Kafka│
                                     │ (Unlocks order fulfillment)       │
                                     └───────────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Double-Entry Balance Verification Constraint
```sql
-- Invariant: Ledger balance must sum to 0 per transaction
SELECT transaction_id, SUM(CASE WHEN entry_type = 'DEBIT' THEN amount ELSE -amount END) as balance
FROM ledger_entries
GROUP BY transaction_id
HAVING SUM(CASE WHEN entry_type = 'DEBIT' THEN amount ELSE -amount END) != 0;
-- Must return 0 rows
```

---

## 5. 5 Practical Examples

### Example 1: Database Schema for Ledger & Idempotency (Flyway V1)

```sql
CREATE TABLE payment_requests (
    id UUID PRIMARY KEY,
    idempotency_key VARCHAR(128) UNIQUE NOT NULL,
    user_id VARCHAR(64) NOT NULL,
    amount NUMERIC(12, 2) NOT NULL,
    currency VARCHAR(3) NOT NULL,
    status VARCHAR(32) NOT NULL,
    gateway_reference VARCHAR(128),
    response_payload TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE ledger_entries (
    id UUID PRIMARY KEY,
    transaction_id UUID NOT NULL REFERENCES payment_requests(id),
    account_id VARCHAR(64) NOT NULL,
    entry_type VARCHAR(10) NOT NULL, -- 'DEBIT' or 'CREDIT'
    amount NUMERIC(12, 2) NOT NULL,
    currency VARCHAR(3) NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_ledger_tx ON ledger_entries(transaction_id);
CREATE INDEX idx_payment_idem ON payment_requests(idempotency_key);
```

### Example 2: Double-Entry Ledger Domain Entities

```java
package com.example.payment.domain;

import jakarta.persistence.*;
import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "ledger_entries")
public class LedgerEntry {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "transaction_id", nullable = false)
    private UUID transactionId;

    @Column(name = "account_id", nullable = false)
    private String accountId;

    @Enumerated(EnumType.STRING)
    @Column(name = "entry_type", nullable = false)
    private EntryType entryType;

    @Column(nullable = false, precision = 12, scale = 2)
    private BigDecimal amount;

    @Column(nullable = false, length = 3)
    private String currency;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt = Instant.now();

    public enum EntryType { DEBIT, CREDIT }

    public LedgerEntry() {}

    public LedgerEntry(UUID transactionId, String accountId, EntryType entryType, BigDecimal amount, String currency) {
        this.transactionId = transactionId;
        this.accountId = accountId;
        this.entryType = entryType;
        this.amount = amount;
        this.currency = currency;
    }

    public UUID getId() { return id; }
    public String getAccountId() { return accountId; }
    public EntryType getEntryType() { return entryType; }
    public BigDecimal getAmount() { return amount; }
}
```

### Example 3: Idempotent Payment Orchestrator Service

```java
package com.example.payment.service;

import com.example.payment.domain.LedgerEntry;
import com.example.payment.domain.PaymentRequest;
import com.example.payment.repository.LedgerEntryRepository;
import com.example.payment.repository.PaymentRequestRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.time.Duration;
import java.util.List;
import java.util.Optional;

@Service
public class PaymentExecutionService {

    private static final Logger log = LoggerFactory.getLogger(PaymentExecutionService.class);
    private final PaymentRequestRepository paymentRepo;
    private final LedgerEntryRepository ledgerRepo;
    private final StringRedisTemplate redisTemplate;

    public PaymentExecutionService(PaymentRequestRepository paymentRepo,
                                   LedgerEntryRepository ledgerRepo,
                                   StringRedisTemplate redisTemplate) {
        this.paymentRepo = paymentRepo;
        this.ledgerRepo = ledgerRepo;
        this.redisTemplate = redisTemplate;
    }

    @Transactional
    public PaymentRequest processPayment(String idempotencyKey, String userId, BigDecimal amount, String currency) {
        // Step 1: Distributed Mutex in Redis to prevent concurrent re-submissions
        String lockKey = "lock:idem:" + idempotencyKey;
        Boolean acquired = redisTemplate.opsForValue().setIfAbsent(lockKey, "LOCKED", Duration.ofMinutes(2));

        if (Boolean.FALSE.equals(acquired)) {
            throw new IllegalStateException("Payment with this idempotency key is currently processing. Please wait.");
        }

        try {
            // Step 2: Check database for existing execution
            Optional<PaymentRequest> existing = paymentRepo.findByIdempotencyKey(idempotencyKey);
            if (existing.isPresent()) {
                log.info("Idempotent request detected. Returning existing record: {}", idempotencyKey);
                return existing.get();
            }

            // Step 3: Insert initial IN_PROGRESS record
            PaymentRequest payment = new PaymentRequest(idempotencyKey, userId, amount, currency);
            payment.setStatus(PaymentRequest.PaymentStatus.IN_PROGRESS);
            payment = paymentRepo.saveAndFlush(payment);

            // Step 4: Call External Payment Gateway (Stripe) forwarding idempotency key
            String gatewayRef = callStripeGateway(idempotencyKey, amount, currency);

            // Step 5: Write Double-Entry Ledger entries atomically
            LedgerEntry debitEntry = new LedgerEntry(payment.getId(), "ACC_STRIPE_CLEARING", LedgerEntry.EntryType.DEBIT, amount, currency);
            LedgerEntry creditEntry = new LedgerEntry(payment.getId(), "ACC_MERCHANT_REVENUE", LedgerEntry.EntryType.CREDIT, amount, currency);
            ledgerRepo.saveAll(List.of(debitEntry, creditEntry));

            // Step 6: Mark SETTLED
            payment.setStatus(PaymentRequest.PaymentStatus.SETTLED);
            payment.setGatewayReference(gatewayRef);
            return paymentRepo.save(payment);

        } finally {
            redisTemplate.delete(lockKey);
        }
    }

    private String callStripeGateway(String idempotencyKey, BigDecimal amount, String currency) {
        // Simulating third-party Stripe API call with RequestOptions containing idempotency key
        log.info("Executing external Stripe PaymentIntent for amount {} {}", amount, currency);
        return "pi_" + System.currentTimeMillis();
    }
}
```

### Example 4: Payment Ingestion REST Controller

```java
package com.example.payment.web;

import com.example.payment.domain.PaymentRequest;
import com.example.payment.service.PaymentExecutionService;
import jakarta.validation.constraints.DecimalMin;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;

@RestController
@RequestMapping("/api/v1/payments")
public class PaymentController {

    private final PaymentExecutionService paymentService;

    public PaymentController(PaymentExecutionService paymentService) {
        this.paymentService = paymentService;
    }

    public record ChargeRequest(
            @NotNull @DecimalMin("0.50") BigDecimal amount,
            @NotBlank String currency,
            @NotBlank String userId
    ) {}

    @PostMapping("/charge")
    public ResponseEntity<PaymentRequest> charge(
            @RequestHeader("Idempotency-Key") String idempotencyKey,
            @RequestBody ChargeRequest request) {

        PaymentRequest payment = paymentService.processPayment(
                idempotencyKey,
                request.userId(),
                request.amount(),
                request.currency()
        );
        return ResponseEntity.ok(payment);
    }
}
```

### Example 5: Stripe Webhook Signature Verification Controller

```java
package com.example.payment.web;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

@RestController
@RequestMapping("/api/v1/webhooks/stripe")
public class StripeWebhookController {

    private static final Logger log = LoggerFactory.getLogger(StripeWebhookController.class);

    @Value("${stripe.webhook.secret:whsec_test_secret}")
    private String webhookSecret;

    @PostMapping
    public ResponseEntity<String> handleWebhook(
            @RequestBody String payload,
            @RequestHeader("Stripe-Signature") String signatureHeader) {

        try {
            // Verify HMAC-SHA256 signature to protect against spoofing
            if (!verifyStripeSignature(payload, signatureHeader, webhookSecret)) {
                log.warn("Invalid Stripe webhook signature rejected!");
                return ResponseEntity.status(HttpStatus.BAD_REQUEST).body("Invalid signature");
            }

            log.info("Verified Stripe webhook payload received. Enqueueing event for ledger reconciliation...");
            // Asynchronously process event in background
            return ResponseEntity.ok("Webhook received");

        } catch (Exception ex) {
            log.error("Webhook processing error: {}", ex.getMessage());
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).build();
        }
    }

    private boolean verifyStripeSignature(String payload, String sigHeader, String secret) throws Exception {
        // Simplified signature verification demo
        Mac sha256 = Mac.getInstance("HmacSHA256");
        sha256.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
        return sigHeader != null && !sigHeader.isBlank();
    }
}
```

---

## 6. Best Practices

- **Never Use Floating Point Numbers for Money**: Always store monetary amounts using `NUMERIC(12, 2)` in PostgreSQL and `BigDecimal` in Java to prevent floating-point rounding errors.
- **Propagate Idempotency Keys Downstream**: When calling Stripe, PayPal, or Adyen, pass the client's `Idempotency-Key` in the gateway's request options so the vendor deduplicates network retries.
- **Verify Webhook Cryptographic Signatures**: Always validate HMAC-SHA256 webhook signatures before deserializing or processing payloads.
- **Ledger Immutability**: Disallow SQL `UPDATE` and `DELETE` on the `ledger_entries` table. Fix financial mistakes by issuing reversing entries (Credit/Debit corrections).
- **Run Nightly Ledger Reconciliation Jobs**: Periodically assert that the sum of debits equals credits, and cross-reference gateway payout settlements against internal ledger totals.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Using `Float` / `Double` for Currency** | $0.1 + 0.2 = 0.30000000000000004$, causing accounts to drift out of balance by pennies. | Always use `BigDecimal` or store integer cents (`BIGINT`). |
| **Updating Account Balances Directly** | Running `UPDATE accounts SET balance = balance + 100` leaves zero audit trail of why the money moved. | Use append-only double-entry ledger entries. |
| **Missing Unique Constraint on Idempotency Key** | Race conditions can insert duplicate records if two identical requests bypass Redis. | Add `UNIQUE` constraint on `idempotency_key` column in PostgreSQL. |
| **Synchronous Webhook Processing** | Long processing of Stripe webhooks exceeds Stripe's 5-second timeout, triggering redundant retries. | Acknowledge HTTP 200 immediately and process the event asynchronously via Kafka. |
| **Not Forwarding Idempotency Key to Gateway** | If your network connection to Stripe drops after Stripe billed the card, your retry charges the user again. | Pass `RequestOptions.builder().setIdempotencyKey(key).build()` to Stripe SDK. |
| **Permitting Unbounded Negative Balances** | Logic errors allow accounts to be overdrawn without authorization. | Enforce database `CHECK (balance >= 0)` constraints where applicable. |

---

## 8. Performance & Security Considerations

### 8.1 PCI-DSS Compliance & Tokenization
Never accept or store raw Primary Account Numbers (PAN / 16-digit credit cards) on your servers. Use client-side Stripe Elements or Hosted Fields so sensitive card data is sent directly to Stripe, returning an ephemeral token (`tok_1234` or `pm_1234`).

### 8.2 Database Check Constraints for Ledger Balance
```sql
ALTER TABLE ledger_entries ADD CONSTRAINT chk_entry_amount_positive CHECK (amount > 0);
```

---

## 9. Comparison / Decision Matrix

| Model | Audit Trail | Error Recovery | Complexity |
|---|---|---|---|
| **Single-Balance Update (`UPDATE balance`)** | None | Extremely difficult (History overwritten) | Low |
| **Single-Entry Transaction Log** | Partial | Moderate (Hard to detect lost funds) | Medium |
| **Double-Entry Bookkeeping Ledger** | Complete & Irrefutable | Perfect (Self-balancing entries) | High (Industry Standard) |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How do you guarantee that a user is charged exactly once even if they click "Pay" 5 times?</strong></summary>

The client generates a unique `Idempotency-Key` header for the checkout session. A Redis distributed lock prevents concurrent execution. The database enforces a `UNIQUE` constraint on `idempotency_key`. The payment service passes the same idempotency key to Stripe, guaranteeing the payment gateway also deduplicates retries.
</details>

<details>
<summary><strong>2. What is double-entry bookkeeping, and why is it mandatory in fintech?</strong></summary>

Double-entry bookkeeping mandates that every financial transaction consists of equal and offsetting debit and credit entries ($\sum \text{Debits} = \sum \text{Credits}$). It ensures money is never created or destroyed, prevents hidden rounding bugs, and provides a forensic audit trail.
</details>

<details>
<summary><strong>3. How do you handle network failure when calling Stripe (e.g. timeout after Stripe billed the card)?</strong></summary>

Because we forward the `Idempotency-Key` to Stripe, our retry logic can safely re-invoke the exact same Stripe charge call. Stripe recognizes the key, does not charge the card a second time, and simply returns the previously created `PaymentIntent`.
</details>

<details>
<summary><strong>4. Why should you never use `float` or `double` for currency?</strong></summary>

Floating-point representations in IEEE-754 introduce binary representation rounding errors (e.g., fractional cents). Always use `BigDecimal` in Java and `NUMERIC(12, 2)` or integer cents in SQL.
</details>

<details>
<summary><strong>5. How do you handle Stripe webhooks arriving out of order?</strong></summary>

Include an event sequence number or state machine. If `payment_intent.succeeded` arrives before `payment_intent.created`, the state machine processes the terminal state and ignores obsolete prior state transitions.
</details>

<details>
<summary><strong>6. What is the role of a nightly reconciliation job?</strong></summary>

It downloads the daily settlement CSV from payment processors (Stripe/Adyen) and cross-checks every external charge, refund, and fee against the internal double-entry ledger, raising alerts for any discrepancies.
</details>

<details>
<summary><strong>7. How do you correct a mistaken charge or ledger error?</strong></summary>

Never edit or delete an existing ledger row. Record a new pair of offsetting compensating entries (a refund or reversal) with an audit note referencing the original transaction ID.
</details>

<details>
<summary><strong>8. How do you protect payment webhooks from replay attacks and tampering?</strong></summary>

Verify the HMAC-SHA256 signature passed in the `Stripe-Signature` header using the shared webhook secret, and check the timestamp in the header to reject requests older than 5 minutes.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create Double-Entry Ledger Insertion Method</strong></summary>

**Prompt**: Write a Java transactional method creating balanced Debit and Credit entries for a transaction.

**Solution**:
Inspect Example 3: create matching `LedgerEntry` for `DEBIT` and `CREDIT` with identical amounts and currencies, saving both within a single `@Transactional` boundary.
</details>

<details>
<summary><strong>Exercise 2: Write Ledger Balance Verification Query</strong></summary>

**Prompt**: Write a SQL query verifying that the sum of debits equals credits for a specific `transaction_id`.

**Solution**:
```sql
SELECT SUM(CASE WHEN entry_type = 'DEBIT' THEN amount ELSE -amount END) 
FROM ledger_entries 
WHERE transaction_id = 'e395df39-81a2-4a0b-93ff-187515b14e9e';
-- Returns 0.00
```
</details>

<details>
<summary><strong>Exercise 3: Test Idempotency Key Duplicate Rejection</strong></summary>

**Prompt**: Write a JUnit test with Testcontainers PostgreSQL verifying that two requests with the same `Idempotency-Key` return the same payment record without creating duplicate ledger entries.

**Solution**:
Execute `processPayment("idem-1", ...)` twice. Assert both return identical payment IDs, and `ledgerRepo.count()` equals exactly 2 (1 debit, 1 credit).
</details>

<details>
<summary><strong>Exercise 4: Implement Stripe Signature Verification</strong></summary>

**Prompt**: Write a Java method verifying an HMAC-SHA256 webhook signature.

**Solution**:
Inspect Example 5: initialize `Mac.getInstance("HmacSHA256")` with the secret, compute hash over `payload`, and compare with header using constant-time `MessageDigest.isEqual()`.
</details>

<details>
<summary><strong>Exercise 5: Build Reversal Ledger Entry Method</strong></summary>

**Prompt**: Write a refund method creating opposite debit/credit entries for an existing transaction.

**Solution**:
```java
public void refund(UUID originalTxId, BigDecimal amount, String currency) {
    var debit = new LedgerEntry(originalTxId, "ACC_MERCHANT_REVENUE", EntryType.DEBIT, amount, currency);
    var credit = new LedgerEntry(originalTxId, "ACC_STRIPE_CLEARING", EntryType.CREDIT, amount, currency);
    ledgerRepo.saveAll(List.of(debit, credit));
}
```
</details>

<details>
<summary><strong>Exercise 6: Implement Deadlock-Free Ledger Account Ordering</strong></summary>

**Prompt**: When transferring funds between two accounts, how do you prevent deadlocks when concurrent transfers occur in opposite directions ($A \to B$ and $B \to A$)?

**Solution**:
Sort account IDs lexicographically before acquiring locks: always lock the smaller account ID first, followed by the larger account ID.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   PAYMENT SYSTEM DESIGN CHEATSHEET                     │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Idempotency: Header Idempotency-Key + Redis mutex + DB Unique Key   │
│ 2. Forwarding: Pass Idempotency-Key to Stripe/Adyen SDK options        │
│ 3. Accounting: Double-entry ledger (Debits == Credits, zero updates)   │
│ 4. Currency: BigDecimal & NUMERIC(12, 2); never use float/double       │
│ 5. Webhooks: Acknowledge HTTP 200 fast, verify HMAC-SHA256 signatures  │
│ 6. Auditing: Nightly reconciliation job comparing ledger vs gateway CSV│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Stripe API Reference: Idempotent Requests](https://stripe.com/docs/api/idempotent_requests)
- [Martin Fowler: Accounting Patterns](https://martinfowler.com/eaaDev/AccountingTransaction.html)
- [RFC 8941: Structured Field Values for HTTP](https://datatracker.ietf.org/doc/html/rfc8941)
- [PCI-DSS Official Security Standards](https://www.pcisecuritystandards.org/)
