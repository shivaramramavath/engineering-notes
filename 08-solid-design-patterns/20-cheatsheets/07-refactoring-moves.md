# Refactoring Moves Cheatsheet

1. **Two Hats**: Never add features and refactor simultaneously.
2. **Untested Legacy**: Write **Characterisation Tests** + introduce **Seams** first.
3. **Top 6 Moves**:
   - *Extract Method* (split long functions at cohesive boundaries)
   - *Guard Clauses* (invert nested `if`s into early returns)
   - *Extract Class* (cure God Objects / Divergent Change)
   - *Move Method* (cure Feature Envy)
   - *Introduce Value Object* (cure Primitive Obsession)
   - *Replace Conditional with Polymorphism / `sealed` ADT*
4. **Large Migrations**: Use **Branch by Abstraction** + **Strangler Fig** + **Expand-and-Contract**.
