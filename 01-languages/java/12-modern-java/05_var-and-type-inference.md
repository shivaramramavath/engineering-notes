# `var` and Type Inference

`var` (Java 10) lets the compiler **infer the type of a local variable** from its initializer, so you don't have to write it twice:

```java
// Before
Map<String, List<Employee>> byDept = new HashMap<String, List<Employee>>();

// After
var byDept = new HashMap<String, List<Employee>>();
```

The variable is still **statically typed**: `byDept` is a `HashMap<String, List<Employee>>` and the compiler checks every use. `var` removes *redundancy*, not *types*. The bytecode is identical to the explicit version.

**Prerequisites:** [Variables and Data Types](../01-fundamentals/01_variables-and-data-types.md), [Generics](../07-generics/00_generic-classes-and-methods.md).

---

## 1. Where `var` is allowed

```java
var name = "Asha";                               // String
var count = 42;                                  // int
var prices = List.of(9.99, 14.50);               // List<Double>

for (var item : prices) { ... }                  // enhanced for: item is Double
for (var i = 0; i < 10; i++) { ... }             // classic for

try (var reader = Files.newBufferedReader(path)) { ... }   // try-with-resources

BiFunction<Integer, Integer, Integer> add = (var a, var b) -> a + b;   // lambda params (Java 11)
```

### Where it is not allowed

```java
var x;                          // no initializer → nothing to infer from
var y = null;                   // null has no type
var arr = {1, 2, 3};            // array initializer needs a target type; use `new int[]{1,2,3}`
var f = () -> 42;               // lambda needs a target type
var g = String::length;         // method reference too

class Foo {
    var field = 1;              // fields not allowed
    var method() { ... }        // return types not allowed
    void m(var p) { ... }       // method parameters not allowed
}
```

`var` is **only for local variables** (including loop variables and try-with-resources), and the initializer must be present and typed. Declare one variable per statement: `var a = 1, b = 2;` is illegal.

`var` is a *reserved type name*, not a keyword, so `int var = 5;` still compiles, but you cannot name a class `var`.

---

## 2. What type gets inferred

The type is the **static type of the initializer expression**, not an interface you might have wanted:

| Declaration | Inferred type |
|---|---|
| `var s = "hi"` | `String` |
| `var n = 42` | `int` |
| `var l = 42L` | `long` |
| `var d = 3.0` | `double` |
| `var list = new ArrayList<String>()` | `ArrayList<String>` (**not** `List<String>`) |
| `var list = new ArrayList<>()` | `ArrayList<Object>` (**diamond has nothing to infer from**) |
| `var names = List.of("a", "b")` | `List<String>` |
| `var e = map.entrySet().iterator().next()` | `Map.Entry<K, V>` |

Consequences:

```java
var list = new ArrayList<String>();
list = new LinkedList<String>();      // error: it's an ArrayList<String>, not a List<String>

List<String> list2 = new ArrayList<>();   // the declaration you want if you need the interface type
list2 = new LinkedList<>();

var items = new ArrayList<>();        // ArrayList<Object>: probably not what you meant
items.add("x"); items.add(1);         // compiles: everything is an Object
```

Two traps follow:

- **Diamond + `var`**: write the type argument on the right (`new ArrayList<String>()`) or use an explicit left-hand type.
- **Numeric literals**: `var x = 1` is an `int`. If you need a `long`, `byte`, or `double`, write `1L`, `(byte) 1`, `1.0`, or state the type. Don't let the literal silently decide.

### Non-denotable types

`var` can hold types you can't write by name, such as an anonymous class's members:

```java
var point = new Object() { int x = 1; int y = 2; };
System.out.println(point.x + point.y);        // works: the type includes x and y
```

It's occasionally useful for small local helpers, but rarely needed.

---

## 3. Style: when to use it, when not to

`var` removes noise when the type is *already obvious from the right-hand side*. It hurts when it **hides** the type the reader needs.

```java
// Good: the type is right there
var users = new ArrayList<User>();
var conn = DataSource.getConnection();           // reasonably clear in context
var reader = Files.newBufferedReader(path);
for (var entry : map.entrySet()) { ... }         // long generic Map.Entry<...> avoided

// Bad: the reader has to go find out
var result = service.process(request);           // what is it?
var data = load();                               // ??
var total = a * b;                               // int? long? double?
```

Guidelines (adapted from the OpenJDK style guidance for local variable type inference):

1. **Choose informative variable names.** With `var`, the name carries the meaning: `var customerNames`, not `var list`.
2. **Use it when the initializer makes the type clear:** constructors, factory methods (`List.of`), literals, casts.
3. **Keep scope small.** The longer a variable lives, the more the reader needs its type up front.
4. **Don't use it to avoid thinking about types** in public APIs (it can't be used there anyway).
5. **Be explicit where inference could surprise you:** primitives and numeric literals, diamond expressions, `?:` mixing types.
6. **Prefer the interface on the left** when the variable is meant to be used polymorphically (`List<String> x = new ArrayList<>()`).
7. Teams should agree on a convention. It's a readability choice, not a performance one.

IDEs can show the inferred type inline, which makes `var` easier to read while editing, but code reviews and diffs have no such hints. Write for a reader without an IDE.

---

## 4. `var` in lambda parameters (Java 11)

```java
(var a, var b) -> a + b
```

Why would you write that instead of `(a, b) -> a + b`? To put **annotations or modifiers** on parameters:

```java
(@NonNull var a, final var b) -> a.compareTo(b)
```

Rule: you can't mix styles. Either all parameters use `var`, or none do. `(var a, b)` and `(var a, int b)` are errors. Otherwise there is no difference in behavior ([Lambda Expressions](../09-functional-java/00_lambda-expressions.md)).

---

## 5. Type inference elsewhere in Java

`var` is just one of several inference features, and they're easy to mix up:

| Feature | Since | Example |
|---|---|---|
| Generic method inference | 5 | `Collections.emptyList()` infers `T` from the context |
| Diamond `<>` | 7 | `Map<String, Integer> m = new HashMap<>();` |
| Lambda parameter types | 8 | `(a, b) -> a + b` types come from the target functional interface |
| Local variable type inference (`var`) | 10 | `var m = new HashMap<String, Integer>();` |
| `var` in lambda parameters | 11 | `(var a, var b) -> ...` |
| Unnamed variables `_` | 22 | `catch (Exception _)`: see [Pattern Matching](04_pattern-matching.md) |

The distinction: diamond and lambda inference work **from the target type on the left** (or from arguments), while `var` works **from the initializer on the right**, and that is why `var x = new ArrayList<>()` gives `ArrayList<Object>`.

---

## Common mistakes and misconceptions

| Mistake / belief | Reality |
|---|---|
| "`var` makes Java dynamically typed, like JavaScript" | The type is fixed at compile time. `var x = 1; x = "a";` doesn't compile |
| "`var` has a runtime cost" | None: it's compile-time only |
| "`var list = new ArrayList<String>()` is a `List`" | It's an `ArrayList<String>` |
| `var list = new ArrayList<>()` | `ArrayList<Object>`: specify the type argument |
| `var x = 10; x = 3.5;` | `x` is `int`: error. Declare `double x` if you need that |
| Using `var` for fields / parameters / returns | Not allowed |
| Replacing every explicit type with `var` | Use it where it improves readability |
| Uninformative names (`var data`, `var result`) | Name by meaning, or write the type |

### Debugging

- "cannot infer type for local variable x" → missing initializer, `null`, a lambda, a method reference, or an array initializer. Give the variable an explicit type.
- Unexpected `ArrayList<Object>` / `Object` types → a diamond without a type argument, or a `var` initialized from an expression whose type is wider than you thought.
- "incompatible types" when reassigning → the inferred type is narrower than you expected (`ArrayList`, `int`).
- Not sure what was inferred → hover over the variable in your IDE, or compile and look at the error message of a deliberately wrong assignment.

---

## Quick Summary

- `var` infers a **local variable's** type from its initializer. It's still statically typed, with identical bytecode.
- Allowed for locals, `for` variables, try-with-resources, and lambda parameters (all-or-none, Java 11). **Not** for fields, parameters, return types, or without an initializer / with `null`.
- The inferred type is the initializer's exact type (`ArrayList<String>`, `int`), and `new ArrayList<>()` becomes `ArrayList<Object>`.
- Use it when the right-hand side makes the type obvious and the name is meaningful. Skip it when it hides what the value is.
- Make numeric types explicit (`1L`, `double x`) when they matter.

**Next module:** [Advanced Language Features](../13-advanced-language-features/00_annotations.md)