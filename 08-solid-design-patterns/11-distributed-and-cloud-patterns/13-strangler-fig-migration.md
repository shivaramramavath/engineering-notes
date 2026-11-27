# Strangler Fig Migration

| Field | Value |
| :--- | :--- |
| **Category** | Distributed & Cloud Patterns (Legacy Modernization) |
| **Origin** | Martin Fowler (2004, Australian Strangler Fig Tree) |
| **Difficulty** | Intermediate |
| **Prerequisite(s)** | [API Gateway](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/11-api-gateway-and-bff.md), [Evolutionary Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/14-evolutionary-architecture.md) |
| **Status** | Complete |

> **TL;DR**: Instead of a risky "Big Bang Rewrite", the **Strangler Fig** places a routing proxy in front of a legacy system and migrates one route/capability at a time to new services—delivering value every week until the legacy system shrinks to zero and is retired.

---

## 1. Three-Step Strangler Fig Playbook

1. **Intercept**: Put an API Gateway / Reverse Proxy (Nginx, Envoy, Cloudflare) in front of the legacy monolith.
2. **Coexist & Shift**: Build one new capability (e.g., `/api/v2/invoices`), sync any needed data (via CDC/Outbox), and route traffic via the proxy—often using **Dark Launch / Shadow Traffic** first to compare outputs.
3. **Eliminate**: Once 100% of routes move to new services, decommission the legacy app.

---

## 2. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Letting new services write directly to legacy's private tables | Keeps both coupled forever | Sync via **CDC / Outbox** or an **Anti-Corruption Layer** |
| Stopping halfway ("two permanent monoliths") | Ops cost doubles forever | Set explicit deprecation milestones per slice |

---

## 3. Interview Questions

<details>
<summary>1. Why is Strangler Fig safer than a Big Bang Rewrite?</summary>

It delivers production value incrementally, allows instant rollback at the proxy per route, and avoids freezing feature work for 18 months.

</details>

<details>
<summary>2. How do you verify a strangled route produces the exact same result as legacy?</summary>

Use **Shadow / Dark Traffic** (mirror requests asynchronously to the new service and diff responses) before switching live user traffic.

</details>

---

## 4. Cheat Sheet

- **Intercept (Proxy) → Migrate Slice-by-Slice → Retire Legacy**.
- Pair with **ACL** + **CDC** + **Shadow Traffic**.

---

## 5. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Sidecar & Ambassador](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/12-sidecar-and-ambassador.md) | [Module 11](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [Anti-Corruption Layer (Distributed)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/14-anti-corruption-layer.md) |
