# Regex & Text Filters

Pattern-matching a string field, and MongoDB's built-in full-text search — two different tools for two different kinds of "find text like this" queries.

## Regex matching

```js
User.find({ name: /^Alice/ }); // a native JavaScript regex literal
User.find({ name: { $regex: "^Alice" } }); // equivalent, using the $regex operator explicitly
```

Both find documents where `name` starts with `"Alice"` — a JavaScript regex literal works directly as a filter value; `$regex` is the explicit operator form, needed when combining with other regex-specific options.

### Case-insensitive matching

```js
User.find({ name: /^alice/i }); // "i" flag on the literal
User.find({ name: { $regex: "^alice", $options: "i" } }); // $options for the explicit form
```

### Partial/contains matching

```js
User.find({ email: /gmail\.com$/ }); // ends with "gmail.com"
User.find({ name: /ali/i }); // contains "ali", case-insensitive, anywhere in the string
```

---

## Performance: regex and indexes

```js
User.find({ name: /^Alice/ }); // ✅ CAN use an index on "name" efficiently — anchored at the start
User.find({ name: /Alice$/ }); // ❌ CANNOT use an index efficiently — anchored only at the end
User.find({ name: /Alice/ }); // ❌ CANNOT use an index efficiently — unanchored, matches anywhere
```

This is a genuinely important, easy-to-miss performance detail: a regex **anchored at the start** (`^`) can use an index the same way a plain equality/range query would, scanning only the relevant portion of the index. A regex that's unanchored, or anchored only at the end, forces MongoDB to scan every document's value directly, since an index (a sorted structure) can't help find "contains this substring anywhere." For anything beyond a simple "starts with" prefix search on an indexed field, full-text search (below) is usually both faster and more appropriate.

---

## `$text` — full-text search

```js
articleSchema.index({ title: "text", body: "text" }); // required — from 04-schemas/07-indexes-in-schemas.md
```

```js
Article.find({ $text: { $search: "mongoose tutorial" } });
```

Unlike regex, `$text` performs genuine text search: tokenizing into words, ignoring common "stop words" (like "the"/"a"/"is"), and supporting relevance scoring — much closer to what a real search feature needs than a regex ever provides, and it **can** use its text index efficiently, unlike an unanchored regex.

### Sorting by relevance

```js
Article.find(
  { $text: { $search: "mongoose tutorial" } },
  { score: { $meta: "textScore" } },
).sort({ score: { $meta: "textScore" } });
```

`$meta: "textScore"` retrieves MongoDB's computed relevance score for each match, letting you show the most relevant results first — something a plain regex query has no concept of at all.

### Phrase search

```js
Article.find({ $text: { $search: '"exact phrase"' } });
```

Wrapping a term in escaped double quotes within the search string requires that exact phrase, rather than matching documents containing the words separately anywhere.

### Excluding terms

```js
Article.find({ $text: { $search: "mongoose -deprecated" } });
```

A `-` prefix excludes documents containing that term.

---

## Regex vs `$text`: when to use which

|                           | Regex                                                       | `$text`                                                 |
| ------------------------- | ----------------------------------------------------------- | ------------------------------------------------------- |
| Needs a special index?    | No (but anchored prefix regex benefits from a normal index) | Yes — a text index                                      |
| Good for                  | Prefix matching (`^`), exact pattern validation             | Genuine keyword/phrase search across larger text fields |
| Relevance ranking         | No                                                          | Yes, via `$meta: "textScore"`                           |
| Performance on large text | Poor for unanchored patterns                                | Designed for this                                       |
| Word-boundary awareness   | No — purely character-pattern based                         | Yes — tokenizes into actual words                       |

A rough rule: reach for a regex for short, structured strings and prefix matching (like an autocomplete-style "starts with" search on a username); reach for `$text` for actual free-text search across longer content (article bodies, product descriptions).

---

## Escaping user input in a regex — a security consideration

```js
// ❌ dangerous if `userInput` comes directly from a client
User.find({ name: new RegExp(userInput) });
```

If `userInput` contains regex special characters (or is deliberately crafted to be a pathologically slow pattern — "ReDoS," ​a form of denial-of-service via a maliciously complex regex), building a regex directly from unsanitized user input is a real risk. Escape special characters first:

```js
function escapeRegex(str) {
  return str.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

User.find({ name: new RegExp(`^${escapeRegex(userInput)}`, "i") });
```

## Common mistakes

- **Using an unanchored regex on a large, indexed collection expecting index-speed performance** — only prefix-anchored (`^`) regexes can actually use an index efficiently.
- **Reaching for regex when `$text` would be the better tool** — for real free-text search needs (multiple words, relevance ranking), `$text` is both faster and more capable.
- **Building a regex directly from unsanitized user input** — a real security/performance risk (ReDoS); always escape special characters first.
- **Forgetting a text index is required before `$text` works at all** — unlike regex, which needs no special index to function (just to perform well).

## Quick summary

- Regex filters (`/pattern/` or `$regex`) work without a special index, but only a start-anchored (`^`) pattern can actually use an existing index efficiently
- `$text` requires a text index but performs genuine word-tokenized search with relevance scoring, exclusion, and phrase-matching support — the better tool for real search features
- Never build a regex directly from unescaped user input — sanitize first, to avoid both incorrect matches and ReDoS risk
- Use regex for short, structured, prefix-style matching; use `$text` for real free-text search over longer content

## Next

**`07-filtering-nested-and-subdocument-fields.md`** covers the subtleties of querying embedded documents — beyond the plain dot-notation basics already touched on in earlier files.
