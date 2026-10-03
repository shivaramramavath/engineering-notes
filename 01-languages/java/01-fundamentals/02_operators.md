# Operators

**Operators** combine values into expressions. Most of Java's surprises here come from integer arithmetic, `==` on objects, and evaluation order.

```java
int total = (price * qty) + shipping;      // arithmetic, grouping
boolean ok = age >= 18 && hasId;           // relational + logical
String label = ok ? "adult" : "minor";     // ternary
```

## Arithmetic

| Operator | Meaning | Example | Result |
|----------|---------|---------|--------|
| `+` | Add (or string concat) | `5 + 2` | `7` |
| `-` | Subtract | `5 - 2` | `3` |
| `*` | Multiply | `5 * 2` | `10` |
| `/` | Divide | `5 / 2` | `2` (integer division) |
| `%` | Remainder | `5 % 2` | `1` |

```java
System.out.println(5 / 2);        // 2    (both ints: fraction discarded)
System.out.println(5 / 2.0);      // 2.5  (one double: double division)
System.out.println(-5 / 2);       // -2   (truncates toward zero)
System.out.println(-7 % 3);       // -1   (sign follows the left operand)
System.out.println(Math.floorMod(-7, 3));   // 2  (always non-negative for positive divisor)
```

### Division by zero

```java
int a = 10 / 0;          // ArithmeticException: / by zero
double b = 10.0 / 0;     // Infinity
double c = 0.0 / 0;      // NaN
int d = 10 % 0;          // ArithmeticException
```

### Increment and decrement

```java
int i = 5;
int a = i++;    // a = 5, i = 6   (post: use value, then increment)
int b = ++i;    // b = 7, i = 7   (pre: increment, then use value)
```

`i = i++;` leaves `i` unchanged. Avoid using `++` inside larger expressions.

## Assignment and compound assignment

```java
int x = 10;
x += 5;     // x = x + 5
x -= 3;  x *= 2;  x /= 4;  x %= 3;
x <<= 1;  x >>= 1;  x &= 0xF;  x |= 1;  x ^= 2;
```

Compound assignment includes an **implicit cast**:

```java
byte b = 10;
b = b + 5;       // ERROR: int cannot be assigned to byte
b += 5;          // OK: b = (byte) (b + 5)

int n = 10;
n *= 1.5;        // n = (int) (n * 1.5) = 15, silently truncates
```

## Relational

`==`, `!=`, `<`, `>`, `<=`, `>=` return `boolean`.

```java
5 == 5            // true
'a' < 'b'         // true (compares char codes)
```

### `==` on references compares identity

```java
String a = new String("hi");
String b = new String("hi");
a == b            // false: different objects
a.equals(b)       // true: same content
```

Use `.equals()` for objects. See [string comparison](../03-strings-and-text/02_string-pool-and-comparison.md) and [wrappers](./04_wrapper-classes-and-autoboxing.md).

## Logical

| Operator | Meaning | Short-circuits? |
|----------|---------|-----------------|
| `&&` | AND | Yes: skips the right side if the left is `false` |
| `\|\|` | OR | Yes: skips the right side if the left is `true` |
| `!` | NOT | n/a |
| `&` `\|` `^` | Non-short-circuit AND, OR, XOR | No |

```java
if (s != null && s.length() > 0) { ... }    // safe: length() is skipped when s is null
if (s != null & s.length() > 0) { ... }     // NullPointerException: both sides evaluated
```

Put the **cheap or guarding** condition first.

## Bitwise and shift

Work on the bits of integer types.

| Operator | Meaning | Example (`int`) |
|----------|---------|-----------------|
| `&` | AND | `0b1100 & 0b1010` → `0b1000` |
| `\|` | OR | `0b1100 \| 0b1010` → `0b1110` |
| `^` | XOR | `0b1100 ^ 0b1010` → `0b0110` |
| `~` | NOT (flip all bits) | `~0` → `-1` |
| `<<` | Shift left (×2 per step) | `1 << 3` → `8` |
| `>>` | Shift right, keeps the sign | `-8 >> 1` → `-4` |
| `>>>` | Shift right, fills with 0 | `-8 >>> 1` → `2147483644` |

```java
boolean isEven = (n & 1) == 0;       // check the lowest bit
int flags = READ | WRITE;            // combine flags
boolean canWrite = (flags & WRITE) != 0;
int pow2 = 1 << k;                   // 2^k
```

Shift distance is masked: for `int`, `1 << 32` equals `1` (shift by `32 & 31 = 0`).

## Ternary

```java
int max = a > b ? a : b;
String s = n == 1 ? "item" : "items";
```

Fine for simple choices; avoid nesting. Both branches are subject to type promotion ([casting](./03_type-casting-and-conversion.md)).

## `instanceof`

```java
if (obj instanceof String s) {          // type test + cast in one (Java 16+)
    System.out.println(s.length());
}
```

More in [12-modern-java/04_pattern-matching.md](../12-modern-java/04_pattern-matching.md).

## String concatenation

`+` with a `String` operand concatenates. Evaluation is **left to right**.

```java
1 + 2 + "3"         // "33"   (1 + 2 = 3, then "3")
"1" + 2 + 3         // "123"
"1" + (2 + 3)       // "15"
"" + 'a' + 'b'      // "ab"
'a' + 'b' + ""      // "195"  (char + char = int 195)
```

Concatenating in a loop is slow; use `StringBuilder` ([details](../03-strings-and-text/03_stringbuilder-and-stringbuffer.md)).

## Precedence (high → low)

| Level | Operators |
|-------|-----------|
| 1 | `x++` `x--` |
| 2 | `++x` `--x` `+x` `-x` `~` `!` (unary) |
| 3 | `*` `/` `%` |
| 4 | `+` `-` |
| 5 | `<<` `>>` `>>>` |
| 6 | `<` `<=` `>` `>=` `instanceof` |
| 7 | `==` `!=` |
| 8 | `&` |
| 9 | `^` |
| 10 | `\|` |
| 11 | `&&` |
| 12 | `\|\|` |
| 13 | `?:` |
| 14 | `=` `+=` `-=` ... (right to left) |

```java
int r = 2 + 3 * 4;           // 14
boolean b = a || c && d;     // a || (c && d)
int m = x & 1 == 0;          // ERROR: parsed as x & (1 == 0)
```

**When in doubt, add parentheses.** Readability beats memorizing the table.

## Evaluation order

Operands are evaluated **left to right**, regardless of precedence.

```java
int i = 1;
int r = i + (i = 5) + i;     // 1 + 5 + 5 = 11
```

Code like this is legal but unreadable. Do not write it.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `double avg = sum / count;` with ints | Truncated result | `(double) sum / count` |
| `a == b` on `String` / `Integer` objects | Wrong `false` (or accidental `true`) | `.equals()` |
| `if (x = 5)` | Compile error for ints; silent bug for `boolean` | Use `==` |
| `&` instead of `&&` with a null guard | `NullPointerException` | `&&` |
| `i = i++` | `i` does not change | `i++` alone |
| `x & 1 == 0` | Compile error or wrong result | `(x & 1) == 0` |
| Negative `%` result | `-7 % 3` is `-1` | `Math.floorMod` |
| `1 << 32` | Equals `1` | Use `long`: `1L << 32` |
| Integer overflow | Wrap-around | See [numeric precision](./08_numeric-precision-and-math.md) |

## Key takeaways

- `int / int` is integer division; make one side `double` for a fractional result
- `==` compares primitives by value and objects by identity; use `.equals()` for content
- `&&` and `||` short-circuit; `&` and `|` do not
- Compound assignment hides an implicit cast
- Use parentheses instead of relying on precedence

**Next:** [Type Casting and Conversion](./03_type-casting-and-conversion.md)
