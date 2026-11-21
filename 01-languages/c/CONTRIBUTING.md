# Contributing Guidelines for C Curriculum (`CONTRIBUTING.md`)

## Standard Format Contract
Every concept note in `01-languages/c/` must strictly implement the standard 13-section structure:
1. Title (`#`) & Blockquote Summary
2. Metadata Table (Level, Prerequisites, Time, Status, Standards)
3. Table of Contents
4. 1 What & Why
5. 2 Core Concepts
6. 3 How It Works Internally (with ASCII/Mermaid diagrams)
7. 4 Syntax & Usage
8. 5 Practical Examples (Basic, Real-World, Edge-Case with verified outputs)
9. 6 Best Practices
10. 7 Common Mistakes & Pitfalls (Markdown table with 6+ rows)
11. 8 Performance & Security Notes
12. 9 Comparison / When to Use What
13. 10 Interview Questions (8+ questions with `<details>` answers)
14. 11 Practice Exercises (3 exercises with detailed solutions)
15. 12 Summary / Cheat Sheet
16. 13 Further Reading
17. Footer Navigation (`Previous | Section Index | Next`)

## Code Verification Rules
- All companion C test code must compile cleanly with `-std=c11 -Wall -Wextra -Wpedantic -Werror`.
- Test execution and compilation artifacts (`.c`, `.exe`, `.json`) must strictly occur in the external scratch directory and must **NEVER** be committed into the markdown repository.
- Every note must contain between 400 and 800 lines of rigorous technical content (zero placeholders, zero TODOs).
