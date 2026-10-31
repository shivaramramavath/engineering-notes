# Template Method

**Intent:** define the skeleton of an algorithm in a base class, deferring some steps to subclasses. Subclasses can redefine specific steps without changing the algorithm's structure.

The base class controls the **flow**; subclasses supply the **details**.

## The problem

Two data importers share the same process (open, parse, validate, save, close) but differ in the parsing step. Duplicating the flow in each class means a fix or new step must be made twice.

```java
class CsvImporter {
    void run(String path) { open(path); parseCsv(); validate(); save(); close(); }
}
class JsonImporter {
    void run(String path) { open(path); parseJson(); validate(); save(); close(); }   // duplicated flow
}
```

## Structure

```text
AbstractClass
  + templateMethod()   ← final: defines the order of steps
       calls: step1(), step2(), hook()
  # step1()  (abstract: subclasses must implement)
  # step2()  (abstract)
  # hook()   (optional: default does nothing)
        ▲
   ConcreteClass: implements steps
```

## Example

```java
public abstract class DataImporter {

    // The template method: final so subclasses can't change the order
    public final ImportResult run(String path) {
        List<Record> records = read(path);          // step varies
        List<Record> valid = validate(records);     // common, may be overridden
        save(valid);                                // common
        afterImport(valid);                         // hook
        return new ImportResult(valid.size(), records.size() - valid.size());
    }

    protected abstract List<Record> read(String path);          // required step

    protected List<Record> validate(List<Record> records) {     // default behavior
        return records.stream().filter(r -> r.id() != null).toList();
    }

    private void save(List<Record> records) { /* common persistence */ }

    protected void afterImport(List<Record> records) { }        // hook: optional
}

public class CsvImporter extends DataImporter {
    @Override protected List<Record> read(String path) { /* parse CSV */ }
}

public class JsonImporter extends DataImporter {
    @Override protected List<Record> read(String path) { /* parse JSON */ }

    @Override protected void afterImport(List<Record> records) {
        // optional extra behavior
    }
}

new CsvImporter().run("users.csv");
```

Three kinds of steps:

| Step type | Declared as | Subclass must... |
|---|---|---|
| **Abstract** | `protected abstract` | Implement it |
| **Default (overridable)** | `protected` with body | Optionally override |
| **Hook** | `protected` with empty body | Optionally add behavior at a given point |
| **Fixed** | `private` or `final` | Can't change it |

## The Hollywood principle

"Don't call us, we'll call you." In normal inheritance a subclass calls the parent. In Template Method, the **parent calls the subclass** at the right time. This inversion of control is common in frameworks: you extend a class and fill in the methods, and the framework decides when to call them.

Example: `HttpServlet.service()` decides whether to call `doGet`, `doPost` and so on, and you override the ones you need.

## Template Method vs Strategy

| | Template Method | [Strategy](./01-strategy.md) |
|---|---|---|
| Mechanism | **Inheritance** | **Composition** |
| Varies | Individual steps | The whole algorithm |
| Bound | Compile time (class chosen) | Runtime (object swapped) |
| Coupling | Subclass tied to base class | Looser |

A common modern approach is to turn a varying step into a strategy and inject it:

```java
public class DataImporter {
    private final RecordReader reader;          // strategy for the varying step
    public DataImporter(RecordReader reader) { this.reader = reader; }

    public ImportResult run(String path) {
        var records = reader.read(path);
        // same fixed flow
    }
}
```

This avoids deep hierarchies and the fragility described in [composition vs inheritance](../01-foundations/02-composition-vs-inheritance.md). Choose Template Method when the skeleton is fixed and there are only a few clear extension points; choose Strategy when you want more flexibility.

## When to use it

- Several classes share the same overall algorithm and differ in a few steps.
- You want to control exactly which parts can be extended.
- You're building a framework or base class with defined extension points.

Typical uses: importers and exporters, report generators, test frameworks (setup / run / teardown), build steps, game turn loops.

## Common mistakes

- **Not making the template method `final`**, so subclasses can reorder or break the flow.
- **Too many abstract steps**, forcing subclasses to implement everything. Provide defaults.
- **Calling overridable methods from constructors**, where the subclass isn't initialized yet.
- **Breaking Liskov**: a subclass that skips or contradicts the base class's expected step behavior.
- **Deep hierarchies** where behavior is spread across several levels.
- **Hooks that are undocumented**, so subclass authors don't know the contract (when called, what state exists).
- **Using inheritance for a single variation** where a strategy or lambda would be lighter.

## Quick Summary

- The base class fixes the algorithm's order; subclasses fill in the steps.
- Make the template `final`, mark required steps `abstract`, and offer optional hooks.
- It's inheritance-based; prefer Strategy when you want runtime flexibility or to avoid hierarchies.
- Common in frameworks, following the "Hollywood principle".

## Related

[Strategy](./01-strategy.md) · [Factory Method](../04-creational-patterns/02-factory-method.md) (often a step inside a template) · [Composition vs inheritance](../01-foundations/02-composition-vs-inheritance.md)

## Next

[06-chain-of-responsibility.md](./06-chain-of-responsibility.md)
