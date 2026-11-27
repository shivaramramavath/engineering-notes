# Refactoring Catalogue (Essential Moves)

| Move | When to Use | Inverse Move |
| :--- | :--- | :--- |
| **Extract Method / Function** | Long method or block needing a comment to explain *what* it does | Inline Function |
| **Extract Class** | God Object / Divergent Change (2+ responsibilities) | Inline Class |
| **Move Method / Field** | Feature Envy (method uses another class's data more than its own) | — |
| **Replace Nested `if` with Guard Clauses** | Deep arrow nesting (`if (a) { if (b) { ... } }`) | — |
| **Introduce Parameter Object / Value Object** | 4+ parameters traveling together (`start, end`) or Primitive Obsession | — |
| **Replace Conditional with Polymorphism / ADT** | Repeated `switch (type)` across multiple methods | — |
| **Encapsulate Collection** | Getter returns mutable `List` allowing callers to bypass invariants | — |
| **Replace Magic Literal with Constant / Enum** | Unexplained numbers/strings (`86400`, `"S02"`) | — |
