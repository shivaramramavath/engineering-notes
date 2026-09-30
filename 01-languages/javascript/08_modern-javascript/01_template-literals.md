# Template Literals

Template literals (ES2015) are strings written with **backticks**. They support embedded expressions, multi-line text, and custom processing through **tagged templates**.

```js
const name = "Ada";
const msg = `Hello, ${name}!`;       // "Hello, Ada!"
```

## Interpolation

`${ ... }` accepts **any expression**, not just variables.

```js
const a = 2, b = 3;
`${a} + ${b} = ${a + b}`;                    // "2 + 3 = 5"
`Status: ${ok ? "on" : "off"}`;              // ternary
`Total: ${items.map((i) => i.price).join(", ")}`;
`Name: ${user?.name ?? "Guest"}`;
`Nested: ${`inner ${a}`}`;                   // templates can nest
```

The result of the expression is converted with `String(value)` (via `toString`). Objects become `[object Object]` and arrays join with commas:

```js
`${[1, 2, 3]}`;         // "1,2,3"
`${{}}`;                // "[object Object]"
`${null} ${undefined}`; // "null undefined"
`${Symbol("x")}`;       // TypeError (symbols cannot convert implicitly)
```

## Multi-line strings

Line breaks in the source become line breaks in the string.

```js
const html = `
  <ul>
    <li>One</li>
    <li>Two</li>
  </ul>
`;
```

The leading newline and indentation are part of the string. Trim or dedent when needed:

```js
const text = `
  Hello
  World
`.trim();
```

## Escaping

```js
`Price: \$5`;           // escape a dollar sign before {
`Use \`backticks\``;    // escape a backtick
`Line1\nLine2`;         // \n, \t, \\ work as in normal strings
`$5 costs ${5}`;        // "$" alone is fine: "$5 costs 5"
```

## Template literals vs concatenation

| | Concatenation | Template literal |
|---|---------------|------------------|
| Readability | `"Hi " + name + ", you are " + age` | `` `Hi ${name}, you are ${age}` `` |
| Multi-line | needs `\n` or `+` | native |
| Expressions | wrap in parentheses | inside `${}` |
| Number formatting | manual | use `toFixed`, `Intl` inside |

```js
`Total: ${total.toFixed(2)}`;
`Date: ${new Intl.DateTimeFormat("en-GB").format(date)}`;
```

## Tagged templates

A **tag** is a function placed before a template literal. It receives the string pieces and the interpolated values.

```js
function tag(strings, ...values) {
  console.log(strings);   // ["Hello ", ", you are ", "!"]   (strings.raw also available)
  console.log(values);    // ["Ada", 36]
  return "result";
}

const name = "Ada", age = 36;
tag`Hello ${name}, you are ${age}!`;
```

Rules:

- `strings.length === values.length + 1`
- `strings.raw` holds the unprocessed source text (backslashes kept)
- The tag can return **anything**, not only a string

## Example: highlight interpolations

```js
function highlight(strings, ...values) {
  return strings.reduce(
    (out, str, i) => out + str + (i < values.length ? `<mark>${values[i]}</mark>` : ""),
    "",
  );
}
highlight`User ${"Ada"} scored ${99}`;   // "User <mark>Ada</mark> scored <mark>99</mark>"
```

## Example: safe HTML escaping

```js
const escapeHtml = (s) =>
  String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

function html(strings, ...values) {
  return strings.reduce((out, str, i) => out + str + (i < values.length ? escapeHtml(values[i]) : ""), "");
}

const userInput = "<script>alert(1)</script>";
html`<p>${userInput}</p>`;   // "<p>&lt;script&gt;alert(1)&lt;/script&gt;</p>"
```

## Example: parameterized queries

```js
function sql(strings, ...values) {
  return { text: strings.reduce((q, s, i) => q + s + (i < values.length ? `$${i + 1}` : ""), ""), values };
}
const id = 5;
sql`SELECT * FROM users WHERE id = ${id}`;
// { text: "SELECT * FROM users WHERE id = $1", values: [5] }
```

Values never get concatenated into the SQL text. Libraries such as `postgres` and Prisma use this pattern.

## `String.raw`

Returns the raw text without processing escape sequences.

```js
String.raw`C:\new\table`;         // "C:\new\table" (no newline/tab)
`C:\new\table`;                   // "C:" + newline + "ew" + tab + "able"

const re = new RegExp(String.raw`\d+\.\d+`);   // fewer double backslashes
```

## Real-world tagged template users

| Library | Use |
|---------|-----|
| styled-components / emotion | CSS in JS |
| lit-html / Lit | HTML templates |
| graphql-tag (`gql`) | parse GraphQL |
| sql-template-strings, postgres.js | safe SQL |
| Google zx | shell commands (`$`) |

## `Intl` inside templates

```js
const money = new Intl.NumberFormat("en-US", { style: "currency", currency: "USD" });
`Total: ${money.format(1234.5)}`;    // "Total: $1,234.50"
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Interpolating `undefined`/`null` accidentally | Prints "undefined" | `??` defaults |
| Building HTML/SQL with plain templates | Injection | Tagged escape/parameterize |
| Interpolating objects | `[object Object]` | `JSON.stringify` or specific fields |
| Unexpected leading whitespace in multi-line strings | Indentation becomes content | `trim()` / dedent helper |
| Complex logic inside `${}` | Unreadable | Compute before the template |
| Symbols in templates | `TypeError` | `String(symbol)` or `.description` |
| Using backticks in JSON | Invalid JSON | JSON needs double quotes |

## Key takeaways

- Backticks give interpolation of any expression and multi-line strings
- Tagged templates receive `strings` and `values` and can return anything
- Use tags for escaping, SQL parameters, styling and DSLs
- `String.raw` preserves backslashes

**Next:** [Enhanced Object Literals](./02_enhanced-object-literals.md)
