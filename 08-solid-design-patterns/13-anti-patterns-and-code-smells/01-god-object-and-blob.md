# God Object & Blob

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Class / Module Bloat) |
| **Violates** | [Single Responsibility Principle](file:///E:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/01-single-responsibility-principle.md) |
| **Fix** | Extract Class, Vertical-Slice Use Cases, Domain Entities |

> **TL;DR**: A **God Object** (or **Blob**) is a single 2,000+ line class (`AppManager`, `OrderService`, `UserController`) that knows and does everything—causing constant merge conflicts, untestable dependencies, and Divergent Change.

## Symptoms
- Class names ending in `Manager`, `Processor`, `Controller`, `Helper`, `Util` with 30+ methods and 15+ injected dependencies.
- Every sprint touches the same file for unrelated reasons (UI, SQL, email, PDF).

## Before (Broken) vs After (Fixed)
```java
// ❌ God Object: 15 dependencies, handles validation, DB, Stripe, Email, PDF
class OrderManager { void checkout() {} void refund() {} void generatePdf() {} void sendEmail() {} }

// ✅ Extracted cohesive collaborators
class OrderPricingService { /* pure pricing */ }
class InvoicePdfGenerator { /* PDF only */ }
class OrderNotificationHandler { /* reacts to OrderPlaced */ }
```

## Interview Q&A
<details><summary>How do you break up a 3,000-line God Object safely?</summary>
Group methods by which fields they actually touch (cohesion clusters), write Characterization Tests around the public methods, and use **Extract Class** one cohesive responsibility at a time.
</details>
