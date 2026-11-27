# Measuring Design Quality & Hotspots

| Field | Value |
| :--- | :--- |
| **Category** | Refactoring (Metrics & Prioritization) |
| **Origin** | Adam Tornhill (*Your Code as a Crime Scene*) / SonarQube |

> **TL;DR**: Don't refactor ugly code that hasn't been touched in 4 years. Prioritize **Hotspots**—files with **high complexity** AND **high Git change frequency (churn)**.

## Key Metrics
1. **Cyclomatic Complexity**: Number of linearly independent paths (`if`, `for`, `case`). High values = hard to test all branches.
2. **Cognitive Complexity** (SonarSource): Penalizes nested `if/for` deeply—much closer to human reading difficulty than Cyclomatic Complexity.
3. **Afferent ($C_a$) vs Efferent ($C_e$) Coupling**: How many modules depend on you ($C_a$) vs how many you depend on ($C_e$).
4. **Code Churn × Complexity (Hotspot)**: `git log --format=format: --name-only` — fix messy files that your team edits *every week*!

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Automated Refactoring Tools](file:///E:/Learning/engineering-notes/08-solid-design-patterns/14-refactoring/06-automated-refactoring-tools.md) | [Module 14](file:///E:/Learning/engineering-notes/08-solid-design-patterns/14-refactoring/README.md) | [Module 15: Testing & Debugging](file:///E:/Learning/engineering-notes/08-solid-design-patterns/15-testing-and-debugging/README.md) |
