# Feature Flags & Dark Launch

| Field | Value |
| :--- | :--- |
| **Category** | Distributed & Cloud Patterns (Progressive Delivery) |
| **Origin** | Pete Hodgson (*Feature Toggles*) / Continuous Delivery |
| **Difficulty** | Beginner → Intermediate |
| **Prerequisite(s)** | [Strategy](file:///E:/Learning/engineering-notes/08-solid-design-patterns/07-behavioral-patterns/01-strategy.md) |
| **Status** | Complete |

> **TL;DR**: **Feature Flags** decouple **code deployment** from **feature release** (`if (flags.isEnabled("new-checkout", user))`), letting you ship unfinished code safely behind a toggle, roll out to 1% of users, or kill-switch a buggy feature in milliseconds. **Dark Launching** runs the new code path silently in production (shadow traffic) without showing results to users.

---

## 1. Types of Feature Flags

1. **Release Toggles** (short-lived, days–weeks): Hide incomplete work in trunk-based development; **delete the flag** once 100% rolled out!
2. **Ops / Kill Switches** (long-lived): Disable expensive features (e.g., AI recommendations) during traffic spikes.
3. **Experiment / A/B Toggles**: Route 50% of users to variant A vs B for metrics.
4. **Permissioning Toggles**: Enable enterprise features for paid tiers.

---

## 2. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Leaving 200 dead release flags in code forever | Combinatorial `if/else` explosion & dead code | Set **expiration dates** and delete release flags after launch |
| Scattering `if (flag)` across 15 files for one feature | Tangled conditional logic | Wrap at **one Decision Point** (Strategy/Factory) |

---

## 3. Interview Questions

<details>
<summary>1. What does "decoupling deployment from release" mean?</summary>

Deploying code to production servers (dark/disabled behind a flag) is an engineering action; releasing/enabling the feature for users is a separate product decision toggled without redeploying.

</details>

<details>
<summary>2. What is a Dark Launch?</summary>

Deploying and invoking a new backend capability silently on real production traffic (often asynchronously or shadowing) to measure latency, errors, and parity before exposing it in the UI.

</details>

---

## 4. Cheat Sheet

- **1 decision point** per flag; **clean up** release flags after 100% rollout.
- **Dark Launch** = test new backend under real load invisibly.

---

## 5. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Health Check & Graceful Degradation](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/18-health-check-and-graceful-degradation.md) | [Module 11](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [Blue–Green & Canary Deployments](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/20-blue-green-and-canary.md) |
