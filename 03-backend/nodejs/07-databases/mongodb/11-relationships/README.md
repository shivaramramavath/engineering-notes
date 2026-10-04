# 11 — Relationships

How documents relate to each other across collections — references, `.populate()`, the embed-vs-reference decision MongoDB deliberately leaves up to you, and virtual populate for the reverse direction.

## In this section

| File                                         | Covers                                                                                               |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| `01-references-and-populate.md`              | Declaring a reference with `ref`, and resolving it with `.populate()`                                |
| `02-embedding-vs-referencing-in-mongoose.md` | The actual decision-making process — when to embed a subdocument vs. reference a separate collection |
| `03-virtual-populate.md`                     | Populating the "other side" of a relationship without storing a redundant reference field            |
| `04-populate-performance.md`                 | The real cost of `.populate()`, and when an aggregation `$lookup` is the better choice               |

## Why this comes after CRUD, filters, errors, validation, and middleware

Relationships build directly on everything covered so far: a reference field is just an `ObjectId`-typed schema field (`04-schemas/02-schema-types-reference.md`), `.populate()` is a query-chaining method (`06-crud-methods/05-query-chaining-and-cursor-methods.md`), and cascading deletes across related documents were already previewed in `10-middleware-hooks/03-common-hook-patterns.md`. This section brings those threads together specifically around the question of "how do separate collections relate to each other."

## What you should be able to do after this section

- Declare a reference field with `ref`, and resolve it into the actual related document with `.populate()`
- Make a deliberate, reasoned choice between embedding and referencing for a given relationship, rather than defaulting to one habitually
- Use virtual populate to query "the other side" of a one-to-many relationship without a redundant array field
- Recognize when `.populate()`'s N+1-style query pattern becomes a real performance concern, and reach for aggregation's `$lookup` instead

## Next

**`12-aggregation-with-mongoose`** covers `Model.aggregate()` — including `$lookup`, the aggregation-pipeline alternative to `.populate()` introduced in this section's performance file.
