# 04 — Schemas

The core concept everything else in Mongoose is built around. A schema defines the shape, types, defaults, validation rules, and behavior of documents in a collection — this section covers it in real depth, since getting schema design right pays off in every later section.

## In this section

| File                                    | Covers                                                                                                                  |
| --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| `01-defining-a-schema.md`               | The basic `new mongoose.Schema({...})` syntax and how it relates to a model                                             |
| `02-schema-types-reference.md`          | Every built-in `SchemaType` — String, Number, Date, Buffer, ObjectId, Array, Map, Mixed, Decimal128, and more           |
| `03-schema-type-options.md`             | Per-field options in depth: `required`, `default`, `min`/`max`, `enum`, `unique`, `index`, and more                     |
| `04-nested-and-subdocument-schemas.md`  | Embedding one schema inside another — plain nested objects vs. real subdocuments                                        |
| `05-schema-options.md`                  | Schema-level (not per-field) options: `timestamps`, `toJSON`/`toObject`, `versionKey`, collection naming, `strict` mode |
| `06-schema-methods-statics-virtuals.md` | Attaching custom behavior directly to a schema — instance methods, static methods, and virtuals                         |
| `07-indexes-in-schemas.md`              | Declaring indexes as part of a schema, rather than managing them separately                                             |

## Why schemas get this much depth

A schema isn't just "the fields a document has" — it's where Mongoose's entire value proposition (`02-mongodb-vs-mongoose/01-what-mongoose-adds.md`) actually lives: every casting rule, every validation rule, every piece of attached behavior, and every index all originate from decisions made here. Time spent understanding schemas deeply pays off directly in `06-crud-methods/`, `08-errors/`, and `09-validation/` — most of what happens (and what can go wrong) in those sections traces back to how the schema was defined.

## What you should be able to do after this section

- Define a schema using the correct type for every field, including nested/array/map shapes
- Use per-field options (`required`, `default`, `enum`, `min`/`max`, `unique`) correctly and know what each actually enforces
- Decide when to nest a plain object vs. define a real subdocument schema
- Configure schema-level behavior like automatic timestamps and clean JSON output
- Attach instance methods, static methods, and virtuals directly to a schema
- Declare indexes as part of schema definition, understanding what each index type is for

## Next

**`05-models`** covers compiling a schema into an actual model — the object your application code interacts with.
