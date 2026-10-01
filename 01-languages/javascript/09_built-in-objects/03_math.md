# Math

`Math` is a namespace object (not a constructor) with constants and functions for numbers. It works on `number`, **not** `BigInt`.

## Constants

```js
Math.PI;       // 3.141592653589793
Math.E;        // 2.718281828459045
Math.LN2; Math.LN10; Math.LOG2E; Math.LOG10E; Math.SQRT2; Math.SQRT1_2;
```

## Rounding

| Function | Behavior | `-2.5` | `2.5` | `2.4` |
|----------|----------|--------|-------|-------|
| `Math.round` | nearest, ties toward +∞ | `-2` | `3` | `2` |
| `Math.floor` | toward −∞ | `-3` | `2` | `2` |
| `Math.ceil` | toward +∞ | `-2` | `3` | `3` |
| `Math.trunc` | drop fraction | `-2` | `2` | `2` |

```js
Math.round(-2.5);              // -2 (not -3)
Math.sign(-3);                 // -1, 0, 1 (or NaN)
Math.fround(5.5);              // nearest 32-bit float
```

Decimals:

```js
const roundTo = (n, d) => Math.round(n * 10 ** d) / 10 ** d;   // fine for display, watch float quirks
```

## Min, max, abs, clamp

```js
Math.min(1, 2, 3);  Math.max(...list);
Math.min();            // Infinity
Math.max();            // -Infinity
Math.abs(-5);
const clamp = (n, lo, hi) => Math.min(Math.max(n, lo), hi);
```

## Powers and roots

```js
Math.pow(2, 10);  2 ** 10;
Math.sqrt(9);         // 3
Math.cbrt(27);        // 3
Math.hypot(3, 4);     // 5 (distance)
Math.exp(1);          // e
Math.expm1(x);        // e^x - 1, precise for tiny x
```

## Logarithms

```js
Math.log(Math.E);   // 1 (natural)
Math.log2(8);       // 3
Math.log10(1000);   // 3
Math.log1p(x);      // ln(1 + x), precise for tiny x
```

## Trigonometry (radians)

```js
Math.sin(Math.PI / 2);  Math.cos(0);  Math.tan(x);
Math.asin(1); Math.acos(1); Math.atan(1);
Math.atan2(y, x);       // angle of point (x, y), handles quadrants
Math.sinh; Math.cosh; Math.tanh; Math.asinh; Math.acosh; Math.atanh;

const toRad = (deg) => (deg * Math.PI) / 180;
const toDeg = (rad) => (rad * 180) / Math.PI;
```

## Random

`Math.random()` returns a float in `[0, 1)`. It is **not cryptographically secure**.

```js
const randInt = (min, max) => Math.floor(Math.random() * (max - min + 1)) + min;   // inclusive
const pick = (list) => list[Math.floor(Math.random() * list.length)];

function shuffle(a) {
  const r = [...a];
  for (let i = r.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [r[i], r[j]] = [r[j], r[i]];
  }
  return r;
}
```

Secure randomness:

```js
crypto.getRandomValues(new Uint32Array(1))[0];
crypto.randomUUID();               // "3b241101-e2bb-4255-8caf-4136c566a962"
```

Use `crypto` for tokens, IDs, passwords, and anything security related.

## Low-level helpers

```js
Math.imul(a, b);       // 32-bit integer multiply
Math.clz32(1);         // 31 (leading zero bits)
```

## Integer utilities

```js
Number.isInteger(x);  Number.isSafeInteger(x);
Math.trunc(x) === x;
const isPowerOfTwo = (n) => n > 0 && (n & (n - 1)) === 0;   // 32-bit range
const gcd = (a, b) => (b === 0 ? a : gcd(b, a % b));
const lcm = (a, b) => (a / gcd(a, b)) * b;
```

## Statistics helpers

```js
const sum = (xs) => xs.reduce((a, b) => a + b, 0);
const mean = (xs) => sum(xs) / xs.length;
const median = (xs) => { const s = xs.toSorted((a, b) => a - b), m = s.length >> 1; return s.length % 2 ? s[m] : (s[m - 1] + s[m]) / 2; };
```

## Edge cases

| Expression | Result |
|------------|--------|
| `Math.max(NaN, 1)` | `NaN` |
| `Math.max("3", 2)` | `3` (coerces) |
| `Math.max([])` | `0` (empty array coerces to 0) |
| `Math.sqrt(-1)` | `NaN` |
| `Math.round(0.49999999999999994)` | `0` |
| `Math.floor(-0.5)` | `-1` |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `Math.random()` for security | Predictable | `crypto.getRandomValues` |
| `Math.max(...hugeArray)` | Call stack limit | `reduce` |
| Assuming `round(-2.5)` is `-3` | Ties go toward +∞ | Know the rule, or custom rounding |
| Degrees passed to trig functions | Wrong results | Convert to radians |
| Off-by-one in random ranges | Missing endpoint | `floor(random * (max - min + 1)) + min` |
| Using `Math` with BigInt | `TypeError` | Convert or write BigInt versions |
| Float rounding for money | Incorrect cents | Integer cents |

## Key takeaways

- `Math` provides constants and pure numeric functions
- Know `round` / `floor` / `ceil` / `trunc` differences, especially for negatives
- `Math.random()` is not for security: use `crypto`
- Trigonometry uses radians

**Next:** [Date and Temporal](./04_date-and-temporal.md)
