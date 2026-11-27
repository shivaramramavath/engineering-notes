# Refactoring Workflow & Safety Nets

| Field | Value |
| :--- | :--- |
| **Category** | Refactoring (Process & Discipline) |
| **Origin** | Martin Fowler (*Refactoring*, 1999/2018) & Kent Beck |

> **TL;DR**: Never mix feature changes and structural refactoring in the same breath (**Two Hats**). Keep tests green at every tiny step, commit frequently, and apply the **Boy Scout Rule** (*leave every file slightly cleaner than you found it*)—never refactoring untouched dead corners blindly.

## The Two Hats Rule
1. **Refactoring Hat**: Restructure code; **zero** new features or bug fixes; all existing tests must keep passing.
2. **Feature Hat**: Add new behavior + new tests; don't reorganize unrelated classes at the same time.

## The Safe 5-Step Loop
1. Verify tests pass (**Green**).
2. Make **one** small structural move (e.g., Extract Method).
3. Run tests (**Green** in seconds).
4. Commit or keep moving.
5. **When NOT to refactor**: If code is about to be deleted, never changes (stable legacy), or has zero tests and a tight hotfix deadline.
