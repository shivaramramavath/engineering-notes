# Enterprise Idempotency Patterns in Distributed Java Systems

> Design patterns, database deduplication schemas, concurrent request locking, payload fingerprinting, and transactional response caching in Java enterprise applications.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [`16-jdbc-and-databases/01_transactions-and-acid.md`](../16-jdbc-and-databases/01_transactions-and-acid.md), [`14-concurrency/04_locks-and-synchronizers.md`](../14-concurrency/04_locks-and-synchronizers.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified (Java 8 to 25 LTS) |
| **Standard** | Java SE 21 / 25 LTS |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In distributed networks, communication is inherently unreliable. When a client invokes a remote service (e.g. `POST /api/v1/payments`), three outcomes can occur:
1. **Success**: The service receives the request, mutates state, and returns HTTP 200.
2. **Failure**: The service fails or crashes; the client receives HTTP 500 or connection refused.
3. **Network Timeout (Indeterminate State)**: The service processed the mutation successfully, but the network connection severed before the response reached the client.

If the client retries upon timeout, an un-idempotent system executes the mutation again—charging the user twice or generating duplicate order records.

An **Idempotent** operation guarantees that making the same request multiple times produces the identical server state and returns the identical response as making the request once:
$$f(f(x)) = f(x)$$

---

## 2. Core Concepts

### 2.1 HTTP Verbs and Natural Idempotency

| HTTP Method | Idempotent Spec | Safe (Read-Only) | Notes |
| :--- | :--- | :--- | :--- |
| **GET** | Yes | Yes | Safe read; multiple calls return identical resource state |
| **HEAD** | Yes | Yes | Identical to GET without response body |
| **PUT** | Yes | No | State replacement; `PUT /users/1 { "name": "Alice" }` produces identical state |
| **DELETE** | Yes | No | Deleting resource 1 produces same outcome (resource is absent) |
| **POST** | **No** | No | Appends new entity; requires explicit idempotency mechanism |
| **PATCH** | **No** | No | Incremental mutations (e.g. `append to list`) are not naturally idempotent |

### 2.2 Idempotency Key Lifecycle & State Machine

```
              Client Request + Idempotency-Key: <UUID>
                                |
                                v
               Check key in Deduplication Table
                /               |              \
               /                |               \
          Not Found          Found &         Found &
              |              PENDING        COMPLETED
              v                 |               |
       Insert PENDING           v               v
       (Unique Key Lock)   Return 409     Return Cached
              |           or Wait on Lock    Response
              v
       Execute Business Logic
              |
       Update record:
       Status = COMPLETED
       Cached Response = {...}
              |
              v
       Return 200 OK
```

---

## 3. How It Works Internally

### 3.1 Relational Database Deduplication Schema

To guarantee atomicity across clustered JVM instances without distributed lock manager overhead, rely on database unique constraints:

```sql
CREATE TABLE idempotency_keys (
    idempotency_key VARCHAR(128) NOT NULL,
    user_id         VARCHAR(64)  NOT NULL,
    request_hash    VARCHAR(64)  NOT NULL, -- SHA-256 fingerprint of request body
    status          VARCHAR(32)  NOT NULL, -- PENDING, COMPLETED, FAILED
    response_code   INT          NULL,
    response_body   TEXT         NULL,
    created_at      TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    locked_until    TIMESTAMP WITH TIME ZONE NOT NULL,
    PRIMARY KEY (idempotency_key, user_id)
);
```

### 3.2 Request Fingerprinting Defense (Payload Tampering)

An attacker or buggy client might reuse an old `Idempotency-Key` with a completely different request body (e.g., changing charge amount from $10 to $1,000,000).
- The server computes the **SHA-256 hash** of the incoming payload.
- If the incoming hash differs from the stored hash for that key, the server rejects the request immediately with `HTTP 422 Unprocessable Entity` or `HTTP 400 Bad Request`.

---

## 4. Syntax & Usage

### 4.1 Idempotency Key HTTP Header Conventions

Standard enterprise APIs (Stripe, Adyen, AWS) use:
```http
POST /v1/charges HTTP/1.1
Host: api.example.com
Authorization: Bearer sec_live_123456
Idempotency-Key: 7b312db2-2df0-4b5c-a1e4-802c61099e2f
Content-Type: application/json

{
  "amount": 4999,
  "currency": "usd",
  "customer": "cus_99214"
}
```

---

## 5. Practical Examples

### 5.1 Basic Example: Thread-Safe In-Memory Idempotency Interceptor

```java
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.time.Duration;
import java.time.Instant;
import java.util.HexFormat;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReentrantLock;
import java.util.function.Supplier;

public final class InMemoryIdempotencyService {
    public enum Status { PENDING, COMPLETED, FAILED }

    public record CachedResponse(int statusCode, String body) {}
    private record IdempotencyRecord(String requestHash, Status status, CachedResponse response, Instant expiresAt) {}

    private final Map<String, IdempotencyRecord> store = new ConcurrentHashMap<>();
    private final Map<String, ReentrantLock> locks = new ConcurrentHashMap<>();
    private final Duration ttl = Duration.ofHours(24);

    public static String computeHash(String payload) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] hash = digest.digest(payload.getBytes(StandardCharsets.UTF_8));
            return HexFormat.of().formatHex(hash);
        } catch (Exception e) {
            throw new RuntimeException("SHA-256 not available", e);
        }
    }

    public CachedResponse process(String key, String payload, Supplier<CachedResponse> businessExecution) {
        String hash = computeHash(payload);
        ReentrantLock lock = locks.computeIfAbsent(key, k -> new ReentrantLock());
        lock.lock();
        try {
            IdempotencyRecord record = store.get(key);
            if (record != null) {
                if (!record.requestHash().equals(hash)) {
                    return new CachedResponse(422, "{\"error\":\"Idempotency key reused with mismatched payload\"}");
                }
                if (record.status() == Status.PENDING) {
                    return new CachedResponse(409, "{\"error\":\"Concurrent operation in progress for this idempotency key\"}");
                }
                if (record.status() == Status.COMPLETED) {
                    return record.response(); // Replay cached response
                }
            }

            // Mark as PENDING
            store.put(key, new IdempotencyRecord(hash, Status.PENDING, null, Instant.now().plus(ttl)));

            try {
                CachedResponse fresh = businessExecution.get();
                store.put(key, new IdempotencyRecord(hash, Status.COMPLETED, fresh, Instant.now().plus(ttl)));
                return fresh;
            } catch (Exception ex) {
                store.put(key, new IdempotencyRecord(hash, Status.FAILED, null, Instant.now().plus(ttl)));
                throw ex;
            }
        } finally {
            lock.unlock();
        }
    }
}
```

### 5.2 Real-World Example: Transactional Database Idempotency Guard with JDBC

```java
import java.sql.*;
import java.time.Instant;
import java.util.Objects;

public final class JdbcIdempotencyGuard {
    private final Connection connection;

    public JdbcIdempotencyGuard(Connection connection) {
        this.connection = Objects.requireNonNull(connection);
    }

    public record ExecutionResult(boolean isNew, int code, String response) {}

    public ExecutionResult acquireLockOrGetCached(String key, String userId, String payloadHash, int lockTimeoutSeconds) throws SQLException {
        String insertSql = "INSERT INTO idempotency_keys (idempotency_key, user_id, request_hash, status, locked_until) " +
            "VALUES (?, ?, ?, 'PENDING', ?) ON CONFLICT (idempotency_key, user_id) DO NOTHING";

        Instant lockExpiry = Instant.now().plusSeconds(lockTimeoutSeconds);

        try (PreparedStatement stmt = connection.prepareStatement(insertSql)) {
            stmt.setString(1, key);
            stmt.setString(2, userId);
            stmt.setString(3, payloadHash);
            stmt.setTimestamp(4, Timestamp.from(lockExpiry));
            int rows = stmt.executeUpdate();

            if (rows > 0) {
                return new ExecutionResult(true, 0, null);
            }
        }

        String selectSql = "SELECT request_hash, status, response_code, response_body, locked_until FROM idempotency_keys WHERE idempotency_key = ? AND user_id = ?";
        try (PreparedStatement stmt = connection.prepareStatement(selectSql)) {
            stmt.setString(1, key);
            stmt.setString(2, userId);
            try (ResultSet rs = stmt.executeQuery()) {
                if (rs.next()) {
                    String existingHash = rs.getString("request_hash");
                    if (!Objects.equals(existingHash, payloadHash)) {
                        return new ExecutionResult(false, 422, "{\"error\":\"Payload hash mismatch for idempotency key\"}");
                    }
                    String status = rs.getString("status");
                    if ("COMPLETED".equals(status)) {
                        return new ExecutionResult(false, rs.getInt("response_code"), rs.getString("response_body"));
                    }
                    if ("PENDING".equals(status)) {
                        Timestamp lockedUntil = rs.getTimestamp("locked_until");
                        if (lockedUntil.toInstant().isAfter(Instant.now())) {
                            return new ExecutionResult(false, 409, "{\"error\":\"Transaction in progress\"}");
                        }
                        return new ExecutionResult(true, 0, null);
                    }
                }
            }
        }
        return new ExecutionResult(false, 500, "{\"error\":\"Unexpected idempotency state\"}");
    }

    public void complete(String key, String userId, int code, String responseBody) throws SQLException {
        String updateSql = "UPDATE idempotency_keys SET status = 'COMPLETED', response_code = ?, response_body = ? WHERE idempotency_key = ? AND user_id = ?";
        try (PreparedStatement stmt = connection.prepareStatement(updateSql)) {
            stmt.setInt(1, code);
            stmt.setString(2, responseBody);
            stmt.setString(3, key);
            stmt.setString(4, userId);
            stmt.executeUpdate();
        }
    }
}
```

### 5.3 Edge Case: Deadlock Recovery via Lock Expiration

If the application server crashes while executing the business logic (after setting status to `PENDING`), the idempotency record would stay locked forever without lock expiration.
- **Solution**: The `locked_until` column defines a timeout (e.g. 120 seconds).
- Subsequent requests checking the record detect `status == 'PENDING' && now > locked_until`, identifying that the previous process died. The new worker claims the lock and completes or restarts the operation safely.

---

## 6. Best Practices

1. **Scope Keys Per Tenant/User**: Primary key should be `(idempotency_key, user_id)`. Without scoping, Tenant A can inadvertently block or inspect Tenant B's responses by guessing their UUID.
2. **Always Fingerprint Payload**: Never rely solely on the string key. Store `SHA-256(request_body)` and verify matching payloads on replay.
3. **Persist the Complete HTTP Response**: Store HTTP status code, headers, and body. When replaying, return the exact original HTTP status (e.g. 201 Created vs 200 OK).
4. **Set Time-To-Live (TTL)**: Retain idempotency records for 24–48 hours. After 48 hours, purge records with a scheduled background cleanup job.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **No Payload Validation** | Client reuses an old key for a new transaction with different items or pricing | Compute and enforce matching SHA-256 hash |
| **Lock Abandonment on Crash** | Node crashes mid-flight, leaving key in `PENDING` indefinitely | Add `locked_until` timestamp and auto-recovery timeout |
| **Global Unscoped Keys** | Malicious users guess keys and view responses belonging to other customers | Enforce composite primary key: `(key, user_id)` |
| **Non-Transactional DB Write** | Deduplication record inserted in DB, but business transaction fails; key is now poisoned | Execute business mutation and idempotency record update inside same ACID transaction |
| **Replaying Error Responses as Success**| Transient error (e.g. DB connection blip) cached as `COMPLETED`; all future retries receive error | Mark transient errors as `FAILED` and delete key so client can retry |

---

## 8. Performance & Security Notes

### 8.1 Performance: Storage Volume Mitigation
High-volume payment platforms process tens of millions of transactions daily. Storing full JSON bodies in the primary relational database can cause table bloat.
- **Optimization**: Store lightweight metadata (`idempotency_key, hash, status, response_code`) in the database; store large response bodies in an object store (S3) or short-lived Redis cache.

### 8.2 Security: Information Disclosure Defense
If an attacker intercepts or guesses an `Idempotency-Key`, they might replay the request to read cached sensitive data (e.g., account balance, masked credit cards).
- **Rule**: Verify that the authenticated security context (`principal.getId()`) matches the user ID recorded during original key insertion.

---

## 9. Comparison / When to Use What

| Architecture | Storage Engine | Concurrency Model | Complexity | Best For |
| :--- | :--- | :--- | :--- | :--- |
| **In-Memory Map** | JVM Heap | `ConcurrentHashMap` + Locks | Low | Monoliths, single-instance background daemons |
| **Relational Database** | PostgreSQL / MySQL | `INSERT ON CONFLICT` / Unique Index | Moderate | Financial ledger, payment APIs, order placement |
| **Distributed Cache** | Redis (Redlock) | SETNX with TTL | Moderate | High-throughput low-latency API gateways |
| **Event-Driven Outbox** | Kafka / Debezium | Exactly-once event deduplication | High | Asynchronous event streaming architectures |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between At-Least-Once, At-Most-Once, and Exactly-Once delivery?</summary>
- **At-Least-Once**: Messages are guaranteed delivered, but may be duplicated upon network retries.
- **At-Most-Once**: Messages are never duplicated, but may be lost if errors occur.
- **Exactly-Once**: Delivered once and only once without duplication. In distributed systems, this is practically achieved via At-Least-Once delivery paired with Idempotent Consumer processing.
</details>

<details>
<summary>2. How do you prevent concurrent duplicate requests using the same Idempotency-Key?</summary>
By atomically setting the key state to `PENDING` using a database unique constraint (`INSERT ... ON CONFLICT DO NOTHING`) or a distributed Redis lock (`SET key token NX PX 30000`). If a second request arrives while status is `PENDING`, return HTTP `409 Conflict` or block waiting for the first request to complete.
</details>

<details>
<summary>3. Why should the server compute a SHA-256 hash of the request payload?</summary>
To guard against idempotency key reuse with modified payloads. If a client transmits `Idempotency-Key: ABC` with `$10` and later sends `Idempotency-Key: ABC` with `$500`, the server detects that the payload hash does not match the stored hash and rejects the request with HTTP 422 Unprocessable Entity.
</details>

<details>
<summary>4. How does an idempotency filter handle a server crash during request processing?</summary>
The record must include a `locked_until` timestamp. If the server crashes during execution, the key remains `PENDING`. Subsequent retries verify that the current time has surpassed `locked_until`, reclaim the lock, and safely re-execute the business operation.
</details>

<details>
<summary>5. Should failed requests (e.g. 500 Internal Server Error) be cached in the idempotency table?</summary>
No. Transient system errors should either delete the idempotency key or mark it `FAILED` so that client retries can re-execute the business logic once downstream dependencies recover.
</details>

<details>
<summary>6. Which standard HTTP methods are required by specification to be idempotent?</summary>
`GET`, `HEAD`, `PUT`, and `DELETE`. `POST` and `PATCH` are not idempotent by default and require custom application-level idempotency mechanisms.
</details>

<details>
<summary>7. What happens if a client retries a completed request after the 24-hour idempotency TTL expires?</summary>
The server treats the request as a brand new operation because the key has been evicted from the database. Clients should only retry within the documented idempotency retention window (typically 24 to 48 hours).
</details>

<details>
<summary>8. Why must idempotency keys be scoped to an authenticated user ID?</summary>
If keys are global across all tenants, Tenant A could accidentally or maliciously generate the same UUID as Tenant B, either blocking Tenant B from executing their mutation (denial of service) or inspecting Tenant B's cached response body.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Implement a SHA-256 Request Body Hash Calculator</summary>

```java
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HexFormat;

public final class RequestHasher {
    public static String hash(String body) {
        if (body == null) body = "";
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] digest = md.digest(body.getBytes(StandardCharsets.UTF_8));
            return HexFormat.of().formatHex(digest);
        } catch (Exception e) {
            throw new IllegalStateException("Crypto error", e);
        }
    }
}
```
</details>

<details>
<summary>Exercise 2: Implement an Idempotency Filter with Response Replay</summary>

```java
public class MockIdempotentOrderController {
    private final InMemoryIdempotencyService idempotencyService = new InMemoryIdempotencyService();

    public InMemoryIdempotencyService.CachedResponse createOrder(String idempotencyKey, String orderJson) {
        return idempotencyService.process(idempotencyKey, orderJson, () -> {
            System.out.println("Executing business logic: Creating order...");
            return new InMemoryIdempotencyService.CachedResponse(201, "{\"orderId\":\"ord_5521\",\"status\":\"CREATED\"}");
        });
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Definition**: $f(f(x)) = f(x)$. Multiple duplicate requests yield identical server state.
- **Key Header**: `Idempotency-Key: <UUID>` passed by client on mutating `POST` endpoints.
- **Lifecycle**: `PENDING` (Lock) -> `COMPLETED` (Cached Response) or `FAILED` (Purged for retry).
- **Security**: Hash payload (SHA-256) to prevent parameter tampering; scope keys per tenant `(key, user_id)`.
- **Concurrency**: Guard state changes with DB unique constraints or atomic distributed locks.
- **Crash Recovery**: Include `locked_until` lease expiration to prevent orphaned pending states.

---

## 13. Further Reading

- [Stripe API Reference: Idempotency](https://stripe.com/docs/api/idempotent_requests)
- [IETF Idempotency-Key HTTP Header Field Specification](https://datatracker.ietf.org/doc/draft-ietf-httpapi-idempotency-key-header/)
- [Brandur Leach: Implementing Stripe-like Idempotency Keys in Postgres](https://brandur.org/idempotency-keys)
