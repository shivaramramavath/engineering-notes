# 09 — Validation

`08-errors/02-validation-errors.md` covered reading a `ValidationError` after the fact. This section covers the other side: actually defining the validation rules that produce (or prevent) one in the first place — built-in validators, writing your own, async validation, and the specific gotcha around validating updates.

## In this section

| File                        | Covers                                                                                                              |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `01-built-in-validators.md` | Every built-in validator Mongoose ships with — `required`, `min`/`max`, `enum`, `match`, `minlength`/`maxlength`    |
| `02-custom-validators.md`   | Writing your own validation logic with `validate`, including cross-field validation                                 |
| `03-async-validators.md`    | Validators that need to check something asynchronously — e.g. querying the database as part of validation           |
| `04-validating-updates.md`  | The `runValidators` gotcha in depth — why update operations skip validation by default, and how to fix it correctly |

## Why this deserves its own section, beyond schema type options

`04-schemas/03-schema-type-options.md` introduced the built-in validators as part of defining a field. This section goes deeper: writing genuinely custom rules, handling validation that needs to be asynchronous (checking something in the database), and the single most commonly-missed validation gotcha in all of Mongoose — that update methods don't validate by default at all.

## What you should be able to do after this section

- Use every built-in validator correctly, with clear custom messages
- Write a custom validator function, including one that depends on another field on the same document
- Write an async validator correctly, and know when one is actually necessary
- Always remember `runValidators: true` on update operations, and understand exactly why it's needed

## Next

**`10-middleware-hooks`** covers `pre`/`post` hooks — the other major way Mongoose lets you attach custom behavior to the save/update/delete lifecycle.
