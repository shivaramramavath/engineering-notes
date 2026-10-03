# Type Casting and Conversion

Converting a value from one type to another. Java converts **safely and automatically** when no information can be lost (widening), and requires an **explicit cast** when it can (narrowing).

```
Widening (automatic)                      Narrowing (explicit cast required)

byte ─► short ─► int ─► long ─► float ─► double       double ─► float ─► long ─► int ─► short ─► byte
              ▲
            char                                      int x = (int) 3.99;   // 3
```

## Widening (implicit)

```java
int i = 100;
long l = i;          // int → long
double d = l;        // long → double
char c = 'A';
int code = c;        // char → int: 65
```

Widening is allowed even when precision can be lost for `int → float` and `long → float/double`:

```java
int big = 16_777_217;       // 2^24 + 1
float f = big;
System.out.println((int) f);  // 16777216: the last digit was lost
```

## Narrowing (explicit cast)

```java
double d = 9.78;
int i = (int) d;             // 9: fraction truncated (not rounded)
long l = 130;
byte b = (byte) l;           // -126: bits wrapped around
```

| Cast | Example | Result |
|------|---------|--------|
| `double → int` | `(int) 3.99` | `3` |
| | `(int) -3.99` | `-3` (toward zero) |
| | `(int) 1e20` | `2147483647` (clamps to `Integer.MAX_VALUE`) |
| | `(int) Double.NaN` | `0` |
| `int → byte` | `(byte) 130` | `-126` (wraps: 130 - 256) |
| `int → char` | `(char) 65` | `'A'` |
| `char → int` | `(int) 'A'` | `65` |
| `long → int` | `(int) 4_294_967_297L` | `1` (high bits discarded) |

**Floating → integer clamps; integer → smaller integer wraps.** Neither warns you at runtime.

To round instead of truncate use `Math.round`, and to fail on overflow use `Math.toIntExact`:

```java
Math.round(3.6);                  // 4  (returns long)
Math.toIntExact(5_000_000_000L);  // ArithmeticException: integer overflow
```

## Numeric promotion in expressions

Before arithmetic, Java promotes operands:

1. If either is `double` → `double`; else if either is `float` → `float`; else if either is `long` → `long`
2. Otherwise **both become `int`** (this includes `byte`, `short` and `char`)

```java
byte a = 10, b = 20;
byte c = a + b;              // ERROR: a + b is int
byte d = (byte) (a + b);     // OK

char ch = 'A';
int next = ch + 1;           // 66 (int)
char nextCh = (char) (ch + 1);   // 'B'
ch++;                        // OK: ++ and += include an implicit cast
```

### The `int / int` trap

```java
int x = 5;
double r1 = x / 2;           // 2.0   (division happens in int first)
double r2 = x / 2.0;         // 2.5
double r3 = (double) x / 2;  // 2.5   (cast applies to x, then promotion)
double r4 = (double) (x / 2);// 2.0   (cast too late)
```

### Constants that fit are allowed

```java
byte b = 10;                 // OK: constant 10 fits in a byte
byte c = 200;                // ERROR: does not fit
final int K = 10;
byte d = K;                  // OK: compile-time constant that fits
int v = 10;
byte e = v;                  // ERROR: v is not a constant
```

### Ternary promotion

```java
Object o = true ? 1 : 2.0;   // 1.0 (the Integer is promoted to double)
```

## Converting to and from `String`

Casts do not convert between `String` and numbers.

```java
int n = (int) "42";                  // ERROR
```

| Direction | Code |
|-----------|------|
| Number → String | `String.valueOf(42)`, `Integer.toString(42)`, `"" + 42` |
| String → int | `Integer.parseInt("42")` |
| String → double | `Double.parseDouble("3.14")` |
| String → boolean | `Boolean.parseBoolean("true")` (any non-"true" gives `false`) |
| String → int with radix | `Integer.parseInt("ff", 16)` → `255` |
| char → int digit value | `Character.getNumericValue('7')` or `'7' - '0'` |

```java
Integer.parseInt("12a");     // NumberFormatException
Integer.parseInt(" 42");     // NumberFormatException (no trimming)
Integer.parseInt("42".trim());   // 42
```

Always validate or catch `NumberFormatException` when parsing user input.

## Reference casting (preview)

Casting between object types does **not** change the object, only how the compiler views it.

```java
Object o = "text";
String s = (String) o;       // OK
Integer n = (Integer) o;     // ClassCastException at runtime
```

Upcasting (to a parent type) is implicit and safe; downcasting needs a cast and can fail. Check with `instanceof` first. See [polymorphism](../04-oop/07_polymorphism.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `int pct = (double) a / b * 100;` | `possible lossy conversion` | `(int) ((double) a / b * 100)` |
| Assuming `(int) 3.99` rounds | Gets `3` | `Math.round` |
| Casting after integer division | `(double) (a / b)` is still truncated | `(double) a / b` |
| `byte c = a + b;` | Compile error | Cast the result |
| Narrowing without checking range | Silent wrap-around | `Math.toIntExact`, range checks |
| `(int) someString` | Compile error | `Integer.parseInt` |
| Parsing unvalidated input | `NumberFormatException` | Validate or catch |
| Downcasting without `instanceof` | `ClassCastException` | Test first |
| `char + int` printed as a number | `'a' + 1` prints `98` | `(char) ('a' + 1)` |

## Key takeaways

- Widening is automatic; narrowing needs `(type)` and can lose data
- `double → int` truncates; larger → smaller integer wraps; neither warns
- In arithmetic, `byte`, `short` and `char` become `int`; mixed numeric types promote to the widest
- Do the cast on an **operand**, not on the finished integer division
- Use `parseXxx`/`valueOf`, not casts, to convert between `String` and numbers

**Next:** [Wrapper Classes and Autoboxing](./04_wrapper-classes-and-autoboxing.md)
