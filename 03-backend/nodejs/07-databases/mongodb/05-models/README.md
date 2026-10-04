# 05 — Models

With schemas covered in depth, this section covers the object your application code actually interacts with: the **model** — compiled from a schema, and the thing every `.find()`, `.create()`, and other method in `06-crud-methods/` is called on.

## In this section

| File                                                   | Covers                                                                                                                      |
| ------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------- |
| `01-creating-a-model.md`                               | `mongoose.model()` — compiling a schema into a model, and the collection-naming behavior that comes with it                 |
| `02-model-vs-document.md`                              | The class-vs-instance distinction: a Model is like a class, a Document is like an instance of it                            |
| `03-compiling-models-and-avoiding-overwrite-errors.md` | The `OverwriteModelError` that shows up constantly with hot-reloading/testing, and how to structure model files to avoid it |

## Why this gets its own (short) section

Schemas describe shape; models are what you actually call methods on. Understanding precisely what a model _is_ — and how it relates to the documents it produces — clears up a lot of confusion once `06-crud-methods/` starts throwing method after method at you, since every one of those methods is either a model method (operates on the collection) or a document method (operates on one instance).

## What you should be able to do after this section

- Compile a schema into a model with `mongoose.model()`, and know what collection name results
- Clearly explain the difference between a Model and a Document, and which kinds of operations belong to each
- Structure model files so repeated imports/hot-reloading doesn't throw an `OverwriteModelError`

## Next

**`06-crud-methods`** covers every database method available on a model and its documents, in full depth.
