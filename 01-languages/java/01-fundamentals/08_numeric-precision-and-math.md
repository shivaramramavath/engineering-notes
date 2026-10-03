# Numeric Precision and Math

Computers store numbers in a fixed number of bits. Integers **overflow**, and decimals like `0.1` **cannot be represented exactly** in binary. Most money and measurement bugs come from ignoring this.

```java
System.out.println(0.1 + 0.2);                 // 0.30000000000000004
System.out.println(Integer.MAX_VALUE + 1);     // -2147483648
```

## Integer overflow

Integer arithmetic **wraps around silently**.

```java
int max = Integer.MAX_VALUE;       // 2147483647
System.out.println(max + 1);       // -2147483648
System.out.println(Math.abs(Integer.MIN_VALUE));   // -2147483648 (no positive counterpart)

int f = 1;
for (int i = 1; i <= 13; i++) f *= i;    // 13! overflows an int
```

### Detect overflow with the `*Exact` methods

```java
Math.addExact(Integer.MAX_VALUE, 1);      // ArithmeticException: integer overflow
Math.multiplyExact(100_000, 100_000);     // ArithmeticException
Math.toIntExact(5_000_000_000L);          // ArithmeticException
Math.incrementExact(x);   Math.negateExact(x);   Math.subtractExact(a, b);
```

### Safer midpoint

```java
int mid = (low + high) / 2;               // can overflow for large values
int mid2 = low + (high - low) / 2;        // safe
int mid3 = (low + high) >>> 1;            // also safe
```

| Type | Max | Use when |
|------|-----|----------|
| `int` | about 2.1 × 10⁹ | Default |
| `long` | about 9.2 × 10¹⁸ | Counters, ids, timestamps, sums of many ints |
| `BigInteger` | Unlimited | Factorials, cryptography, exact huge values |

## Floating-point (IEEE 754)

`float` and `double` store a binary fraction. Values such as `0.1` have no exact binary form, the same way 1/3 has no exact decimal form.

| | `float` | `double` |
|---|---------|----------|
| Size | 32 bit | 64 bit |
| Decimal digits | about 7 | about 15-16 |
| Default for literals | `1.5f` | `1.5` |

```java
double a = 0.1 + 0.2;
System.out.println(a == 0.3);              // false
System.out.println(new BigDecimal(0.1));   // 0.1000000000000000055511151231257827...

float f = 0.1f;
double d = f;
System.out.println(d);                     // 0.10000000149011612
```

### Comparing floating-point numbers

```java
static boolean nearlyEqual(double a, double b) {
    return Math.abs(a - b) < 1e-9;                 // absolute tolerance
}
```

For values with very different magnitudes use a **relative** tolerance (`|a - b| <= eps * max(|a|, |b|)`).

### Special values

| Value | How it arises | Notes |
|-------|---------------|-------|
| `Infinity`, `-Infinity` | `1.0 / 0`, overflow | `Double.isInfinite(x)` |
| `NaN` | `0.0 / 0`, `Math.sqrt(-1)` | `NaN != NaN`; use `Double.isNaN(x)` |
| `-0.0` | `-1.0 * 0` | `0.0 == -0.0` is `true`, but `Double.compare(0.0, -0.0)` is `1` |

```java
double nan = 0.0 / 0;
nan == nan;                // false
Double.isNaN(nan);         // true
```

### Precision loss with large values

```java
double big = 1e16;
System.out.println(big + 1 == big);        // true: +1 is lost
float f = 16_777_216f;
System.out.println(f + 1 == f);            // true
```

Adding numbers of very different size loses the small one. Summing many values can accumulate error; consider sorting, `Math.fma`, or `BigDecimal` when exactness matters.

## The `Math` class

| Method | Result | Notes |
|--------|--------|-------|
| `Math.abs(-5)` | `5` | `abs(Integer.MIN_VALUE)` stays negative |
| `Math.max(a, b)`, `Math.min(a, b)` | | |
| `Math.pow(2, 10)` | `1024.0` | Returns `double` |
| `Math.sqrt(16)` | `4.0` | `NaN` for negatives |
| `Math.cbrt(27)` | `3.0` | |
| `Math.hypot(3, 4)` | `5.0` | Avoids intermediate overflow |
| `Math.floor(-1.5)` | `-2.0` | Toward negative infinity |
| `Math.ceil(-1.5)` | `-1.0` | Toward positive infinity |
| `Math.round(2.5)` | `3` (`long`) | `round(float)` returns `int` |
| `Math.rint(2.5)` | `2.0` | Round-half-even |
| `Math.floorDiv(-7, 2)`, `Math.floorMod(-7, 3)` | `-4`, `2` | Floor semantics for negatives |
| `Math.log(x)`, `log10`, `exp` | | Natural log, base-10, `e^x` |
| `Math.sin`, `cos`, `toRadians`, `toDegrees` | | Trig works in **radians** |
| `Math.clamp(v, lo, hi)` | | Java 21+ |

```java
Math.round(2.5);     // 3
Math.round(-2.5);    // -2   (rounds half toward positive infinity)
Math.round(2.4);    // 2
```

### Rounding to a number of decimals

```java
double rounded = Math.round(value * 100.0) / 100.0;     // 2 decimals, fine for display
```

For anything financial use `BigDecimal` with an explicit `RoundingMode`.

## `BigDecimal`: exact decimal arithmetic

Use it for **money, tax, and any value where decimal exactness matters**.

```java
BigDecimal a = new BigDecimal("0.1");           // from a String
BigDecimal b = new BigDecimal("0.2");
a.add(b);                                       // 0.3 (exact)

new BigDecimal(0.1);                            // BAD: carries the double's error
BigDecimal.valueOf(0.1);                        // OK: uses Double.toString → 0.1
```

**Always construct from a `String`** (or use `valueOf`), never from a `double` literal.

### Operations

```java
BigDecimal price = new BigDecimal("19.99");
BigDecimal qty = BigDecimal.valueOf(3);

price.multiply(qty);                            // 59.97
price.subtract(new BigDecimal("5"));            // 14.99
price.negate();  price.abs();  price.pow(2);

BigDecimal.ONE.divide(new BigDecimal("3"));
// ArithmeticException: Non-terminating decimal expansion

BigDecimal.ONE.divide(new BigDecimal("3"), 2, RoundingMode.HALF_UP);   // 0.33
BigDecimal.ONE.divide(new BigDecimal("3"), MathContext.DECIMAL64);     // 0.3333333333333333

price.setScale(1, RoundingMode.HALF_EVEN);      // 20.0
```

### Rounding modes

| Mode | `2.5` → | `-2.5` → | Notes |
|------|---------|----------|-------|
| `HALF_UP` | 3 | -3 | School rounding |
| `HALF_EVEN` | 2 | -2 | "Banker's rounding"; reduces bias in sums |
| `DOWN` | 2 | -2 | Truncate |
| `FLOOR` | 2 | -3 | Toward negative infinity |
| `CEILING` | 3 | -2 | Toward positive infinity |

### `equals` vs `compareTo`

```java
BigDecimal x = new BigDecimal("2.0");
BigDecimal y = new BigDecimal("2.00");
x.equals(y);                 // false: the scale differs
x.compareTo(y) == 0;         // true: numerically equal
```

Use `compareTo` for numeric comparison. Be careful with `BigDecimal` in `HashSet`/`HashMap` keys (`equals` includes scale), or normalize with `stripTrailingZeros()` or `setScale`.

## `BigInteger`: unlimited integers

```java
BigInteger f = BigInteger.ONE;
for (int i = 1; i <= 30; i++) f = f.multiply(BigInteger.valueOf(i));
System.out.println(f);                       // 265252859812191058636308480000000

new BigInteger("123456789012345678901234567890");
a.add(b);  a.mod(m);  a.pow(10);  a.gcd(b);  a.isProbablePrime(20);
```

## Handling money

| Approach | When |
|----------|------|
| `BigDecimal` with explicit scale and `RoundingMode` | General purpose, calculations with rates and tax |
| `long` storing the smallest unit (cents) | Simple, fast, exact adding and subtracting |
| `double` | **Never** |

Also decide a currency and rounding policy once, and apply it consistently.

## Random numbers (brief)

```java
new Random(42).nextInt(100);                         // reproducible with a seed
ThreadLocalRandom.current().nextInt(1, 7);           // 1..6 (upper bound exclusive)
Math.random();                                       // double in [0, 1)
```

None of these are secure. For tokens, passwords or keys use `SecureRandom` ([21-security/01_cryptography.md](../21-security/01_cryptography.md)).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `double` for money | Cent errors | `BigDecimal` or `long` cents |
| `a == b` on doubles | Wrong `false` | Compare with a tolerance |
| `new BigDecimal(0.1)` | Long ugly value | `new BigDecimal("0.1")` / `valueOf` |
| `BigDecimal.divide` without scale | `ArithmeticException` | Pass scale and `RoundingMode` |
| `BigDecimal.equals` for numeric comparison | `2.0` is not `2.00` | `compareTo` |
| Silent `int` overflow | Negative or wrong totals | `long`, `*Exact`, or `BigInteger` |
| `Math.abs(Integer.MIN_VALUE)` | Negative result | Handle that edge case |
| `(a + b) / 2` | Overflow on large values | `a + (b - a) / 2` |
| `Math.round` expecting half-up for negatives | `-2.5` → `-2` | Choose an explicit `RoundingMode` |
| `NaN == NaN` | `false` | `Double.isNaN` |
| Using `float` for general work | About 7 digits only | `double` |
| `Math.random()` for security | Predictable | `SecureRandom` |

## Key takeaways

- Integers wrap on overflow; use `long`, the `*Exact` methods, or `BigInteger`
- Binary floating-point cannot store most decimals exactly; never compare with `==`
- Money belongs in `BigDecimal` (built from strings) or integer cents
- `BigDecimal` division needs a scale and rounding mode; compare with `compareTo`
- `Math` provides rounding, powers and exact arithmetic; know `floorMod`, `round` and `*Exact`

**Next:** [Console Input and Output](./09_console-input-output.md)
