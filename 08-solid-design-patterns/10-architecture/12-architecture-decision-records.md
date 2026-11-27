# Architecture Decision Records (ADRs)

| Field | Value |
| :--- | :--- |
| **Category** | Software Architecture (Governance & Documentation) |
| **Origin** | Michael Nygard (2011, *"Documenting Architecture Decisions"*) |
| **Difficulty** | Beginner |
| **Prerequisite(s)** | None |
| **Status** | Complete |

> **TL;DR**: An ADR is a short, immutable Markdown file stored in Git (`docs/adr/0004-use-modular-monolith.md`) capturing **Context, Decision, Alternatives Considered, and Consequences**—preserving *why* a hard-to-reverse choice was made long after the original authors move on.

---

## 1. The Problem: Why Do ADRs Exist?

Two years after a system ships, a new engineer asks: *"Why on earth did we choose PostgreSQL + Outbox instead of Kafka Streams here?"* Without ADRs, the reasoning is lost in forgotten Slack threads or deleted Confluence pages—so teams either fear changing anything or blindly repeat the exact mistake the original team avoided.

---

## 2. Standard ADR Template (MADR / Nygard)

```markdown
# ADR-0007: Use Optimistic Locking for Inventory Updates

- **Status**: Accepted (2026-03-10)
- **Deciders**: Core Platform Team

## Context
Concurrent checkout requests occasionally overwrite stock counts on popular SKUs.
Average contention per SKU is low (< 2% conflicts), except during rare flash sales.

## Decision
Add a `version` integer column to `inventory` and enforce `WHERE id = ? AND version = ?`.

## Alternatives Considered
1. `SELECT ... FOR UPDATE` (pessimistic): Rejected for normal checkout due to lock-wait latency across pools.
2. Distributed Redis lock: Rejected to avoid split-brain complexity with Postgres.

## Consequences
- **Positive**: Zero lock contention on 98% of reads/writes; simple SQL.
- **Negative**: Callers must handle `409 Conflict` / bounded retries (max 3).
```

---

## 3. Rules for Healthy ADRs

1. **Keep them in Git** right next to the code (`docs/adr/`), reviewed via Pull Request.
2. **Immutable once Accepted**: Never rewrite history—if a decision changes later, write `ADR-0019` marked `Supersedes ADR-0007`.
3. **1–2 Pages Max**: Focus on **why** and **trade-offs**, not implementation tutorials.

---

## 4. Interview Questions

<details>
<summary>1. What four sections belong in every ADR?</summary>

**Context** (the forces/problem), **Decision** (what was chosen), **Alternatives Considered**, and **Consequences** (both positive and negative trade-offs).

</details>

<details>
<summary>2. Why should ADRs be immutable and stored in Git?</summary>

Storing them in Git ties architectural history to code PRs; immutability (`Superseded by ADR-X`) preserves the historical context of *why* the team made that choice at that time.

</details>

<details>
<summary>3. When should you write an ADR vs a code comment?</summary>

Write a code comment for local *why* inside a function; write an ADR for cross-cutting, hard-to-reverse decisions (DB choice, auth protocol, monolith vs microservices, sync vs async).

</details>

---

## 5. Cheat Sheet

- **Location**: `docs/adr/NNNN-short-title.md` in Git.
- **Sections**: Status, Context, Decision, Alternatives, Consequences.
- **Never delete/edit accepted ADRs** — supersede with a new ADR.

---

## 6. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Space-Based & Cell-Based Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/11-space-based-and-cell-based.md) | [Module 10](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/README.md) | [Architecture Fitness Functions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/13-architecture-fitness-functions.md) |
