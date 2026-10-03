# Classes and Objects

A **class** is a blueprint that defines **fields** (data) and **methods** (behavior). An **object** is one concrete instance of a class, created with `new`. A **variable** of a class type holds a **reference** to an object, not the object itself.

```java
public class BankAccount {
    private String owner;              // fields: the state
    private double balance;

    public void deposit(double amount) {   // methods: the behavior
        balance += amount;
    }

    public double getBalance() {
        return balance;
    }
}
```

```java
BankAccount acc = new BankAccount();   // create an object
acc.deposit(100);
System.out.println(acc.getBalance());  // 100.0
```

## Class vs object vs reference

```
  class BankAccount            objects (instances, on the heap)         variables (references)
  ┌─────────────────┐          ┌──────────────────────┐
  │ fields          │  new ──► │ owner=null balance=0 │ ◄──────────────── acc1
  │ methods         │          └──────────────────────┘
  └─────────────────┘          ┌──────────────────────┐
     (blueprint)       new ──► │ owner=null balance=50│ ◄──────────────── acc2, acc3
                               └──────────────────────┘
```

| Concept | Meaning |
|---------|---------|
| Class | The type; describes what every instance has and can do |
| Object | A specific instance; has its own copy of every instance field |
| Reference | A variable that points to an object, or to `null` |
| Instance | Synonym for object ("an instance of `BankAccount`") |

```java
BankAccount a = new BankAccount();
BankAccount b = a;           // b and a point to the SAME object
b.deposit(50);
a.getBalance();              // 50.0
BankAccount c = new BankAccount();   // a different object
```

Objects live on the **heap** and are reclaimed by the garbage collector when no reference reaches them ([15-jvm-internals/04_garbage-collection.md](../15-jvm-internals/04_garbage-collection.md)). Local reference variables live on the stack ([pass-by-value](../02-methods/01_pass-by-value.md)).

## Declaring a class

```java
public class Person {
    // fields (instance variables)
    private String name;
    private int age;

    // constructor
    public Person(String name, int age) {
        this.name = name;
        this.age = age;
    }

    // methods
    public String getName() { return name; }

    public void haveBirthday() { age++; }

    public boolean isAdult() { return age >= 18; }

    @Override
    public String toString() { return name + " (" + age + ")"; }
}
```

| Member | Purpose |
|--------|---------|
| Fields | The object's state; each object has its own copy |
| Constructors | Initialize a new object ([next file](./01_constructors.md)) |
| Instance methods | Behavior that reads or changes the state of one object |
| Static members | Belong to the class itself ([02_static.md](./02_static.md)) |
| Nested types | Classes declared inside ([12](./12_nested-and-inner-classes.md)) |

One public top-level class per file, with the same name as the file ([syntax](../01-fundamentals/00_syntax-and-program-structure.md)).

## Creating and using objects

```java
Person p = new Person("Ada", 36);   // 1. allocate  2. initialize fields  3. run constructor  4. assign reference
p.haveBirthday();                    // call a method with the dot operator
System.out.println(p.getName());     // Ada
System.out.println(p);               // Ada (37): toString() is called implicitly
```

### Default values

Fields get defaults automatically (`0`, `false`, `'\u0000'`, `null`); local variables do not ([variables](../01-fundamentals/01_variables-and-data-types.md)).

```java
class Item { int qty; String label; boolean ready; }
Item i = new Item();
i.qty;      // 0
i.label;    // null
i.ready;    // false
```

### `null` and `NullPointerException`

```java
Person p = null;           // no object
p.getName();               // NullPointerException

Person q;                  // declared, not assigned
q.getName();               // compile error: might not have been initialized (local)
```

Check for `null` where it is legitimately possible, and prefer designs where it is not (empty collections, `Optional`: [09-functional-java/03_optional.md](../09-functional-java/03_optional.md)).

## The `this` keyword

`this` refers to the **current object**.

| Use | Example |
|-----|---------|
| Distinguish a field from a parameter of the same name | `this.name = name;` |
| Pass the current object | `registry.add(this);` |
| Return the current object (fluent / chaining) | `return this;` |
| Call another constructor | `this(name, 0);` ([constructors](./01_constructors.md)) |

```java
class Person {
    private String name;
    Person(String name) {
        name = name;            // BUG: assigns the parameter to itself; the field stays null
        this.name = name;       // correct
    }
}
```

### Method chaining

```java
class Pizza {
    private final StringBuilder desc = new StringBuilder("Pizza");
    Pizza add(String topping) { desc.append(" + ").append(topping); return this; }
    @Override public String toString() { return desc.toString(); }
}
new Pizza().add("cheese").add("olives");    // Pizza + cheese + olives
```

## Identity, equality, state

| Notion | Meaning | How to test |
|--------|---------|-------------|
| **Identity** | Same object in memory | `a == b` |
| **Equality** | Same logical value | `a.equals(b)` (needs a correct `equals`: [14](./14_equals-and-hashcode.md)) |
| **State** | Current field values | Getters, `toString` |

```java
Person a = new Person("Ada", 36);
Person b = new Person("Ada", 36);
a == b;            // false: two objects
a.equals(b);       // false until you override equals (default is identity)
```

## Arrays and collections of objects

```java
Person[] people = new Person[3];       // three null references, no Person objects yet
people[0] = new Person("Ada", 36);

List<Person> list = new ArrayList<>();
list.add(new Person("Linus", 17));
for (Person p : list) System.out.println(p);
```

## Designing a class

| Guideline | Why |
|-----------|-----|
| Make fields `private` | Control how state changes ([encapsulation](./04_encapsulation-and-access-modifiers.md)) |
| Give each class **one** clear responsibility | Easier to understand and test |
| Name classes as nouns (`Invoice`), methods as verbs (`pay()`) | Reads naturally |
| Establish valid state in the constructor | Objects are never half-built |
| Prefer immutability when practical | Safe sharing ([03_final.md](./03_final.md), [23-design-and-clean-code/04_immutability.md](../23-design-and-clean-code/04_immutability.md)) |
| Override `toString` | Debugging and logging ([13](./13_object-class.md)) |
| Keep behavior with the data it uses | Avoids "anemic" data bags |

Short data carriers can be written as records: [12-modern-java/01_records.md](../12-modern-java/01_records.md).

## Anonymous objects

```java
new Person("Temp", 1).haveBirthday();         // no variable; garbage once the statement ends
System.out.println(new Person("Ada", 36));
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Using a reference before `new` | `NullPointerException` | Create the object first |
| `name = name;` in a constructor or setter | Field never set | `this.name = name;` |
| Comparing objects with `==` | `false` for equal data | `equals` |
| Public fields | Anyone can break invariants | `private` + methods |
| Confusing "class" and "object" | Mixed-up terminology | The class is the blueprint, objects are instances |
| Assuming `b = a` copies the object | Both change together | Create a copy ([15](./15_clone-and-copying.md)) |
| Forgetting that arrays of objects start as `null` | `NullPointerException` | Create each element |
| One class doing everything ("god class") | Unmaintainable | Split by responsibility |
| Calling instance methods without an object from `static main` | Compile error | `new Foo().method()` ([methods](../02-methods/00_methods.md)) |

## Key takeaways

- A class defines fields and methods; `new` creates an object with its own copy of the instance fields
- Variables hold references; assignment copies the reference, not the object
- `this` is the current object; use it to avoid shadowing and for chaining
- `==` tests identity; `equals` tests logical equality (and needs overriding)
- Keep fields private, keep classes focused

**Next:** [Constructors](./01_constructors.md)
