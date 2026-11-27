# Retry with Exponential Backoff & Jitter

| Field | Value |
| :--- | :--- |
| **Category** | Distributed & Cloud Patterns (Resilience) |
| **Origin** | AWS Architecture Blog (Marc Brooker, *Exponential Backoff And Jitter*) |
| **Difficulty** | Beginner → Intermediate |
| **Prerequisite(s)** | [Idempotency](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/06-idempotency-key-and-idempotent-consumer.md) |
| **Status** | Complete |

> **TL;DR**: Transient network glitches often self-heal in milliseconds, but retrying immediately or in lockstep causes **Thundering Herd** retry storms. **Exponential Backoff + Full Jitter** (`random(0, min(cap, base * 2^attempt))`) spreads retries out safely—only for **idempotent** and **transient** operations.

---

## 1. The Problem: Retry Storms & Thundering Herds

1. **Immediate Retries**: Hammering a struggling server 3 times in 1 ms guarantees it stays down.
2. **Fixed Exponential Backoff Without Jitter**: If 1,000 clients fail at `t=0`, all 1,000 retry together at `t=1s`, `t=2s`, `t=4s`—creating synchronized traffic spikes that knock the server back down!

---

## 2. Full Jitter Formula

$$\text{delay} = \text{random}\big(0,\ \min(\text{max\_cap},\ \text{base} \times 2^{\text{attempt}})\big)$$

- **Full Jitter** spreads all 1,000 clients uniformly across `[0, cap]`, flattening spikes dramatically.

---

## 3. Multi-Language Python Example (Verified)

```python
import random

def retry_with_jitter(fn, max_attempts=3, base_delay=0.01, max_cap=1.0):
    for attempt in range(max_attempts):
        try:
            return fn(attempt)
        except RuntimeError as e:
            if attempt == max_attempts - 1:
                raise
            cap = min(max_cap, base_delay * (2 ** attempt))
            delay = random.uniform(0, cap)
            print(f"Attempt {attempt + 1} failed ({e}); sleeping {delay:.4f}s")
```

---

## 4. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Retrying non-idempotent `POST /payments` without an `Idempotency-Key` | Charges the customer's card twice! | Only retry **idempotent** ops or attach an `Idempotency-Key` |
| Retrying at *every* hop in a 4-service call chain (`3 * 3 * 3 * 3 = 81x` amplification!) | Amplifies load 81x during partial outages | Retry only at the **outermost** edge or use a tight **Retry Budget** |
| Retrying `400 Bad Request` / `401` / `403` | Permanent errors never succeed on retry | Only retry transient errors (`429`, `502`, `503`, `504`, network timeouts) |

---

## 5. Interview Questions

<details>
<summary>1. Why do we add Jitter to Exponential Backoff?</summary>

Without jitter, clients that failed at the same moment retry in synchronized waves (Thundering Herd). Jitter randomizes the delay to spread load smoothly.

</details>

<details>
<summary>2. What is "Retry Amplification" in microservices?</summary>

When Service A retries 3x, B retries 3x, and C retries 3x, a single failure causes $3^3 = 27$ calls at the leaf service. Prevent it with **Retry Budgets** and **Deadlines**.

</details>

<details>
<summary>3. When is it unsafe to retry?</summary>

When the operation is **non-idempotent** (e.g., creating a payment or incrementing a counter without an idempotency key) or the error is permanent (`400`, `404`, `422`).

</details>

---

## 6. Cheat Sheet

- **Formula**: `random(0, min(cap, base * 2^attempt))`.
- **Only retry** transient faults on **idempotent** operations, honoring `Retry-After` headers.

---

## 7. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Circuit Breaker](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/01-circuit-breaker.md) | [Module 11](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [Timeout & Deadline](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/03-timeout-and-deadline.md) |
