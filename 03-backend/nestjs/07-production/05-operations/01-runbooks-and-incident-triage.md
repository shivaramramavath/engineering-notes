---
title: "Runbooks and Incident Triage for NestJS Services"
description: "Authoring actionable engineering runbooks, severity classification, on-call triage workflows, and automated diagnostic scripts"
tags: [nestjs, operations, runbooks, incident-response, triage, on-call]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Runbooks and Incident Triage for NestJS Services

## Conceptual Overview
When an automated alert fires at 3:00 AM (e.g. error rate $> 5\%$ or P99 latency $> 2\text{s}$), on-call engineers need unambiguous, actionable procedures rather than guesswork. An **Engineering Runbook** provides step-by-step diagnostic workflows, mitigation commands, and escalation trees for critical service failures.

---

## Standard Production Triage Workflow

```
+-------------------------------------------------------------------------------+
|                       On-Call Incident Triage Decision Tree                   |
|                                                                               |
|  [ PagerDuty Alert Triggered ]                                                |
|               |                                                               |
|               v                                                               |
|  Is HTTP Error Rate > 5%?                                                     |
|       |                                                                       |
|       +--- YES ---> Check Grafana / Sentry for top failing route & status code|
|       |             - 502/503: Check if pods crash-looping (OOMKilled)        |
|       |             - 500: Check Sentry for unhandled database/type errors    |
|       |             - 504: Check downstream dependency latency (Stripe, DB)   |
|       |                                                                       |
|       +--- NO  ---> Is P99 Latency > 2s?                                      |
|                     |                                                         |
|                     +---> Check Event Loop Lag histogram in Prometheus        |
|                           - Lag > 100ms: CPU thrashing or sync blocking task  |
|                           - Lag normal (<20ms): Database lock contention      |
+-------------------------------------------------------------------------------+
```

---

## Key Takeaways & Cheat Sheet

- Keep runbooks concise, versioned alongside code, and tested during game days.
- Prioritize customer mitigation (reverting release, restarting pods, enabling circuit breakers) over finding root cause during live incidents.
- Maintain automated diagnostic scripts for rapid state capture.

---

## Cross-References
- [04-incident-post-mortems.md](./04-incident-post-mortems.md) - Writing blameless post-mortems
- [07-error-tracking-and-alerting.md](../03-observability/07-error-tracking-and-alerting.md) - Sentry alerting
