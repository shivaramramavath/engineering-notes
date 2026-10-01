# JSON

**JSON** (JavaScript Object Notation) is a text format for data. The global `JSON` object converts between JSON text and JavaScript values.

```js
JSON.stringify({ a: 1, b: [true, null] });   // '{"a":1,"b":[true,null]}'
JSON.parse('{"a":1}');                        // { a: 1 }
```

## The format

| JSON type | Rules |
|-----------|-------|
| object | `{ "key": value }` with **double-quoted** keys |
| array | `[1, 2, 3]` |
| string | double quotes only, escapes `\n \" \\ \uXXXX` |
| number | no `NaN`, `Infinity`, hex, leading `+`, or trailing dot |
| `true`, `false`, `null` | lowercase |

Not allowed: comments, trailing commas, single quotes, `undefined`, functions, `BigInt`, unquoted keys. For config files with comments use JSON5, JSONC or YAML.

## JSON.parse

```js
JSON.parse("[1, 2]");
JSON.parse('"text"');
JSON.parse("nope");            // SyntaxError
```

Always wrap untrusted input:

```js
function safeParse(text, fallback = null) {
  try { return JSON.parse(text); } catch { return fallback; }
}
```

### Reviver

Transforms values as they are parsed (bottom-up).

```js
const data = JSON.parse('{"when":"2026-09-30T10:15:00Z","n":5}', (key, value) =>
  key === "when" ? new Date(value) : value,
);
data.when instanceof Date;   // true
```

## JSON.stringify

```js
JSON.stringify(value, replacer, space);
```

```js
JSON.stringify({ a: 1 }, null, 2);
/*
{
  "a": 1
}
*/
```

`space`: number (up to 10 spaces) or string (`"\t"`) for pretty printing.

## What stringify does with types

| Value | Result |
|-------|--------|
| `undefined`, function, symbol (as object property) | property **omitted** |
| `undefined`, function, symbol (in array) | `null` |
| `NaN`, `Infinity`, `-Infinity` | `null` |
| `Date` | ISO string (via `toJSON`) |
| `Map`, `Set` | `{}` (data lost) |
| `RegExp`, `Error` | `{}` |
| `BigInt` | throws `TypeError` |
| Circular reference | throws `TypeError` |
| Class instance | own enumerable properties only |
| Top-level `undefined` | `undefined` (not a string) |

```js
JSON.stringify({ a: undefined, b: () => {}, c: NaN, d: new Date(0), e: [undefined] });
// '{"c":null,"d":"1970-01-01T00:00:00.000Z","e":[null]}'
```

## Replacer

Array: whitelist of keys. Function: transform each `(key, value)`.

```js
JSON.stringify(user, ["id", "name"]);                       // only these keys (at every level)
JSON.stringify(user, (key, value) => (key === "password" ? undefined : value));
JSON.stringify(data, (k, v) => (typeof v === "bigint" ? v.toString() : v));
JSON.stringify(obj, (k, v) => (v instanceof Map ? Object.fromEntries(v) : v));
```

## `toJSON`

An object can control its own serialization.

```js
class User {
  constructor(name, password) { this.name = name; this.password = password; }
  toJSON() { return { name: this.name }; }
}
JSON.stringify(new User("Ada", "secret"));   // '{"name":"Ada"}'
```

`Date.prototype.toJSON` is why dates become ISO strings.

## Deep copy caveat

```js
const copy = JSON.parse(JSON.stringify(obj));    // loses Dates, Maps, undefined, etc.
structuredClone(obj);                            // preferred deep clone
```

## Circular references

```js
const a = {}; a.self = a;
JSON.stringify(a);       // TypeError: Converting circular structure

const seen = new WeakSet();
JSON.stringify(a, (k, v) => {
  if (typeof v === "object" && v !== null) {
    if (seen.has(v)) return "[Circular]";
    seen.add(v);
  }
  return v;
});
```

## Big numbers

`JSON.parse` turns numbers beyond 2⁵³ into imprecise floats.

```js
JSON.parse('{"id": 9007199254740993}').id;   // 9007199254740992 (wrong)
```

Send big IDs as **strings** or use a reviver with source access where supported.

## Key order and stability

- Property order follows JS rules: integer-like keys first (ascending), then insertion order
- For canonical output (hashing/signing), sort keys yourself or use a canonical JSON library

## JSON in practice

```js
const res = await fetch("/api/users");
const users = await res.json();                // parses the body

await fetch("/api/users", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ name: "Ada" }),
});

localStorage.setItem("prefs", JSON.stringify(prefs));
const prefs = JSON.parse(localStorage.getItem("prefs") ?? "{}");
```

## Related formats

| Format | Notes |
|--------|-------|
| JSON Lines / NDJSON | one JSON value per line, great for streaming and logs |
| JSON5 / JSONC | comments, trailing commas for config files |
| YAML / TOML | human-friendly config |
| MessagePack / CBOR | compact binary |
| `structuredClone` / `postMessage` | supports more types, not text |

## Security

- `JSON.parse` is safe to run on untrusted text (unlike `eval`), but parsed data is still untrusted: **validate** it (Zod, Ajv, Valibot)
- Objects produced by `JSON.parse` can contain an own key named `__proto__`; merging them unsafely into other objects can cause prototype pollution
- Escape JSON embedded in HTML `<script>` tags (`<` as `\u003c`)

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| JSON round trip as deep clone | Loses types | `structuredClone` |
| Expecting Dates back as Dates | They become strings | Reviver |
| `undefined` values vanish | Missing keys | Use `null` deliberately |
| Serializing `Map`/`Set` directly | `{}` | Convert to arrays/objects |
| Parsing without `try/catch` | Crashes on bad input | Wrap and validate |
| Big integer IDs as numbers | Precision loss | Strings |
| Trusting parsed data | Missing/typed wrongly | Schema validation |
| Comments/trailing commas in JSON | `SyntaxError` | JSONC/JSON5 parsers |
| Pretty-printing in production payloads | Larger responses | Minify |

## Key takeaways

- `JSON.stringify` drops `undefined`/functions/symbols and cannot handle `BigInt` or cycles
- `JSON.parse` needs error handling and validation
- Use `toJSON`, replacers and revivers to control conversion
- Send big IDs as strings; use `structuredClone` for deep copies

**Next:** [Intl](./09_intl.md)
