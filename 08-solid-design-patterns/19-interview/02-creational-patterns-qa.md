# Creational Patterns Q&A

<details><summary>1. Why is Singleton often considered an anti-pattern today?</summary>
It creates hidden global mutable state, hides dependencies from constructors, and breaks parallel unit tests. Prefer DI-managed singleton scope instead of `getInstance()`.
</details>

<details><summary>2. Factory Method vs Abstract Factory?</summary>
**Factory Method** creates **one** product via an overridable method/function. **Abstract Factory** creates a **family** of matching products (`WinButton` + `WinCheckbox` vs `MacButton` + `MacCheckbox`).
</details>

<details><summary>3. When do you use Builder instead of a constructor?</summary>
When an object has 4+ optional parameters (telescoping constructor problem) or needs step-by-step validation before building an immutable result. In TS/Python, named options objects or dataclass defaults often suffice.
</details>

<details><summary>4. How does Double-Checked Locking fail without `volatile`, and what's the fix in Java?</summary>
Instruction reordering can publish a partially constructed reference before its constructor finishes. Use `volatile` (or better: **Initialization-on-Demand Holder** static inner class or `enum` Singleton).
</details>

<details><summary>5. Object Pool vs Flyweight?</summary>
**Flyweight** shares **immutable** objects simultaneously (glyphs, cached strings). **Object Pool** lends **mutable/expensive** objects (DB connections) to one borrower at a time and resets them on return.
</details>
