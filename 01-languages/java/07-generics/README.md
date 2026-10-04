# 07 - Generics

**Generics** let classes, interfaces and methods work with types as **parameters**: `List<String>`, `Map<String, Integer>`, `Optional<User>`. The compiler checks the types, so you get safety without casts and without duplicating code per type. Generics are everywhere in the JDK (collections, streams, `Optional`, `Comparator`) and in every framework, so this folder is a prerequisite for most of what follows.

```
generic classes & methods ─► bounded types ─► wildcards & PECS ─► type erasure ─► generic patterns
        (what and how)         (limit T)        (flexible APIs)     (the limits)      (real designs)
```

## Prerequisites

[04-oop](../04-oop/README.md): [interfaces](../04-oop/09_interfaces.md), [inheritance](../04-oop/06_inheritance.md) and [polymorphism](../04-oop/07_polymorphism.md); [06-exceptions-and-debugging](../06-exceptions-and-debugging/README.md) (generics turn many `ClassCastException`s into compile errors).

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_generic-classes-and-methods.md](./00_generic-classes-and-methods.md) | Type parameters, generic classes, interfaces, methods, the diamond, inference, raw types |
| 1 | [01_bounded-types.md](./01_bounded-types.md) | `T extends X`, multiple bounds, recursive bounds like `T extends Comparable<T>` |
| 2 | [02_wildcards-and-pecs.md](./02_wildcards-and-pecs.md) | `?`, `? extends`, `? super`, invariance, the PECS rule, wildcard capture |
| 3 | [03_type-erasure.md](./03_type-erasure.md) | What the compiler erases, what that forbids, heap pollution, workarounds |
| 4 | [04_generic-patterns.md](./04_generic-patterns.md) | Type tokens, self-typed builders, generic repositories, result types, API design tips |

## Practice

| After file | Try |
|------------|-----|
| 00 | Write `Pair<A, B>`, a generic `Stack<E>` backed by an `ArrayList`, and `static <T> void swap(T[] a, int i, int j)` |
| 01 | Write `max(List<T>)` for `T extends Comparable<T>`, and `sum` for `T extends Number` |
| 02 | Write `copy(src, dest)` with the correct wildcards and explain each `?` with PECS |
| 03 | Show that `List<String>` and `List<Integer>` have the same `getClass()`; trigger a `ClassCastException` through heap pollution |
| 04 | Build a type-safe `Favorites` container keyed by `Class<T>` and a generic `Repository<T, ID>` with an in-memory implementation |

## You are done when you can

- [ ] Explain what problem generics solve compared with `Object` and casts
- [ ] Write a generic class and a generic method, and say when the compiler infers the type arguments
- [ ] Explain why `List<Integer>` is **not** a `List<Number>`, and how wildcards fix the use cases
- [ ] Apply **PECS** to choose `? extends` or `? super` without guessing
- [ ] Explain type erasure and list four things it forbids (`new T()`, `instanceof List<String>`, `new T[]`, overloads that differ only in type arguments)
- [ ] Recognize an unchecked warning as a real risk and say when `@SuppressWarnings("unchecked")` is justified

## Key takeaways

- Generics move type errors from **runtime** (`ClassCastException`) to **compile time**
- Generic types are **invariant**; wildcards (`? extends`, `? super`) add flexibility at use sites
- Type arguments are **erased** at runtime: that is the source of most generics restrictions
- Prefer generic methods and bounded types over `Object` and casts; avoid raw types

**Next:** [08-collections](../08-collections/README.md)
