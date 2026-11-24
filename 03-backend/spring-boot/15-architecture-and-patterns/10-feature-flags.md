# Feature Flags and Dynamic Toggles in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Togglz, Unleash, LaunchDarkly, Spring `@Conditional` |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Traditional continuous delivery workflows suffer from long-lived feature branches. When teams wait days or weeks to merge large features into the `main` branch, merge conflicts multiply and release coordination becomes agonizing. Furthermore, releasing a new capability requires a high-risk full container re-deployment; if a critical bug is discovered in production, the only recourse is an emergency hotfix or a rolling rollback, causing service disruption.

### The Evolution
**Feature Flags (Feature Toggles)**, pioneered by Martin Fowler and Pete Hodgson, decouple **Code Deployment** from **Feature Release**:
1. Code can be merged into `main` and deployed to production continuously in a "dark" state (turned OFF).
2. Features can be enabled dynamically in milliseconds via an admin dashboard or API call without restarting the application.
3. Teams execute safe release strategies: **Canary Releases** (enable for 5% of users), **Targeted Rollouts** (enable for beta customers only), and **Kill Switches** (instantly disable degrading third-party integrations).

### Production Relevance
In modern microservices and high-availability Spring Boot systems, dynamic feature flagging is indispensable. It enables Trunk-Based Development, automated A/B experimentation, progressive risk mitigation, and zero-downtime operational safety.

---

## 2. Core Concepts

### Feature Flag Taxonomy (Pete Hodgson Model)

```
+-----------------------------------------------------------------------------------------+
|                                FEATURE TOGGLE CATEGORIES                                |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  1. RELEASE TOGGLES                                                                     |
|     - Lifespan: Days to weeks (Temporary)                                               |
|     - Purpose: Trunk-Based Development; allows incomplete code to ship safely to PROD   |
|                                                                                         |
|  2. EXPERIMENT TOGGLES (A/B Testing)                                                    |
|     - Lifespan: Weeks to months (Temporary)                                             |
|     - Purpose: Multivariate user testing (e.g., checkout flow variant A vs variant B)   |
|                                                                                         |
|  3. OPS TOGGLES (Kill Switches)                                                         |
|     - Lifespan: Years / Permanent                                                       |
|     - Purpose: Circuit-breaker fallback; disables non-critical features under high load  |
|                                                                                         |
|  4. PERMISSION TOGGLES (Entitlements)                                                   |
|     - Lifespan: Permanent                                                               |
|     - Purpose: Feature gating based on user subscription tier (e.g., PRO vs BASIC)      |
|                                                                                         |
+-----------------------------------------------------------------------------------------+
```

### Static vs Dynamic Toggles in Spring Boot
- **Static Configuration (`@ConditionalOnProperty`)**: Evaluated once at application startup. Requires an environment variable change and a pod restart to toggle.
- **Dynamic Feature Flags (Togglz / Unleash / LaunchDarkly)**: Evaluated per-request or per-user dynamically in real time without restarting the JVM process.

---

## 3. How It Works Internally

### Real-Time Feature Evaluation Architecture with Unleash / Togglz

```
[ User Request: customerId="cust-42", country="US" ]
                         |
                         v
[ FeatureManager / UnleashClient.isEnabled("NEW_CHECKOUT_FLOW", context) ]
                         |
                         +---> [ Local In-Memory Fast Cache ] (Evaluated in <100 nanoseconds)
                         |     - Checks activation strategies:
                         |       * Is toggle globally ACTIVE?
                         |       * Is user in RolloutPercentage (10%)?
                         |       * Is user in BetaCustomers whitelist?
                         |
                         +---> (Background Sync Thread: polls Unleash Server / Redis every 10s)
                         |
                         v
           +-------------+-------------+
           |                           |
        (TRUE)                      (FALSE)
           v                           v
[ Execute New Checkout ]     [ Execute Legacy Checkout ]
```

1. **In-Memory Local Evaluation**: High-scale feature flagging systems never make a remote HTTP network call on every `isEnabled()` check. An embedded background daemon thread continuously synchronizes feature flag definitions from a central server (Unleash, LaunchDarkly, or Redis) into an in-memory memory map.
2. **Contextual Activation Strategies**: Evaluation accepts a `FeatureContext` containing user attributes (`userId`, `email`, `tenantId`, `country`). Custom strategies evaluate hash rings or whitelist rules to determine if the specific user qualifies for the feature.
3. **Execution Routing**: Business code uses simple boolean branching (`if (features.isActive(NEW_FLOW))`) or the Strategy Pattern to route execution.

---

## 4. Syntax & Usage

### 1. Static Feature Toggling via Spring `@ConditionalOnProperty`

```java
package com.example.feature.config;

import com.example.feature.service.RecommendationEngine;
import com.example.feature.service.AiRecommendationEngine;
import com.example.feature.service.SimpleRecommendationEngine;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class RecommendationFeatureConfig {

    @Bean
    @ConditionalOnProperty(name = "features.ai-recommendations.enabled", havingValue = "true")
    public RecommendationEngine aiEngine() {
        return new AiRecommendationEngine();
    }

    @Bean
    @ConditionalOnProperty(name = "features.ai-recommendations.enabled", havingValue = "false", matchIfMissing = true)
    public RecommendationEngine simpleEngine() {
        return new SimpleRecommendationEngine();
    }
}
```

### 2. Dynamic Feature Flags via Togglz Enum

```java
package com.example.feature.togglz;

import org.togglz.core.Feature;
import org.togglz.core.annotation.EnabledByDefault;
import org.togglz.core.annotation.Label;
import org.togglz.core.context.FeatureContext;

public enum MyFeatures implements Feature {

    @Label("New High-Performance Checkout Flow")
    NEW_CHECKOUT_FLOW,

    @Label("Instant AI Search Suggestions")
    AI_SEARCH_SUGGESTIONS,

    @EnabledByDefault
    @Label("Send SMS Notifications on Order Shipped")
    SMS_NOTIFICATIONS;

    public boolean isActive() {
        return FeatureContext.getFeatureManager().isActive(this);
    }
}
```

---

## 5. Practical Examples

### Example 1: Dynamic Execution Routing via Strategy Pattern and Feature Flags

```java
package com.example.checkout;

import com.example.feature.togglz.MyFeatures;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.UUID;

@Service
public class CheckoutService {

    private static final Logger log = LoggerFactory.getLogger(CheckoutService.class);

    private final ModernCheckoutProcessor modernProcessor;
    private final LegacyCheckoutProcessor legacyProcessor;

    public CheckoutService(ModernCheckoutProcessor modernProcessor, LegacyCheckoutProcessor legacyProcessor) {
        this.modernProcessor = modernProcessor;
        this.legacyProcessor = legacyProcessor;
    }

    public OrderResult checkout(UUID customerId, BigDecimal amount) {
        // Dynamic runtime check: evaluated in sub-microsecond time
        if (MyFeatures.NEW_CHECKOUT_FLOW.isActive()) {
            log.info("Routing customer {} through MODERN checkout flow.", customerId);
            return modernProcessor.process(customerId, amount);
        } else {
            log.info("Routing customer {} through LEGACY checkout flow.", customerId);
            return legacyProcessor.process(customerId, amount);
        }
    }
}
```

### Example 2: Enterprise Integration with Unleash Client

```java
package com.example.feature.unleash;

import io.getunleash.Unleash;
import io.getunleash.UnleashContext;
import org.springframework.stereotype.Service;

import java.util.UUID;

@Service
public class UnleashFeatureService {

    private final Unleash unleash;

    public UnleashFeatureService(Unleash unleash) {
        this.unleash = unleash;
    }

    public boolean isNewPricingActiveForCustomer(UUID customerId, String countryCode) {
        // Build contextual attributes for graduated or geographic rollout
        UnleashContext context = UnleashContext.builder()
                .userId(customerId.toString())
                .addProperty("country", countryCode)
                .build();

        // Evaluates strategies like: "Rollout to 20% of users in US and CA"
        return unleash.isEnabled("NEW_PRICING_MODEL", context);
    }
}
```

### Example 3: Operational Kill Switch for Downstream 3rd-Party APIs

```java
package com.example.integration;

import com.example.feature.togglz.MyFeatures;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

@Component
public class FraudDetectionClient {

    private static final Logger log = LoggerFactory.getLogger(FraudDetectionClient.class);

    public boolean checkFraud(String transactionId, double amount) {
        // Ops Kill Switch: if external vendor experiences outage or latency spikes,
        // on-call engineers flip this flag to OFF via Togglz admin console in 1 second!
        if (!MyFeatures.SMS_NOTIFICATIONS.isActive()) {
            log.warn("Fraud detection kill-switch ACTIVE. Bypassing check for txn={}", transactionId);
            return true; // Safe fallback
        }

        // Call external third-party vendor REST API...
        return callThirdPartyFraudVendor(transactionId, amount);
    }

    private boolean callThirdPartyFraudVendor(String txnId, double amount) {
        return true; 
    }
}
```

### Example 4: Anti-Pattern (Long-Lived Dead Flags) vs Clean Feature Lifecycle

#### The Anti-Pattern: Technical Debt Accumulation
```java
// BAD: Feature flag from 2023 still present in 2026!
// Codebase has 40 nested if-else blocks checking flags that are permanently ON.
public void processReport(ReportRequest req) {
    if (featureManager.isActive("ENABLE_REDIS_CACHE_2023")) {
        if (featureManager.isActive("NEW_DATE_FORMAT_DEC_2024")) {
            if (featureManager.isActive("TEMP_MIGRATION_STEP_3")) {
                // Hard to read, impossible to test all permutation branches!
            }
        }
    }
}
```

#### Production Fix: Scheduled Flag Retirement Process
```java
// GOOD: Once a feature rollout reaches 100% in production for 2 weeks:
// 1. Delete the feature toggle check completely.
// 2. Delete the legacy branch.
// 3. Keep business logic simple and direct.
public void processReport(ReportRequest req) {
    // Clean, direct production logic without archaic toggle cruft
    reportService.generateReport(req);
}
```

### Example 5: Custom Togglz Activation Strategy (VIP Customer Whitelist)

```java
package com.example.feature.strategy;

import org.togglz.core.activation.Parameter;
import org.togglz.core.repository.FeatureState;
import org.togglz.core.spi.ActivationStrategy;
import org.togglz.core.user.FeatureUser;

import java.util.Arrays;
import java.util.List;

public class VipCustomerActivationStrategy implements ActivationStrategy {

    public static final String ID = "vip-customer-strategy";
    public static final String PARAM_CUSTOMERS = "allowed_customer_ids";

    @Override
    public String getId() { return ID; }

    @Override
    public String getName() { return "VIP Customer Whitelist"; }

    @Override
    public boolean isActive(FeatureState featureState, FeatureUser user) {
        if (user == null || user.getName() == null) return false;

        String rawList = featureState.getParameter(PARAM_CUSTOMERS);
        if (rawList == null || rawList.isBlank()) return false;

        List<String> allowedIds = Arrays.stream(rawList.split(","))
                .map(String::trim)
                .toList();

        return allowedIds.contains(user.getName());
    }

    @Override
    public Parameter[] getParameters() {
        return new Parameter[] {
            new Parameter() {
                @Override public String getName() { return PARAM_CUSTOMERS; }
                @Override public String getLabel() { return "Comma-separated Customer IDs"; }
                @Override public String getDescription() { return "List of VIP customers granted early access"; }
                @Override public boolean isOptional() { return false; }
                @Override public boolean isLargeText() { return true; }
            }
        };
    }
}
```

---

## 6. Best Practices

1. **Establish a Strict Retirement Lifecycle for Release Flags**: Feature flags are temporary inventory. Set a calendar reminder or Jira ticket to delete the flag and its legacy branch 14 days after reaching 100% rollout.
2. **Never Evaluate Feature Flags via Remote HTTP on the Request Thread**: Use feature flag clients (Unleash, LaunchDarkly, Togglz) that cache flag states in local JVM memory and sync in the background.
3. **Use the Strategy Pattern for Large Architectural Changes**: When replacing an entire subsystem, inject a feature-flag-aware decorator or router rather than littering 50 `if-else` checks across your services.
4. **Pass Rich Contextual Attributes**: Always provide user ID, tenant ID, and geographic attributes to the feature manager so gradual canary rollouts execute deterministically.
5. **Secure the Feature Management Console**: Secure admin endpoints (`/togglz-console` or Unleash webhooks) behind Spring Security with multi-factor authentication (MFA) and audit logging.
6. **Test Both Toggle States in Automated Unit Tests**: Write parameterized tests executing both the `flag=ON` and `flag=OFF` paths to ensure legacy code remains stable until retired.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Accumulating "Dead" Flags Over Years** | Exponential explosion of code paths; codebase becomes unmaintainable and impossible to test. | Treat flags as technical debt; aggressively delete release flags after full rollout. |
| **Evaluating Flags via Synchronous REST Calls**| Every user click incurs an extra 50ms network roundtrip to the feature flag server, degrading latency. | Use client SDKs with local in-memory caching and background pollers. |
| **Exposing `/togglz-console` to the Public Web** | Attackers can toggle operational kill-switches or activate unreleased features. | Bind to private management port (`9090`) and enforce `hasRole('ADMIN')`. |
| **Using Feature Flags for Long-Term Authorization**| Conflating feature toggles with Role-Based Access Control (RBAC) complicates security audits. | Use Spring Security (`@PreAuthorize`) for permissions; use feature flags for release management. |
| **Non-Deterministic Rollout Hashing** | A user refreshes their browser and sees the new checkout flow, then refreshes again and sees the old flow! | Use consistent hashing on `userId` (e.g., MurmurHash3) so a user stays pinned to their assigned bucket. |
| **Toggling Database Schema Changes Dynamically** | Toggling code that expects a column to exist while the DB migration has not run throws `SQLException`. | Follow the Expand-Contract pattern for database migrations independently of feature flags. |

---

## 8. Performance & Security Notes

### Performance
- **Sub-Microsecond In-Memory Evaluation**: In-memory hash map lookup and bitwise bucket calculation takes under **50 nanoseconds**. Feature flags evaluated via local cache have virtually zero impact on request throughput.
- **Garbage Collection Overhead**: Avoid allocating large `Context` objects repeatedly in tight loops. Reuse or pool context representations.

### Security
- **Audit Logging for State Changes**: Every time an engineer flips a feature flag in production, record an audit event in OpenSearch/Datadog capturing: `who`, `which_flag`, `old_state`, `new_state`, and `timestamp`.
- **RBAC on Flag Controls**: Restrict kill-switch access to SRE on-call engineers; restrict experiment flags to product managers.

---

## 9. Comparison / When to Use What

| Tool / Pattern | Evaluation Model | Dynamic Changes | Best Used For |
|---|---|---|---|
| **Spring `@ConditionalOnProperty`**| Startup time (JVM boot) | Requires Pod Restart | Enabling whole infrastructure modules (e.g., Mock vs Real AWS) |
| **Togglz (Open Source)** | Runtime (In-Memory) | Instant (Admin UI / DB) | Small-to-medium teams, simple monoliths |
| **Unleash (Open Source / Enterprise)**| Runtime (In-Memory + Daemon) | Instant (Web console/API)| Distributed microservices, graduated rollouts |
| **LaunchDarkly (Commercial SaaS)** | Runtime (Edge SSE Stream) | Real-time (<200ms) | Global enterprise scale, multi-platform, A/B experimentation |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the difference between a Release Toggle and an Ops Toggle (Kill Switch)?</strong></summary>

- **Release Toggle**: Temporary flag (lifetime: 1–2 weeks) used to enable Trunk-Based Development. It allows unfinished features to be deployed safely into production in an inactive state. Once the feature is validated and rolled out to 100% of users, the flag is retired and deleted.
- **Ops Toggle (Kill Switch)**: Long-lived or permanent flag designed for system resilience. It gates non-critical or resource-intensive capabilities (e.g., real-time recommendation carousels or image OCR). If the database or third-party vendor experiences severe degradation, operations teams flip the kill switch to shed load immediately without deploying code.
</details>

<details>
<summary><strong>2. How do feature flag SDKs achieve gradual canary rollouts (e.g., enabling for exactly 10% of users) consistently?</strong></summary>

SDKs use **Consistent Hashing**:
1. Take the feature name and the unique user identifier: `hashKey = "NEW_CHECKOUT:" + userId`.
2. Compute a hash value using a fast, uniform hashing algorithm (e.g., MurmurHash3 or MD5):
   `hashValue = Math.abs(murmur3(hashKey)) % 100;`
3. If `hashValue < 10`, the feature evaluates to `true`.
**Properties**:
- **Uniformity**: Exactly 10% of users fall into the bucket.
- **Consistency**: A specific user with ID `12345` always hashes to the same bucket; they will never experience UI flickering between refreshes.
- **Independent Bucketing**: Different features hash independently, so a user in the 10% bucket for Feature A is not automatically in the 10% bucket for Feature B.
</details>

<details>
<summary><strong>3. Why should you avoid using feature flags to control database schema migrations?</strong></summary>

Feature flags are designed to toggle back and forth instantaneously. Relational database schemas (PostgreSQL, MySQL) cannot toggle back and forth instantaneously:
- Dropping a column or renaming a table is a destructive, irreversible operation.
- If you toggle a feature OFF, but the code still relies on the old schema which was altered, the application crashes immediately with `PSQLException`.
**Solution**: Always use the **Expand-Contract (Parallel Run) Pattern** for database migrations, keeping the schema backward-compatible with both toggle states.
</details>

<details>
<summary><strong>4. How do you prevent "Toggle Debt" from overwhelming a Spring Boot codebase?</strong></summary>

1. **Tag Flags with Expiration Dates**: Include metadata on feature definitions (e.g., `@FeatureMetadata(owner="team-billing", expires="2026-12-01")`).
2. **Automated CI/CD Warnings**: Write a unit test that fails or logs loud warnings in CI when an active release toggle passes its expiration date.
3. **Strict "One In, One Out" Policy**: Require engineering teams to remove an old toggle before introducing a new release toggle.
4. **Use Abstract Routers / Strategy Pattern**: Isolate toggle branching to a single factory class instead of sprinkling `if (flag)` checks across 30 controller and service classes.
</details>

<details>
<summary><strong>5. How does Spring's `@ConditionalOnProperty` differ fundamentally from Togglz or Unleash?</strong></summary>

`@ConditionalOnProperty` is a **static, compile/startup-time** condition.
- It is evaluated by Spring's `ConfigurationClassPostProcessor` when the `ApplicationContext` is being initialized.
- If the property is false, the bean is never registered in the Spring context.
- You **cannot change it at runtime** without modifying environment variables and restarting the entire JVM container.
In contrast, Togglz/Unleash are **dynamic runtime** engines: beans are loaded into memory, and boolean branching occurs on every HTTP request in real time.
</details>

<details>
<summary><strong>6. How do you test code wrapped in feature flags in JUnit 5?</strong></summary>

In unit tests, mock the `FeatureManager` or use Togglz testing utilities:
```java
@ExtendWith(TogglzRule.class)
class CheckoutServiceTest {

    @Test
    void shouldExecuteModernFlowWhenFlagEnabled() {
        // Explicitly set state for this test execution
        FeatureContext.getFeatureManager().setFeatureState(
            new FeatureState(MyFeatures.NEW_CHECKOUT_FLOW, true)
        );

        OrderResult res = checkoutService.checkout(custId, amount);
        assertTrue(res.isModern());
    }
}
```
</details>

<details>
<summary><strong>7. How can feature flags be used to conduct dark launches?</strong></summary>

A **Dark Launch** means deploying new backend logic to production and running it in the background for live production requests *without* displaying the results to the user:
- The system calls the legacy implementation and returns its result to the user.
- In parallel (or via a feature flag), the system executes the new algorithm, compares performance and accuracy, and logs discrepancies.
- Once stability is proven under live production traffic, the feature flag is toggled to serve the new implementation directly.
</details>

<details>
<summary><strong>8. What security precautions must be applied to dynamic feature toggle endpoints?</strong></summary>

Feature flag administration endpoints (such as `/actuator/togglz` or Togglz Web Console) can disable core business functionality or activate sensitive internal tools.
Precautions:
1. Enforce strict authentication via Spring Security with Role-Based Access (`ROLE_RELEASE_ENGINEER`).
2. Require Multi-Factor Authentication (MFA) for production changes.
3. Emit immutable audit log records to a central SIEM system on every state mutation.
4. Restrict management access to internal corporate VPN / VPC networks.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Configure Static Feature Flag with `@ConditionalOnProperty`</strong></summary>

**Objective**: Create a notification service bean that is registered only when `app.notifications.email.enabled=true`.

**Solution**:
```java
@Service
@ConditionalOnProperty(prefix = "app.notifications.email", name = "enabled", havingValue = "true")
public class EmailNotificationService implements NotificationService {
    @Override
    public void notifyUser(String message) {
        // Send email
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Define a Togglz Feature Enum</strong></summary>

**Objective**: Create an enum with three distinct feature flags using Togglz annotations.

**Solution**:
```java
public enum AppFeatures implements Feature {
    @Label("Enable Virtual Threads for Async Processing")
    VIRTUAL_THREADS,

    @Label("Enable Stripe Payment Element Gateway")
    STRIPE_GATEWAY,

    @EnabledByDefault
    @Label("Collect User Analytics Telemetry")
    ANALYTICS_TELEMETRY;

    public boolean isActive() {
        return FeatureContext.getFeatureManager().isActive(this);
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement a Feature-Flagged Strategy Router</strong></summary>

**Objective**: Use the Strategy Pattern to route between two search engines based on a feature flag.

**Solution**:
```java
@Service
public class SearchRoutingService {
    private final SearchEngine elasticEngine;
    private final SearchEngine pgVectorEngine;

    public SearchRoutingService(
            @Qualifier("elastic") SearchEngine elastic,
            @Qualifier("pgVector") SearchEngine pgVector) {
        this.elasticEngine = elastic;
        this.pgVectorEngine = pgVector;
    }

    public SearchResult search(String query) {
        if (AppFeatures.VIRTUAL_THREADS.isActive()) {
            return pgVectorEngine.search(query);
        }
        return elasticEngine.search(query);
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build an Automated Feature Flag Expiry Test</strong></summary>

**Objective**: Write an ArchUnit or JUnit 5 test that fails if any feature flag marked with `@ExpiresBefore` has expired.

**Solution**:
```java
@Target(ElementType.FIELD)
@Retention(RetentionPolicy.RUNTIME)
public @interface ExpiresBefore {
    String date(); // Format: yyyy-MM-dd
}

class FeatureFlagSanityTest {

    @Test
    void assertNoExpiredFeatureFlagsInProduction() {
        LocalDate today = LocalDate.now();
        for (Field field : AppFeatures.class.getFields()) {
            ExpiresBefore expiry = field.getAnnotation(ExpiresBefore.class);
            if (expiry != null) {
                LocalDate expirationDate = LocalDate.parse(expiry.date());
                assertFalse(today.isAfter(expirationDate), 
                    "Feature flag " + field.getName() + " expired on " + expirationDate + " and must be retired!");
            }
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Consistent Hashing for Gradual Rollout Strategy</strong></summary>

**Objective**: Implement a pure Java consistent hashing algorithm that assigns a user deterministically to a percentage bucket (0-99).

**Solution**:
```java
public class RolloutCalculator {

    public static boolean isEligibleForRollout(String featureName, String userId, int targetPercentage) {
        if (targetPercentage <= 0) return false;
        if (targetPercentage >= 100) return true;

        String input = featureName + ":" + userId;
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] hash = md.digest(input.getBytes(StandardCharsets.UTF_8));
            
            // Convert first 4 bytes to integer
            int hashInt = ByteBuffer.wrap(hash).getInt();
            int bucket = Math.abs(hashInt % 100);

            return bucket < targetPercentage;
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build a Redis-Backed Feature Flag Provider with In-Memory Cache</strong></summary>

**Objective**: Implement a high-performance Spring feature store that reads from a local `ConcurrentHashMap` updated via Redis Pub/Sub invalidation.

**Solution**:
```java
@Component
public class CachedRedisFeatureProvider {

    private final Map<String, Boolean> localCache = new ConcurrentHashMap<>();
    private final StringRedisTemplate redisTemplate;

    public CachedRedisFeatureProvider(StringRedisTemplate redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    public boolean isEnabled(String flagName) {
        // 1. Fast in-memory lookup (<50ns)
        return localCache.computeIfAbsent(flagName, key -> {
            String val = redisTemplate.opsForValue().get("features:" + key);
            return "true".equalsIgnoreCase(val);
        });
    }

    // Listens to Redis PubSub topic when admin flips a flag in the dashboard
    public void onFeatureUpdateNotification(String flagName, boolean newState) {
        localCache.put(flagName, newState);
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| FEATURE FLAGS IN SPRING BOOT QUICK REFERENCE                                                       |
+----------------------------------------------------------------------------------------------------+
| Category            | Tool / Annotation                         | Characteristics                  |
+---------------------+-------------------------------------------+----------------------------------+
| Static Flag         | @ConditionalOnProperty                    | Startup-time evaluation;         |
|                     |                                           | requires JVM restart to change   |
+---------------------+-------------------------------------------+----------------------------------+
| Release Toggle      | Togglz / Unleash (Release Flag)           | Short-lived; enables Trunk-Based |
|                     |                                           | Development; delete after 100%   |
+---------------------+-------------------------------------------+----------------------------------+
| Ops Kill-Switch     | Togglz / Unleash (Kill Switch)            | Permanent; disables heavy third- |
|                     |                                           | party APIs during outages        |
+---------------------+-------------------------------------------+----------------------------------+
| Consistent Hashing  | MurmurHash3 / SHA-256 on userId           | Deterministic, flicker-free      |
|                     |                                           | percentage canary rollouts       |
+---------------------+-------------------------------------------+----------------------------------+
| Local Caching Rule  | Never evaluate flags via remote HTTP      | In-memory caching with           |
|                     | on the live request path                  | background daemon synchronization|
+---------------------+-------------------------------------------+----------------------------------+
| Security Rule       | Protect /togglz-console behind MFA & RBAC | Avoid unauthorized toggle flips  |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Martin Fowler & Pete Hodgson: Feature Toggles (Feature Flags) Comprehensive Guide](https://martinfowler.com/articles/feature-toggles.html)
- [Togglz Documentation for Spring Boot](https://www.togglz.org/documentation/overview.html)
- [Unleash Open-Source Feature Management Platform](https://www.getunleash.io/)
- [LaunchDarkly: Architecture and Effective Feature Management](https://launchdarkly.com/)
- [Trunk-Based Development Guide](https://trunkbaseddevelopment.com/)
