# 10 — Middleware (Hooks)

Mongoose's other major mechanism for attaching custom behavior — not to a single call site, but to the schema itself, guaranteeing it runs every time a particular operation happens, regardless of where in the codebase that operation is triggered from.

## In this section

| File                                              | Covers                                                                                                              |
| ------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `01-pre-and-post-hooks.md`                        | The basic `pre`/`post` hook syntax, error handling inside hooks, and the difference between the two                 |
| `02-document-vs-query-vs-aggregate-middleware.md` | Which hook type actually fires for which method — a source of real confusion, especially around updates and deletes |
| `03-common-hook-patterns.md`                      | Real, practical patterns: password hashing, slug generation, and cascading deletes                                  |

## Why middleware matters as much as schema validation

Validation (`09-validation/`) answers "is this data allowed to be saved." Middleware answers a different question: "what else should happen, automatically, whenever this operation occurs." Password hashing before a save, generating a URL-friendly slug from a title, cleaning up related documents when a parent is deleted — none of these are validation rules, but all of them should happen reliably, every time, without every call site needing to remember to do it manually.

## What you should be able to do after this section

- Write `pre` and `post` hooks correctly, including handling errors inside them
- Know exactly which hook type (document, query, or aggregate) fires for a given method call — especially the important distinction around `deleteOne`/`updateOne` called on the Model vs. on a loaded Document
- Implement the three most common real-world hook patterns: hashing a password before save, auto-generating a slug, and cascading a delete to related documents

## Next

**`11-relationships`** covers references and `.populate()` — how documents relate to each other across collections.
