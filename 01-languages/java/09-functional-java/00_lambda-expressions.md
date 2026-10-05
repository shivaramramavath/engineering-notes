# Lambda Expressions

A **lambda expression** is a compact way to write an **anonymous function**: parameters, an arrow, and a body. It can be passed around and called later. In Java, a lambda is always the implementation of a **functional interface** (an interface with exactly one abstract method).

```java
Runnable r = () -> System.out.println("hello");           // no parameters
Comparator<String> byLength = (a, b) -> a.length() - b.length();
Function<Integer, Integer> square = x -> x * x;

r.run();
square.apply(5);                // 25
```

```
  (a, b)   ->   a.length() - b.length()
  └─────┘ └┘    └──────────────────────┘
 parameters arrow          body
```

## Syntax

| Form | Example |
|------|---------|
| No parameters | `() -> 42` |
| One parameter (parentheses optional) | `x -> x * 2` or `(x) -> x * 2` |
| Several parameters | `(a, b) -> a + b` |
| Explicit types | `(int a, int b) -> a + b` |
| `var` parameters (Java 11+) | `(var a, var b) -> a + b` (needed only to add annotations) |
| Expression body (value is returned) | `x -> x + 1` |
| Block body (use `return` for a value) | `x -> { int y = x + 1; return y; }` |
| `void` block body | `s -> { log(s); count++; }` |

Rules:
- Either **all** parameters declare types (or all use `var`), or **none** do
- An expression body implicitly returns its value; a `void` method accepts any expression statement as the body
- A block body needs `return` when the interface method returns a value

```java
Runnable r1 = () -> System.out.println("x");          // OK: void; expression used as a statement
Supplier<String> s1 = () -> "x";                      // OK: returns "x"
Supplier<String> s2 = () -> { return "x"; };          // same, block form
Consumer<String> c = str -> { System.out.println(str); };
```

## A lambda needs a target type

A lambda has **no type of its own**; the compiler infers it from the **context** (the *target type*):

```java
Runnable r = () -> doWork();                          // assignment
executor.submit(() -> doWork());                      // method argument
return x -> x * 2;                                    // return value (method returns Function<Integer,Integer>)
Object o = (Runnable) () -> doWork();                 // cast
// Object bad = () -> doWork();                       // ERROR: Object is not a functional interface
// var v = () -> 1;                                   // ERROR: cannot infer the type
```

The same lambda text can mean different things in different contexts:

```java
Callable<Integer> c = () -> 42;                       // returns a value, may throw checked exceptions
Supplier<Integer> s = () -> 42;
```

Parameter and return types are inferred from the interface method, so you rarely write them. See [01_functional-interfaces.md](./01_functional-interfaces.md).

## Everyday uses

```java
// Sorting
people.sort((a, b) -> a.name().compareTo(b.name()));
people.sort(Comparator.comparing(Person::name));                 // preferred: see 02_method-references

// Iteration, conditional removal, transformation
list.forEach(s -> System.out.println(s));
list.removeIf(s -> s.isBlank());
list.replaceAll(s -> s.toUpperCase());
map.forEach((k, v) -> System.out.println(k + "=" + v));
map.computeIfAbsent(key, k -> new ArrayList<>()).add(value);

// Streams
names.stream().filter(n -> n.length() > 3).map(n -> n.toUpperCase()).toList();

// Threads and tasks
new Thread(() -> System.out.println("in a thread")).start();
executor.submit(() -> compute());

// Callbacks and event handlers
button.onClick(event -> handle(event));
```

## Capturing variables (closures)

A lambda can use variables from the enclosing scope. This is called **capturing**; the lambda is a **closure**.

```java
String prefix = "Hello, ";                               // local variable
Function<String, String> greet = name -> prefix + name;  // captures prefix
greet.apply("Ada");                                      // "Hello, Ada"
```

### Captured locals must be effectively final

```java
int count = 0;
Runnable r = () -> count++;        // ERROR: local variables referenced from a lambda expression must be final or effectively final

int base = 10;                     // never reassigned: effectively final
Supplier<Integer> s = () -> base + 1;    // OK
// base = 20;                      // would make the lambda above fail to compile
```

Why: a lambda may run **later**, even on another thread, after the method that created it has returned. Local variables live on the stack and cannot be shared safely; Java therefore captures the **value** and requires it not to change.

| Can capture and modify | Can capture but not modify |
|------------------------|----------------------------|
| **Fields** (instance and static) | **Local variables** and parameters (must be effectively final) |
| Elements of a captured array/object (`arr[0]++`, `list.add(...)`) | |

Workarounds when you need mutable state:

```java
AtomicInteger count = new AtomicInteger();
list.forEach(x -> count.incrementAndGet());              // mutable holder (also safe across threads)

int[] box = {0};
list.forEach(x -> box[0]++);                             // works but is hacky; not thread-safe
```

Better: avoid mutation entirely, using streams (`list.stream().count()`, `reduce`, collectors) ([05](./05_stream-operations.md)).

### Lambdas and local variable names

A lambda body is part of the enclosing scope, so its parameters and locals **cannot shadow** enclosing locals:

```java
String name = "x";
Function<String, String> f = name -> name.trim();        // ERROR: variable name is already defined
```

## `this` inside a lambda

In a lambda, `this` is the **enclosing** object (a lambda is not a new scope or class). In an anonymous class, `this` is the anonymous object itself.

```java
class Greeter {
    String name = "outer";

    void demo() {
        Runnable lambda = () -> System.out.println(this.name);            // "outer"
        Runnable anon = new Runnable() {
            String name = "inner";
            public void run() { System.out.println(this.name); }          // "inner"
        };
    }
}
```

## Lambdas vs anonymous classes

| | Lambda | Anonymous class |
|---|--------|-----------------|
| Works for | **Functional** interfaces only | Any interface or class |
| Syntax | Concise | Verbose |
| `this` | The enclosing instance | The anonymous instance |
| Can have fields / multiple methods | No | Yes |
| Compiled as | A private method + `invokedynamic` (no extra class file per lambda) | A separate `Outer$1.class` |
| Shadowing of enclosing names | Not allowed | Allowed |
| Identity | Unspecified (do not rely on `==` or `hashCode`) | A normal object |

Prefer lambdas for single-method interfaces; keep anonymous classes for the few cases that need state, several methods, or a class to extend ([04-oop/12_nested-and-inner-classes.md](../04-oop/12_nested-and-inner-classes.md)).

## Exceptions in lambdas

A lambda may throw **unchecked** exceptions freely. Checked exceptions are allowed only if the functional interface's method declares them (`Callable` does, `Function` does not):

```java
Callable<String> c = () -> Files.readString(path);               // OK: Callable.call() throws Exception
Function<Path, String> f = p -> Files.readString(p);             // ERROR: unreported exception IOException

Function<Path, String> ok = p -> {
    try { return Files.readString(p); }
    catch (IOException e) { throw new UncheckedIOException(e); } // wrap
};
```

A reusable wrapper is shown in [08_functional-patterns.md](./08_functional-patterns.md) and [06-exceptions-and-debugging/02_throw-and-throws.md](../06-exceptions-and-debugging/02_throw-and-throws.md).

## Recursion

A lambda cannot refer to itself by a local name. Use a field, or a helper method:

```java
static final Function<Integer, Integer> FACT =
        n -> n <= 1 ? 1 : n * Main.FACT.apply(n - 1);            // qualified reference to the static field

static int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); } // a plain method is usually clearer
```

## Writing good lambdas

| Guideline | Why |
|-----------|-----|
| Keep them **short** (one expression, or a few lines) | Long lambdas hide logic and make stack traces hard to read |
| Extract longer bodies into a **named method**, then use a method reference | Reusable, testable, named: `.map(this::normalize)` |
| Avoid **side effects** (modifying shared state) | Surprising behavior, impossible to parallelize ([07](./07_parallel-streams.md)) |
| Do not **capture mutable state** | Concurrency bugs |
| Prefer **method references** when the lambda only forwards (`x -> foo(x)` → `this::foo`) | Reads better ([02](./02_method-references.md)) |
| Do not nest lambdas deeply | Readability |
| Choose meaningful **parameter names** (`order -> ...`, not `x -> ...`) in non-trivial code | Intent |
| Do not log or throw inside long chains without context | Debugging gets harder |

```java
// Hard to read
orders.stream().filter(o -> o.getItems().stream().anyMatch(i -> i.getPrice() > 100 && i.getCategory().equals("A")) && o.getCustomer().getAge() > 18).toList();

// Better: named pieces
Predicate<Item> premiumItem = i -> i.getPrice() > 100 && i.getCategory().equals("A");
Predicate<Order> adultWithPremium = o -> o.getCustomer().getAge() > 18 && o.getItems().stream().anyMatch(premiumItem);
orders.stream().filter(adultWithPremium).toList();
```

## Debugging lambdas

- Stack traces show synthetic names such as `lambda$main$0`; short lambdas and named methods make traces readable
- Put a breakpoint **inside** the lambda body; the debugger handles it ([06-exceptions-and-debugging/06_stack-traces-and-debugging.md](../06-exceptions-and-debugging/06_stack-traces-and-debugging.md))
- For stream pipelines, `peek(x -> System.out.println(x))` shows intermediate values temporarily ([04](./04_streams-fundamentals.md))

## Overload ambiguity

Overloaded methods taking different functional interfaces can be ambiguous when the lambda matches several:

```java
void run(Runnable r) { }
void run(Callable<String> c) { }

run(() -> "x");              // resolved: only Callable returns a value
run(() -> doIt());           // AMBIGUOUS if doIt() returns a value (both a Runnable statement and a Callable fit)
run((Runnable) () -> doIt());   // fix with a cast, or rename the methods
```

`ExecutorService.submit(Runnable)` vs `submit(Callable)` is the famous example. Prefer distinct method names in your own APIs ([02-methods/02_overloading-and-varargs.md](../02-methods/02_overloading-and-varargs.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Modifying a captured local variable | `must be final or effectively final` | `AtomicInteger`, or restructure with streams/`reduce` |
| Using a lambda where the target is not a functional interface | `Object is not a functional interface` | Use a proper interface type |
| `var f = x -> x;` | Cannot infer type | Declare `Function<Integer, Integer> f` |
| Redeclaring an enclosing local's name in a lambda parameter | `already defined` | Rename |
| Expecting `this` to refer to the lambda | Refers to the enclosing object | Anonymous class if you need its own `this` |
| Throwing a checked exception from a `Function` lambda | `unreported exception` | Wrap in an unchecked exception |
| Long multi-line lambdas | Unreadable code and traces | Extract a named method |
| Mutating shared state inside stream lambdas | Wrong results, especially in parallel | Pure functions, collectors |
| Ambiguous overloads (`submit(Runnable)` vs `submit(Callable)`) | `reference to ... is ambiguous` | Cast, or distinct names |
| Assuming each evaluation creates a new object | Non-capturing lambdas may be reused | Do not rely on lambda identity |

## Key takeaways

- A lambda is `(params) -> body`, implementing a **functional interface**; the target type provides all the types
- Lambdas capture **effectively final** locals and any fields; `this` means the enclosing instance
- Keep lambdas short and side-effect free; extract longer logic into named methods
- Checked exceptions need wrapping; overloads taking different functional interfaces can be ambiguous
- Lambdas replace most anonymous classes but not those needing state or multiple methods

**Next:** [Functional Interfaces](./01_functional-interfaces.md)