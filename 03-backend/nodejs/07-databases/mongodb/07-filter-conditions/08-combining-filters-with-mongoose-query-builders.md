# Combining Filters with Mongoose Query Builders

Everything in this section so far has used a plain filter object. Mongoose also offers `.where()` — a chainable, method-based way to build the exact same filters. This file covers that alternative syntax and when it's actually useful.

## `.where()` — the basic idea

```js
User.find().where("status").equals("active");
// identical to:
User.find({ status: "active" });
```

```js
User.find().where("age").gte(18).lte(65).where("status").equals("active");
// identical to:
User.find({ age: { $gte: 18, $lte: 65 }, status: "active" });
```

Every operator from this section has a corresponding chainable method: `.equals()`, `.gt()`, `.gte()`, `.lt()`, `.lte()`, `.ne()`, `.in()`, `.nin()`, `.exists()`, `.regex()`, `.all()`, `.size()`, `.elemMatch()`, and more.

---

## When this is actually worth using

### Building a query conditionally, step by step

```js
function buildUserQuery({ status, minAge, city }) {
  let query = User.find();

  if (status) {
    query = query.where("status").equals(status);
  }
  if (minAge) {
    query = query.where("age").gte(minAge);
  }
  if (city) {
    query = query.where("address.city").equals(city);
  }

  return query;
}
```

This can read more naturally than conditionally building up a plain object field by field, especially when each condition involves a different operator — though the plain-object version (from `01-basic-filter-syntax.md`'s dynamic filter-building example) achieves the same result and is arguably just as clear, if not more so, for most people.

```js
// the equivalent plain-object version, for comparison
function buildUserFilter({ status, minAge, city }) {
  const filter = {};
  if (status) filter.status = status;
  if (minAge) filter.age = { $gte: minAge };
  if (city) filter["address.city"] = city;
  return filter;
}
```

Both are completely valid; which one reads better is genuinely a matter of preference and team convention, not a functional difference.

---

## `.or()` / `.and()` / `.nor()` on the query builder

```js
User.find().or([{ status: "active" }, { role: "admin" }]);
// identical to:
User.find({ $or: [{ status: "active" }, { role: "admin" }] });
```

These take the exact same array-of-conditions shape as the plain-object logical operators (`03-logical-operators.md`) — the builder form doesn't change the underlying operator semantics at all, just the syntax for expressing it.

---

## Mixing plain filter objects and the builder together

```js
User.find({ status: "active" })
  .where("age")
  .gte(18)
  .sort({ createdAt: -1 })
  .limit(10);
```

Perfectly valid — a plain object filter and `.where()` chaining can be combined in the same query, along with the cursor methods from `06-crud-methods/05-query-chaining-and-cursor-methods.md`.

---

## `.equals()` vs comparing to a plain value

```js
User.find().where("email").equals("alice@example.com");
// identical to:
User.find({ email: "alice@example.com" });
```

No functional difference — `.equals()` is just the chainable-syntax equivalent of the plain value shorthand.

---

## Plain object vs `.where()`: which should you actually use?

|                                                         | Plain filter object                                   | `.where()` chaining                                                        |
| ------------------------------------------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------- |
| Readability for a static, known filter                  | Usually clearer, more concise                         | More verbose for simple cases                                              |
| Readability for a dynamically-built, conditional filter | Fine, if slightly repetitive with lots of `if` blocks | Can read more fluently as a sequence of "and this condition, and this one" |
| What most Mongoose code/documentation uses              | The overwhelming majority                             | Less common, but fully supported                                           |
| Learning curve                                          | Same query-operator vocabulary you already know       | An additional method-name mapping to learn on top of the same operators    |

There's no functional difference in what either can express — everything covered across this entire section can be written either way. The plain object form is what the vast majority of real-world Mongoose code (and this documentation set) uses by default, since it maps directly onto the same filter syntax used in raw MongoDB/`mongosh`/the native driver — one syntax to learn and recognize everywhere, rather than two.

## Common mistakes

- **Assuming `.where()` can express something a plain filter object can't, or vice versa** — they're fully equivalent; the choice is purely stylistic.
- **Mixing both styles inconsistently within the same codebase without a clear convention** — pick one as the team default (plain objects are the more common choice) and reserve the other for genuine cases where it reads better.
- **Forgetting `.where()` chains still need to be awaited/executed** like any other Mongoose `Query` — the "doesn't execute until awaited" behavior from `06-crud-methods/05-query-chaining-and-cursor-methods.md` applies identically here.

## Quick summary

- `.where("field").operatorMethod(value)` is a fully equivalent, chainable alternative syntax to a plain filter object — same underlying operators, different surface syntax
- `.or()`/`.and()`/`.nor()` on the builder take the same condition-array shape as their plain-object counterparts
- Plain filter objects and `.where()` chaining can be freely mixed in the same query
- Most real-world Mongoose code defaults to plain filter objects, since that syntax is shared with raw MongoDB/`mongosh` — `.where()` is a matter of preference, not a capability gap

## Section complete

That's filter conditions covered in full depth — basic syntax, comparison, logical, element/type, array, regex/text operators, nested-field subtleties, and the alternative `.where()` builder syntax. **`08-errors`** covers exactly what happens when a write or a cast fails — every Mongoose error type, and how to turn them into clean application responses.
