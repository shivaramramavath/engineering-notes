# 04: Designing a Multi-Channel Distributed Notification System

| Metadata Field | Value |
|---|---|
| **Topic** | Enterprise Multi-Channel Notification Architecture |
| **Spring Boot Version** | 3.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Scale** | 100 Million Notifications / Day (Email, SMS, Push, In-App) |
| **Key Middleware** | Apache Kafka (Priority Topics), Redis (Rate Limits & Idempotency), PostgreSQL, Third-Party Providers |

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

A distributed notification platform is responsible for delivering real-time, transactional, and marketing alerts across multiple communication channels: Mobile Push (APNs, FCM), SMS (Twilio, AWS SNS), Email (SendGrid, AWS SES), and In-App WebSockets. 

At a scale of 100 million messages per day, the primary architectural challenges include:
- **Deduplication & Idempotency**: Ensuring transient network retries never spam users with duplicate OTPs or billing receipts.
- **Priority Tiering**: Guaranteeing that critical Two-Factor Authentication (2FA) SMS messages are dispatched within 2 seconds, while marketing email campaigns consume background bandwidth without choking the system.
- **Provider Failover & Rate Limiting**: Handling downstream vendor rate limits and transient outages gracefully using Circuit Breakers and Dead Letter Queues.

---

## 2. Core Concepts

### 2.1 Priority Queue Partitioning
Rather than dumping all notifications into a single FIFO queue, traffic is segmented into prioritized Kafka topics:
1. `notifications.critical`: OTPs, fraud alerts, password resets (Low latency, high concurrency workers).
2. `notifications.transactional`: Order updates, payment confirmations, shipment notices.
3. `notifications.promotional`: Marketing blasts, weekly digests (Rate-limited, bulk workers).

### 2.2 Token Bucket Rate Limiting per User
To prevent spamming end-users or violating telecom regulations (e.g., maximum 5 marketing SMS per user per day), a distributed Redis Token Bucket or Sliding Window checks user preferences before dispatch.

---

## 3. Internals & Architecture

```
       [ Microservice Clients (Order, Auth, Billing) ]
                             │
                  (POST /api/v1/notifications)
                             ▼
  ┌─────────────────────────────────────────────────────────────┐
  │                 Notification Ingestion API                  │
  │  1. Validate Schema & User Opt-Out Preferences              │
  │  2. Redis Idempotency Check: "seen:notif:{dedupKey}"        │
  │  3. Route to Priority Kafka Topic                           │
  └──────────────────────────┬──────────────────────────────────┘
                             │
     ┌───────────────────────┼────────────────────────┐
     ▼                       ▼                        ▼
┌──────────────────┐ ┌───────────────────┐ ┌───────────────────┐
│ Kafka: Critical  │ │ Kafka: Transaction│ │ Kafka: Promotional│
│ (Partitioned)    │ │ (Partitioned)     │ │ (Partitioned)     │
└────────┬─────────┘ └─────────┬─────────┘ └─────────┬─────────┘
         │                     │                     │
         ▼                     ▼                     ▼
┌──────────────────────────────────────────────────────────────┐
│             Notification Dispatcher Workers                  │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Channel Routers: Push / SMS / Email / In-App           │  │
│  └────────┬───────────────────┬───────────────────┬───────┘  │
│           ▼                   ▼                   ▼          │
│    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│    │ FCM / APNs   │    │ Twilio / SNS │    │ SendGrid/SES │  │
│    │ (Push)       │    │ (SMS)        │    │ (Email)      │  │
│    └──────┬───────┘    └──────┬───────┘    └──────┬───────┘  │
│           │ (Failure)         │ (Failure)         │ (Failure)│
│           └─────────────┬─────┴───────────────────┘          │
│                         ▼                                    │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ Dead Letter Topic (DLT) + Exponential Backoff Retry    │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────┬───────────────────────────────┘
                               ▼
               ┌───────────────────────────────┐
               │ PostgreSQL / ScyllaDB History │
               │ - delivery_logs, audit_trail  │
               └───────────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Capacity & Bandwidth Estimates
$$\text{Average Ingestion QPS} = \frac{100,000,000\text{ messages}}{86,400\text{ seconds}} \approx 1,157\text{ QPS}$$
$$\text{Peak Ingestion QPS (Black Friday / Promo Launch)} = 1,157 \times 5 \approx 6,000\text{ QPS}$$
$$\text{Daily Audit Storage} = 100\text{M} \times 1\text{ KB} = 100\text{ GB / day} \approx 36.5\text{ TB / year}$$

---

## 5. 5 Practical Examples

### Example 1: Notification Ingestion Controller with Idempotency

```java
package com.example.notification.web;

import com.example.notification.dto.NotificationRequest;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.web.bind.annotation.*;

import java.time.Duration;

@RestController
@RequestMapping("/api/v1/notifications")
public class NotificationIngestionController {

    private static final Logger log = LoggerFactory.getLogger(NotificationIngestionController.class);
    private final KafkaTemplate<String, Object> kafkaTemplate;
    private final StringRedisTemplate redisTemplate;

    public NotificationIngestionController(KafkaTemplate<String, Object> kafkaTemplate, StringRedisTemplate redisTemplate) {
        this.kafkaTemplate = kafkaTemplate;
        this.redisTemplate = redisTemplate;
    }

    @PostMapping
    public ResponseEntity<String> enqueueNotification(@RequestBody NotificationRequest request) {
        String dedupKey = "notif:dedup:" + request.idempotencyKey();
        Boolean isNew = redisTemplate.opsForValue().setIfAbsent(dedupKey, "QUEUED", Duration.ofHours(24));

        if (Boolean.FALSE.equals(isNew)) {
            log.info("Duplicate notification {} ignored.", request.idempotencyKey());
            return ResponseEntity.status(HttpStatus.ACCEPTED).body("Notification already queued");
        }

        String targetTopic = switch (request.priority()) {
            case CRITICAL -> "notifications.critical";
            case TRANSACTIONAL -> "notifications.transactional";
            case PROMOTIONAL -> "notifications.promotional";
        };

        kafkaTemplate.send(targetTopic, request.recipientId(), request);
        return ResponseEntity.status(HttpStatus.ACCEPTED).body("Queued for delivery");
    }
}
```

### Example 2: Polymorphic Channel Dispatcher Architecture

```java
package com.example.notification.dispatcher;

import com.example.notification.dto.ChannelType;
import com.example.notification.dto.NotificationMessage;

public interface NotificationChannelDispatcher {
    ChannelType supportsChannel();
    void dispatch(NotificationMessage message);
}
```

```java
package com.example.notification.dispatcher;

import com.example.notification.dto.ChannelType;
import com.example.notification.dto.NotificationMessage;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

@Component
public class SmsChannelDispatcher implements NotificationChannelDispatcher {

    private static final Logger log = LoggerFactory.getLogger(SmsChannelDispatcher.class);

    @Override
    public ChannelType supportsChannel() {
        return ChannelType.SMS;
    }

    @Override
    public void dispatch(NotificationMessage message) {
        log.info("Dispatching SMS via Telecom Provider to {}: {}", message.recipientPhone(), message.body());
        // Execute third-party Twilio / AWS SNS SDK call
    }
}
```

### Example 3: Kafka Multi-Worker Consumer with Exponential Backoff

```java
package com.example.notification.listener;

import com.example.notification.dispatcher.NotificationChannelDispatcher;
import com.example.notification.dto.ChannelType;
import com.example.notification.dto.NotificationMessage;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;

import java.util.List;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Collectors;

@Component
public class NotificationEventListener {

    private static final Logger log = LoggerFactory.getLogger(NotificationEventListener.class);
    private final Map<ChannelType, NotificationChannelDispatcher> dispatchers;

    public NotificationEventListener(List<NotificationChannelDispatcher> dispatcherList) {
        this.dispatchers = dispatcherList.stream()
                .collect(Collectors.toMap(NotificationChannelDispatcher::supportsChannel, Function.identity()));
    }

    @KafkaListener(
            topics = "notifications.critical",
            groupId = "critical-notification-workers",
            concurrency = "5"
    )
    public void onCriticalNotification(NotificationMessage message) {
        log.info("Processing critical notification for recipient: {}", message.recipientId());
        NotificationChannelDispatcher dispatcher = dispatchers.get(message.channel());
        if (dispatcher != null) {
            dispatcher.dispatch(message);
        } else {
            log.error("Unsupported channel: {}", message.channel());
        }
    }
}
```

### Example 4: User Notification Preference Filter

```java
package com.example.notification.service;

import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;

import java.time.Duration;

@Service
public class RateLimitAndPreferenceService {

    private final StringRedisTemplate redis;

    public RateLimitAndPreferenceService(StringRedisTemplate redis) {
        this.redis = redis;
    }

    public boolean canReceiveMarketingNotification(String userId) {
        String key = "limits:user:marketing:" + userId;
        Long count = redis.opsForValue().increment(key);
        if (count != null && count == 1) {
            redis.expire(key, Duration.ofDays(1));
        }
        // Limit to at most 3 promotional messages per day
        return count != null && count <= 3;
    }
}
```

### Example 5: Spring Kafka Dead Letter Publishing Recoverer Configuration

```java
package com.example.notification.config;

import org.apache.kafka.common.TopicPartition;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.kafka.listener.DeadLetterPublishingRecoverer;
import org.springframework.kafka.listener.DefaultErrorHandler;
import org.springframework.util.backoff.ExponentialBackOff;

@Configuration
public class KafkaRetryConfig {

    @Bean
    public DefaultErrorHandler kafkaErrorHandler(KafkaTemplate<String, Object> template) {
        // Forward failed deliveries to '.DLT' topic
        DeadLetterPublishingRecoverer recoverer = new DeadLetterPublishingRecoverer(template,
                (record, ex) -> new TopicPartition(record.topic() + ".DLT", record.partition()));

        ExponentialBackOff backOff = new ExponentialBackOff(1000L, 2.0);
        backOff.setMaxInterval(10000L);
        backOff.setMaxElapsedTime(30000L); // Retry for up to 30 seconds

        return new DefaultErrorHandler(recoverer, backOff);
    }
}
```

---

## 6. Best Practices

- **Enforce Strict Idempotency**: Mobile networks frequently duplicate webhook receipts. Every notification must have a deterministic `idempotencyKey` (e.g. `order-shipped-12345`).
- **Use Multi-Topic Priority Routing**: Never let marketing newsletters delay one-time passwords (OTPs). Run dedicated consumer pools on `notifications.critical`.
- **Implement Multi-Vendor Active-Passive Fallbacks**: If Twilio returns 5xx errors, automatically failover SMS dispatching to AWS SNS or Infobip via Resilience4j.
- **Honor User Notification Frequency Settings**: Maintain user opt-in flags and frequency counters in Redis before queuing downstream messages.
- **Store Templated Content, Not Static Strings**: Store template IDs (e.g., `TEMPLATE_WELCOME_EMAIL`) and render placeholders at delivery time to reflect updated data.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Single FIFO Queue for All Alerts** | A 500,000-user marketing blast blocks 2FA authentication SMS messages, causing login outages. | Segregate into distinct priority Kafka topics with independent consumer thread pools. |
| **Synchronous Ingestion Processing** | The API endpoint waits for third-party email providers (500ms latency), exhausting Tomcat threads. | Acknowledge HTTP 202 Accepted immediately upon writing to Kafka; dispatch asynchronously. |
| **No DLQ (Dead Letter Queue)** | Malformed email addresses trigger infinite retry loops, locking Kafka consumer partitions. | Use `DeadLetterPublishingRecoverer` with a max retry count of 3. |
| **Sending Unredacted PII in Kafka** | Logging cleartext credit cards or passwords in event payloads violates GDPR and PCI-DSS. | Mask sensitive information before publishing notification events. |
| **No Vendor Rate Limit Throttling** | Sending 10,000 req/sec to SendGrid triggers HTTP 429 quota bans and message drops. | Apply client-side rate limiters (Resilience4j RateLimiter / Token Bucket). |
| **Missing Unsubscribe Links** | Promotional emails without mandatory one-click unsubscribe headers violate CAN-SPAM. | Inject RFC 8058 `List-Unsubscribe` headers in all promotional email payloads. |

---

## 8. Performance & Security Considerations

### 8.1 Kafka Partition Sizing
For 6,000 peak QPS across the critical notifications stream:
$$\text{Partitions} = \frac{\text{Target QPS}}{\text{Consumer Throughput per Partition}} = \frac{6,000}{500} = 12\text{ Partitions}$$
Ensure consumers are pinned to virtual threads for lightweight, non-blocking HTTP dispatching.

### 8.2 PII Protection & Data Retention
Store audit logs in cold storage (S3 / Parquet) with automatic 90-day lifecycle expiry to adhere to privacy regulations.

---

## 9. Comparison / Decision Matrix

| Channel | Typical Latency | Cost per 1k | Deliverability Rate | Primary Use Case |
|---|---|---|---|---|
| **Push (FCM/APNs)** | $< 1\text{ sec}$ | Free | Moderate (User can disable) | Breaking news, in-app alerts |
| **SMS (Twilio/SNS)** | $1-3\text{ sec}$ | High (\$10-\$50) | Very High (98%+) | 2FA OTP, fraud alerts |
| **Email (SendGrid)** | $5-30\text{ sec}$ | Low (\$0.50-\$1) | High (Subject to spam filters) | Receipts, invoices, newsletters |
| **In-App (WebSocket)**| $< 100\text{ms}$ | Free | High (Online users only) | Realtime chat, badge counts |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How do you prevent users from receiving duplicate notifications?</strong></summary>

Enforce client-supplied or event-derived idempotency keys. On ingestion, perform an atomic Redis `SETNX` on `notif:dedup:{key}` with a 24-hour TTL. If the key exists, return HTTP 202 without queuing a second Kafka message.
</details>

<details>
<summary><strong>2. How do you ensure OTPs are not delayed by a million promotional emails?</strong></summary>

Use separate Kafka topics: `notifications.critical` and `notifications.promotional`. Assign dedicated, high-priority consumer pods to the critical topic, while throttling promotional consumers to respect downstream email provider quotas.
</details>

<details>
<summary><strong>3. What happens if a third-party SMS gateway goes down?</strong></summary>

Wrap the provider client in a Resilience4j Circuit Breaker. If failure rates exceed 50%, the circuit trips open, and requests automatically route to a secondary backup SMS provider (e.g., failover from Twilio to AWS SNS).
</details>

<details>
<summary><strong>4. How do you handle user time-zone quiet hours (e.g., no promotional SMS at 3 AM)?</strong></summary>

Inspect the recipient's timezone profile. If current local time falls between 10 PM and 8 AM, delay the event by scheduling it into a delayed Redis sorted set or Kafka delayed topic to fire at 8:01 AM local time.
</details>

<details>
<summary><strong>5. Why is Kafka preferred over RabbitMQ for a 100M/day notification system?</strong></summary>

Kafka provides partitioned, append-only distributed logs capable of millions of writes per second, horizontal scaling, consumer offset tracking, and event replayability in the event of consumer service crashes.
</details>

<details>
<summary><strong>6. How do you track delivery status callbacks from email providers?</strong></summary>

Expose an authenticated Webhook ingestion endpoint. Providers (e.g., SendGrid) POST delivery, bounce, and open events. The webhook updates the PostgreSQL `notification_status` table asynchronously.
</details>

<details>
<summary><strong>7. How do you scale consumer workers in Spring Boot?</strong></summary>

Set the `concurrency` attribute on `@KafkaListener` to match the partition count, and configure Java 21 Virtual Threads so blocking HTTP calls to SMS/Email APIs do not exhaust carrier threads.
</details>

<details>
<summary><strong>8. What is the Dead Letter Topic (DLT) strategy?</strong></summary>

When a message fails after maximum retries (e.g., invalid phone format), the `DeadLetterPublishingRecoverer` writes the poisoned record to `notifications.critical.DLT` so normal queue processing is not blocked.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create Priority Ingestion Kafka Producer</strong></summary>

**Prompt**: Write a Spring service method that routes `NotificationRequest` objects to their respective Kafka topics based on priority enum.

**Solution**:
Inspect Example 1: use a `switch (request.priority())` expression and call `kafkaTemplate.send(topic, key, payload)`.
</details>

<details>
<summary><strong>Exercise 2: Implement Redis Idempotency Check</strong></summary>

**Prompt**: Write a method that ensures a notification key is processed at most once within 12 hours.

**Solution**:
```java
public boolean markIfFirstTime(String dedupKey) {
    return Boolean.TRUE.equals(
        redis.opsForValue().setIfAbsent("dedup:" + dedupKey, "1", Duration.ofHours(12))
    );
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure Spring Kafka Dead Letter Topic Handler</strong></summary>

**Prompt**: Configure a `DefaultErrorHandler` with a 3-attempt backoff and DLT publisher.

**Solution**:
Inspect Example 5: instantiate `DeadLetterPublishingRecoverer` and `ExponentialBackOff`.
</details>

<details>
<summary><strong>Exercise 4: Implement In-Memory Email Template Renderer</strong></summary>

**Prompt**: Use Mustache or Thymeleaf to render a notification body given dynamic key-value parameters.

**Solution**:
```java
public String render(String template, Map<String, String> params) {
    for (var entry : params.entrySet()) {
        template = template.replace("{{" + entry.getKey() + "}}", entry.getValue());
    }
    return template;
}
```
</details>

<details>
<summary><strong>Exercise 5: Build a Provider Fallback Circuit Breaker</strong></summary>

**Prompt**: Implement a primary SMS sender with Resilience4j `@CircuitBreaker` that falls back to a secondary provider.

**Solution**:
```java
@CircuitBreaker(name = "primarySms", fallbackMethod = "backupSms")
public void sendSms(String phone, String msg) {
    twilioClient.send(phone, msg);
}
public void backupSms(String phone, String msg, Throwable t) {
    awsSnsClient.send(phone, msg);
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Notification Ingestion End-to-End</strong></summary>

**Prompt**: Write an integration test using Testcontainers (Kafka + Redis) sending two identical notification requests and verifying only one message arrives in Kafka.

**Solution**:
Post first message -> assert HTTP 202. Post duplicate message -> assert HTTP 202 (cached). Verify Kafka consumer receives exactly 1 payload.
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│               NOTIFICATION SYSTEM DESIGN CHEATSHEET                    │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Ingestion: HTTP 202 Accepted immediately; never block on vendor API │
│ 2. Deduplication: Redis SETNX with 24-hr TTL on idempotencyKey         │
│ 3. Priority: Separate Kafka topics for Critical, Trans, and Promo      │
│ 4. Failover: Resilience4j Circuit Breaker for multi-vendor SMS/Email   │
│ 5. Error Recovery: Dead Letter Topic (DLT) with Exponential Backoff    │
│ 6. Scale: 12+ Kafka partitions per topic, Virtual Thread consumers    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring for Apache Kafka Documentation](https://docs.spring.io/spring-kafka/reference/)
- [Designing a Notification System (Alex Xu System Design Volume 1)](https://bytebytego.com/)
- [Kafka Dead Letter Publishing Recoverer](https://docs.spring.io/spring-kafka/docs/current/reference/html/#dead-letters)
- [Resilience4j Circuit Breaker and Retry](https://resilience4j.readme.io/)
