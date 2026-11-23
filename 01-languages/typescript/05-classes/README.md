# Module 05: Classes

> Comprehensive architectural guide to TypeScript classes: object-oriented patterns, prototype delegation, access modifiers, `#private` fields, inheritance, abstract contracts, mixins, Stage 3 decorators, and erasable syntax boundaries.

---

## Module Overview

Classes provide structural blueprints, prototype inheritance, and state encapsulation. This module covers both traditional OOP patterns and modern TypeScript 5.x/7.0 constraints (erasable syntax, Stage 3 decorators, and nominal branding).

| Note | Title | Level | Baseline | Description |
| :--- | :--- | :--- | :--- | :--- |
| **00** | [Classes](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/00-classes.md) | Intermediate | TS 0.9 / TS 5.0 | Dual type space, constructor vs instance types, `strictPropertyInitialization`. |
| **01** | [Access Modifiers](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/01-access-modifiers.md) | Intermediate | TS 0.9 / TS 3.8 | `public`, `protected`, `private` vs ECMAScript `#private` runtime fields. |
| **02** | [Inheritance and Abstract Classes](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/02-inheritance-and-abstract-classes.md) | Intermediate | TS 0.9 / TS 4.3 | Prototype delegation, abstract methods, template method pattern, `override` keyword. |
| **03** | [Class Implements Clause](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/03-implements.md) | Intermediate | TS 0.9+ | Contract fulfillment without prototype inheritance, multiple interface implementation. |
| **04** | [Static Members and Static Blocks](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/04-static-members.md) | Intermediate | TS 0.9 / TS 4.4 | Static methods, static inheritance, and ECMAScript `static {}` initialization blocks. |
| **05** | [Getters and Setters](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/05-getters-and-setters.md) | Intermediate | TS 0.9 / TS 4.3 / TS 5.0 | Accessors, TS 4.3 differing get/set types, and TS 5.0 `accessor` auto-accessors. |
| **06** | [Mixins](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/06-mixins.md) | Advanced | TS 2.2 / TS 4.2 | Constructor factory mixins, abstract mixins (`abstract new`), composition pipelines. |
| **07** | [Decorators](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/07-decorators.md) | Advanced | TS 5.0 / TS 5.2 | Stage 3 TC39 decorators, decorator context, `addInitializer`, and `Symbol.metadata`. |
| **08** | [Constructor Parameter Properties and Erasable Syntax](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/08-constructor-parameter-properties-and-erasable-syntax.md) | Intermediate | TS 0.9 / TS 5.8 | Parameter property codegen, Node 22 type stripping failure, `--erasableSyntaxOnly`. |
| **09** | [Class Expressions and Structural Typing](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/09-class-expressions-and-structural-typing.md) | Advanced | TS 0.9+ | Anonymous/named class expressions, structural subtyping vs nominal branding. |

---

## Architectural Class Memory Layout

```
+--------------------------------------------------------------------------+
|                  Dual Memory Layout of a TypeScript Class                |
|                                                                          |
|       Value Space: Constructor Function        Prototype Object          |
|       ┌─────────────────────────────────┐      ┌──────────────────────┐  |
|       │ MyClass                         │      │ MyClass.prototype    │  |
|       │ ├── static members              │      │ ├── methods          │  |
|       │ ├── static initialization block │      │ ├── getters/setters  │  |
|       │ └── prototype ──────────────────┼─────►│ └── __proto__        │  |
|       └─────────────────────────────────┘      └──────────────────────┘  |
|                        ▲                                  ▲              |
|                        │ constructor                      │ __proto__    |
|                        │                                  │              |
|       Instance (Heap): ┴──────────────────────────────────┴              |
|       ├── Instance fields (public, protected, private, #private)         |
|       └── Own instance properties                                        |
+--------------------------------------------------------------------------+
```

---

## Recommended Learning Sequence

1. **Class Foundations**: Master `00-classes.md`, `01-access-modifiers.md`, and `08-constructor-parameter-properties-and-erasable-syntax.md` (critical for Node 22+ type stripping and `--erasableSyntaxOnly`).
2. **Object Hierarchy**: Progress through `02-inheritance-and-abstract-classes.md` (using `override`), `03-implements.md`, and `04-static-members.md`.
3. **Property Interception**: Study `05-getters-and-setters.md` and auto-accessors.
4. **Advanced Metaprogramming**: Complete the module with `06-mixins.md`, `07-decorators.md` (Stage 3 standards), and `09-class-expressions-and-structural-typing.md`.
