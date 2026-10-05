# Method References

A **method reference** is a shorthand for a lambda that does nothing but call an existing method. The syntax is `Target::method`. It does not call the method; it creates a functional-interface instance that will.

```java
list.forEach(s -> System.out.println(s));          // lambda
list.forEach(System.out::println);                 // method reference: same meaning, less noise

names.stream().map(s -> s.length());               // lambda
names.stream().map(String::length);                // method reference
```

## The four kinds

| Kind | Syntax | Lambda equivalent | Example |
|------|--------|-------------------|---------|
| **1. Static method** | `Class::staticMethod` | `x -> Class.staticMethod(x)` | `Integer::parseInt` |
| **2. Instance method of a *specific object* (bound)** | `object::method` | `x -> object.method(x)` | `System.out::println`, `this::validate` |
| **3. Instance method of an *arbitrary object of a type* (unbound)** | `Class::instanceMethod` | `(a, b) -> a.method(b)` | `String::length`, `String::compareToIgnoreCase` |
| **4. Constructor** | `Class::new` | `x -> new Class(x)` | `ArrayList::new`, `Person::new`, `int[]::new` |

### 1. Static method reference

The method's parameters match the functional interface's parameters one-for-one.

```java
Function<String, Integer> parse = Integer::parseInt;       // s -> Integer.parseInt(s)
BinaryOperator<Integer> sum = Integer::sum;                // (a, b) -> Integer.sum(a, b)
Predicate<String> isNumber = Utils::looksNumeric;
List<Integer> nums = strings.stream().map(Integer::parseInt).toList();
Math::max                                                    // (a, b) -> Math.max(a, b)
```

### 2. Bound instance method: a specific object

The receiver is fixed when the reference is created; the interface's parameters become the **method's arguments**.

```java
Consumer<String> out = System.out::println;                // s -> System.out.println(s)
String prefix = "ID-";
Function<String, Boolean> startsWithId = prefix::startsWith;      // s -> prefix.startsWith(s)
Predicate<String> inSet = allowedSet::contains;                   // s -> allowedSet.contains(s)
Supplier<Integer> size = list::size;                              // () -> list.size()

class OrderService {
    void run(List<Order> orders) { orders.forEach(this::process); }   // this::process
    private void process(Order o) { ... }
}
```

**The receiver expression is evaluated once, when the reference is created.** If `object` is `null` then, you get a `NullPointerException` immediately (not at call time). Later changes to which object the variable points to are not seen (similar to a captured effectively-final variable).

### 3. Unbound instance method: any object of the type

The **first parameter of the functional interface becomes the receiver**, the rest become arguments. This is the trickiest form.

```java
Function<String, Integer> len = String::length;                   // s -> s.length()
Function<String, String> upper = String::toUpperCase;             // s -> s.toUpperCase()
BiPredicate<String, String> eq = String::equalsIgnoreCase;        // (a, b) -> a.equalsIgnoreCase(b)
Comparator<String> cmp = String::compareToIgnoreCase;             // (a, b) -> a.compareToIgnoreCase(b)
Function<Person, String> name = Person::name;                     // p -> p.name()
```

```java
people.sort(Comparator.comparing(Person::name));                  // very common
names.stream().map(String::trim).filter(Predicate.not(String::isEmpty)).toList();
```

How to tell kinds 1 and 3 apart: with `Class::method`, if `method` is **static** it is kind 1, if it is an **instance** method it is kind 3 (the first parameter is the object).

### 4. Constructor references

```java
Supplier<List<String>> newList = ArrayList::new;                  // () -> new ArrayList<>()
Function<String, Person> create = Person::new;                    // name -> new Person(name)
BiFunction<String, Integer, Person> create2 = Person::new;        // (n, a) -> new Person(n, a)
Function<Integer, List<String>> sized = ArrayList::new;           // capacity -> new ArrayList<>(capacity)
IntFunction<int[]> makeArray = int[]::new;                        // n -> new int[n]

String[] arr = stream.toArray(String[]::new);                     // array constructor reference: very common
Set<String> set = list.stream().collect(Collectors.toCollection(TreeSet::new));
Map<String, List<Item>> m = items.stream().collect(Collectors.groupingBy(Item::category, TreeMap::new, Collectors.toList()));
```

Which constructor is called is determined by the functional interface's parameters, so the same `Person::new` can be a `Function<String, Person>` or a `BiFunction<String, Integer, Person>`.

## Special forms

```java
super::toString                  // call the superclass version, inside an instance method
this::helper                     // bound to the current object
OuterClass.this::method          // bound to an enclosing instance
String[]::clone                  // arrays work too
Outer.Inner::new                 // nested class constructors
```

## Lambda or method reference?

| Prefer a **method reference** when | Prefer a **lambda** when |
|------------------------------------|---------------------------|
| It just forwards its arguments to one existing method | You need to **combine** or modify arguments (`x -> x * 2`) |
| The method name explains the intent (`Person::name`) | The expression has more than one call or operator |
| It avoids a meaningless parameter name (`x`, `s`) | The method reference would be longer or less clear |
| It removes noise (`System.out::println`) | Overloads would make the reference ambiguous |

```java
.map(p -> p.getName())                    →  .map(Person::getName)
.filter(s -> !s.isEmpty())                →  .filter(Predicate.not(String::isEmpty))      (or keep the lambda)
.map(x -> x * 2)                          // keep as a lambda: no existing method does this
.forEach(item -> process(item, config))   // keep: two arguments, one is captured
```

Readability decides; method references are not always better. Extracting a longer lambda into a **named method** and referencing it is often the best of both ([00_lambda-expressions.md](./00_lambda-expressions.md)).

## Compile-time matching and ambiguity

The compiler picks the method by looking at the functional interface's parameter types. Problems arise when several overloads fit:

```java
Function<Integer, String> f = Integer::toString;
// ERROR: reference to toString is ambiguous:
//   static Integer.toString(int)   and   instance Integer.toString()   both match a single Integer argument

Function<Integer, String> g = String::valueOf;     // OK: unambiguous
Function<Integer, String> h = i -> i.toString();   // OK: lambda is explicit
Function<Integer, String> k = Object::toString;    // OK
```

Fixes: use a lambda, or pick a method that has no static/instance clash (`String::valueOf`).

Other gotchas:

```java
List<String> list = ...;
list.stream().map(String::new);          // which constructor? Depends on the stream element type; may be ambiguous
list.forEach(this::process);             // if process is overloaded for (String) and (Object), the compiler picks the best match for String
```

## Method references and generics

```java
Function<String, Integer> f = Integer::valueOf;                    // overloaded: valueOf(String), valueOf(int); the target type picks the String one
Supplier<Map<String, List<Integer>>> s = HashMap::new;             // diamond inferred
Comparator<Person> c = Comparator.comparing(Person::name);         // T inferred from the method reference
```

When the receiver type is needed for inference, method references help where lambdas fail:

```java
people.sort(Comparator.comparing(p -> p.name()).reversed());       // ERROR: p is Object (see 08-collections/05)
people.sort(Comparator.comparing(Person::name).reversed());        // OK
```

## Debugging method references

Stack traces show the referenced method directly (often clearer than `lambda$main$0`). A `NullPointerException` thrown **at creation** points to a `null` bound receiver; one thrown **during use** points to a `null` argument or element.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `Integer::toString` for `Function<Integer, String>` | `reference to toString is ambiguous` | `String::valueOf` or a lambda |
| Confusing bound (`obj::m`) with unbound (`Type::m`) forms | Wrong receiver or arity errors | Remember: unbound = first argument is the receiver |
| Bound receiver is `null` | `NullPointerException` when the reference is created | Check before creating it |
| Expecting a bound receiver to track a reassigned variable | Uses the original object | Use a lambda that reads the variable (if it is effectively final, same result) |
| Method reference to an overloaded method with several fitting overloads | Ambiguity errors | Lambda with explicit parameters |
| Method reference where a transformation of arguments is needed | Does not compile | Use a lambda |
| `Person::new` with no matching constructor | `invalid constructor reference` | Add the constructor or use a lambda |
| Overusing references for readability-neutral code | Terse, puzzling pipelines | Choose clarity |
| Using `System.out::println` on a `List<char[]>` expecting characters | Calls `println(Object)` (or `println(char[])` unexpectedly) | Know which overload matches |

## Key takeaways

- A method reference replaces a lambda that only calls one existing method
- Four kinds: **static** (`Integer::parseInt`), **bound instance** (`System.out::println`), **unbound instance** (`String::length`, the first parameter is the receiver), **constructor** (`ArrayList::new`, `int[]::new`)
- The functional interface decides which overload is chosen; ambiguous overloads need a lambda
- Prefer references when they make the intent clearer; keep lambdas for real logic

**Next:** [Optional](./03_optional.md)