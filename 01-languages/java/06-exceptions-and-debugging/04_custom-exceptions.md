# Custom Exceptions in Java: Domain-Driven Error Modeling

> An authoritative guide to designing domain-specific custom exceptions, the 4 standard constructor recipe, checked vs unchecked decision matrices, and error payload modeling.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_exception-hierarchy-and-types.md](./00_exception-hierarchy-and-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Java 25 LTS verified) |

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
9. [Comparison: Custom vs Standard JDK Exceptions](#9-comparison-custom-vs-standard-jdk-exceptions)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

The Java Standard Library provides dozens of general-purpose exception classes (`IllegalArgumentException`, `IllegalStateException`, `NoSuchElementException`, `UnsupportedOperationException`). While reusing standard exceptions is recommended for generic programming errors (Effective Java Item 72), complex business applications require **Custom Domain Exceptions**.

Custom exceptions allow an application to:
- Convey rich domain context (e.g. `InsufficientFundsException` carrying `availableBalance` and `attemptedWithdrawal`).
- Differentiate business errors from raw technical bugs in telemetry and alerting dashboards.
- Allow API clients to catch and recover from specific business failures without parsing fragile error strings.

---

## 2. Core Concepts

### 2.1 Checked vs Unchecked Custom Exception Decision Tree
```
                         Should this failure be handled?
                                       │
                ┌──────────────────────┴──────────────────────┐
                ▼                                             ▼
     Can caller reasonably recover?                Is it a programming bug,
     (e.g., Retry, Alternative path)                invalid argument, or bad state?
                │                                             │
                ▼                                             ▼
      CHECKED EXCEPTION                             UNCHECKED EXCEPTION
   (extends java.lang.Exception)               (extends java.lang.RuntimeException)
```

In modern cloud-native architectures (Spring Boot, REST APIs, Microservices), **over 95% of custom exceptions should extend `RuntimeException`**. Checked exceptions do not scale well through functional pipelines (Streams, Lambdas) and force intermediate service layers to declare unhelpful checked exception signatures.

---

## 3. How It Works Internally

### 3.1 The Canonical 4-Constructor Recipe
Every well-designed exception class in Java should provide the four standard constructors defined by `java.lang.Throwable`:
1. **No-arg:** `public CustomException()`
2. **Message-only:** `public CustomException(String message)`
3. **Message and Cause:** `public CustomException(String message, Throwable cause)`
4. **Cause-only:** `public CustomException(Throwable cause)`

Providing all four constructors guarantees that your custom exception integrates seamlessly with third-party logging frameworks, serialization engines, and exception translation pipelines.

---

## 4. Syntax & Usage

### 4.1 Rich Domain Exception with Typed Properties
```java
public class InsufficientFundsException extends RuntimeException {
    private final String accountId;
    private final double currentBalance;
    private final double attemptedAmount;

    // Standard 4 Constructors + Domain Constructor
    public InsufficientFundsException(String accountId, double currentBalance, double attemptedAmount) {
        super(String.format("Account %s has balance $%.2f; attempted withdrawal of $%.2f",
            accountId, currentBalance, attemptedAmount));
        this.accountId = accountId;
        this.currentBalance = currentBalance;
        this.attemptedAmount = attemptedAmount;
    }

    public String getAccountId() { return accountId; }
    public double getCurrentBalance() { return currentBalance; }
    public double getAttemptedAmount() { return attemptedAmount; }
}
```

---

## 5. Practical Examples

### Example 1: Full Implementation of Canonical 4 Constructors
```java
public class OrderProcessingException extends RuntimeException {
    public OrderProcessingException() {
        super();
    }

    public OrderProcessingException(String message) {
        super(message);
    }

    public OrderProcessingException(String message, Throwable cause) {
        super(message, cause);
    }

    public OrderProcessingException(Throwable cause) {
        super(cause);
    }

    public static void main(String[] args) {
        var ex1 = new OrderProcessingException("Payment gateway timeout");
        var ex2 = new OrderProcessingException("Inventory lock failed", new IllegalStateException("Out of stock"));

        System.out.println("Ex 1 Message: " + ex1.getMessage());
        System.out.println("Ex 2 Cause:   " + ex2.getCause().getMessage());
    }
}
```
**Output:**
```text
Ex 1 Message: Payment gateway timeout
Ex 2 Cause:   Out of stock
```

### Example 2: Real-World REST Error Payload Exception
```java
import java.time.Instant;

public class ApiProblemException extends RuntimeException {
    private final int httpStatus;
    private final String errorCode;
    private final Instant timestamp;

    public ApiProblemException(int httpStatus, String errorCode, String detail) {
        super(detail);
        this.httpStatus = httpStatus;
        this.errorCode = errorCode;
        this.timestamp = Instant.now();
    }

    public int getHttpStatus() { return httpStatus; }
    public String getErrorCode() { return errorCode; }
    public Instant getTimestamp() { return timestamp; }

    public static void main(String[] args) {
        try {
            throw new ApiProblemException(404, "USER_NOT_FOUND", "No user with ID 8912");
        } catch (ApiProblemException e) {
            System.out.printf("[%s] HTTP %d: %s (at %s)%n",
                e.getErrorCode(), e.getHttpStatus(), e.getMessage(), e.getTimestamp());
        }
    }
}
```
**Output:**
```text
[USER_NOT_FOUND] HTTP 404: No user with ID 8912 (at ...)
```

### Example 3: Edge Case – Hierarchical Domain Exceptions
```java
// Base abstract domain exception for entire billing subsystem
public abstract class BillingException extends RuntimeException {
    private final String billingTransactionId;
    public BillingException(String txId, String message) {
        super(message);
        this.billingTransactionId = txId;
    }
    public String getBillingTransactionId() { return billingTransactionId; }
}

public class CardExpiredException extends BillingException {
    public CardExpiredException(String txId) {
        super(txId, "Credit card on file has expired");
    }
}

public class CardDeclinedException extends BillingException {
    public CardDeclinedException(String txId, String reason) {
        super(txId, "Card declined: " + reason);
    }
}
```
*Benefit: Callers can catch specific `CardExpiredException` to prompt the user, or catch general `BillingException` to abort the transaction!*

### Example 4: Failure Case – Stringly-Typed Exceptions
```java
public class StringlyTypedAntiPattern {
    public static void badCaller() {
        try {
            // ANTI-PATTERN: Throwing generic exception with structured data jammed into a string!
            throw new RuntimeException("ERROR:CODE=502:RETRY=TRUE:USER=101");
        } catch (RuntimeException ex) {
            // DANGEROUS: Caller must parse string with regex to make business decisions!
            if (ex.getMessage().contains("RETRY=TRUE")) {
                System.out.println("Retrying based on fragile string parsing...");
            }
        }
    }

    public static void main(String[] args) {
        badCaller();
    }
}
```
**Output:**
```text
Retrying based on fragile string parsing...
```
*Fix: Store `isRetryable()` as an explicit boolean getter on a custom exception.*

### Example 5: Optimized / Idiomatic – Pattern Matching over Custom Exceptions (Java 21+)
```java
public class ExceptionPatternMatchingDemo {
    public static void handleFailure(BillingException ex) {
        // Java 21+ pattern matching switch on domain exceptions!
        String action = switch (ex) {
            case CardExpiredException c -> "Notify user to update expiration date for TX: " + c.getBillingTransactionId();
            case CardDeclinedException d -> "Alert fraud detection team for TX: " + d.getBillingTransactionId();
            default -> "Log generic billing alert: " + ex.getMessage();
        };

        System.out.println("Action: " + action);
    }

    public static void main(String[] args) {
        handleFailure(new CardExpiredException("TX-9901"));
        handleFailure(new CardDeclinedException("TX-9902", "Suspected Fraud"));
    }
}
```
**Output:**
```text
Action: Notify user to update expiration date for TX: TX-9901
Action: Alert fraud detection team for TX: TX-9902
```

---

## 6. Best Practices

1. **Favor Standard Exceptions When Applicable:** Re-use `IllegalArgumentException`, `IllegalStateException`, `NullPointerException`, or `UnsupportedOperationException` if domain context is not required (Effective Java Item 72).
2. **Provide Rich Diagnostic Context:** Include IDs, parameter values, thresholds, and timestamps as typed getter fields rather than formatting them only into the message string.
3. **Always Implement All 4 Canonical Constructors:** Ensure subclasses of `Exception` or `RuntimeException` provide constructors with `(message)`, `(cause)`, and `(message, cause)`.
4. **Extend `RuntimeException` by Default:** Unless callers are strictly expected to recover programmatically, create unchecked exceptions to maintain compatibility with Streams, Lambdas, and clean architecture.
5. **Group Exceptions into a Logical Subsystem Hierarchy:** Create a common abstract base class (`BillingException`) for related domain exceptions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Over-creating custom exceptions | Proliferates hundreds of redundant exception classes that duplicate standard JDK types. | Re-use standard JDK exceptions where appropriate. |
| Missing cause constructor | Prevents chaining root causes when wrapping technical exceptions. | Implement `CustomException(String msg, Throwable cause)`. |
| "Stringly-typed" exceptions | Encoding metadata inside error strings forces callers to parse messages with regex. | Provide typed getter methods (`getId()`, `getCode()`). |
| Checked exceptions in Lambda pipelines | Cannot be thrown from standard Java 8 functional interfaces (`Function`, `Consumer`). | Extend `RuntimeException`. |
| Modifying custom exception state | Exception objects should be immutable diagnostics. | Make all custom fields `final`. |

---

## 8. Performance & Security Notes

### 8.1 Serialization of Custom Exceptions
Because `java.lang.Throwable` implements `java.io.Serializable`, every custom exception class is serializable. If an exception contains custom fields containing non-serializable objects (like open sockets or database connections), mark those fields `transient` to prevent `NotSerializableException` during RMI or distributed logging serialization.

---

## 9. Comparison: Custom vs Standard JDK Exceptions

| Scenario | Re-use Standard JDK Exception | Create Custom Domain Exception |
| :--- | :--- | :--- |
| **Null Parameter** | ✅ `NullPointerException` | ❌ Redundant |
| **Out-of-Range Number**| ✅ `IllegalArgumentException` | ❌ Redundant |
| **Object in Wrong State**| ✅ `IllegalStateException` | ❌ Redundant |
| **Insufficient Funds** | ❌ Insufficient context | ✅ `InsufficientFundsException` |
| **User Not Found in DB**| ❌ Too generic | ✅ `UserNotFoundException` |
| **API Rate Limit Exceeded**| ❌ Too generic | ✅ `RateLimitExceededException` |

---

## 10. Interview Questions

<details>
<summary><strong>1. When should you create a custom exception instead of reusing a standard JDK exception?</strong></summary>

Create a custom exception when:
1. The domain failure carries rich business context (e.g. account numbers, retry intervals) that callers need to inspect programmatically.
2. Callers need to handle this specific business error distinctly from other errors.
3. You are designing a public API or library and want errors clearly categorized under your library's namespace.
Re-use standard JDK exceptions (`IllegalArgumentException`, `IllegalStateException`) for generic programming errors.
</details>

<details>
<summary><strong>2. Why should custom exceptions generally extend RuntimeException in modern Java?</strong></summary>

Checked exceptions do not integrate smoothly with Java 8+ functional interfaces (Streams, Lambdas), which do not declare checked exceptions in their method signatures. Furthermore, in microservice architectures, checked exceptions force every intermediate layer to pollute method signatures with unhelpful `throws` clauses, whereas unchecked exceptions bubble up cleanly to centralized exception handlers (`@ControllerAdvice`).
</details>

<details>
<summary><strong>3. What are the four canonical constructors every custom exception should provide?</strong></summary>

1. `CustomException()` (no-arg)
2. `CustomException(String message)`
3. `CustomException(String message, Throwable cause)`
4. `CustomException(Throwable cause)`
</details>

<details>
<summary><strong>4. Why should all custom fields on an exception be marked final?</strong></summary>

An exception represents an immutable historical record of an event that failed in the past. Allowing fields to be modified after creation could allow race conditions or inaccurate diagnostic data in logs and telemetry dashboards.
</details>

<details>
<summary><strong>5. What is the naming convention for custom exceptions in Java?</strong></summary>

Custom exception class names should always end with the suffix `Exception` (e.g. `PaymentFailedException`, `ResourceNotFoundException`). Custom errors should end with `Error`, though user-defined errors are strongly discouraged.
</details>

<details>
<summary><strong>6. How can custom exceptions support internationalization (i18n)?</strong></summary>

Store an error code or message bundle key (e.g. `error.insufficient.funds`) and parameter arguments inside the custom exception, rather than hardcoding a localized English string in the message. The presentation layer can look up the localized text using `ResourceBundle` based on the user's locale.
</details>

<details>
<summary><strong>7. Why should non-serializable fields in custom exceptions be marked transient?</strong></summary>

Because `Throwable` implements `Serializable`. If custom exceptions are transmitted across network boundaries (RMI, remote logging, distributed caching), any non-serializable fields will trigger a `java.io.NotSerializableException`.
</details>

<details>
<summary><strong>8. What is the benefit of defining an abstract base custom exception for a module?</strong></summary>

An abstract base exception (e.g. `abstract class PaymentException extends RuntimeException`) allows API consumers to write a single catch block (`catch (PaymentException e)`) to catch all payment-related errors, while still permitting fine-grained catch blocks for specific subtypes (`CardExpiredException`).
</details>

---

## 11. Practice Exercises

### Easy
1. **Invalid Age Exception:** Create an unchecked custom `InvalidAgeException` with the 4 canonical constructors.
2. **Entity Not Found Exception:** Create `EntityNotFoundException` taking `entityName` and `entityId`.
3. **Throw and Catch Custom:** Write a test verifying custom exception message and cause.

### Medium
4. **Rich Balance Exception:** Implement `InsufficientBalanceException` with typed fields for `balance` and `attempted`, validating getters.
5. **Subsystem Exception Hierarchy:** Design an exception hierarchy for an E-Commerce Cart subsystem with 1 base class and 3 subclasses.
6. **I18n Custom Exception:** Build an exception storing message key and arguments, rendering localized text using `MessageFormat`.

### Hard
7. **Pattern Matching Exception Dispatcher:** Write an exception processing gateway using Java 21 switch pattern matching over a sealed exception hierarchy.
8. **Serialized Exception Verification:** Write a test serializing and deserializing a custom exception containing transient and non-transient fields.
9. **Zero-Allocation Custom Exception:** Create an ultra-low-latency custom exception with stack traces disabled and pre-cached singleton instances.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 2: Entity Not Found Exception
public class EntityNotFoundException extends RuntimeException {
    private final String entityName;
    private final Object entityId;

    public EntityNotFoundException(String entityName, Object entityId) {
        super(String.format("%s with ID '%s' was not found", entityName, entityId));
        this.entityName = entityName;
        this.entityId = entityId;
    }

    public String getEntityName() { return entityName; }
    public Object getEntityId() { return entityId; }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                      CUSTOM EXCEPTIONS CHEAT SHEET                     │
├────────────────────────────────────────────────────────────────────────┤
│ Rule of Thumb:      Extend RuntimeException for 95%+ of custom errors  │
│ 4 Constructors:     Provide (), (msg), (msg, cause), (cause)           │
│ Rich Properties:    Add final fields with getters for typed telemetry  │
│ Naming:             Always suffix with Exception (e.g. OrderException) │
│ Re-use Standard:    Prefer IllegalArgumentException for generic bugs   │
│ Hierarchy:          Group related exceptions under abstract base class │
│ Modern Java 21:     Use pattern matching for switch over custom errors │
│ Serialization:      Mark non-serializable fields transient             │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- *Effective Java (3rd Edition)*, Joshua Bloch:
  - Item 70: "Use checked exceptions for recoverable conditions and runtime exceptions for programming errors".
  - Item 72: "Favor the use of standard exceptions".
  - Item 75: "Include failure-capture information in detail messages".
