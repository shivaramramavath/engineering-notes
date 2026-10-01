# RegExp

Regular expressions describe text patterns. In JavaScript they are `RegExp` objects with literal syntax `/pattern/flags`.

```js
const re = /\d{4}-\d{2}-\d{2}/;
re.test("Due 2026-09-30");     // true
```

## Creating

```js
/ab+c/i;                          // literal (compiled at parse time)
new RegExp("ab+c", "i");          // dynamic pattern
new RegExp(String.raw`\d+\.\d+`); // fewer backslashes
```

Escape user input before putting it in a pattern:

```js
const escape = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
// RegExp.escape(s) is available in newer runtimes (check support)
```

## Flags

| Flag | Meaning |
|------|---------|
| `g` | global: find all matches |
| `i` | case-insensitive |
| `m` | `^` and `$` match at each line |
| `s` | dotAll: `.` matches newlines |
| `u` | Unicode mode (code points, `\p{...}`) |
| `v` | Unicode sets: set operations in classes (superset of `u`) |
| `y` | sticky: match only at `lastIndex` |
| `d` | produce match indices (`match.indices`) |

## Syntax cheat sheet

| Token | Meaning |
|-------|---------|
| `.` | any char except newline (unless `s`) |
| `\d \D` | digit / non-digit |
| `\w \W` | word char `[A-Za-z0-9_]` / not |
| `\s \S` | whitespace / not |
| `\b \B` | word boundary / not |
| `^ $` | start / end |
| `[abc] [^abc] [a-z]` | character class |
| `a|b` | alternation |
| `* + ? {n} {n,} {n,m}` | quantifiers (greedy) |
| `*? +? ??` | lazy quantifiers |
| `(...)` | capture group |
| `(?:...)` | non-capturing group |
| `(?<name>...)` | named group |
| `\1` `\k<name>` | backreference |
| `(?=...)` `(?!...)` | lookahead positive / negative |
| `(?<=...)` `(?<!...)` | lookbehind positive / negative |
| `\p{L}` `\p{Emoji}` | Unicode property (needs `u`/`v`) |

## Methods

| Method | Returns |
|--------|---------|
| `re.test(str)` | boolean |
| `re.exec(str)` | match array or `null` (stateful with `g`/`y`) |
| `str.match(re)` | first match (no `g`) or all matches (`g`), or `null` |
| `str.matchAll(re)` | iterator of full match objects (requires `g`) |
| `str.replace(re, repl)` / `replaceAll` | new string |
| `str.search(re)` | index or `-1` |
| `str.split(re)` | array |

```js
"2026-09-30".match(/(\d+)-(\d+)-(\d+)/);
// ["2026-09-30", "2026", "09", "30", index: 0, input: "...", groups: undefined]

for (const m of "a1b22c333".matchAll(/\d+/g)) console.log(m[0], m.index);
```

## Named groups

```js
const { groups: { year, month, day } } = /(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/.exec("2026-09-30");

"2026-09-30".replace(/(?<y>\d{4})-(?<m>\d{2})-(?<d>\d{2})/, "$<d>/$<m>/$<y>");   // "30/09/2026"
```

## Replace patterns

```js
"john smith".replace(/(\w+) (\w+)/, "$2 $1");          // "smith john"
"a-b".replace(/-/, "$&$&");                            // "a--b"  ($& whole match)
"abc".replace(/b/, (match, offset, whole) => match.toUpperCase());
"price 10".replace(/\d+/, (n) => n * 2);               // "price 20"
```

## The `g` flag and `lastIndex` trap

```js
const re = /a/g;
re.test("a");   // true
re.test("a");   // false! lastIndex moved past the match
re.lastIndex;   // 0 after failure
```

Regex objects with `g` or `y` are **stateful** when used with `test`/`exec`. Create a new regex per use, reset `lastIndex = 0`, or use `str.match`/`matchAll`.

## Lookarounds

```js
"$10 €20".match(/(?<=\$)\d+/);          // ["10"] preceded by $
"foo.js foo.ts".match(/\w+(?=\.js)/g);  // ["foo"] followed by .js
/^(?=.*\d)(?=.*[a-z]).{8,}$/;           // password rule: has digit and lowercase, 8+
```

## Unicode

```js
/\p{L}+/u.exec("héllo wörld")[0];       // letters in any script
/\p{Script=Greek}/u.test("α");
/^\p{Emoji}$/u.test("😀");
/./u.exec("😀")[0];                      // "😀" (whole code point; without u it is half)
/[\p{L}--[a-z]]/v;                       // v flag: set subtraction
```

## Greedy vs lazy

```js
"<a><b>".match(/<.+>/)[0];     // "<a><b>"   greedy
"<a><b>".match(/<.+?>/)[0];    // "<a>"      lazy
```

## Common patterns

```js
const digits = /^\d+$/;
const hex = /^#?([0-9a-f]{3}|[0-9a-f]{6})$/i;
const slug = /^[a-z0-9]+(?:-[a-z0-9]+)*$/;
const isoDate = /^\d{4}-\d{2}-\d{2}$/;
const words = /\b\w+\b/g;
const trimLines = /^\s+|\s+$/gm;
```

Emails, URLs and HTML are **not** well suited to regex: use `new URL()`, a parser, or the browser's `<input type="email">` validation plus server checks.

## Performance and ReDoS

Nested quantifiers can cause **catastrophic backtracking**, letting a short malicious string freeze the process.

```js
/^(a+)+$/.test("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa!");   // exponential time
```

Defenses:

- Avoid nested quantifiers on overlapping patterns (`(a+)+`, `(.*)*`)
- Anchor patterns and limit input length
- Prefer specific classes over `.*`
- Use a linear-time engine or timeouts for untrusted input
- Test patterns with tools like `safe-regex` / `recheck`

## Debugging tools

Regex101 (choose the JavaScript flavor), the DevTools console, and named groups make patterns easier to read. Use `x`-style comments by building from parts:

```js
const year = String.raw`(?<year>\d{4})`;
const month = String.raw`(?<month>0[1-9]|1[0-2])`;
const date = new RegExp(`^${year}-${month}$`);
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Reusing a `g` regex with `test`/`exec` | `lastIndex` state | New regex, reset, or `match` |
| Forgetting to escape user input | Injection and wrong matches | Escape helper |
| Greedy `.*` | Over-matches | Lazy `.*?` or specific classes |
| Parsing HTML/emails/URLs with regex | Fragile | Parser, `URL` |
| No `u` flag with emoji/Unicode | Broken code points | `u` or `v` |
| Catastrophic backtracking | ReDoS | Simple, anchored patterns |
| Assuming `\w` matches accents | ASCII only | `\p{L}` with `u` |
| `str.replace("a", "b")` expecting all | Replaces first | `replaceAll` / `/g` |
| Missing `s` flag when text has newlines | `.` stops at `\n` | `s` flag |

## Key takeaways

- Learn flags, classes, groups, quantifiers and lookarounds
- `g`/`y` regexes are stateful: watch `lastIndex`
- Use named groups and `matchAll` for readable extraction
- Guard against ReDoS and use `u`/`v` for Unicode text

**Next:** [Map and Set](./06_map-and-set.md)
