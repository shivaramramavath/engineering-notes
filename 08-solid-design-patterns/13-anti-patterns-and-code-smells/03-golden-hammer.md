# Golden Hammer

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Engineering Mindset) |
| **Origin** | Abraham Maslow (*"If all you have is a hammer, everything looks like a nail"*) |
| **Fix** | Problem-First Pattern Selection, [KISS / YAGNI](file:///E:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/02-kiss-principle.md) |

> **TL;DR**: **Golden Hammer** is forcing one familiar pattern, framework, or database onto every problem—using Microservices for a 2-person CRUD app, Redis as a primary ACID ledger, or GoF Visitor in Python where a 5-line function works.

## Examples
- Using **Kafka** for a 10-req/min cron job.
- Using **MongoDB** for heavily relational financial accounting.
- Using **Singleton** or **AbstractFactory** everywhere because it's the only pattern you memorized.

## Fix
Always start **problem-first** ("what forces exist here?") and compare at least 2 alternatives in an ADR before picking a tool.
