# Builder

**Intent:** separate the construction of a complex object from its representation, so the same process can build different results, and so complex objects can be built step by step.

In everyday Java the Builder is mostly used to create **immutable objects with many parameters, some optional**, while keeping the call site readable.

## The problem

### Telescoping constructors

```java
new Pizza("large", true, false, true, false, null, 2);   // what do these mean?
```

Problems:

- You can't tell what each argument is without opening the constructor.
- Many overloads exist for different combinations.
- Swapping two arguments of the same type compiles silently.

### JavaBean setters

```java
Pizza p = new Pizza();
p.setSize("large");
p.setCheese(true);
```

Readable, but the object is **mutable** and can be observed in a half-built, invalid state.

## Builder solution

```java
public final class Pizza {
    private final String size;          // required
    private final boolean cheese;
    private final boolean pepperoni;
    private final List<String> extras;

    private Pizza(Builder b) {
        this.size = b.size;
        this.cheese = b.cheese;
        this.pepperoni = b.pepperoni;
        this.extras = List.copyOf(b.extras);
    }

    public static Builder builder(String size) { return new Builder(size); }

    public static final class Builder {
        private final String size;
        private boolean cheese = true;                   // defaults
        private boolean pepperoni = false;
        private final List<String> extras = new ArrayList<>();

        private Builder(String size) { this.size = Objects.requireNonNull(size); }

        public Builder cheese(boolean v)    { this.cheese = v; return this; }
        public Builder pepperoni(boolean v) { this.pepperoni = v; return this; }
        public Builder extra(String e)      { this.extras.add(e); return this; }

        public Pizza build() {
            if (pepperoni && !cheese) throw new IllegalStateException("Pepperoni requires cheese");
            return new Pizza(this);
        }
    }
}

Pizza pizza = Pizza.builder("large")
        .pepperoni(true)
        .extra("olives")
        .build();
```

Why this works well:

- Call sites name every value.
- Required values go in the builder's constructor; optional values have defaults.
- `build()` is the single place to **validate**, so an invalid `Pizza` can never exist.
- The result is **immutable**.

```text
Client ──► Builder.size(...).cheese(...).extra(...) ──► build() ──► Pizza (immutable, validated)
```

## Records and the builder

Java records already give you concise, immutable data classes, and a compact constructor can validate:

```java
public record Address(String street, String city, String zip) {
    public Address {
        Objects.requireNonNull(street);
        Objects.requireNonNull(city);
    }
}
```

For records with few fields, a builder is usually unnecessary. Reach for one when there are many fields, many optional ones, or step-wise construction.

## The Director (classic GoF structure)

The original pattern includes a **Director** that knows a *recipe* for building and drives a builder interface:

```java
interface DocumentBuilder {
    void addTitle(String t);
    void addParagraph(String p);
    Document build();
}

class ReportDirector {
    Document buildMonthlyReport(DocumentBuilder b, Data data) {
        b.addTitle("Monthly report");
        b.addParagraph(data.summary());
        return b.build();
    }
}
```

With different builders (`PdfDocumentBuilder`, `HtmlDocumentBuilder`), the same director produces different representations. This form matters when the *same construction steps* yield different products. Most day-to-day builders skip the director.

## Variations

| Form | Use for |
|---|---|
| Fluent builder (above) | Immutable objects with optional fields |
| Step builder | Forcing a required order of steps through types (advanced) |
| Test data builder | Creating objects in tests with sensible defaults, overriding only what matters |
| Library builders | Used by frameworks for requests and configuration (for example, HTTP request builders) |

A test data builder is a very practical use:

```java
User user = aUser().withRole(ADMIN).build();    // other fields get defaults
```

## Builder vs other options

| Option | Best for |
|---|---|
| Constructor | Few required parameters |
| Static factory method | Named creation, caching, few parameters |
| Record | Simple immutable data |
| **Builder** | Many parameters, several optional, validation, readability |
| Setters | Mutable objects where partial state is acceptable |

## Common mistakes

- **A builder for a three-field class**, which adds boilerplate for no gain.
- **Skipping validation** in `build()`, producing an object in an invalid state.
- **Reusing a builder** after `build()` when it holds mutable lists, so later changes leak into earlier objects. Copy collections defensively (`List.copyOf`).
- **Builder that returns a mutable object**, losing the main benefit.
- **Not making required fields required**, leaving everything optional and failing late at runtime.
- **Thread-unsafe sharing:** builders are not designed to be shared across threads.

## Quick Summary

- Builder creates complex objects step by step with readable, named calls.
- Its everyday value: immutability, optional parameters and validation in `build()`.
- The Director is only needed when the same steps produce different representations.
- For small classes, use a constructor, static factory or record instead.

## Related

[Factory Method](./02-factory-method.md) · [Abstract Factory](./03-abstract-factory.md) · [Test doubles and test data](../09-testing-and-debugging/02-test-doubles.md)

## Next

[05-prototype.md](./05-prototype.md)
