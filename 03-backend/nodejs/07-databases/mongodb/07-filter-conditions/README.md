# 07 — Filter Conditions

Every method in `06-crud-methods/` that reads, updates, or deletes documents starts with a filter object. This section goes deep on exactly what can go inside one — every operator family, in detail, with realistic combined examples.

## In this section

| File                                                   | Covers                                                                                                          |
| ------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------- |
| `01-basic-filter-syntax.md`                            | How a plain filter object actually maps to a query, and the implicit-`$and` behavior of listing multiple fields |
| `02-comparison-operators.md`                           | `$eq`, `$ne`, `$gt`, `$gte`, `$lt`, `$lte`, `$in`, `$nin`                                                       |
| `03-logical-operators.md`                              | `$and`, `$or`, `$nor`, `$not`                                                                                   |
| `04-element-and-type-operators.md`                     | `$exists`, `$type`                                                                                              |
| `05-array-filter-conditions.md`                        | `$all`, `$elemMatch`, `$size`, and dot-notation into arrays                                                     |
| `06-regex-and-text-filters.md`                         | Regex matching and full-text search filters                                                                     |
| `07-filtering-nested-and-subdocument-fields.md`        | Dot notation into embedded documents, and the subtleties of matching whole subdocuments                         |
| `08-combining-filters-with-mongoose-query-builders.md` | `.where()` and Mongoose's chainable condition-builder syntax, as an alternative to a raw filter object          |

## Why filters get this much depth

A filter object looks simple, but real applications constantly need more precision than `{ field: value }` — ranges, multiple conditions, array membership, existence checks, and combinations of all of the above. Every operator here works identically whether you're calling `find`, `updateOne`, `deleteMany`, or building an aggregation `$match` stage later — mastering this section is a skill that transfers everywhere in Mongoose (and raw MongoDB) that a filter is accepted.

## What you should be able to do after this section

- Write a filter combining multiple fields and conditions correctly, understanding the implicit `$and`
- Use every comparison, logical, element, and array operator appropriately
- Query nested/subdocument fields correctly with dot notation, including the difference between matching a whole subdocument and matching one of its fields
- Use `$elemMatch` correctly when a single array element must satisfy multiple conditions together
- Choose between a plain filter object and Mongoose's `.where()` builder syntax

## Next

**`08-errors`** covers exactly what happens when things go wrong — every Mongoose error type, and how to turn them into clean, meaningful application responses.
