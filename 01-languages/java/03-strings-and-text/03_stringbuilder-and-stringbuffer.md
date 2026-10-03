# StringBuilder and StringBuffer

`String` is immutable, so every concatenation allocates a new string. **`StringBuilder`** is a **mutable** sequence of characters designed for building text efficiently. **`StringBuffer`** is the older, synchronized version.

```java
StringBuilder sb = new StringBuilder();
sb.append("Hello").append(", ").append("World").append('!');
String result = sb.toString();            // "Hello, World!"
```

## Why not just `+`?

```java
String s = "";
for (int i = 0; i < 50_000; i++) {
    s += i;                      // each step copies the whole string so far: O(n²) total
}
```

```
String +=:   "1" → "12" → "123" → "1234" ...   every step allocates and copies everything
StringBuilder: one growable buffer, characters appended in place
```

| | `String +` in a loop | `StringBuilder` |
|---|---------------------|-----------------|
| Time for n appends | O(n²) | O(n) amortized |
| Garbage created | A new string per step | Almost none |
| Mutable | No | Yes |

A **single expression** like `"a" + x + "b"` is fine: the compiler already turns it into efficient code. The problem is concatenation **inside loops**.

## Core API

```java
StringBuilder sb = new StringBuilder("Hello");

sb.append(" World");          // "Hello World"      (accepts any type)
sb.append(42).append('!');    // "Hello World42!"
sb.insert(0, ">> ");          // ">> Hello World42!"
sb.delete(0, 3);              // "Hello World42!"   (end exclusive)
sb.deleteCharAt(sb.length() - 1);   // "Hello World42"
sb.replace(0, 5, "Howdy");    // "Howdy World42"
sb.reverse();                 // "24dlroW ydwoH"
sb.setCharAt(0, 'X');         // replace one character
sb.setLength(5);              // truncate (or pad with '\0' if longer)
sb.length();                  // number of characters
sb.charAt(2);                 // read a character
sb.indexOf("W");              // search
sb.toString();                // copy into an immutable String
sb.isEmpty();                 // Java 15+
```

Every mutator returns `this`, so calls can be **chained**.

### Common patterns

```java
// Join with separators, no trailing comma
StringBuilder sb = new StringBuilder();
for (String item : items) {
    if (sb.length() > 0) sb.append(", ");
    sb.append(item);
}

// Or: remove the last separator
sb.setLength(sb.length() - 2);          // only if at least one item was added

// Build a table row
sb.append(String.format("%-10s %5d%n", name, qty));

// Reverse a string
String reversed = new StringBuilder(s).reverse().toString();

// Repeat
sb.append("=".repeat(40)).append('\n');
```

For simple joins prefer `String.join` or `Collectors.joining` ([string methods](./01_string-methods.md)).

## `StringBuilder` vs `StringBuffer`

| | `StringBuilder` | `StringBuffer` |
|---|-----------------|----------------|
| Since | Java 5 | Java 1.0 |
| Thread-safe | **No** | Yes (methods are `synchronized`) |
| Speed | Faster | Slower (lock overhead) |
| Use for | Almost everything | Legacy code needing a shared buffer |

A `StringBuilder` is normally a **local variable**, used by one thread, so synchronization is wasted work. Even when shared, synchronizing single method calls rarely makes a compound operation (check, then append) safe. Use `StringBuilder` by default; reach for `StringBuffer` only when an API requires it or when several threads genuinely append to one buffer ([14-concurrency/02_thread-safety.md](../14-concurrency/02_thread-safety.md)).

## Capacity

A builder holds a buffer; when it is full, it allocates a larger one (roughly doubling) and copies.

```java
new StringBuilder();            // initial capacity 16
new StringBuilder(1000);        // preallocate when you know the rough size
new StringBuilder("abc");       // capacity = 16 + 3

sb.capacity();                  // current buffer size
sb.ensureCapacity(10_000);
sb.trimToSize();
```

Preallocating avoids repeated copying when building very large strings.

## Pitfalls specific to builders

### `equals` and `hashCode` are not overridden

```java
StringBuilder a = new StringBuilder("hi");
StringBuilder b = new StringBuilder("hi");
a.equals(b);                          // false (identity!)
a.toString().equals(b.toString());    // true
a.compareTo(b);                       // 0 (Comparable since Java 11)
a.isEmpty();
"hi".contentEquals(a);                // true
```

Never use a `StringBuilder` as a map key or in a `Set`; it is mutable and uses identity equality.

### `toString()` copies

Each `toString()` creates a new `String`. Call it once, after building, not inside a loop.

### Passing a builder to a method

It is mutable and shared through the reference ([pass-by-value](../02-methods/01_pass-by-value.md)):

```java
static void addSuffix(StringBuilder sb) { sb.append("!"); }    // caller sees the change
```

### `insert(0, ...)` and `delete` are O(n)

They shift the remaining characters. Inserting at the front in a loop is quadratic; append and reverse instead, or use a `Deque`.

## Related tools

### `StringJoiner`

```java
StringJoiner sj = new StringJoiner(", ", "[", "]");
sj.add("a").add("b").add("c");
sj.toString();                          // "[a, b, c]"
sj.setEmptyValue("[]");                 // result when nothing was added
```

### `String.join` and `Collectors.joining`

```java
String.join(", ", list);                                  // simplest for a collection
list.stream().map(Object::toString)
    .collect(Collectors.joining(", ", "{", "}"));         // transform, then join
```

### `CharSequence`

`String`, `StringBuilder` and `StringBuffer` all implement `CharSequence` (`length`, `charAt`, `subSequence`). Accept `CharSequence` in method parameters when you only need to read characters.

## Which one should I use?

| Task | Use |
|------|-----|
| A few values in one expression | `+` or `formatted` |
| Building text in a loop | `StringBuilder` |
| Joining a collection with a separator | `String.join` / `Collectors.joining` |
| Template with placeholders | `String.format` / `formatted` ([formatting](./04_string-formatting.md)) |
| Multi-line literal | Text block ([text blocks](./05_text-blocks.md)) |
| Several threads appending to one shared buffer | `StringBuffer` (or better, collect per thread, then merge) |
| Heavy character-level edits (reverse, insert, delete) | `StringBuilder` |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `s += x` in a loop | Slow, lots of garbage | `StringBuilder` |
| Using `StringBuffer` everywhere "to be safe" | Needless slowdown | `StringBuilder` |
| `sb.toString()` inside the loop | Extra copies | Call once at the end |
| Comparing builders with `==` or `equals` | Always `false` for different objects | Compare `toString()` or use `contentEquals` |
| Trailing separator | `"a, b, "` | Check `length() > 0` or use `String.join` |
| `setLength(length() - 2)` on an empty builder | `StringIndexOutOfBoundsException` | Guard with `if (sb.length() > 0)` |
| Reusing one builder across tasks without clearing | Old text remains | `sb.setLength(0)` |
| Sharing a `StringBuilder` between threads | Corrupted text | One per thread, or `StringBuffer` |
| Repeated `insert(0, ...)` | Quadratic time | Append then `reverse()`, or use a `Deque` |
| Building tiny strings with `StringBuilder` | More code than needed | Plain `+` |

## Key takeaways

- `StringBuilder` is a mutable buffer for efficient string building; call `toString()` once at the end
- Use it for concatenation in loops; plain `+` is fine for one-off expressions
- `StringBuffer` is the synchronized legacy twin; prefer `StringBuilder`
- Builders use identity `equals`: compare their string contents instead
- `String.join`, `Collectors.joining` and `StringJoiner` handle separators neatly

**Next:** [String Formatting](./04_string-formatting.md)
