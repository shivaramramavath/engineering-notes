# 12 — Aggregation with Mongoose

`Model.aggregate()` — Mongoose's interface to MongoDB's aggregation pipeline, the tool for anything a plain filter/query can't express: joins across collections (`$lookup`, previewed in `11-relationships/04-populate-performance.md`), grouping/summing, and multi-stage data transformations.

## In this section

| File                            | Covers                                                                                               |
| ------------------------------- | ---------------------------------------------------------------------------------------------------- |
| `01-the-aggregate-method.md`    | `Model.aggregate()` itself — how it differs from `find()`, and how a pipeline is structured          |
| `02-common-pipeline-recipes.md` | Practical, ready-to-adapt pipelines: joins, grouping/counting, and combining several stages together |

## Why this section is relatively short

Aggregation is a large topic in MongoDB generally, but `Model.aggregate()` itself is a thin pass-through to the same pipeline syntax covered in raw MongoDB documentation — Mongoose doesn't add much of a layer on top of it (notably, aggregation results are **not** cast/validated against your schema the way `find()` results are, covered in the next file). This section focuses on the Mongoose-specific integration points and the recipes you'll reach for constantly, rather than re-deriving the entire aggregation pipeline stage-by-stage from scratch.

## What you should be able to do after this section

- Call `Model.aggregate()` with a multi-stage pipeline, understanding that results are plain objects, not Documents
- Use `$lookup` to join related collections directly in a query, for the cases `.populate()` can't handle (`11-relationships/04-populate-performance.md`)
- Group and count documents with `$group`
- Combine `$match`, `$lookup`, `$group`, `$sort`, and `$project` into a realistic, multi-stage pipeline

## Next

**`13-transactions`** covers wrapping multiple operations — including, potentially, ones involving aggregation-adjacent reads — into a single atomic unit.
