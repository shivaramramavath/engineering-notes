# Webhooks: Secure Dispatching, HMAC Signatures, and Resilient Delivery

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `RestClient`, `org.springframework.retry:spring-retry`, Java Cryptography Architecture (JCA) |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Polling external APIs at periodic intervals to detect state changes creates massive resource waste, latency spikes, and scalability bottlenecks. Clients waste server compute and network bandwidth asking "Has order 123 changed?", receiving 99% negative responses. Conversely, when third parties need real-time notifications about events occurring inside an enterprise platform (e.g., Stripe, Shopify, GitHub workflows), synchronous polling is unacceptable.

### The Evolution
Webhooks (reverse APIs) invert the communication model: rather than consumers continuously polling, the event producer issues an HTTP POST request to a pre-registered callback URL whenever an event occurs. However, naive webhook dispatching introduces severe security risks (spoofing, replay attacks) and reliability hazards (network timeouts, consumer downtime, cascading backpressure). Modern systems enforce HMAC cryptographic signatures, timestamp verification, and asynchronous retry queues with exponential backoff.

### Relevance in Modern Engineering
In event-driven SaaS and fintech architectures, webhooks are the primary integration mechanism. Spring Boot engineers must design both sides: securely dispatching outbound webhooks with HMAC-SHA256 signatures and dead-letter queues, and defensively consuming incoming webhooks with constant-time signature verification and idempotent processing.

---

## 2. Core Concepts

- **Webhook Dispatcher**: An asynchronous service that serializes domain events into HTTP POST requests sent to subscriber endpoint URLs.
- **HMAC SHA-256 Signatures**: A symmetric cryptographic hash (`HmacSHA256`) calculated across the request payload and timestamp using a shared secret, passed in headers (e.g., `X-Signature-SHA256` or `X-Hub-Signature-256`).
- **Replay Attack Defense**: Including an explicit timestamp header (e.g., `X-Timestamp`) and verifying that the event occurred within a tight tolerance window (typically 5 minutes).
- **Constant-Time Comparison**: Using `MessageDigest.isEqual()` to prevent side-channel timing attacks during signature verification.
- **Exponential Backoff & Dead Letter Queue (DLQ)**: Retrying failed HTTP deliveries with increasing delays (e.g., 5s, 30s, 5m, 1h) before isolating unrecoverable deliveries to a persistent DLQ.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                           Secure Webhook Dispatch Architecture                    |
|                                                                                   |
| 1. Domain Event Published                                                         |
|    PaymentCompletedEvent(paymentId, amount, status)                               |
|                   |                                                               |
|                   v                                                               |
| 2. Asynchronous Queue / Outbox Task                                               |
|    +-----------------------------+                                                |
|    | Webhook Dispatch Worker     | ---> Retrieves Subscriber Target URL & Secret  |
|    +-----------------------------+                                                |
|                   |                                                               |
|                   v                                                               |
| 3. Cryptographic Signature Generation                                             |
|    Payload: '{"event":"payment.completed","id":"tx_123"}'                        |
|    Timestamp: 1716982000                                                          |
|    HMAC-SHA256(Secret, Timestamp + "." + Payload) = "a4b7...c9f2"                |
|                   |                                                               |
|                   v                                                               |
| 4. Outbound HTTP Request via RestClient                                           |
|    POST https://consumer.example.com/webhooks                                     |
|    Headers:                                                                       |
|      X-Webhook-Timestamp: 1716982000                                              |
|      X-Webhook-Signature: t=1716982000,v1=a4b7...c9f2                            |
|                   |                                                               |
|      +------------+------------+                                                  |
|      | 200 OK                  | 500 / Network Error                              |
|      v                         v                                                  |
|   Mark Delivery Done        Schedule Retry (Exponential Backoff)                  |
|                             Max Retries Exceeded -> Send to DLQ                   |
+-----------------------------------------------------------------------------------+
```

When a business transaction commits, an outbox table or message broker emits a webhook task. The dispatcher serializes the payload to canonical JSON, captures the current UNIX epoch timestamp, and passes the concatenated string (`timestamp + "." + json`) through `javax.crypto.Mac` initialized with `HmacSHA256`. The outbound request is dispatched via `RestClient`. If the target server returns non-2xx or times out, the task re-queues with an exponential delay multiplier.

---

## 4. Syntax & Usage

### Maven Dependencies
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-web</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.retry</groupId>
    <artifactId>spring-retry</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-aop</artifactId>
</dependency>
```

---

## 5. Practical Examples

### Example 1: Cryptographic HMAC-SHA256 Signature Generator
```java
package com.example.platform.webhook.security;

import org.springframework.stereotype.Component;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;

@Component
public class WebhookSigner {

    private static final String HMAC_SHA256 = "HmacSHA256";

    public String generateSignature(String payload, long timestamp, String secret) {
        try {
            String signingContent = timestamp + "." + payload;
            Mac mac = Mac.getInstance(HMAC_SHA256);
            SecretKeySpec secretKeySpec = new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), HMAC_SHA256);
            mac.init(secretKeySpec);
            byte[] rawHmac = mac.doFinal(signingContent.getBytes(StandardCharsets.UTF_8));
            return HexFormat.of().formatHex(rawHmac);
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            throw new IllegalStateException("Failed to calculate HMAC-SHA256 signature", e);
        }
    }
}
```

### Example 2: Outbound Webhook Dispatcher with Spring RestClient
```java
package com.example.platform.webhook.dispatcher;

import com.example.platform.webhook.security.WebhookSigner;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;

import java.time.Duration;
import java.time.Instant;

@Service
public class WebhookDispatcher {

    private static final Logger log = LoggerFactory.getLogger(WebhookDispatcher.class);
    private final RestClient restClient;
    private final WebhookSigner webhookSigner;

    public WebhookDispatcher(RestClient.Builder restClientBuilder, WebhookSigner webhookSigner) {
        this.restClient = restClientBuilder
                .connectTimeout(Duration.ofSeconds(3))
                .readTimeout(Duration.ofSeconds(5))
                .build();
        this.webhookSigner = webhookSigner;
    }

    public boolean dispatch(String targetUrl, String payload, String secret) {
        long timestamp = Instant.now().getEpochSecond();
        String signature = webhookSigner.generateSignature(payload, timestamp, secret);
        String headerValue = String.format("t=%d,v1=%s", timestamp, signature);

        try {
            var response = restClient.post()
                    .uri(targetUrl)
                    .contentType(MediaType.APPLICATION_JSON)
                    .header("X-Webhook-Signature", headerValue)
                    .header("X-Webhook-Timestamp", String.valueOf(timestamp))
                    .body(payload)
                    .retrieve()
                    .toBodilessEntity();

            log.info("Webhook successfully delivered to {}: status {}", targetUrl, response.getStatusCode());
            return true;
        } catch (Exception e) {
            log.warn("Webhook delivery failed to {}: {}", targetUrl, e.getMessage());
            return false;
        }
    }
}
```

### Example 3: Resilient Retry Service with Exponential Backoff
```java
package com.example.platform.webhook.service;

import com.example.platform.webhook.dispatcher.WebhookDispatcher;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.retry.annotation.Backoff;
import org.springframework.retry.annotation.Recover;
import org.springframework.retry.annotation.Retryable;
import org.springframework.stereotype.Service;

@Service
public class ResilientWebhookService {

    private static final Logger log = LoggerFactory.getLogger(ResilientWebhookService.class);
    private final WebhookDispatcher dispatcher;

    public ResilientWebhookService(WebhookDispatcher dispatcher) {
        this.dispatcher = dispatcher;
    }

    @Retryable(
            retryFor = { RuntimeException.class },
            maxAttempts = 4,
            backoff = @Backoff(delay = 1000, multiplier = 3.0, maxDelay = 30000)
    )
    public void sendWebhookWithRetry(String targetUrl, String payload, String secret) {
        boolean success = dispatcher.dispatch(targetUrl, payload, secret);
        if (!success) {
            throw new RuntimeException("Target endpoint failed to acknowledge webhook with 2xx status");
        }
    }

    @Recover
    public void recover(RuntimeException e, String targetUrl, String payload, String secret) {
        log.error("Exhausted all retry attempts for target {}. Moving payload to Dead Letter Queue (DLQ). Error: {}",
                targetUrl, e.getMessage());
        // Persist to DLQ table or Kafka DLQ topic for manual inspection
    }
}
```

### Example 4: Timing Attack in Signature Verification (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Using standard String.equals() which terminates on the first mismatched byte,
// allowing attackers to deduce valid signatures byte-by-byte using microsecond timing differences.
// boolean isValid = expectedSignature.equals(providedSignature);

// FIX: Use constant-time array comparison via java.security.MessageDigest.isEqual()
package com.example.platform.webhook.security;

import org.springframework.stereotype.Component;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

@Component
public class ConstantTimeSignatureVerifier {

    public boolean verify(String calculatedSignature, String receivedSignature) {
        if (calculatedSignature == null || receivedSignature == null) {
            return false;
        }
        byte[] expectedBytes = calculatedSignature.getBytes(StandardCharsets.UTF_8);
        byte[] receivedBytes = receivedSignature.getBytes(StandardCharsets.UTF_8);
        
        // Constant-time comparison defends against side-channel timing analysis
        return MessageDigest.isEqual(expectedBytes, receivedBytes);
    }
}
```

### Example 5: High-Performance Incoming Webhook Receiver Controller
```java
package com.example.platform.webhook.receiver;

import com.example.platform.webhook.security.ConstantTimeSignatureVerifier;
import com.example.platform.webhook.security.WebhookSigner;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;

@RestController
@RequestMapping("/api/v1/incoming-webhooks")
public class WebhookReceiverController {

    private final WebhookSigner signer;
    private final ConstantTimeSignatureVerifier verifier;
    private final String webhookSecret;
    private static final long TOLERANCE_SECONDS = 300; // 5 minutes

    public WebhookReceiverController(
            WebhookSigner signer,
            ConstantTimeSignatureVerifier verifier,
            @Value("${webhook.secret}") String webhookSecret) {
        this.signer = signer;
        this.verifier = verifier;
        this.webhookSecret = webhookSecret;
    }

    @PostMapping
    public ResponseEntity<Void> receiveWebhook(
            @RequestHeader("X-Webhook-Timestamp") long timestamp,
            @RequestHeader("X-Webhook-Signature") String signatureHeader,
            @RequestBody String rawPayload) {

        // 1. Guard against Replay Attacks
        long currentSeconds = Instant.now().getEpochSecond();
        if (Math.abs(currentSeconds - timestamp) > TOLERANCE_SECONDS) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).build();
        }

        // 2. Extract v1 signature
        String receivedSig = parseSignature(signatureHeader);

        // 3. Re-compute HMAC and compare in constant time
        String expectedSig = signer.generateSignature(rawPayload, timestamp, webhookSecret);
        if (!verifier.verify(expectedSig, receivedSig)) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).build();
        }

        // 4. Acknowledge receipt immediately; process payload asynchronously
        processPayloadAsync(rawPayload);

        return ResponseEntity.ok().build();
    }

    private String parseSignature(String header) {
        for (String part : header.split(",")) {
            if (part.startsWith("v1=")) {
                return part.substring(3);
            }
        }
        return "";
    }

    private void processPayloadAsync(String payload) {
        // Enqueue into internal work queue
    }
}
```

---

## 6. Best Practices

1. **Sign Over Raw Payload Strings**: Never re-serialize parsed JSON objects to calculate HMAC; formatting, whitespace, and key sorting differences will invalidate the signature.
2. **Enforce Strict Connection and Read Timeouts**: Configure outbound HTTP clients with tight timeouts (e.g., 3s connect, 5s read) to prevent misbehaving consumer servers from exhausting thread pools.
3. **Always Include Timestamps**: Require a UNIX epoch timestamp in header signatures to neutralize replay attacks.
4. **Respond with HTTP 2xx Immediately**: Consumers must acknowledge receipt within 1-2 seconds and offload heavy processing to internal background workers.
5. **Implement Idempotency Keys**: Embed a unique `webhook_id` or event UUID in headers or payload so consumers can safely discard duplicate webhook deliveries.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Re-serializing parsed DTOs to calculate HMAC | JSON key reordering or whitespace alterations invalidate the cryptographic hash | Hash the exact raw byte array or raw String received from the HTTP body. |
| Using `String.equals()` for signature verification | Enables side-channel timing attacks to crack cryptographic signatures | Compare byte arrays using `MessageDigest.isEqual()`. |
| Synchronous webhook dispatching inside database transactions | Slow or unresponsive subscriber hosts block DB connections and thread pools | Dispatch webhooks asynchronously via an Outbox worker or message queue. |
| Missing replay attack protection | Attackers intercept and replay valid past requests (e.g., re-triggering fulfillment) | Validate that `X-Webhook-Timestamp` is within +/- 5 minutes of current clock. |
| Endless retry loops on HTTP 400 Bad Request | Semantic errors will never succeed on retry, congesting retry queues | Only retry transient 5xx server errors and network connection timeouts. |
| Blocking webhook consumer with heavy database processing | Producer times out and issues duplicate webhooks | Enqueue the payload into Redis/Kafka and return HTTP 200 immediately. |

---

## 8. Performance & Security Notes

- **Secret Rotation**: Support multiple active secrets (e.g., `v1` and `v2` signatures in the same header) to allow seamless customer secret rotation without downtime.
- **SSRF (Server-Side Request Forgery) Defense**: Validate subscriber URLs against internal IP ranges (127.0.0.1, 10.0.0.0/8, 169.254.169.254) to prevent attackers from using webhooks to probe internal cloud infrastructure.
- **Virtual Threads for Dispatch**: Running webhook HTTP requests on Java 21 Virtual Threads (`Executors.newVirtualThreadPerTaskExecutor()`) allows thousands of concurrent outbound deliveries with minimal OS memory overhead.

---

## 9. Comparison / When to Use What

| Pattern | Webhooks (HTTP Push) | Polling (HTTP Pull) | WebSockets (Full Duplex) |
|---|---|---|---|
| **Initiator** | Server / Producer | Client / Consumer | Bidirectional |
| **Network Overhead** | Low (sent only on event) | High (constant requests) | Low (persistent TCP) |
| **Firewall Traversal** | Requires public endpoint | Works behind NAT/firewall | Works through standard proxies |
| **Best For** | Server-to-server notifications | Legacy clients without public URLs | Interactive browser UI updates |

---

## 10. Interview Questions

<details>
<summary>1. How does HMAC-SHA256 ensure the authenticity and integrity of a webhook?</summary>
HMAC uses a shared secret known only to the sender and receiver. Because the hash is computed over both the secret and the message payload, an attacker cannot forge the signature without knowing the secret, and cannot tamper with the payload without altering the resulting hash.
</details>

<details>
<summary>2. Why is MessageDigest.isEqual() required when verifying webhook signatures?</summary>
Standard `String.equals()` terminates immediately upon encountering the first non-matching character. Attackers can measure the response time in microseconds to guess the signature character-by-character. `MessageDigest.isEqual()` runs in constant time regardless of where mismatches occur.
</details>

<details>
<summary>3. What is a webhook replay attack and how is it mitigated?</summary>
In a replay attack, an attacker intercepts a legitimate webhook request and resends it to the target endpoint. It is mitigated by transmitting a UNIX timestamp in the header signed inside the HMAC, and rejecting any request whose timestamp differs from the server clock by more than a predefined tolerance (e.g., 5 minutes).
</details>

<details>
<summary>4. Why should webhooks never be dispatched synchronously inside a database transaction?</summary>
If the external recipient's server is down, slow, or hanging, the database transaction remains open, holding locks on database rows and exhausting the database connection pool, which can bring down the entire application.
</details>

<details>
<summary>5. How should consumers handle duplicate webhook deliveries?</summary>
Consumers must design idempotent webhook receivers. Each webhook should contain a unique event UUID. The receiver checks an idempotency store (e.g., Redis or database unique constraint); if already processed, it returns HTTP 200 immediately without re-executing business logic.
</details>

<details>
<summary>6. What HTTP status codes should trigger automatic retries in a webhook dispatcher?</summary>
Retries should only be triggered for transient failures: network timeouts, connection resets, and HTTP 5xx responses (500, 502, 503, 504), as well as HTTP 429 (Too Many Requests). Client errors like 400 Bad Request, 401 Unauthorized, and 404 Not Found indicate persistent faults and should not be retried.
</details>

<details>
<summary>7. What is SSRF in the context of webhook dispatching?</summary>
Server-Side Request Forgery occurs when an attacker registers a malicious webhook URL pointing to internal private IP addresses (e.g., `http://169.254.169.254/latest/meta-data/` or `http://localhost:8080/admin`). The dispatcher must resolve DNS and reject private/loopback IP addresses before opening HTTP connections.
</details>

<details>
<summary>8. How does the Transactional Outbox pattern improve webhook reliability?</summary>
Instead of dispatching webhooks directly from the service layer, the application inserts an event record into a database outbox table within the same transaction as the business entity changes. A separate background worker reads the outbox table and dispatches webhooks, guaranteeing at-least-once delivery even if the application crashes.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Calculate HMAC-SHA256 in Java</summary>
Write a method that takes a String secret and String payload and returns the hex-encoded HMAC-SHA256 signature using `javax.crypto.Mac`.
</details>

<details>
<summary>Exercise 2 (Easy): Verify timestamp window</summary>
Write a method `boolean isTimestampValid(long timestampSeconds, long maxSkewSeconds)` that checks if a timestamp is within the allowable skew window.
</details>

<details>
<summary>Exercise 3 (Medium): Build a constant-time signature verifier filter</summary>
Implement a Spring MVC `HandlerInterceptor` that intercepts requests to `/webhooks/**`, validates the `X-Signature` header against the request body, and aborts with 401 if invalid.
</details>

<details>
<summary>Exercise 4 (Medium): Configure Spring Retry with exponential backoff</summary>
Configure `@EnableRetry` and a `@Retryable` method with initial delay of 2 seconds, multiplier of 2.0, max attempts of 5, and a `@Recover` fallback method.
</details>

<details>
<summary>Exercise 5 (Hard): Implement an Outbox Webhook Dispatcher with Virtual Threads</summary>
Design a worker polling an outbox repository every 2 seconds, dispatching pending webhook notifications concurrently using Java 21 Virtual Threads and logging delivery outcomes.
</details>

<details>
<summary>Exercise 6 (Hard): SSRF IP Validation Guard</summary>
Implement a custom `UriTemplateHandler` or IP validator that resolves DNS for destination webhook URLs and rejects loopback, link-local, and RFC 1918 private IPv4/IPv6 address ranges.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Secure Webhook Architecture                             |
+---------------------------------------------------------------------------------+
| Cryptography:       HmacSHA256(Secret, Timestamp + "." + RawJsonPayload)        |
| Verification:       MessageDigest.isEqual(expected, received) [Constant-Time]  |
| Anti-Replay:        |current_time - header_timestamp| < 300 seconds             |
| Header Format:      X-Webhook-Signature: t=1716982000,v1=9f8c...3a             |
| Client Config:      Connect timeout 3s, Read timeout 5s                         |
| Delivery Pattern:   Outbox table -> Async Worker -> RestClient -> DLQ           |
| Consumer Rule:      Return HTTP 200 within 2s, process asynchronously           |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Standard Webhooks Specification](https://www.standardwebhooks.com/)
- [Stripe Webhook Signatures Guide](https://docs.stripe.com/webhooks/signatures)
- [RFC 2104: HMAC Keyed-Hashing for Message Authentication](https://datatracker.ietf.org/doc/html/rfc2104)
