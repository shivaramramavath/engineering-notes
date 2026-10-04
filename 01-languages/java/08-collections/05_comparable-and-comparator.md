# Comparable and Comparator

Sorting needs a rule for "which comes first?". Java gives you two ways to define it:

| | `Comparable<T>` | `Comparator<T>` |
|---|-----------------|-----------------|
| Where the rule lives | **Inside** the class (its *natural ordering*) | **Outside**, as a separate object |
| Method | `int compareTo(T other)` | `int compare(T a, T b)` |
| How many orderings | One per class | As many as you like |
| Use when | There is one obvious order (numbers, strings, dates, versions) | Different orders in different places, or you cannot edit the class |

Both return a **negative** number (first is smaller), **zero** (equal in order), or a **positive** number (first is larger).

```java
List<String> names = new ArrayList<>(List.of("Linus", "Ada", "Grace"));
Collections.sort(names);                                    // natural order (String is Comparable)
names.sort(Comparator.comparing(String::length));           // custom order via a Comparator
```

## `Comparable`: natural ordering

```java
public class Version implements Comparable<Version> {
    private final int major, minor;
    public Version(int major, int minor) { this.major = major; this.minor = minor; }

    @Override
    public int compareTo(Version other) {
        int c = Integer.compare(major, other.major);
        return c != 0 ? c : Integer.compare(minor, other.minor);
    }
}

List<Version> versions = new ArrayList<>(List.of(new Version(2, 1), new Version(1, 9)));
Collections.sort(versions);                  // uses compareTo
new TreeSet<>(versions);                     // uses compareTo
```

Built-in `Comparable` types: `String`, all wrappers (`Integer`, `Double`, ...), `BigDecimal`, `BigInteger`, `LocalDate`/`LocalDateTime`, `Duration`, enums (declaration order), `Boolean`, `Character`.

### The `compareTo` contract

| Rule | Meaning |
|------|---------|
| **Antisymmetric** | `sgn(a.compareTo(b)) == -sgn(b.compareTo(a))` |
| **Transitive** | `a > b` and `b > c` ⇒ `a > c` |
| **Substitutive for zero** | `a.compareTo(b) == 0` ⇒ `a` and `b` compare the same way against any `c` |
| **Consistent with `equals` (strongly recommended)** | `a.compareTo(b) == 0` ⇔ `a.equals(b)` |
| `null` | `a.compareTo(null)` should throw `NullPointerException` |

If `compareTo` and `equals` disagree, sorted collections and hash collections treat "duplicates" differently:

```java
BigDecimal a = new BigDecimal("2.0"), b = new BigDecimal("2.00");
a.equals(b);                   // false (scale differs)
a.compareTo(b);                // 0
new HashSet<>(List.of(a, b)).size();   // 2
new TreeSet<>(List.of(a, b)).size();   // 1 (TreeSet uses compareTo)
```

See [04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md).

## Never subtract to compare

```java
public int compareTo(Person o) { return this.age - o.age; }       // BUG: overflows for large/negative values
public int compareTo(Person o) { return Integer.compare(age, o.age); }   // correct

return (int) (a.price - b.price);                                   // BUG: 0.5 difference truncates to 0
return Double.compare(a.price, b.price);                            // correct (handles NaN and -0.0 consistently)
```

Always use `Integer.compare`, `Long.compare`, `Double.compare`, `String.compareTo`, `Boolean.compare`, `Character.compare`.

## `Comparator`: custom ordering

A `Comparator` is a function of two arguments, so you can write it as a lambda or build it from helper methods.

```java
record Person(String name, int age) { }

Comparator<Person> byAge = (a, b) -> Integer.compare(a.age(), b.age());      // lambda
Comparator<Person> byAge2 = Comparator.comparingInt(Person::age);            // key extractor (preferred)

people.sort(byAge2);
Collections.sort(people, byAge2);
Arrays.sort(array, byAge2);               // object arrays only
people.stream().sorted(byAge2).toList();
new TreeSet<>(byAge2);   new TreeMap<>(byAge2);   new PriorityQueue<>(byAge2);
```

### Building comparators

| Factory / method | Purpose |
|------------------|---------|
| `Comparator.comparing(keyExtractor)` | Compare by a key (must be `Comparable`) |
| `comparing(keyExtractor, keyComparator)` | Compare by a key using another comparator |
| `comparingInt` / `comparingLong` / `comparingDouble` | Avoid boxing for primitive keys |
| `.thenComparing(...)` | Tie-breaker(s); also `thenComparingInt` etc. |
| `.reversed()` | Reverse this comparator |
| `Comparator.naturalOrder()` / `reverseOrder()` | Natural order of `Comparable` types, and its reverse |
| `Comparator.nullsFirst(c)` / `nullsLast(c)` | Handle `null` elements or keys |
| `String.CASE_INSENSITIVE_ORDER` | Case-insensitive strings |

```java
// By age descending, then name ascending
Comparator<Person> order = Comparator.comparingInt(Person::age).reversed()
                                     .thenComparing(Person::name);

// Case-insensitive name, nulls last
Comparator<Person> byName = Comparator.comparing(Person::name,
        Comparator.nullsLast(String.CASE_INSENSITIVE_ORDER));

// Descending natural order
list.sort(Comparator.reverseOrder());
list.sort(Collections.reverseOrder());          // equivalent
```

### Type-inference gotcha with `reversed()`

```java
people.sort(Comparator.comparing(p -> p.age()).reversed());              // ERROR: p inferred as Object
people.sort(Comparator.comparing(Person::age).reversed());               // OK: method reference gives the type
people.sort(Comparator.comparing((Person p) -> p.age()).reversed());     // OK: explicit lambda parameter type
people.sort(Comparator.comparingInt(Person::age).reversed());            // OK
people.sort(Comparator.comparingInt(Person::age).reversed().thenComparing(Person::name));
```

When a lambda is the first call in a chain, the compiler cannot infer `T`; use method references or declare the parameter type.

### Writing a comparator by hand

```java
Comparator<Person> manual = (a, b) -> {
    int c = Integer.compare(b.age(), a.age());          // descending age
    return c != 0 ? c : a.name().compareTo(b.name());   // then name
};
```

The builder style is shorter and avoids mistakes.

## Sorting APIs

```java
list.sort(comparator);                       // List: in place; null comparator = natural order
Collections.sort(list);                      // natural order
Collections.sort(list, comparator);
Arrays.sort(intArray);                       // primitives: natural order only
Arrays.sort(objArray, comparator);
Arrays.sort(objArray, from, to, comparator); // a range
stream.sorted();  stream.sorted(comparator); // new sorted stream
Collections.min(coll, comparator);  Collections.max(coll, comparator);
```

- Sorting objects is **stable**: elements that compare equal keep their original relative order (it uses TimSort/merge sort). This makes multi-pass sorts and `thenComparing` chains predictable
- Primitive arrays use dual-pivot quicksort (not stable, but stability does not matter for primitives)
- Sorting is O(n log n)

## Where ordering is used

| API | Uses |
|-----|------|
| `TreeSet`, `TreeMap` | Order **and** equality/duplicate detection ([09](./09_treemap-and-treeset.md)) |
| `PriorityQueue` | Which element is the head ([10](./10_priorityqueue.md)) |
| `Collections.sort`, `List.sort`, `Arrays.sort`, `stream.sorted` | Sorting |
| `Collections.binarySearch` | The list must be sorted with the **same** ordering |
| `Collections.max`/`min`, `stream.max`/`min` | Extremes |

## Comparator consistency problems

A comparator that is not a valid total order can make `sort` throw or misbehave:

```
java.lang.IllegalArgumentException: Comparison method violates its general contract!
```

Typical causes:

```java
(a, b) -> a.score > b.score ? 1 : -1                    // never returns 0: not antisymmetric for equal scores
(a, b) -> a.x - b.x                                      // overflow
(a, b) -> Math.random() < 0.5 ? -1 : 1                   // not even consistent: use Collections.shuffle
// Comparing mixed-type or partially defined fields (null handling missing)
```

A comparator must be consistent: the same inputs give the same result; it is antisymmetric and transitive.

## `Comparable` vs `Comparator`: which to choose

| Situation | Choose |
|-----------|--------|
| One obvious, universal ordering you control | `Comparable` (and keep it consistent with `equals`) |
| Several orderings (by name, by age, by date) | `Comparator`s |
| You cannot modify the class (library or JDK type) | `Comparator` |
| Sorting by a field of a record or DTO | `Comparator.comparing(...)` |
| Ordering depends on context, user setting or locale | `Comparator` (or `Collator`) |
| Used as a `TreeMap` key with no explicit comparator | The key type must be `Comparable` |

You can combine both: implement `Comparable` for the default and expose named `Comparator` constants:

```java
public record Person(String name, int age) implements Comparable<Person> {
    public static final Comparator<Person> BY_AGE = Comparator.comparingInt(Person::age);
    private static final Comparator<Person> NATURAL = Comparator.comparing(Person::name).thenComparingInt(Person::age);
    @Override public int compareTo(Person o) { return NATURAL.compare(this, o); }
}
```

## Locale-aware text ordering

`String.compareTo` compares UTF-16 code units: `"Z" < "a"` and accented letters sort after `"z"`. For user-facing sorting use a `Collator`:

```java
Collator collator = Collator.getInstance(Locale.FRENCH);
names.sort(collator);                      // Collator implements Comparator<Object>
```

See [03-strings-and-text/02_string-pool-and-comparison.md](../03-strings-and-text/02_string-pool-and-comparison.md).

## Generics: `Comparator<? super T>`

APIs accept `Comparator<? super T>` so that a comparator for a **supertype** can sort subtypes ([07-generics/02_wildcards-and-pecs.md](../07-generics/02_wildcards-and-pecs.md)):

```java
Comparator<Object> byString = Comparator.comparing(Object::toString);
List<Integer> ints = new ArrayList<>(List.of(10, 9, 100));
ints.sort(byString);                      // [10, 100, 9]: compares as strings
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `a.x - b.x` in `compareTo`/`compare` | Wrong order for large or negative values | `Integer.compare` |
| `(int) (a.price - b.price)` | Differences under 1 vanish | `Double.compare` |
| Comparator never returns `0` for equal items | `Comparison method violates its general contract!` | Return 0 when equal; use `compare` helpers |
| `compareTo` inconsistent with `equals` | `TreeSet` drops "different" objects; sets differ | Compare every field that `equals` uses |
| `Comparator.comparing(p -> p.age()).reversed()` | Compile error | Method reference or typed lambda |
| Natural ordering used on a type that is not `Comparable` | `ClassCastException` at runtime (in `TreeSet`, `sort`) | Provide a `Comparator` |
| Not handling `null` | `NullPointerException` | `nullsFirst`/`nullsLast` |
| `binarySearch` with a different comparator than the sort | Wrong result | Same ordering for both |
| Sorting user-facing text with `compareTo` | Wrong order for accents/case | `Collator` |
| Sorting an immutable list (`List.of`) | `UnsupportedOperationException` | Copy to a new `ArrayList` first |
| Mutating the sort key of elements already in a `TreeSet`/`PriorityQueue` | Broken ordering | Remove, change, re-add |

## Key takeaways

- `Comparable` = one natural order inside the class; `Comparator` = any number of external orders
- Use the `compare` helpers and `Comparator.comparing(...).thenComparing(...)`; never subtract
- Keep `compareTo` consistent with `equals`; make comparators antisymmetric and transitive
- Object sorting is stable and O(n log n); tree collections and `PriorityQueue` rely on ordering for correctness
- Use `Collator` for human-language sorting

**Next:** [ArrayList](./06_arraylist.md)
