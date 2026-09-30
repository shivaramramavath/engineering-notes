# 06 — CRUD Methods

Every database method Mongoose provides, grouped by what they do, covered in real detail — signatures, options, return values, and the trade-offs between similar-looking methods.

## In this section

| File                                      | Covers                                                                                                 |
| ----------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| `01-create-methods.md`                    | `Model.create()`, `new Model().save()`, `insertMany()` — every way to create documents                 |
| `02-read-methods.md`                      | `find`, `findOne`, `findById`, `countDocuments`, `exists`, `distinct`                                  |
| `03-update-methods.md`                    | `updateOne`, `updateMany`, `findByIdAndUpdate`, `findOneAndUpdate`, and updating via `document.save()` |
| `04-delete-methods.md`                    | `deleteOne`, `deleteMany`, `findByIdAndDelete`, `findOneAndDelete`                                     |
| `05-query-chaining-and-cursor-methods.md` | `.sort()`, `.limit()`, `.skip()`, `.select()`, `.lean()`, `.populate()`, chaining order, and `.exec()` |
| `06-bulk-and-batch-methods.md`            | `bulkWrite()`, and `insertMany()`'s batch-specific options                                             |

## How this section is organized

Each file covers one _group_ of methods that solve the same problem in slightly different ways — read them together to understand not just what each method does, but **when to reach for which one**, since Mongoose deliberately offers several ways to do most things, each with a different trade-off (efficiency, whether middleware runs, whether you get the affected document back, and so on).

## What you should be able to do after this section

- Choose correctly between `Model.create()` and `new Model().save()` based on whether you need to modify the document before saving
- Use every read method appropriately, including the lighter-weight `exists`/`countDocuments`/`distinct` alternatives to a full `find`
- Choose between `updateOne` (fast, no returned document) and `findOneAndUpdate` (atomic, returns the document) based on what your code actually needs
- Chain query methods in a way that's both correct and readable, and know when `.exec()` is actually necessary
- Use `bulkWrite` for mixed batch operations efficiently

## Next

**`07-filter-conditions`** goes deep on the filter object every one of these methods accepts — every operator, in detail.
