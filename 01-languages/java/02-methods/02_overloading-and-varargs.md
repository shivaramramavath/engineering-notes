# Overloading and Varargs

**Overloading** means several methods in the same class share a name but have **different parameter lists**. **Varargs** (`T...`) lets one method accept any number of arguments. The compiler picks the overload at **compile time** from the static types of the arguments, and the rules produce many interview puzzles.

```java
static int    max(int a, int b)       { return a > b ? a : b; }
static double max(double a, double b) { return a > b ? a : b; }
static int    max(int a, int b, int c){ return max(max(a, b), c); }
```

## What counts as a different overload

| Different | Example |
|-----------|---------|
| Number of parameters | `f(int)` and `f(int, int)` |
| Types of parameters | `f(int)` and `f(String)` |
| Order of different types | `f(int, String)` and `f(String, int)` |

| Does **not** make an overload | Why |
|-------------------------------|-----|
| Return type alone | `int f(int)` and `long f(int)` is a compile error: the call site cannot tell them apart |
| Parameter names | Not part of the signature |
| `throws` clause, modifiers | Not part of the signature |

```java
int  parse(String s) { ... }
long parse(String s) { ... }       // ERROR: method parse(String) is already defined
```

## Overloading vs overriding

| | Overloading | Overriding |
|---|-------------|-----------|
| Where | Same class (or inherited) | Subclass redefines a parent method |
| Signature | Different parameters | Same signature |
| Resolved | **Compile time**, by static argument types | **Runtime**, by the actual object type |
| Also called | Static / compile-time polymorphism | Dynamic polymorphism |

```java
static void print(Object o) { System.out.println("Object"); }
static void print(String s) { System.out.println("String"); }

Object o = "hello";
print(o);            // Object: chosen by the static type Object, not the runtime String
print("hello");      // String
```

Overriding is covered in [04-oop/06_inheritance.md](../04-oop/06_inheritance.md) and [07_polymorphism.md](../04-oop/07_polymorphism.md).

## How the compiler chooses: three phases

The compiler tries each phase in order and stops at the first one that finds an applicable method:

| Phase | Allowed conversions |
|-------|---------------------|
| 1 | Exact match and **widening** (`int → long → double`, subtype); **no** boxing, **no** varargs |
| 2 | Phase 1 plus **boxing/unboxing** |
| 3 | Phase 2 plus **varargs** |

If several methods apply in the same phase, the **most specific** wins. If none is most specific, it is a compile error (ambiguous).

### Examples

```java
static void m(long x)    { System.out.println("long"); }
static void m(Integer x) { System.out.println("Integer"); }
static void m(Object x)  { System.out.println("Object"); }
static void m(int... x)  { System.out.println("varargs"); }

m(5);    // long: phase 1 (widening) beats boxing
```

Remove `m(long)`:

```java
m(5);    // Integer: phase 2 (box to Integer); Integer is more specific than Object
```

Remove `m(Integer)` too:

```java
m(5);    // Object: phase 2 (box, then widen to Object) beats varargs
```

Remove `m(Object)`:

```java
m(5);    // varargs: only phase 3 applies
```

### Primitives and widening

```java
static void p(int x)    { System.out.println("int"); }
static void p(long x)   { System.out.println("long"); }
static void p(double x) { System.out.println("double"); }

byte b = 1;  short s = 2;  char c = 'a';
p(b);        // int    (byte widens to int: the closest match)
p(s);        // int
p(c);        // int    (char → int)
p(3L);       // long
p(3.0f);     // double (float → double)
```

The closest widening wins: `int` is more specific than `long`, which is more specific than `double`. If `p(char)` also existed, `p('a')` would call it.

### `null` arguments

```java
static void f(Object o) { System.out.println("Object"); }
static void f(String s) { System.out.println("String"); }
f(null);                     // String: String is more specific than Object

static void g(String s)  { }
static void g(Integer i) { }
g(null);                     // ERROR: reference to g is ambiguous
g((String) null);            // OK: cast picks one
```

### Overloading with `remove`

```java
List<Integer> list = new ArrayList<>(List.of(10, 20, 30));
list.remove(1);                     // remove(int index) → removes 20
list.remove(Integer.valueOf(10));   // remove(Object)    → removes the value 10
```

See [wrappers](../01-fundamentals/04_wrapper-classes-and-autoboxing.md).

## Varargs

```java
static int sum(int... nums) {
    int total = 0;
    for (int n : nums) total += n;
    return total;
}

sum();                 // 0   (zero arguments allowed)
sum(5);                // 5
sum(1, 2, 3);          // 6
sum(new int[]{4, 5});  // 9   (an array can be passed directly)
```

Inside the method, `nums` is just an **array** (`int[]`); the compiler creates it at the call site.

| Rule | Detail |
|------|--------|
| Syntax | `Type... name` |
| Position | Must be the **last** parameter |
| Count | At most **one** varargs parameter per method |
| Zero arguments | Allowed: the array is empty (not `null`) |
| Passing `null` | `sum((int[]) null)` gives a `null` array; avoid |

```java
static String join(String sep, String... parts) {     // OK
    return String.join(sep, parts);
}
static void bad(int... a, String s) { }               // ERROR: varargs must be last
```

Familiar varargs methods: `String.format(fmt, args...)`, `System.out.printf`, `Arrays.asList(T...)`, `List.of(E...)`, `Path.of(first, more...)`.

### Requiring at least one argument

```java
static int max(int first, int... rest) {
    int m = first;
    for (int n : rest) if (n > m) m = n;
    return m;
}
max();         // compile error: good
max(7);        // 7
max(3, 9, 4);  // 9
```

### Varargs ambiguity

```java
static void f(int... a)       { System.out.println("A"); }
static void f(int a, int... b){ System.out.println("B"); }
f(1);          // ERROR: ambiguous
f();           // A
```

A method with fixed arity is always preferred to varargs (phases 1 and 2 before phase 3):

```java
static void h(int a, int b) { System.out.println("fixed"); }
static void h(int... a)     { System.out.println("varargs"); }
h(1, 2);       // fixed
h(1, 2, 3);    // varargs
```

### Varargs and generics

```java
@SafeVarargs
static <T> List<T> listOf(T... items) {          // generic array creation warning without the annotation
    return new ArrayList<>(Arrays.asList(items));
}
```

Generic varargs create an array of a non-reifiable type (**heap pollution** risk). Use `@SafeVarargs` only when the method does not store or expose the array ([07-generics/03_type-erasure.md](../07-generics/03_type-erasure.md)).

### Cost

Every call allocates a new array. For hot paths with a few arguments, provide fixed overloads (`of(a)`, `of(a, b)`, ...). `List.of` does exactly this.

## Design guidelines

| Guideline | Why |
|-----------|-----|
| Overload only for the **same operation** on different inputs | `print(int)`, `print(String)` |
| Use different names when the meaning differs | `removeAt(int)` and `remove(Object)` would avoid the list trap |
| Avoid overloads whose parameter types are related (`Object`/`String`, `int`/`Integer`, `long`/`Integer`) | Hard to predict which runs |
| Avoid two overloads with the same number of **functional-interface** parameters | Lambda calls become ambiguous (`submit(Runnable)` vs `submit(Callable)`) |
| Prefer a varargs method over a long list of arity overloads, unless performance matters | Less code |
| Use overloading to simulate default parameters | See below |
| Keep behavior of all overloads consistent | Surprises hurt |

### Simulating default arguments

```java
static void connect(String host, int port, int timeoutMs) { ... }
static void connect(String host, int port) { connect(host, port, 5000); }
static void connect(String host)           { connect(host, 80); }
```

All overloads funnel into one implementation. Constructors do the same with `this(...)` ([04-oop/01_constructors.md](../04-oop/01_constructors.md)).

## Predict the output

```java
static void show(double d)  { System.out.println("double"); }
static void show(Integer i) { System.out.println("Integer"); }
static void show(Object o)  { System.out.println("Object"); }

show(1);          // ?
show(1L);         // ?
show('a');        // ?
show("x");        // ?
```

Answers: `double` (widening beats boxing), `double` (long widens to double), `double` (char widens to double), `Object`.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Overloading by return type only | `already defined` | Change the parameters or the name |
| Expecting runtime-type dispatch for overloads | The `Object` version runs | Overloads use **static** types; use overriding or `instanceof` |
| Passing `null` to overloaded methods | `ambiguous` | Cast the argument |
| Mixing `int` and `Integer`/`Object` overloads | Surprising choice | Rename the methods |
| Varargs not in last position | Compile error | Move it to the end |
| Two ambiguous varargs overloads | `reference is ambiguous` | Make one fixed-arity or rename |
| `varargs` called with a single `null` | `null` array or a warning | Pass an empty array or cast |
| Modifying the varargs array | May affect the array you passed in | Copy first |
| Varargs in hot loops | Extra allocations | Add fixed-arity overloads |
| Generic varargs without `@SafeVarargs` | Unchecked warnings, possible heap pollution | Avoid, or annotate when safe |

## Key takeaways

- Overloads differ in parameter lists, never only in return type
- The compiler chooses at compile time: exact/widening, then boxing, then varargs; the most specific match wins
- Overloading uses the static type; overriding uses the runtime type
- Varargs is an array in disguise; it must be the last parameter, and zero arguments are allowed
- Avoid clever overload sets; use different names when the meaning differs

**Next:** [Recursion](./03_recursion.md)
