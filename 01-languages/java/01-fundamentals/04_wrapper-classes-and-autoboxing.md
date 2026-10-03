# Wrapper Classes and Autoboxing

Every primitive has an object counterpart, a **wrapper class**. Wrappers exist because generics, collections and `null` only work with objects. **Autoboxing** converts between the two automatically, and that convenience hides several classic bugs.

```
primitive ──boxing──► wrapper object ──unboxing──► primitive
   int                    Integer                     int
```

## The wrappers

| Primitive | Wrapper | Parent |
|-----------|---------|--------|
| `byte` | `Byte` | `Number` |
| `short` | `Short` | `Number` |
| `int` | `Integer` | `Number` |
| `long` | `Long` | `Number` |
| `float` | `Float` | `Number` |
| `double` | `Double` | `Number` |
| `char` | `Character` | `Object` |
| `boolean` | `Boolean` | `Object` |

All wrappers are **immutable** and live in `java.lang`.

## Why they exist

```java
List<int> bad = new ArrayList<>();            // ERROR: generics need objects
List<Integer> ok = new ArrayList<>();         // OK

Integer maybe = null;                         // "no value": primitives cannot be null
Map<String, Integer> counts = new HashMap<>();
```

Wrappers also provide useful static methods and constants (`Integer.MAX_VALUE`, `Integer.parseInt`, `Character.isDigit`).

## Autoboxing and unboxing

```java
Integer a = 5;           // boxing:   Integer.valueOf(5)
int b = a;               // unboxing: a.intValue()

List<Integer> list = new ArrayList<>();
list.add(3);             // boxing
int first = list.get(0); // unboxing
Integer sum = a + 10;    // unbox, add, box again
```

Boxing creates objects, so it costs memory and time ([performance](#performance)).

## Pitfall 1: `==` on wrappers and the Integer cache

`Integer.valueOf` (used by autoboxing) **caches** values from -128 to 127.

```java
Integer a = 127, b = 127;
System.out.println(a == b);        // true  (same cached object)

Integer c = 128, d = 128;
System.out.println(c == d);        // false (two different objects)
System.out.println(c.equals(d));   // true
```

The rule: compare wrappers with **`.equals()`** (or unbox with `.intValue()`). `==` between two wrappers compares references. `==` between a wrapper and a primitive unboxes, so that works.

```java
Integer c = 128;
int p = 128;
c == p                  // true  (c is unboxed)
```

Caches: `Byte`, `Short`, `Integer`, `Long` (-128 to 127), `Character` (0 to 127), `Boolean` (both values). `Float` and `Double` have no cache.

## Pitfall 2: `NullPointerException` on unboxing

```java
Integer count = null;
int n = count;                          // NullPointerException

Map<String, Integer> map = new HashMap<>();
int hits = map.get("missing");          // NPE: get returns null
int safe = map.getOrDefault("missing", 0);   // 0
```

Also watch for conditional expressions:

```java
Integer boxed = null;
int x = flag ? 1 : boxed;               // unboxes boxed when flag is false: NPE
```

## Pitfall 3: `equals` between different wrappers

```java
Long l = 1L;
l.equals(1);                  // false: argument is an Integer
l.equals(1L);                 // true
new Integer(5)                // deprecated for removal: use Integer.valueOf(5)
```

## Pitfall 4: overload ambiguity

```java
List<Integer> list = new ArrayList<>(List.of(10, 20, 30));
list.remove(1);                       // removes index 1       → [10, 30]
list.remove(Integer.valueOf(10));     // removes the value 10  → [30]
```

`remove(int index)` is preferred over `remove(Object o)`, so the first call is **not** autoboxed.

## Performance

```java
Long sum = 0L;
for (long i = 0; i < 1_000_000; i++) {
    sum += i;                // unbox, add, box: creates about a million Long objects
}

long fast = 0L;              // primitive: no allocation
for (long i = 0; i < 1_000_000; i++) fast += i;
```

Use primitives in tight loops and numeric code. Use wrappers only where objects are required: collections, generics, optional values. For large numeric collections, consider primitive streams (`IntStream`) or arrays ([09-functional-java/04_streams-fundamentals.md](../09-functional-java/04_streams-fundamentals.md)).

## Useful static methods

```java
// Parsing and conversion
Integer.parseInt("42");                // int
Integer.valueOf("42");                 // Integer
Integer.toString(42);                  // "42"
Integer.toBinaryString(10);            // "1010"
Integer.toHexString(255);              // "ff"
Integer.parseInt("ff", 16);            // 255

// Comparison and math
Integer.compare(3, 5);                 // negative number
Integer.max(3, 5);   Integer.sum(3, 5);
Integer.signum(-9);                    // -1
Integer.bitCount(7);                   // 3

// Constants
Integer.MAX_VALUE;  Integer.MIN_VALUE;
Double.MAX_VALUE;   Double.MIN_VALUE;  // smallest POSITIVE double, not the most negative!

// Doubles
Double.isNaN(x);  Double.isInfinite(x);  Double.compare(a, b);

// Characters
Character.isDigit('7');  Character.isLetter('x');  Character.isUpperCase('A');
Character.toUpperCase('a');  Character.getNumericValue('7');

// Booleans
Boolean.parseBoolean("TRUE");          // true (case-insensitive)
```

## Primitive or wrapper?

| Use a primitive when | Use a wrapper when |
|----------------------|--------------------|
| Local numeric work, loops, arithmetic | Storing in collections (`List<Integer>`) |
| A value is always present | A value may be missing (`null`) |
| Performance or memory matters | Using generics, `Optional<Integer>` |
| A field is required with a sensible default | A field needs "unset" vs "zero" |

A common rule: **return and accept primitives, store wrappers only when you need `null` or generics.**

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `a == b` on `Integer` values above 127 | Unexpected `false` | `.equals()` |
| Unboxing a `null` | `NullPointerException` | Null checks, `getOrDefault`, `Optional` |
| `list.remove(i)` intending a value | Removes by index | `remove(Integer.valueOf(x))` |
| `Long.equals(int literal)` | Always `false` | Use `1L` |
| Boxed accumulators in loops | Slow, lots of garbage | Use primitives |
| `new Integer(5)` | Deprecated warning | `Integer.valueOf(5)` or autoboxing |
| `Double.MIN_VALUE` as "most negative" | It is tiny and positive | `-Double.MAX_VALUE` |
| Comparing `Double` objects with `==` | Wrong results | `Double.compare` or `.equals` |

## Key takeaways

- Wrappers let primitives live in generics and collections and allow `null`
- Autoboxing is convenient but creates objects and can throw `NullPointerException`
- Compare wrappers with `.equals()`; `==` only works by accident for -128..127
- Prefer primitives for computation; use wrappers only where objects are needed

**Next:** [Conditionals](./05_conditionals.md)
