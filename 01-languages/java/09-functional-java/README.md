# 09 - Functional Java

Since Java 8, Java lets you treat **behavior as a value**: pass a piece of code to a method, store it in a variable, return it from a method. This folder covers **lambdas**, the **functional interfaces** they implement, **method references**, **`Optional`**, and the **Stream API** for declarative data processing, then the design patterns that come with them.

```
lambdas ─► functional interfaces ─► method references ─► Optional
                                                            │
        functional patterns ◄─ parallel streams ◄─ collectors ◄─ stream operations ◄─ stream basics
```

Compare the two styles on the same task: "names of adults, sorted, joined with commas".

```java
// Imperative: HOW to do it
List<String> names = new ArrayList<>();
for (Person p : people) if (p.age() >= 18) names.add(p.name());
Collections.sort(names);
String result = String.join(", ", names);

// Declarative: WHAT you want
String result = people.stream()
        .filter(p -> p.age() >= 18)
        .map(Person::name)
        .sorted()
        .collect(Collectors.joining(", "));
```

## Prerequisites

[04-oop](../04-oop/README.md) ([interfaces](../04-oop/09_interfaces.md), [anonymous classes](../04-oop/12_nested-and-inner-classes.md)), [07-generics](../07-generics/README.md) (functional interfaces are generic) and [08-collections](../08-collections/README.md) (streams process collections).

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_lambda-expressions.md](./00_lambda-expressions.md) | Lambda syntax, capturing variables, `this`, lambdas vs anonymous classes |
| 1 | [01_functional-interfaces.md](./01_functional-interfaces.md) | `Function`, `Predicate`, `Supplier`, `Consumer` and their variants; composing them |
| 2 | [02_method-references.md](./02_method-references.md) | The four kinds of `::` and when to use them |
| 3 | [03_optional.md](./03_optional.md) | Modeling "no value": correct use and misuse |
| 4 | [04_streams-fundamentals.md](./04_streams-fundamentals.md) | Pipelines, sources, laziness, single use |
| 5 | [05_stream-operations.md](./05_stream-operations.md) | `filter`, `map`, `flatMap`, `reduce`, short-circuiting, primitive streams |
| 6 | [06_collectors.md](./06_collectors.md) | `groupingBy`, `partitioningBy`, `toMap`, downstream collectors, custom collectors |
| 7 | [07_parallel-streams.md](./07_parallel-streams.md) | When parallelism helps, when it hurts, and the rules to stay safe |
| 8 | [08_functional-patterns.md](./08_functional-patterns.md) | Composition, currying, memoization, validation pipelines, functional core |

## Practice

| After file | Try |
|------------|-----|
| 00-01 | Sort strings by length then alphabetically with a lambda; build a `Predicate` that combines three conditions with `and`/`or`/`negate` |
| 02 | Rewrite five lambdas as method references, one of each kind |
| 03 | Write `findUser(id)` returning `Optional<User>` and chain `map`/`filter`/`orElse` without any `null` check |
| 04-05 | Replace three loops from earlier exercises with streams; get the 3 longest distinct words from a text |
| 06 | Group employees by department with the average salary; partition numbers into primes and non-primes |
| 07 | Time a CPU-heavy computation sequentially vs in parallel on a small and a large input; explain the difference |
| 08 | Build a validation pipeline of `Predicate`s with error messages, and a memoizing `Function` wrapper |

**Project:** [26-projects/04-ecommerce](../26-projects/04-ecommerce/) (order analytics with streams and collectors).

## You are done when you can

- [ ] Write a lambda for any single-method interface and explain what "effectively final" means
- [ ] Pick the right functional interface (`Function`, `Predicate`, `Supplier`, `Consumer`, `UnaryOperator`, primitive variants) for a task
- [ ] Name the four kinds of method reference and convert a lambda into each
- [ ] Use `Optional` as a return type and say three places **not** to use it
- [ ] Explain why a stream does nothing until a terminal operation, and why it cannot be reused
- [ ] Group, partition, count and join with collectors, and write `toMap` without the duplicate-key crash
- [ ] Decide when `parallelStream()` is safe and worthwhile, and when it is not

## Key takeaways

- A lambda is a short implementation of a **functional interface** (one abstract method)
- Streams are **lazy, single-use pipelines**: source → intermediate operations → terminal operation
- Keep lambdas **small, pure and side-effect free**; use method references when they read better
- `Optional` is for **return values**, not fields or parameters
- Use parallel streams only for large, CPU-bound, stateless work, and measure

**Next:** [10-date-and-time](../10-date-and-time/README.md)