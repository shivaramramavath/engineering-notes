# Type Conversion and Equality

JavaScript converts values between types **explicitly** (you ask) and **implicitly** (the engine decides). Understanding both removes most "WAT" moments.

## Explicit conversion

```js
String(123); // "123"
Number("42"); // 42
Boolean("hi"); // true
parseInt("42px"); // 42
parseFloat("3.5kg"); // 3.5
BigInt("10"); // 10n
```

## Number conversion table

| Value            | `Number(value)` |
| ---------------- | --------------- |
| `"42"`, `" 42 "` | `42`            |
| `""`, `"   "`    | `0`             |
| `"42px"`         | `NaN`           |
| `true` / `false` | `1` / `0`       |
| `null`           | `0`             |
| `undefined`      | `NaN`           |
| `[]`             | `0`             |
| `[5]`            | `5`             |
| `[1, 2]`         | `NaN`           |
| `{}`             | `NaN`           |

`parseInt` reads until it fails, `Number` rejects the whole string:

```js
parseInt("12abc"); // 12
Number("12abc"); // NaN
parseInt("08", 10); // always pass the radix
```

## Truthy and falsy

Exactly **8 falsy values**. Everything else is truthy.

| Falsy           |
| --------------- |
| `false`         |
| `0`, `-0`, `0n` |
| `""`            |
| `null`          |
| `undefined`     |
| `NaN`           |

```js
Boolean([]); // true   (empty array is truthy)
Boolean({}); // true
Boolean("0"); // true
Boolean("false"); // true
```

Use `!!value` or `Boolean(value)` to force a boolean.

## Implicit coercion

```js
"5" + 1; // "51"   (+ with a string concatenates)
"5" - 1; // 4      (- converts to numbers)
"5" * "2"; // 10
true + 1; // 2
[] + []; // ""
[] + {}; // "[object Object]"
null + 1; // 1
undefined + 1; // NaN
```

Rules of thumb:

1. `+` with any string operand: **string concatenation**
2. `- * / % **`: **numeric** conversion
3. `if`, `!`, `&&`, `||`, `? :`: **boolean** conversion
4. Objects convert through `Symbol.toPrimitive`, then `valueOf`, then `toString`

```js
const price = {
  valueOf() {
    return 10;
  },
  toString() {
    return "ten";
  },
};
price + 5; // 15
`${price}`; // "ten"
```

## Equality: four comparisons

| Operator                   | Name       | Type conversion | `NaN` equals `NaN` | `0` equals `-0` |
| -------------------------- | ---------- | --------------- | ------------------ | --------------- |
| `==`                       | Loose      | Yes             | No                 | Yes             |
| `===`                      | Strict     | No              | No                 | Yes             |
| `Object.is`                | Same-value | No              | **Yes**            | **No**          |
| `includes` (SameValueZero) |            | No              | **Yes**            | Yes             |

```js
0 == ""; // true
0 == "0"; // true
"" == "0"; // false  (not transitive!)
null == undefined; // true
null == 0; // false
NaN == NaN; // false

1 === "1"; // false
Object.is(NaN, NaN); // true
Object.is(0, -0); // false
[NaN].includes(NaN); // true
[NaN].indexOf(NaN); // -1
```

## The one useful `==`

```js
value == null; // true for both null and undefined
```

Some teams allow exactly this and forbid all other `==`. Otherwise use `===`.

## Comparing with `<` and `>`

```js
"10" < "9"; // true   (both strings: compared alphabetically)
"10" < 9; // false  (one number: numeric)
null >= 0; // true   (>= converts null to 0)
null > 0; // false
null == 0; // false  (== has its own rules)
undefined > 0; // false  (NaN)
```

## Conversion pitfalls

| Pitfall                                 | Why it hurts               | Better                           |
| --------------------------------------- | -------------------------- | -------------------------------- |
| `==` between different types            | Surprising, non-transitive | `===`                            |
| `if (arr)` to test for content          | Empty arrays are truthy    | `arr.length > 0`                 |
| `if (value)` when `0` or `""` are valid | Falsy check drops them     | `value != null` or `??`          |
| `parseInt` without radix                | Odd legacy behavior        | `parseInt(s, 10)` or `Number(s)` |
| Comparing strings that hold numbers     | Alphabetical order         | Convert first                    |

## Key takeaways

- Learn the 8 falsy values; everything else is truthy
- `+` prefers strings, other math operators prefer numbers
- Default to `===`; `Object.is` for `NaN` and `-0` edge cases
- Convert explicitly at the boundaries (input, JSON, DOM values)

**Next:** [Operators](./04_operators.md)
