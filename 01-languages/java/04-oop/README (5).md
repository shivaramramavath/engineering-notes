# 04 - Object-Oriented Programming

Java organizes programs as **objects** that combine data and behavior. This folder covers the language mechanics (classes, constructors, `static`, `final`), the four pillars (encapsulation, inheritance, polymorphism, abstraction), the design tools around them (interfaces, composition, enums, nested classes), and the `Object` methods every class inherits.

```
classes & objects ─► constructors ─► static ─► final ─► encapsulation
                                                            │
   ┌────────────────────────────────────────────────────────┘
   ▼
initialization order ─► inheritance ─► polymorphism ─► abstract classes ─► interfaces
                                                                             │
   ┌─────────────────────────────────────────────────────────────────────────┘
   ▼
composition ─► enums ─► nested classes ─► Object ─► equals & hashCode ─► clone & copying
```

## The four pillars at a glance

| Pillar | Idea | Main files |
|--------|------|------------|
| **Encapsulation** | Hide internal state; expose a small, safe API | [04](./04_encapsulation-and-access-modifiers.md), [03](./03_final.md) |
| **Inheritance** | A class reuses and specializes another (**is-a**) | [06](./06_inheritance.md) |
| **Polymorphism** | One reference type, many behaviors at runtime | [07](./07_polymorphism.md) |
| **Abstraction** | Depend on contracts, not details | [08](./08_abstract-classes.md), [09](./09_interfaces.md) |

## Prerequisites

[01-fundamentals](../01-fundamentals/README.md), [02-methods](../02-methods/README.md) (especially [pass-by-value](../02-methods/01_pass-by-value.md)) and [03-strings-and-text](../03-strings-and-text/README.md).

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_classes-and-objects.md](./00_classes-and-objects.md) | Fields, methods, `new`, references, `this`, object lifecycle |
| 1 | [01_constructors.md](./01_constructors.md) | Constructors, chaining, copy constructors, static factories |
| 2 | [02_static.md](./02_static.md) | Static members, static blocks, utility classes, pitfalls |
| 3 | [03_final.md](./03_final.md) | `final` variables, methods and classes; effectively final |
| 4 | [04_encapsulation-and-access-modifiers.md](./04_encapsulation-and-access-modifiers.md) | `private`/package/`protected`/`public`, invariants, safe APIs |
| 5 | [05_initialization-order.md](./05_initialization-order.md) | Exactly what runs when: static, instance, constructors |
| 6 | [06_inheritance.md](./06_inheritance.md) | `extends`, `super`, overriding rules, when inheritance goes wrong |
| 7 | [07_polymorphism.md](./07_polymorphism.md) | Dynamic dispatch, upcasting and downcasting |
| 8 | [08_abstract-classes.md](./08_abstract-classes.md) | The abstraction concept, abstract classes, template use |
| 9 | [09_interfaces.md](./09_interfaces.md) | Contracts, default/static/private methods, abstract class vs interface |
| 10 | [10_composition-and-object-relationships.md](./10_composition-and-object-relationships.md) | Association, aggregation, composition, "composition over inheritance" |
| 11 | [11_enums.md](./11_enums.md) | Type-safe constants with behavior |
| 12 | [12_nested-and-inner-classes.md](./12_nested-and-inner-classes.md) | Static nested, inner, local and anonymous classes |
| 13 | [13_object-class.md](./13_object-class.md) | `Object`, `toString`, `getClass`, `Objects` utilities |
| 14 | [14_equals-and-hashcode.md](./14_equals-and-hashcode.md) | The contracts and a correct implementation |
| 15 | [15_clone-and-copying.md](./15_clone-and-copying.md) | Shallow vs deep copies, why `clone()` is avoided, alternatives |

## Practice

| After file | Try |
|------------|-----|
| 00-01 | Model a `Book`, `Student` and `Rectangle`; add constructors with validation |
| 03-04 | Write an immutable `Money` class (final fields, no setters, defensive copies) |
| 05 | Predict the output of a three-level `static`/instance/constructor print chain, then run it |
| 06-07 | A `Shape` hierarchy (`Circle`, `Rectangle`, `Triangle`) and a method that sums areas of a `List<Shape>` |
| 08-09 | Add a `Payable` interface to employees and a template-method `ReportGenerator` |
| 10 | Rewrite a `Stack extends ArrayList` as a stack that **has** a list |
| 11 | A `Planet` enum with mass and radius, and an `Operation` enum with constant-specific behavior |
| 14 | Write `equals`/`hashCode` for `Point`; prove with a `HashSet` what breaks when you omit `hashCode` |

**Project:** [26-projects/02-banking-system](../26-projects/02-banking-system/) (after this folder and `06-exceptions-and-debugging`).

## You are done when you can

- [ ] Explain the difference between a class, an object and a reference
- [ ] Say in what order static blocks, instance blocks, field initializers and constructors run in a two-class hierarchy
- [ ] Explain why a field can be `final` and still change (mutable object), and why `final` on a method differs from `final` on a class
- [ ] Predict which method runs when a `Parent` reference holds a `Child` object, including for fields and `static` methods
- [ ] Choose between an abstract class, an interface and composition for a design, and justify it
- [ ] Write `equals` and `hashCode` that satisfy their contracts, and explain what breaks a `HashMap` if they disagree
- [ ] Explain why `clone()` is considered broken and what to use instead

## Key takeaways

- A class is a type; an object is an instance; variables hold references to objects
- Make state `private`, validate in constructors, prefer immutability
- Polymorphism works on **methods** (runtime), not on fields or static methods
- Prefer interfaces and composition; use inheritance only for true is-a relationships
- Override `equals` and `hashCode` together, never one without the other

**Next:** [05-packages-and-modules](../05-packages-and-modules/README.md)
