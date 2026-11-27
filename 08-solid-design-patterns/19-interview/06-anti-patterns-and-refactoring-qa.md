# Anti-Patterns & Refactoring Q&A

<details><summary>1. How do you safely refactor legacy code that has zero tests?</summary>
Write **Characterisation Tests** (Golden Master) around the public entry points to pin existing behavior, introduce **Seams** (parameterized deps or subclass-and-override) to break hard I/O coupling, and refactor in tiny green-to-green steps.
</details>

<details><summary>2. Divergent Change vs Shotgun Surgery?</summary>
**Divergent Change**: 1 class is edited for 5 unrelated reasons (fix: Extract Class). **Shotgun Surgery**: 1 small change forces edits across 10 files (fix: Move Method / Inline Class).
</details>

<details><summary>3. What is the Anemic Domain Model anti-pattern?</summary>
Entities with only getters/setters while all business invariants live in procedural Services, allowing callers to put entities into invalid states.
</details>

<details><summary>4. How does Branch by Abstraction avoid long-lived feature branches?</summary>
Wrap the legacy component behind an interface, route callers through it on `main`, build the new implementation behind the same interface, flip via feature flag, and delete the legacy code.
</details>
