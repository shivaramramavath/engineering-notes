---
title: "Incident Post-Mortems and Blameless Culture"
description: "Writing actionable, blameless post-mortems, identifying systemic root causes (Five Whys), and tracking corrective action items"
tags: [nestjs, operations, post-mortem, blameless, reliability, incident-review]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Incident Post-Mortems and Blameless Culture

## Conceptual Overview
An incident is an opportunity to learn about systemic weaknesses before they cause catastrophic business impact. A **Blameless Post-Mortem** assumes that engineers act in good faith with the information available to them at the time; failures stem from flawed processes, inadequate automated checks, or missing safeguards.

---

## Post-Mortem Template Structure

1. **Incident Summary**: Date, severity, duration, customer impact (e.g. 14,200 requests dropped).
2. **Timeline (UTC)**: Chronological sequence of alert triggers, triage steps, and remediation.
3. **Root Cause Analysis (5 Whys)**: Progressive questioning to identify systemic architectural or process deficiencies.
4. **Where We Got Lucky**: What safety nets worked (e.g. circuit breakers prevented cascading failure).
5. **Action Items (Prevent / Detect / Mitigate)**: Concrete engineering tickets with assigned owners and deadlines.

---

## Key Takeaways & Cheat Sheet

- Conduct post-mortems within 48 hours of major incidents.
- Focus on system improvements, test coverage, and guardrails rather than individual blame.
- Track action items in project management tools with high priority.

---

## Cross-References
- [01-runbooks-and-incident-triage.md](./01-runbooks-and-incident-triage.md) - Incident triage
- [05-dynamic-configuration-rotation.md](./05-dynamic-configuration-rotation.md) - Config rotation
