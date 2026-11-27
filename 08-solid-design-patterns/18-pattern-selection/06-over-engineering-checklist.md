# Over-Engineering Checklist

Before introducing a new interface, factory, or pattern, ask these 6 questions:

1. **Rule of Three**: Do I have 3+ real variations *today*, or am I inventing an interface for 1 implementation?
2. **Language Idiom Check**: Can a 3-line pure function, `record`, or options object replace a 4-class GoF hierarchy?
3. **Deleted-in-1-PR Test**: Does this abstraction make the code *easier* to delete and understand, or harder?
4. **Indirection Tax**: How many files must a new teammate open to trace one request?
5. **Distributed Tax**: Am I adding Kafka/Microservices/Event Sourcing for a problem a single PostgreSQL table + Modular Monolith solves?
6. **Team Familiarity**: Will the on-call engineer at 3 a.m. understand this pattern immediately?

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Pattern Combinations & Composites](file:///E:/Learning/engineering-notes/08-solid-design-patterns/18-pattern-selection/05-pattern-combinations-and-composites.md) | [Module 18](file:///E:/Learning/engineering-notes/08-solid-design-patterns/18-pattern-selection/README.md) | [Module 19: Technical Interview Guides](file:///E:/Learning/engineering-notes/08-solid-design-patterns/19-interview/README.md) |
