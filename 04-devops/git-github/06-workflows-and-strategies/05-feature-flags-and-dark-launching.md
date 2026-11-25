---
title: "Feature Flags, Toggles & Dark Launching"
category: "04-devops"
subcategory: "git-github / 06-workflows-and-strategies"
tags: ["git", "feature-flags", "feature-toggles", "dark-launching", "continuous-deployment", "openfeature"]
difficulty: "Advanced"
estimated_read_time: "24 min"
prerequisites:
  - "06-workflows-and-strategies/01-trunk-based-development"
  - "06-workflows-and-strategies/02-gitflow-and-github-flow"
last_updated: "2026-11-25"
---

# Feature Flags, Toggles & Dark Launching

> **Navigation**: [← Previous: 04-conventional-commits-and-semantic-versioning.md](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/04-conventional-commits-and-semantic-versioning.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/README.md) | [Next: 06-monorepo-vs-polyrepo-git-strategies.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/06-monorepo-vs-polyrepo-git-strategies.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Decoupling Deployment from Release](#2-decoupling-deployment-from-release)
3. [The 4 Categories of Feature Toggles](#3-the-4-categories-of-feature-toggles)
4. [Dark Launching & Shadow Traffic](#4-dark-launching--shadow-traffic)
5. [Canary & Percentage-Based Rollouts](#5-canary--percentage-based-rollouts)
6. [The OpenFeature Standard & Architecture](#6-the-openfeature-standard--architecture)
7. [Technical Debt: The Flag Retirement Lifecycle](#7-technical-debt-the-flag-retirement-lifecycle)
8. [Practical Examples](#8-practical-examples)
9. [Best Practices](#9-best-practices)
10. [Common Mistakes](#10-common-mistakes)
11. [Comparison Matrix: Feature Branching vs Feature Flags](#11-comparison-matrix-feature-branching-vs-feature-flags)
12. [Interview Questions & Answers](#12-interview-questions--answers)
13. [Practice Exercises](#13-practice-exercises)
14. [Summary & Cheat Sheet](#14-summary--cheat-sheet)
15. [Further Reading](#15-further-reading)

---

## 1. What & Why

### What are Feature Flags?
A Feature Flag (or Feature Toggle) is a software engineering technique that wraps new or modified code execution paths inside dynamic decision points. Instead of deciding what code runs at **compile-time** or **deployment-time**, feature flags evaluate conditions dynamically at **runtime** (based on user ID, tenant, percentage, or geographical region).

### Why Do We Use Them?
1. **Enabling Trunk-Based Development**: Developers merge uncompleted features into `main` continuously without breaking production for end users.
2. **Instant Kill Switches**: If a newly released feature triggers CPU spikes or database deadlocks, engineers can disable the feature instantly via an admin toggle (<1 second) without executing a 15-minute CI/CD deployment rollback.
3. **Targeted Testing in Production**: Test new features in live production environments restricted exclusively to internal company employees (`@company.com`).
4. **Zero-Downtime Migration**: Safely migrate databases and cloud providers using dark launching and shadow traffic.

---

## 2. Decoupling Deployment from Release

Historically, "Deployment" and "Release" were treated as identical events:
$$\text{Deployment} \equiv \text{Release}$$

In modern DevOps, they are strictly decoupled:
- **Deployment (Technical)**: Installing binaries, containers, or assets onto production infrastructure. Performed continuously by CI/CD pipelines. Zero business risk.
- **Release (Business)**: Exposing functionality to real end-users. Controlled dynamically by product managers via feature flag toggles.

```
                  Decoupled Deployment vs Release:

[ CI/CD Pipeline ] ──► Deploy v2.4 to Production ──► [ Servers Running Code ]
                                                            │
                                        ┌───────────────────┴───────────────────┐
                                        ▼                                       ▼
                             Internal Staff (Flag: ON)                Real Users (Flag: OFF)
                             • Live Testing in Prod                   • Uses Stable v2.3 Logic
                             • Real Data & Infrastructure             • Zero Disruption
```

---

## 3. The 4 Categories of Feature Toggles

Martin Fowler and Pete Hodgson categorize feature toggles across longevity and dynamicity:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Feature Toggle Taxonomy Matrix                                          │
├──────────────────────┬─────────────┬─────────────┬──────────────────────┤
│ Toggle Category      │ Longevity   │ Dynamicity  │ Typical Purpose      │
├──────────────────────┼─────────────┼─────────────┼──────────────────────┤
│ **Release Toggles**  │ Days/Weeks  │ Static/Per-deploy│ Incomplete TBD code │
│ **Experiment Toggles**│ Weeks/Months│ Highly Dynamic│ A/B conversion tests│
│ **Ops Toggles**      │ Months/Years│ Highly Dynamic│ Circuit breakers/kill│
│ **Permission Toggles**│ Permanent   │ Dynamic/User │ Tiered pricing (Pro) │
└──────────────────────┴─────────────┴─────────────┴──────────────────────┘
```

---

## 4. Dark Launching & Shadow Traffic

### Dark Launching
Deploying backend algorithms, database schema changes, or API endpoints to production without making them visible in the user interface.

### Shadow Traffic (Mirroring)
Production requests are sent to the existing stable service, and a background asynchronous copy is sent to the new dark-launched service:
```
[ Incoming User Request ]
            │
            ├──► (Primary Synchronous)   ──► [ Legacy Payment Service ] ──► 200 OK to User
            │
            └──► (Async Shadow Copy)     ──► [ New V2 Payment Engine ]  ──► Metrics / Logs
```
Allows engineers to stress-test the new engine under real production load and compare output accuracy without risking customer transactions!

---

## 5. Canary & Percentage-Based Rollouts

Instead of flipping a flag from 0% to 100% instantly ("Big Bang Release"), modern teams roll out changes incrementally:

```
Percentage Rollout Trajectory:
Day 1: 1% of users   ──► Monitor APM error rates, latency & logs
Day 2: 10% of users  ──► Validate database connection pool scaling
Day 3: 50% of users  ──► Compare user conversion metrics
Day 4: 100% of users ──► Complete rollout! Retain flag for 1 sprint, then delete
```

---

## 6. The OpenFeature Standard & Architecture

**OpenFeature** (a Cloud Native Computing Foundation standard) provides a vendor-neutral API for feature flagging, allowing applications to switch between flag providers (LaunchDarkly, Unleash, Flagsmith, or local in-memory providers) with zero vendor lock-in.

```typescript
import { OpenFeature } from '@openfeature/server-sdk';

// Initialize client
const client = OpenFeature.getClient();

// Evaluate boolean flag with evaluation context
const showNewCheckout = await client.getBooleanValue(
  'new-checkout-flow',
  false, // Fallback default
  {
    targetingKey: user.id,
    email: user.email,
    plan: user.subscriptionTier,
  }
);

if (showNewCheckout) {
  renderV2Checkout();
} else {
  renderV1Checkout();
}
```

---

## 7. Technical Debt: The Flag Retirement Lifecycle

> **A feature flag is technical debt from the minute it is written.**

Leaving dead toggles in code creates spaghetti logic (`if/else` mazes) and ballooning test matrices ($2^N$ execution paths for $N$ flags).

### Standard 4-Stage Retirement Protocol:
1. **Creation**: Open PR with flag; create a corresponding Jira/GitHub ticket titled `chore: delete flag <FLAG_NAME>`.
2. **Rollout**: Ramp from 1% to 100% in production.
3. **Hardening**: Keep flag at 100% for 1 sprint to guarantee stability.
4. **Purge**: Execute the cleanup ticket: remove the `if/else` block, delete flag from admin console, and deploy.

---

## 8. Practical Examples

### Example 1: Minimal In-Memory Environment Flag
```typescript
export function isFeatureActive(flagName: string): boolean {
  const flags: Record<string, boolean> = {
    ENABLE_AI_RECOMMENDATIONS: process.env.ENABLE_AI === 'true',
    ENABLE_STRIPE_V3: process.env.ENABLE_STRIPE_V3 === 'true',
  };
  return flags[flagName] ?? false;
}
```

### Example 2: Percentage-Based Hashing Algorithm (Deterministic User Bucket)
```typescript
import * as crypto from 'crypto';

export function isUserInPercentage(userId: string, percentage: number): boolean {
  // Deterministically hash userId into integer 0..99
  const hash = crypto.createHash('md5').update(userId).digest('hex');
  const value = parseInt(hash.substring(0, 4), 16) % 100;
  return value < percentage;
}

// 10% rollout: returns true for the EXACT same 10% of users on every request!
if (isUserInPercentage(user.id, 10)) {
  serveV2Feature();
}
```

---

## 9. Best Practices

1. **Always Provide a Safe Fallback Default**: If the feature flag service times out or is unreachable, the SDK must fail gracefully back to the safe, stable implementation.
2. **Keep Flags Short-Lived**: Set an expiration date on release toggles (maximum 30–60 days).
3. **Log Flag Evaluations**: When logging application errors, include active flag evaluation state in structured JSON logs to diagnose which flag variant caused an incident.
4. **Never Put Business Logic Inside Flag Providers**: Flag providers should only return booleans or strings. Keep actual business rules inside your application code.
5. **Enforce Role-Based Access on Flag Toggles**: Restrict production flag toggling permissions to authorized engineers and product leads with audit logging.

---

## 10. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Never deleting flags | Unmaintainable code with hundreds of dead `if/else` branches | No retirement lifecycle | Create flag deletion ticket when creating flag |
| Flag evaluation blocking HTTP requests | Latency spikes on every web request | Calling remote API synchronously | Use local SDK caching with background updates |
| Inverting flag defaults | Outage when flag service is down | Defaulting experimental code to true | Always set fallback default to stable legacy code |
| Testing only with flag enabled | Regressions in fallback code | Missing dual-state test coverage | Write unit tests for both flag=true and flag=false |
| Sensitive data leaked in flag targeting | PII compliance violation (GDPR) | Sending raw passwords/SSNs to SaaS flag vendor | Hash user identifiers before targeting |

---

## 11. Comparison Matrix: Feature Branching vs Feature Flags

| Dimension | Long-Lived Feature Branching | Feature Flags (Trunk-Based) |
|:---|:---|:---|
| **Integration Frequency** | Delayed until feature is 100% complete | **Immediate and continuous** |
| **Merge Conflicts** | Severe ("Merge Hell") | **Near zero** |
| **Production Rollback Speed**| 15–30 minutes (Revert, re-build, re-deploy) | **< 1 second (Instant toggle switch)** |
| **Targeted User Rollouts** | Not possible | **Yes (Internal staff, beta testers, %)** |
| **Code Complexity** | Low initial in-code complexity | Adds runtime conditionals (`if/else`) |
| **Technical Debt Type** | Integration & branching debt | In-code conditional cleanup debt |

---

## 12. Interview Questions & Answers

<details>
<summary><strong>Q1: What does "Decoupling Deployment from Release" mean, and how do Feature Flags make it possible?</strong></summary>

Deployment is the technical process of installing code binaries or containers onto production servers. Release is the business decision to expose that functionality to real users.
Without feature flags, deployment and release happen simultaneously. With feature flags, code can be deployed to production servers continuously (even several times a day) while the code paths remain dormant behind toggles. The feature is "released" later by flipping a toggle in an admin dashboard without triggering a build or deployment pipeline.
</details>

<details>
<summary><strong>Q2: What is an "Ops Toggle" (Kill Switch) and how does it improve system reliability?</strong></summary>

An Ops Toggle is a long-lived feature flag used to control operational aspects of a system. In high-traffic scenarios or database brownouts, engineers can flip an Ops Toggle to disable non-critical, compute-intensive functionality (e.g. disabling recommendation engines, complex search filters, or animated widgets). This sheds load instantly without redeploying code, preserving core checkout and login capabilities.
</details>

<details>
<summary><strong>Q3: How do percentage-based rollouts ensure that a specific user consistently sees the same feature variant?</strong></summary>

Percentage rollouts use deterministic hashing algorithms (e.g. MD5 or MurmurHash3). The user's unique identifier (e.g. `userId`) combined with the flag name is hashed into a numerical value and modulo-divided by 100 to yield a bucket between 0 and 99. If the flag rollout is set to 25%, users whose hash yields 0–24 see the feature. Because the hash is pure and deterministic, the same user will consistently fall into the same bucket on every request without requiring database lookups.
</details>

<details>
<summary><strong>Q4: Why are Feature Flags considered technical debt, and how should an engineering team manage flag retirement?</strong></summary>

Feature flags introduce branching logic (`if/else`) into source code. If flags are abandoned after 100% rollout, the codebase becomes cluttered with dead code paths, confusing future maintainers and exponentially multiplying the combinatorial test matrix ($2^N$ paths).
Teams manage retirement by treating every flag as temporary: when a flag is created, a cleanup ticket is scheduled in the sprint backlog. Once the feature is at 100% for one release cycle, the toggle is removed from code and the flag is archived in the flag management system.
</details>

<details>
<summary><strong>Q5: What is "Dark Launching"?</strong></summary>

Dark Launching is deploying new backend features to production without exposing any user-facing UI elements. Real production traffic or shadow traffic is routed through the new backend algorithms or database queries in the background. This allows engineers to assess memory consumption, response latency, database index efficiency, and error rates under real-world load before unveiling the feature to customers.
</details>

<details>
<summary><strong>Q6: How do modern Feature Flag SDKs prevent network latency from slowing down user requests?</strong></summary>

Modern SDKs (like LaunchDarkly, Unleash, or OpenFeature providers) never make synchronous HTTP requests during flag evaluation. Instead, they establish a background persistent connection (Server-Sent Events / WebSockets) to the flag server. Flag evaluation rules are cached entirely in local server memory. Evaluating `client.getBooleanValue()` takes microseconds ($\mu s$) because it executes strictly in-memory without network I/O.
</details>

<details>
<summary><strong>Q7: What is the OpenFeature standard?</strong></summary>

OpenFeature is an open-source, vendor-agnostic specification and SDK hosted by the Cloud Native Computing Foundation (CNCF). It provides a standardized API for feature evaluation across programming languages. This allows organizations to write flag-evaluation code against a universal interface, enabling them to switch flag providers (or use internal in-memory flags in unit tests) without refactoring application code.
</details>

<details>
<summary><strong>Q8: What is the risk of using database columns directly for feature toggles?</strong></summary>

Querying a database table (e.g. `SELECT is_enabled FROM flags WHERE name = 'x'`) on every request adds massive database query overhead, latency, and creates a single point of failure. Modern feature flagging uses in-memory cached rules with streaming updates.
</details>

---

## 13. Practice Exercises

<details>
<summary><strong>Exercise 1: Implement Deterministic User Hash Bucket</strong></summary>

**Scenario**: Write a Node.js function that consistently places user into 0-99 bucket.
```javascript
const crypto = require('crypto');
function getBucket(userId) {
  const hash = crypto.createHash('md5').update(userId).digest('hex');
  return parseInt(hash.substring(0, 4), 16) % 100;
}
console.log("Bucket for user-1:", getBucket("user-1"));
```
</details>

<details>
<summary><strong>Exercise 2: Write a Unit Test for Both Flag Branches</strong></summary>

**Scenario**: Test both true and false paths of a flagged service.
```typescript
it('executes V2 when flag is true', () => {
  const service = new PaymentService({ enableV2: true });
  expect(service.process()).toBe('V2_PROCESSED');
});

it('executes V1 when flag is false', () => {
  const service = new PaymentService({ enableV2: false });
  expect(service.process()).toBe('V1_PROCESSED');
});
```
</details>

<details>
<summary><strong>Exercise 3: Simulate Instant Kill-Switch</strong></summary>

**Scenario**: Demonstrate dynamic toggle disable without restart.
```javascript
let flags = { PAYMENT_V2: true };
// Simulate kill-switch toggle:
flags.PAYMENT_V2 = false;
```
</details>

<details>
<summary><strong>Exercise 4: Implement OpenFeature In-Memory Provider</strong></summary>

**Scenario**: Configure OpenFeature with static in-memory provider.
```javascript
const { OpenFeature, InMemoryProvider } = require('@openfeature/server-sdk');
const provider = new InMemoryProvider({ 'dark-mode': true });
OpenFeature.setProvider(provider);
```
</details>

<details>
<summary><strong>Exercise 5: Add Structured Flag Evaluation Logging</strong></summary>

**Scenario**: Include active flags in log payload.
```javascript
console.log(JSON.stringify({
  event: "checkout_failed",
  userId: "123",
  activeFlags: { NEW_CHECKOUT: true }
}));
```
</details>

<details>
<summary><strong>Exercise 6: Draft a Flag Retirement PR</strong></summary>

**Scenario**: Remove `if/else` toggle block and retain only V2 logic.
```javascript
// Before: if (flag) { v2(); } else { v1(); }
// After:
v2();
```
</details>

---

## 14. Summary & Cheat Sheet

```text
Feature Flag Lifecycle:
1. CREATE: Add flag behind interface + Open deletion ticket
2. DEPLOY: Push to trunk continuously (Flag = OFF for public)
3. DARK LAUNCH: Test in prod with internal staff (Flag = ON for @company.com)
4. RAMP UP: 1% -> 10% -> 50% -> 100% rollout
5. PURGE: Delete if/else conditional and retire flag within 30 days!

Rule of Thumb:
- Release Toggles: Temporary (delete fast).
- Ops Toggles: Permanent kill switches for high-risk dependencies.
```

---

## 15. Further Reading
- [Martin Fowler - Feature Toggles (Feature Flags)](https://martinfowler.com/articles/feature-toggles.html)
- [OpenFeature CNCF Specification](https://openfeature.dev/)
- [Pete Hodgson - Exploring Feature Toggles](https://www.youtube.com/watch?v=R32A_jVdK5w)
- [06-workflows-and-strategies/06-monorepo-vs-polyrepo-git-strategies.md](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/06-monorepo-vs-polyrepo-git-strategies.md)
