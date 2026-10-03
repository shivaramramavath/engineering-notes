# String Pool and Comparison

Java keeps one shared copy of each string literal in the **string pool**. Understanding it explains why `==` sometimes works on strings and sometimes does not, which is a favorite interview topic and a frequent real-world bug.

```java
String a = "hello";
String b = "hello";
String c = new String("hello");

a == b           // true   same pooled object
a == c           // false  c is a separate object
a.equals(c)      // true   same content
```

## The string pool

```
          String pool (inside the heap)
          ┌─────────────────────────┐
 a ─────► │  "hello"                │ ◄───── b      (literals share one object)
          └─────────────────────────┘

          Heap (normal objects)
          ┌─────────────────────────┐
 c ─────► │  String "hello"         │            (new String creates a separate copy)
          └─────────────────────────┘
```

| Fact | Detail |
|------|--------|
| What is pooled | String **literals** and compile-time constant expressions |
| Where | In the heap (since Java 7; earlier it was in PermGen) |
| Why | Saves memory and enables fast sharing of identical literals |
| Safe because | Strings are immutable ([basics](./00_string-basics.md)) |

## Which strings are in the pool?

```java
String a = "hello";                    // literal: pooled
String b = "hel" + "lo";               // constant expression: folded at compile time → pooled
String c = new String("hello");        // new object (the literal "hello" is pooled too)
String part = "hel";
String d = part + "lo";                // runtime concatenation: new object, NOT pooled
final String fpart = "hel";
String e = fpart + "lo";               // fpart is a compile-time constant → pooled

a == b;          // true
a == c;          // false
a == d;          // false
a == e;          // true
a.equals(d);     // true
```

Strings built at runtime (concatenation of variables, `StringBuilder.toString()`, `substring`, input from `Scanner`, files, network) are **not** pooled automatically.

## `intern()`

`intern()` returns the pooled instance with the same content, adding it if absent.

```java
String c = new String("hello");
String d = c.intern();
"hello" == d;            // true
c == d;                  // false (c is still the heap copy)
```

| Pros | Cons |
|------|------|
| Can reduce memory when millions of duplicate strings exist | A lookup in a global table on every call |
| Allows `==` comparisons on interned strings | Easy to misuse; fragile code |

Rarely needed. For large duplicate-heavy workloads, G1 offers `-XX:+UseStringDeduplication` ([20-performance/02_memory-optimization.md](../20-performance/02_memory-optimization.md)); or deduplicate in your own `Map`.

## `==` vs `equals`

| | Compares | Use for |
|---|----------|---------|
| `==` | **References** (same object?) | Primitives, enums, identity checks |
| `equals` | **Content** | Strings and other objects |

```java
Scanner in = new Scanner(System.in);
String answer = in.nextLine();           // user types: yes
answer == "yes"                          // false: runtime string vs pooled literal
answer.equals("yes")                     // true
```

Code that works only because two literals happen to be pooled is a bug waiting to happen. **Always use `equals` for strings.**

### Null-safe comparison

```java
String s = null;
s.equals("x");                  // NullPointerException
"x".equals(s);                  // false (literal first: never throws)
Objects.equals(s, other);       // true if both null or equal; never throws
```

### Case-insensitive and other comparisons

```java
"Java".equalsIgnoreCase("JAVA");        // true
"Java".contentEquals(stringBuilder);    // compare with any CharSequence
"hello".regionMatches(1, "ell", 0, 3);  // true
```

`equalsIgnoreCase` does simple per-character case folding; for locale-correct comparison of natural language, use `Collator` (below).

## Ordering: `compareTo`

```java
"apple".compareTo("banana");       // negative: apple comes first
"banana".compareTo("apple");       // positive
"apple".compareTo("apple");        // 0
"apple".compareTo("apples");       // -1 (a prefix is smaller; result is the length difference)
"Zebra".compareTo("apple");        // negative: uppercase letters come before lowercase
```

- Compares **UTF-16 code units** one by one; the first difference decides (the result is the difference of those values)
- If one string is a prefix of the other, the shorter is smaller
- Rely only on the **sign** of the result, not its exact value

### Sorting strings

```java
List<String> names = new ArrayList<>(List.of("bob", "Alice", "carol"));

Collections.sort(names);                                  // [Alice, bob, carol]  code-unit order
names.sort(String.CASE_INSENSITIVE_ORDER);                // [Alice, bob, carol]
names.sort(Comparator.comparing(String::length)
                     .thenComparing(Comparator.naturalOrder()));

Collator collator = Collator.getInstance(Locale.FRENCH);  // language-aware ordering
names.sort(collator);
```

Natural `compareTo` order is **not** dictionary order for accents (`é` sorts after `z`) or mixed case. Use `Collator` for user-facing sorting. More: [08-collections/05_comparable-and-comparator.md](../08-collections/05_comparable-and-comparator.md).

## `equals`, `hashCode` and `HashMap` keys

`String` overrides `equals` and `hashCode` consistently, which is why it is the most common map key:

```java
Map<String, Integer> ages = new HashMap<>();
ages.put("Ada", 36);
ages.get(new String("Ada"));       // 36: lookup by content, not identity
```

The hash code is cached inside the string, so repeated lookups are cheap ([04-oop/14_equals-and-hashcode.md](../04-oop/14_equals-and-hashcode.md), [08-collections/08_hashmap-and-hashset.md](../08-collections/08_hashmap-and-hashset.md)).

## Predict the output

```java
String s1 = "java";
String s2 = "ja" + "va";
String s3 = new String("java");
String s4 = s3.intern();
String x = "ja";
String s5 = x + "va";

System.out.println(s1 == s2);        // ?
System.out.println(s1 == s3);        // ?
System.out.println(s1 == s4);        // ?
System.out.println(s1 == s5);        // ?
System.out.println(s1.equals(s5));   // ?
System.out.println(s3.equals(s5));   // ?
```

Answers: `true`, `false`, `true`, `false`, `true`, `true`.

## How many objects does `new String("hello")` create?

At most **two**: the pooled literal `"hello"` (created once, when first loaded) and the new heap `String`. If `"hello"` is already in the pool, only one new object is created.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `s == "text"` | Works in tests with literals, fails with input | `"text".equals(s)` |
| `s.equals("text")` with a possibly-`null` `s` | `NullPointerException` | `"text".equals(s)` or `Objects.equals` |
| Using `new String("x")` | Extra object, breaks `==` assumptions | Use a literal |
| Relying on `intern()` for correctness | Fragile, global state | Use `equals` |
| Comparing with `compareTo` for equality | Works, but awkward | `equals` |
| Using the exact value of `compareTo` | Implementation detail | Use only the sign |
| Sorting user-visible text with natural order | Wrong order for case and accents | `Collator` |
| `switch` on a possibly-`null` string | `NullPointerException` | Null-check first |
| Case-insensitive checks with `toLowerCase()` and default locale | Turkish locale bug | `equalsIgnoreCase`, `Locale.ROOT` |

## Key takeaways

- Literals and compile-time constants are pooled; runtime-created strings are not
- `==` compares references, `equals` compares content: use `equals` for strings
- Put the literal first (`"x".equals(s)`) or use `Objects.equals` to avoid `NullPointerException`
- `compareTo` orders by UTF-16 code units; use `Collator` for human-facing sorting
- `intern()` exists but is rarely the right tool

**Next:** [StringBuilder and StringBuffer](./03_stringbuilder-and-stringbuffer.md)
