# Module 13: Anti-Patterns & Code Smells

Knowing good patterns is only half the skill—recognizing **anti-patterns** (common, seductive solutions that backfire) and **code smells** (surface symptoms of deeper structural rot) lets you catch design decay in PR reviews before it turns into a Big Ball of Mud.

---

## Notes in This Module

| # | Topic | Category |
| :--- | :--- | :--- |
| 00 | [Module Overview](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/00-overview.md) | Smells vs Anti-Patterns & severity triage |
| 01 | [God Object & Blob](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/01-god-object-and-blob.md) | One 3,000-line class owning everything |
| 02 | [Spaghetti Code](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/02-spaghetti-code.md) | Tangled control flow & hidden global mutations |
| 03 | [Golden Hammer](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/03-golden-hammer.md) | Forcing one favorite tool/pattern onto every problem |
| 04 | [Copy-Paste Programming](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/04-copy-paste-programming.md) | Duplicated logic (and premature DRY vs Rule of Three) |
| 05 | [Lava Flow & Dead Code](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/05-lava-flow-and-dead-code.md) | Unmaintained legacy strata nobody dares delete |
| 06 | [Singleton Abuse & Global State](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/06-singleton-abuse-and-global-state.md) | Hidden coupling & flaky tests from mutable globals |
| 07 | [Service Locator Abuse](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/07-service-locator-abuse.md) | Runtime dependency lookup hiding constructor contracts |
| 08 | [Anemic Domain Model](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/08-anemic-domain-model.md) | Getter/setter data bags with scattered invariants |
| 09 | [Big Ball of Mud](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/09-big-ball-of-mud.md) | Absence of perceivable architecture |
| 10 | [Premature Optimisation & Abstraction](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/10-premature-optimisation-and-abstraction.md) | Speculative generality & micro-tuning before profiling |
| 11 | [Cargo Cult & Résumé-Driven Development](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/11-cargo-cult-and-resume-driven-development.md) | Adopting Kafka/K8s/patterns without the problem |
| 12 | [Primitive Obsession](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/12-primitive-obsession.md) | Raw strings/ints instead of `Money`/`Email` Value Objects |
| 13 | [Feature Envy & Inappropriate Intimacy](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/13-feature-envy-and-inappropriate-intimacy.md) | Methods obsessed with another class's internal data |
| 14 | [Shotgun Surgery & Divergent Change](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/14-shotgun-surgery-and-divergent-change.md) | 1 change touches 12 files vs 1 file changes for 5 reasons |
| 15 | [Long Method & Large Class](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/15-long-method-and-large-class.md) | Bloaters hiding cohesive abstractions |
| 16 | [Magic Numbers & Strings](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/16-magic-numbers-and-strings.md) | Unexplained literals (`86400`, `"ST_03"`) |
| 17 | [Boolean Flags & Flag Arguments](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/17-boolean-flags-and-flag-arguments.md) | `render(true, false, true)` unreadable call sites |
| 18 | [Deep Inheritance & Yo-Yo Problem](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/18-deep-inheritance-and-yo-yo-problem.md) | Fragile 6-level class hierarchies |
| 19 | [Leaky Abstractions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/19-leaky-abstractions.md) | Abstractions that force callers to know underlying quirks |
| 20 | [Smell-to-Refactoring Map](file:///E:/Learning/engineering-notes/08-solid-design-patterns/13-anti-patterns-and-code-smells/20-smell-to-refactoring-map.md) | Quick diagnostic lookup: Symptom → Root Cause → Fix |

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Module 12: Functional & Modern Idioms](file:///E:/Learning/engineering-notes/08-solid-design-patterns/12-functional-and-modern-idioms/README.md) | [Track Index](file:///E:/Learning/engineering-notes/08-solid-design-patterns/INDEX.md) | [Module 14: Refactoring](file:///E:/Learning/engineering-notes/08-solid-design-patterns/14-refactoring/README.md) |
