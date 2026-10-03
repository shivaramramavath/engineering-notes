# Methods

A **method** is a named block of code that can be called with inputs (**parameters**) and can produce an output (a **return value**). Methods remove duplication, give code a name, and let you test and change one piece at a time.

```java
public static int add(int a, int b) {      // declaration
    return a + b;
}

int result = add(3, 4);                    // call → 7
```

## Anatomy

```
  modifiers   return type   name    parameters
     │             │          │         │
public static     int       add   (int a, int b)  { 
    return a + b;                                    ◄── body
}
```

| Part | Meaning |
|------|---------|
| Modifiers | Access (`public`, `private`, ...), `static`, `final`, ... |
| Return type | The type of the value returned, or `void` for none |
| Name | `camelCase`, usually a verb: `calculateTotal`, `isEmpty` |
| Parameters | Typed, comma-separated inputs; may be empty `()` |
| Body | The statements in `{ }` |

The **method signature** is the name plus the parameter types: `add(int, int)`. The return type and parameter names are not part of it.

## Calling methods

```java
public class Calculator {
    static int square(int n) {
        return n * n;
    }

    public static void main(String[] args) {
        int a = square(5);                 // 25
        int b = square(square(2));         // 16: calls can be nested
        System.out.println(square(3) + 1); // 10: used as an expression
        square(7);                         // allowed: result ignored (rarely useful)
    }
}
```

- **Arguments** are the values you pass; **parameters** are the names in the declaration
- Arguments are matched to parameters **by position and type**
- Argument types must be assignable to the parameter types (widening is fine, narrowing needs a cast: [type casting](../01-fundamentals/03_type-casting-and-conversion.md))

## Return values

```java
static boolean isEven(int n) {
    return n % 2 == 0;
}

static void greet(String name) {           // void: returns nothing
    if (name == null) return;              // bare return ends the method early
    System.out.println("Hello, " + name);
}
```

| Rule | Example |
|------|---------|
| A non-`void` method must return on **every** path | Else: `missing return statement` |
| A `void` method may use `return;` to exit early | |
| Code after `return` is unreachable | Compile error |
| Only **one** value can be returned | See "Returning several values" |

```java
static String grade(int score) {
    if (score >= 90) return "A";
    if (score >= 80) return "B";
    return "C";                            // required: covers all remaining cases
}
```

### Returning several values

Java has no tuples. Options:

| Option | When |
|--------|------|
| An array | Same type, fixed small count (`int[]{min, max}`) |
| A `record` | Related values; clear names ([12-modern-java/01_records.md](../12-modern-java/01_records.md)) |
| A small class | Same, on older Java versions |
| An object that gets filled in | Rarely; prefer returning a value |

```java
record MinMax(int min, int max) {}

static MinMax minMax(int[] a) {
    int min = a[0], max = a[0];
    for (int n : a) {
        if (n < min) min = n;
        if (n > max) max = n;
    }
    return new MinMax(min, max);
}
```

## Parameters

```java
static double average(int[] values) { ... }       // an array parameter
static void log(String level, String message) { }  // several parameters
static void process(final int id) { }              // final: cannot be reassigned inside
```

- Java has **no default parameter values** and **no named arguments**; use [overloading](./02_overloading-and-varargs.md) or an options object
- Parameters are **local variables** initialized from the arguments
- Java is pass-by-value: see [01_pass-by-value.md](./01_pass-by-value.md)
- Too many parameters (more than 3-4) is a smell; group related ones into an object (or a builder, see [24-design-patterns/01-creational/03_builder.md](../24-design-patterns/01-creational/03_builder.md))

## Scope and local variables

Variables declared in a method exist only during that call and are invisible to other methods.

```java
static int count() {
    int n = 0;                 // local: created on each call, gone on return
    n++;
    return n;
}
count();   // 1
count();   // 1 again: nothing is remembered between calls
```

To keep state between calls, use a **field** ([04-oop/00_classes-and-objects.md](../04-oop/00_classes-and-objects.md)).

## `static` and instance methods

```java
public class Counter {
    private int value;                        // instance state

    void increment() { value++; }             // instance method: needs an object
    static int twice(int x) { return 2 * x; } // static method: belongs to the class

    public static void main(String[] args) {
        System.out.println(twice(4));         // OK
        // increment();                       // ERROR: non-static method cannot be referenced from a static context
        Counter c = new Counter();
        c.increment();                        // OK
    }
}
```

`main` is static, so it can call only static methods directly. Calling an instance method needs an object. More in [04-oop/02_static.md](../04-oop/02_static.md).

## What happens at a call: the call stack

Each call gets its own **stack frame** with the parameters and local variables. The frame is removed when the method returns.

```java
static int square(int n) { return n * n; }
static int sumOfSquares(int a, int b) { return square(a) + square(b); }
// main calls sumOfSquares(3, 4)
```

```
          ┌───────────────────┐
          │ square(n=4)       │  ◄── top: running now
          ├───────────────────┤
          │ sumOfSquares(3,4) │
          ├───────────────────┤
          │ main(args)        │
          └───────────────────┘
```

When `square` returns, its frame is popped and execution resumes in `sumOfSquares`. Too many nested calls cause `StackOverflowError` ([recursion](./03_recursion.md), [15-jvm-internals/02_jvm-memory-areas.md](../15-jvm-internals/02_jvm-memory-areas.md)).

## Designing good methods

| Guideline | Why |
|-----------|-----|
| **One job** per method | Easy to name, test and reuse |
| Short (about 5-20 lines) | Fits in your head |
| Name with a verb; booleans as questions (`isValid`, `hasItems`) | Reads like prose |
| Same level of abstraction inside | Do not mix business rules with string formatting |
| Avoid output (`println`) inside logic methods; return values | Testable and reusable |
| Prefer fewer parameters | Callers get simpler |
| Avoid hidden side effects | Methods that change unrelated state surprise people |
| Validate inputs at the boundary | Fail early with a clear message |

```java
// Hard to test and reuse
static void report(int[] scores) {
    int sum = 0;
    for (int s : scores) sum += s;
    System.out.println("Average: " + (double) sum / scores.length);
}

// Better: logic returns a value; printing is separate
static double average(int[] scores) {
    int sum = 0;
    for (int s : scores) sum += s;
    return (double) sum / scores.length;
}
static void printReport(int[] scores) {
    System.out.println("Average: " + average(scores));
}
```

### Pure functions vs side effects

A method is **pure** when its result depends only on its arguments and it changes nothing else. Pure methods are the easiest to test and reason about ([09-functional-java/08_functional-patterns.md](../09-functional-java/08_functional-patterns.md)).

## Invalid input

```java
static double divide(int a, int b) {
    if (b == 0) {
        throw new IllegalArgumentException("b must not be zero");
    }
    return (double) a / b;
}
```

Declare and throw exceptions rather than returning magic values like `-1`: see [06-exceptions-and-debugging/02_throw-and-throws.md](../06-exceptions-and-debugging/02_throw-and-throws.md).

## Javadoc

```java
/**
 * Returns the larger of two numbers.
 *
 * @param a first number
 * @param b second number
 * @return the maximum of a and b
 */
static int max(int a, int b) {
    return a > b ? a : b;
}
```

Document **public** methods: what they do, units, accepted ranges, and what they throw.

## Other kinds of methods (preview)

| Kind | Where covered |
|------|---------------|
| Constructors (no return type, name = class) | [04-oop/01_constructors.md](../04-oop/01_constructors.md) |
| Overridden methods | [04-oop/06_inheritance.md](../04-oop/06_inheritance.md) |
| Abstract and interface methods | [04-oop/08_abstract-classes.md](../04-oop/08_abstract-classes.md), [09_interfaces.md](../04-oop/09_interfaces.md) |
| Lambdas and method references | [09-functional-java/00_lambda-expressions.md](../09-functional-java/00_lambda-expressions.md) |
| Generic methods (`<T> T first(List<T> l)`) | [07-generics/00_generic-classes-and-methods.md](../07-generics/00_generic-classes-and-methods.md) |

Java does **not** allow a method inside a method. Use a lambda, a local class or a private helper method instead.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Missing `return` on some path | `missing return statement` | Return on every path (or throw) |
| Calling an instance method from `static main` | `non-static method ... cannot be referenced` | Create an object, or make it `static` |
| Ignoring the return value | Nothing seems to happen (`s.toUpperCase();`) | Assign it: `s = s.toUpperCase();` |
| Expecting parameters to change the caller's variable | Value unchanged | See [pass-by-value](./01_pass-by-value.md) |
| Printing instead of returning | Cannot reuse or test | Return; print at the edge |
| Doing several things in one method | Hard to name or test | Split it |
| Long parameter lists | Easy to swap arguments by mistake | Group into an object |
| Wrong argument order with same-typed parameters | Silent bug | Distinct types, or a builder |
| `void` method with a name suggesting a result (`getTotal`) | Confusing | Return the value |
| Mutating inputs unexpectedly | Surprising callers | Copy or document it |
| Using a local variable after the method returned | Not possible | Return it or store it in a field |

## Key takeaways

- A method has a name, parameters, a return type and a body; its signature is name + parameter types
- Every call gets a fresh stack frame; locals disappear on return
- Return values instead of printing; keep methods short and single-purpose
- Java has no default parameters; `main` can call only static methods directly
- Document public methods and fail clearly on invalid input

**Next:** [Pass-by-Value](./01_pass-by-value.md)
