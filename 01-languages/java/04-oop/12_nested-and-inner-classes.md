# Nested and Inner Classes

A class can be declared **inside** another class. Java has four kinds; they differ in whether they need an outer **instance**, where they are declared, and what they can capture.

```
Nested classes
├── static nested class        static class Node { }       no outer instance needed
└── inner classes (non-static)
    ├── member inner class     class Iter { }              tied to an outer instance
    ├── local class            class Helper { } in a method
    └── anonymous class        new Runnable() { ... }      one-off, no name
```

## Why nest classes?

| Reason | Example |
|--------|---------|
| **Grouping**: a class is only useful to one other class | `LinkedList.Node`, `Map.Entry` |
| **Encapsulation**: hide helpers from the rest of the package | A `private` nested helper |
| **Access to private members** of the outer class | Iterators over the outer's internal array |
| **Readability**: keep related code together | Builders, small value types |

## 1. Static nested class

Declared with `static`. It behaves like a top-level class that happens to live inside another, with **no** reference to an outer instance.

```java
public class LinkedStack<E> {
    private Node<E> head;

    private static class Node<E> {              // static nested: needs no LinkedStack instance
        final E value;
        final Node<E> next;
        Node(E value, Node<E> next) { this.value = value; this.next = next; }
    }

    public void push(E e) { head = new Node<>(e, head); }
}

LinkedStack.Node<String> n = new LinkedStack.Node<>("x", null);   // only if Node is not private
```

- Can access the outer class's **static** members, including private ones
- Cannot access the outer's instance members without an object reference
- Can have any access modifier
- **Default choice**: if the nested class does not need the outer instance, make it `static`

Common uses: nodes, entries, **builders** ([24-design-patterns/01-creational/03_builder.md](../24-design-patterns/01-creational/03_builder.md)), result types, small helpers.

## 2. Member inner class (non-static)

Each instance is **bound to an instance of the outer class** and can use its fields and methods directly.

```java
public class Bank {
    private final String name;
    private final List<Account> accounts = new ArrayList<>();
    Bank(String name) { this.name = name; }

    class Account {                                   // inner class
        private double balance;
        String bankName() { return name; }            // uses Bank.this.name
        String describe() { return Bank.this.name + ":" + balance; }   // explicit form
    }

    Account open() { Account a = new Account(); accounts.add(a); return a; }
}

Bank bank = new Bank("First");
Bank.Account acc = bank.new Account();                // creating from outside needs an outer instance
Bank.Account acc2 = bank.open();                      // usual: the outer creates it
```

```
  Bank instance ◄────── hidden reference ────── Account instance
```

- The compiler adds a hidden reference to the outer instance (`Outer.this`)
- Inner classes **cannot** declare static members before Java 16; since Java 16, they may declare static members (including records and enums)
- Typical use: iterators over the outer collection, event handlers

### The hidden-reference cost

```java
class Cache {
    private final byte[] huge = new byte[100_000_000];
    class Entry { }                                   // every Entry keeps the whole Cache alive
    Entry makeEntry() { return new Entry(); }
}
```

A long-lived inner object prevents the outer object from being garbage collected: a classic **memory leak**. If you do not need `Outer.this`, declare the class `static`.

## 3. Local class

Declared inside a method (or any block). It is visible only there and can **capture** local variables that are effectively final ([03_final.md](./03_final.md)).

```java
List<String> sortByLength(List<String> words) {
    class ByLength implements Comparator<String> {
        public int compare(String a, String b) { return Integer.compare(a.length(), b.length()); }
    }
    List<String> copy = new ArrayList<>(words);
    copy.sort(new ByLength());
    return copy;
}
```

Rare today; lambdas and private methods usually replace them. Local **records, enums and interfaces** are also allowed (Java 16+) and are implicitly static.

## 4. Anonymous class

A class with **no name**, declared and instantiated in one expression, extending a class or implementing an interface.

```java
Runnable r = new Runnable() {                         // implements Runnable
    @Override public void run() { System.out.println("hi"); }
};

Shape unit = new Shape("unit") {                      // extends an abstract class
    @Override double area() { return 1; }
};

Comparator<String> byLen = new Comparator<>() {       // diamond with anonymous classes: Java 9+
    @Override public int compare(String a, String b) { return a.length() - b.length(); }
};
```

### Anonymous class vs lambda

| | Lambda | Anonymous class |
|---|--------|-----------------|
| Works for | **Functional** interfaces only | Any interface or class (several methods, abstract classes, fields) |
| `this` means | The **enclosing** object | The **anonymous object** itself |
| State | None | Can have fields and initializer blocks |
| Syntax | Concise | Verbose |
| Compiled to | `invokedynamic` | A real class file (`Outer$1.class`) |

```java
button.onClick(e -> handle(e));                       // prefer a lambda when possible
```

Use an anonymous class when you need several methods, state, or a subclass of a class ([09-functional-java/00_lambda-expressions.md](../09-functional-java/00_lambda-expressions.md)).

## What nested classes can access

| | Outer's static members | Outer's instance members | Outer's locals |
|---|:---------------------:|:------------------------:|:--------------:|
| Static nested | ✅ | ❌ (needs an object) | n/a |
| Inner (member) | ✅ | ✅ | n/a |
| Local / anonymous | ✅ | ✅ (in instance contexts) | ✅ if effectively final |

The outer class can also access the **private** members of its nested classes. Private members of nested classes are accessible within the whole top-level class (compiled via synthetic accessors or nestmates, Java 11+).

## Shadowing

```java
class Outer {
    int x = 1;
    class Inner {
        int x = 2;
        void show(int x) {
            System.out.println(x);              // parameter
            System.out.println(this.x);         // Inner's field
            System.out.println(Outer.this.x);   // Outer's field
        }
    }
}
```

## Naming and class files

Nested classes compile to separate files: `Outer$Inner.class`, `Outer$1.class` (anonymous), `Outer$1Local.class`. Refer to them as `Outer.Inner` from other classes.

## Nested interfaces, enums and records

These are **implicitly static**:

```java
class Order {
    enum Status { NEW, PAID }                  // implicitly static
    record Line(String sku, int qty) { }       // implicitly static
    interface Listener { void changed(Order o); }
}
Order.Status s = Order.Status.NEW;
```

## Choosing

| If the class | Use |
|--------------|-----|
| Is a helper that does not need the outer object | **static nested** (default) |
| Needs the outer instance's state, one per outer object (iterators) | **inner** |
| Is used in one method only | **local** class, or a lambda/private method |
| Is a one-off implementation of a type, needing more than a lambda | **anonymous** |
| Is useful on its own or reused widely | A **top-level** class |
| Is just a data carrier | A nested **record** |

Do not nest deeply: one level is almost always enough.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Using an inner class when `static` would do | Hidden outer reference, memory leak, awkward construction | Make it `static` |
| `new Inner()` from a static method | `an enclosing instance that contains Outer.Inner is required` | Create an outer instance (`outer.new Inner()`) or make the class static |
| Capturing a non-effectively-final local | Compile error | Copy to a final local, or restructure |
| Expecting `this` in a lambda to refer to the lambda | It refers to the enclosing object | Use an anonymous class if you need its own `this` |
| Serializing inner or anonymous classes | Serializes the outer instance too | Avoid; use static nested or top-level |
| Deeply nested classes | Hard to read | Extract to separate files |
| Anonymous class for a single-method interface | Verbose | Lambda |
| Giving a long-lived inner object to other code | Outer instance cannot be collected | Static nested class, or pass only the data needed |
| Overusing nested classes to hide complexity | Giant files | Split into top-level classes |

## Key takeaways

- Four kinds: static nested, inner, local, anonymous
- Default to **static** nested classes; use inner only when you truly need the outer instance
- Inner objects hold a hidden reference to the outer one: beware leaks
- Local and anonymous classes capture effectively final variables; prefer lambdas for single-method interfaces
- Nested enums, records and interfaces are implicitly static

**Next:** [The Object Class](./13_object-class.md)
