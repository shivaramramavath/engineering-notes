# String Basics

A `String` is an **immutable** sequence of characters. It is an ordinary class (`java.lang.String`) with special language support: literals in double quotes and `+` for concatenation.

```java
String name = "Ada";
String greeting = "Hello, " + name + "!";
System.out.println(greeting.length());     // 11
```

## Strings are objects

```
String s = "Java";

 s ───► ┌──────────────┐
        │ String object│   characters: J a v a
        │  "Java"      │   (immutable)
        └──────────────┘
```

A `String` variable holds a **reference** (or `null`), not the characters directly ([variables](../01-fundamentals/01_variables-and-data-types.md)).

## Creating strings

```java
String a = "hello";                       // literal (goes into the string pool)
String b = new String("hello");           // new object on the heap: rarely needed
String c = String.valueOf(42);            // "42"
String d = String.valueOf(3.14);          // "3.14"
String e = String.valueOf(new char[]{'h', 'i'});   // "hi"
String f = new String(new char[]{'h', 'i'});       // "hi"
String g = String.join(", ", "a", "b", "c");       // "a, b, c"
String h = "ab".repeat(3);                          // "ababab" (Java 11+)
String i = "";                                      // empty string
```

Prefer **literals**. `new String("hello")` creates a needless duplicate ([pool and comparison](./02_string-pool-and-comparison.md)).

## Immutability

Once created, a `String` never changes. Methods that look like they modify it return a **new** string.

```java
String s = "java";
s.toUpperCase();                    // returns "JAVA", but the result is thrown away
System.out.println(s);              // java

s = s.toUpperCase();                // reassign to keep the result
System.out.println(s);              // JAVA
```

```
Before:   s ──► "java"
After:    s ──► "JAVA"        ("java" is unchanged and becomes garbage if unreferenced)
```

### Why strings are immutable

| Reason | Benefit |
|--------|---------|
| **Safe sharing** | Many references can share one object, so literals can be pooled |
| **Security** | A file path, URL or class name cannot change after validation |
| **Thread safety** | No synchronization needed |
| **Hash caching** | `hashCode` is computed once, making strings ideal `HashMap` keys |
| **Predictability** | A method cannot modify the string you passed in ([pass-by-value](../02-methods/01_pass-by-value.md)) |

The class is also `final`, so it cannot be subclassed.

## Length and characters

```java
String s = "Hello";
s.length();          // 5       (a METHOD, unlike array.length)
s.charAt(0);         // 'H'     (zero-based)
s.charAt(4);         // 'o'
s.charAt(5);         // StringIndexOutOfBoundsException
s.isEmpty();         // false   (length() == 0)
"   ".isBlank();     // true    (empty or only whitespace, Java 11+)
```

## Concatenation

```java
String full = "Hello" + ", " + "world";
String s = "Total: " + 5 + 3;           // "Total: 53"   (left to right)
String t = "Total: " + (5 + 3);         // "Total: 8"
String u = 1 + 2 + " items";            // "3 items"
String v = "x" + null;                  // "xnull"
String w = "x" + 'a' + 1.5 + true;      // "xa1.5true"
```

`+` converts the non-string operand using `String.valueOf` (objects use `toString()`, and `null` becomes `"null"`).

### The loop problem

Each `+` creates a new string, copying all characters. In a loop that is quadratic:

```java
String s = "";
for (int i = 0; i < 100_000; i++) {
    s += i;                              // new String every iteration: O(n²) total
}

StringBuilder sb = new StringBuilder();
for (int i = 0; i < 100_000; i++) {
    sb.append(i);                        // amortized O(1) per append
}
```

A single expression such as `"a" + b + "c"` is fine; the compiler optimizes it. Use `StringBuilder` when building in a loop ([03_stringbuilder-and-stringbuffer.md](./03_stringbuilder-and-stringbuffer.md)).

## `null`, empty and blank

| Value | `s == null` | `s.isEmpty()` | `s.isBlank()` | `s.length()` |
|-------|-------------|---------------|---------------|--------------|
| `null` | true | `NullPointerException` | `NullPointerException` | `NullPointerException` |
| `""` | false | true | true | 0 |
| `"   "` | false | false | true | 3 |
| `"abc"` | false | false | false | 3 |

```java
if (s == null || s.isBlank()) { ... }     // null-safe "no real content" check
```

```java
Objects.toString(s, "")          // null becomes "" (safe default)
String.valueOf((Object) s)       // null becomes "null"
```

## Strings and `char`

```java
char c = 'A';              // single quotes: one UTF-16 code unit
String s = "A";            // double quotes: a String of length 1

String fromChar = String.valueOf(c);   // or "" + c, or Character.toString(c)
char first = "Hello".charAt(0);
char[] chars = "Hello".toCharArray();  // copy as a char array
String back = new String(chars);       // or String.valueOf(chars)

for (char ch : "Hello".toCharArray()) { ... }          // iterate
"Hello".chars().filter(Character::isUpperCase).count(); // as an IntStream
```

`char` arithmetic gives `int` ([operators](../01-fundamentals/02_operators.md)):

```java
'a' + 1                     // 98
(char) ('a' + 1)            // 'b'
"abc".charAt(0) - 'a'       // 0: position in the alphabet
```

Characters outside the basic range need more than one `char`: [unicode and encoding](./07_unicode-and-encoding.md).

## Strings in `switch`

```java
switch (command) {
    case "start": start(); break;
    case "stop":  stop();  break;
    default:      System.out.println("Unknown: " + command);
}
```

Matching uses `equals`. A `null` selector throws `NullPointerException` ([conditionals](../01-fundamentals/05_conditionals.md)).

## Conversions

```java
Integer.parseInt("42");          // String → int
Double.parseDouble("3.14");      // String → double
String.valueOf(42);              // int → String
Integer.toString(42);            // int → String
"" + 42;                         // works, but less explicit
```

See [type casting](../01-fundamentals/03_type-casting-and-conversion.md) for parse errors.

## Under the hood

- Since Java 9, strings use **compact strings**: a byte array in Latin-1 when every character fits, otherwise UTF-16. This is invisible to your code and halves memory for typical English text
- `hashCode()` is cached after the first call
- Literals live in the **string pool** ([next topics](./02_string-pool-and-comparison.md))
- `substring` copies its characters (since Java 7u6); holding a small substring does not keep a huge original string alive

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `s.toUpperCase();` and ignoring the result | String unchanged | `s = s.toUpperCase();` |
| `s.length` or `arr.length()` | Compile error | `s.length()`, `arr.length` |
| `s == "text"` | Unreliable | `"text".equals(s)` |
| `s += x` in a big loop | Slow | `StringBuilder` |
| Calling methods on a `null` string | `NullPointerException` | Check, or put the literal first: `"x".equals(s)` |
| `char c = "A";` | `incompatible types` | `'A'` for char |
| `"Total: " + 5 + 3` expecting `8` | `"Total: 53"` | `(5 + 3)` |
| Treating `""` and `null` as the same | Surprising crashes | Handle both explicitly |
| `charAt(length())` | `StringIndexOutOfBoundsException` | Last index is `length() - 1` |
| Storing passwords in `String` | Cannot be wiped from memory | Use `char[]` where practical ([21-security](../21-security/README.md)) |

## Key takeaways

- `String` is an immutable, final class with literal and `+` support
- Methods return new strings; assign the result
- Use literals; avoid `new String(...)`
- Use `StringBuilder` for repeated concatenation in loops
- Distinguish `null`, empty and blank

**Next:** [String Methods](./01_string-methods.md)
