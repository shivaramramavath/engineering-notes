# 08 — Errors

Every kind of error Mongoose can throw, in detail — what triggers each one, how to read its structure, and how to turn it into a clean, meaningful response for your application's users. This section covers exactly the "email duplicate → user already exists" pattern and everything needed to build it correctly.

## In this section

| File                                           | Covers                                                                                                                                                 |
| ---------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `01-mongoose-error-types-overview.md`          | Every Mongoose error type at a glance — `ValidationError`, `CastError`, `MongoServerError`, `DocumentNotFoundError`, `VersionError`, and more          |
| `02-validation-errors.md`                      | Reading a `ValidationError`'s structure in depth — `.errors`, per-field messages, and iterating over them                                              |
| `03-cast-errors.md`                            | `CastError` specifically — invalid `ObjectId`s, wrong types, and exactly when this fires vs. a `ValidationError`                                       |
| `04-duplicate-key-errors.md`                   | Code `11000`, parsing the raw MongoDB error message, and identifying **which** field/index actually caused it                                          |
| `05-turning-errors-into-friendly-responses.md` | The full pattern: catching a duplicate-key error and responding with a clean "email already exists" message, via centralized error-handling middleware |
| `06-custom-application-errors.md`              | Wrapping/rethrowing Mongoose's errors as your own application-specific error classes                                                                   |

## Why this section matters as much as it does

Every one of Mongoose's error types has a genuinely different shape and cause — treating them identically ("just show a generic 400 error") throws away information your application could use to respond precisely and helpfully (a specific field-level validation message, a clear "this email is taken," a real 500 for something genuinely unexpected). This section is about handling each error type correctly, not just catching and logging everything the same way.

## What you should be able to do after this section

- Identify which Mongoose error type you're dealing with, and know the general shape each one takes
- Read a `ValidationError`'s `.errors` object to extract per-field messages
- Distinguish a `CastError` (bad input shape) from a `ValidationError` (correctly-shaped but rule-violating input)
- Recognize code `11000`, and extract exactly which field/index caused a duplicate-key violation
- Implement the full "email already exists" pattern: catch the duplicate-key error, map it to a clean `409 Conflict` response
- Wrap Mongoose's errors in your own application error classes, so the rest of your codebase doesn't need to know Mongoose-specific error shapes

## Next

**`09-validation`** goes deeper on preventing bad data in the first place — built-in validators, custom validators, and the specific gotchas around validating updates.
