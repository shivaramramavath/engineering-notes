# Payment Integration with Stripe: PaymentIntents, Idempotency, and Webhooks

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `com.stripe:stripe-java:28.x` |

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
Integrating online payments into web applications introduces severe security and regulatory obligations. Under PCI-DSS (Payment Card Industry Data Security Standard) regulations, if raw credit card numbers or CVV codes ever touch an application server's memory or disks, the organization is subjected to expensive security audits, legal liabilities, and strict compliance mandates. Furthermore, financial transactions are vulnerable to double-charging errors caused by network timeouts, browser reloads, and asynchronous payment authorizations (like 3D Secure / SCA).

### The Evolution
Modern payment gateways like Stripe solve this using client-side tokenization and the **PaymentIntents API**. Card details are collected securely in the user's browser using sandboxed iframes (Stripe Elements) and submitted directly to Stripe's PCI-compliant vaults. The backend application server manages only high-level payment states, generating intent tokens, enforcing **Idempotency Keys** to prevent double charging, and listening for asynchronous payment confirmation events via cryptographically verified **Webhooks**.

### Relevance in Modern Engineering
In fintech, e-commerce, and subscription SaaS systems, payment flows must be resilient against network partitions and fraud. Spring Boot developers must master the full Stripe lifecycle: creating PaymentIntents with currency-minor-unit handling, verifying HMAC webhook signatures, safely handling SCA authentication challenges, and managing subscription lifecycles.

---

## 2. Core Concepts

- **PCI-DSS Scope Minimization**: Ensuring raw card details never touch backend application servers by delegating collection to client-side Stripe Elements.
- **PaymentIntent**: A stateful Stripe object tracking the lifecycle of an individual customer payment from creation (`requires_payment_method`) through confirmation (`succeeded`) or failure (`canceled`).
- **Idempotency Key**: A unique string (typically a UUID) passed in the `Idempotency-Key` header; Stripe guarantees that duplicate requests carrying the same key will never result in multiple charges.
- **Currency Minor Units**: Financial amounts in Stripe are represented as integers in the currency's smallest unit (e.g., \$10.50 USD is represented as `1050` cents, whereas ¥100 JPY is `100`).
- **Webhook Signature Verification (`Webhook.constructEvent`)**: Cryptographically verifying the `Stripe-Signature` header using the webhook signing secret to defend against spoofing and tampering.
- **Strong Customer Authentication (SCA / 3D Secure)**: Regulatory standard requiring two-factor biometric or OTP authentication for European cards, handled dynamically via PaymentIntent status transitions (`requires_action`).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Modern Stripe PaymentIntent Flow                           |
|                                                                                   |
| 1. Customer Initiates Checkout                                                    |
|    POST /api/v1/checkout/create-intent { orderId: "ord_101" }                     |
|                   |                                                               |
|                   v                                                               |
| 2. Spring Backend Creates PaymentIntent via Stripe SDK                            |
|    PaymentIntent.create(params, RequestOptions.withIdempotencyKey("ord_101"))    |
|                   | Stripe returns client_secret: "pi_123_secret_abc"             |
|                   v                                                               |
| 3. Backend Returns client_secret to Frontend Browser                              |
|                   |                                                               |
|                   v                                                               |
| 4. Frontend Confirms Payment via Stripe Elements (Browser to Stripe Vault)        |
|    stripe.confirmCardPayment(client_secret, { payment_method: cardElement })      |
|                   | (Handles 3D Secure / SCA challenge if required)               |
|                   v                                                               |
| 5. Stripe Processes Charge & Emits Webhook to Backend Server                      |
|    POST /api/v1/payments/webhook                                                  |
|    Header: Stripe-Signature: t=1716982000,v1=9f8c...3a                           |
|                   |                                                               |
|                   v                                                               |
| 6. Spring Webhook Controller                                                      |
|    Webhook.constructEvent(payload, signature, webhookSecret)                      |
|                   | Signature Valid -> Handle "payment_intent.succeeded"          |
|                   v                                                               |
| 7. Fulfill Order in Database & Return 200 OK to Stripe                            |
+-----------------------------------------------------------------------------------+
```

The browser submits checkout details to the Spring backend. The service calculates the monetary amount and creates a `PaymentIntent` via the Stripe Java SDK, attaching an `Idempotency-Key` derived from the order ID. Stripe returns a `client_secret`. The frontend uses this secret with Stripe.js to prompt the customer for card details and authorize payment. Once the payment settles, Stripe dispatches a `payment_intent.succeeded` webhook to the backend. The backend verifies the HMAC signature, confirms order fulfillment, and returns HTTP 200.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>com.stripe</groupId>
    <artifactId>stripe-java</artifactId>
    <version>28.0.0</version>
</dependency>
```

### Application Properties
```properties
# Stripe API Keys (Use test keys during development)
stripe.api-key=sk_test_51Mz...
stripe.webhook-secret=whsec_9b...
```

---

## 5. Practical Examples

### Example 1: Stripe Initialization Bean
```java
package com.example.platform.payment.config;

import com.stripe.Stripe;
import jakarta.annotation.PostConstruct;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Configuration;

@Configuration
public class StripeConfig {

    @Value("${stripe.api-key}")
    private String apiKey;

    @PostConstruct
    public void initStripe() {
        // Globally initialize Stripe API key
        Stripe.apiKey = apiKey;
    }
}
```

### Example 2: Idempotent PaymentIntent Creation Service
```java
package com.example.platform.payment.service;

import com.stripe.exception.StripeException;
import com.stripe.model.PaymentIntent;
import com.stripe.net.RequestOptions;
import com.stripe.param.PaymentIntentCreateParams;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.Map;

@Service
public class PaymentIntentService {

    private static final Logger log = LoggerFactory.getLogger(PaymentIntentService.class);

    public record PaymentIntentResponse(String paymentIntentId, String clientSecret, long amountCents) {}

    public PaymentIntentResponse createPaymentIntent(String orderId, BigDecimal amountDollars, String customerEmail) {
        log.info("Creating PaymentIntent for order: {}, amount: ${}", orderId, amountDollars);

        // Convert dollars to cents (Stripe minor currency unit)
        long amountCents = amountDollars.multiply(BigDecimal.valueOf(100)).longValueExact();

        PaymentIntentCreateParams params = PaymentIntentCreateParams.builder()
                .setAmount(amountCents)
                .setCurrency("usd")
                .setReceiptEmail(customerEmail)
                .putMetadata("orderId", orderId)
                .setAutomaticPaymentMethods(
                        PaymentIntentCreateParams.AutomaticPaymentMethods.builder().setEnabled(true).build())
                .build();

        // Enforce idempotency: re-submitting with same orderId will return existing intent without charging twice
        RequestOptions options = RequestOptions.builder()
                .setIdempotencyKey("create-intent-" + orderId)
                .build();

        try {
            PaymentIntent intent = PaymentIntent.create(params, options);
            return new PaymentIntentResponse(intent.getId(), intent.getClientSecret(), intent.getAmount());
        } catch (StripeException e) {
            log.error("Stripe API error while creating PaymentIntent for order: {}", orderId, e);
            throw new IllegalStateException("Payment provider error: " + e.getMessage(), e);
        }
    }
}
```

### Example 3: Cryptographically Verified Stripe Webhook Controller
```java
package com.example.platform.payment.controller;

import com.stripe.exception.SignatureVerificationException;
import com.stripe.model.Event;
import com.stripe.model.PaymentIntent;
import com.stripe.net.Webhook;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/v1/payments/webhook")
public class StripeWebhookController {

    private static final Logger log = LoggerFactory.getLogger(StripeWebhookController.class);
    private final String webhookSecret;

    public StripeWebhookController(@Value("${stripe.webhook-secret}") String webhookSecret) {
        this.webhookSecret = webhookSecret;
    }

    @PostMapping
    public ResponseEntity<String> handleStripeWebhook(
            @RequestHeader("Stripe-Signature") String sigHeader,
            @RequestBody String rawPayload) {

        Event event;
        try {
            // Cryptographically verify HMAC-SHA256 signature using the raw payload string
            event = Webhook.constructEvent(rawPayload, sigHeader, webhookSecret);
        } catch (SignatureVerificationException e) {
            log.warn("Invalid Stripe webhook signature: {}", e.getMessage());
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body("Invalid signature");
        } catch (Exception e) {
            log.error("Failed to parse Stripe webhook event", e);
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body("Webhook parse error");
        }

        // Handle specific event types
        switch (event.getType()) {
            case "payment_intent.succeeded" -> {
                PaymentIntent intent = (PaymentIntent) event.getDataObjectDeserializer().getObject().orElse(null);
                if (intent != null) {
                    handlePaymentSuccess(intent);
                }
            }
            case "payment_intent.payment_failed" -> {
                PaymentIntent intent = (PaymentIntent) event.getDataObjectDeserializer().getObject().orElse(null);
                if (intent != null) {
                    handlePaymentFailure(intent);
                }
            }
            default -> log.debug("Unhandled Stripe event type: {}", event.getType());
        }

        return ResponseEntity.ok("Success");
    }

    private void handlePaymentSuccess(PaymentIntent intent) {
        String orderId = intent.getMetadata().get("orderId");
        log.info("Payment SUCCEEDED for order: {}, PaymentIntent: {}, Amount: {}",
                orderId, intent.getId(), intent.getAmount());
        // Transition order state to PAID in database
    }

    private void handlePaymentFailure(PaymentIntent intent) {
        String orderId = intent.getMetadata().get("orderId");
        log.warn("Payment FAILED for order: {}, Failure: {}",
                orderId, intent.getLastPaymentError() != null ? intent.getLastPaymentError().getMessage() : "Unknown");
    }
}
```

### Example 4: Parsing Parsed JSON Instead of Raw Payload (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Injecting a parsed DTO or Map in the webhook controller.
// Jackson re-formats whitespace and sorts keys, causing Webhook.constructEvent to fail signature verification every time!
// @PostMapping
// public ResponseEntity<?> handleWebhook(@RequestBody Map<String, Object> jsonMap) { ... } // SIGNATURE FAILS!

// FIX: Always inject the raw, unparsed String payload directly from the HTTP request body
@PostMapping
public ResponseEntity<Void> handleWebhook(
        @RequestHeader("Stripe-Signature") String sigHeader,
        @RequestBody String rawPayload) { // Crucial: Inject raw String directly!
    // Signature verification succeeds
    Event event = Webhook.constructEvent(rawPayload, sigHeader, webhookSecret);
    return ResponseEntity.ok().build();
}
```

### Example 5: Customer Creation and Saved Payment Method Service
```java
package com.example.platform.payment.service;

import com.stripe.exception.StripeException;
import com.stripe.model.Customer;
import com.stripe.param.CustomerCreateParams;
import org.springframework.stereotype.Service;

import java.util.Map;

@Service
public class StripeCustomerService {

    public String createOrGetCustomer(String userId, String email, String name) throws StripeException {
        CustomerCreateParams params = CustomerCreateParams.builder()
                .setEmail(email)
                .setName(name)
                .putMetadata("internalUserId", userId)
                .build();

        Customer customer = Customer.create(params);
        return customer.getId(); // e.g. "cus_Q1a2B3c4D5"
    }
}
```

---

## 6. Best Practices

1. **Never Accept Raw Credit Card Data on Your Servers**: Delegate card data collection exclusively to client-side Stripe Elements or Stripe Checkout to keep your infrastructure under PCI SAQ-A compliance scope.
2. **Always Use Idempotency Keys**: Attach a unique idempotency key (derived from order ID or checkout session UUID) to all mutating Stripe requests (`PaymentIntent.create`, `Refund.create`).
3. **Verify Webhook Signatures Using Raw Request Bodies**: Never parse the request body before verifying; always pass the unparsed raw `String` body to `Webhook.constructEvent()`.
4. **Use Integer Minor Units for Monetary Values**: Always handle currency amounts as integers (cents for USD/EUR, yen for JPY) to eliminate floating-point arithmetic errors.
5. **Acknowledge Webhooks Within 3 Seconds**: Return HTTP 200 to Stripe immediately; offload long-running order fulfillment tasks to background workers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Re-serializing JSON before signature verification | Key sorting or whitespace changes invalidate the HMAC signature | Accept `@RequestBody String rawPayload` directly in the controller. |
| Using floating-point `double` for currency amounts | IEEE 754 precision issues (e.g., \$19.99 * 100 becomes 1998.99999) | Use `BigDecimal` with explicit scale and convert to integer minor units. |
| Fulfilling orders synchronously in the checkout return URL | Customers closing browser tabs before redirect causes unfulfilled orders | Fulfill orders exclusively in response to `payment_intent.succeeded` webhooks. |
| Committing live secret keys (`sk_live_...`) to version control | Scrapers steal keys and compromise payment vaults | Inject keys via environment variables or cloud secrets managers. |
| Forgetting to handle duplicate webhook deliveries | Stripe retries webhooks on network drops, causing duplicate order fulfillments | Record processed `event.getId()` in database and enforce idempotency. |
| Passing wrong currency codes | Stripe rejects invalid ISO-4217 currency abbreviations | Always pass lowercase three-letter ISO currency codes (`usd`, `eur`, `gbp`). |

---

## 8. Performance & Security Notes

- **PCI SAQ-A Compliance**: By using Stripe Elements, your backend never touches PAN (Primary Account Numbers) or CVV codes, qualifying your business for the simplest self-assessment questionnaire (SAQ-A).
- **Webhook Timestamp Tolerance**: `Webhook.constructEvent()` enforces a default 300-second timestamp tolerance to protect against replay attacks. Ensure server system clocks are synchronized via NTP.
- **TLS 1.2+ Requirement**: Stripe's API strictly rejects any incoming or outgoing HTTP connection that does not support TLS 1.2 or TLS 1.3.

---

## 9. Comparison / When to Use What

| Pattern | Stripe PaymentIntents (Elements) | Stripe Checkout (Hosted) | Direct API (Custom Card Collection) |
|---|---|---|---|
| **PCI Scope** | SAQ-A (Minimal scope) | SAQ-A (Lowest scope) | SAQ D (Full audit required - DO NOT USE) |
| **UI Customization** | Completely customized UI inside your site | Redirects to Stripe-hosted page | Completely customized |
| **SCA / 3D Secure** | Handled natively by Stripe.js | Handled automatically by Stripe | Must implement 3DS flows manually |
| **Best For** | Modern web & mobile e-commerce | Rapid MVP launches, simple checkouts | Legacy enterprise banking |

---

## 10. Interview Questions

<details>
<summary>1. What is the PaymentIntents API and why did Stripe introduce it over the legacy Charges API?</summary>
The PaymentIntents API was introduced to handle complex, multi-step asynchronous payment flows, particularly Strong Customer Authentication (SCA) and 3D Secure 2 required in Europe. Unlike the legacy Charges API (which assumed immediate success or failure), PaymentIntents represent a state machine that tracks payments through authentication challenges (`requires_action`), captures, and webhooks.
</details>

<details>
<summary>2. Why should order fulfillment occur in a webhook handler rather than on the client-side redirect?</summary>
Relying on client-side redirects is fragile: the customer may close the browser tab, lose internet connectivity, or experience a crash immediately after authorizing payment. The webhook is sent directly from Stripe's servers to your backend, guaranteeing that payment confirmation is delivered regardless of client browser state.
</details>

<details>
<summary>3. How does Stripe's Idempotency Key feature work?</summary>
When creating or updating charges, the client passes an `Idempotency-Key: <unique-string>` header. Stripe caches the initial response associated with that key for 24 hours. If network timeouts cause the application to retry the identical request, Stripe returns the cached response without creating a second charge.
</details>

<details>
<summary>4. Why must the raw HTTP request body string be used when verifying Stripe webhook signatures?</summary>
Stripe's cryptographic signature is an HMAC-SHA256 hash computed over the exact byte sequence of the original JSON payload and timestamp. If an application parses the body into an object and re-serializes it, key order, formatting, or whitespace changes alter the string, resulting in signature verification failure.
</details>

<details>
<summary>5. How does Stripe represent currency amounts in its API?</summary>
Stripe represents monetary amounts as integers in the currency's smallest unit (minor unit). For example, \$10.50 USD is passed as `1050` cents, €20.00 EUR as `2000`, and ¥500 JPY as `500` (zero-decimal currency). This eliminates floating-point rounding errors.
</details>

<details>
<summary>6. How do you defend against duplicate webhook deliveries?</summary>
Webhook handlers must be idempotent. Each Stripe event has a globally unique `id` (e.g., `evt_123456`). Before processing an event, the handler checks whether the event ID has already been recorded in a database table or Redis cache. If present, it acknowledges HTTP 200 immediately without re-processing.
</details>

<details>
<summary>7. What is PCI-DSS SAQ-A and how does Stripe Elements qualify a merchant for it?</summary>
SAQ-A is the shortest, least restrictive PCI self-assessment questionnaire. Stripe Elements qualifies merchants for SAQ-A because cardholder data is collected inside iframes hosted directly on Stripe's servers. Card data travels directly from the user's browser to Stripe's secure vault; the merchant's server never receives, transmits, or stores cardholder details.
</details>

<details>
<summary>8. What is the difference between client_secret and the publishable/secret API keys?</summary>
The publishable key (`pk_test_...`) identifies your account in client-side code. The secret key (`sk_test_...`) has full administrative permissions and is kept strictly on the backend. The `client_secret` is a scoped, short-lived token generated for a specific `PaymentIntent` allowing the client to complete that single transaction without exposing account credentials.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Initialize Stripe SDK with API key</summary>
Write a Spring `@Configuration` class initializing `Stripe.apiKey` from an injected application property.
</details>

<details>
<summary>Exercise 2 (Easy): Convert dollar amounts to Stripe currency cents</summary>
Write a utility method `long toCents(BigDecimal amount)` that accurately converts dollars to cents without floating-point errors.
</details>

<details>
<summary>Exercise 3 (Medium): Implement createPaymentIntent endpoint</summary>
Write a `@PostMapping("/api/v1/payments/intent")` controller returning the `clientSecret` and intent ID with an idempotency key.
</details>

<details>
<summary>Exercise 4 (Medium): Build an idempotent webhook handler</summary>
Implement a webhook controller that stores processed Stripe event IDs in a set and ignores events that have already been processed.
</details>

<details>
<summary>Exercise 5 (Hard): Implement Stripe subscription creation flow</summary>
Write a service creating a customer, attaching a payment method, and creating a recurring `Subscription` for a specific price ID.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test testing webhook signature verification</summary>
Write a Spring MVC test that generates a test HMAC signature and tests that the webhook controller successfully accepts valid signatures and rejects forged ones.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Stripe Payment Integration Summary                      |
+---------------------------------------------------------------------------------+
| SDK Dependency:     com.stripe:stripe-java                                      |
| API Model:          PaymentIntent (Replaces legacy Charge API)                  |
| Money Rule:         Integer minor units only (USD $10.00 = 1000 cents)          |
| Idempotency:        RequestOptions.builder().setIdempotencyKey(key).build()     |
| PCI Compliance:     Stripe Elements in browser -> SAQ-A scope minimization      |
| Webhook Verification: Webhook.constructEvent(rawString, sigHeader, secret)      |
| Invariant:          Never parse JSON before verifying webhook HMAC signature    |
| Order Fulfillment:  Fulfill exclusively in payment_intent.succeeded webhook     |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Stripe PaymentIntents Overview](https://docs.stripe.com/payments/payment-intents)
- [Stripe Java SDK Documentation](https://github.com/stripe/stripe-java)
- [PCI Security Standards Council (SAQ-A Guidelines)](https://www.pcisecuritystandards.org/)
