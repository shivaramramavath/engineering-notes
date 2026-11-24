---
title: "The Specification Pattern in NestJS"
description: "Encapsulating business rules into composable, reusable boolean predicates: and(), or(), and not() specifications"
tags: [nestjs, design-patterns, specification, business-rules, composable-rules]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Specification Pattern in NestJS

## Conceptual Overview
The Specification pattern encapsulates a domain business rule as a first-class object that evaluates whether a candidate object satisfies its criteria.

Specifications can be combined using boolean logic:
$$	ext{Eligible} = 	ext{IsActiveSpec} \land 	ext{HasPaidSubscriptionSpec} \land 
eg 	ext{IsBannedSpec}$$

---

## Implementation Details: Composable Specification

```typescript
export interface Specification<T> {
  isSatisfiedBy(candidate: T): boolean;
  and(other: Specification<T>): Specification<T>;
  or(other: Specification<T>): Specification<T>;
}

export abstract class CompositeSpecification<T> implements Specification<T> {
  abstract isSatisfiedBy(candidate: T): boolean;

  and(other: Specification<T>): Specification<T> {
    return new AndSpecification(this, other);
  }

  or(other: Specification<T>): Specification<T> {
    return new OrSpecification(this, other);
  }
}

class AndSpecification<T> extends CompositeSpecification<T> {
  constructor(private one: Specification<T>, private other: Specification<T>) {
    super();
  }
  isSatisfiedBy(candidate: T): boolean {
    return this.one.isSatisfiedBy(candidate) && this.other.isSatisfiedBy(candidate);
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Specification encapsulates business criteria into single, testable classes.
- Compose specifications using `.and()`, `.or()`, and `.not()`.

---

## Cross-References
- [04-domain-driven-design.md](../01-architecture/04-domain-driven-design.md) - Domain-Driven Design
- [07-service-layer.md](./07-service-layer.md) - Service layer pattern
