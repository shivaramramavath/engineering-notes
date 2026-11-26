# Contributing Guidelines (CONTRIBUTING.md)

Guidelines for contributing notes, patterns, exercises, and code snippets to the `08-solid-design-patterns/` curriculum.

---

## 1. Core Principles for Contributors

1. **Problem-First Always**: Never open with an abstract definition. Start with a real production system failure, the cost of the problem, and why naive code broke when a realistic Change Request arrived.
2. **Mandatory 17-Section Template**: Every principle and pattern note must implement all 17 sections defined in [_templates/principle-template.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/_templates/principle-template.md) or [_templates/pattern-template.md](file:///e:/Learning/engineering-notes/08-solid-design-patterns/_templates/pattern-template.md).
3. **Three-Language Verification**: Every runnable example must provide code in:
   - **Java 25 (LTS)**: Clean static typing, idiomatic record/interface/stream usage.
   - **TypeScript 5.8 (Node 24 native)**: Strict typing, modern structural typing.
   - **Python 3.14**: Idiomatic typing, `typing.Protocol`, `dataclasses`, closures.
4. **Markdown-Only Policy**: Zero raw `.java`, `.ts`, or `.py` files inside the repository. All runnable code lives in fenced markdown blocks. Test execution is done externally in the scratch runner directory.
5. **Real-World Verifiable Citations**: When citing standard libraries (JDK, Python stdlib, WHATWG DOM) or frameworks, cite exact verified classes and APIs. Never invent internal framework mechanics.

---

## 2. Note Checklist Before Submission

- [ ] Note follows the 17-section structure.
- [ ] Section 4 ("The Problem Without It") and Section 5 ("How It Solves It") share the **exact same scenario and change request**.
- [ ] Code examples verified against the multi-language runner.
- [ ] Real measured output pasted below code snippets.
- [ ] Section 11 contains a 6+ row Common Mistakes table (Mistake / Why Wrong / Fix).
- [ ] Section 14 includes 8 deep interview questions with `<details>`.
- [ ] Section 15 includes 6 graded practice exercises with verified `<details>` solutions.
- [ ] Cross-links to language tracks formatted properly in the "In your language" table.

---
[Back to Track Root](file:///e:/Learning/engineering-notes/08-solid-design-patterns/README.md)
