# Contributing & Technical Authoring Guidelines

> Standards, conventions, verification requirements, and formatting rules for maintaining the Java Engineering Curriculum.

---

## Core Authoring Principles

1. **Production-Grade Rigor:** No pseudo-code, no hand-waving explanations, and zero `// TODO` placeholders. Every example must be fully implemented and syntactically valid.
2. **Authoritative Standard:** Oracle JDK 25 LTS is the baseline platform. Every code snippet must compile cleanly against `javac --release 25 -Xlint:all`.
3. **Repository Cleanliness (Markdown-Only):** The repository directory (`01-languages/java/`) strictly contains `.md` files. Never commit compiled `.class` binaries, `.jar` archives, or non-markdown artifacts into the track directory. All verification builds must run in external scratch directories.

---

## Note Architecture Standard

Every concept note must strictly follow `_templates/concept-template.md` and feature all 13 mandated sections:
1. `# Title` with one-line summary blockquote and metadata table (Level, Prerequisites with relative links, Time to read, Status, Java Version).
2. Table of Contents.
3. **1. What & Why:** Architectural rationale and industry motivation.
4. **2. Core Concepts:** Definitions, formal models, and mechanisms.
5. **3. How It Works Internally:** HotSpot memory layouts, bytecode, or ASCII diagrams.
6. **4. Syntax & Usage:** Complete API signatures and configurations.
7. **5. Practical Examples:** At least 5 verified code examples progressing from basic to real-world edge cases.
8. **6. Best Practices:** Production guidelines and design patterns.
9. **7. Common Mistakes & Pitfalls:** Structured table (Mistake | Why It Fails | Fix).
10. **8. Performance & Security Notes:** Cache line alignment, allocations, and OWASP recommendations.
11. **9. Comparison / When to Use What:** Trade-off comparison matrix.
12. **10. Interview Questions:** 8+ difficult questions with collapsible `<details>` answers.
13. **11. Practice Exercises:** 3+ problems with verified code solutions in `<details>`.
14. **12. Summary / Cheat Sheet:** Rapid-review summary card.
15. **13. Further Reading:** Relative links to other notes and official JEPs.

---

## Verification Workflow

Before modifying or adding notes, execute the following verification checklist:
```bash
# 1. Extract and test compilation in scratch directory
javac --release 25 -Xlint:all -d scratch/ scratch/SnippetTest.java

# 2. Run with assertions enabled
java -ea -cp scratch/ com.engineering.SnippetTest

# 3. Verify markdown formatting and relative link integrity
```
