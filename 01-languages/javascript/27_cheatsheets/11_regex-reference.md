# Regular Expressions Reference & Recipes Cheatsheet

> Quick-reference guide covering RegExp syntax tokens, quantifiers, assertions, flags, and production-tested patterns for common engineering validation tasks.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Level** | Reference / Quick Lookup |

---

## 1. Character Classes & Quantifiers

| Token | Matches | Quantifier | Matches |
| :--- | :--- | :--- | :--- |
| `.` | Any character except newline (unless `s` flag) | `*` | 0 or more (greedy) |
| `\d` | Digit `[0-9]` | `*?` | 0 or more (lazy) |
| `\D` | Non-digit `[^0-9]` | `+` | 1 or more (greedy) |
| `\w` | Word character `[a-zA-Z0-9_]` | `+?` | 1 or more (lazy) |
| `\W` | Non-word character | `?` | 0 or 1 (optional) |
| `\s` | Whitespace `[\t\r\n\f\v ]` | `{n}` | Exactly $n$ times |
| `\S` | Non-whitespace | `{n,}` | At least $n$ times |
| `[abc]` | Any character in set | `{n,m}` | Between $n$ and $m$ times |
| `[^abc]` | Any character NOT in set | `{n,m}?` | Between $n$ and $m$ (lazy) |

---

## 2. Anchors & Lookaround Assertions

| Syntax | Name | Matches |
| :--- | :--- | :--- |
| `^` | Start of String | Matches start of input (or start of line with `m` flag) |
| `$` | End of String | Matches end of input (or end of line with `m` flag) |
| `\b` | Word Boundary | Boundary between `\w` and `\W` |
| `\B` | Non-Word Boundary | Position inside word or between two non-words |
| `(?=...)` | Positive Lookahead | Matches if followed by pattern (does not consume characters) |
| `(?!...)` | Negative Lookahead | Matches if NOT followed by pattern |
| `(?<=...)` | Positive Lookbehind | Matches if preceded by pattern |
| `(?<!...)` | Negative Lookbehind | Matches if NOT preceded by pattern |

---

## 3. Production Regex Recipes

### Email Validation (RFC 5322 Simplified)
```javascript
const EMAIL_REGEX = /^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)+$/;
```

### UUID v4 Validation
```javascript
const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
```

### Semantic Versioning (SemVer 2.0)
```javascript
const SEMVER_REGEX = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$/;
```

### ISO 8601 UTC Timestamp
```javascript
const ISO_DATE_REGEX = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z$/;
```

### IPv4 Address
```javascript
const IPV4_REGEX = /^(?:(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.){3}(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)$/;
```

### Strong Password (Min 8 chars, 1 uppercase, 1 lowercase, 1 number, 1 special char)
```javascript
const STRONG_PASSWORD_REGEX = /^(?=.*[a-z])(?=.*[A-Z])(?=.*\d)(?=.*[@$!%*?&])[A-Za-z\d@$!%*?&]{8,}$/;
```
