# OOP Pillars

Encapsulation, abstraction, inheritance and polymorphism are the four ideas every object-oriented language is built around. They are often taught as definitions to memorize. It's more useful to remember what *problem* each one solves, because that is what SOLID and design patterns build on.

| Pillar | Problem it solves |
|---|---|
| Encapsulation | Outside code breaking an object's internal rules |
| Abstraction | Callers being forced to know how something works |
| Inheritance | Duplicating behavior across related types |
| Polymorphism | Callers needing `if/else` on type to do the right thing |

## Encapsulation

An object owns its state and enforces its own rules. Outside code asks the object to do something; it doesn't reach in and change fields.

```java
public class BankAccount {
    private long balanceCents;

    public void withdraw(long amountCents) {
        if (amountCents <= 0) throw new IllegalArgumentException("Amount must be positive");
        if (amountCents > balanceCents) throw new IllegalStateException("Insufficient funds");
        balanceCents -= amountCents;
    }

    public long balanceCents() { return balanceCents; }
}
```

The rule "balance never goes negative" lives in one place. If `balanceCents` were public, every caller would have to remember it.

**Common mistake:** private fields plus a public getter *and* setter for each one. That is a public field with extra steps. Expose behavior (`withdraw`), not raw state (`setBalance`).

## Abstraction

Expose *what* something does and hide *how*. Callers depend on the abstraction, so the implementation can change freely.

```java
public interface PaymentGateway {
    PaymentResult charge(String customerId, long amountCents);
}
```

The caller doesn't know whether this talks to Stripe, a bank API or a test stub. Abstraction is the basis of [programming to interfaces](./04-programming-to-interfaces.md).

**Abstraction vs encapsulation:** abstraction is about the *outside view* (what the caller sees); encapsulation is about *protecting the inside* (how state is guarded). They work together but are not the same thing.

## Inheritance

A subclass reuses and extends a superclass, and can be used wherever the superclass is expected.

```java
public abstract class Account {
    protected long balanceCents;
    public abstract long monthlyFee();
}

public class SavingsAccount extends Account {
    @Override public long monthlyFee() { return 0; }
}
```

Inheritance gives you two things at once: **code reuse** and a **subtype relationship**. Mixing them up is the source of most inheritance problems. If you only want reuse, inheritance is usually the wrong tool; see [composition vs inheritance](./02-composition-vs-inheritance.md).

## Polymorphism

Different types respond to the same call in their own way, and the caller doesn't need to know which type it has.

```java
List<Shape> shapes = List.of(new Circle(2), new Rectangle(3, 4));

double total = 0;
for (Shape s : shapes) {
    total += s.area();   // each object runs its own area()
}
```

Without polymorphism this becomes:

```java
if (s instanceof Circle c) { ... }
else if (s instanceof Rectangle r) { ... }   // grows with every new type
```

Polymorphism is what makes the Open/Closed Principle possible: add a new `Triangle` and the loop above doesn't change.

### How dispatch works

```text
shape.area()
    ↓
JVM looks at the runtime type of the object (not the variable's declared type)
    ↓
calls Circle.area() or Rectangle.area()
```

This is *runtime* (dynamic) dispatch for overridden instance methods. Overloaded methods are chosen at compile time from the declared argument types, which is a frequent interview trap.

## Common mistakes

- **Public fields or setters everywhere.** Breaks encapsulation.
- **Interfaces that leak implementation** (for example, a method named `executeSqlQuery` on a general repository). The abstraction isn't abstract.
- **Inheriting just to reuse a method.** Creates a type relationship you didn't mean.
- **`instanceof` chains.** Usually a sign polymorphism is missing.

## Quick Summary

- Encapsulation: keep rules next to the data they protect.
- Abstraction: depend on what, not how.
- Inheritance: powerful, but couples subclass to superclass; use sparingly.
- Polymorphism: removes type checks and enables extension without modification.

## Next

[02-composition-vs-inheritance.md](./02-composition-vs-inheritance.md)
