# Module 06: Structural Design Patterns

Welcome to **Module 06: Structural Design Patterns**. Structural patterns explain how to assemble objects and interfaces into larger, flexible structures while keeping those structures loosely coupled, composable, and resilient to third-party or legacy changes.

---

## Module Roadmap & Contents

| # | Note | Core Focus & Purpose | Status | Est. Time |
|---|---|---|:---:|:---:|
| 00 | [Structural Patterns Overview](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/00-overview.md) | Disambiguating Adapter, Decorator, Proxy, and Facade by architectural intent | Complete | 15 min |
| 01 | [Adapter Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/01-adapter.md) | Converting incompatible third-party/legacy interfaces into clean domain ports | Complete | 30 min |
| 02 | [Bridge Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/02-bridge.md) | Decoupling orthogonal abstractions from implementations to stop $M \times N$ subclassing | Complete | 30 min |
| 03 | [Composite Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/03-composite.md) | Uniform tree operations across individual leaves and nested containers | Complete | 30 min |
| 04 | [Decorator Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/04-decorator.md) | Stacking orthogonal behaviors (retry, cache, encrypt, log) dynamically at runtime | Complete | 30 min |
| 05 | [Facade Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/05-facade.md) | Providing a simple unified entry point over a complex multi-class subsystem | Complete | 25 min |
| 06 | [Flyweight Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/06-flyweight.md) | Sharing immutable intrinsic state across millions of objects to slash RAM footprint | Complete | 30 min |
| 07 | [Proxy Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/07-proxy.md) | Controlling access (Virtual, Protection, Caching, Remote RPC) behind same interface | Complete | 30 min |
| 08 | [Null Object Pattern](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/08-null-object.md) | Eliminating repetitive `if (x != null)` checks via safe no-op objects | Complete | 25 min |
| 09 | [Private Class Data & Immutable Objects](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/09-private-class-data-and-immutable-objects.md) | Sealing internal state from accidental mutation inside and outside a class | Complete | 25 min |
| 10 | [Extension Object, Mixins & Traits](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/10-extension-object-and-mixins-and-traits.md) | Adding capabilities without polluting core interfaces (ISP + Composition) | Complete | 25 min |
| 11 | [Structural Patterns Comparison](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/11-structural-patterns-comparison.md) | Decision matrix, wrapper disambiguation, and anti-pattern checklist | Complete | 15 min |

---

## Core Learning Outcomes

By completing this module, you will:
1. Confidently distinguish **Adapter**, **Decorator**, **Proxy**, and **Facade** by intent rather than class-diagram similarity.
2. Eliminate $M \times N$ subclass explosion using **Bridge** and **Decorator**.
3. Model hierarchical structures (file systems, org charts, pricing bundles) cleanly with **Composite**.
4. Reduce memory consumption by orders of magnitude with **Flyweight**, and eliminate `NullPointerException` clutter with **Null Object**.

---

[← Back to Module 05: Creational Patterns](file:///e:/Learning/engineering-notes/08-solid-design-patterns/05-creational-patterns/README.md) | [Next: 00 - Structural Patterns Overview →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/00-overview.md)
