# Number and BigInt

## Number

All `number` values are **64-bit IEEE 754 floating-point** (double precision). There is no separate integer type.

```js
42; 3.14; 1e6; 0xff; 0b1010; 0o17; 1_000_000;
```

## Special values and constants

| Value | Meaning |
|-------|---------|
| `NaN` | "not a number" (`typeof NaN === "number"`), never equals itself |
| `Infinity`, `-Infinity` | overflow, division by zero |
| `-0` | negative zero (`Object.is(0, -0)` is `false`) |
| `Number.MAX_SAFE_INTEGER` | `9007199254740991` (2⁵³ − 1) |
| `Number.MIN_SAFE_INTEGER` | `-9007199254740991` |
| `Number.MAX_VALUE` / `MIN_VALUE` | largest / smallest positive |
| `Number.EPSILON` | smallest difference between 1 and the next float |

## The precision problem

```js
0.1 + 0.2;                                  // 0.30000000000000004
0.1 + 0.2 === 0.3;                          // false
Math.abs(0.1 + 0.2 - 0.3) < Number.EPSILON; // true (safe comparison)

9007199254740992 === 9007199254740993;      // true (beyond safe range)
```

Money: use **integers in the smallest unit** (cents), or a decimal library, or `Intl` for display only.

## Checking

```js
Number.isNaN(x);            // true only for NaN (global isNaN coerces, avoid)
Number.isFinite(x);         // no strings coerced
Number.isInteger(5.0);      // true
Number.isSafeInteger(2 ** 53);   // false
```

## Parsing and conversion

| Expression | Result |
|------------|--------|
| `Number("42")` | `42` |
| `Number("  42  ")` | `42` |
| `Number("")` | `0` |
| `Number("42px")` | `NaN` |
| `parseInt("42px", 10)` | `42` |
| `parseFloat("3.5kg")` | `3.5` |
| `+"42"` | `42` |
| `Number(null)` / `Number(undefined)` | `0` / `NaN` |

Always pass a radix to `parseInt`. Prefer `Number()` when the whole string must be numeric.

## Formatting

```js
(1234.5678).toFixed(2);          // "1234.57"  (returns a STRING)
(0.000001234).toPrecision(2);    // "0.0000012"
(255).toString(2);               // "11111111"
(1e21).toString();               // "1e+21"
(1234567.891).toLocaleString("en-US");   // "1,234,567.891"
```

`toFixed` rounding has float quirks: `(1.005).toFixed(2)` gives `"1.00"`.

## Rounding safely

```js
const round = (n, d = 0) => Number(Math.round(Number(n + "e" + d)) + "e-" + d);
round(1.005, 2);   // 1.01
```

## Arithmetic notes

```js
5 / 2;          // 2.5 (no integer division)
Math.trunc(5 / 2);   // 2
-7 % 3;         // -1 (sign follows the dividend)
((-7 % 3) + 3) % 3;  // 2 (true modulo)
1 / 0;          // Infinity
0 / 0;          // NaN
```

Bitwise operators convert to **32-bit integers**: `2 ** 32 | 0` gives `0`.

## BigInt

Arbitrary-precision integers (ES2020).

```js
const big = 9007199254740993n;
const also = BigInt("123456789012345678901234567890");
BigInt(42);                     // 42n
typeof big;                     // "bigint"

big + 1n;                       // 9007199254740994n
2n ** 100n;                     // huge exact value
7n / 2n;                        // 3n (truncates toward zero)
-7n % 3n;                       // -1n
```

## BigInt rules

| Rule | Example |
|------|---------|
| No mixing with `number` | `1n + 1` throws `TypeError` |
| Explicit conversion | `Number(5n)`, `BigInt(5)` |
| Comparison across types works | `1n < 2` true, `1n == 1` true, `1n === 1` false |
| No `Math` functions | `Math.max(1n)` throws |
| No unary `+` | `+1n` throws |
| `JSON.stringify` throws | provide `toJSON` or convert to string |
| Cannot use decimals | `1.5n` is a syntax error |

```js
BigInt.prototype.toJSON = function () { return this.toString(); };   // caution: global patch
JSON.stringify({ id: 1n }, (k, v) => (typeof v === "bigint" ? v.toString() : v));
```

Wrap to fixed width:

```js
BigInt.asUintN(64, -1n);   // 18446744073709551615n
BigInt.asIntN(8, 255n);    // -1n
```

## When to use BigInt

| Use | Example |
|-----|---------|
| IDs beyond 2⁵³ | Twitter snowflake IDs, database bigints |
| Exact large arithmetic | factorials, cryptography |
| Timestamps in nanoseconds | `process.hrtime.bigint()` |
| Not for | money with decimals, general math |

Parse large IDs from JSON as strings; `JSON.parse` turns numbers above 2⁵³ into imprecise floats.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Comparing floats with `===` | Rounding errors | Epsilon or integers |
| `toFixed` returning a string | Concatenation surprises | `Number(x.toFixed(2))` |
| `parseInt` without radix | Legacy quirks | `parseInt(s, 10)` |
| Global `isNaN("abc")` is `true` | Coerces | `Number.isNaN` |
| Integers above 2⁵³ | Silent precision loss | `BigInt` or strings |
| Bitwise ops on large numbers | 32-bit truncation | `BigInt` or `Math.floor` |
| Mixing BigInt and Number | `TypeError` | Convert explicitly |
| Storing money as floats | Cent errors | Integer cents |

## Key takeaways

- Numbers are doubles: mind precision, `NaN`, `-0`, and safe-integer limits
- Use `Number.isNaN`, `Number.isInteger`, `Number.EPSILON`
- BigInt is for exact big integers and cannot mix with numbers
- Keep money in integers and format with `Intl`

**Next:** [Math](./03_math.md)
