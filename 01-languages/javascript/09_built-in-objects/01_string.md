# String

Strings are **immutable** sequences of UTF-16 code units. Every "modifying" method returns a **new** string.

```js
const s = "Hello";
s[0] = "J";              // ignored
s.toUpperCase();         // "HELLO" (new string)
```

## Creating

```js
"double"; 'single'; `template ${1 + 1}`;
String(123);             // "123"
new String("x");         // wrapper OBJECT (avoid)
"abc".repeat(2);         // "abcabc"
```

## Length and Unicode

`length` counts **UTF-16 code units**, not characters.

```js
"abc".length;            // 3
"😀".length;             // 2 (surrogate pair)
[..."😀"].length;        // 1 (iterates by code point)
"é".length;              // 1 or 2 depending on composed vs decomposed form
"é".normalize("NFC") === "e\u0301".normalize("NFC");   // true
```

For user-visible characters (grapheme clusters, emoji sequences) use `Intl.Segmenter`.

```js
[...new Intl.Segmenter().segment("👨‍👩‍👧")].length;   // 1
```

## Accessing characters

```js
s.at(-1);                // last character
s.charAt(1);             // "" if out of range
s[10];                   // undefined
s.charCodeAt(0);         // UTF-16 unit
s.codePointAt(0);        // full code point
String.fromCodePoint(128512);   // "😀"
```

## Searching

| Method | Returns |
|--------|---------|
| `includes(x)` | boolean |
| `startsWith(x)` / `endsWith(x)` | boolean |
| `indexOf(x)` / `lastIndexOf(x)` | index or `-1` |
| `search(regex)` | index or `-1` |
| `match(regex)` | matches or `null` |
| `matchAll(regex)` | iterator of all matches (needs `g`) |

## Extracting

| Method | Notes |
|--------|-------|
| `slice(start, end)` | supports negative indexes (preferred) |
| `substring(start, end)` | swaps args if needed, no negatives |
| `substr` | deprecated |
| `split(sep, limit)` | returns an array |

```js
"abcdef".slice(1, 3);    // "bc"
"abcdef".slice(-2);      // "ef"
"a,b,c".split(",");      // ["a", "b", "c"]
"abc".split("");         // ["a", "b", "c"] (UTF-16 units; spread is safer)
```

## Transforming

```js
"  hi  ".trim();  ".trimStart();  .trimEnd();
"abc".toUpperCase();  "ABC".toLowerCase();
"i".toLocaleUpperCase("tr");      // locale-aware ("İ")
"5".padStart(3, "0");             // "005"
"5".padEnd(3, "-");               // "5--"
"a-b-c".replace("-", "+");        // "a+b-c" (first only)
"a-b-c".replaceAll("-", "+");     // "a+b+c"
"John Smith".replace(/(\w+) (\w+)/, "$2, $1");    // "Smith, John"
"abc".replace(/b/, (m) => m.toUpperCase());       // function replacer
```

## Comparing

```js
"a" < "b";                        // true (UTF-16 order)
"a" < "B";                        // false ("B" comes first)
"a".localeCompare("B");           // -1 (locale aware)
["ä", "a", "z"].sort((x, y) => x.localeCompare(y, "de"));
"a".localeCompare("A", undefined, { sensitivity: "base" });   // 0
```

## Conversion

```js
String(null);  String(undefined);  String([1, 2]);  String({});
(255).toString(16);               // "ff"
parseInt("ff", 16);               // 255
```

## Well-formed strings (ES2024)

```js
"ab\uD800".isWellFormed();        // false (lone surrogate)
"ab\uD800".toWellFormed();        // "ab\uFFFD"
```

## Escapes and raw strings

```js
"tab\there";  "line\nbreak";  "\u00e9";  "\u{1F600}";
String.raw`C:\new`;               // "C:\new"
```

## Common recipes

```js
const capitalize = (s) => s.charAt(0).toUpperCase() + s.slice(1);
const truncate = (s, n) => (s.length > n ? s.slice(0, n - 1) + "…" : s);
const slug = (s) => s.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase().replace(/\W+/g, "-");
const reverse = (s) => [...s].reverse().join("");
const isPalindrome = (s) => { const t = s.toLowerCase().replace(/\W/g, ""); return t === [...t].reverse().join(""); };
const count = (s, sub) => s.split(sub).length - 1;
```

## Performance notes

- Repeated `+=` in loops is fine in modern engines, but for huge builds collect pieces in an array and `join`
- Strings are compared by value, so `===` works, but comparing long strings is O(n)
- `split("")`, `.length` and indexing work on UTF-16 units, not visible characters

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming `length` equals character count | Emoji and combined marks | `[...s]`, `Intl.Segmenter` |
| Comparing with `<` for user-facing sorting | Wrong locale order | `localeCompare`, `Intl.Collator` |
| `replace` with a string replaces once | Missing replacements | `replaceAll` or `/g` |
| `new String("x")` | Object, truthy, `===` fails | Use primitives |
| Trying to mutate (`s[0] = "x"`) | Silently ignored | Build a new string |
| `substr` | Deprecated | `slice` |
| Unnormalized comparisons | `"é"` may not equal `"é"` | `normalize()` first |
| Building HTML from user strings | XSS | Escape or use DOM APIs |

## Key takeaways

- Strings are immutable UTF-16 sequences; `length` counts code units
- Prefer `slice`, `at`, `includes`, `replaceAll`, `padStart`
- Use `localeCompare`/`Intl` for human-facing comparison and `normalize` for equality
- Use `Intl.Segmenter` when you need real "characters"

**Next:** [Number and BigInt](./02_number-and-bigint.md)
